/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

decls.c -- Scanning of declarations.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "folding.h"
#include "statements.h"
/* To get clear_initializer_cache: */
#include "exprutil.h"
#if USER_CONTROL_OF_STRUCT_PACKING
#include "layout.h"
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro to test whether the current token is a Microsoft storage class
specifier.  Includes an "||" at the beginning.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_is_microsoft_storage_class() ||			      \
  (curr_token == tok_declspec ||				      \
   curr_token == tok_microsoft_inline ||			      \
   curr_token == tok_forceinline)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_microsoft_storage_class() /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro to test for "__thread", "thread_local", or "_Thread_local" storage
specifier.
*/
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
#define or_is_thread_local_storage_specifier() ||                     \
  ((curr_token == tok_thread_local) ||                                \
   (curr_token == tok_c11_thread_local) ||                            \
   (curr_token == tok_thread))
#else /* !THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
#define or_is_thread_local_storage_specifier() ||                     \
  ((curr_token == tok_thread_local) ||                                \
   (curr_token == tok_c11_thread_local))
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */

/*
Macro that is TRUE if the current token is the start of a storage class
specifier.
*/
#define is_storage_class()                                            \
  (curr_token == tok_typedef  || curr_token == tok_extern   ||        \
   curr_token == tok_static   || curr_token == tok_mutable  ||        \
   curr_token == tok_register ||                                      \
   (auto_storage_class_specifier_enabled && curr_token == tok_auto)   \
   or_is_thread_local_storage_specifier()                             \
   or_is_microsoft_storage_class())

/*
Macro that is TRUE if the current token is the start of a function
specifier.
*/
#define is_function_specifier()                                      \
  (curr_token == tok_inline   || curr_token == tok_virtual ||        \
   curr_token == tok_explicit || curr_token == tok_noreturn)

#if !NULL_POINTER_IS_ZERO

static void clear_init_state_fields(an_init_state  *is)
/*
Clear the fields of *is.
*/
{
  is->init_con = NULL;
  is->init_dip = NULL;
  is->decl_parse_state = NULL;
  is->class_to_look_in = NULL;
  is->arg_match = NULL;
  is->direct_init = FALSE;
  is->static_lifetime_init = FALSE;
  is->initializer_must_be_constant = FALSE;
  is->force_dynamic_init = FALSE;
  is->no_diagnostics = FALSE;
  is->check_validity_only = FALSE;
  is->error_on_narrowing = FALSE;
  is->warning_on_narrowing = FALSE;
  is->init_error = FALSE;
  is->has_dynamic_init_component = FALSE;
  is->any_uninitialized_const_or_ref_member = FALSE;
  is->partial_initializer = FALSE;
  is->pack_expansion_handled = FALSE;
  is->chained_designator_okay = FALSE;
  is->non_top_level_aggregate = FALSE;
  is->elided_braces_disallowed = (gpp_mode && gnu_version < 40800);
  is->elements_are_full_expressions = FALSE;
  is->variable_size_array = FALSE;
  is->initializer_can_dimension_array = FALSE;
  is->not_evaluated = FALSE;
  is->not_potentially_evaluated = FALSE;
  is->traditional_const_expr_required = FALSE;
  is->constant_expr_ruled_out = FALSE;
  is->resumable = FALSE;
  is->pending_elements = FALSE;
}  /* clear_init_state_fields */

#endif /* !NULL_POINTER_IS_ZERO */

static
void clear_decl_parse_state_fields(a_decl_parse_state  *dps,
                                   a_boolean           secondary_declarator)
/*
Clear the fields of *dps.  If secondary_declarator is TRUE, only those fields
associated with the declarator part of a declaration should be initialized
(and some fields clobbered by the processing of a previous declarator should
be restored).
*/
{
  if (!secondary_declarator) {
    /* Initialize fields not particularly associated with a declarator. */
    dps->sym = NULL;
    dps->dso_flags = 0;
    dps->start_pos = null_source_position;
    dps->specifiers_pos = null_source_position;
    dps->return_type_pos = null_source_position;
    dps->qualifiers = TQ_NONE;
    dps->qualifiers_pos = null_source_position;
    dps->restrict_pos = null_source_position;
    dps->inline_pos = null_source_position;
    dps->virtual_pos = null_source_position;
    dps->auto_pos = null_source_position;
    dps->constexpr_pos = null_source_position;
    dps->in_class_scope = FALSE;
    dps->secondary_declarator = FALSE;
    dps->is_template_declaration = FALSE;
    dps->is_template_rescan = FALSE;
    dps->is_trailing_return_type = FALSE;
    dps->is_type_name = FALSE;
    dps->is_alias_template_type = FALSE;
    dps->is_template_type_argument = FALSE;
    dps->trailing_return_type_allowed = FALSE;
    dps->has_trailing_return_type = FALSE;
    dps->is_new_expr_type = FALSE;
    dps->is_evaluated_sizeof_type_arg = FALSE;
    dps->disallow_variably_modified_type = FALSE;
    dps->unused_qualifiers = FALSE;
    dps->auto_type_allowed = FALSE;
    dps->auto_type_specifier_seen = FALSE;
    dps->decltype_auto_specifier_seen = FALSE;
    dps->has_deducible_return_type = FALSE;
    dps->is_asm_function = FALSE;
    dps->function_definition_allowed = FALSE;
    dps->is_old_style_param_decl = FALSE;
    dps->is_top_level_declaration = FALSE;
    dps->is_linkage_spec_decl = FALSE;
    dps->marked_as_gnu_extension = FALSE;
    dps->decl_specifiers_omitted = FALSE;
    dps->decl_specifiers_error = FALSE;
    dps->need_semicolon_remove_stop_token = FALSE;
    dps->need_comma_remove_stop_token = FALSE;
    dps->need_assign_remove_stop_token = FALSE;
    dps->need_lbrace_remove_stop_token = FALSE;
    dps->restore_name_linkage = FALSE;
    dps->redeclares_tag = FALSE;
    dps->tag_def_or_forward_decl = FALSE;
    dps->is_property_or_event_field = FALSE;
    dps->is_declspec_property_field = FALSE;
    dps->has_cli_context_sensitive_keyword = FALSE;
    dps->has_cli_property_keyword = FALSE;
    dps->has_cli_event_keyword = FALSE;
    dps->has_cli_initonly_keyword = FALSE;
    dps->has_cli_literal_keyword = FALSE;
    dps->initializer_is_single_expr = FALSE;
    dps->is_explicit_instantiation = FALSE;
    dps->range_based_for = FALSE;
    dps->decl_okay_in_constexpr_body = FALSE;
    dps->is_inheriting_ctor = FALSE;
    dps->is_explicit_override = FALSE;
    dps->is_init_capture = FALSE;
    dps->is_lambda = FALSE;
    dps->is_alias = FALSE;
    dps->prefix_attributes = NULL;
    dps->specifier_attributes = NULL;
    dps->tag_attributes = NULL;
    clear_decl_modifiers_block(&dps->decl_modifiers);
    dps->ms_attributes = NULL;
    dps->register_id = 0;
    dps->storage_class_pos = null_source_position;
    dps->declared_storage_class = (a_storage_class)sc_unspecified;
    dps->storage_class = (a_storage_class)sc_unspecified;
    dps->specifiers_type = NULL;
    dps->declared_type = NULL;
    dps->type = NULL;
    dps->prev_type = NULL;
    dps->auto_type = NULL;
    dps->deduced_auto_type = NULL;
    dps->param_id = NULL;
    dps->param_id_list = NULL;
    dps->upc_block_size = UPC_BLOCK_SIZE_NONE;
    dps->p_postfix_entities = NULL;
    dps->assoc_func_decl_state = NULL;
    dps->end_of_parse_actions = NULL;
    dps->position_of_this_reference_in_trailing_return = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    dps->extra_positions = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    dps->next = NULL;
  } else {
    /* Set field values specifically for a secondary declarator. */
    dps->secondary_declarator = TRUE;
    dps->storage_class = dps->declared_storage_class;
    dps->declared_type = dps->specifiers_type;
    dps->type = dps->specifiers_type;
    if (dps->has_trailing_return_type) {
      /* The previous declarator had a trailing return type, which caused us
         to override the "auto" type with the actual return type.  Restore the
         "auto" type. */
      dps->has_trailing_return_type = FALSE;
      dps->specifiers_type = dps->auto_type;
      dps->declared_type = dps->auto_type;
      dps->type = dps->auto_type;
      dps->return_type_pos = null_source_position;
    }  /* if */
  }  /* if */
  /* Initialize fields associated with a declarator. */
  dps->do_flags = DO_NO_OUTPUT_FLAGS;
  dps->declarator_start_pos = null_source_position;
  dps->declarator_pos = null_source_position;
  dps->is_definition = FALSE;
  dps->in_nested_declarator = FALSE;
  dps->pack_ellipsis_allowed = FALSE;
  dps->has_pack_ellipsis = FALSE;
  dps->is_pack_element = FALSE;
  dps->nested_ptr_or_ref_seen = FALSE;
  dps->function_declarator_seen = FALSE;
  dps->has_initializer = FALSE;
  dps->has_direct_initializer = FALSE;
  dps->first_decl = FALSE;
  dps->first_decl_of_predeclared_entity = FALSE;
  dps->override_okay = FALSE;
  dps->initializer_is_expr_list = FALSE;
  dps->no_special_cli_class_type_check = FALSE;
  dps->is_generic_declaration = FALSE;
  dps->template_void_specifier = FALSE;
  dps->is_inclass_member_function_decl = FALSE;
  dps->is_out_of_class_member_function_decl = FALSE;
  dps->position_of_this_reference_in_trailing_return_set = FALSE;
  dps->vla_field_treated_as_zero_length_array = FALSE;
  clear_init_state(&dps->init_state);
  dps->id_attributes = NULL;
  dps->asm_name = NULL;
  dps->asm_name_pos = null_source_position;
  clear_initializer_cache(&dps->prescanned_initializer_cache);
  dps->prescanned_initializer_levels_down = 0;
  dps->source_sequence_entry = NULL;
  dps->alignment = 0;
  dps->auto_params = NULL;
  dps->routine_fixup = NULL;
}  /* clear_decl_parse_state_fields */


static a_decl_parse_state_ptr
		avail_decl_parse_states;
			/* Pointer to state entries available for reuse. */

#if DEBUG
static unsigned long
		num_decl_parse_states_allocated = 0;
#endif /* DEBUG */


a_decl_parse_state_ptr alloc_decl_parse_state(void)
/*
Allocate a declaration parse state in front end memory, initialize it, and
return a pointer to it.
*/
{
  a_decl_parse_state_ptr  dps = alloc_fe_of_type(a_decl_parse_state);

  if (avail_decl_parse_states != NULL) {
    dps = avail_decl_parse_states;
    avail_decl_parse_states = avail_decl_parse_states->next;
  } else {
    dps = alloc_fe_of_type(a_decl_parse_state);
#if DEBUG
    ++num_decl_parse_states_allocated;
#endif /* DEBUG */
  }  /* if */
  init_decl_parse_state(dps);
  return dps;
}  /* alloc_decl_parse_state */


void free_decl_parse_state(a_decl_parse_state_ptr  dps)
/*
Return the given declaration parse state entry to the list of available
entries.
*/
{
  dps->next = avail_decl_parse_states;
  avail_decl_parse_states = dps;
}  /* free_decl_parse_state */


static a_decl_parse_callback_ptr
		avail_decl_parse_callbacks;
			/* Pointer to callback entries available for reuse. */

#if DEBUG
static unsigned long
		num_decl_parse_callbacks_allocated = 0;
#endif /* DEBUG */


void add_end_of_parse_action(a_decl_parse_callback_function  *fn,
                             a_decl_parse_state              *dps,
                             a_boolean                       secondary_decls)
/*
Allocate an entry to call back the given function with the given parse state,
and add it to the actions to be performed at the end of the declaration
described by dps.  If secondary_decls is TRUE, the action should also be
performed for declarations associated with subsequent secondary declarators.
*/
{
  a_decl_parse_callback_ptr  entry;

  if (avail_decl_parse_callbacks != NULL) {
    entry = avail_decl_parse_callbacks;
    avail_decl_parse_callbacks = avail_decl_parse_callbacks->next;
  } else {
    entry = alloc_fe_of_type(a_decl_parse_callback);
#if DEBUG
    ++num_decl_parse_callbacks_allocated;
#endif /* DEBUG */
  }  /* if */
  entry->callback_fn = fn;
  entry->apply_to_secondary_declarators = secondary_decls;
  entry->next = dps->end_of_parse_actions;
  dps->end_of_parse_actions = entry;
}  /* add_end_of_parse_action */


void run_end_of_parse_actions(a_decl_parse_state  *dps,
                              a_boolean           more_declarators)
/*
Execute the end-of-parse callbacks registered for the declaration described by
*dps, and, if appropriate, free up the associated callback entries.
more_declarators is TRUE if secondary declarators will follow (in which case
some associated callback entries should not be freed).
*/
{
  a_decl_parse_callback_ptr  actions = dps->end_of_parse_actions, *p_action;

  /* Clear dps->end_of_parse_actions.  The execution of the actions could
     conceivably add more actions, but that is currently prohibited. */
  dps->end_of_parse_actions = NULL;
  for (p_action = &actions; *p_action != NULL;) {
    a_decl_parse_callback_ptr       action = *p_action;
    a_decl_parse_callback_function  *callback = action->callback_fn;
    if (!more_declarators || !action->apply_to_secondary_declarators) {
      /* Remove this action from the list and free it up for reuse. */
      *p_action = action->next;
      action->next = avail_decl_parse_callbacks;
      action->callback_fn = NULL;
      avail_decl_parse_callbacks = action;
    } else {
      /* Leave the current action in the list for subsequent declarators, and
         move to the next action. */
      p_action = &action->next;
    }  /* if */
    /* Execute the action. */
    callback(dps);
  }  /* for */
  /* End-of-parse actions are currently not allowed to generate more actions
     for the same declaration. */
  check_assertion(dps->end_of_parse_actions == NULL);
  /* Reinstall actions that should persist to subsequent secondary
     declarators. */
  dps->end_of_parse_actions = actions;
}  /* run_end_of_parse_actions */


void discard_end_of_parse_actions(a_decl_parse_state  *dps)
/*
Discard the end-of-parse callbacks registered for the declaration described by
*dps without executing them.
*/
{
  a_decl_parse_callback_ptr  action = dps->end_of_parse_actions;

  /* Loop through the list of actions to clear the callback pointers and
     find the last element. */
  if (action != NULL) {
    for (;; action = action->next) {
      action->callback_fn = NULL;
      if (action->next == NULL) {
        /* Last element found: Move the actions to the available list and
           we're done. */
        action->next = avail_decl_parse_callbacks;
        avail_decl_parse_callbacks = dps->end_of_parse_actions;
        dps->end_of_parse_actions = NULL;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* discard_end_of_parse_actions */


static an_auto_param_descr_ptr
		avail_auto_param_descriptions;
			/* Pointer to "auto" parameter description entries
			   available for reuse. */

#if DEBUG
static unsigned long
		num_auto_param_descriptions_allocated = 0;
#endif /* DEBUG */


void record_auto_param_descr(a_decl_parse_state_ptr  dps)
/*
Allocate an entry to describe an "auto" type specifier encountered while
prescanning a function declarator (for a C++14 generic lambda) and add it to
the front of the list pointed to by dps->auto_params.  The "auto" specifier
must be the current token.
*/
{
  an_auto_param_descr_ptr  entry;

  check_assertion(curr_token == tok_auto);
  if (avail_auto_param_descriptions != NULL) {
    entry = avail_auto_param_descriptions;
    avail_auto_param_descriptions = avail_auto_param_descriptions->next;
  } else {
    entry = alloc_fe_of_type(an_auto_param_descr);
#if DEBUG
    ++num_auto_param_descriptions_allocated;
#endif /* DEBUG */
  }  /* if */
  entry->next = dps->auto_params;
  entry->template_type_parameter = NULL;
  entry->auto_tsn = curr_token_sequence_number;
  entry->param_num = 0;
  entry->is_parameter_pack = FALSE;
  entry->start_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  entry->end_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  dps->auto_params = entry;
}  /* record_auto_param_descr */


void free_auto_param_descriptions(a_decl_parse_state_ptr  dps)
/*
Return the "an_auto_param_descr" entries pointed to by dps to the list of
available entries.
*/
{
  if (dps->auto_params != NULL) {
    an_auto_param_descr_ptr  last = dps->auto_params;
    while (last->next != NULL) last = last->next;
    last->next = avail_auto_param_descriptions;
    avail_auto_param_descriptions = dps->auto_params;
    dps->auto_params = NULL;
  }  /* if */
}  /* free_auto_param_descriptions */


an_attribute_ptr f_find_decl_attribute(a_byte_attribute_kind  kind,
                                       a_decl_parse_state     *dps)
/*
If the lists dps->prefix_attributes or dps->id_attributes include an attribute
entry of the given kind, return one of those entries.
*/
{
  an_attribute_ptr  ap = find_attribute(kind, dps->prefix_attributes);

  if (ap == NULL) {
    ap = find_attribute(kind, dps->id_attributes);
  }  /* if */
  return ap;
}  /* f_find_decl_attribute */


static void disallow_attributes(an_attribute_ptr  *p_attributes)
/*
If *p_attributes is non-NULL, issue an error message indicating that
attributes are not allowed at the location in which the associated attribute
appeared, and set *p_attributes to NULL;
*/
{
  if (*p_attributes != NULL) {
    pos_error(ec_invalid_attribute_location,
              &(*p_attributes)->group->position);
    *p_attributes = NULL;
  }  /* if */
}  /* disallow_attributes */


static void diagnose_unattached_attributes(an_attribute_ptr  attributes)
/*
Attributes is a list (possibly NULL) of attributes that do not apply to any
entity.  Issue diagnostics as appropriate.
*/
{
  if (attributes != NULL) {
    an_attribute_ptr   ap = attributes, err_ap = NULL;
    an_error_severity  sev = es_warning;
    /* Look for a standard attribute: It would elicit an error (whereas GNU
       attributes only trigger a warning). */
    for (; ap != NULL; ap = ap->next) {
      if (is_std_attribute(ap) &&
          ap->kind != (a_byte_attribute_kind)ak_empty_attr &&
          !gnu_mode) {
        sev = es_error;
        err_ap = ap;
        break;
      } else if (is_unapplicable_attr(ap)) {
        /* Do not issue a diagnostic for an unrecognized GNU or Microsoft
           attribute. */
      } else {
        err_ap = ap;
      }  /* if */
    }  /* for */
    if (err_ap != NULL) {
      pos_diagnostic(sev, ec_unattached_attribute, &err_ap->position);
    }  /* if */
  }  /* if */
}  /* diagnose_unattached_attributes */


void attach_parse_state_to_attributes(a_decl_parse_state  *dps)
/*
Set the "extra_info" field of the attributes recorded in *dps to dps.  This
allows the functions processing the attributes to access information about
the declaration in which the attributes appeared.
*/
{
  an_attribute_ptr  ap;

  for (ap = dps->prefix_attributes; ap != NULL; ap = ap->next) {
    ap->assoc_info = (void*)dps;
  }  /* for */
  for (ap = dps->id_attributes; ap != NULL; ap = ap->next) {
    ap->assoc_info = (void*)dps;
  }  /* for */
}  /* attach_parse_state_to_attributes */


void detach_parse_state_from_attributes(a_decl_parse_state  *dps)
/*
Set the "extra_info" field of the attributes recorded in *dps to NULL (to
avoid a dangling pointer when *dps goes away).
*/
{
  an_attribute_ptr  ap;

  for (ap = dps->prefix_attributes; ap != NULL; ap = ap->next) {
    ap->assoc_info = NULL;
  }  /* for */
  for (ap = dps->id_attributes; ap != NULL; ap = ap->next) {
    ap->assoc_info = NULL;
  }  /* for */
}  /* detach_parse_state_from_attributes */


static void attach_decl_attributes_to_entity(a_decl_parse_state  *dps,
                                             an_il_entry_kind    entity_kind,
                                             char                *entity,
                                             a_boolean           primary_decl)
/*
Attach the attributes recorded in *dps to the given entity.  If primary_decl
is TRUE, the on_primary_declaration flag of the attributes is set to TRUE
before they are attached.  
This routine is usually called through attach_decl_attributes, which uses
dps->sym to identify the target entity.  It can be called directly in cases
where dps->sym might be NULL (e.g., exception handler parameters, which may
be unnamed).
*/
{
  if (dps->id_attributes != NULL || dps->prefix_attributes != NULL) {
    if (dps->secondary_declarator) {
      dps->prefix_attributes = copy_of_attributes_list(dps->prefix_attributes);
    }  /* if */
    attach_parse_state_to_attributes(dps);
    if (primary_decl) mark_primary_decl_attributes(dps->id_attributes);
    attach_attributes(dps->id_attributes, entity, entity_kind);
    if (primary_decl) mark_primary_decl_attributes(dps->prefix_attributes);
    attach_attributes(dps->prefix_attributes, entity, entity_kind);
    detach_parse_state_from_attributes(dps);
  }  /* if */
}  /* attach_decl_attributes_to_entity */


void attach_decl_attributes(a_decl_parse_state  *dps,
                            a_boolean           primary_decl)
/*
Attach the attributes recorded in *dps to the entity whose declaration is
described by *dps (preserving any already-attached attributes).  If
primary_decl is TRUE, the on_primary_declaration flag of the attributes is
set to TRUE before they are attached.
*/
{
  if (dps->id_attributes != NULL || dps->prefix_attributes != NULL) {
    an_il_entry_kind  entity_kind;
    char              *entity;
    if ((dps->dso_flags & DSO_FRIEND) != 0 && !primary_decl) {
      /* Standard attributes are allowed on friend declarations only if that
         friend declaration is also a definition. */
      an_attribute_ptr  ap, err_ap = NULL;
      for (ap = dps->prefix_attributes; ap != NULL; ap = ap->next) {
        if (is_std_attribute(ap)) {
          if (err_ap == NULL) err_ap = ap;
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
      for (ap = dps->id_attributes; ap != NULL; ap = ap->next) {
        if (is_std_attribute(ap)) {
          if (err_ap == NULL) err_ap = ap;
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
      if (err_ap != NULL) {
        pos_error(ec_friend_attribute_requires_definition,
                  &err_ap->group->position);
        goto done;
      }  /* if */
    }  /* if */
    if (dps->sym == NULL) {
      entity = NULL;
      entity_kind = iek_none;
    } else if (dps->sym->kind == (a_symbol_kind)sk_function_template) {
      a_template_symbol_supplement_ptr  tssp = dps->sym->variant.template_info;
      entity = (char*)tssp->variant.function.routine;
      entity_kind = iek_routine;
    } else {
      entity = il_entry_for_symbol(dps->sym, &entity_kind);
    }  /* if */
    attach_decl_attributes_to_entity(dps, entity_kind, entity, primary_decl);
  }  /* if */
done:;
}  /* attach_decl_attributes */


void attach_param_attributes(a_decl_parse_state  *dps,
                             a_param_type_ptr    ptp)
/*
Attach the attributes recorded in *dps to the indicated parameter.  When that
is done, clear the corresponding attribute pointers in *dps (id_attributes and
prefix_attributes).
*/
{
  if (dps->id_attributes != NULL || dps->prefix_attributes != NULL) {
    attach_parse_state_to_attributes(dps);
    mark_primary_decl_attributes(dps->id_attributes);
    attach_attributes(dps->id_attributes, (char*)ptp, iek_param_type);
    mark_primary_decl_attributes(dps->prefix_attributes);
    attach_attributes(dps->prefix_attributes, (char*)ptp, iek_param_type);
    dps->type = ptp->type;
    dps->prefix_attributes = dps->id_attributes = NULL;
    detach_parse_state_from_attributes(dps);
  }  /* if */
}  /* attach_param_attributes */

#if GNU_EXTENSIONS_ALLOWED

static void deactivate_gnu_decl_attributes_on_template_redecl(
                                        a_decl_parse_state  *dps,
                                        an_attribute_ptr    prev_attributes)
/*
Mark all the id attributes and prefix attributes of the declaration described
by dps as "unrecognized", thereby deactivating any effects those attributes
might otherwise have had.  Issue a warning if there is such an attribute and
an attribute of the same kind did not appear on the list pointed to by
prev_attributes (attributes that were recorded when the first declaration
of the template was seen; NULL if none).
*/
{
  an_attribute_ptr  ap, diag_ap = NULL;

  for (ap = dps->prefix_attributes; ap != NULL; ap = ap->next) {
    if (ap->family == (a_byte_attribute_family)af_gnu) {
      if (diag_ap == NULL && !is_unapplicable_attr(ap) &&
          find_attribute(ap->kind, prev_attributes) == NULL) {
        /* This is the first deactivated attribute not mentioned in the
           original template declaration. */
        diag_ap = ap;
      }  /* if */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* for */
  for (ap = dps->id_attributes; ap != NULL; ap = ap->next) {
    if (ap->family == (a_byte_attribute_family)af_gnu) {
      if (!is_unapplicable_attr(ap)) {
        if (diag_ap == NULL &&
            find_attribute(ap->kind, prev_attributes) == NULL) {
          /* This is the first deactivated attribute not mentioned in the
             original template declaration. */
          diag_ap = ap;
        }  /* if */
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
  }  /* for */
  if (diag_ap != NULL) {
    /* Issue a warning on the first deactivated attribute that did not appear
       in the original declaration.  If the original declaration had
       attributes, the warning affirms that those are not deactivated. */
    /* Make sure that the previous attribute referred to has a position (i.e.,
       that it is not compiler-generated). */
    for (; prev_attributes != NULL; prev_attributes = prev_attributes->next) {
      if (prev_attributes->position.seq != 0) break;
    }  /* for */
    if (prev_attributes == NULL) {
      pos_warning(ec_gnu_attr_on_template_redecl, &diag_ap->position);
    } else {
      pos2_diagnostic(es_warning,
                      ec_gnu_attr_on_template_redecl_but_original_kept,
                      &diag_ap->position, &prev_attributes->position);
    }  /* if */
  }  /* if */
}  /* deactivate_gnu_decl_attributes_on_template_redecl */

#endif /* GNU_EXTENSIONS_ALLOWED */

void f_check_pending_qualifiers_used(a_decl_parse_state  *state)
/*
If the given a_decl_parse_state object indicates that pending type qualifiers
have not had an effect on the type, issue a warning and mark the qualifiers as
now having had an effect.  For example:
  void f(int i) {
    typedef int &RI;
    (RI const)i;  // "const" has no effect
  }
*/
{
  if (state->unused_qualifiers) {
    an_error_severity  sev = es_warning;
    if (is_real_instantiation_context() &&
        is_nonspecialized_instantiation_context()) {
      sev = es_remark;
    }  /* if */
    pos_diagnostic(sev, ec_useless_type_qualifiers_in_type_name,
                   &state->qualifiers_pos);
    /* Discard the qualifiers to avoid duplicating diagnostics or confusing
       later operations that expect a cv-qualified type when state->qualifiers
       is not TQ_NONE. */
    state->qualifiers = TQ_NONE;
    state->unused_qualifiers = FALSE;
  }  /* if */
}  /* f_check_pending_qualifiers_used */

#if GNU_EXTENSIONS_ALLOWED

void scan_gnu_asm_name(a_decl_parse_state  *dps)
/*
Scan a construct of the form
    asm ( "string" )
and record the contents and position of the string literal in *dps.  This is a
GNU extension that provides the name to be used for an entity in generated
assembler code.
*/
{
  if (gnu_mode && curr_token == tok_asm) {
    a_const_char       *asm_name = NULL;
    a_source_position  asm_start_pos, asm_name_pos = null_source_position;
    asm_start_pos = pos_curr_token;
    report_gnu_extension_if_needed(&pos_curr_token,
                                   ec_asm_name_is_gnu_extension);
    /* Bypass "asm" and the leading paren. */
    (void)get_token();
    if (required_token(tok_lparen, ec_exp_lparen)) {
      add_stop_token(tok_rparen);
      /* The next token must be a string constant.  If it isn't, flush and
         then consume any right paren to avoid double errors. */
      if (curr_token != tok_string_literal) {
        syntax_error(ec_exp_string_literal);
        if (curr_token == tok_rparen) {
          (void)get_token();
        }  /* if */
      } else {
        /* If there was an error in parsing the string, we do not need to
           issue another error here. */
        if (!is_error_constant(&const_for_curr_token)) {
          /* GCC accepts string literals with embedded null characters (like
             "ab\0c") but ignores everything after the "\0".  So, storing the
             asm argument as a character pointer, without a length, gives
             compatibility with GCC. */
          asm_name = const_for_curr_token.variant.string.value;
          asm_name_pos = pos_curr_token;
        }  /* if */
        /* Consume the string constant. */
        (void)get_token();
        /* There should now be a right parenthesis. */
        (void)required_token(tok_rparen, ec_exp_rparen);
      }  /* if */
      remove_stop_token(tok_rparen);
    }  /* if */
    if (asm_name != NULL) {
      if (dps->declared_storage_class == (a_storage_class)sc_typedef) {
        pos_warning(ec_asm_name_in_typedef, &asm_start_pos);
      } else if (depth_innermost_function_scope != NO_SCOPE_DEPTH &&
                 (dps->declared_storage_class == (a_storage_class)sc_auto ||
                  dps->declared_storage_class ==
                                           (a_storage_class)sc_unspecified) &&
                 !(is_function_type(dps->type) &&
                   !dps->is_old_style_param_decl)) {
        /* Automatic variables can only have an asm() name if they are also
           declared with the "register" keyword. */
        pos_warning(ec_asm_name_on_auto_variable, &asm_start_pos);
      } else {
        dps->asm_name = asm_name;
        dps->asm_name_pos = asm_name_pos;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* scan_gnu_asm_name */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_symbol_ptr curr_type_symbol(a_boolean is_new_type_name,
                              a_boolean in_prescan,
                              a_boolean in_type_check)
/*
If the current token is an identifier or, in C++, the "::" at the start of a
global qualified name, and if it starts the name of a type (a typedef name or,
in C++, the name of a class, struct, union, or enum), return a pointer to
the symbol.  Otherwise, return NULL.  Ambiguity and access control checking
is not done.  in_prescan is TRUE when we are called from the prescanning
routines used for disambiguation.  This flag suppresses errors that
might result from class template names that are missing argument lists.
in_type_check is used when this routine is called directly or indirectly
from places such as is_type_start.
*/
{
  a_symbol_ptr               assoc_symbol;
  a_boolean                  err;
  an_identifier_options_set  options;

  assoc_symbol = NULL;
  /* Set the options.  Since this call is a "probe" to determine if the
     current identifier is a type name, don't complain if the name is that
     of a template but there are no template args (since it may actually
     be a different use of the name). */
  options = GID_NO_OPTIONS;
  if (is_new_type_name) options |= GID_IS_NEW_TYPE_NAME;
  if (in_prescan) options |= GID_TEMPLATE_ARGS_OPTIONAL;
  if (is_generalized_identifier_start(options)) {
    if (locator_for_curr_id.is_operator_name ||
        locator_for_curr_id.is_conversion_name) {
      /* Cannot be a type name. */
    } else {
      /* Look up the current token identifier, which may be a qualified name.
         Since curr_type_symbol is often called as part of a test of the
         presence of a type name identifier, it is inappropriate to cause a
         projection symbol to be created in the current scope if in fact it
         projects something other than a type name.  It's easier to suppress
         the creation of such gratuitous projections here than to try to ignore
         them in symbol entry later.  Defer any access errors that may occur
         because we may actually be scanning something that is not a type
         (e.g., a declarator). */
      a_symbol_header_ptr  saved_header = locator_for_curr_id.symbol_header;
      /* Save locator_for_curr_id: If the lookup finds something that is not a
         type, we will restore the saved value.  This is more thorough than a
         call to clear_specific_symbol to account for e.g. changes to the
         symbol_header field when a constructor is found (constructors have
         their own symbol header not in the main symbol table). */
      assoc_symbol =
          coalesce_and_lookup_generalized_identifier(options,
                                                     ilm_tentative_type, &err);
      if (assoc_symbol != NULL && !is_type_symbol(assoc_symbol)) {
        /* Symbol was found, but it is not a type name symbol.  Return NULL,
           clear the specific symbol (to avoid biasing future lookups), and
           restore the symbol header (if a constructor was found, it may have
           been changed to a special header that is not part of the main
           symbol table). */
        assoc_symbol = NULL;
        clear_specific_symbol(locator_for_curr_id);
        locator_for_curr_id.symbol_header = saved_header;
      }  /* if */
      /* Check to see if this is a pack reference. */
      if (!in_prescan && !in_type_check && assoc_symbol != NULL) {
        record_potential_pack_reference(assoc_symbol, &pos_curr_token);
      }  /* if */
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_type_symbol */


/*
Macro that is TRUE if the current token is an identifier that represents
the name of a type (a typedef name or, in C++, the name of a class, struct,
union, or enum).  Also works if the current is the "::" at the start of
a global qualified name.
*/
#define type_name_next(options) (is_generalized_identifier_start(options) && \
                                 curr_id_is_type_name())


a_boolean is_type_start(a_boolean is_expr_context)
/*
Return TRUE if the current token looks like the start of a type.  A type
starts with a type-specifier (including a typedef name) or a type-qualifier.
is_expr_context is TRUE if this is called from a context in which an
expression is permitted.
*/
{
  a_boolean    is_start = FALSE;

  if (curr_token == tok_decltype) {
    /* A decltype could be decltype(x) or decltype(x)::something.  In the
       latter case we need to coalesce it before deciding what it is.  */
    (void)is_generalized_identifier_start(GID_NO_OPTIONS);
  }  /* if */
  if ((is_type_specifier() &&
       !(is_expr_context && list_init_enabled &&
         is_type_keyword(curr_token) && next_token() == tok_lbrace)) ||
      is_type_qualifier() || is_function_specifier() ||
      curr_token == tok_friend || curr_token == tok_constexpr) {
    is_start = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled && is_expr_context && is_type_keyword(curr_token) &&
        next_token() == tok_colon_colon) {
      /* Something like int::something.  In C++/CLI this is an expression.
         Typically something like int::Parse("1"). */
      is_start = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    an_identifier_options_set  gid_options = GID_NO_OPTIONS;
    if (is_expr_context) {
      gid_options |= GID_IS_EXPR_CONTEXT;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_bugs) {
      /* Microsoft treats Q::X as just X if Q denotes the class currently
         being defined. */
      gid_options |= GID_SIMPLIFY_CURR_CLASS_QUALIFIED_NAME;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (type_name_next(gid_options)) {
      /* Identifier that is a type name (a typedef name or, in C++,
         the name of a class, struct, or union).  A type name cannot be
         followed by an opening brace of an initializer list. */
      is_start = !(is_expr_context &&
                   list_init_enabled && next_token() == tok_lbrace);
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (curr_token == tok_microsoft_w64) {
      is_start = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (curr_token == tok_identifier && locator_for_curr_id.is_error &&
               locator_for_curr_id.is_template_id) {
      /* This is an error case -- presumably, an ill-formed template-id -- but
         it is treated as the start of a type anyway. */
      is_start = TRUE;
    }  /* if */
  }  /* if */
  return(is_start);
}  /* is_type_start */


a_boolean is_decl_start(an_is_decl_start_options_set options)
/*
Return TRUE if the current token looks like the start of a declaration,
i.e., it is the start of a type-specifier, a type-qualifier, or a
storage-class-specifier.  Note that this does not cover the start of
function-definitions, since they can start with the declarator.  "options"
provides information about the current context that affects the kinds
of declarations that are permitted.
*/
{
  a_boolean     is_start = FALSE;
  a_token_kind  next_tok;
  a_boolean	expr_context = (options & IDS_EXPR_CONTEXT) != 0;

  if (is_storage_class()) {
    /* A storage-class-specifier. */
    is_start = TRUE;
  } else if (curr_token == tok_template || curr_token == tok_export) {
    /* Probably an error. */
    is_start = TRUE;
  } else if (curr_token == tok_static_assert) {
    /* static_assert is (syntactically) a declarative construct. */
    is_start = TRUE;
  } else if (curr_token == tok_constexpr) {
    /* constexpr is always a specifier for a declaration. */
    is_start = TRUE;
  } else if (is_type_start(expr_context)) {
    /* Is start of type. */
    is_start = TRUE;
  } else if (curr_token == tok_lbracket) {
    /* If this is a standard attribute ([[ ... ]]) or a Microsoft attribute,
       a declaration can follow.  However, if this is a lambda, that is not
       the case. */
    if (std_attribute_tokens_next()) {
      /* A standard attribute. */
      is_start = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode && (options & IDS_MS_ATTRIB_NOT_ALLOWED) == 0 &&
               !is_lambda()) {
      /* A Microsoft attribute. */
      is_start = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  } else if (curr_token == tok_alignas) {
    /* "alignas(...)" is really just a funny attribute syntax. */
    is_start = TRUE;
#if GNU_EXTENSIONS_ALLOWED
  } else if (curr_token == tok_attribute) {
    /* An attribute can start a declaration. */
    is_start = TRUE;
  } else if (curr_token == tok_extension) {
    /* The __extension__ keyword could be followed by an arbitrary expression
       or declaration.  Cache the token and recursively examine what
       follows. */
    a_token_cache  cache;
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_curr_token(&cache);
    (void)get_token();
    is_start = is_decl_start(options);
    rescan_cached_tokens(&cache);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  } else if (is_sun_link_scope_specifier()) {
    is_start = TRUE;
#endif /* SUN_EXTENSIONS_ALLOWED */
  } else if (curr_token == tok_identifier &&
             !(expr_context && list_init_enabled &&
               next_token() == tok_lbrace) &&
             !is_error_locator(locator_for_curr_id)) {
    /* A special check to produce better error recovery in certain cases.
       If the lexical sequence suggests that this is a declaration even
       though the current identifier is not defined (and therefore not
       recognized as a type name), call it a declaration anyway.  A type
       name cannot be followed by an opening brace of an initializer list. */
    if ((options & IDS_REAL_DECLARATOR_ALLOWED) == 0) {
      /* With a sizeof or cast operation, real_declarator_allowed will come
         in as FALSE.  There's no point in looking ahead in such cases:
         "sizeof(x y)" isn't syntactically possible, so if x is not a
         type name, we'll assume it's an object name. */
    } else if (symbol_list_from_locator(locator_for_curr_id) == NULL) {
      /* If the current token is an identifier we'll proceed with the error
         recovery optimization only if we can be quite sure the name can't
         have another meaning.  The first indication of that is that there
         are no active symbols with this name in scope.  However, if the
         scope stack has a class reactivation entry on it or a class that
         itself has base classes, a deactivated symbol (one from the inactive
         list) may be visible.  Note that that this is an issue only if we
         are in a context that accepts an expression and there are inactive
         symbols associated with this name. */
      if (expr_context &&
          inactive_symbol_list_from_locator(locator_for_curr_id) != NULL &&
          scope_stack[depth_scope_stack].inactive_symbols_may_be_visible) {
        /* The scope stack contains either a class with base classes or a
           reactivated class.  In either case there may be a member among the
           inactive symbols, so we will suppress the optimization. */
      } else {
        /* An undefined identifier.  Check the next token -- we may have a
           token pattern that can be nothing but a declaration. */
        next_tok = next_token();
        if (next_tok == tok_identifier || next_tok == tok_operator) {
          /* Pattern "x y" or "x operator..." -- looks like a declaration in
             'most any context. */
          is_start = TRUE;
        } else if (!expr_context &&
                   (next_tok == tok_star || next_tok == tok_ampersand ||
                    (rvalue_references_enabled && next_tok == tok_and_and)
                    or_is_cli_declarator_operator(next_tok))) {
          /* Pattern "x *..." or x &..." -- looks like a declaration as long as
             the context rules out expressions. */
          is_start = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return(is_start);
}  /* is_decl_start */


a_boolean f_check_for_overload_anachronism(void)
/*
Issue a diagnostic, bypass the current token, which is "overload", and check
the tokens that follow.  If a declaration is of the format "overload f;" (or
"overload f, g, h;") just check for syntax errors and discard the entire
declaration; in such cases return TRUE.  Otherwise, return FALSE --
declaration processing will continue as though "overload" had not been seen.
(This function is only called from the macro check_for_overload_anachronism.)
*/
{
  a_boolean     discard_declaration = FALSE;
  a_token_kind  next_tok;

  db_enter(3, "f_check_for_overload_anachronism");
  check_assertion(curr_token == tok_overload);
  /* Issue an anachronism diagnostic indicating that "overload" is
     no longer allowed.  This can be either an error or a warning. */
  diagnostic(anachronism_error_severity, ec_overload_anachronism);
  /* Bypass "overload" */
  (void)get_token();
  if (curr_token == tok_identifier) {
    next_tok = next_token();
    if (next_tok == tok_semicolon || next_tok == tok_comma) {
      /* We have a single function name or a comma separated list of
         function names.  (We do not support a mixed list of function
         names and function declarations.) Throw away the identifier
         and advance to the ";" or ",". */
      (void)get_token();
      if (curr_token == tok_comma) {
        /* It is a list of names.  Loop through them just to flag syntax
           errors. */
        add_stop_token(tok_semicolon);
        /* Advance past the comma */
        (void)get_token();
        do {
          (void)required_token(tok_identifier, ec_exp_identifier);
        } while (loop_token(tok_comma));
        remove_stop_token(tok_semicolon);
      }  /* if */
      /* The check for the final semicolon is done by the caller. */
      /* Tell the caller to do no more processing. */
      discard_declaration = TRUE;
    } else {
      /* Treat this as a function declaration.  Having bypassed the overload
         keyword we return to the caller. */
    }  /* if */
  }  /* if */
  db_exit();
  return discard_declaration;
}  /* f_check_for_overload_anachronism */


a_boolean check_member_function_typedef(a_type_ptr         tp,
                                        a_source_position  *pos)
/*
If tp is a "member function typedef" (cfront compatibility mode only) issue
an error diagnostic and return TRUE.
*/
{
  a_boolean     is_member_function_typedef = FALSE;
  a_type_ptr    rout_type, class_type;
  a_symbol_ptr  sym;

  if (is_cfront_member_function_typedef(tp, &rout_type, &class_type, &sym)) {
    pos_sy_error(ec_bad_use_of_member_function_typedef, pos, sym);
    is_member_function_typedef = TRUE;
  }  /* if */
  return is_member_function_typedef;
}  /* check_member_function_typedef */


#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* <-- attributes is not used in this case. */
#endif /* !GNU_EXTENSIONS_ALLOWED */
void adjust_parameter_type(a_type_ptr  *type_ptr)
/*
*type_ptr points to the type of a parameter.  Modify the type if
necessary.  See 3.7.1:  A declaration of a parameter as "array of
type" shall be adjusted to "pointer to type", and the declaration of
a parameter as "function returning type" shall be adjusted to
"pointer to function returning type", as in 3.2.2.1.
*/
{
  db_enter(4, "adjust_parameter_type");
  /* Note that incomplete types are allowed.  One can't call a function
     that has an incomplete-type parameter, but one can complete it later. */
  if (is_array_type(*type_ptr)) {
    /* Array, adjust to pointer to element type. */
    a_type_qualifier_set  qualifiers =
                           skip_typerefs(*type_ptr)->variant.array.qualifiers;
    *type_ptr = make_pointer_type(array_element_type(*type_ptr));
    /* A parameter type that is restrict-qualified-array-of-T decays into
       restrict-qualified-ptr-to-T.  (Same with const and volatile in C99.) */
    if (qualifiers != TQ_NONE) {
      *type_ptr = make_qualified_type(*type_ptr, qualifiers);
    }  /* if */
  } else if (is_function_type(*type_ptr)) {
    /* Function, adjust to pointer to function. */
    *type_ptr = make_pointer_type(*type_ptr);
  }  /* if */
  db_exit();
}  /* adjust_parameter_type */


static void report_qualifiers_as_useless(a_type_ptr         *type_ptr,
                                         a_source_position  *error_pos)
/*
An parameter, variable, or function is about to be declared with the given
type.  If it is a qualified type, the qualification is useless: Issue a
warning.
*/
{
  if (get_type_qualifiers(*type_ptr)
#if NEAR_AND_FAR_ALLOWED
                                     & ~(TQ_NEAR|TQ_FAR)
#endif /* NEAR_AND_FAR_ALLOWED */
                                                        ) {
    /* The type has type qualifiers. */
    pos_warning(ec_useless_type_qualifiers, error_pos);
    /* The useless qualifiers could be removed by the statement
         *type_ptr = make_unqualified_type(*type_ptr);
       but they are kept in case the back end assigns any meaning to them. */
  }  /* if */
}  /* report_qualifiers_as_useless */


static void check_ptr_or_ref_to_unspecified_bound_array(
                                                a_type_ptr         tp,
                                                a_source_position  *error_pos)
/*
Check that the given parameter type does not include a reference or a pointer
to an array of unspecified bound and issue an error at the given position if
needed.  This restriction was introduced in the standard to avoid having
certain constructs valid in both C and C++ mean different things in those two
languages.  For example:
    void f(int (*)[]);
    void f(int (*)[3]);  // Redeclaration in C, but could be an overloaded
                         // declaration in C++.

Nested function declarator parameters are not examined, since they will have
been checked earlier.  Template arguments and pointer-to-members are not
examined either (they pose no problem).  In some modes the constraints are
relaxed: see the configurations macros
DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE and
DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE.
*/
{
  a_boolean   is_ref = FALSE;

  tp = skip_typerefs(tp);
  for (;;) {
    if (tp->kind == (a_type_kind)tk_pointer) {
      is_ref = tp->variant.pointer.is_reference;
      tp = type_pointed_to(tp);
      tp = skip_typerefs(tp);
    } else if (tp->kind == (a_type_kind)tk_routine) {
      tp = skip_typerefs(tp->variant.routine.return_type);
    } else if (tp->kind == (a_type_kind)tk_array) {
      if (is_incomplete_array_type(tp)) {
        if (ref_to_unknown_bound_array_allowed_in_param_type && is_ref) {
          check_assertion(ptr_to_unknown_bound_array_allowed_in_param_type);
        } else if (!ptr_to_unknown_bound_array_allowed_in_param_type ||
                   is_ref) {
          pos_error(is_ref ? ec_param_type_ref_array_of_unknown_bound :
                             ec_param_type_ptr_to_array_of_unknown_bound,
                    error_pos);
          break;
        }  /* if */
      }  /* if */
      tp = skip_typerefs(tp->variant.array.element_type);
    } else {
      /* No need to look further. */
      break;
    }  /* if */
  }  /* for */
}  /* check_ptr_or_ref_to_unknown_bound_array */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean in_cppcx_externally_visible_parameter_scope()
/*
Return TRUE if the current function prototype scope is that of a delegate
definition or a class member with external visibility.
*/
{
  a_scope_stack_entry *ssep = &scope_stack_top();

  check_assertion(scope_is(ssep, sck_func_prototype));
  /* Skip the function prototype scope. */
  --ssep;
  /* Skip any template declaration scopes to get the enclosing class,
     file, or namespace scope. */
  while (ssep->kind == (a_scope_kind)sck_template_declaration) {
    --ssep;
  }  /* while */
  return ssep->scanning_cli_delegate_definition ||
         (scope_is(ssep, sck_class_struct_union) &&
          is_immediate_managed_class_type(ssep->assoc_type) &&
          is_cppcx_externally_visible_assembly_access(
                                              ssep->current_assembly_access));
}  /* in_cppcx_externally_visible_parameter_scope */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean is_special_rvalue_ref_generic_parameter_at_pos(
                                                    a_symbol_ptr   func_templ,
                                                    unsigned long  param_pos)
/*
Return true if the parameter of the given function template at the given
position (1, 2, 3, ...) is of the form "T&&" where T is a parameter of the
function template.  (Such parameters are subject to special deduction rules,
and in Microsoft mode the constraint that T not be substituted by an array of
unknown length is not enforced for such parameters.)
*/
{
  a_boolean                 result = FALSE;
  a_type_ptr                ftp;
  a_param_type_ptr          ptp;
  a_template_decl_info_ptr  tdip;

  check_assertion(param_pos >= 1 &&
                  func_templ != NULL &&
                  symbol_is(func_templ, sk_function_template));
  ftp = func_templ->variant.template_info->variant.function.routine->type;
  check_assertion(ftp->kind == (a_type_kind)tk_routine);
  ptp = ftp->variant.routine.extra_info->param_type_list;
  check_assertion(ptp != NULL);
  /* Move ptp to the given numbered parameter if necessary. */
  while (ptp->param_num != param_pos) {
    ptp = ptp->next;
    check_assertion(ptp != NULL);
  }  /* while */
  tdip = func_templ->variant.template_info->cache.decl_info;
  if (tdip != NULL &&
      is_parameter_type_with_special_ref_deduction(ptp->type,
                                                   tdip->parameters)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_special_rvalue_ref_generic_parameter_at_pos */


void check_and_adjust_parameter_type(a_decl_parse_state  *dps,
                                     unsigned long       param_num,
                                     a_source_position   *error_pos)
/*
This routine is called for all function parameter declarations.  It does
error checking and type adjustments as required.  dps describes the parameter
declaration.  param_num is the ordinal position of the parameter (zero for
old-style parameter declarations).  error_pos is the default position for
diagnostics.
*/
{
  if (any_cfront_mode() &&
      check_member_function_typedef(dps->type, error_pos)) {
    /* The type is a cfront-style member function typedef -- it is an error
       to use it anywhere but in a pointer-to-member declaration. */
    dps->type = error_type();
  } else {
    /* Verify that the parameter type is not a qualified function type. */
    a_type_ptr  rtp = skip_typerefs(dps->type);
    if (type_is_typedef(dps->type) &&
        rtp->kind == (a_type_kind)tk_routine &&
        (rtp->variant.routine.extra_info->qualifiers != TQ_NONE ||
         rtp->variant.routine.extra_info->this_qualifiers != TQ_NONE)) {
      pos_error(ec_bad_qualified_function_type_parameter, error_pos);
    }  /* if */
    /* Adjust the type if necessary (for example, "array of x" becomes
       "pointer to x"). */
    adjust_parameter_type(&dps->type);
    /* Disallow "void" as a parameter type. */
    if (is_void_type(rtp)) {
      pos_error(ec_void_param_not_allowed, error_pos);
      dps->type = error_type();
#if UPC_EXTENSIONS_ALLOWED
    } else if (upc_mode && is_shared_qualified_type(dps->type)) {
      /* Do not allow directly shared (i.e. non-pointer) parameter types. */
      pos_error(ec_shared_parameter, error_pos);
      dps->type = error_type();
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    } else if (type_qualified_with_named_address_space(dps->type)) {
      pos_error(ec_named_address_space_for_parameter, error_pos);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled && is_pin_ptr_type(dps->type)) { 
      /* A pin pointer cannot be used as a parameter type. */
      pos_error(ec_pin_ptr_param_not_allowed, error_pos);
    } else if (cli_or_cx_enabled && is_cli_interface_type(dps->type)) { 
      /* A C++/CLI interface cannot be used as a parameter type. */
      pos_error(ec_parameter_with_interface_type, error_pos);
      dps->type = error_type();
    } else if (cppcx_enabled &&
               is_handle_to_nonconst_cppcx_plain_array_type(dps->type) &&
               in_cppcx_externally_visible_parameter_scope()) {
      /* If the function is a delegate definition or a class member with
         external visibility, issue an error if the parameter is of type
         "Platform::Array<T>^". */
      pos_error(ec_cppcx_non_const_array_parameter, error_pos);
      dps->type = error_type();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      if (!C_mode() && !(ptr_to_unknown_bound_array_allowed_in_param_type &&
                         ref_to_unknown_bound_array_allowed_in_param_type)) {
        /* In C++ disallow a parameter type that includes a pointer or
           reference to an array of unspecified size.  This restriction is
           relaxed in cfront mode and (for the pointer case) in Microsoft mode;
           it can also be relaxed in default mode -- see
           DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE and
           DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE.
           The check is also skipped for a reference to an array type during
           instantiations in GNU C++ mode; the same is true in Microsoft mode,
           but only if the reference to array type was produced through the
           (special) deduction from a parameter of the form T&& (with T a
           template parameter). */
        if (!dps->is_old_style_param_decl &&
            dps->assoc_func_decl_state->is_template_rescan &&
            scope_stack[depth_scope_stack-1].function_partial_instantiation &&
            (gpp_mode ||
             (microsoft_mode && rvalue_references_enabled &&
              /* Ignore nested function declarators and function declarators
                 in return types. */
              dps->assoc_func_decl_state->assoc_func_decl_state == NULL &&
              !dps->assoc_func_decl_state->function_declarator_seen &&
              is_special_rvalue_ref_generic_parameter_at_pos(
                                scope_stack[depth_scope_stack-1].template_sym,
                                param_num)))) {
        } else {
          check_ptr_or_ref_to_unspecified_bound_array(dps->type, error_pos);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_and_adjust_parameter_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean is_implicit_array_new_or_delete_symbol(a_symbol_ptr  sym)
/*
Return TRUE if and only if the given symbol represents an implicitly
declared array new or delete operator.
*/
{
  a_boolean  result = FALSE;

  if (sym->kind == (a_symbol_kind)sk_routine &&
      sym->decl_position.seq == 0 &&
      sym->decl_scope == file_scope_number) {
    /* In Microsoft mode with microsoft_version >= 1400, the predeclared array
       new and delete symbols point to the corresponding non-array routines.
       Rather than examining the routine entry, we must therefore look at the
       symbol's identifier string. */
    result = strcmp(sym->header->identifier, "operator new[]") == 0 ||
             strcmp(sym->header->identifier, "operator delete[]") == 0;
  }  /* if */
  return result;
}  /* is_implicit_array_new_or_delete_symbol */


a_boolean valid_static_conversion_class_type(a_type_ptr  tp,
                                             a_type_ptr  class_type)
/*
Return TRUE if tp is T or T^ where T is class_type, or any reference (lvalue,
rvalue, or tracking) to such a type.
*/
{
  if (is_any_reference_type(tp)) {
    tp = type_pointed_to(tp);
  }  /* if */
  if (is_handle_type(tp)) {
    tp = type_pointed_to(tp);
  }  /* if */
  tp = skip_typerefs(tp);
  return types_are_compatible(tp, class_type);
}  /* valid_static_conversion_class_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void check_operator_function_params(a_type_ptr        rout_type,
                                    a_type_ptr        class_type,
                                    a_symbol_locator  *locator)
/*
Check the argument list on the declaration of a user-defined conversion
or overloaded operator function.  For member function (and member function
template) declarations, class_type indicates the enclosing class.
The rules for standard C++ are as follows.  For conversion functions, no
arguments are allowed.  For operators there are different requirements for
different operator kinds.  Issue a diagnostic if an error is found.
In C++/CLI mode, additional possibilities exist: Member operators can be
static member functions, and parameter types can be handles or tracking
references.
If this routine is modified to use additional fields from the locator
make_template_function needs to be updated to make sure that the
new fields are set properly.
*/
{
  an_opname_kind                 opname;
  int                            param_count;
  a_param_type_ptr               ptp;
  a_boolean                      any_class_or_enum_type_params = FALSE;
  a_boolean                      any_template_param_type_params = FALSE;
  a_type_ptr                     tp;
  a_boolean                      is_nonstatic_member_function;
  an_error_code                  error_code = ec_no_error;
  a_boolean                      err = FALSE;
  a_routine_type_supplement_ptr  rtsp;

  db_enter(4, "check_operator_function_params");
  rout_type = skip_typerefs(rout_type);
  rtsp = rout_type->variant.routine.extra_info;
  if (is_error_locator(*locator)) {
    /* Nothing to do. */
  } else if (locator->is_conversion_name) {
    check_assertion(class_type != NULL);
    if (cli_or_cx_enabled && rtsp->this_class == NULL) {
      /* A C++/CLI static conversion function. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (rtsp->param_type_list == NULL ||
          rtsp->param_type_list->next != NULL ||
          rtsp->has_ellipsis) {
        pos_error(ec_static_conversion_function_must_have_one_parameter,
                  &locator->source_position);
        err = TRUE;
      } else if (is_cli_param_array_routine_type(rout_type)) {
        /* A C++/CLI parameter array cannot be used in a static conversion
           operator. */
        pos_error(ec_parameter_array_on_operator_function,
                  &locator->source_position);
        err = TRUE;
      } else {
        /* Ensure the argument type or conversion-id type is T or T^ (or any
           kind of reference to T or T^), with T the type indicated by
           class_type. */
        if (valid_static_conversion_class_type(rtsp->param_type_list->type,
                                               class_type)) {
          /* The destination type is not the parent class.  If it is another
             class (or a handle thereto), mark that flag as being the target
             of a user-defined conversion function. */
          set_target_of_conversion_function_flag_if_needed(
                                     locator->variant.conversion_result_type);
        } else if (!valid_static_conversion_class_type(
                       locator->variant.conversion_result_type, class_type)) {
          pos_ty_error(ec_bad_parameter_type_for_static_member_operator,
                       &locator->source_position, class_type);
          err = TRUE;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (rtsp->param_type_list != NULL || rtsp->has_ellipsis) {
      /* Any parameter is too many for a (standard) conversion function. */
      pos_error(ec_too_many_args_for_conversion, &locator->source_position);
      err = TRUE;
    }  /* if */
  } else if (locator->is_operator_name) {
    /* It's an operator. */
    a_boolean  this_equivalent_seen = FALSE;
    opname = locator->variant.opname;
    check_assertion(opname != (an_opname_kind)onk_none);
    is_nonstatic_member_function = (rtsp->this_class != NULL);
#if CHECKING
    if (is_new_operator(opname) || is_delete_operator(opname)) {
      /* Operator new/delete cannot be a nonstatic member function. */
      check_assertion_str2(!is_nonstatic_member_function,
                           "check_operator_function_params:",
                           "new or delete is nonstatic member function");
    }  /* if */
#endif /* CHECKING */
    /* Make a pass over the param types list to count the number of
       arguments to see if there are any parameters that are of class type
       or reference-to-class type.  Note that param_count is initialized to
       0 except in the case of nonstatic member functions, for which it is
       initialized to 1. This is because the implicit "this" parameter is
       counted in the latter case. */
    param_count = is_nonstatic_member_function ? 1 : 0;
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      param_count++;
      tp = ptp->type;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled && class_type != NULL &&
          !is_nonstatic_member_function &&
          cli_class_type_kind_is(class_type, cctk_value)) {
        /* For special value class types (like System::Double) that correspond
           to fundamental types, static member operators will have those
           fundamental types as parameter types.  This satisfies the
           requirement of a parameter matching class_type. */
        a_type_ptr  fund = fundamental_type_from_system_type(class_type);
        if (fund != NULL &&
            types_are_compatible_ignoring_qualifiers(tp, fund)) {
           this_equivalent_seen = TRUE;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (is_any_reference_type(tp)) {
        tp = type_pointed_to(tp);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled && is_handle_type(tp)) {
        /* Parameters of the form T^, T^%, and T^& are also acceptable in
           C++/CLI mode. */
        tp = type_pointed_to(tp);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (is_class_struct_union_type(tp) ||
          (operator_overloading_on_enums_enabled && is_enum_type(tp))) {
        any_class_or_enum_type_params = TRUE;
        if (cli_or_cx_enabled && class_type != NULL &&
            !is_nonstatic_member_function &&
            types_are_compatible_ignoring_qualifiers(tp, class_type)) {
          /* For static C++/CLI member operators, at least one parameter must
             T, T&, T&&, T%, or T^, with T the type indicated by class_type
             (a template parameter is not sufficient); i.e., a parameter
             similar to "this" in a nonstatic member version of the operator.
             We record here that such a parameter was seen. */
          this_equivalent_seen = TRUE;
        }  /* if */
      } else if (is_template_param_type(tp)) {
        any_template_param_type_params = TRUE;
      }  /* if */
    }  /* for */
    if (is_new_operator(opname) ||
        is_delete_operator(opname) ||
        opname == (an_opname_kind)onk_function_call) {
      /* Function call and new must have one or more arguments. */
      if (param_count == 0) {
        if (rtsp->has_ellipsis) {
          /* operator()(...) and operator new(...) are errors, but we do
             allow operator()(T, ...) and operator new(size_t, ...). */
          error_code = ec_ellipsis_on_operator_function;
        } else {
          error_code = ec_too_few_args_for_operator;
        }  /* if */
      } else if (opname != (an_opname_kind)onk_function_call) {
        ptp = rout_type->variant.routine.extra_info->param_type_list;
        tp = ptp->type;
        if (!is_error_type(tp)) {
          if (is_new_operator(opname)) {
            /* operator new or operator new[]. */
            if (!is_integral_type(tp) ||
                skip_typerefs(tp)->variant.integer.int_kind !=
                                                      targ_size_t_int_kind) {
              error_code = ec_bad_arg_type_for_operator_new;
              ptp->type = error_type();
            }  /* if */
          } else {
            /* operator delete or operator delete[]. */
            if (!is_void_star_type(tp)) {
              /* Error. */
              an_error_severity  severity;
              if (cfront_2_1_mode && is_pointer_type(tp) &&
                  is_void_type(type_pointed_to(tp))) {
                /* In cfront 2.1 "const void *" is allowed.   Issue a warning
                   and ignore the qualifier on the type. */
                severity = es_warning;
                ptp->type = make_pointer_type(void_type());
              } else {
                /* Error case. */
                severity = es_error;
                ptp->type = error_type();
                err = TRUE;
              }  /* if */
              pos_diagnostic(severity,
                             ec_bad_first_arg_type_for_operator_delete,
                             &locator->source_position);
            }  /* if */
            /* Actually using placement delete only occurs when exception
               handling is enabled, and only with newer ABIs.  If EH support
               is disabled or an old ABI is used, issue a diagnostic if this
               turns out to be a placement delete declaration. */
            if (!err
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
                     && !exceptions_enabled
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
                                           ) {
              ptp = ptp->next;
              if (ptp != NULL) {
                /* There is a second argument.  Except for the case in which
                   a class member operator delete has a second parameter type
                   of size_t, issue a diagnostic. */
                tp = skip_typerefs(ptp->type);
                if (!is_error_type(tp)) {
                  if (class_type != NULL && is_integral_type(tp) &&
                      tp->variant.integer.int_kind == targ_size_t_int_kind) {
                    /* No warning for X::operator delete(void *, size_t). */
                  } else {
                    pos_diagnostic(exceptions_enabled ? es_warning : es_remark,
                                   ec_useless_placement_delete,
                                   &locator->source_position);
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cppcli_enabled && is_cli_param_array_routine_type(rout_type)) {
      /* All overloaded operators (except "call" and "new", which are handled
         above) require a specific number of arguments.  A C++/CLI parameter
         array is therefore not allowed here. */
      error_code = ec_parameter_array_on_operator_function;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (rtsp->has_ellipsis) {
      /* All overloaded operators (except function call and new, handled
         above) require a specific number of arguments, so ellipsis is not
         allowed. */
      error_code = ec_ellipsis_on_operator_function;
    } else if (opname == (an_opname_kind)onk_compl ||
        opname == (an_opname_kind)onk_not ||
        opname == (an_opname_kind)onk_arrow) {
      /* Unary operator must have exactly one argument. */
      if (param_count > 1) {
        error_code = ec_too_many_args_for_operator;
      } else if (param_count < 1) {
        error_code = ec_too_few_args_for_operator;
      }  /* if */
    } else if (param_count == 1 &&
               (opname == (an_opname_kind)onk_plus ||
                opname == (an_opname_kind)onk_minus ||
                opname == (an_opname_kind)onk_star ||
                opname == (an_opname_kind)onk_ampersand ||
                opname == (an_opname_kind)onk_plus_plus ||
                opname == (an_opname_kind)onk_minus_minus)) {
       /* These operators can be either unary or binary.  It is legal for
          them to have exactly one argument. */
    } else if (param_count == 2 &&
               (opname == (an_opname_kind)onk_plus_plus ||
                opname == (an_opname_kind)onk_minus_minus)) {
      /* Extra argument on postfix operator must be of type "int".  (This
         variant is not allowed as a C++/CLI static member operator.) */
      if (cli_or_cx_enabled && class_type != NULL &&
          !is_nonstatic_member_function) {
        error_code = ec_too_many_args_for_operator;
      } else {
        ptp = rout_type->variant.routine.extra_info->param_type_list;
        if (!is_nonstatic_member_function) ptp = ptp->next;
        tp = skip_typerefs(ptp->type);
        if (!is_error_type(tp) && !is_template_dependent_type(tp)) {
          if (!is_integral_type(tp) ||
              tp->variant.integer.int_kind != (an_integer_kind)ik_int) {
            pos_st_error(ec_bad_extra_arg_for_postfix_operator,
                         &locator->source_position,
                         (char *)(opname == (an_opname_kind)onk_plus_plus
                                                              ? "++" : "--"));
            ptp->type = error_type();
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Binary operator must have exactly two arguments. */
      if (param_count > 2) {
        error_code = ec_too_many_args_for_operator;
      } else if (param_count < 2) {
        error_code = ec_too_few_args_for_operator;
      }  /* if */
    }  /* if */
    if (error_code != ec_no_error) {
      pos_error(error_code, &locator->source_position);
      err = TRUE;
    }  /* if */
    if (is_new_operator(opname) || is_delete_operator(opname)) {
      /* Check return type. */
      tp = rout_type->variant.routine.return_type;
      if (!is_error_type(tp)) {
        if (is_new_operator(opname)) {
          /* operator new or operator new[]: return type must be "void *". */
          if (!is_void_star_type(tp) || is_qualified_type(tp)) {
            pos_error(ec_bad_return_type_for_op_new,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        } else {
          /* operator delete or operator delete[]: return type must be
             "void". */
          if (!is_void_type(tp) || is_qualified_type(tp)) {
            pos_error(ec_bad_return_type_for_op_delete,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* If operator function is not a nonstatic member and does not have
         operands of class or enum type (or reference to class or enum type),
         issue an error.  This restriction does not apply to new and delete,
         however.  Also, in C++/CLI mode, static member operators of special
         value class types (like System::Double) that correspond to fundamental
         types can have those fundamental types as the only parameter types. */
      if (!is_nonstatic_member_function && !any_class_or_enum_type_params &&
          !any_template_param_type_params && !this_equivalent_seen) {
        pos_error(operator_overloading_on_enums_enabled ?
                        ec_no_params_with_class_or_enum_type :
                        ec_no_params_with_class_type,
                  &locator->source_position);
        err = TRUE;
      } else if (cli_or_cx_enabled && class_type != NULL &&
                 !is_nonstatic_member_function && !this_equivalent_seen) {
        pos_ty_error(ec_bad_parameter_type_for_static_member_operator,
                     &locator->source_position, class_type);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) set_to_error_locator(*locator);
  db_exit();
}  /* check_operator_function_params */


a_boolean is_single_param_operator_new_or_delete(
                                             a_symbol_locator *locator,
                                             a_type_ptr       type,
                                             a_boolean        include_nothrow)
/*
Return TRUE if the locator is for an operator new or delete and the type
indicates that it is the default version (i.e., if it has exactly one
parameter, which elsewhere is confirmed to have type size_t (new) or void*
(delete).  If include_nothrow is TRUE, return TRUE also for a two-parameter
operator new or delete whose second parameter has type std::nothrow_t const&.
*/
{
  a_boolean         match = FALSE;
  a_param_type_ptr  ptp;

  if (locator->is_operator_name &&
      (is_new_operator(locator->variant.opname) ||
       is_delete_operator(locator->variant.opname))) {
    check_assertion(is_function_type(type));
    ptp = (skip_typerefs(type))->variant.routine.extra_info->param_type_list;
    if (ptp != NULL) {
      if (ptp->next == NULL) {
        match = TRUE;
      } else if (include_nothrow && ptp->next->next == NULL) {
        /* Check whether the second parameter has type
           std::nothrow_t const&. */
        a_type_ptr  tp = ptp->next->type;
        if (is_lvalue_reference_type(tp)) {
          tp = type_pointed_to(tp);
          if (is_std_nothrow_type(tp) && get_type_qualifiers(tp) == TQ_CONST) {
            match = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* is_single_param_operator_new_or_delete */


static void report_bad_new_or_delete(a_symbol_locator    *locator,
                                     a_decl_parse_state  *dps)
/*
Issue a diagnostic when attempting to declare an operator new or delete
function that is a namespace member or that has internal linkage (i.e.,
dps->storage_class == sc_static).  Also diagnose attempts to declare an
inline allocation or deallocation function.  If an actual error is issued
mark *locator as an error locator (this is not done if only a warning is
issued).
*/
{
  an_error_code      error_code = ec_no_error;
  an_error_severity  severity = es_none;
  a_storage_class    storage_class = dps->storage_class;

  if (locator->is_operator_name && !locator->is_class_member &&
      (is_new_operator(locator->variant.opname) ||
       is_delete_operator(locator->variant.opname))) {
    /* A new or delete operator that is not a class member. */
    a_boolean          declared_inline = (dps->dso_flags & DSO_INLINE) != 0;
    a_source_position  *inline_diag_pos = &dps->inline_pos;
    if (depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE &&
        (!locator->is_qualified_name ||
         !locator->is_file_scope_qualified_name)) {
      /* This operator declaration either appears inside a namespace or else
         has the effect of injecting a declaration into a namespace. */
      if (microsoft_mode || (gpp_mode && gnu_version < 40000)) {
        /* Microsoft compilers and early GNU compilers accept namespace-scope
           new/delete operators.  (See also opname_function_symbol.) */
        severity = es_warning;
      } else {
        severity = es_error;
      }  /* if */
      error_code = is_new_operator(locator->variant.opname)?
                                        ec_allocation_operator_in_namespace :
                                        ec_deallocation_operator_in_namespace;
    } else if (storage_class == (a_storage_class)sc_static) {
      severity = strict_ansi_mode ? strict_ansi_error_severity : es_warning;
      error_code = ec_no_internal_linkage_for_new_or_delete;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (gpp_mode && !declared_inline) {
      an_attribute_ptr  ap = find_decl_attribute(ak_always_inline, dps);
      if (ap != NULL) {
        declared_inline = TRUE;
        inline_diag_pos = &ap->position;
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (declared_inline && is_function_type(dps->type) &&
        !dps->is_template_declaration &&
        is_single_param_operator_new_or_delete(locator, dps->type,
                                               /*include_nothrow=*/TRUE)) {
      /* The predefined operators new and delete cannot be declared "inline"
         (the standard does not require a diagnostic for this; hence, it's
         just a warning in default mode).  The GNU attribute "always_inline"
         is handled like the "inline" keyword in this context. */
      pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity
                                      : es_warning,
                     ec_inline_new_or_delete_operator, inline_diag_pos);
    }  /* if */
    if (error_code != ec_no_error) {
      diagnostic(severity, error_code);
      if (severity == es_error) {  /*lint !e774*/
        /* Set the is_error flag in the locator. */
        set_to_named_error_locator(*locator);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* report_bad_new_or_delete */


static a_boolean compare_exception_specification_type_list(
                              an_exception_specification_ptr  spec_1,
                              an_exception_specification_ptr  spec_2,
                              a_source_position_ptr           throw_pos,
                              an_error_code                   diff_msg,
                              an_error_code                   intro_msg,
                              a_symbol_ptr                    prev_sym,
                              a_boolean                       difference_seen)
/*
Helper function to report differences between two exception specifications
spec_1 and spec_2.  Normally this routine is called twice with the arguments
for spec_1 and spec_2 exchanged and diff_msg set to an error message that
reports missing or extraneous types.  The position of the keyword "throw" that
introduced the latest declaration is throw_pos.  The earlier declaration
resulted in symbol prev_sym.
If difference_seen is FALSE and a difference is seen in this comparison, an
introductory message is issued, and difference_seen is set to TRUE.
The routine returns difference_seen.
*/
{
  an_exception_specification_type_ptr  etype_1, etype_2;

  if (spec_1->is_noexcept) {
    etype_1 = NULL;
  } else {
    etype_1 = spec_1->variant.exception_specification_type_list;
  }  /* if */
  for (; etype_1 != NULL; etype_1 = etype_1->next) {
    if (etype_1->redundant) {
      /* Don't bother looking for a match on redundant types.  It will already
         have been done. */
    } else {
      a_boolean  match = FALSE;
      if (spec_2->is_noexcept) {
        etype_2 = NULL;
      } else {
        etype_2 = spec_2->variant.exception_specification_type_list;
      }  /* if */
      for (; etype_2 != NULL; etype_2 = etype_2->next) {
        if (!etype_2->redundant && etype_2->type != NULL &&
            identical_types(etype_1->type, etype_2->type)) {
          /* An entry of the same type was found on the list of spec_2. */
          match = TRUE;
          break;
        }  /* if */
      }  /* for */
      if (!match) {
        /* No match was found, so the list of spec_2 does not have a type that
           is on the list of spec_1. */
        if (!difference_seen) {
          /* The diagnostics will be combined with a header message followed
             by additional messages identifying the specific discrepancy.
             This is the first diagnostic, so put out the header message
             first. */
          pos_stsy_start_error(intro_msg, throw_pos, ":", prev_sym);
          difference_seen = TRUE;
        }  /* if */
        ty_add_diag_info(diff_msg, etype_1->type);
      }  /* if */
    }  /* if */
  }  /* for */
  return difference_seen;
}  /* compare_exception_specification_type_list */


static an_error_severity pos_adjusted_severity(an_error_severity  severity,
                                               a_symbol_ptr       prev_decl)
/*
If the given symbol was previously declared in a system header, limit the
given severity to a warning.  The adjustment is only made in GNU C++ mode.
(In GNU C++ mode, exception specification conflicts with declarations in
system headers are downgraded to warnings.)
*/
{
  if (gpp_mode && (int)severity > (int)es_warning &&
      seq_is_in_system_header(prev_decl->decl_position.seq)) {
    severity = es_warning;
  }  /* if */
  return severity;
}  /* pos_adjusted_severity */


static a_boolean is_template_dependent_noexcept_specification(
                                          an_exception_specification_ptr  esp)
/*
Return TRUE if esp (which may be NULL) points to an exception specification
entry for a noexcept-specification whose argument is template-dependent.
(The caller is responsible for ensuring that the argument has been parsed;
i.e., esp->arg_cached cannot be TRUE).
*/
{
  a_boolean  result = FALSE;

  if (esp != NULL && esp->is_noexcept) {
    check_assertion(!esp->arg_cached);
    result = esp->variant.noexcept_arg != NULL &&
             esp->variant.noexcept_arg->kind ==
                                      (a_constant_repr_kind)ck_template_param;
  }  /* if */
  return result;
}  /* is_template_dependent_noexcept_specification */


void check_exception_specification(a_type_ptr         new_rout_type,
                                   a_symbol_ptr       prev_decl,
                                   a_source_position  *throw_pos,
                                   a_boolean          is_redecl)
/*
Check that the throw specification on the current declaration, if any, is
consistent with that of the previous declaration.
*/
{
  a_boolean                       any_difference_seen;
  an_exception_specification_ptr  new_esp, old_esp;
  an_error_code                   error_code;
  a_routine_ptr                   rp = NULL;
  a_type_ptr                      prev_type = NULL;

  db_enter(4, "check_exception_specification");
  /* Retrieve the routine type of the previous declaration: */
  switch (prev_decl->kind) {
    case sk_routine:
    case sk_member_function:
      rp = prev_decl->variant.routine.ptr;
      prev_type = rp->type;
      break;
    case sk_extern_routine:
      rp = prev_decl->variant.extern_symbol_descr->variant.routine.ptr;
      prev_type = rp->type;
      break;
    case sk_function_template:
      rp = prev_decl->variant.template_info->variant.function.routine;
      prev_type = rp->type;
      break;
    case sk_variable:
      prev_type = prev_decl->variant.variable.ptr->type;
      break;
    case sk_static_data_member:
      prev_type = prev_decl->variant.static_data_member.variable->type;
      break;
    case sk_extern_variable:
      prev_type =
               prev_decl->variant.extern_symbol_descr->variant.variable->type;
      break;
    default:
      unexpected_condition_str(
                            "check_exception_specification: bad symbol kind");
  }  /* switch */
  if (is_or_contains_error_type(prev_type) ||
      is_or_contains_error_type(new_rout_type)) {
    /* Something went wrong earlier on; do not attempt to issue more
       diagnostics. */
    goto done;
  }  /* if */
  if (rp == NULL) {
    /* Not a routine type, but a pointer-to, reference-to or pointer-to-member
       function.  (C++/CLI handles and tracking references cannot refer to
       functions and are therefore not handled here.) */
    if (is_ptr_to_member_type(prev_type) &&
        is_ptr_to_member_type(new_rout_type)) {
      prev_type = pm_member_type(skip_typerefs(prev_type));
      new_rout_type = pm_member_type(skip_typerefs(new_rout_type));
    } else if (is_ptr_or_ref_type(prev_type) &&
               is_ptr_or_ref_type(new_rout_type)) {
      prev_type = type_pointed_to(skip_typerefs(prev_type));
      new_rout_type = type_pointed_to(skip_typerefs(new_rout_type));
    }  /* if */
  }  /* if */
  if (!(is_function_type(prev_type) && is_function_type(new_rout_type))) {
    /* There is something more fundamentally wrong that an exception
       specification mismatch (likely the same name is used for two very
       different kinds of entities).  Skip this processing. */
    goto done;
  }  /* if */
  if (exceptions_enabled) {
    an_error_severity  severity = es_error;
    if (microsoft_mode && microsoft_version >= 1300) {
      /* Recent Microsoft compilers do not require exception specifications
         on multiple declarations to match.  We issue a warning in case of
         a mismatch.  Note that calls to composite_type will ensure that
         the original specification is retained. */
      severity = es_warning;
    }  /* if */
    old_esp = skip_typerefs(prev_type)->
                         variant.routine.extra_info->exception_specification;
    new_esp = skip_typerefs(new_rout_type)->
                    variant.routine.extra_info->exception_specification;
    /* Set error_code for issuing diagnostics. */
    if (is_redecl) {
      /* This a function redeclaration -- the exception specifications have to
         match. */
      error_code = ec_incompatible_exception_specification;
    } else {
      /* Not a redeclaration -- probably a template specialization.
         In diagnostics refer to the template rather than a previous
         declaration of this instance. */
      error_code = ec_bad_exception_specification_for_specialization;
      /* Distinguish between (member) function specializations and static
         data member specializations: */
      if (rp != NULL && prev_decl->variant.routine.instance_ptr != NULL) {
        /* In diagnostics refer to template rather than a previous declaration
           of this instance. */
        prev_decl = prev_decl->variant.routine.instance_ptr->template_sym;
      } else if (rp == NULL &&
                 prev_decl->variant.static_data_member.instance_ptr != NULL) {
        prev_decl =
             prev_decl->variant.static_data_member.instance_ptr->template_sym;
      }  /* if */
    }  /* if */
    if (rp != NULL && rp->compiler_generated) {
      /* Ignore any differences between exception specifications on a
         compiler generated routine (e.g., predeclared operator new or delete)
         and the current declaration. */
      if (new_esp != NULL) {
        /* Reset the never_throws flag (it will be recomputed based on the
           current declaration). */
        rp->never_throws = FALSE;
      }  /* if */
    } else if (new_esp != NULL && new_esp->arg_cached) {
      /* Compatibility cannot be checked in some template cases. */
    } else if (rp != NULL && rp->is_prototype_instantiation &&
               (is_template_dependent_noexcept_specification(old_esp) ||
                is_template_dependent_noexcept_specification(new_esp))) {
      /* If template-dependent noexcept specifiers are involved, the argument
         constants must be equivalent. */
      if (old_esp == NULL || !old_esp->is_noexcept ||
          old_esp->variant.noexcept_arg == NULL ||
          new_esp == NULL || !new_esp->is_noexcept ||
          new_esp->variant.noexcept_arg == NULL ||
          !eq_constants(old_esp->variant.noexcept_arg,
                        new_esp->variant.noexcept_arg)) {
        pos_stsy_diagnostic(pos_adjusted_severity(severity, prev_decl),
                            error_code, throw_pos, "", prev_decl);
      }  /* if */
    } else if (old_esp == NULL || old_esp->throw_any) {
      /* Previous specification asserted that any exception may be thrown
         ("throw (...)", "noexcept(<false-constant>)", or no specification at
         all). */
      if (new_esp != NULL && !new_esp->throw_any) {
        /* The new declaration restricts the permitted exceptions.  Issue an
           error (except if the incompatibility is with a declaration from a
           system header in GNU C++ modes). */
        pos_stsy_diagnostic(pos_adjusted_severity(severity, prev_decl),
                            error_code, throw_pos, "", prev_decl);
      }  /* if */
    } else if (new_esp == NULL ||
               (new_esp->throw_any && !new_esp->is_noexcept)) {
      /* The new declaration may throw anything (the noexcept case is handled
         later since it requires a different diagnostic message) but the old
         declaration may not: Issue a diagnostic. */
      if (is_redecl && rp != NULL && !rp->source_corresp.is_class_member &&
          special_kind_is(rp, sfk_operator) &&
          (is_new_operator(rp->variant.opname_kind) ||
           is_delete_operator(rp->variant.opname_kind))) {
        /* Unless we are in strict mode, issue a warning instead of an error
           if this is a redeclaration of what may be a library new or delete
           routine: the relaxation is to ease the upgrading of old code. */
        severity = strict_ansi_mode ? strict_ansi_error_severity : es_warning;
      } else if (gpp_mode) {
        /* In GNU C++ modes, dropping a throw specifier is not diagnosed if
           the earlier declaration came from a system header. */
        severity = pos_adjusted_severity(severity, prev_decl);
      }  /* if */
      pos_sy_diagnostic(severity,
                        is_redecl?
                          ec_omitted_exception_specification :
                          ec_omitted_exception_specification_on_specialization,
                        throw_pos, prev_decl);
    } else if (is_nothrow_spec(old_esp)) {
      /* Previous specification asserted that no exceptions will be thrown.
         It is compatible only with another nonthrowing specification on the
         current declaration. */
      if (!is_nothrow_spec(new_esp)) {
        pos_stsy_start_error(error_code, throw_pos, ":", prev_decl);
        add_diag_info(ec_previous_exception_specification_was_empty);
        end_error();
      }  /* if */
    } else {
      /* Both specifications list the types that will be thrown or the new one
         is noexcept and the previous one lists some types.  Describe the
         mismatch between the two specifications, if any. */
      any_difference_seen = FALSE;
      /* Check extraneous types: */
      any_difference_seen = compare_exception_specification_type_list(
                              new_esp, old_esp, throw_pos,
                              ec_omitted_in_previous_exception_specification,
                              error_code, prev_decl, any_difference_seen);
      /* Check missing types: */
      any_difference_seen = compare_exception_specification_type_list(
                              old_esp, new_esp, throw_pos,
                              ec_included_in_previous_exception_specification,
                              error_code, prev_decl, any_difference_seen);
      if (any_difference_seen) end_error();
    }  /* if */
  }  /* if */
done:
  db_exit();
}  /* check_exception_specification */


a_routine_ptr make_routine(a_type_ptr      type_ptr,
                           a_storage_class storage_class,
                           a_scope_depth   scope_depth)
/*
Allocate an entry for a routine with function type type_ptr and storage class
storage_class, and return a pointer to it.  The entry is allocated at the
file scope.  type_ptr must be in the file scope.  Unless scope_depth is
NO_SCOPE_DEPTH, add the new routine entry to the routines list of the
specified scope.
*/
{
  a_routine_ptr          rp;
  a_memory_region_number region_to_switch_back_to;

  /* Always allocate routines at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  rp = alloc_routine();
  rp->type = type_ptr;
  rp->storage_class = storage_class;
  if (scope_depth != NO_SCOPE_DEPTH) add_to_routines_list(rp, scope_depth);
  switch_back_to_original_region(region_to_switch_back_to);
  return rp;
}  /* make_routine */


static void decl_anonymous_union_variable(a_decl_parse_state  *dps)
/*
Create a variable to represent an anonymous union declared by the current
declaration (which is described by *dps).  Issue an error if its storage class
is invalid.  Also promote the fields of the union type to the current scope.
*/
{
  a_type_ptr       anon_union_type = dps->specifiers_type;
  a_variable_ptr   vp;
  a_boolean        at_file_or_namespace_scope;
  a_symbol_ptr     assoc_object_sym;
  a_scope_depth    scope_depth;
  a_storage_class  storage_class = dps->declared_storage_class;

  if (is_qualified_type(anon_union_type)) {
    /* GNU and Microsoft compilers accept cv-qualified anonymous unions, but
       recent GNU compilers no longer apply the qualifiers to the implied
       variable. */
    if (gpp_mode && gnu_version >= 40002) {
      anon_union_type = skip_typerefs(anon_union_type);
      pos_warning(ec_anonymous_union_qualifier_ignored, &dps->start_pos);
    } else {
      pos_warning(ec_nonstandard_anonymous_union_qualifier, &dps->start_pos);
    }  /* if */
  }  /* if */
  /* Check the storage class.  At file scope, only static is allowed. */
  at_file_or_namespace_scope =
                       (depth_scope_stack == depth_innermost_namespace_scope);
  if (at_file_or_namespace_scope) {
    switch (storage_class) {
      case sc_static:
        /* Okay. */
        break;
      case sc_extern:
      case sc_unspecified:
        /* Disallowed (ARM 9.5). */
        error(ec_anon_union_storage_class);
        storage_class = (a_storage_class)sc_static;
        break;
      default:
        /* Invalid for any variable at file scope. */
        error(ec_bad_file_scope_storage_class);
        storage_class = (a_storage_class)sc_static;
    }  /* switch */
  } else {
    /* Not at file scope or namespace scope. */
    switch (storage_class) {
      case sc_extern:
        /* Error, then default to automatic. */
        error(ec_anon_union_storage_class);
        /*FALLTHROUGH*/
      case sc_unspecified:
        /* Default to automatic. */
        storage_class = (a_storage_class)sc_auto;
        /*FALLTHROUGH*/
      case sc_static:
      case sc_auto:
      case sc_register:
        /* Okay. */
        break;
      default:
        unexpected_condition_str(
                           "decl_anonymous_union_variable: bad storage class");
    }  /* switch */
  }  /* if */
  /* Allocate a variable to represent the anonymous union. */
  scope_depth = at_file_or_namespace_scope ?
                     depth_innermost_namespace_scope : decl_scope_level; 
  vp = make_variable(anon_union_type, storage_class, scope_depth);
  vp->is_anonymous_parent_object = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_storage_class = dps->declared_storage_class;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  vp->source_corresp.decl_position = pos_curr_token;
  /* Promote the fields of the anonymous union to the current scope, and do
     some error checking on the anonymous union's members. */
  assoc_object_sym = make_anonymous_parent_object_symbol(
                                         (a_symbol_kind)sk_variable,
                                         &pos_curr_token,
                                         scope_stack[decl_scope_level].number);
  assoc_object_sym->variant.variable.ptr = vp;
  if (at_file_or_namespace_scope) {
    set_namespace_membership(assoc_object_sym, &vp->source_corresp,
                             (a_namespace_ptr)NULL);
  } else {
    vp->source_corresp.is_local_to_function = TRUE;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Mark the type declaration as autonomous. */
  skip_typerefs(anon_union_type)->autonomous_primary_tag_decl = TRUE;
  /* Also put out a source sequence entry for the variable (even though the
     variable declaration doesn't actually appear). */
  vp->declared_type = anon_union_type;
  add_to_source_sequence_list((char *)vp, (an_il_entry_kind)iek_variable);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (unrestricted_unions_enabled) {
    (void)def_initializer(assoc_object_sym, &dps->start_pos);
  }  /* if */
  /* Promote symbols for anonymous unions members to the enclosing scope.
     Error checking is also done. */
  check_anonymous_union_symbols(assoc_object_sym, /*is_nonstd=*/FALSE);
}  /* decl_anonymous_union_variable */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void reload_source_sequence_entry(a_decl_parse_state  *dps)
/*
*dps describes a declaration being processed, including the source sequence
entry dps->source_sequence_entry associated with that declaration.  Under
certain circumstances (having to do with memory region constraints), the
source sequence entry recorded in dps is discarded and a new one is allocated.
If this has happened, this routine sets dps->source_sequence_entry to the new
value.
*/
{
  if (dps->source_sequence_entry == NULL) {
    /* No source sequence entry is associated with the declarator. */
  } else if (ss_entry_kind(dps->source_sequence_entry) == iek_none) {
    /* record_symbol_declaration did not use the source sequence entry
       allocated for the declarator.  So update dps->source_sequence_entry
       with the actual entry used. */
    if (dps->source_sequence_entry->prev != NULL) {
      dps->source_sequence_entry = dps->source_sequence_entry->prev->next;
    } else {
      dps->source_sequence_entry =
                         scope_stack[depth_scope_stack].source_sequence_list;
    }  /* if */
  }  /* if */
}  /* reload_source_sequence_entry */

#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
#define reload_source_sequence_entry(dps)  /* Nothing */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                a_symbol_locator *locator,
                                a_scope_depth    scope_level,
                                a_boolean        suppress_redecl_error)
/*
Enter a symbol declarative scope specified by scope_level.  kind indicates
the kind of symbol (e.g., a variable), and *locator is a locator for the
identifier.  Enter the symbol at the file scope if is_file_scope is TRUE.
If suppress_redecl_error is TRUE, suppress any error on a duplicate
declaration of this symbol.
*/
{
  a_symbol_ptr  sym;

  db_enter(4, "enter_local_symbol");
  if (scope_stack[scope_level].kind == (a_scope_kind)sck_func_prototype) {
    if (kind == (a_symbol_kind)sk_variable) {
      /* A variable declared in a function prototype scope is the result of
         an error in an old-style param list. */
    } else if (C_dialect == C_dialect_cplusplus) {
      /* We can't get here in C++ in a legal program.  If there was some
         sort of error, just go ahead and enter the symbol in the current
         scope. */
    } else {
      /* Any other declaration expected in a prototype scope is that of a
         type or an enumeration constant. */
      check_assertion(kind == (a_symbol_kind)sk_class_or_struct_tag ||
                      kind == (a_symbol_kind)sk_union_tag ||
                      kind == (a_symbol_kind)sk_enum_tag ||
                      kind == (a_symbol_kind)sk_type ||
                      kind == (a_symbol_kind)sk_constant);
      /* In C-mode a type declared in a parameter declaration is local to
         the function.  Issue a warning on type declarations, since they will
         not be visible outside the function declaration.  For example:
             inf f(struct s a;);
             struct s {int b;};
         The first "struct s" is a different type than the second, which is
         probably not what was wanted. */
      if (kind != (a_symbol_kind)sk_constant &&
          !is_error_locator(*locator)) {
        pos_warning(ec_decl_in_prototype_scope, &locator->source_position);
      }  /* if */
    }  /* if */
  }  /* if */
  sym = enter_symbol(kind, locator, scope_level, suppress_redecl_error);
  db_exit();
  return(sym);
}  /* enter_local_symbol */


typedef struct an_id_linkage_block {
  a_symbol_locator
		*locator;
			/* Locator associated with the current variable,
			   routine, or function template declaration. */
  a_symbol_ptr	linked_symbol;
			/* A symbol apparently representing a prior
			   declaration of the entity currently being
			   declared. */
  a_symbol_ptr	homonym_symbol;
			/* If linked_symbol is NULL, another declaration
			   with the same name and belonging to the same
			   scope; otherwise, if linked_symbol is a newly
			   created symbol representing a function template
			   instance that is a guiding declaration,
			   homomyn_symbol may point to the symbol for the
			   template, with which the guiding-declaration symbol
			   will be overloaded; otherwise, homonym_symbol is
			   always NULL when linked_symbol in non-NULL.  (C++
			   only, and used only with function declarations.) */
  a_symbol_ptr  prior_decl_in_enclosing_scope;
			/* If the current declaration is a block-extern or
			   friend declaration, a prior declaration in an
			   enclosing scope (used to determine linkage);
			   otherwise NULL. */
  a_symbol_ptr	overload_symbol;
			/* A symbol representing the overload set to which
			   the current declaration already belongs or will
			   belong.  Specifically, if linked_symbol or
			   homonym_symbol is non-NULL and is an overload-set
			   member, it is the associated sk_overloaded_function
			   symbol.  If homonym_symbol is itself an overload
			   symbol, a pointer to the same symbol.  It is
			   always NULL when both linked_symbol and
			   homonym_symbol are NULL.  (C++ only, and used only
			   with function declarations.) */
  a_scope_depth
		effective_decl_level;
			/* The effective scope depth of the declaration.  It
			   is usually the same as decl_scope_level, except
			   in the case of friend declarations and certain
			   template declarations. */
  a_storage_class
		storage_class;
			/* On entry to id_linkage, the storage class
			   explicitly specified in the declaration.  May be
			   replaced in id_linkage by a setting corresponding
			   to the linkage. */
  a_func_info_block
		*func_info;
			/* Pointer to a func_info block for the declaration.
			   NULL for variable declarations. */
  a_type_ptr	type;
			/* The type with which the entity was declared. */
  a_byte_boolean
		is_friend_decl;
			/* TRUE when the declaration is a friend
			   declaration. */
  a_byte_boolean
		is_function_template;
			/* TRUE when the declaration is a function template
			   declaration. */
  a_byte_boolean
		is_new_template_instance;
			/* TRUE when the declaration matches a function
			   template instance that is newly created by the
			   declaration. */
  a_byte_boolean
		is_definition;
			/* TRUE when the declaration is a definition. */
  a_byte_boolean
		is_block_extern_decl;
			/* TRUE when the declaration is a block extern
			   declaration (a declaration within a function of
			   an entity with linkage outside the function). */
  a_byte_boolean
		is_local_class_friend_decl;
			/* TRUE when is_friend_decl is TRUE and the class in
			   which the declaration appears is a local class. */
  a_byte_boolean
		within_unnamed_namespace;
			/* TRUE when the declaration appears within an
			   unnamed namespace. */
  a_byte_boolean
		extern_C_name_linkage_specified;
			/* TRUE when the declaration appears within the
			   context of an extern "C" linkage specification. */
  a_byte_boolean
		direct_linkage_specifier;
			/* If TRUE, a linkage specification appeared directly
			   on the declaration (as opposed to the declaration
			   just being in a linkage specification block). */
  a_byte_boolean
		namespace_reactivated;
			/* TRUE if a namespace reactivation scope was
			   pushed during linkage processing; it must be
			   popped by the caller. */
  a_byte_boolean
		from_inline_namespace;
			/* TRUE if the symbol found is a projection symbol
			   for a symbol made visible by an inline namespace. */
  a_template_decl_info_ptr
		templ_info;
			/* When is_function_template is TRUE, a pointer to
			   the associated template declaration information.
			   (Particularly, information about the parameter
			   list.) */
  an_id_linkage_kind
		linkage;
			/* The linkage (none, internal, external) computed
			   for the current declaration. */
  a_name_linkage_kind
		name_linkage;
			/* The name linkage for the current declaration. */
  a_byte_boolean
		name_linkage_is_explicit;
			/* TRUE if the name linkage for the current
			   declaration was explicitly specified. */
} an_id_linkage_block;

#if NULL_POINTER_IS_ZERO

#define clear_id_linkage_block(idlbp)                                        \
  (memzero((char*)(idlbp), sizeof(an_id_linkage_block)))

#else /* !NULL_POINTER_IS_ZERO */

static void clear_id_linkage_block(an_id_linkage_block *idlbp)
/*
Clear the fields of the given id linkage block.
*/
{
  idlbp->locator = NULL;
  idlbp->homonym_symbol = NULL;
  idlbp->prior_decl_in_enclosing_scope = NULL;
  idlbp->linked_symbol = NULL;
  idlbp->overload_symbol = NULL;
  idlbp->effective_decl_level = NO_SCOPE_DEPTH;
  idlbp->storage_class = (a_storage_class)sc_unspecified;
  idlbp->func_info = NULL;
  idlbp->type = NULL;
  idlbp->is_friend_decl = FALSE;
  idlbp->is_function_template = FALSE;
  idlbp->is_new_template_instance = FALSE;
  idlbp->is_definition = FALSE;
  idlbp->is_block_extern_decl = FALSE;
  idlbp->is_local_class_friend_decl = FALSE;
  idlbp->within_unnamed_namespace = FALSE;
  idlbp->extern_C_name_linkage_specified = FALSE;
  idlbp->direct_linkage_specifier = FALSE;
  idlbp->namespace_reactivated = FALSE;
  idlbp->from_inline_namespace = FALSE;
  idlbp->templ_info = NULL;
  idlbp->linkage = idl_none;
  idlbp->name_linkage = (a_name_linkage_kind)nlk_none;
  idlbp->name_linkage_is_explicit = FALSE;
}  /* clear_id_linkage_block */

#endif /* NULL_POINTER_IS_ZERO */
#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean for_init_declaration_uses_standard_scope(
                                                  an_id_linkage_block  *idlbp,
                                                  a_scope_depth        depth)
/*
A declaration with the properties described in *idlbp appeared in a for-init
construct in a Microsoft mode that determines the scoping of for-init variable
based on its type or the type of a synonym in the surrounding scope.  depth is
the depth of the standard for-init scope within the current scope stack.
Return whether or not the for-init variable should use the standard for-init
scope.
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  type;
  a_scope_depth  saved_decl_scope_level = decl_scope_level;
  a_symbol_ptr   sym;

  check_assertion(microsoft_mode && microsoft_type_dependent_for_init_scope);
  /* Look for an existing declaration in the scope in which the for-statement
     appears. */
  decl_scope_level = depth-1;
  sym = curr_scope_id_lookup(idlbp->locator, IDL_NO_OPTIONS);
  decl_scope_level = saved_decl_scope_level;
  /* Check that no synonym declaration exists in that nonstandard scope
     (except for other nonstandard for-init variables). */
  if (sym != NULL && sym->decl_scope == scope_stack[depth-1].number) {
    /* There already is a declaration of the same name in the scope enclosing
       the for-statement. */
    if (sym->kind == (a_symbol_kind)sk_variable &&
        sym->variant.variable.declared_in_for_init) {
      /* The existing declaration is also a for-init variable: Hide it. */
      sym->is_invisible = TRUE;
    } else {
      /* The existing declaration is not a for-init variable: Use the standard
         scope for the current variable. */
      result = TRUE;
    }  /* if */
  }  /* if */
  if (!result) {
    /* If the current declaration has a class type with a destructor, use the
       standard scope. */
    type = skip_typerefs(idlbp->type);
    if (is_immediate_class_type(type)) {
      a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
      if (cssp != NULL && cssp->destructor != NULL) {
        /* The for-init variable has a class type with a destructor: Use the
           standard scope. */
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* for_init_declaration_uses_standard_scope */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void compute_effective_decl_level(an_id_linkage_block  *idlbp,
                                         a_scope_depth        depth)
/*
A name is being declared, and the "effective declaration level" identifies
the scope into which the newly declared symbol will be entered.  Usually
this is exactly the same as the current declaration scope, as indicated by
decl_scope_level.  This routine checks for some special cases and returns
the appropriate scope depth.  (The normal case is that the effective
declaration level is the same as decl_scope_level, which in turn is usually
the same as depth_scope_stack).
*/
{
  db_enter(4, "compute_effective_decl_level");
  if (C_mode()) {
    if (idlbp->is_block_extern_decl) {
      if (C_dialect == C_dialect_pcc &&
          (idlbp->storage_class == (a_storage_class)sc_extern ||
           idlbp->func_info != NULL)) {
        /* In pcc mode, functions and extern variables are always effectively
           declared at the file scope level. */
        depth = DEPTH_OF_FILE_SCOPE;
      } else if (idlbp->func_info != NULL &&
                 idlbp->storage_class == (a_storage_class)sc_static) {
        /* In C mode, as an extension, "static" is accepted on function
           declarations at function scope.  Such declarations are promoted
           to file scope. */
        depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
    }  /* if */
  } else if (idlbp->is_friend_decl) {
    /* Make the appropriate adjustment for a friend declaration.  The
       The "effective declaration level" of a friend function declaration is 
       usually the innermost non-class scope.  Starting from the specified
       scope depth, find the depth of the containing non-class scope. */
    a_scope_kind  kind = scope_stack[depth].kind;
    while (kind == (a_scope_kind)sck_class_struct_union ||
           kind == (a_scope_kind)sck_class_reactivation) {
      depth--;
      kind = scope_stack[depth].kind;
      if (kind == (a_scope_kind)sck_template_instantiation) {
        if (scope_stack[depth].in_prototype_instantiation ||
            scope_stack[depth].in_nonreal_instantiation) {
          /* During prototype instantiation, use the instantiation scope
             as the effective declaration scope. */
        } else {
          depth = depth_innermost_namespace_scope;
        }  /* if */
        break;
      } else if (kind == (a_scope_kind)sck_instantiation_context) {
        /* An instantiation context scope without an intervening instantiation
           scope.  This can occur when get_definition_of_class is being
           used. */
        depth = depth_innermost_namespace_scope;
        break;
      } else if (kind == (a_scope_kind)sck_template_declaration) {
        /* Must be an error of some sort. */
        depth = depth_innermost_namespace_scope;
        break;
      }  /* if */
    }  /* while */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_type_dependent_for_init_scope &&
             scope_stack[depth].is_for_init_block &&
             !for_init_declaration_uses_standard_scope(idlbp, depth)) {
    /* The scoping of for-init variables in MSVC++ 7.1 depends on the type of
       that variable and on the kind of declaration that might be hidden in the
       nonstandard scope.  The call to for_init_declaration_uses_standard_scope
       may cause a previous for-init declaration to become invisible. */
    --depth;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  idlbp->effective_decl_level = depth;
  db_exit();
}  /* compute_effective_decl_level */


static a_boolean namespace_extension_is_for_instantiation(
                                                a_scope_stack_entry_ptr  ssep)
/*
Return TRUE if the given namespace extension scope stack entry sits on top of
an instantiation context (possibly with intervening namespace extensions).
*/
{
  while (scope_is(ssep, sck_namespace_extension) &&
         !ssep->explicitly_declared_namespace_extension) {
    ssep -= 1;
  }  /* while */
  return scope_is(ssep, sck_instantiation_context);
}  /* namespace_extension_is_for_instantiation */


static a_scope_depth get_effective_depth_innermost_namespace(void)
/*
Return the "effective" innermost namespace scope depth.  Usually, this is just
depth_innermost_namespace_scope.  In GNU C++ modes, however, namespace
extensions that are implicitly pushed on the scope stack as part of a
class or namespace reactivation are ignored (but reactivations for
instantiations are treated as "explicit" in this context).  This is used to
emulate GCC's behavior wrt. certain block extern declarations.  For example:
      namespace N { struct S { void f(); }; }
      void N::S::f() {
        void g();  // ::g in g++ mode, N::g otherwise.
      }
*/
{
  a_scope_depth  depth = depth_innermost_namespace_scope;

  if (gpp_mode && depth != DEPTH_OF_FILE_SCOPE) {
    while (scope_stack[depth].kind == (a_scope_kind)sck_namespace_extension &&
           !scope_stack[depth].explicitly_declared_namespace_extension &&
           !namespace_extension_is_for_instantiation(&scope_stack[depth])) {
      depth = scope_stack[depth-1].depth_innermost_namespace_scope;
    }  /* while */
  }  /* if */
  return depth;
}  /* get_effective_depth_innermost_namespace */


static a_boolean is_local_class_friend_decl(an_id_linkage_block  *idlbp)
/*
Return TRUE if and only if the declaration being processed (with the given
id-linkage block) is a friend declaration in a local class.
*/
{
  a_boolean  result = FALSE;
  a_scope_depth decl_level = idlbp->effective_decl_level;

  if (idlbp->is_friend_decl &&
      decl_level != depth_innermost_namespace_scope &&
      (scope_stack[decl_level].kind == (a_scope_kind)sck_block ||
       scope_stack[decl_level].kind == (a_scope_kind)sck_function)) {
      result = TRUE;
  }  /* if */
  return result;
}  /* is_local_class_friend_decl */


static void set_linkage_environment(an_id_linkage_block  *idlbp,
                                    a_scope_depth        orig_decl_level)
/*
Set values in the indicated id-linkage block to reflect the environment of
the current declaration.  orig_decl_level is usually the current scope,
except in the case of function-template declarations (for which a template
declaration scope will have been pushed).
*/
{
  if (scope_stack[orig_decl_level].kind ==
                           (a_scope_kind)sck_class_struct_union) {
    /* This must be a friend declaration. */
    check_assertion(!C_mode());
    idlbp->is_friend_decl = TRUE;
  }  /* if */
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    /* A declaration inside a function. */
    if (idlbp->func_info != NULL ||
        idlbp->storage_class == (a_storage_class)sc_extern) {
      /* It's a function declaration or a variable declaration on which
         "extern" appeared explicitly. */
      idlbp->is_block_extern_decl = TRUE;
    }  /* if */
  }  /* if */
  /* The effective declaration level is usually the current scope -- but not
     always (e.g., friend declarations). */
  compute_effective_decl_level(idlbp, orig_decl_level);
  if (!C_mode()) {
    a_scope_depth  depth;

    /* Record whether this is a friend declaration inside a local class. */
    if (idlbp->is_friend_decl) {
      idlbp->is_local_class_friend_decl = is_local_class_friend_decl(idlbp);
    }  /* if */
    /* Record whether this declaration appears within the scope of an
       unnamed namespace. */
    if (idlbp->is_block_extern_decl ||
        idlbp->is_local_class_friend_decl) {
      depth = depth_innermost_namespace_scope;
    } else {
      depth = idlbp->effective_decl_level;
    }  /* if */
    if (scope_stack[depth].within_unnamed_namespace) {
      idlbp->within_unnamed_namespace = TRUE;
    }  /* if */
    /* Record whether this declaration appears within the context of an
       extern "C" linkage specification. */
    if (scope_stack[decl_scope_level].default_name_linkage ==
                                       (a_name_linkage_kind)nlk_external) {
      idlbp->extern_C_name_linkage_specified = TRUE;
    }  /* if */
  }  /* if */
}  /* set_linkage_environment */


static void compute_name_linkage(an_id_linkage_block  *idlbp)
/*
Based on the current state of the indicated id-linkage block, set the
fields describing the name-linkage for the current declaration.  This
routine can be called more than once for a given declaration (e.g., when
an entity initially declared with external linkage is redeclared to have
internal linkage).
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_symbol_ptr             prior_decl;
  a_source_correspondence  *scp;

  idlbp->name_linkage_is_explicit = FALSE;
  if (idlbp->linkage == idl_external ||
      /* In Sun mode a name linkage specifier also affects functions with
         internal name linkage: */
      (sun_mode && idlbp->linkage == idl_internal &&
       ssep->name_linkage_is_explicit)) {
    if (C_mode()) {
      /* External entity in C mode. */ 
      idlbp->name_linkage = (a_name_linkage_kind)nlk_external;
    } else if (idlbp->func_info != NULL &&
               idlbp->func_info->is_main_function) {
      /* In C++ "main" gets C++ linkage. */
      idlbp->name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    } else if (idlbp->is_function_template) {
      /* Function templates with external linkage get C++ linkage. */
      idlbp->name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    } else if (ssep->name_linkage_is_explicit &&
               !((microsoft_mode || gpp_mode) && idlbp->is_block_extern_decl &&
                 idlbp->prior_decl_in_enclosing_scope != NULL &&
                 !idlbp->direct_linkage_specifier)) {
      /* The name linkage was explicit specified (although not necessarily
         directly on this declaration).  Use that name-linkage even when there
         is a prior declaration with different linkage.  In Microsoft and GNU
         modes, a block-extern declaration in a function declared with an
         explicit name-linkage specification still takes its name linkage from
         a prior declaration. */
      idlbp->name_linkage = ssep->default_name_linkage;
      idlbp->name_linkage_is_explicit = TRUE;
    } else {
      /* No explicit linkage specification.  Use the prior declaration (if
         there is one), or else the default. */
      prior_decl = idlbp->linked_symbol;
      if (prior_decl == NULL) {
        /* A block extern declaration for which there is a visible prior
           declaration. */
        prior_decl = idlbp->prior_decl_in_enclosing_scope;
      }  /* if */
      if (prior_decl != NULL) {
        reduce_projection_symbol_to_fundamental_symbol(prior_decl);
        if (is_type_symbol(prior_decl) ||
            prior_decl->kind == (a_symbol_kind)sk_field ||
            prior_decl->kind == (a_symbol_kind)sk_constant ||
            prior_decl->kind == (a_symbol_kind)sk_parameter ||
            prior_decl->kind == (a_symbol_kind)sk_undefined) {
          /* The prior declaration may have been a typedef in an enclosing
             scope; there is no relationship wrt. name linkage.  In error
             situations, we may also pick up fields, constants, parameters
             (when lambdas appear in function prototypes), or undefined
             identifiers: ignore them. */
          prior_decl = NULL;
        }  /* if */
      }  /* if */
      if (prior_decl != NULL &&
          (idlbp->func_info == NULL) ==
                     (prior_decl->kind == (a_symbol_kind)sk_variable)) {
       /* Use the linkage specifier from the prior declaration. */
        scp = source_corresp_entry_for_symbol(prior_decl);
        check_assertion(scp != NULL);
        idlbp->name_linkage = scp->name_linkage;
      } else {
        /* Use the default. */
        idlbp->name_linkage = ssep->default_name_linkage;
      }  /* if */
    }  /* if */
    if (idlbp->type->kind == (a_type_kind)tk_routine &&
        is_name_linkage_kind_for_rout_type(idlbp->name_linkage)) {
      /* In case there's a change, reset the routine-name-linkage (i.e.,
         calling convention) in the routine type.  Note this is not done
         when the type is based on a typedef.  Also, certain custom
         name linkage kinds may not apply to routine types. */
      idlbp->type->variant.routine.extra_info->
                            routine_name_linkage = idlbp->name_linkage;
    }  /* if */
  } else if (idlbp->linkage == idl_internal) {
    /* Internal linkage is easy -- ignore the default linkage specifier and
       the linkage of prior declarations. */
    idlbp->name_linkage = (a_name_linkage_kind)nlk_internal;
  }   /* if */
}  /* compute_name_linkage */


static void find_linked_symbol(an_id_linkage_block  *idlbp)
/*
Find and return a symbol idlbp->linked_symbol representing the potential prior
declaration of the variable, routine, or function template declaration
described by *idlbp.  Two distinct cases are handled here:
  -- redeclaration in the same scope:
        void f();
        void f() { }        // the other "f" is returned as linked symbol
        static int i;
        extern int i;       // the other "i" is returned as linked symbol
  -- friend declaration (C++ only):
        void g();
        class A {
          friend g();       // the other "g" is returned as linked symbol
        };

It also returns other sorts of symbols:
(1) idlbp->prior_decl_in_enclosing_scope is a visible linked declaration in a
prior scope.  For example, with a block extern declaration:
        int j;
        void f() {
          extern int j;     // No linked symbol but the other "j" is returned
        }                   // via idlbp->prior_decl_in_enclosing_scope.
(Note, however, that this is only the case for "visible" prior declarations.
E.g.,
        static int k;
        void f() {
          int k;            // ::k is no longer visible
          { extern int k; } // idlbp->prior_decl_in_enclosing_scope is NULL.
        }
This behavior affects how id_linkage determines the linkage of block-extern
declared k; the latter gets external linkage, which results in a linkage
conflict that is reported in strict mode.)

(2) In C++ only, for function and function template declarations,
idlbp->overload_symbol is returned when a name match was found with another
function or function template declaration, whether or not a type match was
also found. For instance,
  -- redeclaration in the same scope:
       void f(int);
       void f(int,int);  // idlbp->prior_decl_in_enclosing_scope and
                         // idlbp->linked_symbol are both NULL, but "f(int)"
                         // is returned via idlbp->overload_symbol.
  -- friend declaration:
       void f(int);
       class A {
         friend f(int,int);  // ditto.
       };
  -- block extern
       void f(int);
       void g() {
         extern f(int,int);  // ditto.
       }

Besides the type, required for function matching in C++, and the locator, the
input parameters include idlbp->effective_decl_level, the scope at which the
entity is to be entered into the symbol table, and idlbp->is_friend_decl, TRUE
when the declaration is a friend declaration within a class.
*/
{
  a_boolean     decls_at_same_scope;
  a_boolean     is_list;
  a_symbol_ptr  other_decl, other_decl_saved;
  a_symbol_kind kind = (a_symbol_kind)sk_last;
  a_boolean     function_template_seen = FALSE;
  a_boolean     is_function = is_function_type(idlbp->type);
  a_boolean     is_namespace_member_def = FALSE;
  a_symbol_locator  *locator = idlbp->locator;
  a_boolean     is_guiding_decl = FALSE;

  db_enter(3, "find_linked_symbol");
  if (locator->specific_symbol != NULL &&
      (qualifier_namespace_ptr(*locator) != NULL ||
       locator->is_file_scope_qualified_name ||
       locator->is_template_id)) {
    /* The declarator is a namespace-qualified or a global-scope-qualified
       name.  If it's not a friend declaration, it is supposed to be a
       definition. */
    if (!idlbp->is_friend_decl) is_namespace_member_def = TRUE;
    other_decl = locator->specific_symbol;
    /* If the name, looked up as a member of the specified scope, turns out
       to be a namespace projection, it must have been pulled into the
       scope via a using declaration or a using directive.  Unless it is
       a symbol for an inline namespace member it is not a valid
       declarator and should be ignored. */
    if (!other_decl->synthesized_namespace_projection &&
        symbol_is(other_decl, sk_namespace_projection)) {
      other_decl = NULL;
    }  /* if */
  } else {
    if (idlbp->is_block_extern_decl || idlbp->is_local_class_friend_decl) {
      /* This is either a block-extern declaration of a function or variable
         or (what amounts to the same thing) a friend declaration within a
         local class.  Find the visible declaration of the same name. */
      (void)normal_id_lookup(locator, IDL_LINKAGE_LOOKUP);
      other_decl = locator->specific_symbol;
    } else {
      /* Not a context in which lookup in enclosing scopes is meaningful.
         Just check for a prior declaration in the current scope. */
      check_assertion(idlbp->effective_decl_level ==
                                            depth_innermost_namespace_scope ||
                      idlbp->is_friend_decl);
      if (depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE) {
        /* Do the lookup in the file scope. */
        (void)file_scope_id_lookup(il_header.primary_scope,
                                   locator, IDL_LINKAGE_LOOKUP);
        other_decl = locator->specific_symbol;
      } else {
        /* Do the lookup in the innermost namespace scope. */
        a_namespace_ptr  nsp;
        nsp = scope_stack[depth_innermost_namespace_scope].il_scope->
                                                       variant.assoc_namespace;
        (void)namespace_qualified_id_lookup(locator, nsp, IDL_LINKAGE_LOOKUP);
        other_decl = locator->specific_symbol;
      }  /* if */
    }  /* if */
    /* Clear out the specific symbol pointer of the locator.  It was set by
       the lookup routine, but it may not be valid. */
    clear_specific_symbol(*locator);
  }  /* if */
  /* We are only interested in variable and function declarations.  If
     something else was found, we're not interested. */
  if (other_decl != NULL) {
    a_symbol_ptr  fund_other_decl = fundamental_symbol_of(other_decl);
    a_boolean     from_inline_namespace;
    from_inline_namespace = !other_decl->ambiguous &&
                            is_symbol_from_inline_namespace(fund_other_decl);
    kind = other_decl->kind;
    decls_at_same_scope = (other_decl->decl_scope ==
                            scope_stack[idlbp->effective_decl_level].number);
    if ((microsoft_mode || sun_mode || gpp_mode) &&
        kind == (a_symbol_kind)sk_namespace_projection) {
      /* In Microsoft and Sun modes, a member of another namespace can be
         redeclared with an unqualified declarator if that member was made
         visible via a using-declaration.  E.g.,
           namespace N { void f(); }  using N::f; void f() {} // Fine: N::f
         We emulate this only if the entity has C name linkage or if it is
         a variable.  In GNU C++ mode, we also emulate this for variables. */
      if ((!gpp_mode &&
           source_corresp_entry_for_symbol(fund_other_decl)->name_linkage ==
                                          (a_name_linkage_kind)nlk_external) ||
          symbol_is(fund_other_decl, sk_variable)) {
        kind = fund_other_decl->kind;
      }  /* if */
    }  /* if */
    if (kind == (a_symbol_kind)sk_variable ||
        kind == (a_symbol_kind)sk_routine ||
        kind == (a_symbol_kind)sk_function_template ||
        kind == (a_symbol_kind)sk_overloaded_function) {
      /* Okay to use other_decl. */
    } else if (from_inline_namespace) {
      /* An inline namespace member can be defined or redeclared using
         a projection symbol. */
    } else {
      if (!C_mode() && kind == (a_symbol_kind)sk_namespace_projection) {
        if (symbol_is(fund_other_decl, sk_routine) ||
            symbol_is(fund_other_decl, sk_function_template)) {
          /* In a case like:
               namespace N { void f(int); }
               using N::f;
               void f();
             we'll need to form an overload set with N::f and ::f. */
          if (decls_at_same_scope) {
            idlbp->homonym_symbol = other_decl;
          }  /* if */
        } else if (symbol_is(fund_other_decl, sk_variable) &&
                   depth_innermost_function_scope == NO_SCOPE_DEPTH) {
          /* This is a variable declaration at file/namespace scope.  We need
             to deal with a case like this:
                 namespace N { extern "C" int i; }
                 using N::i;
                 extern "C" int i;
             [namespace.udecl] says a declaration can coexist with a
             using-declaration as long as they refer to the same entity.
             We know the other declaration was in fact a using-declaration,
             so check whether both declarations refer to extern "C"
             variables of the same type. */
          a_variable_ptr  vp = fund_other_decl->variant.variable.ptr;
          if (vp->source_corresp.name_linkage ==
                                    (a_name_linkage_kind)nlk_external &&
              scope_stack[depth_scope_stack].default_name_linkage ==
                                    (a_name_linkage_kind)nlk_external &&
              identical_types(vp->type, idlbp->type)) {
            /* Remove other_decl from the symbol table.  It will be replaced
               by the current variable declaration. */
            remove_symbol(other_decl);
          }  /* if */
        }  /* if */
      }  /* if */
      if ((idlbp->is_block_extern_decl || idlbp->is_local_class_friend_decl) &&
          other_decl->decl_scope != scope_stack[depth_scope_stack].number) {
        /* This is a friend or block-extern declaration and a declaration
           that cannot possibly match it was found in an enclosing scope.
           Remember it for a subsequent error message. */
        idlbp->prior_decl_in_enclosing_scope = other_decl;
      }  /* if */
      other_decl = NULL;
    }  /* if */
  }  /* if */
  if (other_decl != NULL) {
    /* A match was found in searching the symbol table.  However, in
       C++ we have to allow for function overloading.  If what we found
       was an sk_overloaded_function symbol, we need to look for a type
       match amongst the instances of the name.  Even if it was an
       sk_routine symbol, we may want to overload the two functions. */
    if (!C_mode() && is_function && kind != (a_symbol_kind)sk_variable) {
      /* C++ function or function template -- type compatibility check is
         required. */
      a_template_param_ptr  params = NULL;
      unsigned short        n_params = 0;
      if (idlbp->is_function_template) {
        params = idlbp->templ_info->parameters;
        n_params = idlbp->templ_info->n_params;
      }  /* if */
      if (decls_at_same_scope) {
        /* *overload_symbol is set for cases in which the current symbol
           may be added to an overload list.  Note that overloading across
           scopes is not allowed.  Also, *overload_symbol may end up being
           cleared later. */
        idlbp->homonym_symbol = other_decl;
      }  /* if */
      if (symbol_is(other_decl, sk_overloaded_function)) {
        if (decls_at_same_scope) idlbp->overload_symbol = other_decl;
        other_decl = other_decl->variant.overloaded_function.symbols;
        is_list = TRUE;
      } else {
        is_list = FALSE;
      }  /* if */
      other_decl_saved = other_decl;
      /* Go through the list of functions and look for type compatibility.
         If types_are_compatible returns TRUE, this is a redeclaration.
         If no type match is found, this is a candidate for overloading. */
      /*lint --e{446} other_decl modified in loop (LINTBUG) */
      for (; other_decl != NULL;
             other_decl = is_list ? other_decl->next : NULL) {
        a_routine_ptr  rp;
        a_symbol_ptr   fund_other_decl = fundamental_symbol_of(other_decl);
        a_boolean      function_from_inline_namespace = 
                              !other_decl->ambiguous &&
                              is_symbol_from_inline_namespace(fund_other_decl);
        if (symbol_is(other_decl, sk_namespace_projection) &&
            !function_from_inline_namespace &&
            !locator->is_template_id &&
            (qualifier_namespace_ptr(*locator) !=
                                     fund_other_decl->parent.namespace_ptr) &&
            !((sun_mode || microsoft_mode) &&
              (source_corresp_entry_for_symbol(fund_other_decl)->name_linkage
                                      == (a_name_linkage_kind)nlk_external ||
               symbol_is(fund_other_decl, sk_variable)))) {
          /* Ignore namespace projection symbols that may have gotten into
             this overload set by a using declaration -- e.g.,
               namespace N { void f(int); }
               using N::f;
               void f();
               int f(int);           // Does *not* match N::f(int)
             (except in Sun and Microsoft modes.)  Symbols from inline
             namespaces are made visible by synthesized namespace projection
             symbols.  Those are allowed. */
        } else if (symbol_is(fund_other_decl, sk_function_template)) {
          if (idlbp->is_function_template) {
            a_template_symbol_supplement_ptr  tssp;
            a_template_decl_info_ptr          tdip;
            tssp = fund_other_decl->variant.template_info;
            tdip = tssp->variant.function.decl_cache.decl_info;
            rp = tssp->variant.function.routine;
            if (n_params == tdip->n_params &&
                equiv_template_param_lists(tdip->parameters, params,
                                           /*issue_errors=*/FALSE,
	                                   ETP_NO_OPTIONS,
                                           (a_source_position*)NULL,
                                           es_error) &&
                routine_types_are_redecl_compatible(rp->type, idlbp->type,
                                                    TCF_NO_FLAGS)) {
              /* The other_decl template function matches the current
                 declaration. */
              idlbp->linked_symbol = fund_other_decl;
              idlbp->homonym_symbol = NULL;
              goto done;
            }  /* if */
          } else {
            /* There may be a match involving an instance of this function
               template, but we delay searching its list of instantiations
               until all normally declared functions have been seen. */
            function_template_seen = TRUE;
          }  /* if */
        } else {
          /* The listed declaration is not a template or a using-decl. */
          if (idlbp->is_function_template || locator->is_template_id) {
            /* If we are matching a template or a template instance, we cannot
               establish the match based on routine types.  That case will be
               covered by going back to the original template below. */
          } else {
            /* Compare the routine type of the current declaration with that
               of the previous declaration. */
            check_assertion(is_simple_function_symbol(fund_other_decl));
            rp = fund_other_decl->variant.routine.ptr;
            if (routine_types_are_redecl_compatible(rp->type, idlbp->type,
                                                    TCF_NO_FLAGS)) {
              /* Other_decl matches the current declaration.  Null out
                 *overload_symbol in case it was set. */
              other_decl = fund_other_decl;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      if (other_decl == NULL && function_template_seen &&
          (guiding_decls_allowed ||
           locator->is_template_id ||
           locator->is_qualified_name)) {
        /* We didn't find a match, but there was at least one function
           template.  See if it either provides a match with an existing
           instance of the template or if a new instance can be created
           based on the current type.  This is done if guiding declarations
           are recognized or if the declaration is known to refer to a
           previously declared template.  The latter is the case when the
           name was specified as a qualified name or includes an explicit
           template argument list. */
        a_symbol_ptr                   sym, match = NULL;
        a_partial_order_candidate_ptr  candidates_list = NULL;
        a_symbol_ptr                   fund_other_decl;

        for (other_decl = other_decl_saved;
             other_decl != NULL;
             other_decl = is_list ? other_decl->next : NULL) {
          if (locator->is_template_id || locator->is_qualified_name) {
            fund_other_decl = fundamental_symbol_of(other_decl);
          } else {
            /* Just a possible guiding-declaration, so ignore projections of
               function templates -- they are pulled into the current scope
               by using-declarations, and so declarations in the current
               scope can't serve as guiding declarations for them. */
            fund_other_decl = other_decl;
          }  /* if */
          if (symbol_is(fund_other_decl, sk_function_template)) {
            /* Look for a match on the list of instantiations. */
            if (has_matching_template_function(fund_other_decl, idlbp->type,
                                               locator->template_arg_list,
                                              /*is_decl_context=*/TRUE)) {
              /* This template can generate an instance of the appropriate
                 type.  Add the matching template to a list of matching
                 candidates. */
              add_to_partial_order_candidates_list(&candidates_list,
                                                   fund_other_decl,
                                                   (a_template_arg_ptr)NULL);
            }  /* if */
          }  /* if */
        }  /* for */
        if (candidates_list != NULL) {
          /* If any of the templates matched, select the best one using
             the partial ordering rules.  If a best match cannot be selected,
             an arbitrary member of the unordered set of templates will be
             returned and the ambiguous flag will be set. */
          a_boolean		ambiguous;
          a_template_arg_ptr	templ_arg_list;
          a_symbol_ptr		best_sym;
          a_boolean             is_new_template_instance;
          select_best_partial_order_candidate(
                           candidates_list, (a_symbol_ptr)NULL, &best_sym,
                           &templ_arg_list, &ambiguous);
          /* Generate a partial instantiation of the matching instance. */
          sym = matching_template_function(best_sym, idlbp->type,
                                           locator->template_arg_list,
					   (a_boolean)locator->is_template_id,
                                           /*is_decl_context=*/TRUE,
					   /*in_class_specialization=*/FALSE,
                                           &is_new_template_instance);
          other_decl = best_sym;
          match = sym;
          idlbp->is_new_template_instance = is_new_template_instance;
          if (ambiguous) {
            /* This declaration cannot be a guiding declaration for more
               than one template function.  Issue an ambiguity error. */
            pos_syty_error(ec_ambiguous_guiding_decl,
                           &locator->source_position, sym, idlbp->type);
          }  /* if */
        }  /* if */
        if (match != NULL) {
          idlbp->linked_symbol = other_decl = match;
          is_guiding_decl = guiding_decls_allowed;
        }  /* if */
      }  /* if */
    }  /* if */
    if (decls_at_same_scope || idlbp->is_friend_decl) {
      /* The symbol was located in the current scope. If there there was an
         exact type match of C++ functions, and in general otherwise, this
         is a redeclaration, and if other_decl has linkage we can return in
         *linked_symbol a pointer to the function or variable it represents. */
      if (other_decl != NULL) {
        if (idlbp->effective_decl_level == depth_innermost_namespace_scope) {
          /* Redeclaration at file or namespace scope. */
          idlbp->linked_symbol = other_decl;
        } else if (is_function_symbol(other_decl)) {
          /* Functions always have linkage. */
          idlbp->linked_symbol = other_decl;
        } else if (!other_decl->variant.variable.ptr
                              ->source_corresp.is_local_to_function) {
          /* Variables at file scope always have linkage.  Automatic,
             register, and static variables in local scopes do not. */
          idlbp->linked_symbol = other_decl;
        }  /* if */
      }  /* if */
    } else if (is_namespace_member_def) {
      idlbp->linked_symbol = other_decl;
    }  /* if */
    if (idlbp->linked_symbol != NULL) {
      /* Find out whether the linked symbol was made visible because it
         is a member of an inline namespace. */
      a_symbol_ptr  fund_sym = fundamental_symbol_of(idlbp->linked_symbol);
      idlbp->from_inline_namespace = is_symbol_from_inline_namespace(fund_sym);
    }  /* if */
    if (other_decl != NULL &&
        (idlbp->is_block_extern_decl || idlbp->is_local_class_friend_decl) &&
        other_decl->decl_scope != scope_stack[depth_scope_stack].number) {
      idlbp->prior_decl_in_enclosing_scope = other_decl;
    }  /* if */
    if (idlbp->linked_symbol != NULL) {
      if (idlbp->homonym_symbol == NULL) {
        /* Okay. */
      } else if (is_guiding_decl && idlbp->overload_symbol == NULL) {
        /* Special case -- return both linked_symbol and homonym_symbol, to
           form an overload set down the line. */
        check_assertion(idlbp->homonym_symbol != NULL &&
                        symbol_is(idlbp->homonym_symbol,
                                  sk_function_template));
      } else {
        idlbp->homonym_symbol = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
done:;
#if DEBUG
  if (debug_level >= 3) {
    if (idlbp->linked_symbol != NULL) {
      db_symbol(idlbp->linked_symbol, "linked symbol: ", 2);
    }  /* if */
    if (idlbp->homonym_symbol != NULL) {
      db_symbol(idlbp->homonym_symbol, "homonym symbol: ", 2);
    }  /* if */
    if (idlbp->overload_symbol != NULL) {
      db_symbol(idlbp->overload_symbol, "overload symbol: ", 2);
    }  /* if */
    if (idlbp->prior_decl_in_enclosing_scope != NULL) {
      db_symbol(idlbp->prior_decl_in_enclosing_scope,
                "prior decl in enclosing scope: ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* find_linked_symbol */


static a_boolean has_linkage_within_innermost_namespace_scope(a_symbol *sym)
/*
Given the symbol *sym representing a declaration found by find_linked_symbol,
this function returns TRUE if that prior symbol's linkage should be applied
to a newly processed symbol of the same name and type.  Called from id_linkage
only.
*/
{
  a_boolean result = FALSE;

  if (sym->decl_scope ==
                    scope_stack[depth_innermost_namespace_scope].number) {
    /* The symbol was found in namespace scope, so it should have linkage. */
    result = TRUE;
  } else if (sym->decl_scope >
                    scope_stack[depth_innermost_namespace_scope].number &&
             !(sym->kind == (a_symbol_kind)sk_variable &&
               sym->variant.variable.ptr
                                    ->source_corresp.is_local_to_function)) {
    /* The symbol was found inside the innermost namespace scope, and it is
       not a local static variable.  So it should have linkage. */
    result = TRUE;
  }  /* if */
  return result;
}  /* has_linkage_within_innermost_namespace_scope */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- dps is unused in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void id_linkage(an_id_linkage_block  *idlbp,
                       a_decl_parse_state   *dps)
/*
Determine the linkage (internal, external, or none) of the current variable,
routine, or function template declaration (described by *dps).  Find previous
declarations with the same name that may affect the linkage.  Return the
information in the specified id-linkage block.
*/
{
  a_boolean        is_object, is_function;
  a_symbol_ptr     prior_decl;
  a_storage_class  local_storage_class = idlbp->storage_class;
  a_boolean        is_template_instance = FALSE;
  a_boolean        const_variable = FALSE;

  db_enter(3, "id_linkage");
  check_assertion(local_storage_class != (a_storage_class)sc_typedef);
  /* Except in Microsoft mode, member functions cannot be redeclared (without
     being defined) outside their parent class: */
  check_assertion(idlbp->locator->specific_symbol == NULL ||
                  !idlbp->locator->specific_symbol->is_class_member ||
                  microsoft_mode);
  is_function = idlbp->func_info != NULL;
  is_object = !is_function;
  if (is_error_locator(*idlbp->locator)) {
    /* Symbol is compiler-generated as a result of an error, so there are
       no other declarations of the same symbol. */
  } else if (is_object && depth_innermost_function_scope != NO_SCOPE_DEPTH &&
             local_storage_class != (a_storage_class)sc_extern) {
    /* Local variable declaration. */
  } else if (scope_stack[decl_scope_level].kind ==
                                     (a_scope_kind)sck_func_prototype) {
    /* Function parameters have no linkage. */
  } else {
    /* Set default linkage, based on the environment of the current
       declaration but without taking into account the prior declaration. */
    if (idlbp->effective_decl_level != depth_innermost_namespace_scope &&
        !idlbp->is_block_extern_decl &&
        !idlbp->is_local_class_friend_decl) {
      /* A declaration of a local variable. */
      idlbp->linkage = idl_none;
    } else if (idlbp->storage_class == (a_storage_class)sc_static) {
      /* A declaration a non-local entity with "static" storage class. */
      idlbp->linkage = idl_internal;
    } else if (!C_mode() && is_function && !extern_inline_allowed &&
               idlbp->func_info->is_inline) {
      /* A C++ inline function.  Note that in C99 mode an inline function
         has internal linkage only if declared "static". */
      idlbp->linkage = idl_internal;
    } else if (!C_mode() && is_object &&
               is_const_qualified_type(idlbp->type) &&
               decl_scope_level == depth_innermost_namespace_scope &&
               idlbp->storage_class == (a_storage_class)sc_unspecified &&
#if MICROSOFT_EXTENSIONS_ALLOWED
               !(microsoft_mode && microsoft_version >= 1400 &&
                 find_attribute(ak_dllexport,
                                dps->prefix_attributes) != NULL) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
               !(idlbp->extern_C_name_linkage_specified &&
                 idlbp->direct_linkage_specifier)) {
      /* In C++ all const qualified objects at file or namespace scope with
         no explicit storage class are internally linked (unless previously
         declared to be extern -- see below).  An exception are variables
         declared with the __declspec(dllexport) attribute in some Microsoft
         modes: They are treated as having external linkage. */
      idlbp->linkage = idl_internal;
      const_variable = TRUE;
    } else {
      idlbp->linkage = idl_external;
    }  /* if */
    /* Find a previously declared entity with the same name and to which
       this declaration is linked. */
    find_linked_symbol(idlbp);
    prior_decl = idlbp->linked_symbol;
    if ((sun_mode || microsoft_mode) && prior_decl != NULL) {
      /* In Sun and Microsoft modes, linked symbol could validly be a
         namespace projection. */
      prior_decl = fundamental_symbol_of(prior_decl);
    }  /* if */
    if (prior_decl == NULL) {
      /* Special case for block extern declarations. */
      prior_decl = idlbp->prior_decl_in_enclosing_scope;
      if (prior_decl != NULL &&
          prior_decl->kind != (a_symbol_kind)sk_variable &&
          prior_decl->kind != (a_symbol_kind)sk_routine &&
          prior_decl->kind != (a_symbol_kind)sk_function_template &&
          prior_decl->kind != (a_symbol_kind)sk_overloaded_function) {
        /* This is a friend or block-extern declaration and a declaration
           that cannot possibly match it was found in an enclosing scope.
           It should not be considered a candidate for linked-symbol. */
        prior_decl = NULL;
      }  /* if */
    }  /* if */
#if ASM_FUNCTION_ALLOWED
    if (local_storage_class == (a_storage_class)sc_asm) {
      /* An asm function has internal linkage. */
      idlbp->linkage = idl_internal;
      prior_decl = NULL;
    }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
    if (prior_decl != NULL) {
      if (is_object == (prior_decl->kind == (a_symbol_kind)sk_variable)) {
        /* The current declaration matches the prior declaration, so the latter
           can be used to determine the linkage of the former. */
      } else {
        /* Ignore the previous declaration. */
        prior_decl = NULL;
      }  /* if */
    }  /* if */
    if (prior_decl != NULL) {
      if ((prior_decl->kind == (a_symbol_kind)sk_routine ||
           prior_decl->kind == (a_symbol_kind)sk_member_function) &&
          prior_decl->variant.routine.instance_ptr != NULL) {
        is_template_instance = TRUE;
      }  /* if */
      if (is_template_instance) {
        if (!idlbp->is_definition &&
            !routine_has_been_defined(prior_decl->variant.routine.ptr)) {
          /* A specific declaration of a function template instance for which
             a specific definition has not been seen.  The storage class of
             this declaration must agree with the storage class of the
             template. */
          a_storage_class  templ_storage_class;
          a_boolean        templ_is_inline;

          templ_storage_class = prior_decl->variant.routine.ptr->storage_class;
          templ_is_inline = prior_decl->variant.routine.ptr->is_inline;
          if (((templ_storage_class != (a_storage_class)sc_static) &&
               (local_storage_class == (a_storage_class)sc_static))) {
            /* The template was not static but the new declaration is.  Issue
               a warning, since the explicitly specified storage class will
               be ignored. */
            pos_sy_warning(ec_template_and_instance_linkage_conflict,
                           &idlbp->locator->source_position, prior_decl);
          } else if (idlbp->func_info->is_inline && !templ_is_inline) {
            /* The specific declaration is inline but the template is not.
               Issue a diagnostic because the inline specifier here will be
               disregarded. */
            pos_sy_warning(ec_incompatible_inline_specifier_on_specific_decl,
                           &idlbp->locator->source_position, prior_decl);
          }  /* if */
          if (!microsoft_mode) {
            idlbp->func_info->is_inline = templ_is_inline;
            local_storage_class = templ_storage_class;
          }  /* if */
        }  /* if */
        idlbp->storage_class = local_storage_class;
        if (local_storage_class == (a_storage_class)sc_static) {
          idlbp->linkage = idl_internal;
        } else if (!extern_inline_allowed && idlbp->func_info->is_inline) {
          idlbp->linkage = idl_internal;
        } else {
          idlbp->linkage = idl_external;
        }  /* if */
      } else if (const_variable &&
                 prior_decl->variant.variable.ptr->storage_class !=
                                              (a_storage_class)sc_static) {
        /* Prior declaration of this variable had external linkage, so that
           is retained. */
        idlbp->linkage = idl_external;
      } else {
        if (local_storage_class == (a_storage_class)sc_extern ||
            (is_function &&
             local_storage_class == (a_storage_class)sc_unspecified)) {
          /* An object or function with extern storage class, or a function
             with no storage class, has the same linkage as any visible
             declaration of this identifier within the enclosing namespace
             scope. */
          if (has_linkage_within_innermost_namespace_scope(prior_decl)) {
            /* There is a prior declaration in or inside the innermost
               namespace scope that is visible from here. */
            switch (prior_decl->kind) {
              case sk_routine:
                local_storage_class = prior_decl->variant.routine.ptr->
                                                               storage_class;
                break;
              case sk_variable:
                local_storage_class = prior_decl->variant.variable.ptr->
                                                                storage_class;
                break;
              case sk_function_template:
                local_storage_class = prior_decl->variant.template_info->
                                      variant.function.routine->storage_class;
                break;
              case sk_overloaded_function:
              default:
                unexpected_condition_str(
                                        "id_linkage: bad kind for prior_decl");
            }  /* switch */
            if (local_storage_class == (a_storage_class)sc_static) {
              /* An entity initially declared "static" is now being declared
                 without "static".  But it still has internal linkage. */
              idlbp->linkage = idl_internal;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "Linkage for %s is ",
                     is_error_locator(*idlbp->locator) ?
                        "<error>" : idlbp->locator->symbol_header->identifier);
    switch (idlbp->linkage) {
      case idl_none:     fputs("none",     f_debug); break;
      case idl_internal: fputs("internal", f_debug); break;
      case idl_external: fputs("external", f_debug); break;
      default:           unexpected_condition_str(
                                   "id_linkage: bad id linkage determination");
    }  /* switch */
    putc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  if (idlbp->linkage == idl_none) {
    /* If there's no linkage, be sure the linked_symbol is NULL. */
    idlbp->linked_symbol = NULL;
#if ASM_FUNCTION_ALLOWED
  } else if (idlbp->storage_class == (a_storage_class)sc_asm) {
    idlbp->name_linkage = (a_name_linkage_kind)nlk_internal;
#endif /* ASM_FUNCTION_ALLOWED */
  } else {
    /* Set the storage class to fit the linkage. */
    if (idlbp->linkage == idl_internal) {
      idlbp->storage_class = (a_storage_class)sc_static;
    } else if (idlbp->linkage == idl_external) {
      if (idlbp->is_definition) {
        idlbp->storage_class = (a_storage_class)sc_unspecified;
      } else {
        idlbp->storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
    /* Based on the linkage that has been determined, figure out what the
       "name linkage" should be. */
    compute_name_linkage(idlbp);
  }  /* if */
  db_exit();
}  /* id_linkage */


static a_boolean incompatible_types_are_SVR4_compatible(a_type_ptr  tp1,
                                                        a_type_ptr  tp2)
/*
tp1 and tp2 are types that have already been determined to be incompatible.
However, we are in SVR4-C mode and the compatibility rules are relaxed in some
cases.  Return TRUE if the two types are compatible by these relaxed rules.
*/
{
  a_boolean   compat = FALSE;
  a_type_ptr  ret1;
  a_type_ptr  ret2;

  check_assertion(SVR4_C_mode);
  if (is_function_type(tp1)) {
    /* Two function types.  In SVR4 mode if they are incompatible solely
       because of their return types, and if the return types are "close
       enough", then consider the routine types themselves to be compatible.
       Further, if the return types are "close enough" and one type has a
       prototyped and the other a nonprototyped parameter list, also consider
       the types compatible. */
    tp1 = skip_typerefs(tp1);
    ret1 = tp1->variant.routine.return_type;
    check_assertion(is_function_type(tp2));
    tp2 = skip_typerefs(tp2);
    ret2 = tp2->variant.routine.return_type;
    if (types_are_compatible(ret1, ret2) ||
        (is_integral_or_enum_type(ret1) &&
         interchangeable_types(ret1, ret2))) {
      /* Either the return types are compatible or else they are incompatible
         but both are integral and they are interchangeable (i.e., they have
         the same size and alignment).  See whether the two routine types
         are otherwise compatible. */
      if (tp1->variant.routine.extra_info->prototyped !=
            tp2->variant.routine.extra_info->prototyped) {
        /* One of the routine types is prototyped and the other is not.
           Ignore incompatibilities (if there are any) in SVR4 mode. */
        compat = TRUE;
      } else {
        tp1->variant.routine.return_type = ret2;
        compat = types_are_compatible(tp1, tp2);
        /* Restore the original return type. */
        tp1->variant.routine.return_type = ret1;
      }  /* if */
    }  /* if */
  } else {
    check_assertion(!is_function_type(tp2));
    if (is_array_type(tp1)) {
      /* Array object types are "compatible" if they have the same element
         type. */
      tp1 = array_element_type(tp1);
      check_assertion(is_array_type(tp2));
      tp2 = array_element_type(tp2);
      compat = identical_types(tp1, tp2);
    } else {
      /* Non-array object types are "compatible" if they are
         interchangeable. */
      compat = interchangeable_types(tp1, tp2);
    }  /* if */
  }  /* if */
  return compat;
}  /* incompatible_types_are_SVR4_compatible */


static void recover_from_irreconcilable_external_symbol_types(
                                           a_type_ptr             latest_type,
                                           an_extern_symbol_descr *esdp,
                                           a_boolean              *okay)
/*
Decide the type of an IL entity and the mode with which to proceed when the
latest declaration of that entity conflicts with the previous (possibly
implicit) declaration.  latest_type is the type implied by the latest
declaration and esdp points to the relevant part of the IL entity (associated
with an sk_extern_routine or sk_extern_variable).  esdp->type is updated with
the type with which to proceed, and *okay is set to FALSE if the subsequent
processing should proceed in error mode.
*/
{
  if (!is_function_type(latest_type)) {
    /* A variable: Imbue an error type for recovery. */
    esdp->type = error_type();
    *okay = FALSE;
  } else {
    a_type_ptr  return_type =
                      skip_typerefs(latest_type)->variant.routine.return_type;
    if (is_auto_type(find_bottom_of_type(return_type))) {
      /* The latest declaration implies a deducible return type, but the
         earlier declaration may not match that, and the routine entry may
         therefore not reflect the deducibility.  Proceed with an error
         type. */
      esdp->type = latest_type;
      *okay = FALSE;
    } else if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
      /* A routine, and the latest declaration is in file-scope: Assume this
         latest declaration has the type intended by the programmer and
         proceed in error mode. */
      esdp->type = latest_type;
      *okay = FALSE;
    } else {
      /* The latest declaration is in block scope.  Proceed in non-error mode
         (although a diagnostic is still emitted for this conflict): This will
         cause the type of this declaration to prevail in this scope, and that
         of the previous declaration to be restored when this scope ends. */
    }  /* if */
  }  /* if */
}  /* recover_from_irreconcilable_external_symbol_types */


static void find_file_scope_decl(a_symbol_ptr  ext_sym,
                                 a_boolean     is_routine,
                                 a_boolean     *non_file_scope_decl_found,
                                 a_boolean     *file_scope_decl_found)
/*
Given an external symbol ext_sym, determine whether there's an intervening
declaration that hides an original at file scope by looping through the
symbol list (only valid in C mode).
*/
{
  a_symbol_ptr                sym = NULL;
  an_extern_symbol_descr_ptr  esdp;

  check_assertion(C_mode());
  /* The symbol header for the external symbol may be truncated and/or
     case insensitive.  We therefore need to determine the header of the
     symbol associated with an actual declaration. */
  esdp = ext_sym->variant.extern_symbol_descr;
  if (is_routine) {
    sym = (a_symbol_ptr)esdp->variant.routine.ptr->source_corresp.assoc_info;
  } else {
    sym = (a_symbol_ptr)esdp->variant.variable->source_corresp.assoc_info;
  }  /* if */
  if (sym != NULL) {
    sym = sym->header->symbol;
  }  /* if */
  for (; sym != NULL; sym = sym->next) {
    if (name_space_for_symbol_kind[(int)sym->kind] == nsk_other) {
      /* Found a symbol of the right sort. */
      if (sym->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
        if (is_tag_symbol(sym)) {
          /* Ignore a tag symbol and look for a routine or variable
             at file scope. */
        } else {
          /* Special handling of symbols encountered at file scope. */
          if ((is_routine &&
               sym->kind == (a_symbol_kind)sk_routine) ||
              (!is_routine &&
               sym->kind == (a_symbol_kind)sk_variable)) {
            *file_scope_decl_found = TRUE;
          }  /* if */
          break;
        }  /* if */
      } else if (*non_file_scope_decl_found) {
        /* We've already located an intervening declaration. */
      } else if (is_routine) {
        /* Current declaration is a routine. */
        if (sym->kind != (a_symbol_kind)sk_routine) {
          /* An intervening declaration of something other than a
             function.  This redeclaration is hidden from the original
             declaration, so issue a warning instead of an error. */
          *non_file_scope_decl_found = TRUE;
        }  /* if */
      } else {
        /* Current declaration is a variable. */
        if (sym->kind == (a_symbol_kind)sk_variable) {
          if (sym->variant.variable.ptr == NULL) {
            /* This is a symbol not yet bound to an IL entry, and so
               it is the one just now being created.  Skip past it. */
          } else if (sym->defined &&
                     sym->decl_scope !=
                            scope_stack[DEPTH_OF_FILE_SCOPE].number) {
            /* This is an intervening declaration of a local variable
               of a parameter. (We check the defined flag to rule out
               an intervening block extern declaration.) */
            *non_file_scope_decl_found = TRUE;
          }  /* if */
        } else {
          /* An intervening declaration of something other than a
             variable.  This redeclaration is hidden from the original
             declaration, so issue a warning instead of an error. */
          *non_file_scope_decl_found = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* find_file_scope_decl */


a_boolean reconcile_external_symbol_types(
                             a_symbol_ptr          ext_sym,
                             a_source_position_ptr position,
                             a_type_ptr            type_ptr,
                             an_error_severity     incompatible_severity)
/*
Change the type of the external symbol ext_sym to type_ptr.  External
symbol entries are constructed for all variables and routines with
linkage (external or internal) as a place to keep the pointer to the
unique IL entry (needed because the normal symbol entries will not
necessarily stay in scope for the entire compilation).  *position
gives the source position to be used in case of error.
An error about a type incompatibility should not be more severe than
incompatible_severity.  If FALSE is returned, an error was encountered.
Occasionally, encountering an error still produces a TRUE return value,
indicating that error recovery should proceed as if no error had occurred
(see also recover_from_irreconcilable_external_symbol_types).
*/
{
  an_extern_symbol_descr_ptr esdp;
  a_type_ptr                 old_type;
  a_boolean                  okay = TRUE;
  a_boolean                  is_routine;
  an_error_severity          severity = incompatible_severity;
  a_symbol_ptr               sym;
  a_boolean                  compat;
  a_boolean                  incompatible_linkage_spec = FALSE;

  db_enter(4, "reconcile_external_symbol_types");
  esdp = ext_sym->variant.extern_symbol_descr;
  old_type = esdp->type;
  /* If the old and new types are the same, no checking or processing is
     required. */
  if (!same_entities(old_type, type_ptr)) {
    /* Use a special comparison for routine types, to ignore calling
       convention differences.  In C mode, overloading is not possible, so
       allow error type mismatches on routine types. */
    is_routine = ext_sym->kind == (a_symbol_kind)sk_extern_routine;
    if (is_routine) {
      if (C_mode()) {
        compat = types_are_compatible(old_type, type_ptr);
      } else {
        compat = types_are_strictly_compatible(old_type, type_ptr);
        if (!compat &&
            routine_types_are_redecl_compatible(old_type, type_ptr,
                                                TCF_NO_FLAGS)) {
          okay = FALSE;
          incompatible_linkage_spec = TRUE;
        }  /* if */
      }  /* if */
    } else {
      compat = types_are_redecl_compatible(old_type, type_ptr);
    }  /* if */
    if (compat) {
      /* The old and new types are compatible.  Form the composite of
         those types, and save that as the type of the external symbol. */
      esdp->type = composite_type(old_type, type_ptr);
      if (is_error_type(old_type) || is_error_type(type_ptr)) {
        /* An error type is treated as "compatible" with any type in this
           context, we but we don't want to link two declarations if their
           types don't match.  (It could e.g. result in inconsistent
           initializer types.) */
        expect_error();
        okay = FALSE;
      }  /* if */
    } else {
      /* The old and new types are incompatible.  Issue a warning instead of
         an error for certain cases (e.g., SVR4 mode). */
      if (SVR4_C_mode) {
        if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
          /* Functions and variables are treated differently.  For example:
               void f() { extern unsigned g(), x; }
               int g();      // Warning in SVR4 C mode.
               int x;        // Error in SVR4 C mode.
          */
          if (is_routine &&
              incompatible_types_are_SVR4_compatible(old_type, type_ptr)) {
            severity = ((int)severity>(int)es_warning) ? es_warning : severity;
            /* Record the most recent type as the external symbol's type. */
            esdp->type = type_ptr;
            goto issue_diagnostic;
          }  /* if */
        } else if (is_array_type(type_ptr)) {
          /* Array types are compatible when the element types are the same
             no matter what the visibility constraints are. */
          if (incompatible_types_are_SVR4_compatible(old_type, type_ptr)) {
            severity = ((int)severity>(int)es_warning) ? es_warning : severity;
            goto issue_diagnostic;
          }  /* if */
        } else {
          /* The SVR4 algorithm is such that an error is issued if the
             incompatibility of a block extern declaration is with a visible
             declaration, but only a warning is given otherwise.  For example:
               extern int f();
               extern int ff();
               void g() { extern float f(); }                    // Error
               void gg() { int ff = 0; { extern float ff(); } }  // Warning
             Determine whether there's an intervening declaration that hides
             an original at file scope by looping through the symbol list.
             Since this only happens in C mode it is pretty straightforward. */
          a_boolean     non_file_scope_decl_found = FALSE;
          a_boolean     file_scope_decl_found = FALSE;
          find_file_scope_decl(ext_sym, is_routine, &non_file_scope_decl_found,
                               &file_scope_decl_found);
          if (non_file_scope_decl_found || !file_scope_decl_found) {
            /* Either there was no other declaration in scope (e.g., when the
               external symbol records another block extern declaration) or
               else there was an intervening declaration.  Issue a warning. */
            severity = ((int)severity>(int)es_warning) ? es_warning : severity;
            if (incompatible_types_are_SVR4_compatible(old_type, type_ptr)) {
              /* If this is a variable, record the most recent type as the
                 external symbol's type. */
              if (!is_routine) esdp->type = type_ptr;
            } else {
              okay = FALSE;
              if (file_scope_decl_found) {
                /* Retain the IL entry on the external symbol. */
              } else {
                /* Force "abandonment" of the IL entry associated with the
                   current external symbol. */
                if (is_routine) {
                  esdp->variant.routine.ptr->superseded_external = TRUE;
                  esdp->variant.routine.ptr = NULL;
                } else {
                  check_assertion_str2(
                             (esdp->variant.variable->storage_class ==
                                             (a_storage_class)sc_extern) &&
                             (esdp->variant.variable->init_kind ==
                                             (an_init_kind)initk_none),
                             "reconcile_external_symbol_types:",
                             "can't set superseded_external");
                  esdp->variant.variable->superseded_external = TRUE;
                  esdp->variant.variable = NULL;
                }  /* if */
                esdp->type = type_ptr;
              }  /* if */
            }  /* if */
            goto issue_diagnostic;
          }  /* if */
        }  /* if */
      } else if (microsoft_mode && is_routine) {
        if (C_mode()) {
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
            sym = NULL;
          } else {
            for (sym = ext_sym->header->symbol; sym != NULL; sym = sym->next) {
              if (sym->kind == (a_symbol_kind)sk_routine &&
                  sym->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          if (sym != NULL) {
            /* The current declaration is a block extern declaration and
               there has been a file-scope declaration of a function with the
               same name.  Don't reset the external symbol. */
          } else {
            /* Force "abandonment" of the IL entry associated with the external
               routine. */
            esdp->variant.routine.ptr->superseded_external = TRUE;
            esdp->variant.routine.ptr = NULL;
          }  /* if */
          if (is_function_type(old_type) && is_function_type(type_ptr) &&
              interchangeable_types(return_type_of(old_type),
                                    return_type_of(type_ptr))) {
            /* Microsoft accepts incompatible function types with a warning,
               as long as the return types are "interchangeable". */
            severity = ((int)severity>(int)es_warning) ? es_warning : severity;
          } else {
            okay = FALSE;
          }  /* if */
        } else if (!incompatible_linkage_spec) {
          /* In Microsoft C++ mode extern "C" routine declarations are
             allowed to have incompatible types when they appear in
             different namespaces. */
          severity = ((int)severity>(int)es_warning) ? es_warning : severity;
          goto issue_diagnostic;
        }  /* if */
      }  /* if */
      /* Decide how to proceed and which type to select for recovery: */
      recover_from_irreconcilable_external_symbol_types(type_ptr, esdp, &okay);
issue_diagnostic:
      /* The old and new types are incompatible.  Error. */
      if (incompatible_severity != es_none) {
        pos_sy_diagnostic(severity,
                          incompatible_linkage_spec ?
                            ec_incompatible_linkage_specifier :
                            ec_decl_incompatible_with_previous_use,
                          position, ext_sym);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return okay;
}  /* reconcile_external_symbol_types */

#if GNU_EXTENSIONS_ALLOWED

static a_boolean matching_builtin_function_name_exists(a_symbol_locator  *loc)
/*
If the name described by the given locator corresponds to the name of a GNU
builtin function without the "__builtin_" prefix, return TRUE.
*/
{
  a_boolean            result = FALSE;
  a_symbol_header_ptr  hdr = loc->symbol_header;
  a_symbol_ptr         matching_sym;
  a_symbol_locator     matching_loc;
  sizeof_t             builtin_name_length;

#define BF_PREFIX "__builtin_"
  builtin_name_length = sizeof(BF_PREFIX)+hdr->identifier_length-1;
  /* Create a name by prefixing "__builtin_" to the name for the given
     locator. */
  ensure_temp_text_buffer_space(builtin_name_length+1);
  strcpy(temp_text_buffer, BF_PREFIX);
  strcpy(temp_text_buffer+sizeof(BF_PREFIX)-1, hdr->identifier);
  /* Look up the prefixed name and check if it corresponds to a GNU built-in
     function. */
  matching_sym = find_symbol(temp_text_buffer, builtin_name_length,
                             &matching_loc);
#undef BF_PREFIX
  if (matching_sym != NULL &&
      is_simple_function_symbol(matching_sym) &&
      is_gnu_builtin_function(matching_sym->variant.routine.ptr)) {
    result = TRUE;
  }  /* if */
  return result; 
}  /* matching_builtin_function_name_exists */

#endif /* GNU_EXTENSIONS_ALLOWED */

static a_symbol_ptr create_external_symbol_for_linked_entity(
                            a_symbol_locator       *locator,
                            a_decl_parse_state     *dps,
                            a_type_ptr             type_ptr,
                            an_id_linkage_block    *idlbp,
                            a_boolean              redeclaration,
                            a_boolean              suppress_incompatible_error,
                            a_boolean              suppress_ext_sym_lookup,
                            a_variable_ptr         *variable_ptr,
                            a_routine_ptr          *routine_ptr)
/*
Find or create an external symbol entry for a variable or routine being
declared.  *locator gives the symbol locator for the identifier; dps describes
the declaration; type_ptr gives the variable or routine type; name_linkage
indicates the linkage (internal, external, C++ external).  Aside from creating
the entry, this routine checks that the new declaration is compatible with
any previous linked declaration of the same name.  redeclaration is
TRUE if the present declaration is a redeclaration within the same scope.
suppress_incompatible_error is TRUE to suppress incompatibility errors
detected in this routine, presumably because the caller has already
issued a similar error.  If *variable_ptr and *routine_ptr are both NULL,
meaning no IL entity has been found for the entity, the appropriate one
of the two will be set to point to the IL entity attached to the external
symbol, if there is one.  Note that the pointer from the external symbol
entry to the IL variable or routine is not filled in if the entry is
created; the caller must set it.
*/
{
  a_symbol_ptr               ext_sym;
  an_extern_symbol_descr_ptr esdp = NULL;
  a_const_char               *old_name, *new_name;
  a_symbol_kind              ext_sym_kind;
  a_symbol_locator           ext_locator;
  a_func_info_block_ptr      func_info = idlbp->func_info;
  a_boolean                  err = FALSE;
  a_boolean                  use_existing_il_entry = FALSE;
  a_type_ptr                 preexisting_type = NULL;
  a_boolean                  is_implicit_declaration = FALSE;
  a_boolean                  is_function;
  an_error_severity          incomp_severity = es_error;
  a_name_linkage_kind        name_linkage = idlbp->name_linkage;

  db_enter(4, "create_external_symbol_for_linked_entity");
  if (func_info != NULL) {
    check_assertion(is_function_type(type_ptr));
    is_function = TRUE;
    is_implicit_declaration = func_info->is_implicit_declaration;
    ext_sym_kind = (a_symbol_kind)sk_extern_routine;
  } else {
    is_function = FALSE;
    ext_sym_kind = (a_symbol_kind)sk_extern_variable;
  }  /* if */
  if (is_error_locator(*locator)) err = TRUE;
  if (suppress_ext_sym_lookup || err) {
    /* Ignore the presence of an external symbol with which the current
       symbol is compatible. */
    ext_sym = NULL;
    ext_locator = *locator;
    clear_specific_symbol(ext_locator);
  } else {
    /* Look up the external name of the identifier (i.e., the name after
       any truncation, etc.). */
    ext_sym = find_external_symbol(locator, name_linkage,
                                   is_function ? type_ptr : NULL,
                                   &ext_locator);
    if (ext_sym != NULL) {
      /* There is an existing external symbol for the name. */
      esdp = ext_sym->variant.extern_symbol_descr;
      if (!C_mode() && (microsoft_bugs || gpp_mode) &&
          depth_innermost_function_scope == NO_SCOPE_DEPTH &&
          name_linkage == (a_name_linkage_kind)nlk_external) {
        /* In Microsoft and GNU compilers, an extern "C" declaration in one
           namespace scope does not link up with an extern "C" declaration of
           the same name in another scope.  Since the linker will catch
           redefinitions of such names we do treat two definitions as linked
           (which will result in a redefinition error later on). */
        a_boolean  already_defined =
                    is_function ? esdp->variant.routine.ptr->defined :
                                  (esdp->variant.variable->storage_class != 
                                                  (a_storage_class)sc_extern);
        if (!(already_defined && idlbp->is_definition)) {
          ext_sym = NULL;
          ext_locator = *locator;
          clear_specific_symbol(ext_locator);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (ext_sym != NULL) {
    a_source_correspondence  *scp;
    /* In pcc mode, functions and extern variables are always effectively
       declared at the file scope level.  So if we get here in pcc mode, we
       must have encountered two incompatible declarations of the same name. */
    check_assertion(C_dialect != C_dialect_pcc || total_errors != 0);
    if (ext_sym->kind == (a_symbol_kind)sk_extern_variable) {
      scp = &esdp->variant.variable->source_corresp;
    } else {
      scp = &esdp->variant.routine.ptr->source_corresp;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (gcc_mode && scp->assoc_info != NULL &&
        (gnu_version < 30400 ||
         (is_function && ext_sym_kind == ext_sym->kind &&
          (is_implicit_declaration ||
           esdp->variant.routine.is_implicit_declaration) &&
          (gnu_version >= 40000 ||
           matching_builtin_function_name_exists(locator))))) {
      /* In GCC 3.3.x and earlier, block-external declarations declared in
         other function scopes are not required to be compatible with the
         current declaration.  GCC 4.0 and later only issue a warning if one
         of the declarations is an implicit function declaration.  GCC 3.4.x
         doesn't normally permit such redeclaration incompatibilities, but for
         most functions that have a __builtin_... counterpart, the implicit
         declaration would have acquired the type of that counterpart, thereby
         potentially avoiding the incompatibility.  We approximate that by
         accepting the incompatibility if the current declaration has a known
         __builtin_... counterpart. */
      a_symbol_ptr  prev_sym = (a_symbol_ptr)scp->assoc_info;
      a_boolean     is_local_to_function;
      if (scope_depth_of_symbol(prev_sym, &is_local_to_function) ==
                                                              NO_SCOPE_DEPTH) {
        /* ext_sym was created for a scope that has already been discarded.
           Incompatibilities are not fatal in such cases. */
        incomp_severity = es_warning;
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (suppress_incompatible_error) {
      incomp_severity = es_none;
    }  /* if */
    if (ext_sym_kind != ext_sym->kind) {
      /* The old entity is a variable and the new one is a routine, or
         vice-versa.  This is normally an error, but GNU C++ compilers
         accept the code if this is a block-extern declaration.  We only
         emulate the GNU behavior (with a warning) if the function has C++
         name linkage. */
      if (gpp_mode && idlbp->is_block_extern_decl) {
        /* Reduce the error to a warning if the routine has C++ name
           linkage. */
        a_name_linkage_kind  rtn_name_linkage;
        if (ext_sym->kind == (a_symbol_kind)sk_extern_routine) {
          rtn_name_linkage = scp->name_linkage;
        } else {
          rtn_name_linkage = idlbp->name_linkage;
        }  /* if */
        if (rtn_name_linkage == (a_name_linkage_kind)nlk_cplusplus_external) {
          incomp_severity = es_warning;
        }  /* if */
      }  /* if */
      if (!suppress_incompatible_error) {
        pos_sy_diagnostic(incomp_severity,
                          ec_decl_incompatible_with_previous_use,
                          &locator->source_position, ext_sym);
      }  /* if */
      err = TRUE;
      /* Force creation of a new external symbol. */
      ext_sym = NULL;
    } else {
      /* Both are variables, or both are routines.  Compare the old and
         new types; they must be compatible. */
      a_type_ptr  type_to_reconcile;
      if (is_function &&
          is_local_scope_kind(scope_stack[decl_scope_level].kind)) {
        /* If the current declaration is local, do not attempt to preserve its
           default arguments in the external symbol entry. */
        type_to_reconcile = routine_type_without_default_args(type_ptr);
      } else {
        type_to_reconcile = type_ptr;
      }  /* if */
      err = !reconcile_external_symbol_types(ext_sym,
                                             &locator->source_position,
                                             type_to_reconcile,
                                             incomp_severity);
      if (ext_sym_kind == (a_symbol_kind)sk_extern_routine) {
        /* If this declaration is not the result of an implicit
           declaration, clear the is_implicit_declaration flag in the
           external symbol entry. */
        if (!is_implicit_declaration) {
          esdp->variant.routine.is_implicit_declaration = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!redeclaration && !err) {
      /* Check if the old entity has a different name than the new entity,
         which would indicate an error (two different names ended up mapping
         to the same external name).  Even when the names are identical, a
         conflict can arise if the two entities are different in nature (e.g.,
         a C linkage declaration in a namespace can conflict with a C++
         variable declaration in global namespace because the latter variable's
         name is not mangled).  Note that the old entity may already have
         been given a mangled name (if it was mangled before being written
         to a PCH file), so compare against the unmangled name. */
      old_name = unmangled_name_of(scp);
      new_name = locator->symbol_header->identifier;
      check_assertion(old_name != NULL);
      if (!C_mode() && !is_function) {
        /* Check for external name conflicts between two variables. */
        an_error_severity  sev = es_none;
        an_error_code      diag = ec_no_error; 
        if ((a_name_linkage_kind)scp->name_linkage != name_linkage &&
            ((a_name_linkage_kind)scp->name_linkage ==
                                           (a_name_linkage_kind)nlk_external ||
             name_linkage == (a_name_linkage_kind)nlk_external)) {
          /* An extern "C" variable in one scope collides with a variable with
             a different linkage (usually "C++") in another scope.  Since most
             compilers accept this and it is mostly harmless, we only issue a
             warning in nonstrict modes. */
          sev = strict_ansi_mode ? strict_ansi_error_severity : es_warning;
          diag = ec_name_linkage_mismatch_for_variable;
        } else if (old_name != new_name && strcmp(old_name, new_name) != 0) {
          /* Two different names in the source end up being mapped onto the
             same external name (e.g., when external names are case-
             insensitive). */
          sev = es_discretionary_error;
          diag = ec_external_name_clash;
        }  /* if */
        if (sev != es_none) {
          if (!suppress_incompatible_error) {
            pos_sy_diagnostic(sev, diag, &locator->source_position, ext_sym);
          }  /* if */
          err = TRUE;
          /* Force creation of a new external symbol. */
          ext_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_function && !err) {
    if (!C_mode() && name_linkage == (a_name_linkage_kind)nlk_external &&
        !func_info->is_main_function) {
      /* This is an extern "C" function declaration in C++.  Be sure no other
         extern "C" function has been declared in this translation unit --
         only one is permitted with a given name, ignoring namespaces.
         (Microsoft compilers ignore this, so in Microsoft bugs mode we
         weaken this to a warning.) */
      a_symbol_ptr   sym = locator->symbol_header->other_symbols;
      a_routine_ptr  rp;
      for (; sym != NULL; sym = sym->next) {
        if (sym->kind == (a_symbol_kind)sk_extern_routine) {
          /* Ignore symbols not associated with the current file scope.  These
             could be extern entities associated with other translation
             units. */
          if (sym->decl_scope != file_scope_number) continue;
          rp = sym->variant.extern_symbol_descr->variant.routine.ptr;
          if (rp->source_corresp.name_linkage ==
                                     (a_name_linkage_kind)nlk_external &&
              !routine_types_are_redecl_compatible(
                                          type_ptr, rp->type, TCF_NO_FLAGS)) {
            /* Illegal overloading involving two extern "C" functions with
               the same name.  Microsoft and GNU C++ compilers let this
               through if the two declarations are in different namespaces. */
            err = !((microsoft_bugs || gpp_mode) &&
                    depth_scope_stack == depth_innermost_namespace_scope &&
                    sym_parent_namespace_or_null(sym) !=
                              scope_stack[depth_scope_stack].assoc_namespace);
            pos_sy_diagnostic(err ? es_error : es_warning,
                              ec_overloaded_function_linkage,
                              &locator->source_position, sym);
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  if (ext_sym == NULL) {
    /* There is no (compatible) external symbol entry for the identifier.
       Create one. */
    a_type_ptr  type_to_record = type_ptr;
    ext_sym = enter_extern_symbol(ext_sym_kind, &ext_locator);
    esdp = ext_sym->variant.extern_symbol_descr;
    if (ext_sym_kind == (a_symbol_kind)sk_extern_routine) {
      esdp->variant.routine.is_implicit_declaration = is_implicit_declaration;
      if (is_local_scope_kind(scope_stack[decl_scope_level].kind)) {
        type_to_record = routine_type_without_default_args(type_ptr);
      }  /* if */
    }  /* if */
    esdp->type = type_to_record;
    /* The pointer to the variable or routine IL entry is filled in later,
       by the caller of this routine. */
  }  /* if */
  if (!err) {
    /* If we do not already have an IL entry, and the external symbol entry
       points to one, reuse it. */
    /* Note that since err == FALSE we know that the external symbol and
       the new entity are both variables or both routines. */
    if (!is_function) {
      /* The entity being declared is a variable. */
      if (*variable_ptr == NULL) {
        *variable_ptr = ext_sym->variant.extern_symbol_descr->variant.variable;
        if (*variable_ptr != NULL) {
          /* There is a variable entry we can reuse. */
          use_existing_il_entry = TRUE;
          preexisting_type = (*variable_ptr)->type;
          if (depth_innermost_function_scope != NO_SCOPE_DEPTH &&
              (gcc_mode || (microsoft_mode && C_mode()))) {
            /* In Microsoft C and GNU C modes, the composite type is retained
               in the local scope. */
            (*variable_ptr)->type = esdp->type;
          } else {
            (*variable_ptr)->type = type_ptr;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* The entity being declared is a routine. */
      if (*routine_ptr == NULL) {
        *routine_ptr =
                    ext_sym->variant.extern_symbol_descr->variant.routine.ptr;
        if (*routine_ptr != NULL && !C_mode()) {
          a_symbol_ptr  rout_sym = symbol_for(*routine_ptr);
          if (func_info->is_definition &&
              routine_has_been_defined(*routine_ptr)) {
            /* This error can come up when the same extern "C" function is
               defined in two different namespaces -- e.g.,
                 namespace N { extern "C" void f() { } }
                 namespace M { extern "C" void f() { } }
            */
            an_error_severity  sev = microsoft_bugs ? es_warning : es_error;
            pos_sy_diagnostic(
                sev, ec_already_defined, &locator->source_position, rout_sym);
            *routine_ptr = NULL;
            ext_sym->variant.extern_symbol_descr->variant.routine.ptr = NULL;
          } else {
            /* Do compatibility checking on the throw specification. */
            check_exception_specification(type_ptr, rout_sym,
                                          &func_info->throw_position,
                                          /*is_redecl=*/TRUE);
          }  /* if */
        }  /* if */
        if (*routine_ptr != NULL) {
          /* There is a routine entry we can reuse. */
          use_existing_il_entry = TRUE;
          preexisting_type = (*routine_ptr)->type;
          if (skip_typerefs(preexisting_type)
                        ->variant.routine.extra_info->assoc_routine == NULL ||
              decl_scope_level != depth_innermost_namespace_scope) {
            /* In general we avoid modifying the routine type if it is already
               the type entry associated with the definition.  However, if
               we're not declaring the function in the current namespace
               scope, the routine type may need to be temporarily changed to
               (e.g.) pick up default arguments during template instantiations.
               In those cases, a fixup entry will be created (below) to later
               restore the original type. */
            if (dps->has_deducible_return_type &&
                (*routine_ptr)->has_deduced_return_type) {
              /* Even if we (temporarily) use the declared type at this point,
                 we have to use the return type already deduced to avoid
                 spurious "auto" types in the expression trees. */
              type_ptr->variant.routine.return_type =
                            (*routine_ptr)->type->variant.routine.return_type;
            }  /* if */
            (*routine_ptr)->type = type_ptr;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (use_existing_il_entry) {
      /* An existing IL entry can be reused. */
      if (dps->prev_type == NULL) dps->prev_type = preexisting_type;
      /* See if the entry's type has been changed.  Note that we're checking
         for pointer equality here, so an equivalent but distinct type
         will fail to match.  One of the issues that deals with is routine
         types with associated routine pointers -- the associated routine
         pointer needs to be preserved. */
      if (!same_entities(type_ptr, preexisting_type)) {
        /* The type has been changed.  See if the pre-existing type will
           have to be restored at the end of the current scope.  If so,
           create a fixup entry that will be processed by pop_scope.  The
           type must be restored in a case like
             int a[];
             main () {
               extern int a[5];
               ... Type of "a" is now "int [5]".
             }
             ... Type of "a" must be restored to "int []" at the end of "main".
           Another example involves local typedefs:
             int *p;
             void f() {
               typedef int *IP;
               extern IP p;
             }  // "p" cannot have type "IP" in file scope; restore "int*"
        */
        /* The fixup is only needed if the pre-existing definition is in
           a scope that surrounds the current one.  That's hard to determine,
           since it's hard to know the scope associated with the previous
           definition (the associated symbol gives only one scope, and
           not necessarily the outermost).  A simple way out is to build
           the fixup whenever the current scope is not the file scope.
           That builds more fixups than needed, but it always works. */
        if (C_dialect == C_dialect_pcc &&
            !identical_types(type_ptr, preexisting_type)) {
          /* In pcc mode all symbols with linkage are entered at the file
             scope, so a fixup is never needed.  However, that leaves the
             possibility that the external entity is typed with a function
             scope typedef (see example above).  To reduce the occurrence
             of such unaesthetic situations, a fixup is applied if the
             types are otherwise identical. */
        } else if (decl_scope_level != depth_innermost_namespace_scope) {
          /* Create a fixup entry, which will be processed at the end
             of the current scope (see pop_scope).  Note that the
             allocation routine places the new entry on the fixup list
             for the current scope. */
          an_extern_type_fixup_ptr etfp;
          etfp = alloc_etype_fixup();
          etfp->type = preexisting_type;
          etfp->is_routine = is_function;
          if (!is_function) {
            etfp->variant.variable = *variable_ptr;
          } else {
            etfp->variant.routine = *routine_ptr;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return ext_sym;
}  /* create_external_symbol_for_linked_entity */


#if MICROSOFT_EXTENSIONS_ALLOWED

void update_dll_info_for_routine(a_routine_ptr         routine,
                                 a_decl_modifier       flags,
                                 a_boolean             is_inline,
                                 a_boolean             is_redecl,
                                 a_boolean             is_definition,
                                 a_source_position     *diag_pos)
/*
Update the DLL interface of the given routine according to flags.  If the
current declaration of the routine is marked as "inline", is_inline is set to
TRUE (even when routine->is_inline may not yet have been set).  If the current
declaration is a redeclaration, is_redecl is set to TRUE.  If the current
declaration is a definition, is_definition is set to TRUE.  Diagnostics should
be issued at the given position.
*/
{
  a_decl_modifier  old_dll_flags = (routine->decl_modifiers & DM_DLLFLAGS);
  a_decl_modifier  new_dll_flags = (flags & DM_DLLFLAGS);

  /* dllimport and dllexport should never be set together. */
  check_assertion(new_dll_flags != DM_DLLFLAGS);
  if ((old_dll_flags | new_dll_flags) != 0) {
    /* Either the current declaration has a DLL interface, or the routine was
       previously declared with a DLL interface. */
    a_boolean  new_dll_export = FALSE, clear_dll_import = FALSE;
    a_boolean  freeze_dll_import = FALSE;
    if (routine->is_inline) is_inline = TRUE;
    if (new_dll_flags != 0) {
      if (routine->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_internal) {
        /* Entities that don't have external linkage cannot be declared with
           a DLL interface. */
        pos_error(ec_dll_interface_requires_external_linkage, diag_pos);
        goto done;
      } else if (routine->source_corresp.name_linkage !=
                                          (a_name_linkage_kind)nlk_external &&
                 is_member_of_unnamed_namespace(&routine->source_corresp)) {
        /* dllimport/dllexport on an unnamed namespace member is unlikely to
           be useful. */
        pos_warning(ec_dll_interface_in_unnamed_namespace, diag_pos);
      }  /* if */
    } else {
      /* The current declaration has no DLL interface, but a previous
         declaration did.  A previous dllexport is preserved, but a previous
         dllimport is dropped.  Exceptions appear to be inline functions,
         block-extern declarations, and out-of-class definitions of members
         of class templates. */
      if ((old_dll_flags & DM_DLLIMPORT) != 0) {
        if (innermost_function_scope != NULL || is_inline ||
            (routine->is_prototype_instantiation && is_definition &&
             routine->source_corresp.is_class_member)) {
          /* Inline functions, block-extern declarations, and out-of-class
             definitions of members of class templates retain the dllimport
             attribute specified on the original declaration.  Setting
             freeze_dll_import ensures the dllimport attribute won't be
             discarded in what follows. */
          freeze_dll_import = TRUE;
          new_dll_flags = DM_DLLIMPORT;
        } else {
          clear_dll_import = TRUE;
          if (microsoft_version <= 1200 || is_definition) {
            /* If the current declaration is a definition (or if we're
               emulating an older Microsoft version), dllimport is
               implicitly replaced by dllexport. */
            new_dll_flags = DM_DLLEXPORT;
          }  /* if */
        }  /* if */
      } else {
        /* Preserve the dllexport attribute of the previous declaration. */
        new_dll_flags = DM_DLLEXPORT;
      }  /* if */
    }  /* if */
    if (old_dll_flags == new_dll_flags) {
      /* This is a redeclaration and it is compatible with the previous
         declaration: Nothing to be done.  (It could also be a full
         instantiation compatible with a prior partial instantiation or a
         declaration of an extern "C" function also declared in another
         namespace.) The "compatibility" may be a result of carrying over the
         dll attribute (e.g., for inline functions or block-extern
         declarations). */
    } else if (old_dll_flags == 0) {
      /* This is the first time a DLL interface is specified: If there was a
         previous declaration, issue a discretionary error. */
      if (is_redecl) {
        pos_sy_diagnostic(es_discretionary_error,
                          ec_redeclaration_adds_dll_interface, diag_pos,
                          symbol_for(routine));
      }  /* if */
      routine->decl_modifiers |= new_dll_flags;
      if ((new_dll_flags & DM_DLLEXPORT) != 0) {
        new_dll_export = TRUE;
      } else if (is_inline) {
        /* The combination of "inline" and "dllimport" indicates that the body
           should only be used for inlining.  It should never be spilled. */
        routine->definition_for_inlining_only = TRUE;
        routine->suppress_inline_body = TRUE;
      }  /* if */
    } else if (!freeze_dll_import) {
      /* A declaration that conflicts with a previous declaration: Issue a
         warning and ignore any dllimport attribute. */
      an_error_code  err_code;
      clear_dll_import = TRUE;
      routine->decl_modifiers |= (new_dll_flags & DM_DLLEXPORT);
      if ((routine->decl_modifiers & DM_DLLEXPORT) != 0) {
        err_code = ec_dll_interface_conflict_dllexport_assumed;
      } else {
        err_code = ec_dll_interface_conflict_none_assumed;
      }  /* if */
      pos_sy_warning(err_code, diag_pos, symbol_for(routine));
    }  /* if */
    if (is_definition && !is_inline && (new_dll_flags & DM_DLLIMPORT) != 0 &&
        !clear_dll_import) {
      /* Noninline function definitions cannot have the dllimport attribute.
         However, early versions of the Microsoft compilers did not diagnose
         this when parsing a template in its generic form. */
      an_error_severity  severity = es_error;
      if (microsoft_version <= 1200 && routine->is_prototype_instantiation &&
          routine->source_corresp.is_class_member) {
        severity = es_warning;
      } else {
        clear_dll_import = TRUE;
      }  /* if */
      pos_diagnostic(severity, ec_cannot_define_dllimport_function, diag_pos);
    }  /* if */
    if (clear_dll_import && (routine->decl_modifiers & DM_DLLIMPORT) != 0) {
      /* Drop any previous dllimport attribute. */
      routine->decl_modifiers &= ~(a_decl_modifier)DM_DLLIMPORT;
      routine->definition_for_inlining_only = FALSE;
      new_dll_export = ((routine->decl_modifiers & DM_DLLEXPORT) != 0);
    }  /* if */
    if (new_dll_export && is_inline) {
      /* dllexport routines must always have an out-of-line copy.  (If extern
         inline routines are instantiated, that instantiation is requested
         below.) */
      routine->need_out_of_line_copy = TRUE;
    }  /* if */
    if (new_dll_export &&
        !routine->is_prototype_instantiation &&
        ((routine->is_template_function && !routine->is_specialized)
#if INSTANTIATE_EXTERN_INLINE
         || is_inline
#endif /* INSTANTIATE_EXTERN_INLINE */
                     )) {
      /* dllexport forces the instantiation of nonexplicit specializations.
         If inline functions are "instantiated", this also applies to inline
         functions. */
      set_instance_required(symbol_for(routine), TRUE, SIR_DEFER_INLINE);
    }  /* if */
  }  /* if */
done:;
}  /* update_dll_info_for_routine */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if DECL_MODIFIERS_IN_USE
#if !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* is_redecl and is_definition are only used in Microsoft
                  mode. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
void update_routine_decl_modifiers(
                             a_routine_ptr               routine,
                             a_decl_modifiers_block_ptr  new_modifiers,
                             a_source_position           *position,
                             a_boolean                   is_redecl,
                             a_boolean                   is_definition,
                             a_boolean                   is_inline)
/*
Update the decl_modifiers field of the routine entry to reflect the modifiers
specified in new_modifiers.  (Some modifiers -- notably, most __declspec
attributes -- were already applied through the general attribute application
mechanism.) If this is a redeclaration or definition of a previously defined
routine, make sure that the new modifiers are consistent with the previous
declaration specified by routine.  position is used as the error position for
any diagnostics.
*/
{
  a_decl_modifier  flags = new_modifiers->flags;

#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Handle dllexport and dllimport separately. */
  update_dll_info_for_routine(routine, flags, is_inline, is_redecl,
                              is_definition, position);
  /* Set the Microsoft-specific inlining flags. */
  routine->decl_modifiers |= (flags & (DM_MICROSOFT_INLINE | DM_FORCEINLINE));
  flags &= ~(a_decl_modifier)(DM_DLLFLAGS |
                              DM_MICROSOFT_INLINE | DM_FORCEINLINE);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  /* If any of the Sun link scope specifiers were seen, handle them now. */
  if (flags & DM_ANY_SUN_LINK_SCOPE) {
    if (routine->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_internal ||
        routine->source_corresp.name_linkage ==
                                              (a_name_linkage_kind)nlk_none) {
      pos_error(ec_link_scope_requires_external_linkage, position);
    } else if ((flags & DM_ANY_SUN_LINK_SCOPE) != 0) {
      /* A redeclaration cannot relax the link scope of a routine. */
      if (is_redecl &&
          (flags & DM_ANY_SUN_LINK_SCOPE) <
                          (routine->decl_modifiers & DM_ANY_SUN_LINK_SCOPE)) {
        pos_error(ec_link_scope_relaxation, position);
      } else {
        /* An explicit instantiation may include a link scope different from
           that recorded in the template.  Therefore, we clear any existing
           link scope recorded in the entry. */
        routine->decl_modifiers &= (a_decl_modifier)~DM_ANY_SUN_LINK_SCOPE;
        routine->decl_modifiers |= (flags & DM_ANY_SUN_LINK_SCOPE);
      }  /* if */
    }  /* if */
    flags &= (a_decl_modifier)~DM_ANY_SUN_LINK_SCOPE;
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (flags & DM_THREAD) {
    /* The "thread" specifier can only be applied to variables with a static
       lifetime. */
    pos_error(ec_cannot_use_thread_local_storage, position);
    flags &= (a_decl_modifier)~DM_THREAD;
  }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  check_assertion(flags == 0 || total_errors != 0);
}  /* update_routine_decl_modifiers */

#if MICROSOFT_EXTENSIONS_ALLOWED

void update_dll_info_for_variable(a_variable_ptr        var,
                                  a_decl_modifier       flags,
                                  a_boolean             is_redecl,
                                  a_boolean             is_definition,
                                  a_source_position     *diag_pos)
/*
Update the DLL interface of the given variable according to flags (and clear
the DLL-related flags of flags).  If the current declaration is a definition,
is_definition is set to TRUE.  If the current declaration is a redeclaration,
is_redecl is set to TRUE.  Diagnostics should be issued at the given
position. */
{
  a_decl_modifier  old_dll_flags = (var->decl_modifiers & DM_DLLFLAGS);
  a_decl_modifier  new_dll_flags = (flags & DM_DLLFLAGS);

  /* dllimport and dllexport should never be set together. */
  check_assertion(new_dll_flags != DM_DLLFLAGS);
  if ((old_dll_flags | new_dll_flags) != 0) {
    /* Either the current declaration has a DLL interface, or the variable was
       previously declared with a DLL interface. */
    a_boolean  new_dll_export = FALSE, clear_dll_import = FALSE;
    a_boolean  freeze_dll_import = FALSE;
    if (var->source_corresp.is_class_member) {
      /* A static data member. */
      if (is_definition &&
          ((old_dll_flags | new_dll_flags) & DM_DLLIMPORT) != 0) {
        /* Imported static data members cannot be defined.  Note that an error
           is issued even if one declaration has dllimport and the other has
           dllexport: Unlike for other entities, Microsoft compilers retain the
           dllimport attribute on static data members. */
        pos_error(ec_dllimport_defined, diag_pos);
        old_dll_flags = new_dll_flags = 0;
      }  /* if */
    } else if (new_dll_flags != 0) {
      /* Entities that don't have external linkage cannot be declared with a
         DLL interface. */
      if (var->source_corresp.name_linkage == (a_name_linkage_kind)nlk_none) {
        /* An error should already have been issued for local variables. */
        check_assertion(total_errors != 0);
        goto done;
      } else if (var->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_internal) {
        pos_error(ec_dll_interface_requires_external_linkage, diag_pos);
        goto done;
      }  /* if */
    } else {
      /* The current declaration has no DLL interface, but a previous
         declaration did.  A previous dllexport is preserved, but a previous
         dllimport is dropped (except if the current declaration is in block
         scope). */
      if (old_dll_flags & DM_DLLIMPORT) {
        if (innermost_function_scope != NULL) {
          /* A block-extern declaration: Carry over the dllimport attribute.
             Setting freeze_dll_import ensures the dllimport attribute won't
             be discarded in what follows. */
          freeze_dll_import = TRUE;
          new_dll_flags = DM_DLLIMPORT;
        } else {
          clear_dll_import = TRUE;
          if (microsoft_version <= 1200 || is_definition) {
            /* If the current declaration is a definition (or if we're
               emulating an older Microsoft version), dllimport is implicitly
               replaced by dllexport. */
            new_dll_flags = DM_DLLEXPORT;
          }  /* if */
        }  /* if */
      } else {
        /* Preserve the dllexport attribute of the previous declaration. */
        new_dll_flags = DM_DLLEXPORT;
      }  /* if */
    }  /* if */
    if (new_dll_flags != 0 &&
        var->source_corresp.name_linkage !=
                                          (a_name_linkage_kind)nlk_external &&
        is_member_of_unnamed_namespace(&var->source_corresp)) {
      /* dllimport/dllexport on an unnamed namespace member is unlikely to
         be useful. */
      pos_warning(ec_dll_interface_in_unnamed_namespace, diag_pos);
    }  /* if */
    if (old_dll_flags == new_dll_flags) {
      /* This is a redeclaration and it is compatible with the previous
         declaration: Nothing to be done.  (It could also be a full
         instantiation compatible with a prior partial instantiation.)
         The "compatibility" may be a result of carrying over the dll
         attribute on or from a block-extern declaration. */
    } else if (old_dll_flags == 0) {
      /* This is the first time a DLL interface is specified: If there was a
         previous declaration, issue a discretionary error. */
      if (is_redecl) {
        pos_sy_diagnostic(es_discretionary_error,
                          ec_redeclaration_adds_dll_interface,
                          diag_pos, symbol_for(var));
      }  /* if */
      var->decl_modifiers |= new_dll_flags;
      if ((new_dll_flags & DM_DLLEXPORT) != 0) {
        new_dll_export = TRUE;
      }  /* if */
    } else if (!freeze_dll_import) {
      /* A declaration that conflicts with a previous declaration: Issue a
         warning and ignore any dllimport attribute. */
      an_error_code  err_code;
      clear_dll_import = TRUE;
      var->decl_modifiers |= (new_dll_flags & DM_DLLEXPORT);
      if ((var->decl_modifiers & DM_DLLEXPORT) != 0) {
        err_code = ec_dll_interface_conflict_dllexport_assumed;
      } else {
        err_code = ec_dll_interface_conflict_none_assumed;
      }  /* if */
      pos_sy_warning(err_code, diag_pos, symbol_for(var));
    }  /* if */
    if (clear_dll_import && (var->decl_modifiers & DM_DLLIMPORT) != 0) {
      /* Drop any previous dllimport attribute. */
      var->decl_modifiers &= ~(a_decl_modifier)DM_DLLIMPORT;
      new_dll_export = ((var->decl_modifiers & DM_DLLEXPORT) != 0);
    }  /* if */
    if (new_dll_export &&
        var->is_template_static_data_member && !var->is_specialized) {
      /* dllexport forces the instantiation of nonexplicit specializations. */
      set_instance_required(symbol_for(var), TRUE, SIR_NONE);
    }  /* if */
  }  /* if */
done:;
}  /* update_dll_info_for_variable */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void update_variable_decl_modifiers(a_decl_parse_state  *dps)
/*
*dps describes a variable declaration.  Update the variable's IL entry to
reflect modifiers that appeared in the declaration.  If this is a redeclaration
or a definition of a previously declared variable, make sure that the new
modifiers are consistent with previous declarations.  (Some modifiers --
notably, most __declspec attributes -- are applied through the general
attribute application mechanism.)
*/
{
  a_variable_ptr   variable = NULL;
  a_decl_modifier  flags;

  if (dps->sym->kind == (a_symbol_kind)sk_variable) {
    variable = dps->sym->variant.variable.ptr;
  } else if (dps->sym->kind == (a_symbol_kind)sk_static_data_member) {
    variable = dps->sym->variant.static_data_member.variable;
  } else {
    unexpected_condition();
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Handle dllexport and dllimport separately. */
  if (dps->in_class_scope) {
    merge_dll_flags_from_parent_class(parent_class_of(variable), dps);
  }  /* if */
  flags = dps->decl_modifiers.flags;
  update_dll_info_for_variable(variable, flags, !dps->first_decl,
                               dps->is_definition, &dps->declarator_pos);
  if (flags & DM_MICROSOFT_INLINE) {
    pos_st_diagnostic(es_discretionary_error,
                      ec_decl_modifiers_invalid_for_this_decl,
                      &dps->declarator_pos,
                      decl_modifier_names[(int)dmt_microsoft_inline]);
  }  /* if */
  if (flags & DM_FORCEINLINE) {
    pos_st_diagnostic(es_discretionary_error,
                      ec_decl_modifiers_invalid_for_this_decl,
                      &dps->declarator_pos,
                      decl_modifier_names[(int)dmt_forceinline]);
  }  /* if */
  flags &= ~(a_decl_modifier)(DM_DLLFLAGS |
                              DM_MICROSOFT_INLINE | DM_FORCEINLINE);
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
  flags = dps->decl_modifiers.flags;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  /* If any of the Sun link scope specifiers were seen, handle them now. */
  if (flags & DM_ANY_SUN_LINK_SCOPE) {
    if (variable->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_internal ||
        variable->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_none) {
      pos_error(ec_link_scope_requires_external_linkage,
                &dps->declarator_pos);
    } else if ((flags & DM_ANY_SUN_LINK_SCOPE) != 0) {
      /* A redeclaration cannot relax the link scope of a variable. */
      if ((flags & DM_ANY_SUN_LINK_SCOPE) <
                         (variable->decl_modifiers & DM_ANY_SUN_LINK_SCOPE)) {
        pos_error(ec_link_scope_relaxation, &dps->declarator_pos);
      } else {
        variable->decl_modifiers |= (flags & DM_ANY_SUN_LINK_SCOPE);
      }  /* if */
    }  /* if */
    flags &= (a_decl_modifier)~DM_ANY_SUN_LINK_SCOPE;
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (flags & DM_THREAD) {
    if (!var_has_static_or_thread_storage_duration(variable)) {
      /* The "thread" specifier can only be applied to variables with a static
         lifetime. */
      pos_error(ec_cannot_use_thread_local_storage, &dps->declarator_pos);
    } else if (!dps->first_decl && !(variable->decl_modifiers & DM_THREAD)) {
      /* This variable was previously declared with no thread locality. */
      pos_sy_error(ec_incompatible_thread_locality, &dps->declarator_pos,
                   dps->sym);
    } else {
      variable->decl_modifiers |= DM_THREAD;
    }  /* if */
    flags &= (a_decl_modifier)~DM_THREAD;
  } else if (!microsoft_mode && !dps->first_decl &&
             (variable->decl_modifiers & DM_THREAD) != 0) {
    /* Issue an error when a variable previously declared as thread-local does
       not repeat the __thread specifier.  (Microsoft compilers do not
       diagnose the equivalent __declspec(thread) case.) */
    pos_sy_diagnostic(es_discretionary_error, ec_incompatible_thread_locality,
                      &dps->declarator_pos, dps->sym);
  }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
  check_assertion(flags == 0);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if ((variable->decl_modifiers & DM_DLLFLAGS) &&
      (variable->decl_modifiers & DM_THREAD)) {
    pos_error(ec_dll_thread_conflict, &dps->declarator_pos);
    variable->decl_modifiers &= ~(a_decl_modifier)DM_THREAD;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* update_variable_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

static void check_for_linkage_conflict(a_storage_class    *old_storage_class,
                                       an_id_linkage_kind *linkage,
                                       a_storage_class    *storage_class,
                                       a_source_position  *position,
                                       a_boolean          suppress_diagnostic)
/*
A variable or routine is being declared again.  The existing storage class
of the entity is *old_storage_class.  The linkage and storage class of the
new declaration are given by *linkage and *storage_class.  Issue a diagnostic
(at the indicated position) if the old and new linkages conflict, and update
*linkage, *storage_class, and *old_storage_class appropriately.
*/
{
  if ((*linkage == idl_internal) !=
                          (*old_storage_class == (a_storage_class)sc_static)) {
    /* External versus internal linkage conflict. */
    if (!suppress_diagnostic) {
      /* There is a conflict between a prior declaration and the current one.
         This is clearly an error in C++ (ARM 7.1.1, 7.1.2), but because of
         prevailing practice we only issue a remark.  The same is done in
         C mode, partly because it is common practice in pcc. */
      pos_diagnostic((strict_ansi_mode ?
                           strict_ansi_error_severity : es_remark),
                     ec_linkage_conflict, position);
    }  /* if */
    /* If either declaration has unspecified storage class (i.e., it's an
       external definition), that takes precedence, and the entity should
       have unspecified storage class.  Otherwise, because of the test above,
       one or the other of the declarations will have static storage class,
       and the entity should have static storage class.  extern storage
       class just defers to the other storage class, i.e., it's considered a
       reference to something else that is not necessarily external.  That
       means, for example, that "extern int f();" followed by
       "static int f();" yields a static routine (that's how pcc does it,
       and it's undefined according to the standard). */
    if (*old_storage_class == (a_storage_class)sc_unspecified ||
        *storage_class == (a_storage_class)sc_unspecified) {
      *storage_class = (a_storage_class)sc_unspecified;
      *linkage = idl_external;
    } else {
      *storage_class = (a_storage_class)sc_static;
      *linkage = idl_internal;
    }  /* if */
    *old_storage_class = *storage_class;
  }  /* if */
}  /* check_for_linkage_conflict */


void check_default_args_for_param_type(a_param_type_ptr  ptp,
                                       a_source_position *pos)
/*
Given a param type pointer, make sure that any parameter with a default
argument value is followed only by other parameters with defaults or by a
parameter pack.  Issue an error otherwise.
*/
{
  /* Loop through the single list. */
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg && ptp->next != NULL &&
        !ptp->next->has_default_arg && !ptp->next->is_parameter_pack) {
      /* Current parameter has a default argument while its successor does not
         and isn't a parameter pack.  Report the error and break out of the
         loop. */
      pos_error(ec_default_arg_not_at_end, pos);
      break;
    }  /* if */
  }  /* for */
}  /* check_default_args_for_param_type */


static void check_default_args(a_decl_parse_state  *dps)
/*
dps describes the current declaration, where there is no prior declaration
with which to merge it.  If appropriate, look for the case in which a
parameter with a default argument is followed in the parameter list by one
without a default argument, and report the error.  (In many cases, this check
can be delayed until the default arguments are actually scanned, but with,
e.g., templates, we do not know if the default arguments will be scanned at
all.  If a C++/CLI param array is present, issue an error if a default
argument is encountered at all.
*/
{
  a_type_ptr        type = skip_typerefs(dps->type);
  a_param_type_ptr  ptp;

  /* Loop through the single list. */
  ptp = type->variant.routine.extra_info->param_type_list;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled && is_cli_param_array_routine_type(type)) {
    a_param_type_ptr  p;
    a_boolean         found_default_arg = FALSE;
    for (p = ptp; p != NULL; p = p->next) {
      if (p->has_default_arg) {
        found_default_arg = TRUE;
        break;
      }  /* if */
    }  /* for */
    if (found_default_arg) {
      /* Default arguments are not allowed in a function with a param array. */
      pos_error(ec_default_arg_used_in_param_array_function, &error_position);
    }  /* if */
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  if (dps->routine_fixup == NULL) {
    /* No fixup is recorded to scan the default arguments later on.  Check the
       constraint now. */
    check_default_args_for_param_type(ptp, &error_position);
  }  /* if */
}  /* check_default_args */


static void check_for_any_default_args(a_type_ptr type)
/*
Check whether the routine type pointer specified by type contains any
default arguments.  Issue an error if any are found.
*/
{
  a_param_type_ptr  ptp;

  /* Look for a param type entry with a default argument. */
  ptp = skip_typerefs(type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg) {
      pos_diagnostic(es_discretionary_error, ec_default_arg_expr_not_allowed,
                     &error_position);
      break;
    }  /* if */
  }  /* for */
}  /* check_for_any_default_args */


static void check_default_arg_compatibility(a_type_ptr  orig_type,
                                            a_type_ptr  new_type,
					    a_boolean	is_function_template)
/*
Given an existing routine type (orig_type) and the type based on a new
declaration (new_type), compare the default argument expressions on a
parameter-by-parameter basis and report any errors.  The merging of the
default arguments occurs in composite_type.  is_function_template is TRUE if
the associated routine is a function template (but not a member function of
a class template).  This routine is only called for template-based cases,
where we don't know if we will ever scan the default argument caches at all
(therefore, to be sure, we issue related diagnostics at the time of the
redeclaration).
*/
{
  a_boolean         not_at_end_of_list_error = FALSE;
  a_boolean         redecl_error = FALSE;
  a_boolean         default_arg_required = FALSE;
  a_param_type_ptr  ptp1, ptp2;

  /* Loop through the two lists in tandem.  We may assume that they are
     of equal length. */
  ptp1 = skip_typerefs(orig_type)->variant.routine.extra_info->param_type_list;
  ptp2 = skip_typerefs(new_type)->variant.routine.extra_info->param_type_list;
  for (; ptp1 != NULL; ptp1 = ptp1->next, ptp2 = ptp2->next) {
    if (ptp1->has_default_arg) {
      /* The parameter on the original type has a default arg. */
      if (ptp2->has_default_arg) {
        /* So does the parameter on the new type.  This is illegal. */
        redecl_error = TRUE;
      }  /* if */
      default_arg_required = TRUE;
    } else if (ptp2->has_default_arg) {
      default_arg_required = TRUE;
    } else if (default_arg_required) {
      not_at_end_of_list_error = TRUE;
    }  /* if */
  }  /* for */
  if (redecl_error) {
    an_error_severity	severity = es_error;
    if (((gpp_mode && is_function_template) ||
         (microsoft_mode && microsoft_version >= 1300 &&
          !is_function_template))  &&
        scope_is(&scope_stack_top(), sck_template_declaration)) {
      /* g++ ignores redeclared default arguments in function template
         declarations.  Microsoft (versions 1300 and above) ignores redeclared
         default arguments in member functions of class templates. */
      severity = es_warning;
    }  /* if */
    diagnostic(severity, ec_default_arg_already_defined);
  }  /* if */
  if (not_at_end_of_list_error) {
    error(ec_default_arg_not_at_end);
  }  /* if */
}  /* check_default_arg_compatibility */


void check_old_specialization_allowed(a_symbol_ptr       sym,
                                      a_source_position  *pos)
/*
Issue a discretionary error if old-style template specializations are not
allowed.  sym is the instance symbol, pos the error position.  If this is
called for a class template specialization, the current token must be the
one following the class specifier (a colon or brace in the case of a
definition).
*/
{
  if (!old_specializations_allowed) {
    an_error_code  code = ec_no_error;
    if (microsoft_mode) {
      if (sym->is_class_member) {
        /* All current versions of Microsoft C++ compilers appear to accept
           old-style specializations of members of class templates. */
      } else if (microsoft_version >= 1310 && microsoft_version < 1400 &&
                 is_class_symbol(sym)) {
        /* Microsoft C++ 8.0 (microsoft_version == 1400) rejects every kind of
           old-style nonmember specialization.  Microsoft C++ 7.0 and earlier
           accept those cases (and old_specializations_allowed is TRUE by
           default when microsoft_version < 1310).  However, Microsoft C++ 7.1
           accepts old-style specializations of class templates that aren't
           definitions. */
        if (curr_token == tok_colon || curr_token == tok_lbrace) {
          code = ec_old_specialization_not_allowed;
        }  /* if */
      } else {
        code = ec_old_specialization_not_allowed;
      }  /* if */
    } else if (strict_ansi_mode) {
      /* Old-style template specialization is nonstandard. */
      code = ec_nonstd_old_specialization;
    } else {
      /* Old-style template specialization is not allowed. */
      code = ec_old_specialization_not_allowed;
    }  /* if */
    if (code != ec_no_error) {
      pos_sy_diagnostic(es_discretionary_error, code, pos, sym);
    }  /* if */
  }  /* if */
}  /* check_old_specialization_allowed */


void reconcile_routine_types(a_routine_ptr       routine_ptr,
                             a_type_ptr          type_ptr,
                             a_boolean           preserve_rout_type,
                             a_boolean           preserve_type_ptr,
                             a_decl_parse_state  *dps)
/*
The routine routine_ptr has been given both the type it already has (i.e.,
routine_ptr->type) and the other type given by type_ptr; it may be assumed
that the two types are compatible.  Check the consistency of the default
argument specifications, if any, of the two types and set the routine type
to the composite of the two types.  If preserve_rout_type is TRUE,
routine_ptr->type must remain the same pointer; that type entry is
guaranteed to be unshared and is modified if necessary.  If
preserve_type_ptr is TRUE, routine_ptr->type must end up equal to type_ptr,
and type_ptr is guaranteed to be unshared and is modified if necessary.
Those flags are used when the associated type is part of a function
definition, and therefore already contains definition information like
assoc_routine which should not be overridden.  Obviously, both flags may
not be TRUE.
*dps describes the current declaration: This routine may update *dps->prev_type
if it must overwrite that type with the composite type (prev_type then becomes
a copy of the previous type).
*/
{
  a_type_ptr        rout_type = routine_ptr->type;
  a_type_ptr        comp_type;
  a_param_type_ptr  rout_type_ptp, comp_type_ptp, next_rout_type_ptp;
  a_routine_type_supplement_ptr
                    rtsp, comp_rtsp;
  a_boolean         preserve_qualifiers_from_rout_type;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_calling_convention
                    orig_calling_convention =
                         skip_typerefs(rout_type)->variant.routine.extra_info->
                                                            calling_convention;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(4, "reconcile_routine_types");
  if (rout_type != type_ptr) {
    /* We only try to reconcile routine types that have already been
       determined to be compatible. */
    check_assertion_str(routine_types_are_redecl_compatible(
                                      type_ptr, rout_type,
                                      TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING),
                        "reconcile_routine_types: types are not compatible");
    /* We cannot be required to preserve the types from both sources. */
    check_assertion_str(!preserve_rout_type || !preserve_type_ptr,
                        "reconcile_routine_types: can't preserve both types");
    if (!C_mode() && dps->routine_fixup == NULL) {
      /* If there are default arguments associated with the parameters, check
         them at this time, unless we have recorded a fixup entry to deal with
         default arguments at the end of the declaration.  They will be merged
         in composite_type. */
      check_default_arg_compatibility(type_ptr, rout_type,
                                      routine_ptr->template_arg_list != NULL);
    }  /* if */
    /* The type of the routine should be the composite of the two types. */
    if (!preserve_rout_type && !preserve_type_ptr) {
      /* Simple case -- no required result type location.  Normally, we prefer
         the original declaration, but if that was compiler-generated, we
         prefer the later one (presumably declared in the source). */
      if (routine_ptr->compiler_generated) {
        routine_ptr->type = composite_type(type_ptr, rout_type);
      } else {
        routine_ptr->type = composite_type(rout_type, type_ptr);
      }  /* if */
    } else {
      /* Some requirement on where the result ends up.  Favor the type we'd
         like by passing it first to composite_type. */
      if (preserve_rout_type) {
        /* rout_type must be preserved. */
        comp_type = composite_type(rout_type, type_ptr);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (!same_entities(comp_type, rout_type) &&
            same_entities(routine_ptr->declared_type, rout_type)) {
          /* The declared type, which was saved when the routine was defined,
             points to a type entry that is going to be modified, so change
             it to point to a copy. */
          routine_ptr->declared_type =
                copy_routine_type_with_param_types(rout_type,
                                                   /*copy_default_args=*/TRUE);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      } else {
        /* type_ptr must be preserved. */
        comp_type = composite_type(type_ptr, rout_type);
        /* If there are pending default arguments, transfer any prior default
           arguments to the new type, to ensure that duplicate default
           arguments are correctly diagnosed.  E.g.:
               void f(int = 3);
               void f(int = 3) {}
           Here, the new type is the one recorded, but its default argument
           expressions will be scanned later (via a routine fixup).  So at this
           point the param type entry returned by composite_type has the
           has_default_arg flag set to TRUE, but the default_arg_expr field
           set to NULL. */
        if (dps->routine_fixup != NULL) {
          a_param_type_ptr  from_ptp, to_ptp;
          from_ptp = function_type_params(skip_typerefs(rout_type));
          to_ptp = function_type_params(skip_typerefs(comp_type));
          while (from_ptp != NULL && to_ptp != NULL) {
            if (from_ptp->has_default_arg && to_ptp->has_default_arg &&
                from_ptp->default_arg_expr != NULL &&
                to_ptp->default_arg_expr == NULL) {
              to_ptp->default_arg_expr = duplicate_default_arg_expr(
                                              from_ptp->default_arg_expr);
            }  /* if */
            from_ptp = from_ptp->next;
            to_ptp = to_ptp->next;
          }  /* while */
        }  /* if */
        routine_ptr->type = rout_type = type_ptr;
      }  /* if */
      /* If rout_type is not what was returned, copy the composite
         type on top of the existing rout_type. */
      if (!same_entities(comp_type, rout_type)) {
        comp_type = skip_typerefs(comp_type);
        comp_rtsp = comp_type->variant.routine.extra_info;
        rout_type = skip_typerefs(rout_type);
        rtsp = rout_type->variant.routine.extra_info;
        /* Before overriding rout_type, preserve a copy of the original. */
        dps->prev_type = copy_routine_type_with_param_types(
                                       rout_type, /*copy_default_args=*/FALSE);
        /* Transfer the composite type to rout_type, which is usually
           unshared.  We want to preserve fields like assoc_routine and
           arg_pragma in rout_type, so we can't just do a copy_type. */
        rout_type->variant.routine.return_type =
                            comp_type->variant.routine.return_type;
#if GNU_EXTENSIONS_ALLOWED
        (void)copy_gnu_type_properties(rout_type, comp_type);
#endif /* GNU_EXTENSIONS_ALLOWED */
        rtsp->prototyped = comp_rtsp->prototyped;
        rtsp->has_ellipsis = comp_rtsp->has_ellipsis;
        preserve_qualifiers_from_rout_type = FALSE;
        if (rtsp->param_type_list == NULL) {
          /* The entire list may just be transferred over. */
          rtsp->param_type_list = comp_rtsp->param_type_list;
        } else if (rtsp->param_type_list == comp_rtsp->param_type_list) {
          /* This is a fairly rare case in C mode: the routine was declared
             twice without prototype, but the latter declaration is a
             definition.  In that case, composite_routine_type will have
             shared the param_type_list between the composed types.
             Nothing needs to be done. */
          check_assertion_str2(C_mode(), "reconcile_routine_types:",
                                         "shared param types unexpected");
        } else if (comp_rtsp->param_type_list != NULL) {
          /* Copy the param type entries from the composite type onto the
             param type entries for the routine type.  This is done in case
             new param type entries were created.  The original ones must be
             preserved, however, since they may be pointed to by the parameter
             variables with which they are associated. */
          rout_type_ptp = rtsp->param_type_list;
          comp_type_ptp = comp_rtsp->param_type_list;
          if (remove_qualifiers_from_param_types) {
            /* Usually, the top-level param-type qualifiers recorded for the
               function (either as currently declared or as previously
               declared when the function was defined) should be preserved.
               Since composite_type cannot be presumed to have gotten it
               right, the qualifiers will have to be copied into the new
               type by hand. */
            if (!preserve_rout_type ||
                routine_ptr->assoc_scope != NULL_region_number) {
              preserve_qualifiers_from_rout_type = TRUE;
            }  /* if */
          }  /* if */
          for (; rout_type_ptp != NULL; rout_type_ptp = next_rout_type_ptp,
                                        comp_type_ptp = comp_type_ptp->next) {
            a_type_qualifier_set  saved_qualifiers = rout_type_ptp->qualifiers;
            a_type_ptr            declared_type = rout_type_ptp->declared_type;
            a_const_char          *saved_name = rout_type_ptp->name;
            uint32_t              saved_param_num = rout_type_ptp->param_num;
#if EXTRA_SOURCE_POSITIONS_IN_IL
            a_decl_position_supplement_ptr saved_decl_pos_info =
                                                rout_type_ptp->decl_pos_info;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
            check_assertion_str2(rout_type_ptp != comp_type_ptp,
                                 "reconcile_routine_types:",
                                 "param type appears on two lists");
            /* Save the original next pointer and restore it after the copy. */
            next_rout_type_ptp = rout_type_ptp->next;
            /* First copy the param type entry, and then update the fields
               that need to be restored. */
            *rout_type_ptp = *comp_type_ptp;
            /* Restore the next pointer. */
            rout_type_ptp->next = next_rout_type_ptp;
            /* Restore the declared type. */
            rout_type_ptp->declared_type = declared_type;
            if (preserve_qualifiers_from_rout_type) {
              /* Restore the qualifiers as originally declared. */
              rout_type_ptp->qualifiers = saved_qualifiers;
            }  /* if */
            /* Restore the name that is associated with the routine type. */
            rout_type_ptp->name = saved_name;
            /* Restore the parameter's ordinal number. */
            rout_type_ptp->param_num = saved_param_num;
#if EXTRA_SOURCE_POSITIONS_IN_IL
            /* Restore the source-range information that was recorded for
               the routine type. */
            rout_type_ptp->decl_pos_info = saved_decl_pos_info;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
            /* Copy the Microsoft attributes list (if any). */
            if (comp_type_ptp->ms_attributes != NULL) {
              rout_type_ptp->ms_attributes = duplicate_ms_attributes(
                                                  comp_type_ptp->ms_attributes,
                                                  (char*)rout_type_ptp);
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          }  /* for */
        }  /* if */
        if (!C_mode()) {
          if (exceptions_enabled) {
            /* Preserve the exception specification -- the pointer will have
               been copied into comp_type by composite_type.  (Note that we
               just copy the pointer, so that two routine types may end up
               pointing to the same exception specification entry.  This
               should be okay.) */
            rtsp->exception_specification = comp_rtsp->exception_specification;
          }  /* if */
          rtsp->routine_name_linkage = comp_rtsp->routine_name_linkage;
          rtsp->routine_name_linkage_is_explicit =
                           comp_rtsp->routine_name_linkage_is_explicit;
        }  /* if */          
        /* has_ellipsis need not be copied -- it will be the same in all of
           the types, since the original two types are compatible. */
        /* Likewise, the this_class pointers should be identical -- this will
           have been verified in types_are_compatible. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (routine_ptr->declared_type != NULL) {
          /* composite_routine_type may have created a type that shares its
             default argument expressions with the original routine types.
             One of those original types may still be part of the IL through
             the "declared_type" field of the routine entry: If so, ensure
             that the expression nodes are unique. */
          disentangle_default_args(rout_type, routine_ptr->declared_type);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The Microsoft Visual C++ compiler always uses the calling convention
       from the declaration of a member function, even if the calling
       convention on the definition is different. */
    if (microsoft_mode &&
        routine_ptr->source_corresp.is_class_member) {
      skip_typerefs(routine_ptr->type)->variant.routine.extra_info->
                                  calling_convention = orig_calling_convention;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  db_exit();
}  /* reconcile_routine_types */


static void mark_symbol_to_suppress_warnings(a_symbol_ptr  sym)
/*
The given symbol is involved in an error in some way or another.  Set flags
as appropriate to suppress warnings (e.g., in end_of_scope_symbol_check).
*/
{
  /* Suppress declared-but-not-referenced warnings. */
  sym->referenced = TRUE;
  if (sym->kind == (a_symbol_kind)sk_variable) {
    /* Suppress set-but-not-used warnings. */
    sym->variant.variable.used = TRUE;
  }  /* if */
}  /* mark_symbol_to_suppress_warnings */


static a_boolean routine_name_linkages_are_compatible(a_type_ptr  rout_type,
                                                      a_type_ptr  type_ptr)
/*
rout_type is a pointer to the type of a previous declaration of a given
routine, and type_ptr is a pointer to the current type.  Return TRUE if
the routine-name-linkages of the two declarations are compatible.
*/
{
  a_boolean                      compat = TRUE;
  a_routine_type_supplement_ptr  rtsp;

  type_ptr = skip_typerefs(type_ptr);
  rtsp = type_ptr->variant.routine.extra_info;
  if (rtsp->routine_name_linkage_is_explicit) {
    rout_type = skip_typerefs(rout_type);
    compat = routine_linkages_are_compatible(
                  rtsp->routine_name_linkage,
                  rout_type->variant.routine.extra_info->routine_name_linkage,
                  /*is_impl_conv=*/FALSE);
  }  /* if */
  return compat;
}  /* routine_name_linkages_are_compatible */


void check_constituent_types_have_linkage(a_symbol_ptr      sym,
                                          a_source_position *error_pos,
					  a_boolean         is_declaration)
/*
Check whether an entity with linkage was declared using types without linkage.
Before C++11 such declarations were not allowed by the language (but were
accepted in some cases).  In C++11 the rules were relaxed to allow such
declarations provided the entity is either unused or is defined in the
translation unit.

The global variable decls_using_types_without_linkage_allowed is used to
specify which behavior is to be checked.  When it is TRUE (C++11 behavior),
the caller is responsible for calling this routine only for referenced entities
that have not been defined.

sym is a pointer to a routine or variable symbol whose type is to be verified.
error_pos determines where any error should be reported.

This routine is always called when an entity is declared (is_declaration is
TRUE) and when decls_using_types_without_linkage_allowed is TRUE may be
called again at the end of the translation unit (is_declaration is FALSE).
When appropriate, this routine sets the declared_using_type_without_linkage
flag when is_declaration is TRUE.
*/
{
  a_boolean                    is_function;
  a_type_ptr                   type;
  an_error_severity            severity;
  an_error_code                err_code;
  a_source_correspondence_ptr  scp;
  a_boolean                    uses_local_type = FALSE;
  a_boolean                    uses_type_without_linkage = FALSE;
  a_routine_ptr	               rp = NULL;
  a_variable_ptr	       vp = NULL;
  a_boolean                    type_without_linkage_flag_set;

  is_function = sym->kind == (a_symbol_kind)sk_routine ||
                sym->kind == (a_symbol_kind)sk_member_function;
  if (is_function) {
    rp = sym->variant.routine.ptr;
    type = rp->type;
    scp = &rp->source_corresp;
    type_without_linkage_flag_set = rp->declared_using_type_without_linkage;
  } else {
    if (sym->kind == (a_symbol_kind)sk_variable) {
      vp = sym->variant.variable.ptr;
    } else {
      check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
      vp = sym->variant.static_data_member.variable;
    }  /* if */
    type = vp->type;
    scp = &vp->source_corresp;
    type_without_linkage_flag_set = vp->declared_using_type_without_linkage;
  }  /* if */
  if (is_function && sym->variant.routine.ptr->compiler_generated) {
    /* Compiler-generated member functions can involve types with no name
       linkage in some error recovery modes (and in Microsoft mode).  A
       diagnostic is not helpful for such functions. */
  } else if (scp->name_linkage == (a_name_linkage_kind)nlk_internal ||
             scp->name_linkage == (a_name_linkage_kind)nlk_external) {
    /* Static entities and entities with "C" linkage are allowed to use
       types without linkage. */
  } else if (is_prototype_instantiation_context()) {
    /* Suppress this check in prototype instantiations. */
  } else if (is_declaration || type_without_linkage_flag_set) {
    /* We are either in a declaration or this is the second call for an
       entity for which a diagnostic is required.  In the first case,
       set the flag in the entity indicating whether it was declared using
       an entity without linkage.  In the second case, call the local/unnamed
       type routines to determine which diagnostic to issue below. */
    uses_local_type = is_or_contains_local_type(type);
    uses_type_without_linkage = is_or_contains_type_with_no_name_linkage(type);
    if (is_declaration) {
      if (uses_local_type || uses_type_without_linkage) {
        if (is_function) {
          rp->declared_using_type_without_linkage = TRUE;
        } else { 
          vp->declared_using_type_without_linkage = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_declaration && decls_using_types_without_linkage_allowed) {
    /* No further processing is done at declaration time in this mode. */
  } else if (uses_local_type) {
    /* A declaration that involves a local type. */
    if (is_variably_modified_type(type)) {
      /* Variably modified types are "local" in a sense, but their use in
         declarations with linkage is already diagnosed elsewhere. */
      expect_error();
    } else if (decls_using_types_without_linkage_allowed) {
      /* C++11 behavior: An error is issued because programs that get this
         diagnostic would fail at link time.  (Since Microsoft compilers
         accept such cases, only a warning is issued in Microsoft mode.) */
      if (microsoft_mode) {
        pos_sy_warning(ec_undefined_decl_using_local_type, error_pos, sym);
      } else {
        pos_sy_diagnostic(es_discretionary_error,
                          ec_decl_with_local_type_but_not_defined,
                          error_pos, sym);
      }  /* if */
    } else {
      /* Pre-C++11 behavior: Issue an error (except in cfront, Microsoft, and
         GNU C++ compatibility modes). */
      if (any_cfront_mode() ||
          (microsoft_mode && (is_function || microsoft_version < 1200)) ||
          (gpp_mode && !is_function)) {
        severity = es_warning;
      } else {
        severity = es_error;
      }  /* if */
      err_code = is_function ? ec_local_type_in_function
                             : ec_local_type_in_nonlocal_var;
      pos_diagnostic(severity, err_code, error_pos);
    }  /* if */
  } else if (uses_type_without_linkage) {
    /* Use of a type that does not have linkage.
       E.g., typedef enum { e1 } *pE; void f(pE); */
    if (decls_using_types_without_linkage_allowed) {
      /* C++11 behavior: An error is issued because programs that get this
         diagnostic would fail at link time.  (Since Microsoft compilers
         accept such cases, only a warning is issued in Microsoft mode.) */
      if (microsoft_mode) {
        pos_sy_warning(ec_undefined_decl_using_no_linkage_type, error_pos,
                       sym);
      } else {
        pos_sy_diagnostic(es_discretionary_error,
                          ec_decl_with_no_linkage_type_but_not_defined,
                          error_pos, sym);
      }  /* if */
    } else {
      /* Pre-C++11 behavior: In strict mode, we issue a discretionary error.
         In other modes, we issue a warning for functions and a remark for
         variables (the variable case is not all that uncommon and few other
         compilers diagnose it at all). */
      if (strict_ansi_mode) {
        severity = strict_ansi_discretionary_severity;
      } else if (is_function) {
        severity = es_warning;
      } else {
        severity = es_remark;
      }  /* if */
      err_code = is_function ? ec_type_with_no_linkage_in_function :
                               ec_type_with_no_linkage_in_var_with_linkage;
      pos_diagnostic(severity, err_code, error_pos);
    }  /* if */
  }  /* if */
}  /* check_constituent_types_have_linkage */


static void set_name_linkage(an_id_linkage_block     *idlbp,
                             a_symbol_ptr            sym,
                             a_source_correspondence *scp,
                             a_symbol_ptr            ext_sym,
                             a_source_position       *error_pos)
/*
Called from decl_variable and decl_routine, this function sets the name
linkage of the IL entry.  The indicated id linkage block indicates the
linkage (internal, external, none) that has been assigned.  sym is the symbol
for the variable or routine whose name linkage is to be set, and scp points
to the source correspondence of the associated IL entry.  ext_sym is the
associated sk_external_variable or sk_external_routine symbol, if any.
*error_pos is the source position of the identifier.
*/
{
  a_boolean                is_function =
                                   (sym->kind == (a_symbol_kind)sk_routine);
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  if (idlbp->linkage != idl_none) {
    if (scp->name_linkage == (a_name_linkage_kind)nlk_none) {
      scp->name_linkage = idlbp->name_linkage;
      sym->explicit_linkage_specifier = idlbp->name_linkage_is_explicit;
      if (ext_sym != NULL) {
        ext_sym->explicit_linkage_specifier = idlbp->name_linkage_is_explicit;
      }  /* if */
    } else {
      an_error_severity  sev = es_none;
      if (scp->name_linkage == idlbp->name_linkage) {
        /* The linkage kinds (C or C++) are the same. */
        if (idlbp->name_linkage_is_explicit) {
          /* The standard says that with the exception of functions with C++
             linkage, a function declaration without a linkage specification
             cannot precede a declaration of that function with an explicit
             linkage specification. */
          if (is_function &&
              scp->name_linkage !=
                                (a_name_linkage_kind)nlk_cplusplus_external &&
              !sym->explicit_linkage_specifier &&
              !(ext_sym != NULL && ext_sym->explicit_linkage_specifier)) {
            sev = es_error;
          }  /* if */
          /* Mark the symbols as having an explicit linkage specifier to
             keep this error from occurring again later. */
          sym->explicit_linkage_specifier = TRUE;
          if (ext_sym != NULL) ext_sym->explicit_linkage_specifier = TRUE;
        }  /* if */
      } else {
        /* Linkage is not the same, but it's no error as long as the current
           specification is implicit.   In non-strict modes, we only warn in
           the case of variables. */
        if (ssep->name_linkage_is_explicit &&
            !(gpp_mode && idlbp->is_friend_decl)) {
          if (is_function) {
            sev = es_error;
          } else if (strict_ansi_mode) {
            sev = strict_ansi_error_severity;
          } else {
            sev = es_warning;
          }  /* if */
        }  /* if */
        /* Reset the name linkage in certain cases: when the current linkage
           was explicitly specified whereas the previous one was not, or when
           one of the declarations specified internal linkage and the other
           didn't (in which case the later declaration is favored, except in
           Microsoft mode where the later name linkage is ignored).  For
           friend declarations in GNU C++ mode, a surrounding name linkage
           specification is ignored. */
        if ((idlbp->name_linkage_is_explicit && !microsoft_mode &&
            !(gpp_mode && idlbp->is_friend_decl) &&
             !sym->explicit_linkage_specifier) ||
            scp->name_linkage == (a_name_linkage_kind)nlk_internal ||
            idlbp->name_linkage == (a_name_linkage_kind)nlk_internal) {
          if (is_function &&
              scp->name_linkage == (a_name_linkage_kind)nlk_external &&
              decl_scope_level != DEPTH_OF_FILE_SCOPE) {
            /* Since this is an extern "C" function, its a_routine entry is
               on the file scope's list.  Changing the name linkage to
               something else requires us to move the entry to the appropriate
               scope: Otherwise, schedule_move_to_current_end_of_routines_list
               will operate on the wrong routines list. */
            a_routine_ptr  rp = sym->variant.routine.ptr;
            remove_from_routines_list(rp, DEPTH_OF_FILE_SCOPE);
            /* Clear the parent_scope pointer so add_to_routines_list can
               update it for the effective scope. */
            rp->source_corresp.parent_scope = NULL;
            add_to_routines_list(rp, idlbp->effective_decl_level);
          }  /* if */
          if (microsoft_mode && !sym->defined && sev == es_error &&
              idlbp->name_linkage == (a_name_linkage_kind)nlk_internal) {
            /* Microsoft compilers silently accept a change to internal
               linkage. */
            sev = es_warning;
          }  /* if */
          scp->name_linkage = idlbp->name_linkage;
          sym->explicit_linkage_specifier = idlbp->name_linkage_is_explicit;
          if (ext_sym != NULL && idlbp->name_linkage_is_explicit) {
            ext_sym->explicit_linkage_specifier = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (sev != es_none) {
        pos_sy_diagnostic(sev, ec_incompatible_linkage_specifier, error_pos,
                          ext_sym == NULL ? idlbp->linked_symbol : ext_sym);
      }  /* if */
    }  /* if */
    if (!C_mode() &&
        scp->name_linkage != (a_name_linkage_kind)nlk_internal &&
        scp->name_linkage != (a_name_linkage_kind)nlk_external &&
        !scope_stack[depth_scope_stack].in_prototype_instantiation) {
      /* A variable or routine with external linkage should not be declared in
         terms of types with no linkage.  Entities with extern "C" linkage are
         exempt from this constraint. */
      check_constituent_types_have_linkage(sym, error_pos,
                                           /*is_declaration=*/TRUE);
    }  /* if */
  }  /* if */
}  /* set_name_linkage */


static void add_namespace_parent_pointer(a_symbol_ptr             sym,
                                         a_source_correspondence  *scp)
/*
If appropriate, set the namespace pointer in the symbol (unless this is a
block-extern declaration) and in the IL entry (unless this is an extern "C"
redeclaration).
*/
{
  a_namespace_ptr  ns_ptr;

  ns_ptr = scope_stack[depth_innermost_namespace_scope].il_scope->
                                                variant.assoc_namespace;
  check_assertion(ns_ptr != NULL);
  if (scope_stack[depth_scope_stack].default_name_linkage ==
                                        (a_name_linkage_kind)nlk_external) {
    /* This is an extern "C" context. */
    if (scp->assoc_info != (char *)sym && scp->parent_scope != NULL &&
        (scp->parent_scope->kind == (a_scope_kind)sck_file ||
         scp->parent_scope->kind == (a_scope_kind)sck_namespace)) {
      /* This entity was originally declared in another namespace scope and
         then redeclared in the current namespace -- e.g.,
           extern "C" void f();
           namespace N {
             extern "C" void f();    // same entity
           }
         Don't reset the parent pointer in such cases. */
      scp = NULL;
    }  /* if */
  }  /* if */
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    /* This is a block-extern declaration, so the symbol is not set. */
    sym = NULL;
  }  /* if */
  set_namespace_membership(sym, scp, ns_ptr);
}  /* add_namespace_parent_pointer */


static void qualified_name_redecl_sym(an_id_linkage_block  *idlbp)
/*
This routine is called from decl_variable, decl_routine, and
decl_function_template for either of two cases:
    (1) when idlbp->is_friend_decl is FALSE, a namespace-qualified identifier
        is being redeclared outside of the namespace of which it is a member
        -- this will be a definition in a well-formed program; and
    (2) when idlbp->is_friend_decl is TRUE, a namespace- or file-scope-
        qualified name is being declared in a friend declaration.
*idlbp describes the declaration being processed; it is updated by this
function.
*/
{
  a_symbol_ptr     linked_symbol;
  a_boolean        err = FALSE;
  a_storage_class  storage_class;
  a_symbol_locator *locator = idlbp->locator;
  a_namespace_ptr  nsp = qualifier_namespace_ptr(*locator);
  a_scope_depth    orig_effective_decl_level = NO_SCOPE_DEPTH;

  db_enter(3, "qualified_name_redecl_sym");
  if (!idlbp->is_definition && !idlbp->is_friend_decl && strict_ansi_mode) {
    /* Improper use of a qualified name in a declarator (WP 8.3).  This
       is permitted as an extension in nonstrict C++ modes. */
    sym_error(ec_bad_scope_for_redeclaration, locator->specific_symbol);
    err = TRUE;
  } else if (idlbp->is_definition && !locator->is_class_member &&
             !namespace_is_enclosed_by_scope(locator->specific_symbol,
                                             &scope_stack[idlbp->
                                                     effective_decl_level])) {
    /* This declaration appears within a namespace scope in which the name
       cannot be defined -- it is a member (directly or indirectly) of a
       namespace that is not enclosed by the current namespace scope
       (see WP 7.3.1.4). */
    sym_error(ec_bad_scope_for_definition, locator->specific_symbol);
    err = TRUE;
  } else {
    /* This is a valid location for such a declaration. */
    if (scope_stack[decl_scope_level].kind ==
                                 (a_scope_kind)sck_template_declaration &&
        is_function_type(idlbp->type)) {
      idlbp->is_function_template = TRUE;
    }  /* if */
    if (nsp != NULL) {
      if (idlbp->is_friend_decl) {
        /* Push a namespace-reactivation scope scope for friend declarations
           (the scope is "read-only" -- no injections allowed), and a
           namespace-extension scope otherwise. */
        push_namespace_reactivation_scope(nsp);
      } else {
        /* This is a definition of a namespace member appearing
           in a scope other than that of the namespace to which it belongs, so
           extend the original namespace scope. */
        push_namespace_extension_scope(nsp);
        orig_effective_decl_level = idlbp->effective_decl_level;
        idlbp->effective_decl_level = depth_scope_stack;
      }  /* if */
      idlbp->namespace_reactivated = TRUE;
    } else {
      /* Must be a file scope qualified name or a template-id. */
      check_assertion(locator->is_file_scope_qualified_name ||
                      locator->is_template_id);
    }  /* if */
    /* Look up the name. */
    find_linked_symbol(idlbp);
    linked_symbol = idlbp->linked_symbol;
    if (idlbp->from_inline_namespace) {
      /* If the linked symbol is a namespace projection for an inline namespace
         member, use the fundamental symbol. */
      linked_symbol = fundamental_symbol_of(linked_symbol);
    }  /* if */
    storage_class = idlbp->storage_class;
    if (linked_symbol != NULL &&
        linked_symbol->kind != (a_symbol_kind)sk_overloaded_function) {
      /* A linked symbol was found -- set the storage class and linkage
         appropriately. */
      switch (linked_symbol->kind) {
        case sk_routine:
          storage_class = linked_symbol->variant.routine.ptr->storage_class;
          break;
        case sk_function_template:
          storage_class = linked_symbol->variant.template_info->
                                      variant.function.routine->storage_class;
          break;
        case sk_variable:
          storage_class = linked_symbol->variant.variable.ptr->storage_class;
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      if (storage_class == (a_storage_class)sc_static) {
        idlbp->linkage = idl_internal;
        idlbp->storage_class = (a_storage_class)sc_static;
      } else {
        idlbp->linkage = idl_external;
        if (linked_symbol->kind == (a_symbol_kind)sk_routine &&
            idlbp->is_definition) {
          idlbp->storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
      if (!idlbp->is_friend_decl && !microsoft_mode &&
          linked_symbol->kind == (a_symbol_kind)sk_routine &&
          linked_symbol->variant.routine.instance_ptr != NULL) {
        /* This is an out-of-scope definition of a namespace template
           function.  Be sure there is a prior declaration -- i.e., that
           this was not just instantiated by a reference or as a result of
           this very declaration (i.e., in the call to find_linked_symbol):
             namespace N {
               template <class T> void f(T);
               void f(int);
             }
             void N::f(int) { ... }          // Okay
             void N::f(double) { ... }       // Error
        */
        if (guiding_decls_allowed) {
          if (!linked_symbol->variant.routine.instance_ptr->is_guiding_decl) {
            pos_sy_error(ec_no_prior_declaration, &locator->source_position,
                         linked_symbol);
            err = TRUE;
          }  /* if */
        } else {
          /* When guiding declarations are not allowed, an out-of-scope
             definition is an old-style specialization.  Check whether
             such specializations are permitted. */
          check_old_specialization_allowed(linked_symbol,
                                           &locator->source_position);
        }  /* if */
      }  /* if */
    } else {
      /* The lookup failed.  Issue the right error. */
      a_symbol_ptr  sym = locator->specific_symbol;
      err = TRUE;
      if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* The qualified name referred to an overloaded function.  We want
           to issue the appropriate error after disregarding namespace
           projection symbols, which indicate names pulled into the scope
           with a using declaration or a using directive. */
        a_symbol_ptr  overload_sym = sym, temp;
        sym = NULL;
        for (temp = overload_sym->variant.overloaded_function.symbols;
             temp != NULL;
             temp = temp->next) {
          if (temp->kind == (a_symbol_kind)sk_routine ||
              temp->kind == (a_symbol_kind)sk_function_template) {
            if (sym == NULL) {
              /* This is the first symbol in the overload set that is not
                 a projection symbol. */
              sym = temp;
            } else {
              /* This is the second -- it's okay to issue a diagnostic for
                 an overloaded function. */
              sym = overload_sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
      if (sym == NULL || sym->kind == (a_symbol_kind)sk_namespace_projection) {
        /* There's no entry directly declared in the specified scope. */
        if (sym != NULL && sym->ambiguous) {
          /* The lookup was ambiguous. */
          pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
        } else if (sym != NULL && is_symbol_from_inline_namespace(sym)) {
          /* A symbol found via an inline namespace -- this is okay. */
        } else if (nsp != NULL) {
          /* Namespace scope. */
          pos_stsy_error(ec_not_an_actual_member, &locator->source_position,
                         locator->symbol_header->identifier,
                         (a_symbol_ptr)nsp->source_corresp.assoc_info);
        } else {
          /* File scope. */
          pos_st_error(ec_name_not_found_in_file_scope,
                       &locator->source_position,
                       locator->symbol_header->identifier);
        }  /* if */
      } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* Overloaded function case. */
        pos_sy_error(ec_no_match_for_type_of_overloaded_function,
                     &locator->source_position, sym);
      } else {
        /* Everything else. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
      }  /* if */
    }  /* if */
    if (err && nsp != NULL) {
      if (idlbp->is_friend_decl) {
        pop_namespace_reactivation_scope();
      } else {
        pop_namespace_extension_scope();
        idlbp->effective_decl_level = orig_effective_decl_level;
      }  /* if */
      idlbp->namespace_reactivated = FALSE;
    }  /* if */
  }  /* if */
  if (err) {
    /* Set safe values when an error has been reported. */
    set_to_named_error_locator(*idlbp->locator);
    idlbp->linked_symbol = NULL;
    idlbp->linkage = idl_none;
    idlbp->overload_symbol = NULL;
    idlbp->homonym_symbol = NULL;
  } else if (idlbp->linkage != idl_none) {
    /* Based on the linkage that has been determined, figure out what the
       "name linkage" should be. */
    compute_name_linkage(idlbp);
  }  /* if */
  db_exit();
}  /* qualified_name_redecl_sym */


static void move_variable_to_end_of_list(a_variable_ptr  var,
                                         a_symbol_ptr    linked_decl,
                                         a_boolean       from_inline_namespace)
/*
The given variable is being redeclared (and defined).  Move the corresponding
IL entry to the end of the associated scope list.  linked_decl is a symbol
representing the previous declaration; in Sun and Microsoft modes it could
be a using-declaration.  from_inline_namespace is TRUE if linked_decl
is a namespace projection symbol made visible by an inline namespace.
*/
{
  a_scope_depth  depth;
  a_boolean      namespace_scope_needed = FALSE;

  if (var->source_corresp.name_linkage == (a_name_linkage_kind)nlk_external) {
    /* Variables with C linkage are always placed on the file scope list. */
    depth = DEPTH_OF_FILE_SCOPE;
  } else {
    a_symbol_ptr     fund_linked_decl;
    fund_linked_decl = linked_decl == NULL
                                   ? NULL : fundamental_symbol_of(linked_decl);
    if (from_inline_namespace) {
      /* The scope for the inline namespace must be pushed. */
      namespace_scope_needed = TRUE;
    } else if ((sun_mode || microsoft_mode) &&
               depth_innermost_function_scope == NO_SCOPE_DEPTH &&
               linked_decl != NULL &&
               linked_decl->kind == (a_symbol_kind)sk_namespace_projection) {
      /* In Sun and Microsoft modes it is possible to redeclare a variable
         outside its namespace when that variable is visible through a
         using-declaration.  In that case, we must push that namespace
         scope so that the associated variable can be found by
         remove_from_variables_list and add_to_variables_list. */
      if (sym_is_namespace_member(fund_linked_decl)) {
        namespace_scope_needed = TRUE;
      }  /* if */
    }  /* if */
    if (namespace_scope_needed) {
      f_push_namespace_extension_scope(
                                sym_parent_namespace(fund_linked_decl), TRUE);
    }  /* if */
    depth = depth_innermost_namespace_scope;
  }  /* if */
  check_assertion(in_file_scope(var));
  remove_from_variables_list(var, depth);
  add_to_variables_list(var, depth);
  if (namespace_scope_needed) {
    pop_namespace_extension_scope();
  }  /* if */
}  /* move_variable_to_end_of_list */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* The parameters are only used when Microsoft extensions are
                  allowed. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static a_boolean microsoft_for_init_hiding(a_symbol_locator  *loc,
                                           a_scope_depth     decl_level,
                                           a_boolean         *in_for_init)
/*
Starting with version 7, Microsoft Visual C++ allows for-initializers to
declare variables that conflict with a declaration in the surrounding scope.
The earlier declaration is hidden by the new one.  The given locator is for
a new variable declaration in the current scope.  Return TRUE if we must
emulate the Microsoft behavior for that declaration.  decl_level is the
scope level in which for-init variables will be placed.  *in_for_init is set
to TRUE if we are in Microsoft mode and in a for-init block.
*/
{
  a_boolean  hiding = FALSE;

  check_assertion(microsoft_mode);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (!C_mode() &&
      struct_stmt_stack != NULL && depth_stmt_stack >= 0 &&
      struct_stmt_stack_top().for_init) {
    *in_for_init = TRUE;
    if (microsoft_version >= 1300 &&
        (use_nonstandard_for_init_scope ||
         (decl_level != depth_scope_stack &&
          microsoft_type_dependent_for_init_scope))) {
      a_scope_depth  saved_decl_scope_level = decl_scope_level;
      a_symbol_ptr   prev_decl;
      /* Look for an existing variable in the scope in which the for-init
         variable will be placed. */
      decl_scope_level = decl_level;
      prev_decl = curr_scope_id_lookup(loc, IDL_NO_OPTIONS);
      decl_scope_level = saved_decl_scope_level;
      if (prev_decl != NULL &&
          prev_decl->decl_scope == scope_stack[decl_level].number) {
        if (!(prev_decl->kind == (a_symbol_kind)sk_variable &&
              prev_decl->variant.variable.declared_in_for_init)) {
          /* Do not issue a warning if the hidden variable is a for-init
             declaration since that is common and unsurprising practice. */
          pos_start_diagnostic(es_warning, ec_for_init_hides_declaration,
                               &loc->source_position);
          add_diag_info_with_pos_insert(ec_for_init_hidden_declaration,
                                        &prev_decl->decl_position);
          end_error();
        }  /* if */
        hiding = TRUE;
      }  /*if */
    }  /*if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return hiding;
}  /* microsoft_for_init_hiding */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void adjust_calling_convention_if_entry_point(
                                           a_symbol_locator       *locator,
                                           a_func_info_block_ptr  func_info,
                                           a_type_ptr             type)
/*
A function is being declared with a name represented by locator, with the
given type, and with additional properties pointed to by func_info.  If this
function is the standard "main" entry point, a cc_default calling convention
is replaced by cc_cdecl.  If its name is "WinMain" or "wWinMain", a cc_default
calling convention is replaced by cc_stdcall (in this case the parent
namespace and type are ignored, so that the modified function may not in
fact be a Microsoft Windows entry point).
*/
{
  /* If the function was declared with a typedef, do not change its calling
     convention since that would change the typedef's type.  (I.e., do not
     apply skip_typerefs to type.) */
  if (type->kind == (a_type_kind)tk_routine) {
    a_routine_type_supplement_ptr  rtsp = type->variant.routine.extra_info;
    if (rtsp->calling_convention == (a_calling_convention)cc_default) {
      if (func_info->is_main_function) {
        /* main() is a __cdecl function by default. */
        rtsp->calling_convention = (a_calling_convention)cc_cdecl;
      } else if (locator->symbol_header != NULL) {
        a_const_char *name = locator->symbol_header->identifier;
        if (name[0] == 'w') {
          /* Treat "wWinMain" as "WinMain". */
          ++name;
        }  /* if */
        if (strcmp(name, "WinMain") == 0) {
          /* WinMain() is a __stdcall function by default. */
          rtsp->calling_convention = (a_calling_convention)cc_stdcall;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_calling_convention_if_entry_point */


static void check_and_adjust_calling_convention(
                                           a_symbol_locator       *locator,
                                           a_func_info_block_ptr  func_info,
                                           a_type_ptr             type)
/*
Check that the function declaration being processed (which is not a class
member) has an acceptable calling convention, and if it is an entry point
function (like "main()") adjust its default calling convention.
*/
{
  adjust_calling_convention_if_entry_point(locator, func_info, type);
  if (skip_typerefs(type)->variant.routine.extra_info->calling_convention ==
                                           (a_calling_convention)cc_thiscall) {
    /* Nonmember functions cannot have the __thiscall calling convention. */
    pos_error(ec_thiscall_requires_nonstatic_member,
              &locator->source_position);
  }  /* if */
}  /* check_and_adjust_calling_convention */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if UPC_EXTENSIONS_ALLOWED

static void check_upc_variable_decl(a_symbol_locator  *locator,
                                    a_type_ptr        type_ptr,
                                    a_storage_class   storage_class)
/*
A variable is being declared with the given type and storage class.  If the 
type relies on some UPC constructs, verify that all UPC constraints are met.
If an error occurs, the given locator may be changed to an error locator.
*/
{
  if (upc_mode) {
    if (!is_static_or_thread_storage_duration_storage_class(storage_class) &&
        is_underlying_shared_qualified_type(type_ptr)) {
      /* Only variables with static storage duration can be UPC shared. */
      pos_error(ec_bad_shared_storage_class, &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
    if (get_underlying_upc_block_size(type_ptr) == UPC_BLOCK_SIZE_INDEFINITE &&
        is_underlying_threads_dimensioned_array_type(type_ptr)) {
      /* A threads-dimensioned array cannot have an indefinite block size. */
      pos_error(ec_threads_dimension_requires_definite_block_size,
                &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
}  /* check_upc_variable_decl */

#endif /* UPC_EXTENSIONS_ALLOWED */

#if NAMED_ADDRESS_SPACES_ALLOWED

a_boolean curr_id_is_named_address_space(void)
/*
The current token is an identifier.  Return TRUE if and only if it stands for
a named address space.
*/
{
  a_boolean  result = FALSE;

  check_assertion(curr_token == tok_identifier);
  if (named_address_spaces_enabled) {
    /* Use IDL_TENTATIVE_TYPE_LOOKUP to avoid warnings generated for the
       the "out-of-scope lookup" feature (carried over from SVR4 C to other
       nonstrict C modes). */
    a_symbol_ptr  sym = normal_id_lookup(&locator_for_curr_id,
                                         IDL_TENTATIVE_TYPE_LOOKUP);
    if (sym != NULL && sym->kind == (a_symbol_kind)sk_named_address_space) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* curr_id_is_named_address_space */


static void check_named_address_space_constraints(
                                              a_symbol_locator  *locator,
                                              a_type_ptr        type_ptr,
                                              a_storage_class   storage_class)
/*
A variable is being declared with the given type and storage class.  If the 
type is qualified with a named address space, verify that it has static storage
duration.  If an error occurs, the given locator is changed to an error
locator.
*/
{
  if (named_address_spaces_enabled) {
    if (!is_static_or_thread_storage_duration_storage_class(storage_class) &&
        type_qualified_with_named_address_space(type_ptr)) {
      /* Only variables with static storage duration can have a named
         address space qualifier (at the top level). */
      pos_error(ec_bad_storage_class_for_named_address_space_variable,
                &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
}  /* check_named_address_space_constraints */

#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

static void check_for_vla_inside_statement_expression(a_source_position *pos)
/*
A VLA is being declared at the indicated position.  If we are inside
a GNU statement expression, i.e., ({ ... }), issue an error.  VLAs are
disallowed because they have cleanup issues -- the allocated storage
may have to be freed on exit from the statement expression.
*/
{
  if (inside_statement_expression()) {
    pos_error(ec_vla_in_statement_expr, pos);
  }  /* if */
}  /* check_for_vla_inside_statement_expression */

#if GNU_EXTENSIONS_ALLOWED

static void record_asm_name_for_variable(
                                    a_variable_ptr         variable,
                                    a_const_char           *asm_name,
                                    a_boolean              is_register,
                                    a_source_position_ptr  diag_pos,
                                    a_boolean              previously_defined)
/*
Record the given asm name in the given variable entry.  is_register indicates
that the asm name should be treated as a register name.  If a problem is
detected, issue a diagnostic at the given position.  previously_defined is
TRUE if a definition preceded the current declaration.
*/
{
  a_named_register  anr = name_to_register(asm_name);

  if (is_register) {
    /* If the variable has been declared with the register keyword, then
       the assembly name indicates a particular register. */
    if (anr == (a_named_register)anr_invalid) {
      /* Unknown register name: Issue an error. */
      pos_st_error(ec_bad_reg_name, diag_pos, asm_name);
    } else {
      a_type_ptr  var_type = skip_typerefs(variable->type);
      if (!C_mode() && is_immediate_class_type(var_type) &&
          !symbol_supplement_for_class(var_type)->is_POD) {
        pos_error(ec_register_mapped_variable_must_be_POD, diag_pos);
      } else if (variable->asm_name_is_valid &&
                 variable->asm_name_or_reg.name == NULL) {
        /* This is the first "asm name" construct for this entity. */
        variable->asm_name_or_reg.reg = anr;
        variable->asm_name_is_valid = FALSE;
      } else if (variable->asm_name_is_valid ||
                 variable->asm_name_or_reg.reg != anr) {
        /* Ignore this construct with a warning since it conflicts with a
           previous declaration. */
        pos_warning(ec_asm_name_conflict, diag_pos);
      }  /* if */
    }  /* if */
  } else {
    /* Otherwise, the assembly name is just a name. */
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
    a_named_register  reg_not_found = (a_named_register)anr_unrecognized;
#else /* !ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
    a_named_register  reg_not_found = (a_named_register)anr_invalid;
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
    if (gnu_version >= 30000 && anr != reg_not_found) {
      pos_error(ec_register_name_on_nonregister, diag_pos);
    } else if (gcc_mode && previously_defined) {
      /* GNU C (but not GNU C++) ignores asm names appearing on declarations
         of previously defined variables.  Version 4.0.0 and later issue a
         a warning, but we issue the warning for all values of gnu_version.  */
      /* Don't warn if the construct is identical to one that applied to the
         definition. */
      if (variable->asm_name_or_reg.name == NULL ||
          strcmp(variable->asm_name_or_reg.name, asm_name) != 0) {
        pos_warning(ec_asm_name_after_definition, diag_pos);
      }  /* if */
    } else if (variable->asm_name_or_reg.name == NULL) {
      /* This is the first declaration of this variable with an "asm name"
         construct. */
      variable->asm_name_or_reg.name = asm_name;
      record_asm_name_for_lookup(symbol_for(variable));
    } else if (strcmp(variable->asm_name_or_reg.name, asm_name) != 0) {
      /* The current declaration has an "asm name" that is different from
         one specified on a previous declaration.  Issue a warning and
         ignore the specification on the current declaration. */
      pos_warning(ec_asm_name_conflict, diag_pos);
    }  /* if */
  }  /* if */
}  /* record_asm_name_for_variable */

#endif /* GNU_EXTENSIONS_ALLOWED */

static a_boolean check_variable_redecl_compatible(a_decl_parse_state  *dps)
/*
*dps represents a variable declaration with type dps->type, but previously
declared with type dps->prev_type.  Return TRUE if the two types are compatible
and set dps->type to the composite type if needed.  Otherwise, return FALSE and
emit an error.
*/
{
  a_boolean                redecl_okay = TRUE;
  a_type_compat_flags_set  tcf = TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |
                                 TCF_REDECLARATION;

  /* If necessary, check that throw-specifications match. */
  if (!C_mode() && ((is_ptr_or_ref_type(dps->type) &&
                     is_function_type(type_pointed_to(dps->type))) ||
                    (is_ptr_to_member_type(dps->type) &&
                     is_function_type(pm_member_type(dps->type))))) {
    check_exception_specification(dps->type, dps->sym, &dps->declarator_pos,
                                  /*is_redecl=*/TRUE);
  }  /* if */
  /* Check for type incompatibility.  In GNU mode, calling conventions are not
     checked at this time because the corresponding attributes have not been
     applied yet. */
  if (gnu_mode) tcf |= TCF_IGNORE_CALLING_CONVENTIONS;
  if (!f_types_are_compatible(dps->type, dps->prev_type, tcf)) {
    an_error_severity  severity = es_none;
    a_type_ptr         orig_type = skip_typerefs(dps->prev_type);
    a_type_ptr         redecl_type = skip_typerefs(dps->type);
    if (gcc_mode && gnu_version < 30000) {
      if (types_are_redecl_compatible(redecl_type, orig_type)) {
        /* Earlier versions of GNU C (but not GNU C++) accept redeclarations of
           variables that only differ in cv-qualification (with a warning). */
        severity = es_warning;
        dps->type = make_qualified_type(redecl_type,
                                        (get_type_qualifiers(dps->type) |
                                         get_type_qualifiers(dps->prev_type)));
        dps->prev_type =
                    make_qualified_type(orig_type,
                                        (get_type_qualifiers(dps->type) |
                                         get_type_qualifiers(dps->prev_type)));
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (C_mode() && microsoft_mode &&
        is_integral_type(redecl_type) && is_integral_type(orig_type) &&
        redecl_type->size == orig_type->size &&
        redecl_type->alignment == orig_type->alignment) {
      /* Just issue a warning in Microsoft C mode.  MSVC uses the first
         declaration, so adjust dps->type. */
      severity = es_warning;
      dps->type = dps->prev_type;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (severity == es_none) {
      severity = es_error;      
      redecl_okay = FALSE;
      if (dps->sym->defined) {
        /* If the previous declaration was a definition, proceed with the
           previous type since it may have to match up with an initializer
           in what follows. */
        dps->type = dps->prev_type;
      }  /* if */
    }  /* if */
    pos_sy_diagnostic(severity, ec_not_compatible_with_previous_decl,
                      &dps->declarator_pos, dps->sym);
  }  /* if */
  if (redecl_okay) {
    /* The type of the variable should be the composite of the two types. */
    dps->sym->variant.variable.ptr->type = dps->type =
                                    composite_type(dps->type, dps->prev_type);
  }  /* if */
  return redecl_okay;
}  /* check_variable_redecl_compatible */


static void check_sym_of_other_decl(a_source_correspondence  *scp,
                                    a_symbol_ptr             new_decl)
/*
This routine is called when processing the declaration of a variable or
function that was previously declared in another scope (this is technically
not a "redeclaration").  scp points to the source correspondence entry of the
declared entity; its assoc_info field points to the symbol for the earlier
declaration.  new_decl points to the symbol associated with the current
declaration.  Update scp->assoc_info to point to new_decl if needed.
*/
{
  a_symbol_ptr  other_decl = (a_symbol_ptr)scp->assoc_info;
  a_boolean     is_local, other_decl_is_block_extern;

  /* The entity was previously declared in a different scope. */
  check_assertion(other_decl != NULL);
  /* If it wasn't a namespace scope declaration, and it is no longer associated
     with a scope on the scope stack, other_decl must have been the result of a
     block-extern declaration. */
  other_decl_is_block_extern =
               !sym_is_namespace_member(other_decl) &&
               other_decl->decl_scope !=
                                    scope_stack[DEPTH_OF_FILE_SCOPE].number &&
               scope_depth_of_symbol(other_decl, &is_local) == NO_SCOPE_DEPTH;
  if (other_decl_is_block_extern) {
    if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
      /* The current declaration is not block-extern, but the previous
         declaration was block-extern.  For example:
           void f() { extern int x; }
           int x;
         Use the symbol for the current declaration in the IL entry instead. */
      a_boolean  saved_referenced_flag = scp->referenced;
      scp->assoc_info = NULL;
      set_source_corresp(scp, new_decl);
      scp->referenced = saved_referenced_flag;
      if (!C_mode() &&
          scp->name_linkage == (a_name_linkage_kind)nlk_external) {
        /* An extern "C" declaration: Set the parent scope to that associated
           with the current declaration.  For example:
             namespace N { extern "C" { void f() { extern int g(); } } }
             int g();  // Treat g as a member of the global namespace,
                       // instead of a member of N.
           Among other things, this avoids having the C++-generating back end
           try to refer to N::g. */
        if (sym_is_namespace_member(new_decl)) {
          set_namespace_membership((a_symbol_ptr)NULL, scp,
                                   sym_parent_namespace(new_decl));
        } else {
          scp->parent_scope = il_header.primary_scope;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_sym_of_other_decl */


void check_constant_valued_variable(a_decl_parse_state  *dps)
/*
If dps represents the declaration of a const variable initialized with a true
constant-expression, set the "constant_valued" flag in the IL entry for that
variable.
*/
{
  a_variable_ptr  vp = dps->sym != NULL ? var_for_symbol(dps->sym)
                                        : (a_variable_ptr)NULL;

  if (!C_mode() && vp != NULL && !dps->init_state.init_error &&
      is_potentially_constant_valued_variable(vp)) {
    a_constant ref_val;
    /* See if the variable is initialized with a constant. */
    a_constant_ptr con_val = initializer_constant(vp);
    if (con_val != NULL) {
      if (strict_ansi_mode && dps->init_state.constant_expr_ruled_out) {
        /* In strict mode, rule out some subtle cases that produce a constant
           but don't have the form of a "constant expression". */
      } else if (is_any_reference_type(vp->type) &&
                 !vp->is_constexpr &&
                 constant_value_at_address(
                                          con_val,
                                          (a_constexpr_evaluation_block *)NULL,
                                          &ref_val) == NULL) {
        /* A reference that is not constexpr is constant-valued only if the
           constant reference address points at a constant. */
      } else {
        vp->constant_valued = TRUE;
      }  /* if */
    }  /* if */
    if (vp->initializer_in_class) vp->is_member_constant = TRUE;
  }  /* if */
}  /* check_constant_valued_variable */


#if !EXTRA_SOURCE_POSITIONS_IN_IL && !NAMED_REGISTERS_ALLOWED && \
    !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/ /* decl_pos_block is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL && !NAMED_REGISTERS_ALLOWED && ... */
void decl_variable(a_symbol_locator             *locator,
                   a_decl_parse_state           *dps,
                   a_symbol_reference_kind      srk_flags,
                   an_id_linkage_kind           *linkage_ptr,
                   a_symbol_ptr                 *ext_sym,
                   a_decl_pos_block_ptr         decl_pos_block)
/*
Enter the declaration of an identifier for a variable.  *locator gives the
symbol locator (and thus its name and its declaration position).  *dps
describes various properties of the declaration (including its type and
storage class).
Create and enter a symbol entry, and return a pointer to it in dps->sym.
Also allocate any associated IL construct, and attach it to the symbol.  If
the identifier has linkage and there is an existing symbol or IL entry, it
will be re-used.  Return in *linkage_ptr the linkage of the identifier.
Return in *dps->prev_type any previously-known type for this identifier from a
linked identifier in the same scope, or NULL if there was no previously-known
type.  If the identifier has linkage, return in *ext_sym a pointer to the
external symbol entry; otherwise, set *ext_sym to NULL.  srk_flags contain
specific information about the kind of declaration (whether it's a definition,
a tentative definition (C only), and so forth); this information is passed on
for use in generating cross-reference output describing this declaration.
*/
{
  a_symbol_ptr             sym = NULL, linked_symbol;
  a_boolean                sym_must_be_entered = FALSE;
  a_boolean                inhibit_redecl_error = FALSE;
  a_type_ptr               type_ptr = dps->type;
  a_storage_class          storage_class = dps->storage_class;
  an_id_linkage_block      idlb;
  a_boolean                alloc_at_file_scope;
  a_boolean                redecl_error_already_issued = FALSE;
  a_boolean                linked_redecl_error = FALSE;
  a_boolean                redeclaration = FALSE;
  a_variable_ptr           variable_ptr = NULL;
  an_id_linkage_kind       linkage;
  a_source_correspondence  *source_corresp_ptr;
  a_scope_depth            effective_decl_level;
  a_scope_depth            saved_depth_innermost_namespace_scope;
  a_boolean                suppress_ext_sym_lookup = FALSE;
  a_boolean                is_variable_def = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr               declared_type;
  a_name_reference_ptr     name_ref = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_boolean                linked_to_previous_variable = FALSE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GNU_EXTENSIONS_ALLOWED
  a_boolean                is_register;
#endif /* GNU_EXTENSIONS_ALLOWED */

  db_enter(3, "decl_variable");
#if GNU_EXTENSIONS_ALLOWED
  is_register = storage_class == (a_storage_class)sc_register;
  /* Declaring a global variable with a mapping onto a specific register makes
     it effectively an "extern" declaration. */
  if (gnu_mode && is_register && dps->asm_name != NULL &&
      decl_scope_level == depth_innermost_namespace_scope) {
    storage_class = (a_storage_class)sc_extern;
    srk_flags &= ~SRK_DEFINITION;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  check_assertion(storage_class != (a_storage_class)sc_typedef);
  if (srk_flags & SRK_DEFINITION) {
    is_variable_def = TRUE;
    dps->is_definition = TRUE;
  }  /* if */
  if (locator->is_template_id && !is_error_locator(*locator)) {
    /* An explicit template argument list is not allowed.  It could have
       sneaked past prior checking (during declarator processing) in Microsoft
       compatibility mode. */
    pos_error(ec_explicit_template_args_not_allowed,
              &locator->source_position);
    set_to_error_locator(*locator);
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  check_upc_variable_decl(locator, type_ptr, storage_class);
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  check_named_address_space_constraints(locator, type_ptr, storage_class);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if ((dps->dso_flags & DSO_CONSTEXPR) != 0 &&
      !is_const_qualified_type(type_ptr)) {
    /* constexpr variables are implicitly const. */
    type_ptr = make_qualified_type(type_ptr, (a_type_qualifier_set)TQ_CONST);
    dps->type = type_ptr;
  }  /* if */
  clear_id_linkage_block(&idlb);
  idlb.locator = locator;
  idlb.type = type_ptr;
  idlb.is_definition = is_variable_def;
  idlb.storage_class = storage_class;
  idlb.direct_linkage_specifier = dps->decl_modifiers.direct_linkage_specifier;
  set_linkage_environment(&idlb, decl_scope_level);
  if (!C_mode() && locator->specific_symbol != NULL &&
      (qualifier_namespace_ptr(*locator) != NULL ||
       locator->is_file_scope_qualified_name)) {
    /* This identifier is a namespace-qualified name that was previously
       declared.  Be sure this is a valid scope in which to define it. */
    qualified_name_redecl_sym(&idlb);
  } else {
    /* Determine the linkage of this symbol. */
    id_linkage(&idlb, dps);
  }  /* if */
  locator = idlb.locator;
  storage_class = idlb.storage_class;
  linked_symbol = idlb.linked_symbol;
  if (idlb.from_inline_namespace) {
    /* If the linked symbol is a namespace projection for an inline namespace
       member, use the fundamental symbol. */
    linked_symbol = fundamental_symbol_of(linked_symbol);
  }  /* if */
  effective_decl_level = idlb.effective_decl_level;
  linkage = idlb.linkage;
  if (gpp_mode && idlb.is_block_extern_decl &&
      depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
    /* In GNU C++ mode, the "innermost namespace scope" considered for block-
       extern declarations should ignore namespace extension scopes that don't
       correspond to actual namespace extension declarations (instead, they
       are the result of "reactivating" the namespace).  E.g.:
          namespace N { struct S { void f(); }; }
          void N::S::f() {
            extern float g;  // ::g in g++ mode, N::g otherwise.
          }
    */
    saved_depth_innermost_namespace_scope = depth_innermost_namespace_scope;
    depth_innermost_namespace_scope =
                                    get_effective_depth_innermost_namespace();
  } else {
    saved_depth_innermost_namespace_scope = NO_SCOPE_DEPTH;
  }  /* if */
  /* alloc_at_file_scope will be TRUE if the IL variable entry must be
     allocated in the file scope memory region.  This is always true for
     variables with linkage, except for extern variables declared in prototype
     instantiations when such instantiations are not part of the IL. */
  alloc_at_file_scope = (linkage != idl_none &&
                         (!scope_stack_top().in_prototype_instantiation ||
                          prototype_instantiations_in_il));
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked. */
    redeclaration = TRUE;
    if (sun_mode || microsoft_mode || gpp_mode) {
      /* In Sun, Microsoft, and GNU modes, the linked symbol may be a namespace
         projection (i.e., a using-declaration). */
      linked_symbol = fundamental_symbol_of(linked_symbol);
    }  /* if */
    if (linked_symbol->kind == (a_symbol_kind)sk_variable) {
      a_variable_ptr  orig_var = linked_symbol->variant.variable.ptr;
      if (C_mode() || (microsoft_mode && (srk_flags & SRK_TENTATIVE_DEF))) {
        if (linked_symbol->defined &&
            orig_var->init_kind != (an_init_kind)initk_none) {
          /* The variable was initialized on a prior declaration, so this
             cannot be a definition or a tentative definition.  (The error
             will be reported by the caller if there is an initializer on
             this declaration, too.) */
          srk_flags &= ~(SRK_TENTATIVE_DEF | SRK_DEFINITION);
          check_assertion(srk_flags & SRK_DECLARATION);
          is_variable_def = FALSE;
        }  /* if */
      } else if (microsoft_mode && is_variable_def && linked_symbol->defined &&
                 decl_scope_level == depth_innermost_namespace_scope &&
                 dps->has_initializer &&
                 dps->declared_storage_class == (a_storage_class)sc_extern &&
                 !type_has_nontrivial_destructor(dps->type) &&
                 orig_var->init_kind == (an_init_kind)initk_none) {
        /* This non-local variable was already defined, but without an
           initializer.  In Microsoft mode, such a definition can be followed
           by another definition that includes an initializer and the "extern"
           storage class specifier.  Turn the previous definition into an
           ordinary declaration. */
        pos_sy_warning(ec_already_defined, &locator->source_position,
                       linked_symbol);
        orig_var->storage_class = (a_storage_class)sc_extern;
        linked_symbol->defined = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        eliminate_variable_definition_source_sequence_entry(orig_var);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
      if (linked_symbol->defined && is_variable_def && !C_mode()) {
        /* Variable has already been defined.  Issue an error here and
           suppress an error when the symbol is entered. */
        pos_sy_error(ec_already_defined, &locator->source_position,
                     linked_symbol);
        set_to_named_error_locator(*locator);
        redecl_error_already_issued = TRUE;
        linked_redecl_error = TRUE;
        /* Set a flag to suppress reuse of the existing external-variable
           symbol and of the variable already in use.  This is to avoid
           redundant errors in case both this and the previous definition
           involved initialization.  Also, to suppress a declared-but-not-used
           message, set the referenced flag in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        mark_symbol_to_suppress_warnings(linked_symbol);
      } else {
        /* Linked symbol and new symbol are both variables.  See if they
           are compatible. */
        sym = dps->sym = linked_symbol;
        variable_ptr = linked_symbol->variant.variable.ptr;
        check_assertion(variable_ptr != NULL);
        dps->prev_type = variable_ptr->type;
        /* Check that the type of the new declaration is compatible with that
           of the previous declaration(s), and create the composite type if
           necessary.  If the current declaration involves the "auto" type
           specifier, do not perform the check now -- it will be done later
           when the actual type is known. */
        if (!dps->auto_type_specifier_seen) {
          dps->type = type_ptr;
          if (!check_variable_redecl_compatible(dps)) {
            redecl_error_already_issued = TRUE;
            /* If this is a definition, ignore the prior declaration and
               proceed with the current type.  Otherwise, proceed with an
               error type. */
            if (is_variable_def) {
              linked_redecl_error = TRUE;
            } else {
              dps->type = error_type();
            }  /* if */
          }  /* if */
          type_ptr = dps->type;
        }  /* if */
      }  /* if */
    } else {
      /* The linked symbol is a routine, while the new one is a variable. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, linked_symbol);
      redecl_error_already_issued = TRUE;
      linked_redecl_error = TRUE;
    }  /* if */
  }  /* if */
  if (linked_redecl_error) {
    /* There is a linked symbol, but it is not compatible with the new
       declaration.  Force a new symbol and a new IL entry. */
    sym = NULL;
    linked_symbol = NULL;
    variable_ptr = NULL;
    dps->prev_type = NULL;
    redeclaration = FALSE;
    suppress_ext_sym_lookup = TRUE;
  }  /* if */
  if (sym == NULL) {
    a_boolean  in_microsoft_for_init = FALSE;
    inhibit_redecl_error = microsoft_mode &&
                           microsoft_for_init_hiding(locator,
                                                     effective_decl_level,
                                                     &in_microsoft_for_init);
    /* There is no (compatible) symbol, so create one now.  The symbol is
       not entered yet because it needs to be set to refer to the
       variable for the checking of hiding. */
    sym = make_symbol((a_symbol_kind)sk_variable, locator);
    sym_must_be_entered = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    sym->variant.variable.declared_in_for_init = in_microsoft_for_init;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if RECORD_HIDDEN_NAMES_IN_IL
    /* Block extern declarations have associated hidden name entries; so we
       must make sure there is an IL scope to attach those entries to. */
      if (scope_stack[effective_decl_level].kind == (a_scope_kind)sck_block &&
          scope_stack[effective_decl_level].il_scope == NULL &&
          linkage != idl_none) {
        (void)ensure_il_scope_exists(&scope_stack[effective_decl_level]);
      }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  }  /* if */
  *ext_sym = NULL;
  if (alloc_at_file_scope && !redeclaration &&
      !(scope_stack_top().in_prototype_instantiation &&
        is_template_dependent_type(type_ptr))) {
    /* The symbol has external or internal linkage.  Find or create an
       external symbol entry for the identifier name, to check that the
       current declaration is compatible with any previous and future
       declarations of the same name.  Note that while other declarations
       are required to be compatible with the present one (because all
       declarations of a name with linkage refer to the same object or
       function), no composite type is formed; the type of the IL entity
       is only what is known in the current scope.  The external symbol
       keeps track of the full composite type behind the scenes.
       If we do not already have an IL entry, and the external symbol entry
       points to one, get a pointer to it and use it. */
    *ext_sym = 
        create_external_symbol_for_linked_entity(locator, dps, type_ptr, &idlb,
                                                 redeclaration,
                                                 redecl_error_already_issued,
                                                 suppress_ext_sym_lookup,
                                                 &variable_ptr,
                                                 (a_routine_ptr*)NULL);
  }  /* if */
  /* The entity being declared is a variable. */
  if (variable_ptr == NULL) {
    /* There is no IL entry, so create one now.  If the variable has
       internal or external linkage, it is entered at the file scope. */
    a_scope_depth  scope_depth;
    if (!alloc_at_file_scope) {
      scope_depth = decl_scope_level;
      if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
        /* The variable will be allocated in file scope after all.
           (This should only occur in error situations.) */
        check_assertion(total_errors != 0);
        alloc_at_file_scope = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_mode && microsoft_version >= 1310 && !C_mode() &&
                 scope_stack[decl_scope_level].is_for_init_block) {
        /* A for-init variable may need to be placed in the surrounding scope
           to emulate MSVC++ 7.1 behavior.  effective_decl_level (which
           determines where the symbol table entry goes) is already updated.
           scope_depth (which determines where the IL entry goes) should
           be the same. */
        scope_depth = idlb.effective_decl_level;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    } else if (depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE ||
               (scope_stack[depth_scope_stack].default_name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
                storage_class != (a_storage_class)sc_static)) {
      scope_depth = DEPTH_OF_FILE_SCOPE;
    } else {
      scope_depth = depth_innermost_namespace_scope;
    }  /* if */
    variable_ptr = make_variable(type_ptr, storage_class, scope_depth);
    source_corresp_ptr = &variable_ptr->source_corresp;
    if (*ext_sym != NULL &&
        (*ext_sym)->variant.extern_symbol_descr->variant.variable != NULL) {
      /* A new variable entry has been created, yet the external symbol
         already refers to a different variable.  This can occur when there
         is an error, but it can also occur in SVR4 C mode -- for example:
           unsigned int i;
           void f(int i) { { extern int i; } }
         where the second declaration of i has an incompatible type, yet
         no error is issued. */
      if (!linked_redecl_error &&
          !is_error_type((*ext_sym)->variant.extern_symbol_descr->type)) {
        check_assertion_str2(SVR4_C_mode && !is_variable_def &&
                             (storage_class == (a_storage_class)sc_extern),
                             "decl_variable:",
                             "can't set superseded_external");
        variable_ptr->superseded_external = TRUE;
      }  /* if */
    } else {
      dps->first_decl = TRUE;
    }  /* if */
  } else {
    /* There is an existing IL entry that we are reusing. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    linked_to_previous_variable = TRUE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for internal linkage on the old but not the new, or
       vice-versa. */
    check_for_linkage_conflict(&variable_ptr->storage_class,
                               &linkage, &storage_class,
                               &locator->source_position,
                               /*suppress_diagnostic=*/linked_redecl_error);
    if (variable_ptr->is_thread_local !=
        ((dps->dso_flags & DSO_THREAD_LOCAL) == DSO_THREAD_LOCAL)) {
      /* If "thread_local" is specified on one declaration, it must be
         specified on all. */
      pos2_diagnostic(es_error,
                      variable_ptr->is_thread_local ?
                                     ec_non_thread_local_follows_thread_local :
                                     ec_thread_local_follows_non_thread_local,
                      &locator->source_position,
                      &variable_ptr->source_corresp.decl_position);
    }  /* if */
    if (linkage != idlb.linkage) {
      /* The linkage has been changed, so change the "name linkage", too. */
      idlb.linkage = linkage;
      compute_name_linkage(&idlb);
    }  /* if */
    /* Modify the storage class if necessary (an unspecified storage 
       class on the new declaration indicates a tentative definition --
       see 3.7.2).  Do not force anything but sc_unspecified on the
       preexisting variable entry -- we don't want to change the storage
       class in a case like this:  int i; extern int i; . */
    if (storage_class == (a_storage_class)sc_unspecified) {
      variable_ptr->storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
    /* If the IL entry was previously referenced, the symbol should be
       marked as referenced too.  We may have a case like this:
         void f() { extern int i; i = 0; }
         int i;
       The IL entity associated with i is referenced in the function scope
       but the symbol at file scope is created later -- it should have its
       "referenced" flag set to prevent unwanted "defined but not referenced"
       warnings from being put out. */
    if (storage_class != (a_storage_class)sc_extern &&
        variable_ptr->source_corresp.referenced) {
      sym->referenced = TRUE;
    }  /* if */
    /* Similarly, it should have its "used" flag set.  This is only needed
       for file-scope static variables, in cases like this:
         int f() { extern int i; return i; }
         static int i = 0;
       to avoid "set-but-never-used" diagnostics. */
    source_corresp_ptr = &variable_ptr->source_corresp;
    if (((a_symbol_ptr)source_corresp_ptr->assoc_info)->
                                                  variant.variable.used) {
      sym->variant.variable.used = TRUE;
    }  /* if */
    /* Move the variable entry to the end of the variables list if this is
       its definition. */
    if (srk_flags & SRK_DEFINITION) {
      /* This is definition of a variable that has previously been declared.
         In C++ only one declaration of a variable can be construed to be
         its definition, so if we are in C++ mode this is it. */
      if (sym->defined && (srk_flags & SRK_TENTATIVE_DEF)) {
        /* In C ignore a tentative definition (i.e., one for which no
           initializer is present) if the variable has already been defined
           in a previous tentative definition.  This may also come up in
           Microsoft mode for arrays of unknown dimension. */
      } else {
        /* This is a definition of a variable that was not previously
           defined, so unlink the variable entry and relink it at the end
           of the variables list, so that variables appear in the order in
           which they are defined. */
        /* This is only possible for file-scope variables, never for local
           variables, since it is only by means of a prior extern declaration
           or (in C mode only) a prior tentative definition that we can be
           defining a variable that has already been declared. */
        move_variable_to_end_of_list(variable_ptr, idlb.linked_symbol,
                                     idlb.from_inline_namespace);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Link the symbol to the IL variable entry. */
  sym->variant.variable.ptr = variable_ptr;
  if (sym_must_be_entered) {
    /* Add the previously created symbol to the symbol table. */
    add_symbol_to_symbol_table(sym, effective_decl_level,
                               inhibit_redecl_error ||
                               redecl_error_already_issued);
  }  /* if */
  if (*ext_sym != NULL &&
      (*ext_sym)->variant.extern_symbol_descr->variant.variable == NULL) {
    /* Link the external symbol to the IL variable entry. */
    (*ext_sym)->variant.extern_symbol_descr->variant.variable = variable_ptr;
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  if (source_corresp_ptr->assoc_info == NULL) {
    /* There is no symbol pointed to from the variable or routine, so
       update it with the current symbol. */
    set_source_corresp(source_corresp_ptr, sym);
    if (idlb.is_block_extern_decl && secondary_translation_unit_seen()) {
      /* This variable entry might have been generated during the
         instantiation of a template.  The correspondence checking process
         must therefore be notified of its existence. */
      establish_block_extern_variable_correspondence(variable_ptr);
    }  /* if */
  } else if (!redeclaration) {
    check_sym_of_other_decl(source_corresp_ptr, sym);
  }  /* if */
  if (dps->dso_flags & DSO_CONSTEXPR) {
    if (is_variable_def) {
      variable_ptr->is_constexpr = TRUE;
    } else {
      pos_error(ec_constexpr_variable_decl_must_be_definition,
                &dps->constexpr_pos);
    }  /* if */
  }  /* if */
  dps->sym = sym;
  attach_decl_attributes(dps, is_variable_def);
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
    /* Record the assembly name. */
    if (dps->asm_name != NULL) {
      record_asm_name_for_variable(
                 variable_ptr, dps->asm_name, is_register, &dps->asm_name_pos,
                 symbol_for(variable_ptr)->defined && !is_variable_def);
    }  /* if */
  }  /* if */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  /* Update the ELF visibility if applicable. */
  { an_ELF_visibility_kind  visibility = variable_ptr->ELF_visibility;
    update_for_default_ELF_visibility(&visibility, /*is_class_member=*/FALSE);
    variable_ptr->ELF_visibility = visibility;
  }
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  if (named_registers_enabled) {
    /* Check and (if needed) record the named-register storage class.
       If there was an explicit storage class specifier, diagnostics will be
       issued for those; otherwise, the diagnostic will point to the
       declarator-id. */
    a_source_position_ptr  diag_pos;
    if (decl_pos_block != NULL && decl_pos_block->storage_class_pos.seq != 0) {
      diag_pos = &decl_pos_block->storage_class_pos;
    } else {
      diag_pos = &locator->source_position;
    }  /* if */
    record_named_register_storage_class(variable_ptr, dps->register_id,
                                        redeclaration, diag_pos);
  }  /* if */
#endif /* NAMED_REGISTERS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (dps->ms_attributes != NULL && !idlb.is_block_extern_decl) {
    /* Microsoft attributes don't normally apply to variables, but some
       attributes that apply to "any" entity are also accepted on variable
       declarations. */
    apply_microsoft_attributes_to_variable(&dps->ms_attributes, variable_ptr);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (linkage != idl_none) {
    /* In case this is a block extern declaration, clear the
       is_local_to_function flag -- it will have been set based on scope
       alone in set_source_corresp. */
    source_corresp_ptr->is_local_to_function = FALSE;
  }  /* if */
  if (depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE &&
      alloc_at_file_scope && !redeclaration) {
    add_namespace_parent_pointer(sym, source_corresp_ptr);
  }  /* if */
  /* The name linkage has already been determined.  Apply it to the current
     declaration, and report inconsistencies, if appropriate. */
  set_name_linkage(&idlb, sym, source_corresp_ptr, *ext_sym,
                   &locator->source_position);
  /* Copy the decl-modifiers into the variable entry. */
  update_variable_decl_modifiers(dps);
  if (!variable_ptr->source_corresp.is_deprecated) {
    /* Check if a deprecated type was involved in this declaration. */
    warn_about_use_of_deprecated_type(type_ptr, &locator->source_position);
  }  /* if */
  /* If cross-reference information is being issued, update the output.  If
     source sequence entries are being generated, update the source sequence
     entry for this declaration (this may cause the allocation of a new source
     sequence entry in some cases). */
  record_symbol_declaration(srk_flags, sym, &locator->source_position,
                            dps->source_sequence_entry);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (is_variable_def || (!redeclaration && !linked_to_previous_variable)) {
    /* The position corresponds to that of the first declaration, or to that
       of the definition if a definition is seen. */
    update_decl_pos_info(&variable_ptr->source_corresp, decl_pos_block);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Restore the scope stack.  This is done prior to updating secondary
     source sequence entries because otherwise we might not find such an
     entry for the case of a nondefining namespace-qualified variable
     declaration (valid in Microsoft mode only). */
  if (idlb.namespace_reactivated) pop_namespace_extension_scope();
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Do fixup on the source sequence entry that was just created to
     represent the current declaration.  Note that dps->source_sequence_entry
     must be updated, since it may have been replaced (e.g., when a file scope
     entity is declared in a local scope and a sublist is generated). */
  reload_source_sequence_entry(dps);
  if (record_name_references_in_context()) {
    name_ref = qualifiable_name_reference(locator, source_corresp_ptr);
  }  /* if */
  if (!is_variable_def || (srk_flags & SRK_TENTATIVE_DEF)) {
    an_sssd_flag_set  flags = SSSD_NO_FLAGS;
#if GNU_EXTENSIONS_ALLOWED
    if (dps->decl_modifiers.marked_as_gnu_extension) {
      flags |= SSSD_MARKED_AS_GNU_EXTENSION;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    (void)update_src_seq_secondary_decl((char *)variable_ptr, declared_type,
                                        name_ref, flags, decl_pos_block);
  } else {
    /* The defining declaration of the variable.  Record the type and the
       form of the declarator. */
    if (name_ref != NULL) {
      name_ref->used_in_primary_declarator = TRUE;
    }  /* if */
    if (variable_ptr->declared_type == NULL) {
      variable_ptr->declared_type = declared_type;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (dps->decl_modifiers.marked_as_gnu_extension) {
      variable_ptr->source_corresp.marked_as_gnu_extension = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (variable_ptr->storage_class == (a_storage_class)sc_static &&
      variable_ptr->source_corresp.is_local_to_function) {
    /* Local static variable definition. */
    check_assertion(is_variable_def && innermost_function_scope != NULL);
    /* Remember that the current function has at least one local static
       variable. */
    current_routine_entry()->contains_local_static_variable = TRUE;
    if (std_c99_inlining) {
      check_c99_inline_definition(variable_ptr, &locator->source_position);
    }  /* if */
#if NEED_NAME_MANGLING
    /* Local static variables may need to be mangled.  If two (or more) such
       variables in a function have the same name, a discriminator must be
       appended to the mangled name (for the IA-64 ABI).  It is convenient
       to compute this discriminator at this time. */
    compute_name_collision_discriminator(sym, decl_scope_level);
#endif /* NEED_NAME_MANGLING */
  }  /* if */
  if (vla_enabled) {
    if (is_variably_modified_type(type_ptr)) {
      /* Since the type may have various run-time dependencies, put out a
         statement indicating where in the executable stream this declaration
         appears.  */
      if (depth_stmt_stack < 0) {
        /* We can get here with a namespace scope declaration of the form
             void (*pf)(int[*][*]);
           Some (unlikely) error situations can also cause us to get here. */
        check_assertion(total_errors > 0 || !is_vla_type(type_ptr));
      } else {
        a_statement_ptr vla_stmt;

        variable_ptr->has_variably_modified_type = TRUE;
        vla_stmt = add_statement_at_stmt_pos((a_statement_kind)stmk_vla_decl,
                                             &locator->source_position);
        vla_stmt->variant.vla.is_typedef_decl = FALSE;
        vla_stmt->variant.vla.variant.variable = variable_ptr;
        check_for_vla_inside_statement_expression(&locator->source_position);
        if (is_vla_type(type_ptr)) {
          if (!is_variable_def) {
            /* Must be an error. */
            check_assertion(total_errors > 0);
          } else {
            /* Memory for this variable will also have to be allocated.  Mark
               the variable as a variable length array that requires allocation
               as well as deallocation upon exit from the current scope. */
            variable_ptr->is_vla = TRUE;
            /* Update the control-flow list used in statement processing to
               diagnose illegal branches. */
            update_init_statement_control_flow(vla_stmt);
          }  /* if */
        }  /* if */
      }  /* if */
    } /* if */
  }  /* if */
  if (is_variable_def && is_or_has_volatile_qualified_type(type_ptr)) {
    /* A variable with a volatile type is considered to be used and modified
       from "elsewhere".  (We use "is_variable_def" to exclude cases like
       "extern volatile int x", for which the flags shouldn't be set unless
       there is an explicit use in this translation unit.)  Note that this
       must be done after set_source_corresp because the latter clears the
       IL referenced flag. */
    source_corresp_ptr->referenced = TRUE;
    sym->referenced = TRUE;
    sym->variant.variable.used = TRUE;
    sym->value_has_been_set = TRUE;
  }  /* if */
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration.  Note that this has to be *after* the
     scope stack is restored, since processing depends on the pending_pragmas
     pointer in the scope stack entry. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  /* Return linkage kind. */
  *linkage_ptr = linkage;
  dps->storage_class = storage_class;
  /* Restore the "innermost namespace scope" if it was modified. */
  if (saved_depth_innermost_namespace_scope != NO_SCOPE_DEPTH) {
    depth_innermost_namespace_scope = saved_depth_innermost_namespace_scope;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_variable */

#if GENERATE_SOURCE_SEQUENCE_LISTS

a_type_ptr update_routine_declared_type(a_type_ptr  rout_type,
                                        a_type_ptr  declared_type)
/*
declared_type is the type with which a routine was declared, and rout_type is
the type the routine actually has: Make the former consistent with the latter
if appropriate.  Return the resulting declared type.
*/
{
  a_routine_type_supplement_ptr  rtsp1, rtsp2;

  rtsp1 = skip_typerefs(rout_type)->variant.routine.extra_info;
  rtsp2 = skip_typerefs(declared_type)->variant.routine.extra_info;
  if (!same_entities(rtsp1->this_class, rtsp2->this_class) ||
      rtsp1->qualifiers != rtsp2->qualifiers ||
      rtsp1->this_qualifiers != rtsp2->this_qualifiers ||
      rtsp1->routine_name_linkage != rtsp2->routine_name_linkage) {
    if (declared_type->kind == (a_type_kind)tk_typeref) {
      check_assertion(!is_qualified_type(declared_type));
      declared_type =
         copy_routine_type_with_param_types(declared_type,
                                            /*copy_default_args=*/TRUE);
      rtsp2 = declared_type->variant.routine.extra_info;
    }  /* if */
    rtsp2->this_class = rtsp1->this_class;
    rtsp2->qualifiers = rtsp1->qualifiers;
    rtsp2->this_qualifiers = rtsp1->this_qualifiers;
    rtsp2->routine_name_linkage = rtsp1->routine_name_linkage;
  }  /* if */
  return declared_type;
}  /* update_routine_declared_type */


void set_routine_declared_type(a_routine_ptr  routine_ptr,
                               a_type_ptr     declared_type)
/*
Set the declared_type field in the indicated routine entry, using its own
type entry if appropriate, otherwise using the indicated declared_type.
*/
{
  a_type_ptr                     rout_type = routine_ptr->type;
  a_boolean                      use_routine_type = TRUE;
  a_routine_type_supplement_ptr  rtsp1, rtsp2;

  if (routine_ptr->declared_type != NULL) {
    check_assertion_str(routine_ptr->is_template_function ||
                        routine_ptr->is_prototype_instantiation ||
                        routine_ptr->defined_in_friend_decl ||
                        (routine_ptr->source_corresp.is_class_member &&
                         scp_parent_class(&routine_ptr->source_corresp)->
                              variant.class_struct_union.
                                                 is_in_class_specialization) ||
                        (total_errors !=  0),
                       "set_routine_declared_type: declared type already set");
    declared_type = routine_ptr->declared_type;
    routine_ptr->declared_type = NULL;
  }  /* if */
  /* Make the declared type consistent with the routine type. */
  declared_type = update_routine_declared_type(rout_type, declared_type);
  /* Record the declared type.  Re-use the actual type if possible. */
  rtsp1 = skip_typerefs(rout_type)->variant.routine.extra_info;
  rtsp2 = skip_typerefs(declared_type)->variant.routine.extra_info;
  if (rtsp2->param_type_list != NULL) {
    /* The name and position information of parameters on this declaration is
       likely different from that of previous declarations.  Hence, force the
       use of the type just parsed to correctly record that information. */
    use_routine_type = FALSE;
  } else if (!identical_types(declared_type, rout_type)) {
    /* The types are not identical, so the routine's type cannot also be
       used as the declared type. */
    use_routine_type = FALSE;
  } else if ((rtsp1->exception_specification == NULL) !=
             (rtsp2->exception_specification == NULL)) {
    /* Exception specification mismatch (usually involves predeclared
       functions like new and delete). */
    use_routine_type = FALSE;
  } else {
    /* The types can be shared. */
    use_routine_type = TRUE;
  }  /* if */
  /* Set the declared_type pointer in the routine entry. */
  if (use_routine_type) {
    routine_ptr->declared_type = rout_type;
  } else {
    /* There was some difference, so use a separate declared type. */
    routine_ptr->declared_type = declared_type;
  }  /* if */
}  /* set_routine_declared_type */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static void record_pragma_state_in_routine(a_routine_ptr  routine_ptr)
/*
Record any pragma state that may affect the meaning of the IL in the definition
of the given routine.
*/
{
  if (c99_mode) {
    /* In C99 mode, save the current settings of the predefined pragmas. */
    routine_ptr->fp_contract = curr_fp_contract_state;
    routine_ptr->fenv_access = curr_fenv_access_state;
    routine_ptr->cx_limited_range = curr_cx_limited_range_state;
  }  /* if */
#if FIXED_POINT_ALLOWED
  if (fixed_point_enabled) {
    /* Save the state of the fixed-point pragmas. */
    routine_ptr->fx_full_precision = curr_fx_full_precision_state;
    routine_ptr->fx_fract_overflow = curr_fx_fract_overflow_state;
    routine_ptr->fx_accum_overflow = curr_fx_accum_overflow_state;
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    /* Record the current UPC access method for this routine.  This is the
       method last specified by a UPC pragma in file scope. */
    routine_ptr->upc_access_method = curr_upc_access_method;
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
}  /* record_pragma_state_in_routine */

#if GNU_EXTENSIONS_ALLOWED

static void record_asm_name_for_routine(
                                    a_routine_ptr          routine,
                                    a_const_char           *asm_name,
                                    a_source_position_ptr  diag_pos,
                                    a_boolean              previously_defined)
/*
Record the given asm name in the given routine entry.  If a conflict is
detected, issue a diagnostic at the given position.  previously_defined is
TRUE if a definition preceded the current declaration.
*/
{
  check_assertion(asm_name != NULL);
  if (gcc_mode && previously_defined) {
    /* GNU C (but not GNU C++) 4.0.0 and later ignore asm names appearing on
       declarations of previously defined functions with a warning.  Earlier
       versions of GNU C take the asm name into account only for subsequent
       references.  We enforce the newer behavior for all values of gnu_version
       because the older behavior would add too much complexity.  E.g.:
         void f() {}           // Always emitted as "f" by gcc (not g++).
         void f() __asm("h");  // Ignored in gcc 4.0.0 and later.
         void g() { f(); }     // Refers to "h" in versions prior to 4.0.0.
       (GNU C++ applies the name change throughout the translation unit.)
    */
    /* Don't warn if the construct is identical to one that applied to the
       definition. */
    if (!has_gnu_routine_supp(routine) ||
        gnu_routine_supp(routine)->asm_name == NULL ||
        strcmp(gnu_routine_supp(routine)->asm_name, asm_name) != 0) {
      pos_warning(ec_asm_name_after_definition, diag_pos);
    }  /* if */
  } else if (!has_gnu_routine_supp(routine) ||
             gnu_routine_supp(routine)->asm_name == NULL) {
    /* This is the first declaration of this routine with an "asm name"
       construct. */
    ensure_gnu_routine_supp(routine)->asm_name = asm_name;
    record_asm_name_for_lookup(symbol_for(routine));
  } else if (strcmp(gnu_routine_supp(routine)->asm_name, asm_name) != 0) {
    /* The current declaration has an "asm name" that is different from
       one specified on a previous declaration.  Issue a warning and
       ignore the specification on the current declaration. */
    pos_warning(ec_asm_name_conflict, diag_pos);
  }  /* if */
}  /* record_asm_name_for_routine */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void check_incompatible_routine_redecl(
                                       a_symbol_ptr   linked_sym,
                                       a_type_ptr     new_type,
                                       a_boolean      old_decl_has_body,
                                       a_boolean      is_function_def,
                                       an_error_code  error_code,
                                       a_source_position_ptr
                                                      diag_pos,
                                       a_type_ptr     *old_type,
                                       a_boolean      *linked_redecl_error,
                                       a_boolean      *suppress_ext_sym_lookup)
/*
An incompatible declaration of routine represented by linked_sym has been
parsed.  The (incompatible) redeclared type is new_type.  Issue a diagnostic
(at the position described by diag_pos) and either record the original type
in *old_type or force the creation of a new routine entry (and symbol) by
setting *linked_redecl_error to TRUE.  Some modes (GNU C, Microsoft C, and
SVR4) allow certain incompatibilities: Only a warning is issued in those
cases.  Otherwise an error is issued and *suppress_ext_sym_lookup is set to
TRUE to notify the caller that a new "extern symbol" should be forced.
old_decl_has_body is TRUE if the function was previously defined, and
is_function_def is TRUE if the redeclaration is a definition.
*/
{
  a_routine_ptr  rp = linked_sym->variant.routine.ptr;
  a_type_ptr     old_return_type = return_type_of(rp->type);
  a_type_ptr     new_return_type = return_type_of(new_type);
  a_boolean      compat = FALSE;

  if (gcc_mode &&
      !skip_typerefs(new_type)->variant.routine.extra_info->prototyped &&
      skip_typerefs(rp->type)->variant.routine.extra_info->prototyped) {
    /* GNU C compilers relax compatibility requirements when an old-style
       definition follows a prototyped declaration.  (Calling conventions are
       ignored here because calling convention attributes may not have taken
       effect yet; calling conventions will be re-checked later if needed.) */
    a_type_compat_flags_set  tcf = TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |
                                   TCF_IGNORE_CALLING_CONVENTIONS;
    if (gnu_version < 40000) {
      /* GCC 2.x and 3.x ignore return type qualifiers in this case. */
      tcf |= TCF_IGNORE_RETURN_TYPE_QUALIFIERS;
      compat = f_types_are_compatible(rp->type, new_type, tcf);
    }  /* if */
    if (!compat) {
      /* Unpromoted argument types are considered compatible in this case. */
      tcf |= TCF_NO_DEFAULT_ARG_PROMOTIONS;
      compat = f_types_are_compatible(rp->type, new_type, tcf);
    }  /* if */
    if (compat) {
      /* The prototyped declaration is retained for typing purposes.  If a
         nondefining unprototyped declaration follows a nondefining prototyped
         declaration, GNU C ignores the prototype. */
      *old_type = rp->type;
      if (new_type->kind == (a_type_kind)tk_routine) {
        /* The later declaration was not through a typedef. */
        if (!old_decl_has_body && !is_function_def) {
          /* Neither declaration was a definition. */
          pos_sy_warning(ec_prototype_lost, diag_pos, linked_sym);
          rp->type = new_type;
        } else if (new_type->variant.routine.extra_info
                           ->old_style_params_scanned) {
          /* The later declaration was an old-style definition.  Record that
             old-style parameters were scanned (even though we will retain the
             prototype type).  Among other things, this may indicate that
             source sequence entries were recorded for the old-style
             parameters. */
          skip_typerefs(*old_type)->variant.routine.extra_info
                                  ->old_style_params_scanned = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (rp->special_kind == (a_special_function_kind)sfk_none &&
             rp->variant.builtin_function_kind !=
                                          (a_builtin_function_kind)bfk_none) {
    /* This is a redeclaration of a predeclared function.  Hide (but do not
       remove) the old declaration by setting linked_redecl_error to TRUE. */
    pos_sy_warning(ec_builtin_function_hidden, diag_pos, linked_sym);
    *linked_redecl_error = TRUE;
    compat = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (SVR4_C_mode &&
             incompatible_types_are_SVR4_compatible(new_type, rp->type)) {
    /* The routine types are incompatible, but in SVR4 mode this is
       not an error as long as the incompatibility is only in the
       return type or if one of the declarations is prototyped while
       the other is not. */
    pos_sy_warning(ec_not_compatible_with_previous_decl, diag_pos, linked_sym);
    *old_type = rp->type;
    /* If this is the definition, reset the type of the routine entry
       to use the new type. */
    if (is_function_def) {
      rp->type = new_type;
    }  /* if */
    compat = TRUE;
  } else if (microsoft_mode && C_mode() &&
             interchangeable_types(old_return_type, new_return_type)) {
    /* In Microsoft C mode "anything goes" as far as function redeclarations
       are concerned, provided the return types are "interchangeable" (the
       Microsoft C compiler allows even stronger incompatibilities, but we
       don't emulate those). */
    pos_sy_warning(ec_not_compatible_with_previous_decl, diag_pos, linked_sym);
    *old_type = rp->type;
    if (is_function_def || !old_decl_has_body) {
      rp->type = new_type;
    }  /* if */
    compat = TRUE;
  }  /* if */
  if (!compat) {
    /* Issue an error on incompatible declarations. */
    pos_sy_error(error_code, diag_pos, linked_sym);
    if (rp->storage_class == (a_storage_class)sc_static) {
      /* Reuse the routine entry to avoid error recovery problems
         connected with constraints placed on static functions. */
      *old_type = rp->type;
      if (is_function_def) rp->type = new_type;
    } else {
      /* Force creation of a new symbol and a new routine entry. */
      *linked_redecl_error = TRUE;
    }  /* if */
    /* Set a flag to suppress reuse of the existing external-routine
       symbol. */
    *suppress_ext_sym_lookup = TRUE;
  }  /* if */
}  /* check_incompatible_routine_redecl */


static a_symbol_ptr create_external_symbol_for_routine(
                         a_symbol_locator       *locator,
                         a_decl_parse_state     *dps,
                         a_type_ptr             type_ptr,
                         an_id_linkage_block    *idlbp,
                         a_boolean              microsoft_specialization_redef,
                         a_boolean              suppress_incompatible_error,
                         a_boolean              suppress_ext_sym_lookup,
                         a_routine_ptr          *routine_ptr)
/*
Find or create an external symbol entry for a routine being declared.
This is a wrapper for create_external_symbol_for_linked_entity that returns
NULL for some template-related cases.  locator, dps, type_ptr, and idlbp
describe the routine being declared.  microsoft_specialization_redef is TRUE
for the relatively rare case of a specialization being redefined (only allowed
in some Microsoft bugs modes).  suppress_incompatible_error is TRUE if no
incompatibility diagnostic should be emitted.  If suppress_ext_sym_lookup is
TRUE, an existing compatible external symbol is ignored.  *routine_ptr is set
to point to a routine entry attached to an existing compatible external symbol
(if any is found).
*/
{
  a_symbol_ptr  result = NULL;
  a_symbol_ptr  linked_symbol = idlbp->linked_symbol;

  check_assertion(linked_symbol == NULL ||
                  linked_symbol->kind == (a_symbol_kind)sk_routine ||
                  linked_symbol->kind == (a_symbol_kind)sk_member_function);
  if (locator->is_template_id ||
      microsoft_specialization_redef ||
      (linked_symbol != NULL &&
       linked_symbol->variant.routine.instance_ptr != NULL)) {
    /* We're dealing with a template instance (or an explicit specialization):
       Do not create an external symbol.  (External symbols cannot really deal
       with template signatures anyway.) */
  } else if (scope_stack[depth_scope_stack].in_prototype_instantiation &&
             !prototype_instantiations_in_il) {
    /* The declaration appeared during a prototype instantiation, but we
       will not record the prototype instantiation in the IL. */
  } else {
    /* Create an external symbol for the present linkable declaration. */
    result = create_external_symbol_for_linked_entity(
                                       locator, dps, type_ptr, idlbp,
                                       /*redeclaration=*/FALSE,
                                       suppress_incompatible_error,
                                       suppress_ext_sym_lookup,
                                       (a_variable_ptr*)NULL, routine_ptr);
  }  /* if */
  return result;
}  /* create_external_symbol_for_routine */


static a_symbol_ptr record_overload(a_symbol_locator  *locator,
                                    a_boolean         is_template,
                                    a_symbol_ptr      homonym_symbol,
                                    a_symbol_ptr      *overload_set,
                                    a_boolean         invisible,
                                    a_boolean         is_friend)
/*
locator represents a new function (or function template, if is_template is
TRUE) declaration that overloads one or more existing declarations represented
by homonym_symbol.  Return a symbol for the new entity and update *overload_set
to represent the resulting overload set.  invisible is TRUE if the new
declaration is invisible during normal name lookup (e.g., because the
declaration is a non-injected friend declaration).  is_friend is TRUE if the
new declaration is a friend declaration.
*/
{
  a_symbol_ptr   sym;
  a_symbol_kind  sym_kind = (a_symbol_kind)sk_routine;
  a_boolean      overload_set_is_invisible = FALSE;

  if (is_template) {
    sym_kind = (a_symbol_kind)sk_function_template;
  }  /* if */
  if (invisible && homonym_symbol->is_invisible) {
    /* If both the previous declaration(s) and the new declaration are
       invisible, the resulting set is invisible too. */
    overload_set_is_invisible = TRUE;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (homonym_symbol->kind == (a_symbol_kind)sk_routine) {
    a_routine_ptr  rp = homonym_symbol->variant.routine.ptr;
    if (rp->special_kind == (a_special_function_kind)sfk_none &&
        rp->variant.builtin_function_kind !=
                                          (a_builtin_function_kind)bfk_none) {
      /* This declaration overloads a predeclared function: Issue a warning. */
      pos_sy_warning(ec_builtin_function_overloaded, &locator->source_position,
                     homonym_symbol);
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  sym = enter_overloaded_symbol(sym_kind, locator, /*is_constructor=*/FALSE,
                                homonym_symbol, overload_set);
  /* Update the visibility of the new symbol and of the overload set it
     belongs to. */
  if (invisible) {
    sym->is_invisible = TRUE;
    if (overload_set_is_invisible) {
      (*overload_set)->is_invisible = TRUE;
    }  /* if */
  } else if (!is_friend) {
    (*overload_set)->is_invisible = FALSE;
  }  /* if */
  return sym;
}  /* record_overload */

#if GNU_EXTENSIONS_ALLOWED

static void check_implicit_routine_alias(a_decl_parse_state  *dps)
/*
GCC recognizes certain declarations as matching standard library functions
and optimizes calls to those routines in some cases.  In particular, the
strlen function is recognized and when applied to a string literal, the result
is a constant-expression.  We emulate the "recognition" process by recording
the routine as an implicit alias (which expression processing can then make
use of).
*dps describes the declaration of a function.
*/
{
  check_assertion(gnu_mode);
  if (is_simple_function_symbol(dps->sym) &&
      !sym_is_class_or_namespace_member(dps->sym)) {
    a_routine_ptr  rp = dps->sym->variant.routine.ptr;
    a_routine_type_supplement_ptr
                   rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
    if (rp->implicit_alias && dps->is_definition) {
      /* If a definition is seen after a declaration that was implicitly
         aliased, the alias is cleared. */
      ensure_gnu_routine_supp(rp)->aliased_routine = NULL;
      rp->implicit_alias = FALSE;
    } else if (dps->first_decl && !dps->sym->defined &&
               rtsp->prototyped &&
               (!has_gnu_routine_supp(rp) ||
                gnu_routine_supp(rp)->aliased_routine == NULL) &&
               rp->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external &&
               /* Exclude routines with an asm alias. */
               (!has_gnu_routine_supp(rp) ||
                 gnu_routine_supp(rp)->asm_name == NULL) &&
               dps->asm_name == NULL &&
               /* Exclude routines with "alias" or "weakref" attributes. */
               find_decl_attribute(ak_alias, dps) == NULL &&
               find_decl_attribute(ak_weakref, dps) == NULL) {
      a_const_char *name = NULL;
      if (strcmp(rp->source_corresp.name, "strlen") == 0) {
        name = "__builtin_strlen";
      } else if (strcmp(rp->source_corresp.name, "abs") == 0) {
        name = "__builtin_abs";
      }  /* if */
      if (name != NULL) {
        a_symbol_locator  loc;
        a_symbol_ptr      bsym;
        bsym = find_symbol(name, (sizeof_t)strlen(name), &loc);
        for (; bsym != NULL; bsym = bsym->next) {
          if (is_simple_function_symbol(bsym) &&
              !sym_is_class_or_namespace_member(bsym)) {
            a_routine_ptr                 brp = bsym->variant.routine.ptr;
            a_routine_type_supplement_ptr brtsp;
            brtsp = skip_typerefs(brp->type)->variant.routine.extra_info;
            if (brtsp->prototyped &&
                types_are_redecl_compatible(rp->type, brp->type)) {
              ensure_gnu_routine_supp(rp)->aliased_routine = brp;
              rp->implicit_alias = TRUE;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_implicit_routine_alias */


static a_boolean check_gnu_inline_attribute(a_decl_parse_state   *dps,
                                            an_id_linkage_block  *idlbp,
                                            a_boolean            redeclaration)
/*
Check whether the function being declared has the "gnu_inline" attribute
(either in the current declaration or in a previous declaration) and return
TRUE if that is the case.  The current declaration is described by *dps and
*idlbp; it is a redeclaration if redeclaration is TRUE.
*/
{
  a_boolean         result = FALSE;
  a_symbol_ptr      linked_symbol = idlbp->linked_symbol;
  an_attribute_ptr  ap;

  if (redeclaration && linked_symbol->kind == (a_symbol_kind)sk_routine &&
      linked_symbol->variant.routine.ptr->gnu_c89_inline) {
    /* The routine was previously declared with the gnu_inline attribute. */
    result = TRUE;
    /* Recent versions of GCC require all subsequent declarations that are
       explicitly declared "inline" to also specify the gnu_inline
       attribute. */
    check_assertion(idlbp->func_info != NULL);
    if (gnu_version >= 40300 && idlbp->func_info->is_inline &&
        find_decl_attribute(ak_gnu_inline, dps) == NULL) {
      ap = find_attribute(ak_gnu_inline,
                          linked_symbol->variant.routine.ptr
                                       ->source_corresp.attributes);
      check_assertion(ap != NULL);
      pos2_diagnostic(es_error, ec_missing_gnu_inline_attr_on_redeclaration,
                      &dps->declarator_pos, &ap->position);
    }  /* if */
  } else if ((dps->prefix_attributes != NULL || dps->id_attributes != NULL) &&
             idlbp->func_info->is_inline) {
    /* An inline function with attributes.  Check if "gnu_inline" is among
       those attributes. */
    ap = find_decl_attribute(ak_gnu_inline, dps);
    if (ap != NULL) {
      if (redeclaration && linked_symbol->kind == (a_symbol_kind)sk_routine &&
          linked_symbol->variant.routine.ptr->is_inline) {
        /* The function was previously declared inline (but not gnu_inline):
           Issue an error.  (This cannot easily be done when the attribute is
           applied because the is_inline flag may have changed by then.) */
        pos_error(ec_first_decl_not_gnu_inline, &ap->position);
        make_attr_unrecognized(ap);
      } else {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* check_gnu_inline_attribute */

#endif /* GNU_EXTENSIONS_ALLOWED */

void issue_no_exception_support_diag_on_throw_spec(
					a_func_info_block_ptr	func_info)
/*
Issue a diagnostic on attempting to define a noninline function with
an exception specification when exception support is not enabled.
The function is specified by func_info.  (No diagnostic is issued on
a nondefinition -- the exception specification is just ignored.  In GNU
C++ mode, even exception specifications on definitions are ignored.)
*/
{
  if (!(func_info->is_inline || gpp_mode) && 
      func_info->throw_position.seq != 0) {
    pos_warning(ec_no_exception_support, &func_info->throw_position);
  }  /* if */
}  /* issue_no_exception_support_diag_on_throw_spec */


a_boolean check_constexpr_routine_def_type(a_routine_ptr      rp,
                                           a_source_position  *diag_pos)
/*
Return TRUE if and only if the given function type is a valid type for a
constexpr function definition (these checks are not performed for a
declaration that isn't a definition).  Otherwise, return FALSE and, if the
given routine is not a template instance, issue a diagnostic at the given
position.
*/
{
  a_boolean   okay = TRUE;
  a_type_ptr  rtp = skip_typerefs(rp->type);

  if (rtp->kind == (a_type_kind)tk_error) {
    /* A severe error must have occurred: Nothing more to be done. */
    expect_error();
  } else {
    check_assertion(rtp->kind == (a_type_kind)tk_routine);
    /* The return type and parameter types of a constexpr function must be
       literal types.  Since destructors don't have a return type, this
       implies they cannot be constexpr. */
    if (special_kind_is(rp, sfk_destructor)) {
      /* Destructors cannot be constexpr: This should have been caught
         earlier. */
      unexpected_condition();
    } else if (!special_kind_is(rp, sfk_constructor) &&
               !could_be_literal_type(rtp->variant.routine.return_type)) {
      okay = FALSE;
      if (!rout_is_real_template_instance(rp)) {
        pos_ty_error(ec_nonliteral_return_type_in_constexpr_function, diag_pos,
                     rtp->variant.routine.return_type);
      }  /* if */
    } else {
      a_param_type_ptr  ptp = rtp->variant.routine.extra_info->param_type_list;
      for (; ptp != NULL; ptp = ptp->next) {
        if (!could_be_literal_type(ptp->type)) {
          okay = FALSE;
          if (!rout_is_real_template_instance(rp)) {
            pos_ty_error(ec_nonliteral_param_type_in_constexpr_function,
                         diag_pos, ptp->type);
          }  /* if */
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return okay;
}  /* check_constexpr_routine_def_type */


a_boolean check_udl_operator_template(a_symbol_ptr       templ_sym,
                                      a_source_position  *pos)
/*
templ_sym represents a literal operator template.  Check that the template has
an acceptable signature.  Return TRUE if it does.  Otherwise, return FALSE,
and, if the given position is non-NULL, issue one or more errors as
appropriate.
*/
{
  a_boolean             result = TRUE;
  a_template_symbol_supplement_ptr
                        tssp = templ_sym->variant.template_info;
  a_routine_ptr         rp = tssp->variant.function.routine;
  a_type_ptr            rtp = skip_typerefs(rp->type);
  a_param_type_ptr      ptp = function_type_params(rtp);
  a_template_param_ptr  tpp;

  if (rp->source_corresp.name_linkage == (a_name_linkage_kind)nlk_external) {
    if (pos != NULL) {
      pos_error(ec_extern_c_literal_operator, pos);
    }  /* if */
    result = FALSE;
  }  /* if */
  if (ptp != NULL || rtp->variant.routine.extra_info->has_ellipsis) {
    if (pos != NULL) {
      pos_error(ec_invalid_parameter_for_literal_operator_template, pos);
    }  /* if */
    result = FALSE;
  }  /* if */
  tpp = tssp->variant.function.decl_cache.decl_info->parameters;
  check_assertion(tpp != NULL);
  if (tpp->next != NULL ||
      !tpp->is_pack  ||
      !symbol_is(tpp->param_symbol, sk_constant) ||
      !is_plain_char_type(tpp->variant.constant.ptr->type)) {
    if (pos != NULL) {
      pos_error(ec_invalid_template_parameter_for_literal_operator_template,
                pos);
    }  /* if */
    result = FALSE;
  }  /* if */
  return result;
}  /* check_udl_operator_template */


static a_boolean is_valid_udl_char_parameter_type(a_type_ptr  char_type)
/*
Return TRUE if char_type is a valid unqualified type underlying the pointer
type parameter of a user-defined literal operator for user-defined string
literals.  Usually, the type must be one of: char, unsigned char, signed char,
wchar_t, char16_t, or char32_t.  However, in some Microsoft modes, a typedef
named wchar_t may be acceptable too (if its underlying type is the integer
type for wide character literals).
*/
{
  a_type_ptr  tp = skip_typerefs(char_type);
  a_boolean   result;

  if (tp->kind != (a_type_kind)tk_integer) {
    result = FALSE;
  } else if (is_plain_char_type(tp) ||
             tp->variant.integer.wchar_t_type ||
             tp->variant.integer.char16_t_type ||
             tp->variant.integer.char32_t_type) {
    result = TRUE;
  } else if (!wchar_t_is_keyword && microsoft_mode &&
             tp->variant.integer.int_kind == targ_wchar_t_int_kind) {
     /* Microsoft compilers accept a "wchar_t" parameter in modes where wchar_t
        is not actually a built-in type, provided the type is expressed via a
        typedef named "wchar_t" (that typedef can appear in any scope, and can
        appear under another typedef). */
    result = FALSE;
    tp = char_type;
    while (tp->kind == (a_type_kind)tk_typeref) {
      if (typeref_is_typedef(tp)) {
        a_const_char  *name = unmangled_name_of(&tp->source_corresp);
        check_assertion(name != NULL);
        if (strcmp(name, "wchar_t") == 0) {
          result = TRUE;
          break;
        }  /* if */
      }  /* if */
      tp = tp->variant.typeref.type;
    }  /* while */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_valid_udl_char_parameter_type */


static void check_udl_operator_type(a_symbol_locator  *loc,
                                    a_routine_ptr     rp)
/*
The given function (declared with the given locator) represents a literal
operator.  Check that it has an acceptable type and linkage and issue an
appropriate error if it doesn't.  Also set the is_raw_literal_operator flag if
needed.
*/
{
  a_type_ptr        rtp = skip_typerefs(rp->type);
  a_param_type_ptr  ptp = function_type_params(rtp);
  a_boolean         param_err = FALSE;

  if (rp->source_corresp.name_linkage == (a_name_linkage_kind)nlk_external) {
    pos_error(ec_extern_c_literal_operator, &loc->source_position);
  }  /* if */
  if (rtp->variant.routine.extra_info->has_ellipsis) {
    pos_error(ec_ellipsis_parameter_for_literal_operator,
              &loc->source_position);
    param_err = TRUE;
  } else if (ptp == NULL) {
    pos_error(ec_no_parameter_for_literal_operator, &loc->source_position);
    param_err = TRUE;
  } else if (is_plain_pointer_type(ptp->type)) {
    a_type_ptr  tp = type_pointed_to(ptp->type),
                size_t_type = integer_type(targ_size_t_int_kind);
    if (get_type_qualifiers(tp) != TQ_CONST) {
      pos_ty_error(ec_pointer_to_nonconst_for_literal_operator,
                   &loc->source_position, skip_typerefs(ptp->type));
      param_err = TRUE;
    } else if (ptp->next == NULL) {
      /* Presumably a raw literal operator. */
      if (is_plain_char_type(tp)) {
        rp->is_raw_literal_operator = TRUE;
      } else {
        pos_ty_error(ec_invalid_parameter_type_for_literal_operator,
                     &loc->source_position, skip_typerefs(ptp->type));
        param_err = TRUE;
      }  /* if */
    } else if (ptp->next->next != NULL) {
      pos_error(ec_too_many_parameters_for_literal_operator,
                &loc->source_position);
      param_err = TRUE;
    } else if (!types_are_compatible(ptp->next->type, size_t_type)) {
      pos_ty_error(ec_invalid_second_parameter_type_for_literal_operator,
                   &loc->source_position, skip_typerefs(ptp->next->type));
      param_err = TRUE;
    } else if (!is_valid_udl_char_parameter_type(tp)) {
      pos_ty_error(ec_invalid_pointer_parameter_for_literal_operator,
                   &loc->source_position, skip_typerefs(ptp->type));
      param_err = TRUE;
    }  /* if */
  } else if (ptp->next != NULL) {
    pos_error(ec_too_many_parameters_for_literal_operator,
              &loc->source_position);
    param_err = TRUE;
  } else {
    a_type_ptr  tp = skip_typerefs(ptp->type);
    if (tp->kind == (a_type_kind)tk_integer) {
      if (tp->variant.integer.enum_type) {
        pos_ty_error(ec_invalid_parameter_type_for_literal_operator,
                     &loc->source_position, tp);
        param_err = TRUE;
      } else if (tp->variant.integer.int_kind != (an_integer_kind)ik_char &&
                 !tp->variant.integer.wchar_t_type &&
                 !tp->variant.integer.char16_t_type &&
                 !tp->variant.integer.char32_t_type &&
                 tp->variant.integer.int_kind !=
                                     (an_integer_kind)ik_unsigned_long_long) {
        pos_ty_error(ec_invalid_integer_parameter_for_literal_operator,
                     &loc->source_position, tp);
        param_err = TRUE;
      }  /* if */
    } else if (tp->kind == (a_type_kind)tk_float) {
      if (tp->variant.float_kind != (a_float_kind)fk_long_double) {
        pos_ty_error(ec_invalid_float_parameter_for_literal_operator,
                     &loc->source_position, tp);
        param_err = TRUE;
      }  /* if */
    } else {
      pos_ty_error(ec_invalid_parameter_type_for_literal_operator,
                   &loc->source_position, tp);
      param_err = TRUE;
    }  /* if */
  }  /* if */
  if (!param_err && ptp != NULL && !gpp_mode &&
      (ptp->has_default_arg ||
       (ptp->next != NULL && ptp->next->has_default_arg))) {
    pos_error(ec_default_arg_for_literal_operator, &loc->source_position);
    param_err = TRUE;
  }  /* if */
}  /* check_udl_operator_type */


#if !(EXTRA_SOURCE_POSITIONS_IN_IL || GENERATE_SOURCE_SEQUENCE_LISTS)
/* ARGSUSED */ /* decl_pos_block is not used in some configurations. */
#endif /* !(EXTRA_SOURCE_POSITIONS_IN_IL || GENERATE_SOURCE_SEQUENCE_LISTS) */
void decl_routine(a_symbol_locator         *locator,
                  a_decl_parse_state       *dps,
                  a_func_info_block_ptr    func_info,
                  a_symbol_reference_kind  srk_flags,
                  an_id_linkage_kind       *linkage_ptr,
                  a_type_ptr               *old_type,
                  a_symbol_ptr             *ext_sym,
                  a_decl_pos_block_ptr     decl_pos_block)
/*
Enter the declaration of an identifier for a nonmember routine.  *locator
gives the symbol locator (and thus its name and its declaration position).
*dps and *func_info describe various properties of the declaration (e.g., its
type, storage class, attributes, etc.).  If func_info->is_implicit_declaration
is TRUE, this declaration is for an implicit function declaration, and dps->sym
already points to the symbol entry, which is already in the symbol table.  If
func_info->is_definition is TRUE, the identifier being defined is part of a
function definition (meaning there is a body in the definition), in which case
it is guaranteed that dps->type points to an unshared type entry, and that type
entry will be preserved as the routine type.  Create and enter a symbol entry,
and return a pointer to it in dps->sym.  Also allocate any associated IL
construct, and attach it to the symbol.  If the identifier has linkage and
there is an existing symbol or IL entry, it will be re-used.  Return in
*linkage_ptr the linkage of the identifier.  Return in *old_type any
previously-known type for this identifier from a linked identifier in the same
scope, or NULL if there was no previously-known type.  If the identifier has
linkage, return in *ext_sym a pointer to the external symbol entry; otherwise,
set *ext_sym to NULL.  srk_flags contain specific information about the kind
of declaration (whether it's a definition, an implicit declaration (C only), a
friend declaration (C++ only), and so forth); this information is passed on
for use in generating cross-reference output describing this declaration.
*/
{
  a_symbol_ptr             sym = NULL;
  a_symbol_ptr             linked_symbol, homonym_symbol = NULL;
  a_symbol_ptr             overload_symbol = NULL;
  a_boolean                redecl_error_already_issued = FALSE;
  a_boolean                linked_redecl_error = FALSE;
  a_boolean                old_decl_has_body = FALSE;
  a_boolean                redeclaration = FALSE;
  a_routine_ptr            routine_ptr = NULL;
  an_id_linkage_kind       linkage;
  a_source_correspondence  *source_corresp_ptr;
  a_scope_depth            effective_decl_level;
  a_scope_depth            saved_depth_innermost_namespace_scope;
  a_boolean                template_function_specific_decl = FALSE;
  a_boolean                explicit_template_reference = FALSE;
  a_boolean                suppress_ext_sym_lookup = FALSE;
  a_boolean                is_function_def = FALSE;
  a_boolean                changed_to_inline = FALSE;
  a_boolean                is_friend_decl;
  a_boolean                set_invisible = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_name_reference_ptr     name_ref = NULL;
  a_boolean                saved_sses_disallowed =
                                           source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  an_id_linkage_block      idlb;
  a_boolean                definition_for_inlining_only = FALSE;
  a_boolean                notify_correspondence_processing = FALSE;
  a_boolean                microsoft_specialization_redef = FALSE;
  a_type_ptr               type_ptr, rtp;
  a_routine_type_supplement_ptr
                           rtsp;
  a_storage_class          storage_class = dps->storage_class;
#if GNU_EXTENSIONS_ALLOWED
  a_type_ptr               orig_type = dps->type;
  a_boolean                use_gnu_c89_inlining = gnu_c89_inlining;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_FUNCTION_MULTIVERSIONING
  a_boolean                requires_gnu_target_attr = FALSE;
#endif /* GNU_FUNCTION_MULTIVERSIONING */
  a_boolean                use_std_c99_inlining = std_c99_inlining;
#if DECL_MODIFIERS_IN_USE || BACK_END_IS_CP_GEN_BE || \
    (GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS)
  a_decl_modifiers_block_ptr
                           decl_modifiers = &dps->decl_modifiers;
#endif /* DECL_MODIFIERS_IN_USE || BACK_END_IS_CP_GEN_BE || ... */
  a_boolean                update_sym_pos = FALSE;

  db_enter(3, "decl_routine");
  *old_type = NULL;
  check_assertion_str(func_info != NULL, "decl_routine: NULL func_info");
  check_assertion_str(storage_class != (a_storage_class)sc_typedef,
                      "decl_routine: bad storage class");
  check_assertion_str(srk_flags & SRK_DECLARATION,
                      "decl_routine: missing SRK_DECLARATION");
  check_assertion_str(is_function_type(dps->type),
                      "decl_routine: not a routine type");
  if (func_info->is_definition) {
    is_function_def = TRUE;
    dps->is_definition = TRUE;
    check_assertion_str(srk_flags & SRK_DEFINITION,
                        "decl_routine: missing SRK_DEFINITION");
  }  /* if */
  /* If this is a function declared through a typedef, we must use a copy of
     the underlying type if that type will be modified. */
  remove_routine_typedef_if_needed(locator, dps, /*no_cv_quals=*/TRUE);
  type_ptr = dps->type;
  rtp = skip_typerefs(type_ptr);
  rtsp = rtp->variant.routine.extra_info;
  if (!C_mode()) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* When the declared_type was created (in declarator), the default args
       were ignored.  If appropriate, copy them from dps->type to the
       declared_type now (i.e., before composite_type is called). */
    if (!is_function_def && source_sequence_entries_disallowed) {
      /* The declared_type is not used. */
    } else if (same_entities(func_info->declared_type, dps->type)) {
      /* No fixup required.  (This can happen when type_ptr is a typedef.) */
    } else if (func_info->declared_type != NULL &&
               skip_typerefs(func_info->declared_type)->
                                variant.routine.extra_info->prototyped) {
      copy_routine_type_default_args(type_ptr, func_info->declared_type);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if CHECKING
    if (!C_mode() && func_info->is_inline && !extern_inline_allowed) {
      check_assertion_str(storage_class == (a_storage_class)sc_unspecified ||
                          storage_class == (a_storage_class)sc_static,
                          "decl_routine: bad storage class for inline");
    }  /* if */
#endif /* CHECKING */
    if (implicit_noexcept_enabled && locator->is_operator_name &&
        rtsp->exception_specification == NULL &&
        is_delete_operator(locator->variant.opname)) {
      /* A delete operator without an explicit exception specification is
         treated as if declared "noexcept". */
      add_noexcept_specification(rtsp);
    }  /* if */
    /* If this is an overloaded operator, check for errors in the
       argument list. */
    check_operator_function_params(type_ptr, /*class_type=*/(a_type_ptr)NULL,
                                   locator);
    report_bad_new_or_delete(locator, dps);
  } else {
    /* C mode. */
    if (strict_ansi_mode) {
      /* CV-qualified void return types aren't permitted in C mode. */
      a_type_ptr  return_type = rtp->variant.routine.return_type;
      if (return_type->kind == (a_type_kind)tk_typeref &&
          is_qualified_type(return_type) &&
          is_void_type(return_type)) {
        pos_error(ec_qualified_void_return_type, &locator->source_position);
      }  /* if */
    }  /* if */
  }  /* if */
  clear_id_linkage_block(&idlb);
  idlb.locator = locator;
  idlb.storage_class = storage_class;
  idlb.type = type_ptr;
  idlb.func_info = func_info;
  idlb.is_definition = is_function_def;
  set_linkage_environment(&idlb, decl_scope_level);
  check_assertion(idlb.is_friend_decl == ((srk_flags & SRK_FRIEND) != 0) ||
                  is_or_contains_error_type(type_ptr));
  idlb.is_friend_decl = ((srk_flags & SRK_FRIEND) != 0);
  is_friend_decl = idlb.is_friend_decl;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (locator->is_template_id && locator->specific_symbol == NULL) {
      /* If this is a template-id for which the symbol has not yet been
         found, look it up now. */
      (void)normal_id_lookup(locator, IDL_NO_OPTIONS);
    }  /* if */
    check_and_adjust_calling_convention(locator, func_info, type_ptr);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (func_info->is_implicit_declaration) {
    check_assertion_str(srk_flags & SRK_IMPLICIT,
                        "decl_routine: missing SRK_IMPLICIT");
    if (C_mode()) {
      /* For an implicit function, the identifier would not be in the process
         of being declared implicitly as a function if there were any visible
         declaration of it, and therefore it must have external linkage. */
      idlb.linkage = linkage = idl_external;
      compute_name_linkage(&idlb);
    } else {
      /* In C++ this is an error case.  Don't give this dummy routine any
         linkage. */
      linkage = idl_none;
    }  /* if */
    linked_symbol = NULL;
    sym = dps->sym;
    effective_decl_level = DEPTH_OF_FILE_SCOPE;
  } else {
    if (!C_mode() && locator->specific_symbol != NULL &&
        (qualifier_namespace_ptr(*locator) != NULL ||
         locator->is_file_scope_qualified_name ||
         locator->is_template_id)) {
      /* This identifier is a namespace-qualified name that was previously
         declared, or else a file-scope qualified name (friend declarations
         only).  Do the appropriate checking, including overload resolution.
         Furthermore, for definitions of namespace-qualified names, be sure
         this is a valid scope for the definition (7.3.1.4). */
      /* Look up the name. */
      qualified_name_redecl_sym(&idlb);
    } else {
      /* Determine the linkage of this symbol. */
      id_linkage(&idlb, dps);
    }  /* if */
    linkage = idlb.linkage;
    linked_symbol = idlb.linked_symbol;
    homonym_symbol = idlb.homonym_symbol;
    overload_symbol = idlb.overload_symbol;
    effective_decl_level = idlb.effective_decl_level;
    storage_class = idlb.storage_class;
    if (idlb.is_new_template_instance) {
      /* This declaration triggered the creation of a new template instance. */
      dps->first_decl = TRUE;
    }  /* if */
#if GNU_FUNCTION_MULTIVERSIONING
    if (linked_symbol != NULL && is_function_def &&
        linked_symbol->kind == (a_symbol_kind)sk_routine &&
        is_multiversion_representative(linked_symbol->variant.routine.ptr)) {
      /* This is a GNU multiversion function; a "target" attribute is
         required if this is not a redefinition. */
      a_routine_list_entry_ptr rlep =
        gnu_routine_supp(linked_symbol->variant.routine.ptr)->
                                      mv_info.representative.targeted_versions;
      check_assertion(rlep != NULL);
      if (rlep->next != NULL) {
        /* At least two target-specific routines have been declared; GNU
           requires that a subsequent definition have a "target" attribute
           in this case. */
        requires_gnu_target_attr = TRUE;
      } else if (routine_has_been_defined(rlep->routine)) {
        /* One target-specific routine has already been defined. */
        requires_gnu_target_attr = TRUE;
      }  /* if */
    }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
  }  /* if */
  if (gpp_mode && idlb.is_block_extern_decl &&
      depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
    /* In GNU C++ mode, the "innermost namespace scope" considered for block-
       extern declarations should ignore namespace extension scopes that don't
       correspond to actual namespace extension declarations (instead, they
       are the result of "reactivating" the namespace).  E.g.:
          namespace N { struct S { void f(); }; }
          void N::S::f() {
            void g();  // ::g in g++ mode, N::g otherwise.
          }
    */
    saved_depth_innermost_namespace_scope = depth_innermost_namespace_scope;
    depth_innermost_namespace_scope =
                                    get_effective_depth_innermost_namespace();
  } else {
    saved_depth_innermost_namespace_scope = NO_SCOPE_DEPTH;
  }  /* if */
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked. */
    if (linked_symbol->kind == (a_symbol_kind)sk_routine &&
        linked_symbol->variant.routine.instance_ptr != NULL) {
      if (locator->is_template_id ||
          (locator->is_qualified_name && is_friend_decl)) {
        /* This is either a friend declaration that nominates an instance of a
           previously declared template, or it is an old-style specialization
           with explicit template arguments. */
        explicit_template_reference = TRUE;
      } else if (!locator->is_qualified_name) {
        /* This is not actually a redeclaration -- linked_symbol refers to a
           function template instantiation. */
        check_assertion(guiding_decls_allowed || microsoft_mode);
        template_function_specific_decl = TRUE;
      }  /* if */
    }  /* if */
    if (!explicit_template_reference && !template_function_specific_decl) {
      /* The new declaration must be compatible with the old. */
      redeclaration = TRUE;
    }  /* if */
#if CHECKING
    if (linked_symbol->overload_set_member && !is_friend_decl) {
      /* Normally we should have a record of which overload set the symbol
         belongs to.  The exception occurs when redeclaring an entity that
         is visible only through a using-declaration in Sun or Microsoft 
         mode. */
      check_assertion((redeclaration && (microsoft_mode || sun_mode) &&
                       scope_stack[decl_scope_level].number !=
                                                 linked_symbol->decl_scope) ||
                      (overload_symbol != NULL &&
                       overload_symbol->kind ==
                                      (a_symbol_kind)sk_overloaded_function));
    }  /* if */
#endif /* CHECKING */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && microsoft_version >= 1400 &&
        locator->is_operator_name &&
        is_implicit_array_new_or_delete_symbol(linked_symbol)) {
      /* Microsoft C++ 8 treats the implicitly declared array new and delete
         operators as aliases for the non-array versions, but the user-defined
         array new and delete operators are distinct routines.  If this is a
         user-defined declaration of an array new or delete operator, we
         therefore remove the predeclared symbol and force the creation of a
         new one. */
      redeclaration = FALSE;
      if (overload_symbol != NULL) {
        remove_symbol_from_overload_set(linked_symbol, overload_symbol);
      } else {
        remove_symbol(linked_symbol);
      }  /* if */
      idlb.linked_symbol = linked_symbol = NULL;
    }  /* if */ 
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  /* In GNU C mode, we must decide whether the semantics of "inline" are the
     standard C99 semantics, or the older GNU C89 semantics.  Unfortunately,
     that may depend on whether the current declaration has the "gnu_inline"
     attribute.  So check_gnu_inline_attribute may have to look through the
     list of attributes specified on the current declaration (before they are
     applied to the function).  This also matters in GNU C++ mode, where the
     "gnu_inline" attribute means that the definition can be displaced by a
     subsequent definition. */
  if (gnu_mode && check_gnu_inline_attribute(dps, &idlb, redeclaration)) {
    use_std_c99_inlining = FALSE;
    use_gnu_c89_inlining = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (use_std_c99_inlining) {
    /* In C99 mode, if a function is declared "inline" every time it is
       declared in a given translation unit and is never declared with an
       explicitly specified storage class, then its definition is regarded as
       an "inline definition" instead of an "external definition" (see 6.9,
       6.7.4).  An inline function with an "inline definition", even though it
       has external linkage, is not visible outside the current translation
       unit.  (This does not apply to block-extern declarations.)  Note that
       early GNU C99 modes do not adhere to these rules. */
    check_assertion(c99_mode);
    if (func_info->is_inline && !idlb.is_block_extern_decl &&
        dps->declared_storage_class == (a_storage_class)sc_unspecified) {
      /* "inline" was present in the declaration, but no storage class was
         specified. */
      definition_for_inlining_only = TRUE;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (use_gnu_c89_inlining &&
             (gpp_mode ||
              (gcc_mode &&
               dps->declared_storage_class == (a_storage_class)sc_extern)) &&
               func_info->is_inline && func_info->is_definition) {
    /* In GNU C89 mode, if a function definition uses both the "extern" and
       "inline" keywords then no definition of the function should be emitted,
       even though it has external linkage.  This treatment is analogous to
       the C99 "inline definition" concept.  GNU C99 follows the standard C99
       rules only when gnu_version is at least 40300, except when the
       gnu_inline attribute was specified.  GNU C++ follows standard C++ rules
       except when the gnu_inline attribute was specified: If the latter is
       TRUE, no definition should be emitted independently of the presence of
       an "extern" keyword. */
    definition_for_inlining_only = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (redeclaration) {
    if (linked_symbol->kind == (a_symbol_kind)sk_routine) {
      /* Linked symbol and new symbol are both routines.  The new declaration
         must be compatible with the old. */
      a_boolean  replace_routine = FALSE;
      sym = linked_symbol;
      routine_ptr = linked_symbol->variant.routine.ptr;
      dps->prev_type = routine_ptr->type;
      check_assertion_str(routine_ptr != NULL,
                          "decl_routine: linked symbol routine is missing");
#if GNU_FUNCTION_MULTIVERSIONING
      if (is_multiversion_representative(routine_ptr)) {
        /* The current routine will not be recorded in the symbol table;
           instead, we'll hang the routine on the multiversion list. */
      } else
#endif /* GNU_FUNCTION_MULTIVERSIONING */
      /* Do not insert code here. */
      {
        if (routine_has_been_defined(routine_ptr)
#if ASM_FUNCTION_ALLOWED
            || routine_ptr->storage_class == (a_storage_class)sc_asm
#endif /* ASM_FUNCTION_ALLOWED */
                                                                    ) {
          /* The previous declaration was a definition.  (We check assoc_scope
             rather than the defined flag in the routine, because in pcc mode
             it is possible to have a nested redeclaration -- e.g.,
               int f() { int f(); ... };
             -- but the flag isn't set till the definition is complete.) */
          old_decl_has_body = TRUE;
        } else if (sym->defined) {
          /* In C++ the defined flag in the symbol may have been set without
             the body having been scanned and bound to the routine yet (e.g.,
             inline friend function or a dllimport function). */
#if MICROSOFT_EXTENSIONS_ALLOWED
          check_assertion_str((routine_ptr->decl_modifiers & DM_DLLIMPORT) ||
                              routine_ptr->defined_in_friend_decl ||
                              scope_stack[decl_scope_level].kind ==
                                          (a_scope_kind)sck_class_struct_union,
                              "decl_routine: defined flag is set wrong");
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
          check_assertion_str(routine_ptr->defined_in_friend_decl ||
                              scope_stack[decl_scope_level].kind ==
                                          (a_scope_kind)sck_class_struct_union,
                              "decl_routine: defined flag is set wrong");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          old_decl_has_body = TRUE;
        }  /* if */
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      if (use_gnu_c89_inlining && old_decl_has_body &&
          routine_ptr->definition_for_inlining_only &&
          ((gpp_mode && !func_info->is_inline) ||
           (gcc_mode && 
            (!func_info->is_inline ||
             dps->declared_storage_class != (a_storage_class)sc_extern)))) {
        replace_routine = TRUE;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (is_function_def && old_decl_has_body && !replace_routine) {
        /* Previous routine already has a body, and new one does (or will)
           too. */
        pos_sy_error(ec_already_defined, &locator->source_position, sym);
        redecl_error_already_issued = TRUE;
        linked_redecl_error = TRUE;
        /* Set a flag to suppress reuse of the existing external-routine
           symbol and of the routine already in use.  Also, to suppress a
           possible declared-but-not-used message, set the referenced flag
           in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        mark_symbol_to_suppress_warnings(linked_symbol);
        set_to_named_error_locator(*locator);
      } else {
        /* Check that the routine types are compatible. */
        a_boolean         routines_compat = TRUE;
        an_error_code     error_code = ec_not_compatible_with_previous_decl;
        a_param_type_ptr  params = skip_typerefs(routine_ptr->type)
                                ->variant.routine.extra_info->param_type_list;
        /* Friend functions that name an existing declaration should not
           introduce default arguments.  Such default arguments are accepted,
           however, in GNU C++ mode or when friend name injection is
           enabled. */
        if (!(friend_function_injection_enabled || gpp_mode ) &&
            is_friend_decl && func_info->any_default_args) {
          pos_diagnostic(es_discretionary_error,
                         ec_friend_cannot_add_default_arguments,
                         &locator->source_position);
        }  /* if */
        if (strict_ansi_mode && routine_ptr->befriending_classes != NULL &&
            old_decl_has_body && linked_symbol->is_invisible) {
          /* In strict mode, friend declarations can have default arguments
             only if the friend declaration is also a definition (checked
             elsewhere), and that definition must be the only declaration of
             the function.  Since the routine already has a befriending class
             and its symbol is invisible, it has only been declared as a
             friend so far (through perhaps more than one friend declaration).
             Check if a previous declaration had default arguments. */
          a_param_type_ptr  ptp = params;
          for (; ptp != NULL; ptp = ptp->next) {
            if (ptp->has_default_arg) {
              pos_sy_diagnostic(strict_ansi_error_severity,
                                ec_redeclaration_of_friend_with_default_args,
                                &locator->source_position, linked_symbol);
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        /* For routines that can be overloaded, id_linkage has already
           checked that the routine types are compatible.  "main" cannot
           be overloaded, so it was not checked. */
        if ((C_mode() || func_info->is_main_function) &&
            !f_types_are_compatible(routine_ptr->type, type_ptr,
                                    TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |
                                    TCF_IGNORE_CALLING_CONVENTIONS)) {
          /* Error -- redeclaration requires type compatibility. */
          routines_compat = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
        } else if (microsoft_mode &&
                   !calling_conventions_are_compatible(routine_ptr->type,
                                                       type_ptr)) {
          /* Error -- calling conventions are not compatible.  (The GNU mode
             test is delayed until attributes are applied.) */
          routines_compat = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
        } else if (!C_mode() && !func_info->is_main_function &&
                   !routine_name_linkages_are_compatible(routine_ptr->type,
                                                         type_ptr)) {
          /* Error -- routine-name-linkages are not compatible. */
          routines_compat = FALSE;
          error_code = ec_incompatible_linkage_specifier;
        } else if (routine_ptr->has_deducible_return_type !=
                                             dps->has_deducible_return_type) {
          /* One declaration used "auto" or "decltype(auto)" as a deduced
             return type and the other did not. */
          routines_compat = FALSE;
        }  /* if */
        if (!routines_compat) {
          /* The old and new declarations are incompatible.  There is special
             handling for SVR4, Microsoft C, and GNU C compatibility modes. */
          check_incompatible_routine_redecl(linked_symbol, type_ptr,
                                            old_decl_has_body, is_function_def,
                                            error_code,
                                            &locator->source_position,
                                            old_type, &linked_redecl_error,
                                            &suppress_ext_sym_lookup);
          redecl_error_already_issued = TRUE;
        } else {
          /* The declarations are compatible.  Form the composite type. */
          *old_type = routine_ptr->type;
          if (C_dialect == C_dialect_cplusplus) {
            if (strict_ansi_mode && idlb.is_local_class_friend_decl) {
              /* A local class friend declaration must refer to a function
                 declared within the immediately enclosing non-class scope. */
              a_symbol_ptr  prior_decl = idlb.prior_decl_in_enclosing_scope;

              if (prior_decl != NULL &&
                  prior_decl->decl_scope !=
                           scope_stack[effective_decl_level].number) {
                /* There was a prior declaration, but it wasn't in the
                   innermost enclosing non-class scope. */
                pos_diagnostic(strict_ansi_discretionary_severity,
                               ec_local_class_friend_requires_prior_decl,
                               &locator->source_position);
              }  /* if */
            }  /* if */
            /* Do compatibility checking on the throw specification. */
            check_exception_specification(type_ptr, linked_symbol,
                                          &func_info->throw_position,
                                          /*is_redecl=*/TRUE);
          }  /* if */
          if (replace_routine && is_function_def &&
              !skip_typerefs(type_ptr)->variant.routine.extra_info
                                      ->prototyped &&
              !skip_typerefs(routine_ptr->type)->variant.routine.extra_info
                                      ->prototyped) {
            /* Don't attempt to reconcile the types of unprototyped function
               definitions when we are going to discard the earlier one.  (GCC
               accepts very different types in such cases, and reconciling the
               types could result in the composite type not matching the
               param-id list). */
            /* Record the new type in the routine type: It will be copied over
               to the new routine entry, and the original type will be restored
               after that. */
            routine_ptr->type = type_ptr;
          } else {
            reconcile_routine_types(
                                  routine_ptr, type_ptr,
                                  /*preserve_rout_type=*/(old_decl_has_body &&
                                                          !replace_routine),
                                  /*preserve_type_ptr=*/(is_function_def ||
                                                         replace_routine),
                                  dps);
          }  /* if */
          if (gpp_mode && params != NULL && !old_decl_has_body &&
              !is_function_def &&
              routine_ptr->type->kind == (a_type_kind)tk_routine &&
              seq_is_in_system_header(sym->decl_position.seq)) {
            /* A redeclaration of a routine first declared in a system header
               and no definition has yet been seen.  GNU compilers retain the
               later exception specifications, but (strangely) only if the
               routine takes at least one parameter. */
            routine_ptr->type->variant.routine.extra_info
                       ->exception_specification =
                                              rtsp->exception_specification;
            /* The new declaration's position is treated as the primary
               position (and may no longer be in a system header). */
            routine_ptr->source_corresp.decl_position =
                                                     locator->source_position;
            update_sym_pos = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (routine_ptr->is_constexpr !=
                                    ((dps->dso_flags & DSO_CONSTEXPR) != 0)) {
        /* The previous declaration doesn't match the current one wrt. the
           "constexpr" specifier.  Issue an error. */
        pos_sy_error(routine_ptr->is_constexpr ?
                       ec_previous_constexpr_decl_conflict :
                       ec_previous_nonconstexpr_decl_conflict,
                     routine_ptr->is_constexpr ? &dps->declarator_pos
                                               : &dps->constexpr_pos,
                     linked_symbol);
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      if (replace_routine) {
        /* In GNU mode a function can be defined "for inlining purposes only"
           ("extern __inline" in GNU C mode, or "__attribute((gnu_inline))" in
           GNU C++ mode).  Such a definition is superseded by the current
           declaration if the current declaration does not imply the "for
           inlining purposes only" semantics.  To emulate this, we create
           a new routine entry (the old one remains in the IL tree to satisfy
           any existing references to it). */
        a_routine_ptr  new_rp = make_routine(type_ptr, storage_class,
                                             decl_scope_level);
        *new_rp = *routine_ptr;
        /* Restore the original type for the original routine entry (it may
           have been changed by the call to reconcile_routine_types). */
        routine_ptr->type = *old_type;
        new_rp->next = NULL;
        new_rp->gnu_extra_info = NULL;
        new_rp->source_corresp.decl_position = locator->source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        new_rp->source_corresp.decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        new_rp->source_corresp.name_references = NULL;
        new_rp->defined = FALSE;
        new_rp->assoc_scope = NULL_region_number;
        ensure_gnu_routine_supp(new_rp)->inline_partner = routine_ptr;
        ensure_gnu_routine_supp(routine_ptr)->inline_partner = new_rp;
        routine_ptr = new_rp;
        routine_ptr->gnu_c89_inline = FALSE;
        old_decl_has_body = FALSE;
        set_inline_flag(routine_ptr, FALSE);
        sym->defined = FALSE;
        sym->variant.routine.ptr = routine_ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* A new source sequence entry is needed for the new declaration, and
           the declared type will be reset as well. */
        routine_ptr->declared_type = NULL;
        routine_ptr->source_corresp.source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    } else {
      /* The linked symbol must be a variable. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, linked_symbol);
      redecl_error_already_issued = TRUE;
      linked_redecl_error = TRUE;
    }  /* if */
  } else {
    /* Not a redeclaration. */
    a_symbol_ptr  symbol_for_overloading = NULL;
    if (C_dialect == C_dialect_cplusplus) {
      /* Be sure the default arguments, if any, are at the end of the
         parameters list.  Also check some C++/CLI constraints on default
         arguments. */
      check_default_args(dps);
      if (is_friend_decl && !friend_function_injection_enabled) {
        set_invisible = TRUE;
      }  /* if */
      symbol_for_overloading = overload_symbol == NULL ? homonym_symbol :
                                                         overload_symbol;
      if (homonym_symbol != NULL &&
          homonym_symbol->kind != (a_symbol_kind)sk_function_template) {
        /* homonym_symbol is a previously declared routine symbol with the
           same name but a different type signature from that of the current
           declaration.  We may have an instance of function overloading. */
        an_error_code  error_code;

        if (!overload_distinguishable(homonym_symbol, type_ptr,
                                      (a_template_param_ptr)NULL,
                                      &error_code)) {
          /* The previous declaration and the current one are not "overload
             distinguishable" for a reason given by the error code returned. */
          pos_error(error_code, &locator->source_position);
          redecl_error_already_issued = TRUE;
          /* We can't add a symbol to the overload list, so change the locator
             to an error locator to prevent hiding the overload symbol when
             the new symbol is entered. */
          set_to_error_locator(*locator);
          /* Don't treat this as a template function specific declaration
             even if it was previously thought to be.  Do treat it as a
             redeclaration error. */
          template_function_specific_decl = FALSE;
          linked_redecl_error = TRUE;
          goto skip_overloading;
        }  /* if */
      }  /* if */
      if (is_friend_decl) {
        a_symbol_ptr  prior_decl = idlb.prior_decl_in_enclosing_scope;

        if (prior_decl != NULL) {
          if (!is_function_symbol(fundamental_symbol_of(prior_decl))) {
            /* Issue an error for a case like this:
                 int x;
                 struct S { friend x(); }    // Incompatible decl
            */
            pos_sy_error(ec_decl_incompatible_with_previous_use,
                         &locator->source_position, prior_decl);
            redecl_error_already_issued = TRUE;
          }  /* if */
        } else {
          if (strict_ansi_mode && idlb.is_local_class_friend_decl) {
            /* A local class friend declaration requires a prior declaration
               in the scope that encloses the class definition. */
            pos_diagnostic(strict_ansi_discretionary_severity,
                           ec_local_class_friend_requires_prior_decl,
                           &locator->source_position);
          }  /* if */
        }  /* if */
      } else if (!C_mode() && idlb.is_block_extern_decl &&
                 idlb.prior_decl_in_enclosing_scope != NULL) {
        /* Be sure to transfer the name linkage ("C"/"C++") to a block extern
           declaration of a function if a prior declaration was visible. */
        a_symbol_ptr  prior_decl =
                    fundamental_symbol_of(idlb.prior_decl_in_enclosing_scope);
        if (is_function_symbol(prior_decl)) {
          a_type_ptr  prior_type = routine_symbol_type(prior_decl);
          if (routine_types_are_redecl_compatible(prior_type, type_ptr,
                                                  TCF_NO_FLAGS) &&
              type_ptr->kind != (a_type_kind)tk_typeref) {
            type_ptr->variant.routine.extra_info->routine_name_linkage =
                 prior_type->variant.routine.extra_info->routine_name_linkage;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (template_function_specific_decl && !inside_local_class &&
        depth_innermost_function_scope == NO_SCOPE_DEPTH) {
      /* This is an explicit declaration of a template function.  Note that
         we are only interested in file- and namespace-scope declarations --
         declarations at local scope are handled separately. */
      sym = linked_symbol;
      routine_ptr = sym->variant.routine.ptr;
      if (routine_has_been_defined(routine_ptr)) {
        old_decl_has_body = TRUE;
      } else if (sym->defined) {
        /* In C++ the defined flag may have been set without the body having
           been scanned and bound to the routine yet (e.g., inline friend
           function). */
        check_assertion_str(scope_stack[decl_scope_level].kind ==
                                        (a_scope_kind)sck_class_struct_union,
                            "decl_routine: defined flag is set wrong");
        old_decl_has_body = TRUE;
      }  /* if */
      if (is_function_def || (microsoft_mode && !is_friend_decl)) {
        /* This is normally a definition (an old-style specialization). */
        /* In Microsoft mode it need not be a definition.  Consider:
             template <class T> void f(T t) { ... }
             void f(int);
           Function f(int) is marked as a specialization, and it is expected
           that the definition will be provided elsewhere (rather than
           generated from the template) -- i.e., the second line appears to
           have the same meaning as if it were written:
             template<> void f(int);
        */
        check_old_specialization_allowed(sym, &locator->source_position);
        if (!old_decl_has_body) {
          /* Okay. */
          /* Update the linkage information in the routine to reflect
             this declaration instead of the information inherited from
             the template. */
          routine_ptr->storage_class = storage_class;
          if (func_info->is_inline && !routine_ptr->is_inline) {
            changed_to_inline = TRUE;
          }  /* if */
          set_inline_flag(routine_ptr, (a_boolean)func_info->is_inline);
          routine_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_static) ?
                                (a_name_linkage_kind)nlk_internal :
                                (a_name_linkage_kind)nlk_cplusplus_external;
          routine_ptr->is_specialized = TRUE;
          routine_ptr->specialized_with_old_syntax = TRUE;
        } else if (is_function_def) {
          /* There is already a definition.  This is some sort of error. */
          linked_redecl_error = TRUE;
          template_function_specific_decl = FALSE;
          redecl_error_already_issued = TRUE;
          if (!routine_ptr->is_specialized &&
              routine_ptr->is_inline && routine_ptr->called) {
            /* An inline function template has been declared, an instance
               of it has been referenced and therefore instantiated on
               the fly, and now a specializing declaration appears.
               Issue an error (you can't reference an inline template
               function that is specialized before the specialization is
               declared) and treat it as a redeclaration error. */
            pos_error(ec_specialization_of_called_inline_template_function,
                      &locator->source_position);
          } else {
            /* Already defined, presumably by a specialization. */
            if (microsoft_bugs && microsoft_version == 1200 &&
                routine_ptr->is_specialized && !is_friend_decl) {
              /* Microsoft Visual C++ 6.0 ignores redefinitions of
                 specializations.  We need to scan the upcoming definition,
                 but it must be discarded and the old body preserved.
                 We will therefore create a new symbol and IL entry, but
                 discard them after the function body has been scanned.
                 We also prevent source sequence entries from being generated
                 for the temporary entry. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
              source_sequence_entries_disallowed = TRUE;
              if (dps->source_sequence_entry != NULL) {
                f_remove_from_src_seq_list(dps->source_sequence_entry,
                                           decl_scope_level);
                dps->source_sequence_entry = NULL;
              }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
              microsoft_specialization_redef = TRUE;
              pos_sy_warning(ec_already_defined, &locator->source_position,
                             sym);
              goto skip_overloading;
            } else {
              pos_sy_error(ec_already_defined, &locator->source_position, sym);
            }  /* if */
          }  /* if */
          set_to_named_error_locator(*locator);
          /* Set a flag to suppress reuse of the existing external-routine
             symbol and of the routine already in use.  Also, to suppress a
             possible declared-but-not-used message, set the referenced flag
             in the linked symbol. */
          suppress_ext_sym_lookup = TRUE;
          mark_symbol_to_suppress_warnings(linked_symbol);
        }  /* if */
      }  /* if */
      if (!linked_redecl_error) {
        a_boolean  preserve_rout_type, preserve_type_ptr;
        if (!sym->variant.routine.instance_ptr->is_guiding_decl &&
            symbol_for_overloading != NULL) {
          a_namespace_ptr  parent_nsp = sym_parent_namespace_or_null(sym);
          a_boolean	   use_namespace = (parent_nsp != NULL);
          check_assertion_str(parent_nsp == sym_parent_namespace_or_null(
                                                      symbol_for_overloading),
                             "decl_routine: namespace mismatch");
          /*  Its symbol is already on the template's function instantiation
              list, but it needs to be added to the overload list as well,
              to assure that it will be found by the ordinary overload
              resolution algorithm. */
          overload_symbol = 
                    add_symbol_to_overload_list(sym, symbol_for_overloading,
                                                use_namespace, parent_nsp);
          sym->variant.routine.instance_ptr->is_guiding_decl = TRUE;
        }  /* if */
        *old_type = routine_ptr->type;
        if (C_dialect == C_dialect_cplusplus) {
          /* Do compatibility checking on the throw specification. */
          check_exception_specification(type_ptr, linked_symbol,
                                        &func_info->throw_position,
                                        /*is_redecl=*/TRUE);
        }  /* if */
        /* A guiding declaration should generally not impose the recorded type
           of an instance or specialization since the guiding declaration may
           be declared using a typedef.  We therefore usually use the type
           derived from the template declaration, even if the guiding
           declaration is simultaneously an old-style specialization.
           However, if the old-style specialization is also a definition, the
           specialization's type must be used to ensure we preserve the
           declared parameter types. */
        preserve_rout_type = !is_function_def;
        preserve_type_ptr = is_function_def;
        reconcile_routine_types(routine_ptr, type_ptr,
                                preserve_rout_type, preserve_type_ptr, dps);
      }  /* if */
      dps->first_decl = TRUE;
    } else if (explicit_template_reference) {
      /* A reference to a template instance in a friend declaration or an
         old-style specialization.  Such a declaration cannot be a definition
         unless we're in Microsoft mode (and for the friend declaration case,
         only Microsoft Visual C++ 7 accepts a definition). */
      check_assertion(linked_symbol != NULL);
      sym = linked_symbol;
      routine_ptr = sym->variant.routine.ptr;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (routine_ptr->is_generic_instance) {
        /* C++/CLI generics cannot be explicitly specialized. */
        pos_error(ec_invalid_generic_specialization,
                  &locator->source_position);
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      if (is_function_def &&
          !(microsoft_mode &&
            (!is_friend_decl || microsoft_version == 1300))) {
        pos_sy_error(ec_old_specialization_not_allowed,
                     &locator->source_position, sym);
        /* Set a flag to suppress reuse of the existing external-routine
           symbol and of the routine already in use.  Also, to suppress a
           possible declared-but-not-used message, set the referenced flag
           in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        mark_symbol_to_suppress_warnings(linked_symbol);
        set_to_named_error_locator(*locator);
      } else if (func_info->is_inline &&
                 (!is_friend_decl || !microsoft_mode)) {
        /* A declaration that is an explicit reference of a template cannot
           include the inline specifier. */
        pos_diagnostic(strict_ansi_discretionary_severity,
                       ec_inline_not_allowed, &locator->source_position);
      }  /* if */
      if (!locator->is_template_id) {
        /* If the declarator was not specified using an explicit template
           argument list, check for the presence of default arguments, which
           are not permitted in template instance declarations. */
        check_for_any_default_args(type_ptr);
      }  /* if */
      /* Do compatibility checking on the throw specification. */
      check_exception_specification(type_ptr, linked_symbol,
                                    &func_info->throw_position,
                                    /*is_redecl=*/TRUE);
      if (!is_friend_decl && !is_error_locator(*locator)) {
        /* This is an old-style specialization (using explicit template
           arguments). */
        /* Update the linkage information in the routine to reflect
           this declaration instead of the information inherited from
           the template. */
        routine_ptr->storage_class = storage_class;
        if (func_info->is_inline && !routine_ptr->is_inline) {
          changed_to_inline = TRUE;
        }  /* if */
        set_inline_flag(routine_ptr, (a_boolean)func_info->is_inline);
        routine_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_static) ?
                                (a_name_linkage_kind)nlk_internal :
                                (a_name_linkage_kind)nlk_cplusplus_external;
        routine_ptr->is_specialized = TRUE;
        routine_ptr->specialized_with_old_syntax = TRUE;
        check_assertion(!routine_ptr->defined);
        if (microsoft_mode && microsoft_version >= 1310) {
          /* More recent Microsoft compilers do not accept an old-style
             specialization, except (curiously) if a point of instantiation
             was encountered before. */
          a_template_instance_ptr  tip = sym->variant.routine.instance_ptr;
          if (tip != NULL && !tip->instance_sym->referenced) {
            pos_diagnostic(es_discretionary_error,
                           ec_explicit_template_args_not_allowed,
                           &locator->source_position);
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (symbol_for_overloading != NULL) {
      /* Overloaded function.  Create the new symbol, which will be on the
         list of functions connected to an sk_overloaded symbol. */
      sym = record_overload(locator, /*is_template=*/FALSE,
                            symbol_for_overloading, &overload_symbol,
                            set_invisible, idlb.is_friend_decl);
    }  /* if */
skip_overloading:;
  }  /* if */
  if (linked_redecl_error || microsoft_specialization_redef) {
    /* There is a linked symbol, but it is not compatible with the new
       declaration or the new declaration is a redefinition.  Force a
       new symbol and a new IL entry. */
    sym = NULL;
    linked_symbol = idlb.linked_symbol = NULL;
    routine_ptr = NULL;
    *old_type = NULL;
    redeclaration = FALSE;
  }  /* if */
  if (sym == NULL) {
    /* There is no (compatible) symbol, so enter one now. */
    sym = enter_local_symbol((a_symbol_kind)sk_routine, locator,
                             effective_decl_level,
                             redecl_error_already_issued);
#if RECORD_HIDDEN_NAMES_IN_IL
    /* Block extern declarations have associated hidden name entries; so we
       must make sure there is an IL scope to attach those entries to. */
      if (scope_stack[effective_decl_level].kind == (a_scope_kind)sck_block &&
          scope_stack[effective_decl_level].il_scope == NULL &&
          linkage != idl_none) {
        (void)ensure_il_scope_exists(&scope_stack[effective_decl_level]);
      }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    if (microsoft_mode && microsoft_specialization_redef) {
      /* Duplicate specialization definitions should not be kept in the symbol
         table. */
      remove_symbol(sym);
    }  /* if */
    /* Mark friend functions for which this is the initial declaration. */
    if (set_invisible) sym->is_invisible = TRUE;
  } else {
    /* Note: if this is an implicit declaration of a function, the symbol has
       already been entered and marked as declared. */
    /* If appropriate, clear the is_invisible flag in the linked symbol and
       in the symbol representing its overload set. */
    if (sym->is_invisible && !is_friend_decl) {
      sym->is_invisible = FALSE;
      if (sym->overload_set_member) {
        check_assertion(overload_symbol != NULL);
        overload_symbol->is_invisible = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* In Microsoft mode, an operator function that is defined in a friend
     declaration (and not declared elsewhere) is not visible when
     referenced using operator notation. */
  sym->is_microsoft_invisible_operator =
                          microsoft_mode && !redeclaration && is_friend_decl &&
                          func_info->is_definition;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  *ext_sym = NULL;
  if (linkage != idl_none && !redeclaration &&
      !(scope_stack[depth_scope_stack].in_prototype_instantiation &&
        is_template_dependent_type(type_ptr))) {
    /* Create an external symbol for the present linkable declaration. */
    *ext_sym = create_external_symbol_for_routine(
                   locator, dps, type_ptr, &idlb,
                   microsoft_specialization_redef, redecl_error_already_issued,
                   suppress_ext_sym_lookup, &routine_ptr);
  }  /* if */
  if (template_function_specific_decl && sym != linked_symbol) {
    /* This is a declaration of a template function at the local scope.
       A function instantiation entry with an associated symbol and routine
       entry already exist.  Be sure this local symbol is properly bound
       to the file-scope entities to which it corresponds. */
    if (routine_ptr != NULL &&
        routine_ptr != linked_symbol->variant.routine.ptr) {
      /* It must be that this routine was mentioned in a block-extern
         declaration before the function template declaration was seen. */
      check_assertion_str2(*ext_sym != NULL &&
                           (*ext_sym)->variant.extern_symbol_descr->
                                        variant.routine.ptr == routine_ptr &&
                           (a_symbol_ptr)routine_ptr->
                                   source_corresp.assoc_info != linked_symbol,
                          "decl_routine: unexpected conditions for routine",
                          "entry mismatch");
      /* Replace the routine pointed to from the extern-routine symbol with
         the new one. */
      (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr = NULL;
    }  /* if */
    sym->variant.routine.instance_ptr =
                                linked_symbol->variant.routine.instance_ptr;
    routine_ptr = linked_symbol->variant.routine.ptr;
    sym->variant.routine.ptr = routine_ptr;
    *old_type = routine_ptr->type;
    reconcile_routine_types(routine_ptr, type_ptr, /*preserve_rout_type=*/TRUE,
                            /*preserve_type_ptr=*/FALSE, dps);
    /* Do compatibility checking for the exception specification. */
    check_exception_specification(type_ptr, linked_symbol,
                                  &func_info->throw_position,
                                  /*is_redecl=*/TRUE);
  } else if (routine_ptr == NULL) {
    /* There is no IL entry, so create one now, and add it to the routines
       list of the innermost namespace scope (or, if this is an extern "C"
       context, add it to routines list of the file scope).  If we're in a
       prototype instantiation scope, do not add it to the routines list
       unless prototype instantiations are stored in the IL.  Similarly, do
       not add routines representing Microsoft duplicate specialization
       definitions to the list. */
    a_scope_depth  scope_depth = depth_innermost_namespace_scope;
    if ((scope_stack[depth_scope_stack].in_prototype_instantiation &&
         !prototype_instantiations_in_il) ||
        microsoft_specialization_redef) {
      scope_depth = NO_SCOPE_DEPTH;
    } else if ((linkage == idl_external || sun_mode) &&
               scope_stack_top().default_name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
      scope_depth = DEPTH_OF_FILE_SCOPE;
    }  /* if */
    routine_ptr = make_routine(type_ptr, storage_class, scope_depth);
    routine_ptr->has_deducible_return_type = dps->has_deducible_return_type;
    if (C_dialect == C_dialect_cplusplus) {
      if (locator->is_operator_name) {
        set_routine_special_kind(routine_ptr,
                                 (a_special_function_kind)sfk_operator);
        routine_ptr->variant.opname_kind = locator->variant.opname;
      } else if (locator->is_udl_operator_name) {
        set_routine_special_kind(routine_ptr,
                                 (a_special_function_kind)sfk_udl_operator);
      }  /* if */
    }  /* if */
    if (!linked_redecl_error && *ext_sym != NULL &&
        (*ext_sym)->variant.extern_symbol_descr
                  ->variant.routine.ptr != NULL) {
      /* A new routine entry has been created, yet the external symbol already
         refers to a different routine.  This can occur when there is an error,
         but it can also occur in SVR4 and Microsoft C mode -- for example:
           extern int ff();
           void f(int ff) { { extern float ff(); } }
         where the second declaration of ff has an incompatible type, yet
         no error is issued.  Another situation where this can happen: GNU C
         implicit declarations displaced by an incompatible explicit
         declaration.  For example:
           int main() { f(); }  // Implicit declaration of f.
           void f() {}          // Explicit but incompatible declaration.
      */
      a_routine_ptr old_rout =
                  (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr;
      if (is_local_scope_kind(scope_stack[effective_decl_level].kind) ||
          func_info->is_implicit_declaration) {
        routine_ptr->superseded_external = TRUE;
      } else {
        old_rout->superseded_external = TRUE;
        dps->first_decl = TRUE;
      }  /* if */
    } else {
      dps->first_decl = TRUE;
    }  /* if */
    if (microsoft_specialization_redef) {
      /* If this is a dummy entry generated to process a duplicate
         specialization definition, we mark is as "defined" so it can
         be recognized later on. */
      routine_ptr->defined = TRUE;
      routine_ptr->is_specialized = TRUE;
    }  /* if */
  } else {
    /* There is an existing IL entry that we are reusing. */
    /* Check for internal linkage on the old but not the new, or
       vice-versa. */
    a_boolean suppress_diagnostic = linked_redecl_error;

    if (routine_ptr->compiler_generated) {
      /* This is an entry for an intrinsic function or operator (e.g., the
         compiler generated ::operator new or ::operator delete).  It was
         created during initialization, but is overridden by the present
         declaration. */
      check_assertion_str2(routine_ptr->source_corresp.decl_position.seq == 0,
                           "decl_routine: compiler-generated function was",
                           "already assigned a position");
      /* Since the flag is cleared below, we're guaranteed that this is the
         first time we see the declaration in this translation unit. */
      dps->first_decl = TRUE;
      dps->first_decl_of_predeclared_entity = TRUE;
      /* Don't diagnose linkage mismatches either. */
      suppress_diagnostic = TRUE;
      /* Record the new source position, both in the symbol and in the
         routine entry (but update the symbol position later, so redeclaration
         diagnostics come out right in what follows). */
      update_sym_pos = TRUE;
      routine_ptr->source_corresp.decl_position = locator->source_position;
      /* Record the type of the latest declaration (which may have a different
         exception specification). */
      routine_ptr->type = type_ptr;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (routine_ptr->source_corresp.decl_pos_info == NULL &&
          decl_pos_block != NULL) {
        /* Update source range information now that we have seen an actual
           declaration. */
        routine_ptr->source_corresp.decl_pos_info =
                         make_decl_pos_supplement(in_file_scope(routine_ptr),
                                                  decl_pos_block);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if CHECKING
      if (routine_ptr->special_kind == (a_special_function_kind)sfk_operator) {
        /* In C++/CLI, there are several compiler generated "+" operators
           (e.g. String concatenation) that can be hidden by user defined 
           versions. */
        check_assertion_str(
             is_new_operator(routine_ptr->variant.opname_kind) ||
             is_delete_operator(routine_ptr->variant.opname_kind) ||
             (cli_or_cx_enabled &&
              routine_ptr->variant.opname_kind == (an_opname_kind)onk_plus),
             "decl_routine: bad opname kind");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
#if ASM_FUNCTION_ALLOWED
    if (storage_class == (a_storage_class)sc_asm ||
        routine_ptr->storage_class == (a_storage_class)sc_asm) {
      /* asm functions have internal linkage but do not conflict
         with previous declarations that are either extern or static. */
        routine_ptr->storage_class = storage_class = (a_storage_class)sc_asm;
    } else
#endif /* ASM_FUNCTION_ALLOWED */
    /* Do not add code here. */
    {
      check_for_linkage_conflict(&routine_ptr->storage_class, &linkage,
                                 &storage_class, &locator->source_position,
                                 suppress_diagnostic);
      if (linkage != idlb.linkage) {
        /* The linkage has been changed, so change the "name linkage", too. */
        idlb.linkage = linkage;
        compute_name_linkage(&idlb);
      }  /* if */
    }  /* if */
    if (is_function_def) {
      a_boolean      saved_referenced_flag;
      /* Put in the storage class for the definition (static or 
         unspecified). */
      routine_ptr->storage_class = storage_class;
      /* If the IL entry was previously referenced, the symbol should
         be considered to have been referenced as well. */
      saved_referenced_flag = routine_ptr->source_corresp.referenced;
      if (saved_referenced_flag) sym->referenced = TRUE;
      /* Reset the source correspondence to the definition symbol. */
      set_source_corresp(&routine_ptr->source_corresp, sym);
      /* Keep an indication of any references so far (the referenced
         flag is reset by the set_source_corresp call). */
      routine_ptr->source_corresp.referenced = saved_referenced_flag;
      if (func_info->is_inline && !routine_ptr->is_inline) {
        changed_to_inline = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (rtsp->exception_specification != NULL &&
      !rtsp->exception_specification->arg_cached &&
      is_nothrow_type(rtp)) {
    routine_ptr->never_throws = TRUE;
  }  /* if */      
  if (func_info->is_inline) set_inline_flag(routine_ptr, TRUE);
  if (use_std_c99_inlining && !idlb.is_block_extern_decl) {
    /* In C99 mode the definition_for_inlining_only flag is set only if that is
       justified by every file-scope declaration of a given inline function. */
    if (redeclaration) {
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
      if ((!definition_for_inlining_only ||
           !routine_ptr->definition_for_inlining_only) &&
          routine_ptr->storage_class == (a_storage_class)sc_unspecified) {
        /* The definition should not be discarded if it was preceded or
           followed by an extern declaration.  (If the current definition has
           an inline specifier and routine->definition_for_inlining_only is
           FALSE, the previous declaration did not have an "inline" specifier.
           If the previous declaration was an inline definition and the
           current declaration has no inline specifier, then
           definition_for_inlining_only will be FALSE.) */
        mark_as_needed((char *)routine_ptr, (an_il_entry_kind)iek_routine);
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */
      routine_ptr->definition_for_inlining_only &=
                                                  definition_for_inlining_only;
    } else {
      routine_ptr->definition_for_inlining_only = definition_for_inlining_only;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (use_gnu_c89_inlining && definition_for_inlining_only) {
    /* In GNU mode the keywords/attributes present at the point of definition
       determine the suppress_inline_body flag.  (In GNU C++ mode,
       use_gnu_c89_inlining is TRUE only when the "gnu_inline" attribute is
       present.) */
    routine_ptr->definition_for_inlining_only = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Link the symbol to the IL routine entry. */
  sym->variant.routine.ptr = routine_ptr;
  if (*ext_sym != NULL &&
      (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr == NULL) {
    /* Link the external symbol to the IL routine entry. */
    (*ext_sym)->variant.extern_symbol_descr->
                                variant.routine.ptr = routine_ptr;
  }  /* if */
  if (any_deferred_access_checks()) {
    /* Now that we know which function has been declared, recheck any
       access errors that occurred while scanning the declaration. */
    perform_deferred_access_checks_for_function(routine_ptr);
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  source_corresp_ptr = &routine_ptr->source_corresp;
  if (source_corresp_ptr->assoc_info == NULL) {
    /* There is no symbol pointed to from the routine, so update it with the
       current symbol. */
    set_source_corresp(source_corresp_ptr, sym);
    if ((is_friend_decl || idlb.is_block_extern_decl) &&
        secondary_translation_unit_seen()) {
      /* This routine entry might have been generated during the instantiation
         of another template.  The correspondence checking process must
         therefore be notified of its existence. */
      notify_correspondence_processing = TRUE;
    }  /* if */
  } else if (!redeclaration) {
    check_sym_of_other_decl(source_corresp_ptr, sym);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* In Microsoft mode we must track whether a routine was only declared
       through friend declarations.  Such declarations do not declare
       specializations of templates.
       (See record_predeclared_template_function.) */
    if (is_friend_decl) {
      if (!redeclaration) {
        routine_ptr->declared_only_as_friend = TRUE;
      }  /* if */
    } else {
      routine_ptr->declared_only_as_friend = FALSE;
    }  /* if */
    if (func_info->is_inline &&
        dps->declared_storage_class == (a_storage_class)sc_extern) {
      routine_ptr->explicit_extern_inline = TRUE;
    }  /* if */
    if (decl_modifiers->direct_linkage_specifier && !is_function_def) {
      routine_ptr->direct_linkage_specifier_on_nondef_decl = TRUE;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE &&
      !redeclaration && !template_function_specific_decl &&
      !explicit_template_reference) {
    /* Set the namespace parent in the symbol and IL entry. */
    add_namespace_parent_pointer(sym, source_corresp_ptr);
  }  /* if */
  if (changed_to_inline) {
    if (routine_ptr->called) {
      pos_sy_remark(ec_called_function_redeclared_inline,
                    &locator->source_position, sym);
    }  /* if */
  }  /* if */
  if (linkage != idl_none) {
    /* In case this is a block extern declaration, clear the
       is_local_to_function flag -- it will have been set based on scope
       alone in set_source_corresp. */
    source_corresp_ptr->is_local_to_function = FALSE;
  }  /* if */
  if (func_info->is_main_function) {
    /* This is "main", so remember the location of its routine entry. */
    if (il_header.main_routine == NULL) {
      il_header.main_routine = routine_ptr;
    } else {
      /* Unless there's an error there cannot be two "main" functions. */
      check_assertion_str(il_header.main_routine == routine_ptr ||
                            total_errors > 0,
                          "decl_routine: main redeclared");
    }  /* if */
  }  /* if */
  /* The name linkage has already been determined.  Apply it to the current
     declaration, and report inconsistencies, if appropriate. */
  set_name_linkage(&idlb, sym, source_corresp_ptr, *ext_sym,
                   &locator->source_position);
  if (is_function_def && !linked_redecl_error) {
    /* If this is a definition, unlink the routine entry and relink it at the
       end of the routines list, so that routines appear in the order that
       their bodies appear.  If a redeclaration error occurred, the scope
       depth is unreliable and this operation might not be possible.  Also,
       this must be done after the call to set_name_linkage because that call
       can move the routine entry from one list to another. */
    schedule_move_to_current_end_of_routines_list(routine_ptr);
  }  /* if */
  if (notify_correspondence_processing) {
    /* This had to be delayed until the name linkage was set. */
    establish_block_extern_function_correspondence(routine_ptr);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (!C_mode()) {
    /* Set the "name linkage environment" for this routine.  This is used by
       the C++-generating back end in cases like the following:
         extern "C" {
           static void f() { extern void g(); }
         }
       where the extern "C" block must be regenerated so that g() has C
       linkage.  */
    routine_ptr->surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
    if (is_function_def) {
      routine_ptr->definition_C_name_linkage_specified =
                                          idlb.extern_C_name_linkage_specified;
      routine_ptr->definition_has_direct_linkage_specifier =
                                      decl_modifiers->direct_linkage_specifier;
    }  /* if */
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (overload_symbol != NULL &&
      sym == overload_symbol->variant.overloaded_function.symbols) {
    /* sym has been newly added to an overload set that may include symbols
       introduced by using-declarations.  Check whether any of the latter
       have the same type as the current function -- report the error and
       remove the offending projection symbol(s) (to avoid overload ambiguity
       errors later on). */
    check_for_conflicts_with_using_decls(overload_symbol,
                                         &locator->source_position);
  }  /* if */
  dps->sym = sym;
  if (is_function_def && is_friend_decl) {
    /* Mark this function as defined in a friend declaration. */
    routine_ptr->defined_in_friend_decl = TRUE;
  }  /* if */
  /* Restore the scope stack. */
  if (idlb.namespace_reactivated)  {
    if (is_friend_decl) {
      pop_namespace_reactivation_scope();
    } else {
      pop_namespace_extension_scope();
    }  /* if */
  }  /* if */
  if (!redeclaration && (dps->dso_flags & DSO_CONSTEXPR) != 0) {
    routine_ptr->is_declared_constexpr = TRUE;
    routine_ptr->is_constexpr = TRUE;
    /* constexpr implies inline. */
    if (!routine_ptr->is_inline) set_inline_flag(routine_ptr, TRUE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (gcc_pragma_options_stack != NULL && !func_info->is_main_function) {
    /* Attach a synthesized "target" attribute from a "#pragma GCC target"
       if applicable. */
    attach_target_pragma_attribute(&dps->prefix_attributes);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_FUNCTION_MULTIVERSIONING
  if (gpp_mode && gnu_version >= 40800) {
    an_attribute_ptr  target_ap = NULL;
    /* GNU accepts "target" attributes in two locations in the declaration,
       but it only acts on the last one. */
    if (dps->id_attributes != NULL) {
      target_ap = find_last_target_attribute(dps->id_attributes);
    }  /* if */
    if (target_ap == NULL && dps->prefix_attributes != NULL) {
      target_ap = find_last_target_attribute(dps->prefix_attributes);
    }  /* if */
    if (target_ap != NULL) {
      a_boolean     found_existing;
      a_routine_ptr representative = NULL, target;
      if (redeclaration && linked_symbol != NULL) {
        /* There is a redeclaration of some sort. */
        if (!is_multiversion_representative(
                                         linked_symbol->variant.routine.ptr)) {
          /* This is the first time we've seen the function with a target
             attribute.  There is a previous instance, but it didn't have a
             target attribute.  For example:
               void foo(); // previous instance is definition without target
               void foo() __attribute__((target("default"))); // this instance
           */
          target = linked_symbol->variant.routine.ptr;
        } else {
          /* We already have a representative routine. */
          representative = linked_symbol->variant.routine.ptr;
          target = routine_ptr;
        }  /* if */
      } else {
        /* No representative routine yet. */
        target = routine_ptr;
      }  /* if */
      (void)process_multiversion_function(target_ap, effective_decl_level,
                                          representative, &target,
                                          &found_existing);
      /* Use the target-specific version for the remainder of the
         declaration. */
      if (*ext_sym != NULL &&
          (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr ==
                                                                 routine_ptr) {
        /* Fix the external symbol. */
        (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr = target;
      }  /* if */
      routine_ptr = target;
      sym = symbol_for(routine_ptr);
      dps->sym = sym;
    }  /* if */
  }  /* if */
  if (requires_gnu_target_attr &&
      (!has_gnu_routine_supp(routine_ptr) ||
       !gnu_routine_supp(routine_ptr)->is_target_specific_version)) {
    pos_sy_error(ec_function_redefinition, &locator->source_position, sym);
  }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
  attach_decl_attributes(dps, is_function_def);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (locator->is_qualified_name) {
      /* Microsoft compilers ignore dllexport/dllimport on qualified
         (re)declarations.  Issue a warning if dllexport or dllimport was
         specified, and it conflicts with the earlier declaration(s). */
      if ((decl_modifiers->flags & DM_DLLFLAGS) != 0 &&
          (decl_modifiers->flags & DM_DLLFLAGS) !=
                                (routine_ptr->decl_modifiers & DM_DLLFLAGS)) {
        pos_warning(ec_dll_interface_ignored_on_qualified_declaration,
                    &locator->source_position);
      }  /* if */
      /* Set the DLL flags to what is already recorded for this routine. */
      decl_modifiers->flags &= ~(a_decl_modifier)DM_DLLFLAGS;
      decl_modifiers->flags |= (routine_ptr->decl_modifiers & DM_DLLFLAGS);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  update_routine_decl_modifiers(routine_ptr, decl_modifiers,
                                &locator->source_position, redeclaration,
                                is_function_def,
                                (a_boolean)func_info->is_inline);
  if (update_sym_pos) {
    /* The position recorded in the symbol was not updated until now so that
       diagnostics would correctly refer to the prior declaration's position
       if needed.  Similarly, the compiler_generated flag was left unchanged
       until now to improve diagnostics. */
    sym->decl_position = locator->source_position;
    if (dps->first_decl_of_predeclared_entity) {
      routine_ptr->compiler_generated = FALSE;
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
    /* Record the assembly name. */
    if (dps->asm_name != NULL) {
      record_asm_name_for_routine(
                   routine_ptr, dps->asm_name, &dps->asm_name_pos,
                   routine_has_been_defined(routine_ptr) && !is_function_def);
    }  /* if */
    /* Some user-declarations are implicitly aliased to built-in functions.
       Check for such cases. */
    check_implicit_routine_alias(dps);
  }  /* if */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  /* Update the ELF visibility if applicable. */
  { an_ELF_visibility_kind  visibility = routine_ptr->ELF_visibility;
    update_for_default_ELF_visibility(&visibility, /*is_class_member=*/FALSE);
    routine_ptr->ELF_visibility = visibility;
  }
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (!routine_ptr->source_corresp.is_deprecated &&
      !routine_ptr->compiler_generated) {
    /* Check if a deprecated type was involved in this declaration. */
    warn_about_use_of_deprecated_type(type_ptr, &locator->source_position);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (dps->ms_attributes != NULL && !idlb.is_block_extern_decl) {
    apply_microsoft_attributes_to_routine(&dps->ms_attributes, routine_ptr);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (is_function_def && qualifier_namespace_ptr(*locator) != NULL) {
    check_assertion(!is_friend_decl || locator->is_error);
    routine_ptr->defined_outside_of_parent = TRUE;
  }  /* if */
  /* If cross-reference information is being issued, update the output.  If
     source sequence entries are being generated, update the source sequence
     entry. */
  record_symbol_declaration(srk_flags, sym, &locator->source_position,
                            dps->source_sequence_entry);
  reload_source_sequence_entry(dps);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (is_function_def || dps->first_decl) {
    update_decl_pos_info(&routine_ptr->source_corresp, decl_pos_block);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Do fixup on the source sequence entry that was just created to
     represent the current declaration.  Note that declaration_ssep is not
     used, since it may have been replaced (e.g., when a file scope entity
     is declared in a local scope and a sublist is generated). */
  if (record_name_references_in_context()) {
    name_ref = qualifiable_name_reference(locator, source_corresp_ptr);
  }  /* if */
  if (is_function_def) {
    /* The defining declaration of the function.  Set a pointer to the
       declared type and record the form of reference. */
    if (name_ref != NULL) {
      name_ref->used_in_primary_declarator = TRUE;
    }  /* if */
    set_routine_declared_type(routine_ptr, func_info->declared_type);
    if (is_friend_decl) {
      /* If there were any default arguments, they still need to be scanned.
         Enable the default-arg fixup processing to find the declared type. */
      func_info->declared_type = routine_ptr->declared_type;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (decl_modifiers->marked_as_gnu_extension) {
      routine_ptr->source_corresp.marked_as_gnu_extension = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (!is_function_def
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
      || func_info->is_movable_member_or_friend_def
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
                                                   ) {
    /* Set the type in the secondary declaration entry. */
    /* Note that the is_movable_member_or_friend_friend_def flag is set for
       non-member friend function definitions where the source-sequence
       entry that is put out within the class definition is a secondary-decl;
       the primary source sequence entry is put out after the class definition
       is complete. */
    an_sssd_flag_set      flags = SSSD_NO_FLAGS;
    a_type_ptr            declared_type;
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
    if (func_info->is_movable_member_or_friend_def) {
      /* Remove default arguments, if any, from the type associated with
         the secondary source-sequence entry; they will appear on the
         source-sequence entry for the definition instead.  (If they were
         repeated the C++-generating back end would put out invalid code.) */
      declared_type =
             routine_type_without_default_args(routine_ptr->declared_type);
    } else
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
    /* Do not insert code here. */
    {
      /* Normal case. */
      declared_type = func_info->declared_type;
    }  /* if */
    if (is_friend_decl) flags |= SSSD_FRIEND_DECL;
    if (func_info->is_implicit_declaration) flags |= SSSD_IMPLICIT_DECL;
    if (dps->first_decl) flags |= SSSD_FIRST_DECLARATION;
#if GNU_EXTENSIONS_ALLOWED
    if (decl_modifiers->marked_as_gnu_extension) {
      flags |= SSSD_MARKED_AS_GNU_EXTENSION;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    (void)update_src_seq_secondary_decl((char *)routine_ptr, declared_type,
                                        name_ref, flags, decl_pos_block);
  } else {
    routine_ptr->declared_storage_class = dps->declared_storage_class;
  }  /* if */
  add_src_seq_end_of_routine_if_needed(dps);
  source_sequence_entries_disallowed = saved_sses_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (is_function_def) {
    /* If a lint-style "argsused" or "varargs" comment appeared, record that in
       the function type.  That will suppress any warnings about unused
       parameters or variable arguments.  Note that this is done before calling
       process_curr_construct_pragmas; otherwise the pragmas we're interested
       in would have been disposed of. */
    record_lint_argsused_and_varargs_state(sym);
    if (!C_mode() && !exceptions_enabled) {
      /* Check whether a diagnostic should be issued on this exception
         specification, and issue one if needed. */
      issue_no_exception_support_diag_on_throw_spec(func_info);
    }  /* if */
    record_pragma_state_in_routine(routine_ptr);
  }  /* if */
  if (microsoft_specialization_redef) {
    /* If pragmas were specified on a definition that is about to be discarded,
       also discard the pragmas. */
    discard_curr_construct_pragmas();
  } else if (!func_info->is_implicit_declaration) {
    /* Do processing required for the rest of the pragmas, if any, that are
       bound to the current declaration.  Note that this has to be *after* the
       scope stack is restored, since processing depends on the pending_pragmas
       pointer in the scope stack entry.  If this is an implicit function
       declaration don't bind the pragmas to it (they'll be bound to the next
       applicable construct). */
    process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  }  /* if */
  if (special_kind_is(routine_ptr, sfk_udl_operator)) {
    check_udl_operator_type(locator, routine_ptr);
  }  /* if */
#if NEED_NAME_MANGLING
  if (func_info->any_default_args) {
    set_parent_routine_for_closure_types_in_default_args(type_ptr, sym);
  }  /* if */
#endif /* NEED_NAME_MANGLING */
  routine_ptr->suppress_inline_body =
                                    routine_ptr->definition_for_inlining_only;
#if GNU_EXTENSIONS_ALLOWED
  /* Restore dps->type, since it may have been modified by type-transforming
     attributes. */
  dps->type = orig_type;
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Return the linkage kind. */
  *linkage_ptr = linkage;
  /* Restore the "innermost namespace scope" if it was modified. */
  if (saved_depth_innermost_namespace_scope != NO_SCOPE_DEPTH) {
    depth_innermost_namespace_scope = saved_depth_innermost_namespace_scope;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_routine */


void decl_function_template(a_symbol_locator            *locator,
                            a_func_info_block           *func_info,
                            a_symbol_ptr                *symbol_ptr,
                            a_tmpl_decl_state_ptr       decl_state)
/*
Roughly speaking, this routine does for function templates what decl_routine
does for ordinary functions.  Look up and reuse or else create a function
template symbol; for new symbols also create a routine entry (though one that
is not added to the IL).  *locator represents the name specified in the
declarator.  func_info holds some function properties.  decl_state points to
a block of information describing the current state of processing for the
template.  The function template may be part of an overload set, it may have
been previously declared (but not defined), and it may be an out-of-line
definition of a member function of a class template.
*/
{
  a_decl_parse_state                *dps = &decl_state->decl_parse;
  a_type_ptr                        type_ptr = dps->type;
  a_storage_class                   storage_class = dps->storage_class;
  a_symbol_ptr                      sym = NULL;
  a_symbol_ptr                      overload_symbol = NULL;
  a_symbol_ptr                      homonym_symbol = NULL;
  a_symbol_ptr                      rout_sym, ext_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_routine_ptr                     rout_ptr, rp;
  a_memory_region_number            region_to_switch_back_to;
  a_boolean                         changed_to_inline = FALSE;
  a_boolean                         set_invisible = FALSE;
  a_boolean			    in_template_dependent_context;
  a_boolean			    in_nonreal_instantiation;
  a_boolean			    redeclaration = FALSE;
  an_id_linkage_block               idlb;
  a_boolean                         microsoft_out_of_class_redecl;
  a_boolean                         proxy_member_friend = FALSE;
  a_template_decl_info_ptr          templ_decl_info = decl_state->decl_info;
  a_scope_depth                     orig_decl_level =
                                                   decl_state->orig_decl_level;
  a_boolean                         is_specialization =
                                                 decl_state->is_specialization;
  a_template_ptr                    il_template_entry =
                                                 decl_state->il_template_entry;

  db_enter(3, "decl_function_template");
  check_assertion(scope_is(&scope_stack_top(), sck_template_declaration));
  if (func_info->is_inline && !extern_inline_allowed) {
    storage_class = (a_storage_class)sc_static;
  } else if (storage_class == (a_storage_class)sc_unspecified) {
    /* Default. */
    storage_class = (a_storage_class)sc_extern;
  }  /* if */
  clear_id_linkage_block(&idlb);
  idlb.templ_info = templ_decl_info;
  idlb.storage_class = storage_class;
  idlb.func_info = func_info;
  idlb.is_function_template = TRUE;
  idlb.type = type_ptr;
  idlb.locator = locator;
  set_linkage_environment(&idlb, orig_decl_level);
  in_nonreal_instantiation =
               scope_stack[idlb.effective_decl_level].in_nonreal_instantiation;
  in_template_dependent_context =
          scope_stack[idlb.effective_decl_level].in_prototype_instantiation ||
          in_nonreal_instantiation ||
          scope_stack[idlb.effective_decl_level].in_generic_definition;
  if (idlb.is_friend_decl && !friend_function_injection_enabled &&
      (!gpp_mode || locator->is_operator_name)) {
    /* g++ injects function templates even when they don't inject normal
       functions. */
    set_invisible = TRUE;
  }  /* if */
  if (locator->is_qualified_name && locator->is_class_member &&
      locator->specific_symbol != NULL) {
    a_type_ptr		parent_class;
    a_symbol_ptr	parent_class_sym;
    /* Member function template. */
    sym = locator->specific_symbol;
    parent_class = sym_parent_class(sym);
    parent_class_sym = (a_symbol_ptr)parent_class->source_corresp.assoc_info;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      sym = NULL;
      set_to_error_locator(*locator);
    } else if (in_template_dependent_context) {
      /* Don't attempt to match a friend declaration while processing
         a prototype instantiation. */
      if (!idlb.is_friend_decl) {
        pos_stsy_error(ec_not_a_member, &locator->source_position,
                       sym->header->identifier, parent_class_sym);
        set_to_error_locator(*locator);
      } else if (curr_token != tok_semicolon) {
        pos_sy_error(ec_bad_scope_for_definition,
                     &locator->source_position, sym);
        set_to_error_locator(*locator);
      }  /* if */
      sym = NULL;
      proxy_member_friend = TRUE;
    } else if (sym->kind != (a_symbol_kind)sk_member_function &&
               sym->kind != (a_symbol_kind)sk_function_template &&
               sym->kind != (a_symbol_kind)sk_overloaded_function) {
      /* We must have nonfunction class member.  This is an error, so set sym
         to NULL to force the creation of a fake member function symbol. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
      sym = NULL;
      set_to_error_locator(*locator);
    } else {
      /* Look for a member function symbol of this type in the symbol table.
         It is an error if it is  not already there. */
      a_symbol_ptr  other_match;
      sym = member_function_redecl_sym(sym, dps, templ_decl_info->parameters,
                                       &other_match);
      if (sym != NULL) {
        if (other_match != NULL) {
          /* There were multiple matches (this is possible with Microsoft-mode
             selective overriders).  Issue an error. */
          pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
          /* Proceed as if no match was found. */
          sym = NULL;
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* This is the symbol for a member template function.  Use
             this symbol. */
          /* Ordinarily, an out-of-class member declaration is not the "first
             declaration" of the member template, but if this is an explicit
             specialization, it can be seen as the first declaration of that
             member (for a particular parent class):
               template<class> struct S { template<class> void f(); };
               template<> template<class T> void S<int>::f() {}
          */
          dps->first_decl = decl_state->is_specialization;
        } else if (is_prototype_instantiation_or_cli_generic(
                                                           parent_class_sym)) {
          /* This is a member function symbol of a prototype instantiation.
             Get the associated function template. */
          sym = get_member_function_template_symbol(sym);
        } else {
          /* Not a template or a member of a prototype instantiation --
             this must be an error. */
          sym = NULL;
        }  /* if */
      }  /* if */
      if (sym == NULL) {
        if (other_match == NULL) {
          /* No member function with a matching type was found.  Issue an
             error. */
          pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                         ec_no_match_for_type_of_overloaded_function :
                         ec_not_compatible_with_previous_decl,
                       &locator->source_position, locator->specific_symbol);
        } else {
          /* Multiple matches were found.  A diagnostic was already issued. */
        }  /* if */
        set_to_error_locator(*locator);
      } else {
        /* Merge type information from the two declarations. */
        a_type_ptr  prev_type;
        tssp = template_supplement_for_symbol(sym);
        prev_type = tssp->variant.function.routine->type;
        /* Be sure the current throw specification is consistent with the one
           on the previous declaration.  This must be done prior to reconciling
           the type with that of a previous declaration. */
        if (locator->is_destructor_name ||
            (locator->is_operator_name &&
             is_delete_operator(locator->variant.opname))) {
          /* For destructors and operator delete, an exception specification
             may need to be generated. */
          update_routine_type_exception_specification_if_needed(
                                   tssp->variant.function.routine, &type_ptr);
        }  /* if */
        proto_instantiate_exception_spec_redecl(decl_state, sym);
        check_exception_specification(type_ptr, sym,
                                      &func_info->throw_position,
                                      /*is_redecl=*/TRUE);
        adjust_member_routine_type(type_ptr, prev_type);
        reconcile_routine_types(tssp->variant.function.routine, type_ptr,
                                /*preserve_rout_type=*/TRUE,
                                /*preserve_type_ptr=*/FALSE, dps);
        /* Merge the default template argument information. */
        if (symbol_is(sym, sk_function_template)) {
          (void)reconcile_template_param_lists(
                                decl_state->decl_info->parameters,
                                decl_state, sym,
                                &locator->source_position,
                                /*default_allowed=*/FALSE,
                                /*checking_parent_params=*/FALSE,
                                /*allow_missing_member_constraint=*/TRUE,
                                es_discretionary_error);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (!is_error_locator(*locator)) {
    if (is_single_param_operator_new_or_delete(locator, type_ptr,
                                               /*include_nothrow=*/FALSE)) {
      /* Overloading should not be allowed on the single-argument version of
         operator new(size_t) or operator delete(void *).  Though it is not
         expressly prohibited, it can be inferred from the fact that new and
         delete have an invariant first argument.  At least one C++ test
         suite expects an error. */
      pos_error(is_new_operator(locator->variant.opname) ?
                    ec_template_operator_new : ec_template_operator_delete,
                &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
  if (func_info->is_definition) {
    /* This is a defining declaration of the function template. */
    idlb.is_definition = TRUE;
    if (func_info->function_type_from_typedef) {
      /* Just as it is an error when a normal function is defined for the
         function type to come from a typedef, so too is that an error when
         a function template is being defined. */
      error(ec_function_type_must_come_from_declarator);
      /* Copy the type entry, since the typedef type may not be shared. */
      type_ptr = copy_routine_type_with_param_types(skip_typerefs(type_ptr),
                                                   /*copy_default_args=*/TRUE);
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    /* Must be a member template. */
    if (sym->is_class_member && idlb.is_friend_decl &&
        func_info->is_definition) {
      /* A class member function cannot be defined in a friend declaration. */
      pos_sy_error(ec_bad_scope_for_definition,
                   &locator->source_position, locator->specific_symbol);
    } else if (idlb.is_friend_decl &&
               (!func_info->is_definition || in_template_dependent_context)) {
      /* Don't check the scope if this is a friend declaration unless it
         is a definition during a real instantiation. */
    } else if (!namespace_is_enclosed_by_scope(sym,
                                               &scope_stack[idlb.
                                                    effective_decl_level])) {
      /* This member template is being defined in a scope that does not
         enclose the scope in which the parent class was defined. */
      if (is_specialization) {
        sym_error(ec_bad_scope_for_specialization, sym);
      } else if (func_info->is_definition) {
        sym_error(ec_bad_scope_for_definition, sym);
      } else {
        sym_error(ec_bad_scope_for_redeclaration, sym);
      }  /* if */
    }  /* if */
  } else if (locator->specific_symbol != NULL &&
             (qualifier_namespace_ptr(*locator) != NULL ||
              locator->is_file_scope_qualified_name)) {
    /* This identifier is a namespace-qualified name that was previously
       declared, or else a file-scope qualified name (friend declarations
       only).  Do the appropriate checking, including overload resolution.
       Furthermore, for definitions of namespace-qualified names, be sure
       this is a valid scope for the definition (7.3.1.4). */
    /* Look up the name. */
    if (in_template_dependent_context) {
      /* Don't try to find a matching qualified name for a friend declaration
         in a prototype instantiation. */
    } else {
      qualified_name_redecl_sym(&idlb);
      sym = idlb.linked_symbol;
      if (sym != NULL && sym->kind != (a_symbol_kind)sk_function_template) {
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
        set_to_error_locator(*locator);
        sym = NULL;
      }  /* if */
      homonym_symbol = idlb.homonym_symbol;
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    tssp = template_supplement_for_symbol(sym);
    rout_ptr = tssp->variant.function.routine;
    if (func_info->is_definition) {
      /* Set the defined_outside_of_parent flag, if appropriate. */
      if (sym->is_class_member) {
        /* If this is the definition of a class member specified with a
           qualified name, it must be outside of the parent. */
        if (qualifier_class_type(*locator) != NULL) {
          rout_ptr->defined_outside_of_parent = TRUE;
        }  /* if */
      } else if (sym_is_namespace_member(sym)) {
        /* Likewise, if this is the definition of a namespace member using
           a qualified name, it must be outside of the parent. */
        if (qualifier_namespace_ptr(*locator) != NULL) {
          tssp->variant.function.routine->defined_outside_of_parent = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    if (scope_stack[idlb.effective_decl_level].in_prototype_instantiation) {
      /* Suppress lookup of friend template declarations during prototype
         instantiation. */
    } else {
      /* id_linkage will set sym to point to an existing symbol when we have
         a redeclaration of a function template. */
      id_linkage(&idlb, dps);
      sym = idlb.linked_symbol;
      homonym_symbol = idlb.homonym_symbol;
      overload_symbol = idlb.overload_symbol;
      if (sym != NULL && sym->kind != (a_symbol_kind)sk_function_template) {
        /* Invalid redeclaration. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
        sym = NULL;
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      /* Not a redeclaration. */
      a_scope_stack_entry_ptr  ssep = &scope_stack[idlb.effective_decl_level];
      an_error_code            error_code;
      a_boolean                membership_recorded = FALSE;
      if (!is_error_locator(*locator)) {
        /* If this is an overloaded operator, check for errors in the
           argument list.  Note that this check is not done for redeclarations,
           on the assumption that once will have been enough. */
        check_assertion(!locator->is_class_member || proxy_member_friend);
        if (!proxy_member_friend) {
          /* Don't do these checks for proxy member friends as we don't
             know whether the function is static or nonstatic. */
          check_operator_function_params(type_ptr, (a_type_ptr)NULL, locator);
        }  /* if */
        /* If it's a new or delete operator, be sure the scope is not a
           namespace scope. */
        report_bad_new_or_delete(locator, dps);
      }  /* if */
      check_default_args(dps);
      if (homonym_symbol != NULL &&
          !overload_distinguishable(homonym_symbol, type_ptr,
                                    templ_decl_info->parameters,
                                    &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        set_to_error_locator(*locator);
        /* Avoid overloading. */
        homonym_symbol = NULL;
      }  /* if */
      if (proxy_member_friend || in_nonreal_instantiation ||
          (in_template_dependent_context && locator->is_qualified_name)) {
        /* A member template of a (dependent) proxy class was named as a
           friend or a qualified friend declaration in a prototype
           instantiation.  Create a dummy symbol for it (it will not be
           linked into the symbol table) and configure it with the appropriate
           parent. */
        sym = alloc_symbol((a_symbol_kind)sk_function_template,
                           locator->symbol_header, &locator->source_position);
        if (locator->is_class_member) {
          set_class_membership(sym, (a_source_correspondence_ptr)NULL,
                               qualifier_class_type(*locator));
        } else if (qualifier_namespace_ptr(*locator) != NULL) {
          set_namespace_membership(sym, (a_source_correspondence *)NULL,
                                   qualifier_namespace_ptr(*locator));
        }  /* if */
        sym->is_error = locator->is_error;
        membership_recorded = TRUE;
      } else if (homonym_symbol != NULL) {
        /* Another function with the same name has been declared already.  It
           may or may not be a function template.  In any case, create a new
           symbol and add it to an overload list. */
        sym = record_overload(locator, /*is_template=*/TRUE, homonym_symbol,
                              &overload_symbol, set_invisible,
                              idlb.is_friend_decl);
      } else {
        /* No overloading.  Simply create a new symbol. */
        sym = enter_local_symbol((a_symbol_kind)sk_function_template, locator,
                                 idlb.effective_decl_level,
                                 /*suppress_redecl_error=*/FALSE);
        /* Mark friend functions for which this is the initial declaration. */
        if (set_invisible) sym->is_invisible = TRUE;
      }  /* if */
      tssp = template_supplement_for_symbol(sym);
      tssp->is_variadic = decl_state->is_variadic;
      tssp->has_variadic_template_params =
                                      decl_state->has_variadic_template_params;
      if (tssp->variant.function.decl_cache.decl_info == NULL) {
        /* If this is the initial declaration of this template, set the
           template cache information to point to the template declaration
           information that was passed in.  This must be done now so that
           things like the template parameter list will be available to
           other routines that are called below.  This includes the
           diagnostic routines that make use of the template parameter
           list in diagnostic output. */
        set_template_cache_info(&tssp->variant.function.decl_cache,
                                (a_token_cache_ptr)NULL,
                                templ_decl_info);
      }  /* if */
      rout_ptr = NULL;
      /* Set namespace membership on this template function. */
      if (!membership_recorded) {
        if (idlb.is_friend_decl && ssep->in_prototype_instantiation) {
          ssep = &scope_stack[depth_innermost_namespace_scope];
        }  /* if */
        if (scope_is(ssep, sck_namespace) ||
            scope_is(ssep, sck_namespace_extension)) {
          set_namespace_membership(sym, (a_source_correspondence *)NULL,
                                   ssep->il_scope->variant.assoc_namespace);
        }  /* if */
      }  /* if */
    } else {
      a_param_type_ptr  ptp;
      check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
      tssp = template_supplement_for_symbol(sym);
      /* Discard any previously created substituted type entries.  These
         may no longer be valid as a result of the redeclaration. */
      free_list_of_substituted_type_list_entries(
                                     tssp->variant.function.substituted_types);
      tssp->variant.function.substituted_types = NULL;
      rout_ptr = tssp->variant.function.routine;
      /* Declaring a default argument on a function template redeclaration is
         nonstandard.  Issue at least a warning, and always an error if the
         template has already been instantiated. */
      for (ptp = type_ptr->variant.routine.extra_info->param_type_list;
           ptp != NULL;
           ptp = ptp->next) {
        if (ptp->has_default_arg) {
          an_error_severity  severity;
          an_error_code      error_code;
          if (tssp->variant.function.instantiations != NULL) {
            severity = es_error;
            error_code = ec_default_arg_on_function_template_not_allowed;
          } else {
            if (strict_ansi_mode) {
              severity = strict_ansi_error_severity;
            } else {
              severity = es_warning;
            }  /* if */
            error_code = ec_nonstd_default_arg_on_function_template_redecl;
          }  /* if */
          pos_diagnostic(severity, error_code, &locator->source_position);
          break;
        }  /* if */
      }  /* for */
      /* Be sure the current throw specification is consistent with the one
         on the previous declaration.  This must be done prior to adjusting
         the member function's type. */
      proto_instantiate_exception_spec_redecl(decl_state, sym);
      check_exception_specification(type_ptr, sym,
                                    &func_info->throw_position,
                                    /*is_redecl=*/TRUE);
      /* Merge type information from the two declarations. */
      reconcile_routine_types(rout_ptr, type_ptr, /*preserve_rout_type=*/TRUE,
                              /*preserve_type_ptr=*/FALSE, dps);
      /* Merge the default template argument information. */
      (void)reconcile_template_param_lists(
                                decl_state->decl_info->parameters,
                                decl_state, sym,
                                &locator->source_position,
                                /*default_allowed=*/TRUE,
                                /*checking_parent_params=*/FALSE,
                                /*allow_missing_member_constraint=*/TRUE,
                                es_discretionary_error);
      /* If appropriate, clear the is_invisible flag in the symbol and
         in the symbol representing its overload set. */
      if (sym->is_invisible && !idlb.is_friend_decl) {
        sym->is_invisible = FALSE;
        if (sym->overload_set_member) {
          check_assertion(overload_symbol != NULL);
          overload_symbol->is_invisible = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  dps->sym = sym;
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Note that the is_microsoft_invisible_operator flag is not set for
     templates as, unlike normal functions, they seem to be visible
     when defined only in a friend declaration. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* A routine entry is created for the function template, but it is not
     always entered in the IL.  It is a convenient place to keep track of
     prototype information: type, storage class, etc.  These values may be
     reused when the template is instantiated. */
  if (rout_ptr == NULL) {
    a_symbol_ptr  prototype_sym;
    dps->first_decl = TRUE;
    switch_to_file_scope_region(&region_to_switch_back_to);
    tssp->variant.function.routine = rout_ptr = alloc_routine();
#if MICROSOFT_EXTENSIONS_ALLOWED
    tssp->is_generic = decl_state->is_generic;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    switch_back_to_original_region(region_to_switch_back_to);
    rout_ptr->type = type_ptr;
    rout_ptr->storage_class = storage_class;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->is_definition) {
      rout_ptr->declared_storage_class = dps->declared_storage_class;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    rout_ptr->has_deducible_return_type = dps->has_deducible_return_type;
    if (func_info->is_inline) set_inline_flag(rout_ptr, TRUE);
    if (dps->dso_flags & DSO_CONSTEXPR) {
      rout_ptr->is_declared_constexpr = TRUE;
      rout_ptr->is_constexpr = TRUE;
      /* constexpr implies inline. */
      if (!rout_ptr->is_inline) set_inline_flag(rout_ptr, TRUE);
    }  /* if */
    if (locator->is_operator_name) {
      set_routine_special_kind(rout_ptr,
                               (a_special_function_kind)sfk_operator);
      rout_ptr->variant.opname_kind = locator->variant.opname;
    } else if (locator->is_udl_operator_name) {
      set_routine_special_kind(rout_ptr,
                               (a_special_function_kind)sfk_udl_operator);
    } else if (locator->is_destructor_name && sym->is_class_member) {
      /* This can happen in a friend declaration that refers to a destructor
         with a dependent name qualifier.  It can also occur with out-of-class
         declarations that look like destructors but have no matching
         declaration in the class definition. */
      check_assertion_or_expect_error(proxy_member_friend);
      set_routine_special_kind(rout_ptr,
                               (a_special_function_kind)sfk_destructor);
    } else if (((dps->dso_flags & DSO_CONSTRUCTOR) != 0 ||
                (dps->do_flags & DO_IS_CONSTRUCTOR) != 0) &&
               sym->is_class_member) {
      /* This can happen in a friend declaration that refers to a constructor
         with a dependent name qualifier.  It can also occur with out-of-class
         declarations that look like constructors but have no matching
         declaration in the class definition. */
      check_assertion_or_expect_error(proxy_member_friend);
      set_routine_special_kind(rout_ptr,
                               (a_special_function_kind)sfk_constructor);
    }  /* if */
    check_assertion(is_error_locator(*locator) ||
                    !locator->is_conversion_name);
    /* Allocate the symbol for the prototype instantiation of the
       function template. */
    prototype_sym = make_function_template_prototype_symbol(
                                  sym, rout_ptr, templ_decl_info->parameters);
    set_source_corresp(&rout_ptr->source_corresp, prototype_sym);
    set_membership_in_source_corresp(&(rout_ptr->source_corresp),
				     prototype_sym);
    rout_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_extern) ?
                                (a_name_linkage_kind)nlk_cplusplus_external :
                                (a_name_linkage_kind)nlk_internal;
    /* Call a routine that manages the correspondence of entities between
       translation units to notify it of the new instance. */
    record_instantiation(prototype_sym, tssp);
    if ((prototype_instantiations_in_il || tssp->is_generic) &&
        !locator->is_error) {
      /* Normally, we let add_to_routines_list determine which scope to add
         the routine to, but for proxy members nominated in friends, that
         would yield a nonexisting scope; instead we just put those on the
         file scope list. */
      add_to_routines_list(rout_ptr,
                           proxy_member_friend ? DEPTH_OF_FILE_SCOPE :
                                                 NO_SCOPE_DEPTH);
    }  /* if */
  } else {
    redeclaration = TRUE;
    decl_state->other_decl_pos = sym->decl_position;
    if (rout_ptr->is_declared_constexpr !=
                                    ((dps->dso_flags & DSO_CONSTEXPR) != 0)) {
      /* The previous declaration doesn't match the current one wrt. the
         "constexpr" specifier.  Issue an error. */
      pos_sy_error(rout_ptr->is_declared_constexpr ?
                     ec_previous_constexpr_decl_conflict :
                     ec_previous_nonconstexpr_decl_conflict,
                   &dps->specifiers_pos, sym);
      rout_ptr->is_declared_constexpr = TRUE;
      if (!sym->defined) {
        /* If the function was not previously defined, treat it as constexpr
           from here on at least. */
        rout_ptr->is_constexpr = TRUE;
      }  /* if */
    }  /* if */
    if (!rout_ptr->is_inline) {
      if (func_info->is_inline || rout_ptr->is_declared_constexpr) {
        set_inline_flag(rout_ptr, TRUE);
        changed_to_inline = TRUE;
      }  /* if */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (!sym->is_class_member) {
      /* GNU attributes on a function template redeclaration appear to have no
         effect.  However, we still want to record their presence in the
         prototype instantiation (e.g., for source analysis purposes).
         Attributes on out-of-class definitions are not deactivated in this
         way. */
      deactivate_gnu_decl_attributes_on_template_redecl(
                                    dps, rout_ptr->source_corresp.attributes);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (decl_state->is_template_friend && func_info->is_definition) {
    rout_ptr->defined_in_friend_decl = TRUE;
  }  /* if */
  attach_decl_attributes(dps, func_info->is_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (dps->ms_attributes != NULL) {
    apply_microsoft_attributes_to_routine(&dps->ms_attributes, rout_ptr);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  check_defaulted_or_deleted_function(dps, func_info,
                                      &locator->source_position);
  if (locator->template_arg_list != NULL && !locator->is_template_id) {
    /* In Microsoft mode, scan_real_declarator_id allows explicit template
       arguments on non-member template declarations, but they should only
       be allowed on function templates, and only if the template arguments
       could substitute for the template parameters (although the resulting
       type need not be compatible in any way).  Microsoft also allows
       an explicit template argument list on a redeclaration of a function
       template in a friend declaration in a class template.  This is
       not considered a redeclaration because the friend could be dependent
       so we don't try to match the declaration until we do a real
       instantiation. */
    a_boolean  template_args_okay = FALSE;
    check_assertion(microsoft_mode);
    if (redeclaration || decl_state->is_template_friend) {
      a_template_arg_ptr  new_arg_list = NULL;
      a_type_ptr          new_type;
      new_type = substitute_template_arguments(
                      sym, locator->template_arg_list, &new_arg_list,
                      tssp->variant.function.decl_cache.decl_info->parameters,
                      /*is_partial_order_check=*/FALSE);
      template_args_okay = new_type != NULL;
      free_template_arg_list(new_arg_list);
    }  /* if */
    locator->template_arg_list = NULL;
    if (!is_error_locator(*locator)) {
      if (template_args_okay) {
        pos_warning(ec_explicit_template_args_ignored,
                    &locator->source_position);
      } else {
        pos_error(ec_explicit_template_args_not_allowed,
                  &locator->source_position);
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
  }  /* if */
  microsoft_out_of_class_redecl = microsoft_mode && sym->is_class_member &&
                                                    !func_info->is_definition;
  if (!is_error_locator(*locator)) {
    if (func_info->is_definition) {
      if (sym->defined) {
        pos_sy_error(ec_already_defined, &locator->source_position, sym);
      } /* if */
    } else if (!microsoft_out_of_class_redecl) {
      if (!microsoft_mode &&
          sym->is_class_member && !idlb.is_friend_decl && !is_specialization) {
        /* A non-defining declaration of a member function is only allowed
           in Microsoft mode. */
        if (func_info->is_deleted || func_info->is_defaulted) {
          /* The member was defined with "= default;" or "= delete;", but
             some error caused it not to be treated as a valid definition.
             Don't issue another error. */
          check_assertion(total_errors != 0);
        } else {
          pos_sy_error(ec_member_function_redecl_outside_class,
                       &locator->source_position, sym);
        }  /* if */
      } /* if */
    } /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->is_definition) {
      set_routine_declared_type(rout_ptr, func_info->declared_type);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (func_info->is_definition || dps->first_decl) {
    update_decl_pos_info(&rout_ptr->source_corresp,
                         &decl_state->decl_pos_block);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  update_routine_decl_modifiers(rout_ptr, &dps->decl_modifiers,
                                &locator->source_position, redeclaration,
                                (a_boolean)func_info->is_definition,
                                (a_boolean)func_info->is_inline);
  if (overload_symbol != NULL && guiding_decls_allowed) {
    /* A new symbol was added to an overload list which may have included
       functions that were specific declarations of the current template.
       For instance,
         void f(int i) {  ... }
         template <class T> void f(T t) { ... }
       Here the first declaration of f turns out to be a specific declaration
       of the template named f, even though the template is declared after the
       instance.  We need to go back over the overload list and associate a
       function instantiation entry with each routine that can in retrospect
       be recognized as a specific declaration of the function template. */
    for (rout_sym = overload_symbol->variant.overloaded_function.symbols;
         rout_sym != NULL;
         rout_sym = rout_sym->next) {
      if (rout_sym->kind == (a_symbol_kind)sk_routine) {
        /* Determine whether rout_sym is a specialization of the function
           template represented by sym. */
        record_predeclared_template_function(sym, rout_sym,
                                             templ_decl_info->parameters,
                                             il_template_entry);
      }  /* if */
    }  /* for */
  }  /* if */
  if (changed_to_inline) {
    /* An existing template function has been redeclared and this time it's
       inline.  Be sure that "inline" and storage class are propagated
       through the instances. */
    a_template_instance_ptr  tip = tssp->variant.function.instantiations;
    for (; tip != NULL; tip = tip->next) {
      rp = tip->instance_sym->variant.routine.ptr;
      if (rp->is_specialized) {
        /* An explicit specialization is only inline if so declared. */
      } else {
        if (!extern_inline_allowed &&
            rp->storage_class != (a_storage_class)sc_static) {
          /* Issue a warning on linkage inconsistency only on nonmember
             function templates. */
          if (!sym->is_class_member) {
            sym_warning(ec_template_and_instance_linkage_conflict,
                        tip->instance_sym);
          }  /* if */
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
        }  /* if */
        /* Issue a diagnostic is the function has already been called. */
        if (!rp->is_inline && rp->called) {
          sym_remark(ec_called_function_redeclared_inline,
                     tip->instance_sym);
        }  /* if */
        set_inline_flag(rp, TRUE);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!sym->is_class_member && guiding_decls_allowed) {
    /* The overload list for the current scope has been searched for previous
       declarations that now appear to be instances of the template, but we
       also need to check for block-extern declarations that fall into the
       same category.  Issue an error if any such routine was actually used
       in a call. */
    for (ext_sym = sym->header->other_symbols;
         ext_sym != NULL;
         ext_sym = ext_sym->next) {
      /* The decl_scope test is used to exclude symbols that are not for
         the current translation unit. */
      if (ext_sym->kind == (a_symbol_kind)sk_extern_routine &&
          sym_parent_namespace_or_null(ext_sym) == 
                                        sym_parent_namespace_or_null(sym) &&
          ext_sym->decl_scope == file_scope_number) {
        /* A routine belonging to the same namespace.  Don't check on the
           the type before determining that there is no instance pointer
           (i.e., it didn't appear in the search of the overload set) and
           it was referenced. */
        rp = ext_sym->variant.extern_symbol_descr->variant.routine.ptr;
        rout_sym = symbol_for(rp);
        if (rp->source_corresp.referenced &&
            rout_sym->variant.routine.instance_ptr == NULL) {
          /* This must be a block-extern declaration.  Check whether it is
             an instance of the function templates represented by sym. */
          a_type_ptr          tp = skip_typerefs(rp->type);
          a_template_arg_ptr  templ_arg_list;
          a_symbol_ptr        dummy;
          if (is_match_for_function_template(sym, tp, &templ_arg_list, &dummy,
                                             templ_decl_info->parameters,
                                             (a_template_arg_ptr)NULL,
                                             /*is_decl_context=*/TRUE)) {
            sym_error(ec_template_instance_already_used, rout_sym);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!redeclaration) {
    if (sym->header->identifier != NULL &&
        !sym->is_class_member &&
        sym_parent_namespace_or_null(sym) == NULL &&
        (strcmp(sym->header->identifier, "main") == 0)) {
      /* A global scope function template named "main" is not allowed. */
      pos_error(ec_function_template_named_main, &locator->source_position);
    }  /* if */
  }  /* if */
  /* If this symbol might not be found because it is invisible, add it
     to the friend list for the class. */
  if (arg_dependent_lookup_enabled && idlb.is_friend_decl) {
    a_boolean	add_to_friend_list = FALSE;
    if (sym->is_invisible) add_to_friend_list = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (sym->is_microsoft_invisible_operator) add_to_friend_list = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (add_to_friend_list) {
      a_scope_stack_entry_ptr ssep = &scope_stack[orig_decl_level];
      check_assertion(!sym->is_class_member || sym->is_error);
      check_assertion(ssep->kind == (a_scope_kind)sck_class_struct_union);
      add_friend_function_to_lookup_list_for_class(sym, ssep->il_scope->
                                                           variant.assoc_type);
    }  /* if */
  }  /* if */
  /* Restore the scope stack. */
  if (idlb.namespace_reactivated)  {
    if (idlb.is_friend_decl) {
      pop_namespace_reactivation_scope();
    } else {
      pop_namespace_extension_scope();
    }  /* if */
  }  /* if */
  if (!rout_ptr->source_corresp.is_deprecated) {
    /* Check if a deprecated type was involved in this declaration. */
    warn_about_use_of_deprecated_type(type_ptr, &locator->source_position);
  }  /* if */
  if (special_kind_is(rout_ptr, sfk_udl_operator)) {
    (void)check_udl_operator_template(sym, &locator->source_position);
  }  /* if */
  /* Return the function template symbol. */
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_function_template */


a_boolean reconcile_static_data_member_types(
					a_symbol_ptr		sym,
					a_type_ptr		type_ptr,
					a_source_position_ptr	err_pos)
/*
The static data member specified by "sym" is being defined outside of
its class with the type specified by "type_ptr".  Verify that the new
type is compatible with the previously declared type.  If "sym" was
previously declared with an incomplete array type, update the array
size information if necessary.  Return TRUE if an error is detected in
the reconciliation process.
*/
{
  a_variable_ptr	var;
  a_boolean		incompatible_ptr_to_member_class_types = FALSE;
  a_boolean		err = FALSE;

  var = sym->variant.static_data_member.variable;
  if (!types_are_redecl_compatible(type_ptr, var->type)) {
    /* Types are not compatible. */
    if (microsoft_bugs &&
        f_types_are_compatible(type_ptr, var->type,
                               TCF_REDECLARATION |
                               TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |
                               TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE)) {
      /* The incompatibility amounts to some difference in the class type
         specified in a pointer to member type that is part of the type of
         the static data member.  This is allowed in Microsoft-bugs mode. */
      pos_sy_warning(ec_not_compatible_with_previous_decl, err_pos, sym);
      incompatible_ptr_to_member_class_types = TRUE;
    } else {
      pos_sy_error(ec_not_compatible_with_previous_decl, err_pos, sym);
      err = TRUE;
    }  /* if */
  } else if ((is_ptr_or_ref_type(type_ptr) &&
              is_function_type(type_pointed_to(type_ptr))) ||
             (is_ptr_to_member_type(type_ptr) &&
              is_function_type(pm_member_type(type_ptr)))) {
    /* Check for mismatches in exception specifications. */
    check_exception_specification(type_ptr, sym, err_pos, /*is_redecl=*/TRUE);
  }  /* if */
  if (!err) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Since this is the defining declaration of the static data member,
       record the type.  Note that this has to be done before composite
       type is called -- in case there's some modification. */
    check_assertion(var->declared_type == NULL);
    var->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (incompatible_ptr_to_member_class_types) {
      /* Microsoft bug -- leave the static data member type (or for
         incomplete arrays the underlying array element type) as it was
         originally declared. */
      if (is_array_type(var->type)) {
        a_type_ptr     array_type, new_type;
        check_assertion(is_array_type(type_ptr));
        array_type = skip_typerefs(var->type);
        if (is_incomplete_type(array_type)) {
          /* The static data member was originally declared as an array
             of unknown size.  Make a copy of the original type, using
             the size from the current type.  (We have to do it this way
             instead of calling composite_type because the two types are
             not actually compatible.) */
          new_type = alloc_type((a_type_kind)tk_array);
          copy_type(array_type, new_type);
          new_type->variant.array.variant.number_of_elements =
                       skip_typerefs(type_ptr)->
                              variant.array.variant.number_of_elements;
          set_type_size(new_type);
          /* Update the variable entry to point to the new type. */
          var->type = new_type;
        }  /* if */            
      }  /* if */
    } else {
      /* The type of the variable should be the composite of the two
         types. */
      var->type = composite_type(type_ptr, var->type);
    }  /* if */
  }  /* if */
  return err;
}  /* reconcile_static_data_member_types */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void define_static_data_member(a_symbol_locator    *locator,
                                      a_decl_parse_state  *dps,
                                      a_boolean           has_initializer,
                                      an_id_linkage_kind  *linkage_ptr,
                                      a_decl_pos_block    *decl_pos_block)
/*
Enter the definition of a static data member.  *locator gives the symbol
locator (and thus its name and its declaration position).  *dps describes
various properties of the declaration (including type and storage class).
Note that static data members must already have been declared within the
class (or struct or union) of which they are members.  The type must be
compatible with the original declaration, and there must be no explicit
storage class on the current definition.  The storage class of defined
static members should be changed to sc_unspecified.  Return a pointer to
the symbol through dps->sym and its linkage (which is always "none") through
*linkage_ptr.
*/
{
  a_variable_ptr           var;
  a_boolean                err = FALSE;
  a_symbol_ptr             sym;
  a_symbol_reference_kind  srk_flags;

  db_enter(3, "define_static_data_member");
  if ((dps->dso_flags & DSO_CONSTEXPR) != 0 &&
      !is_const_qualified_type(dps->type)) {
    /* constexpr variables are implicitly const. */
    dps->type = make_qualified_type(dps->type, (a_type_qualifier_set)TQ_CONST);
  }  /* if */
  /* This routine is called after a qualified name has been seen, but be sure
     the object is a static data member.  (In invalid programs it could also
     be the name of a nonstatic data member or a member function.) */
  sym = locator->specific_symbol;
  /* A storage class of sc_unspecified means "no storage class explicitly
     specified" -- anything else is an error.  (Some cases -- like "auto" --
     were already checked by the caller.)  An exception is GNU C++ mode, which
     accepts and ignores the "extern" case (with a warning). */
  if (dps->storage_class != (a_storage_class)sc_unspecified) {
    if (dps->declared_storage_class == (a_storage_class)sc_unspecified) {
      /* This can happen with attributes (like dllimport) in error cases.
         The error should be issued elsewhere. */
      expect_error();
    } else {
      an_error_severity  sev = es_error;
      if (gpp_mode && dps->storage_class == (a_storage_class)sc_extern) {
        sev = es_warning;
        dps->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
      pos_diagnostic(sev, ec_storage_class_not_allowed,
                     &dps->storage_class_pos);
    }  /* if */
  }  /* if */
  if (microsoft_mode && sym->kind == (a_symbol_kind)sk_projection) {
    /* In Microsoft compatibility mode it's permitted to define a static
       data member by referring to it as an inherited member.  However, only
       allow this if the reference is unambiguous. */
    if (!sym->ambiguous) sym = fundamental_symbol_of(sym);
  }  /* if */
  if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    dps->is_definition = TRUE;
    dps->sym = sym;
    var = sym->variant.static_data_member.variable;
    if (sym->defined) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
      err = TRUE;
    } else if (!namespace_is_enclosed_by_scope(sym, &scope_stack_top())) {
      /* This static data member is being defined in a scope that does not
         enclose the scope in which the parent class was defined. */
      sym_error(ec_bad_scope_for_definition, sym);
      err = TRUE;
    } else {
      /* Verify that the type supplied on this declaration matches the one
         from the class.  Some differences are allowed.  Create a composite
         type if necessary. */
      err = reconcile_static_data_member_types(sym, dps->type,
                                               &locator->source_position);
    }  /* if */
    if (!err) {
      /* Ordinarily a static data member will have been given a storage class
         of sc_extern; promote it to sc_unspecified, now that the definition
         has been seen.  (In cfront mode the storage class is promoted from
         sc_static at the end of the translation unit.) */
      if (var->storage_class == (a_storage_class)sc_extern) {
        var->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
      if (!var->is_constexpr && (dps->dso_flags & DSO_CONSTEXPR) != 0) {
        pos_sy_error(ec_previous_nonconstexpr_decl_conflict,
                     &dps->specifiers_pos, sym);
        dps->dso_flags &= ~(a_decl_flag_set)DSO_CONSTEXPR;
      }  /* if */
      if (var->is_thread_local !=
          ((dps->dso_flags & DSO_THREAD_LOCAL) == DSO_THREAD_LOCAL)) {
        /* If "thread_local" is specified on one declaration, it must be
           specified on all. */
        pos2_diagnostic(es_error,
                        var->is_thread_local ?
                                     ec_non_thread_local_follows_thread_local :
                                     ec_thread_local_follows_non_thread_local,
                        &locator->source_position,
                        &var->source_corresp.decl_position);
      }  /* if */
      /* Set the IL referenced flag since, as an externally visible variable,
         it could be referenced from another translation unit. */
      var->source_corresp.referenced = TRUE;
      /* If this is a member of an instantiation of a class
         template, set the supress_instantiation field of the variable. */
      if (sym->variant.static_data_member.instance_ptr != NULL) {
        check_old_specialization_allowed(sym, &locator->source_position);
        var->is_specialized = TRUE;
        var->specialized_with_old_syntax = TRUE;
      }  /* if */
      srk_flags = SRK_DECLARATION | SRK_DEFINITION;
      /* Even without an explicit initializer this is an initializing
         declaration it is the static data member is nontrivially
         constructible -- i.e., if it is a class object (or array of class)
         and the class has a nontrivial default constructor (which must be a
         user-declared default constructor if the static data member's type
         is const qualified -- WP 7.1.5.1 [dcl.type.cv]). */
      if (has_initializer ||
          is_const_qualified_type(var->type) ?
            type_has_user_provided_default_constructor(var->type) :
            type_has_nontrivial_default_constructor(var->type)) {
        srk_flags |= SRK_INITIALIZATION;
      }  /* if */
      record_symbol_declaration(srk_flags, sym, &locator->source_position,
                                dps->source_sequence_entry);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (record_name_references_in_context()) {
        a_name_reference_ptr  name_ref;
        name_ref = qualifiable_name_reference(locator, &var->source_corresp);
        name_ref->used_in_primary_declarator = TRUE;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      attach_decl_attributes(dps, /*primary_decl=*/TRUE);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      update_decl_pos_info(&var->source_corresp, decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
  } else {
    /* Not a static data member (but a member of some sort, since it is a
       qualified name).  Issue the appropriate error. */
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* Nonstatic data members (fields) cannot be defined. */
      pos_error(ec_nonstatic_member_def_not_allowed,
                &locator->source_position);
    } else if (is_member_function_symbol(sym)) {
      /* A member function -- this is treated as a type incompatibility. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
    } else if (!namespace_is_enclosed_by_scope(sym, &scope_stack_top())) {
      /* The member is being defined in a scope that does not enclose the
         scope in which the parent class was defined. */
      sym_error(ec_bad_scope_for_definition, sym);
    } else if (sym->kind == (a_symbol_kind)sk_projection ||
               sym->is_nonreal_member) {
      /* A member of a base class (or assumed to be a member of a nonreal base
         class). */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else if (sym->kind != (a_symbol_kind)sk_undefined &&
               !is_error_locator(*locator)) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
    }  /* if */
    err = TRUE;
  }  /* if */
  if (err) {
    /* An error occurred which prevents using the object specified as
       target of any initialization that may follow.  Create a dummy
       variable with an error type (to suppress semantic errors on the
       initialization, if any). */
    a_type_ptr           tp = sym_parent_class(sym);
    a_symbol_header_ptr  hdr = locator->symbol_header;
    a_variable_ptr       vp;
    /* Record the symbol declaration, using the original symbol, even
       though there was an error.  This will make it show up on a cross
       reference listing. */
    record_symbol_declaration(SRK_DECLARATION, sym, &locator->source_position,
                              dps->source_sequence_entry);
    /* "Enter" the symbol using an error locator -- this means a symbol
       entry will be created but it will not be added to any lists.  Then
       we'll restore the header to the new symbol, so that the correct name
       will be available in diagnostics. */
    set_to_error_locator(*locator);
    sym = enter_symbol((a_symbol_kind)sk_static_data_member,
                       locator, DEPTH_OF_FILE_SCOPE,
                       /*suppress_redecl_error=*/TRUE);
    sym->header = hdr;
    vp = make_variable(error_type(), (a_storage_class)sc_static,
                       depth_innermost_namespace_scope);
    sym->variant.static_data_member.variable = vp;
    set_source_corresp(&(vp->source_corresp), sym);
    /* Make the error symbol a class member -- it is expected of
       sk_static_data_member symbols downstream. */
    set_class_membership(sym, (a_source_correspondence*)NULL, tp);
  }  /* if */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  *linkage_ptr = idl_none;
  dps->sym = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* define_static_data_member */


static void remove_any_inherited_type_synonym(a_symbol_locator  *locator,
					      a_symbol_ptr	sym)
/*
Microsoft and g++ compilers accept code like:
  struct B { typedef int I; };
  struct D: B {
    typedef I J;      // Uses B::I
    typedef double I; // Introduces D::I
  };
To emulate this, we must remove projections of a type synonymous with the
type being declared.

"sym" is a projection symbol found in the scope class scope in which the
typedef is being declared.
*/
{
  if (!sym->variant.projection.is_using_decl) {
    remove_symbol(sym);
  }  /* if */
  clear_specific_symbol(*locator);
}  /* remove_any_inherited_type_synonym */


static void set_name_linkage_for_enumerators(a_type_ptr  tp)
/*
The given type should be an enumeration type.  This routine ensures its
associated enumerator constants are assigned the same name linkage as the
type itself.
*/
{
  a_constant_ptr  enumerator = enum_constants(tp);

  for (; enumerator != NULL; enumerator = enumerator->next) {
    enumerator->source_corresp.name_linkage = tp->source_corresp.name_linkage;
  }  /* for */
}  /* set_name_linkage_for_enumerators */


static void set_linkage_for_class_members(a_type_ptr  tp)
/*
The given type should be a class type.  If it acquired linkage through a
typedef, we must make sure to propagate that to its members.
*/
{
  a_scope_ptr          scope;
  a_routine_ptr        routine;
  a_variable_ptr       var;
  a_type_ptr           type;
  a_name_linkage_kind  name_linkage = tp->source_corresp.name_linkage;

  check_assertion(!C_mode() && is_immediate_class_type(tp));
  scope = tp->variant.class_struct_union.extra_info->assoc_scope;
  if (scope != NULL) {
    for (routine = scope->routines; routine != NULL; routine = routine->next) {
      routine->source_corresp.name_linkage = name_linkage;
      if (name_linkage == (a_name_linkage_kind)nlk_cplusplus_external ||
          name_linkage == (a_name_linkage_kind)nlk_external) {
        routine->storage_class = (a_storage_class)
                                  (routine->assoc_scope != NULL_region_number ?
                                                   sc_unspecified : sc_extern);
      }  /* if */
    }  /* for */
    for (var = scope->variables; var != NULL; var = var->next) {
      var->source_corresp.name_linkage = name_linkage;
      if (name_linkage == (a_name_linkage_kind)nlk_cplusplus_external ||
          name_linkage == (a_name_linkage_kind)nlk_external) {
        var->storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* for */
    for (type = scope->types; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        set_name_linkage_for_type(type);
        set_linkage_for_class_members(type);
      } else if (is_immediate_enum_type(type)) {
        set_name_linkage_for_type(type);
        set_name_linkage_for_enumerators(type);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* set_linkage_for_class_members */


static a_boolean dependent_typedef_redecl_allowed(a_type_ptr	type1,
						  a_type_ptr	type2)
/*
Return TRUE if type1 and/or type2 are template dependent types that may
end up being compatible during an actual instantiation.
*/
{
  a_boolean	result = FALSE;

  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* dependent_typedef_redecl_allowed */

#if DECL_MODIFIERS_IN_USE

static void diagnose_decl_modifiers_on_type_declaration(
                                                   a_decl_parse_state  *state) 
/*
At least one extended declaration modifier appeared on a type declaration.
Issue a diagnostic if the modifier is invalid.
*/
{
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED || SUN_EXTENSIONS_ALLOWED
  a_decl_modifier    flags = state->decl_modifiers.flags;
  a_source_position  *pos = &state->start_pos;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED || SUN_EXTENSIONS_ALLOWED */
  
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (flags & DM_THREAD) {
    pos_error(ec_cannot_use_thread_local_storage, pos);
    flags &= ~(a_decl_modifier)DM_THREAD;
  }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_... */
#if SUN_EXTENSIONS_ALLOWED
  if (flags & DM_ANY_SUN_LINK_SCOPE) {
    pos_diagnostic(es_discretionary_error, ec_invalid_link_scope, pos);
    flags &= ~(a_decl_modifier)DM_ANY_SUN_LINK_SCOPE;
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
}  /* diagnose_decl_modifiers_on_type_declaration */

#endif /* DECL_MODIFIERS_IN_USE */

#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
void decl_typedef(a_symbol_locator             *locator,
                  a_decl_parse_state           *state,
                  a_type_ptr                   class_type,
                  a_decl_pos_block_ptr         decl_pos_block)
/*
Enter the declaration of an identifier for a typedef.  *locator gives the
symbol locator (and thus its name and its declaration position).  *state
describes various properties of the declaration.  If this is a member typedef,
class_type identifies the class of which it is a member.  Create and enter a
symbol entry, and return a pointer to it in state->sym.
*/
{
  a_type_ptr               tp = NULL, type_ptr = state->type;
  a_symbol_ptr             sym = NULL;
  a_boolean                is_redecl = FALSE;
  a_boolean                suppress_redecl_error = FALSE;
  a_boolean                saved_referenced_flag;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];
  a_namespace_ptr          nsp;
  a_symbol_ptr             loc_sym;

  db_enter(3, "decl_typedef");
  /* typedefs (and alias declarations, which also come through here) are
     usually permitted in constexpr bodies.  (They aren't if they define a
     class or enumeration type, but that is checked elsewhere.) */
  state->decl_okay_in_constexpr_body = TRUE;
  if (state->auto_type != NULL && !state->has_trailing_return_type) {
    /* An "auto" type specifier is not allowed in a typedef declaration. */
    pos_error(ec_auto_not_allowed_here, &state->auto_pos);
    state->auto_type_specifier_seen = FALSE;
    state->auto_type = NULL;
    invalidate_type(state);
    type_ptr = error_type();
  }  /* if */
  /* Apply type-transforming GNU attributes (like vector_size) early.  (This
     is needed so e.g. type compatibility can be established.)  However, the
     attributes should be recorded in the typedef rather than the underlying
     type, because GCC accepts
       typedef int Vec __attribute((vector_size(16), aligned(16)));
     but rejects
       typedef int __attribute((vector_size(16))) Vec
                                                   __attribute((aligned(16)));
     */
  transform_type_with_gnu_attributes(&type_ptr, state->id_attributes,
                                     (void*)state);
  sym = curr_scope_id_lookup(locator, IDL_PROJ_SYMBOL_ALLOWED);
  loc_sym = locator->specific_symbol;
  if (loc_sym != NULL && loc_sym->kind == (a_symbol_kind)sk_projection) {
    check_assertion(sym != NULL);
    if ((microsoft_mode || gpp_mode) &&
        ssep->kind == (a_scope_kind)sck_class_struct_union) {
      remove_any_inherited_type_synonym(locator, loc_sym);
      sym = NULL;
    } else if (loc_sym->variant.projection.is_using_decl) {
      /* If the symbol found is from a using-declaration, ignore it.  This
         will result in an error when the new symbol is entered. */
      sym = NULL;
    } else if (is_type_template_param_symbol(sym)) {
      /* A case like:
           template<typename T> struct S: T {
             typedef typename T::X X;
           };
         In this example, sym would be the result of projecting T::X.
         Such a projection does not conflict with any typedef in the
         current class scope. */
      suppress_redecl_error = TRUE;
      sym = NULL;
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    /* This name already exists in the current scope.  C++ allows a
       redefinition of the typedef with the same type, and we allow that
       also in C.  See if this is a redefinition. */
    a_boolean	dependent_typedef_redeclaration = FALSE;
    if (sym->kind == (a_symbol_kind)sk_type ||
        (C_dialect == C_dialect_cplusplus && is_type_symbol(sym))) {
      /* sym is a type name symbol from the current scope.  Issue an error
         if this is an illegal redefinition of the name; otherwise, reuse
         the existing symbol. */
      a_boolean  types_are_identical;
      tp = type_symbol_type(sym);
      types_are_identical = identical_types(tp, type_ptr);
      if (!types_are_identical) {
        /* If the types are not the same, see if they are dependent types
           that could turn out to be the same. */
        a_symbol_ptr nonreal_sym = NULL;
        if (sym->corresp_nonreal_or_nested_type != NULL &&
            !sym->is_nonreal_nested_type) {
          nonreal_sym = sym->corresp_nonreal_or_nested_type;
        }  /* if */
        dependent_typedef_redeclaration =
                            (sym->kind == (a_symbol_kind)sk_type &&
                             is_template_dependent_context() &&
                             dependent_typedef_redecl_allowed(tp, type_ptr)) ||
                             (nonreal_sym != NULL &&
                              identical_types(type_ptr,
                                              nonreal_sym->variant.type.ptr));
      }  /* if */
      if (((types_are_identical || dependent_typedef_redeclaration)
#if NEAR_AND_FAR_ALLOWED
           /* When near/far qualifiers appear, they have to match what was
              explicitly specified. */
           && (!near_and_far_enabled() ||
               (get_original_type_qualifiers(tp) ==
                   get_original_type_qualifiers(type_ptr)))
#endif /* NEAR_AND_FAR_ALLOWED */
                                                           ) ||
          is_error_type(tp) ||
          (microsoft_bugs && C_mode() && is_integral_type(type_ptr) &&
           interchangeable_types(tp, type_ptr))) {
        /* The current declaration simply redefines the name to the same
           type, which is permitted in C++ (ARM 7.1.3) and warned about for
           ordinary C.  In Microsoft C mode we also accept a redeclaration
           to an integral type that is "similar" to the original. */
        if (C_mode() || is_error_type(tp)) {
          /* C mode is handled below (excludes tag types). */
        } else if (is_tag_symbol_kind(sym->kind)) {
          /* Something like
               typedef struct X {} X;
             which is acceptable in all scopes. */
        } else if (class_type != NULL &&
                   loc_sym->kind != (a_symbol_kind)sk_projection) {
          /* A duplicate (but compatible) declaration in class scope. */
          check_assertion(ssep->kind == (a_scope_kind)sck_class_struct_union);
          if (same_entities(class_type, tp)) {
            /* sym corresponds to the injected class name for the current
               class, which means that we are attempting to create a typedef
               with the same as the enclosing class. */
            pos_diagnostic(strict_ansi_mode ? es_error : es_warning,
                           ec_class_and_member_name_conflict,
                           &locator->source_position);
          } else if (strict_ansi_mode || sun_mode) {
            /* Member typedefs cannot be redeclared in strict C++ mode
               (clarified in TC1; see 7.1.3/2 in the 2003 standard).
               Most compilers do not enforce this (and we issue a warning
               below), but Sun compilers do. */
            pos_error(ec_duplicate_typedef_in_class,
                      &locator->source_position);
          } else if (tp->source_corresp.access != ssep->current_access) {
            /* Access for previous declaration does not correspond to access
               for current declaration. */
            pos_sy_warning(ec_cannot_change_access, &locator->source_position, 
                           sym);
            /* Stay with the access specified on the original declaration. */
          } else {
            /* Issue a warning since this is no longer standard C++. */
            pos_warning(ec_duplicate_typedef_in_class,
                        &locator->source_position);
          }  /* if */
        }  /* if */
        /* In C++ we may still need an sk_type symbol, since tags and typedefs
           do not occupy the same name space. */
        if (locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_namespace_projection) {
          /* A typedef declares the same name as a using declaration, and both
             also correspond to the same type.  For example:
               namespace N { typedef int I; }
               using N::I;
               typedef int I;
             Inhibit the redeclaration error.
          */
          sym = NULL;
          suppress_redecl_error = TRUE;
          clear_specific_symbol(*locator);
        } else if (sym->kind == (a_symbol_kind)sk_type) {
          a_symbol_reference_kind  ref_kind = SRK_DECLARATION;
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode && sym == predeclared_size_t_symbol &&
              !sym->defined) {
            a_type_ptr  size_t_type = sym->variant.type.ptr;
            check_assertion(type_is_typedef(size_t_type));
            /* This is a redeclaration of the predeclared symbol for size_t.
               We know it's the first explicit declaration because the defined
               flag is not set. */
            ref_kind |= SRK_DEFINITION;
            /* Retain the underlying type specified in this declaration
               because it may include the __w64 annotation, but beware of
               loops introduced by something like "typedef size_t size_t;". */
            tp = type_ptr;
            while (tp->kind == (a_type_kind)tk_typeref) {
              if (same_entities(tp, size_t_type)) {
                /* Prevent a loop by using the underlying type directly. */
                state->type = type_ptr = tp->variant.typeref.type;
                break;
              }  /* if */
              tp = tp->variant.typeref.type;
            }  /* while */
            size_t_type->variant.typeref.type = type_ptr;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          if (C_mode()) {
            /* Allowing a benign redeclaration is an extension in C, so issue
               a warning. */
            pos_diagnostic(strict_ansi_mode ?
                             strict_ansi_error_severity : es_warning,
                           types_are_identical ?
                             ec_duplicate_typedef : ec_similar_typedef,
                           &locator->source_position);
          }  /* if */
          record_symbol_declaration(ref_kind, sym, &locator->source_position,
                                    state->source_sequence_entry);
          reload_source_sequence_entry(state);
          if (!(ref_kind & SRK_DEFINITION)) {  /*lint !e774*/
#if GENERATE_SOURCE_SEQUENCE_LISTS
            an_sssd_flag_set  sssd_flags = SSSD_NO_FLAGS;
            a_type_ptr        declared_type = type_ptr;
#if GNU_EXTENSIONS_ALLOWED
            if (state->marked_as_gnu_extension) {
              sssd_flags |= SSSD_MARKED_AS_GNU_EXTENSION;
            }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
            /* When we applied attributes, we may have created a typeref entry
               to apply attributes to later on, but that entry isn't needed for
               a redeclaration. */
            if (declared_type->kind == (a_type_kind)tk_typeref &&
                declared_type->variant.typeref.for_type_attributes &&
                declared_type->source_corresp.attributes == NULL) {
              declared_type = declared_type->variant.typeref.type;
            }  /* if */
            (void)update_src_seq_secondary_decl((char *)sym->variant.type.ptr,
                                                declared_type,
                                                (a_name_reference_ptr)NULL,
                                                sssd_flags, decl_pos_block);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          } else {
#if EXTRA_SOURCE_POSITIONS_IN_IL
            /* The first explicit declaration of size_t in Microsoft mode
               (see above).  Set the extended position information. */
            update_decl_pos_info(&sym->variant.type.ptr->source_corresp,
                                 decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          }  /* if */
          is_redecl = TRUE;
        } else {
          /* C++ only.  Must be a tag symbol. */
          suppress_redecl_error = TRUE;
        }  /* if */
      } else if (gcc_mode && is_enum_type(type_ptr) && is_integral_type(tp) &&
                 skip_typerefs(type_ptr)->variant.integer.int_kind ==
                                skip_typerefs(tp)->variant.integer.int_kind &&
                 seq_is_in_system_header(sym->decl_position.seq)) {
        /* In GNU C mode, a typedef for an enum type can replace a typedef for
           the type underlying the enum type if the earlier typedef appeared
           in a system header. */
        sym = NULL;
        suppress_redecl_error = TRUE;
      } else {
        /* This typedef statement redefines a type name to a different type.
           -- diagnostic is issued in enter_symbol processing. */
      }  /* if */
    } else {
      /* Either the symbol was not declared in the current scope, in which
         case a redefinition here is legal, or else it is not a type name
         symbol, in which case the error message will be issued by
         enter_symbol. */
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus && !is_error_locator(*locator)) {
    /* No symbol by this name.  See if this is a tagless type for which the
       typedef name will serve as the "name for linkage purposes" (WP 7.1.3
       [dcl.typedef]).  If so, set the name pointer in the type entry to
       point to the same name as the current typedef name. */
    a_type_ptr  type_to_check = type_ptr, assoc_template_param = NULL;
    a_boolean   is_class_or_enum, via_typeof = FALSE;
#if GNU_EXTENSIONS_ALLOWED
    if (gpp_mode) {
      /* Normally, a typedef imbues a name for linkage purposes only when it
         directly contains a class type definition.  E.g.:
           typedef struct {} S;
         However, GNU compilers also allow this to happen through a typeof
         construct:
           struct {} x;
           typedef typeof(x) S;   // "S" is the name for linkage purposes.
           typedef typeof(x) S2;  // "S2" isn't the name for linkage purposes,
                                  // since one is established already. */
      while (type_to_check->kind == (a_type_kind)tk_typeref &&
             type_to_check->variant.typeref.is_typeof) {
        via_typeof = TRUE;
        type_to_check = type_to_check->variant.typeref.type;
      }  /* while */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* If the typedef refers to the nonreal version of a nested type of a
       class template, use the real version as the type to check for linkage
       purposes. */
    { a_symbol_ptr	type_sym;
      a_symbol_ptr	nested_sym;
      type_sym = symbol_for(type_to_check);
      if (type_sym != NULL) {
        nested_sym = type_sym->corresp_nonreal_or_nested_type;
        if (nested_sym != NULL) {
          assoc_template_param = type_to_check;
          type_to_check =  type_symbol_type(nested_sym);
        }  /* if */
      }  /* if */
    }
    is_class_or_enum = is_immediate_class_type(type_to_check) ||
                       is_immediate_enum_type(type_to_check);
    tp = NULL;
    if (is_class_or_enum) {
      if (type_to_check->source_corresp.name == NULL) {
        /* A class/struct/union or enum type with no name. */
        a_symbol_ptr  tag_sym = symbol_for(type_to_check);
        /* Through deduction templates are sometimes instantiated with
           unnamed enum or class types: In such cases, the unnamed type (which
           is a template argument) will be in a scope that's outside that of
           the current scope (which is created for the template instantiation
           after the template arguments have been determined).  A typedef in
           such an instantiation should not affect the name of the type.  In
           GNU C++ mode, the scope comparison check cannot always be done
           because a typeof construct can bring in an unnamed type from
           another scope to give it a name for linkage purposes. */
        if (via_typeof ||
            tag_sym->decl_scope == scope_stack[decl_scope_level].number) {
          tp = type_to_check;
        }  /* if */
      }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
    } else {
      /* Normally, inferring a linkage name from a typedef name is allowed
         only for unqualified class/struct/union and enum types.  However, in
         cfront's name mangling scheme it is also done when there is a type
         qualifier on top of the tagless class or enum:
           typedef struct { ... } A;         // linkage name "A" (all modes)
           typedef const struct { ... } B;   // linkage name "B" (cfront mode)
           typedef enum { ... } C;           // linkage name "C" (all modes)
           typedef const enum { ... } D;     // linkage name "D" (cfront mode)
      */
      if (is_class_struct_union_type(type_to_check) ||
          is_enum_type(type_to_check)) {
        if (skip_typedefs(type_to_check) == type_to_check &&
            skip_typerefs(type_to_check)->source_corresp.name == NULL) {
          /* A possibly qualified class or enum type with no name.  Get at the
             underlying type. */
          tp = skip_typerefs(type_to_check);
        }  /* if */
      }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */
    }  /* if */
    if (tp != NULL) {
      if (any_cfront_mode()) {
        /* An unnamed tag symbol was created for the class and can be reused
           now that we have a name to assign to it.  We need to unlink it
           from the symbol table, give it the name, and relink it into the
           symbol table. */
        sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
        check_assertion(sym != NULL && is_unnamed_tag_symbol(sym));
        relink_unnamed_tag_symbol(sym, locator);
        /* Call set_source_corresp, but preserve the current IL referenced
           setting, which set_source_corresp will clear. */
        saved_referenced_flag = tp->source_corresp.referenced;
        set_source_corresp(&(tp->source_corresp), sym);
        tp->source_corresp.referenced = saved_referenced_flag;
        suppress_redecl_error = TRUE;
        /* Note that we do not look for conflicts between the class's new
           name and the names of its members.  This is an area where the
           wording of the ARM (7.1.3) has been clarified and/or amended by
           the C++98 standard, and so the restrictions specified in
           ARM 9.2 do not apply. */
      } else {
        /* The typedef name is the name of the class or enum "for linkage
           purposes".  That means the typedef name should be recorded in the
           source correspondence field for the type.  However, we won't
           reenter the symbol into the symbol table; this keeps the typedef
           name from being used in an elaborated type specifier (7.1.3 para 5,
           9.1 para 5). */
        /* We also assign a name to class and enum types when one of their
           cv-qualified forms has been typedefed: that name is used to
           mangle functions that make use of these types. */
        /* tp cannot point to a typeref at this point since it was either a
           direct class or enum type, or it was obtained after a skip_typerefs
           operation. */
        if (is_class_or_enum ||
            (!has_name(tp) && (is_immediate_class_type(tp) ||
                               is_immediate_enum_type(tp)))) {
          /* Note that in non-cfront mode this is done only for types that
             actually do have linkage. */
#if NEED_NAME_MANGLING
          a_boolean  recompute_discriminator = FALSE;
          if (type_to_check == type_ptr &&
              symbol_for(tp)->decl_scope ==
                                       scope_stack[decl_scope_level].number) {
            /* A discriminator was assigned to the unnamed type, but now it
               turns out not to be unnamed for mangling purposes after all.
               (The type_to_check == type_ptr check ensures that that this
               is not the GNU case where a typeof(...) construct renames an
               anonymous type.  The second test excludes unnamed types that
               come in as template arguments.) */
            cancel_name_collision_discriminator(symbol_for(tp),
                                                decl_scope_level);
            recompute_discriminator = TRUE;
          }  /* if */
#endif /* NEED_NAME_MANGLING */
          set_source_corresp_name(&tp->source_corresp, locator->symbol_header);
          if (assoc_template_param != NULL &&
              !has_name(assoc_template_param)) {
            /* Also apply the name to the corresponding nonreal type in
               prototype instantiation contexts. */
            set_source_corresp_name(&assoc_template_param->source_corresp,
                                    locator->symbol_header);
          }  /* if */
#if NEED_NAME_MANGLING
          if (recompute_discriminator) {
            /* The new name may cause a collision of its own: Compute a new
               discriminator if needed. */
            compute_name_collision_discriminator(symbol_for(tp),
                                                 decl_scope_level);
          }  /* if */
#endif /* NEED_NAME_MANGLING */
          if (!is_class_or_enum && !any_cfront_mode()) {
            tp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Recompute the name linkage. */
      if (is_class_or_enum) {
        set_name_linkage_for_type(tp);
        if (is_immediate_class_type(tp)) {
          set_linkage_for_class_members(tp);
        } else {
          set_name_linkage_for_enumerators(tp);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_redecl) {
    /* Create a new type entry and add it to the types list for the current
       scope.  If applying attributes to the underlying type yielded a new
       tk_typeref entry for the purposes of attaching attributes, we can
       reuse that entry (this ensures that e.g. the may_alias flag is set on
       the same entry that records the associated ak_may_alias attribute). */
    if (type_ptr->kind == (a_type_kind)tk_typeref &&
        type_ptr->variant.typeref.for_type_attributes &&
        type_ptr->source_corresp.attributes == NULL) {
      type_ptr->variant.typeref.for_type_attributes = FALSE;
      tp = type_ptr;
      type_ptr = tp->variant.typeref.type;
    } else {
      tp = alloc_type((a_type_kind)tk_typeref);
      tp->variant.typeref.type = type_ptr;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS
    if (state->marked_as_gnu_extension) {
      tp->source_corresp.marked_as_gnu_extension = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Create a new symbol for this type and bind it to the new type. */
    sym = enter_typedef_symbol(tp, locator, decl_scope_level,
                               suppress_redecl_error);
    set_source_corresp(&(tp->source_corresp), sym);
    state->first_decl = TRUE;
    nsp = NULL;
    if (!C_mode()) {
      if (class_type != NULL) {
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (class_type->variant.class_struct_union.is_interface) {
          pos_error(ec_interface_cannot_have_typedef,
                    &locator->source_position);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        set_class_membership(sym, &tp->source_corresp, class_type);
        tp->source_corresp.access = ssep->current_access;
#if MICROSOFT_EXTENSIONS_ALLOWED
        tp->source_corresp.assembly_access = ssep->current_assembly_access;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        set_namespace_membership(sym, &tp->source_corresp,
                                 (a_namespace_ptr)NULL);
        nsp = parent_namespace_or_null(tp);
      }  /* if */
      if (microsoft_bugs) {
        /* The Microsoft C++ compiler effectively ignores a typedef of a named
           class type to the same name (and declared in the same scope).  E.g.:
             typedef struct S {} S;
             void S();  // Accepted in Microsoft mode.
           Enumeration types are treated similarly. */
        a_type_ptr  unqual_type = skip_typerefs(type_ptr);
        if (((is_immediate_class_type(unqual_type) &&
              !unqual_type->variant.class_struct_union.is_template_class) ||
             is_immediate_enum_type(unqual_type)) &&
            symbol_for(unqual_type)->header == sym->header &&
            symbol_for(unqual_type)->decl_scope == sym->decl_scope &&
            identical_types(tp, unqual_type)) {
          sym->is_invisible = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                              &locator->source_position,
                              state->source_sequence_entry);
    reload_source_sequence_entry(state);
#if NEED_NAME_MANGLING
    if (tp->source_corresp.is_local_to_function &&
        !tp->source_corresp.is_class_member) {
      /* Local typedefs may need to be mangled.  If two (or more) such types
         in a function have the same name, a discriminator must be appended to
         the mangled name (this is not strictly an ABI issue, but dictated by
         our use of a C-generating back end). */
      compute_name_collision_discriminator(sym, decl_scope_level);
    }  /* if */
#endif /* NEED_NAME_MANGLING */
#if BACK_END_IS_CP_GEN_BE
    /* Set the "name linkage environment" for this type.  This is used by the
       C++-generating back end to decide when to emit extern "C". */
    tp->variant.typeref.surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
#endif /* BACK_END_IS_CP_GEN_BE */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&tp->source_corresp, decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    add_to_types_list(tp, decl_scope_level);
    /* Issue a diagnostic if size_t is declared in a way inconsistent with
       the target configuration. */
    if (!is_error_type(type_ptr) &&
        (decl_scope_level == DEPTH_OF_FILE_SCOPE ||
         (nsp != NULL &&
          nsp == symbol_for_namespace_std->variant.namespace_info.ptr)) &&
        strcmp(sym->header->identifier, "size_t") == 0) {
      /* "size_t" declared at file scope or in namespace "std". */
      if (!is_integral_type(type_ptr) ||
          skip_typerefs(type_ptr)->variant.integer.int_kind !=
                                                  targ_size_t_int_kind ||
          is_qualified_type(type_ptr)) {
        pos_ty_warning(ec_unexpected_type_for_size_t,
                       &locator->source_position,
                       integer_type(targ_size_t_int_kind));
      }  /* if */
    }  /* if */
    if (vla_enabled && innermost_function_scope != NULL) {
      /* A typedef declaration inside a function. */
      if (is_variably_modified_type(type_ptr)) {
        a_statement_ptr  sp;
        
        sp = add_statement_at_stmt_pos((a_statement_kind)stmk_vla_decl,
                                       &locator->source_position);
        sp->variant.vla.is_typedef_decl = TRUE;
        sp->variant.vla.variant.typedef_type = tp;
        tp->variant.typeref.has_variably_modified_type = TRUE;
        check_for_vla_inside_statement_expression(&locator->source_position);
      }  /* if */
    }  /* if */
  }  /*if */
  /* Return the type name symbol to the caller. */
  state->sym = sym;
  attach_decl_attributes(state, /*primary_decl=*/!is_redecl);
  if (!is_redecl && !is_error_type(type_ptr)) {
    if (!tp->source_corresp.is_deprecated) {
      /* Check if a deprecated type was involved in this declaration. */
      warn_about_use_of_deprecated_type(type_ptr, &locator->source_position);
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && !is_error_type(type_ptr)) {
    if (state->ms_attributes != NULL) {
      apply_microsoft_attributes_to_type(&state->ms_attributes, tp);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(state->sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_typedef */


static a_boolean implicitly_predeclared_gcc_function(a_symbol_ptr  sym)
/*
Early versions of GNU C predeclare the "exit" function implicitly; i.e., the
declaration is not visible by default, but if "exit" is called, then it becomes
visible in the file scope.  This function returns TRUE if sym represents
"exit".
*/
{
  a_boolean result = FALSE;

  if (gcc_mode && gnu_version < 30400 &&
      sym->kind == (a_symbol_kind)sk_routine) {
    a_const_char *name = sym->header->identifier;
    if (name != NULL && strcmp(name, "exit") == 0) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* implicitly_predeclared_gcc_function */


void decl_default_function(a_symbol_ptr symbol_ptr)
/*
Declare the given symbol as a default function.  This routine is called
when a previously-unknown identifier appears in an expression followed by
a left parenthesis, indicating that it is an undeclared function.  The
symbol has already been entered as an undefined symbol.
*/
{
  an_id_linkage_kind     linkage;
  a_type_ptr             rout_type, old_type;
  a_symbol_ptr           ext_sym;
  a_symbol_locator       locator;
  a_memory_region_number region_to_switch_back_to;
  a_func_info_block      func_info;
  a_decl_parse_state     dps;

  db_enter(4, "decl_default_function");
  /* Change the symbol kind to routine.  Note that the symbol has already
     been entered.  Fortunately, "undefined" and "routine" are in the
     same name space, so that proper checking for a duplicate definition
     will have already been done (there's a check in symbol_tbl_init
     that the name spaces are the same). */
  set_symbol_kind(symbol_ptr, (a_symbol_kind)sk_routine);
  /* In pcc mode, all routines are entered at file scope level.  Remove
     and re-enter the symbol (if necessary) so it will be there.  (In some
     GNU C modes, some implicit declarations are also treated this way.) */
  if (C_dialect == C_dialect_pcc ||
      (gcc_mode && implicitly_predeclared_gcc_function(symbol_ptr))) {
    if (symbol_ptr->decl_scope != file_scope_number) {
      /* Take the symbol out of the symbol table. */
      remove_symbol(symbol_ptr);
      /* Put the symbol back into the symbol table at the file scope level. */
      reenter_symbol(symbol_ptr, DEPTH_OF_FILE_SCOPE, /*suppress_error=*/TRUE);
    }  /* if */
  }  /* if */
  /* All IL routines and their types must be at the file scope level, so switch
     to that memory region if necessary. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Generate the function type.  In C mode indicate it has an old-style
     no-information parameter list and a return type of "int".  See 3.3.2.2,
     semantics.  In C++ this must be an error, so give it a return type of
     tk_error and call it prototyped. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.extra_info->param_type_list = NULL;
  if (C_mode()) {
    rout_type->variant.routine.return_type =
                                       integer_type((an_integer_kind)ik_int);
    rout_type->variant.routine.extra_info->prototyped = FALSE;
  } else {
    /* Making the return type an error type prevents cascading errors. */
    rout_type->variant.routine.return_type = error_type();
    rout_type->variant.routine.extra_info->prototyped = TRUE;
    /* Setting has_ellipsis to TRUE means no diagnostics will be issued for
       having too many arguments. */
    rout_type->variant.routine.extra_info->has_ellipsis = TRUE;
  }  /* if */
  make_locator_for_symbol(symbol_ptr, &locator);
  /* Declare the function identifier. */
  clear_func_info(&func_info);
  func_info.is_implicit_declaration = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  func_info.declared_type = form_declared_type(rout_type, &func_info);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (exceptions_enabled) func_info.throw_position = locator.source_position;
  init_decl_parse_state(&dps);
  dps.declared_storage_class = dps.storage_class = (a_storage_class)sc_extern;
  dps.type = rout_type;
  dps.sym = symbol_ptr;
  decl_routine(&locator, &dps, &func_info, (SRK_DECLARATION | SRK_IMPLICIT),
               &linkage, &old_type, &ext_sym, (a_decl_pos_block_ptr)NULL);
  done_with_func_info(func_info);
  /* Set the referenced flag on the routine entry.  The implicit declaration
     is also an immediate reference. */
  dps.sym->variant.routine.ptr->source_corresp.referenced = TRUE;
  switch_back_to_original_region(region_to_switch_back_to);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(dps.sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_default_function */


a_label_ptr scan_label(a_boolean  is_definition,
                       a_boolean  is_declaration)
/*
Scan a label as part of a statement label, a goto statement, or a GNU local
label declaration.  Return a pointer to the IL label.  The current token
should be a label identifier.  is_definition is TRUE if the label is being
scanned as part of a label definition.  is_declaration is TRUE if the label
is being scanned as part of a GNU local label declaration.
*/
{
  a_symbol_ptr  label_sym;
  a_label_ptr   label;
  a_boolean     err = FALSE;

  a_source_position start_pos;

  db_enter(3, "scan_label");

  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    (void)required_token(tok_identifier, ec_exp_identifier);
    set_to_error_locator(locator_for_curr_id);
    label_sym = NULL;
    err = TRUE;
  } else {
    /* See if the label identifier is already in the symbol table. */
    label_sym = find_label_symbol(locator_for_curr_id.symbol_header);
    if (is_declaration && label_sym != NULL) {
      /* For a local label declaration, we have to check whether the
         new declaration is in the same scope as the one found. */
      if (label_sym->decl_scope == scope_stack[decl_scope_level].number) {
        /* A duplicate declaration. */
        sym_error(ec_already_defined, label_sym);
        err = TRUE;
      } else {
        /* The new declaration is going to hide the old one. */
        label_sym = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  if (label_sym == NULL) {
    /* Enter the label identifier into the symbol table.  This is normally
       done at the function level even if we are inside some blocks.  The
       exception is a GNU local label declaration.  Use a locator with an
       undefined source position; the decl_position will be handled
       explicitly shortly. */
    a_memory_region_number region_to_switch_back_to;
    a_scope_depth          depth;
    check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH);
    depth = is_declaration ? decl_scope_level : depth_innermost_function_scope;
    locator_for_curr_id.source_position = null_source_position;
    label_sym = enter_symbol((a_symbol_kind)sk_label, &locator_for_curr_id,
                             depth, /*suppress_error=*/TRUE);
    /* Allocate the IL label and attach it to the symbol.  Be careful to
       allocate it in the current function scope memory region (the file scope
       memory region may be the default when e.g. dealing with label addresses
       appearing in template arguments). */
    switch_to_scope_region(depth_innermost_function_scope,
                           &region_to_switch_back_to);
    label_sym->variant.label.ptr = label = alloc_label();
    switch_back_to_original_region(region_to_switch_back_to);
#if GNU_EXTENSIONS_ALLOWED
    label->locally_declared = is_declaration;
#endif /* GNU_EXTENSIONS_ALLOWED */
    add_to_labels_list(label);
    set_source_corresp(&label->source_corresp, label_sym);
    /* The exec_stmt field stays NULL to indicate that the declaration
       has not been (fully?) processed yet. */
  }  /* if */
  if (!err) {
    /* Record the right kind of reference to the label symbol. */
    if (is_definition) {
      /* Note that we want mark_defined is called even if the symbol
         was previously entered.  Labels are strange in that a reference
         can come up before a declaration. */
      mark_defined(label_sym, &pos_curr_token);
    } else if (is_declaration) {
      mark_declared(label_sym, &pos_curr_token);
    } else {
      mark_referenced(label_sym, &pos_curr_token);
      /* Set the decl_position in case no declaration shows up, so we
         have the location of the use. */
      if (label_sym->decl_position.seq == 0 &&
          label_sym->decl_position.column == SP_COL_UNKNOWN) {
        copy_source_position(pos_curr_token, label_sym->decl_position);
      }  /* if */
    }  /* if */
    /* Advance past the identifier. */
    (void)get_token();
  }  /* if */

  copy_source_position(start_pos, error_position);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(label_sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(label_sym->variant.label.ptr);
}  /* scan_label */


a_boolean simplify_curr_class_qualified_name(void)
/*
If the current token is the start of a qualified name in which the class
name component is the name of a class currently being defined, advance past
the class name and the "::" so that the current token is a non-qualified
name.  Return TRUE if such a modification is done and FALSE otherwise.
This routine is called in C++ only.

This functionality is provided to deal with declarations of class members
where a qualified name is used instead of a simple name, e.g., when a
constructor for class A is declared A::A() rather than A().  This is
nonstandard, but it is allowed by cfront and other compilers.  GNU C++
also allows this for member templates.
*/
{
  a_boolean                is_member_id = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];

  db_enter(3, "simplify_curr_class_qualified_name");

  if (gpp_mode && ssep->kind == (a_scope_kind)sck_template_declaration) {
    /* In GNU mode, the simplification is also performed for the declarators
       of member templates. */
    --ssep;
  }  /* if */
  if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
      is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL) &&
      locator_for_curr_id.is_qualified_name) {
    if (same_entities(qualifier_class_type(locator_for_curr_id),
                      ssep->assoc_type) &&
        locator_for_curr_id.is_global_qualified_name == FALSE) {
      is_member_id = TRUE;
      /* Reset the fields in the locator to make it appear as if the
         qualifier was not present. */
      clear_qualifier_from_locator(&locator_for_curr_id);
      if (any_cfront_mode() || microsoft_bugs) {
        /* No diagnostic, to be consistent with cfront's and Microsoft's
           behavior.  In the Microsoft case, we may also end up here for
           friend declarations ("struct S { friend void S::f(); };") and
           dropping the qualifier may result in the injection of the name
           in namespace scope. */
      } else {
        /* Accepting qualified member names is an extension -- issue a
           diagnostic. */
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_qualifier_in_member_declaration);
      }  /* if */ 
    }  /* if */
  }  /* if */
  db_exit();
  return is_member_id;
}  /* simplify_curr_class_qualified_name */


void report_missing_type_specifier(a_source_position  *err_pos,
                                   a_type_ptr         type,
                                   a_boolean          is_function,
                                   a_boolean          is_function_def,
                                   a_boolean          is_main_func,
                                   a_boolean          any_decl_specifiers)
/*
No type was explicitly specified for the current declaration.  Issue the
appropriate diagnostic at the source position given by *err_pos.
type points to the type, which may have derived type levels on
top of the underlying implicitly-generated type.  is_function is TRUE
if this is a function declaration; is_function_def is TRUE if it is
a function declaration that is also a definition; is_main_func is TRUE
if it is a declaration of global scope "main".  any_decl_specifiers is
TRUE if at least one decl-specifier was seen (e.g., a storage class or
cv-qualifier).
Note that this routine determines whether the "implicit int" rule applies.
*/
{
  an_error_code      error_code;
  an_error_severity  severity = es_none;
  a_type_ptr         bottom_type;

  bottom_type = find_bottom_of_type(type);
  if (is_error_type(bottom_type) || is_unknown_type(bottom_type)) {
    /* An error was previously issued on the specifiers type, so do
       not issue another diagnostic. */
    goto done;
  }  /* if */
  /* Different modes call for different severities.  Most diagnostics involve
     one of two messages depending on whether any decl-specifiers were seen at
     all, but the global function "main" is handled with a different error
     code (in case discretionary-error control for "main" should be
     independent of that for other functions). */
  error_code = is_main_func        ? ec_implicit_int_on_main :
               any_decl_specifiers ? ec_missing_type_specifier :
                                     ec_missing_decl_specifiers;
  if (C_dialect == C_dialect_pcc) {
    /* pcc mode is the most permissive when it comes to diagnosing missing
       type specifiers.  Only non-function cases are diagnosed, and the
       diagnostic is just a warning. */
    if (!is_function) {
      severity = es_warning;
    }  /* if */
  } else if (C_mode() && (!c99_mode || microsoft_mode || gcc_mode)) {
    /* In C89 modes the "implicit int" rule applies in most cases, but it's
       nonstandard for declarations that aren't function definitions and
       have no decl-specifiers at all (e.g., "f();").  The standard cases
       usually deserve a remark. */
    /* The combination of Microsoft mode and C99 mode is treated like a C89
       mode in this respect (Microsoft compilers currently don't have a true
       C99 mode).  GNU C doesn't enforce the standard constraints in its C99
       mode either. */
    if (is_function) {
      /* The "main" function is silently accepted without any specifiers. */
      if (!is_main_func) {
        if (!any_decl_specifiers && !is_function_def) {
          /* Something like "f();". */
          severity = strict_ansi_mode ?
                         strict_ansi_discretionary_severity : es_warning;
        } else {
          /* Something like "static f();" or "f() { ... }".  If the are no
             decl_specifiers but a function definition follows, the diagnostic
             is for the missing type specifier rather than for the missing
             specifiers in general.  (The message indicates that "int" is
             implicit.) */
          severity = (gcc_mode && c99_mode) ? es_warning : es_remark;
          error_code = ec_missing_type_specifier;
        }  /* if */
      }  /* if */
    } else {
      /* A non-function declaration. */
      if (!any_decl_specifiers) {
        /* Microsoft and GNU compilers are as permissive as pcc in this
           case. */
        severity = (microsoft_mode || gcc_mode) ? es_warning
                                                : es_discretionary_error;
      } else {
        severity = es_warning;
      }  /* if */
    }  /* if */
  } else if (!C_mode() && (any_cfront_mode() ||
                           (microsoft_mode && microsoft_version < 1400)) &&
             !auto_type_specifier_enabled) {
    /* Cfront compilers and early Microsoft C++ compilers are fairly
       permissive and allow "implicit int" in most cases.  However, if "auto"
       may appear as a type specifier, we fall back on the standard
       constraints. */
    if (is_main_func) {
      severity = es_remark;
    } else if (is_function) {
      severity = any_cfront_mode() ? es_remark : es_warning;
      error_code = ec_nonstd_implicit_int;
    } else if (!any_decl_specifiers) {
      severity = es_discretionary_error;
    } else {
      severity = es_warning;
      error_code = ec_nonstd_implicit_int;
    }  /* if */
  } else {
    /* Apply the standard C++/C99 constraints.  Implicit int is normally not
       accepted, but some allowances are made in nonstrict modes for "main".
       If "auto" may appear as a type specifier, diagnostics are emitted as if
       we were in strict mode. */
    if (is_main_func) {
      severity = (strict_ansi_mode || auto_type_specifier_enabled) ?
                    strict_ansi_discretionary_severity : es_remark;
    } else {
      severity = es_discretionary_error;
      if (is_function) {
        /* Function declaration in C++ or C99 mode.  Issue the same message
           whether or not any_decl_specifiers is TRUE and whether this is a
           definition or merely a declaration.  That is, all the following are
           treated the same way:
             a();
             b() {}
             extern c();
             extern d() {}
        */
        error_code = ec_missing_type_specifier;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Unless the diagnostic is suppressed (e.g., many pcc cases), put out the
     diagnostic. */
  if (severity != es_none) {
    pos_diagnostic(severity, error_code, err_pos);
  }  /* if */
done:;
}  /* report_missing_type_specifier */


static an_attribute_ptr extract_gnu_attributes(an_attribute_ptr  *p_list)
/*
Extract and return the GNU attributes from the list of attributes pointed to
by *p_list (the list can be empty; i.e., *p_list can be NULL).
*/
{
  an_attribute_ptr  gnu_list = NULL, *p_end = &gnu_list, *p_ap;

  /* Extract GNU attributes. */
  for (p_ap = p_list; *p_ap != NULL;) {
    if ((*p_ap)->family == (a_byte_attribute_family)af_gnu) {
      *p_end = *p_ap;
      *p_ap = (*p_ap)->next;
      p_end = &(*p_end)->next;
      *p_end = NULL;
    } else {
      p_ap = &(*p_ap)->next;
    }  /* if */
  }  /* for */
  return gnu_list;
}  /* extract_gnu_attributes */


static void process_type_name_attributes(a_decl_parse_state  *dps)
/*
This routine is called from type_name_full to handle non-type-transforming
attributes that might be recorded in dps->prefix_attributes and
dps->id_attributes.  GNU attributes may be valid in this context (in
particular, the "aligned" attribute) and are therefore applied to dps->type.
Other attributes are invalid and are diagnosed.
*/
{
  an_attribute_ptr  gnu_list, ap;

  gnu_list = extract_gnu_attributes(&dps->id_attributes);
  *f_last_attribute_link(&gnu_list) =
                              extract_gnu_attributes(&dps->prefix_attributes);
  if (gnu_list != NULL) {
    for (ap = gnu_list; ap != NULL; ap = ap->next) ap->assoc_info = (void*)dps;
    /* Create a typeref to attach the attributes to (the attachment is done
       by the call to attach_attributes). */
    dps->type = make_typeref_with_attributes(dps->type, NULL);
    attach_attributes(gnu_list, (char*)dps->type, iek_type);
    for (ap = gnu_list; ap != NULL; ap = ap->next) ap->assoc_info = NULL;
  }  /* if */
  if (dps->prefix_attributes != NULL || dps->id_attributes != NULL) {
    diagnose_unattached_attributes(dps->prefix_attributes);
    diagnose_unattached_attributes(dps->id_attributes);
  }  /* if */
}  /* process_type_name_attributes */


void type_name_full(a_decl_parse_state  *dps)
/*
Scan a type-name (a sequence of specifiers optionally followed by an abstract
declarator) and record information about it in *dps.  In particular, dps->type
returns the representation of the type.  Callers can constrain the type by
setting certain fields in *dps (e.g., to disallow variably modified types).
See type_name for a convenient way to scan type names (via this function) in
common cases.
*/
{
  a_decl_flag_set              dsi_flags, di_flags;

  db_enter(3, "type_name_full");
  set_err_pos_to_curr_token();
  dps->is_type_name = TRUE;
  dps->trailing_return_type_allowed = trailing_return_types_enabled;
  copy_source_position(pos_curr_token, dps->start_pos);
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED | DSI_NO_REAL_DECLARATOR;
  if (dps->is_trailing_return_type) {
    /* When scanning a C++11 trailing return type, top-level class definitions
       should not be considered.  E.g., in "[]()->struct S {}" the "{}" is
       considered to be the lambda body; not the definition of S. */
    dsi_flags |= DSI_NO_TAG_DEFINITION;
  }  /* if */
  /* Allow specifier attributes. */
  if (std_attributes_enabled)  dsi_flags |= DSI_STD_ATTRIBUTES_ALLOWED;
  if (gnu_attributes_enabled) dsi_flags |= DSI_GNU_ATTRIBUTES_ALLOWED;
  decl_specifiers(dsi_flags, dps, (a_decl_pos_block_ptr)NULL);
  check_assertion(dps->type != NULL);
  if (!(dps->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    report_implicit_int(&dps->start_pos, dps->specifiers_type);
  }  /* if */
  (skip_typerefs(dps->type))->source_corresp.referenced = TRUE;
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_abstract_declarator_start()) {
    /* A declarator follows the type specifiers. */
    di_flags = DI_ABSTRACT_DECLARATOR_ALLOWED | DI_QUALIFIED_NAME_ALLOWED;
    if (vla_enabled && depth_innermost_function_scope != NO_SCOPE_DEPTH &&
        !dps->disallow_variably_modified_type) {
      /* Note that int[*] is not allowed, but int(*)[*] is okay.  Therefore
         we turn on DI_VLA_ASTERISK_ALLOWED and check for the error case
         once the scan has been completed. */
      di_flags |= DI_VLA_ALLOWED | DI_VLA_ASTERISK_ALLOWED;
    }  /* if */
    declarator(di_flags, dps, /*member_parent_type=*/(a_type_ptr)NULL,
               (a_symbol_locator *)NULL, (a_func_info_block_ptr)NULL,
               (a_decl_pos_block_ptr)NULL);
    if (di_flags & DI_VLA_ALLOWED) {
      /* VLA checking was done. */
      if (is_array_type(dps->type) &&
          is_or_contains_vla_type_with_unspecified_bound(dps->type)) {
        /* This is an array in which the variable bound is unspecified in
           one of its dimensions. */
        pos_error(ec_vla_with_unspecified_bound_not_allowed, &dps->start_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  check_use_of_auto_type(dps);
  if ((any_cfront_mode() &&
       check_member_function_typedef(dps->type, &dps->start_pos)) ||
      is_unknown_type(dps->type)) {
    /* If the type is of the unknown kind, presumably an error occurred and
       hence an error type should be returned.  If the type is a cfront-style
       member function typedef -- it is an error to use it anywhere but in a
       pointer-to-member declaration. */
    invalidate_type(dps);
  }  /* if */
  check_pending_qualifiers_used(dps);
  if (dps->prefix_attributes != NULL || dps->id_attributes != NULL) {
    if (!is_error_type(dps->type)) {
      process_type_name_attributes(dps);
    }  /* if */
  }  /* if */
  copy_source_position(dps->start_pos, error_position);
  run_end_of_parse_actions(dps, /*more_declarators=*/FALSE);
  db_exit();
}  /* type_name_full */


void check_type_definition_in_type_name(a_decl_parse_state  *dps)
/*
dps describes a type-name scanned by type_name_full.  In C++ mode, issue an
error if the specifiers in the type-name defined a class or enum type.  (Early
GNU C++ modes are an exception: They do allow such definitions.  However, if
lambdas are enabled in such a mode, we do not emulate that g++ extension to
avoid problems with closure types in function prototype scopes.  Similarly, we
don't permit type definitions at all in the type-names of alias templates to
avoid inconsistencies later on.)
*/
{
  if ((dps->dso_flags & DSO_DEFINES_SOMETHING) != 0 && !C_mode() &&
      !(gpp_mode && gnu_version < 30400 && !lambdas_enabled &&
        !dps->is_alias_template_type)) {
    pos_error(ec_type_definition_not_allowed, &dps->start_pos);
    dps->type = error_type();
  }  /* if */
}  /* check_type_definition_in_type_name */


void type_name(a_type_ptr  *p_type)
/*
Scan a type-name and set *p_type to the scanned type.  In C++, issue an error
if the type-name includes a class or enum definition.
*/
{
  a_decl_parse_state  dps;

  init_decl_parse_state(&dps);
  type_name_full(&dps);
  check_type_definition_in_type_name(&dps);
  *p_type = dps.type;
}  /* type_name */


a_type_ptr scan_type_for_cast(a_boolean  const_expr_context,
                              a_boolean  *explicit_cv_qualifiers,
                              a_boolean  *type_definition)
/*
Scan a type for a cast or the cast-like construct of a compound literal and
return a pointer to its representation.  If explicit_cv_qualifiers is non-NULL,
return in *explicit_cv_qualifiers whether the type included explicit top-level
cv-qualifiers.  If type_definition is non-NULL, return in *type_definition
whether the scanned type specifiers included a class or enum definition;
otherwise, issue a diagnostic on such a definition if appropriate.
const_expr_context is TRUE if the cast appears in a context that requires a
constant-expression.
*/
{
  a_decl_parse_state  dps;

  init_decl_parse_state(&dps);
  dps.disallow_variably_modified_type = const_expr_context;
  type_name_full(&dps);
  if (type_definition != NULL) {
    /* Return whether a type was defined in the type specifiers. */
    *type_definition = (dps.dso_flags & DSO_DEFINES_SOMETHING) != 0;
  } else {
    check_type_definition_in_type_name(&dps);
  }  /* if */
  if (explicit_cv_qualifiers != NULL) {
    /* Return whether the type included explicit top-level cv-qualifiers. */
    if (dps.declarator_start_pos.seq != 0) {
      /* A declarator was scanned. */
      *explicit_cv_qualifiers = is_top_level_qualified_type(dps.type);
    } else {
      *explicit_cv_qualifiers = (dps.qualifiers != TQ_NONE &&
                                 !dps.unused_qualifiers);
    }  /* if */
  }  /* if */
  return dps.type;
}  /* scan_type_for_cast */


a_type_ptr scan_type_for_sizeof(a_boolean  evaluated_context)
/*
Scan a type-name that is the argument to a sizeof operator and return the
corresponding IL entry.  evaluated_context is TRUE if the sizeof operator
will itself be evaluated.
*/
{
  a_decl_parse_state  dps;

  init_decl_parse_state(&dps);
  dps.is_evaluated_sizeof_type_arg = evaluated_context;
  type_name_full(&dps);
  check_type_definition_in_type_name(&dps);
  return dps.type;
}  /* scan_type_for_sizeof */


a_type_ptr scan_template_type_argument(void)
/*
Scan a template type argument.  The heavy lifting for this routine is done by
type_name_full.
*/
{
  a_decl_parse_state  dps;

  init_decl_parse_state(&dps);
  dps.is_template_type_argument = TRUE;
  dps.disallow_variably_modified_type = TRUE;
  type_name_full(&dps);
  check_type_definition_in_type_name(&dps);
  return dps.type;
}  /* scan_template_type_argument */


void new_type_name(a_decl_parse_state  *state,
                   a_boolean           is_parenthesized)
/*
Scan a C++ new-type-name or a parenthesized type-name that may appear in a
"new" expression, and return a pointer to the type through state->type.
*state also records various aspects of the type name parsing process (in
particular, information about any use of the "auto" specifier).
The syntax is:

   new-type-id:
              type-specifier-seq new-declarator(opt)

   new-declarator:
              ptr-operator new-declarator(opt)
              noptr-new-declarator

   noptr-new-declarator:
              [ expression ]
              noptr-new-declarator [ constant-expression ]

This syntax allows only a restricted form of types, but other types can be
specified by enclosing a type-name in parentheses.  If is_parenthesized is
TRUE, the caller has already trapped the left parenthesis for such a
construct.  The parenthesis is also checked from within this routine if
is_parenthesized comes in FALSE.
*/
{
  a_type_ptr                  complete_type, new_type_ptr;
  a_type_ptr                  derived_type, bottom_derived_type;
  a_decl_flag_set             dsi_flags;
  a_decl_pos_block            decl_pos_block;
  a_boolean                   rparen_in_new_declarator = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_boolean                   end_pos_set = FALSE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  db_enter(3, "new_type_name");
  /* Check for the parenthesized form. */
  if (!is_parenthesized && curr_token == tok_lparen) {
    is_parenthesized = TRUE;
    (void)get_token();
  }  /* if */
  if (is_parenthesized) add_stop_token(tok_rparen);
  set_err_pos_to_curr_token();
  clear_decl_pos_block(&decl_pos_block);
  copy_source_position(pos_curr_token, state->start_pos);
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED | DSI_IS_NEW_TYPE_NAME |
              DSI_NO_REAL_DECLARATOR;
  decl_specifiers(dsi_flags, state, &decl_pos_block);
  if (state->dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &state->start_pos);
  } else if (!(state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    report_implicit_int(&error_position, state->specifiers_type);
  }  /* if */
  if (state->type != NULL) {
    (skip_typerefs(state->type))->source_corresp.referenced = TRUE;
  }  /* if */
  if (gpp_mode && gnu_version < 30400 && is_parenthesized &&
      curr_token == tok_rparen && next_token() == tok_lbracket) {
    /* GNU compilers accept new-expressions like "new (int)[n]" where the
       "[n]" is part of the type specifier.  (It also accepts forms like
       "new (int[n])[3]", which are handled in the call to declarator
       below.) */
    rparen_in_new_declarator = TRUE;
    is_parenthesized = FALSE;
    (void)get_token();
    remove_stop_token(tok_rparen);
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_parenthesized) {
    /* In the parenthesized form, the full declarator syntax is allowed. */
    if (is_abstract_declarator_start()) {
      declarator(DI_ABSTRACT_DECLARATOR_ALLOWED |
                    DI_QUALIFIED_NAME_ALLOWED |
                    DI_DIMENSION_EXPRESSION_ALLOWED,
                 state, /*member_parent_type=*/(a_type_ptr)NULL,
                 (a_symbol_locator *)NULL, (a_func_info_block_ptr)NULL,
                 &decl_pos_block);
    }  /* if */
    if (state->do_flags & DO_RPAREN_IN_NEW_DECLARATOR) {
      /* We parsed something like "new (int[n])[3]" in a GNU C++ mode.  The
         right parenthesis was consumed as part of declarator processing. */
      check_assertion(gpp_mode && gnu_version < 30400);
    } else {
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (curr_token == tok_rparen) {
        /* Remember the right parenthesis position as the end position. */
        curr_construct_end_position = end_pos_curr_token;
        end_pos_set = TRUE;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_stop_token(tok_rparen);
    }  /* if */
  } else {
    /* In the non-parenthesized form, a limited declarator syntax is
       allowed. */
    a_boolean  ptr_to_member_scanned;
    /* Scan pointer declarators. */
    complete_type = pointer_declarator(state->type, state,
                                       /*reference_allowed=*/TRUE,
				       (a_call_conv_descr_ptr)NULL,
				       (a_call_conv_descr_ptr)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       &ptr_to_member_scanned,
                                       &decl_pos_block);
    derived_type = NULL;
    bottom_derived_type = NULL;
    add_stop_token(tok_lbracket);
    if (curr_token == tok_lbracket) {
      /* Scan array declarators.  The first one allows an expression
         as the size; the others require a constant size. */
      array_declarator(state, &new_type_ptr, /*nonconstant_allowed=*/TRUE,
                       /*vla_is_allowed=*/FALSE,
                       /*vla_asterisk_allowed=*/FALSE,
                       /*threads_dimension_allowed=*/FALSE,
                       /*top_level_field_decl=*/FALSE,
                       /*top_level_param_decl=*/FALSE,
                       &decl_pos_block);
      add_to_derived_type_list(
                     new_type_ptr, &derived_type, &bottom_derived_type, state,
                     /*parameter_type=*/FALSE);
      if (rparen_in_new_declarator) {
        /* A form like "new (int)[n]" is accepted in some GNU C++ modes: Only
           one array declarator level is permitted after the right
           parenthesis. */
      } else {
        while (curr_token == tok_lbracket) {
          array_declarator(state, &new_type_ptr, /*nonconstant_allowed=*/FALSE,
                           /*vla_is_allowed=*/FALSE,
                           /*vla_asterisk_allowed=*/FALSE,
                           /*threads_dimension_allowed=*/FALSE,
                           /*top_level_field_decl=*/FALSE,
                           /*top_level_param_decl=*/FALSE,
                           &decl_pos_block);
          /* Add the new type to the bottom of the existing derived type list.
             Note that this involves error checking. */
          add_to_derived_type_list(
                     new_type_ptr, &derived_type, &bottom_derived_type, state,
                     /*parameter_type=*/FALSE);
        }  /* while */
      }  /* if */
      if (derived_type != NULL) {
        if (complete_type != NULL) {
          if (!is_error_type(bottom_derived_type)) {
            /* Combine derived_type and complete_type. */
            add_to_derived_type_list(
                     complete_type, &derived_type, &bottom_derived_type, state,
                     /*parameter_type=*/FALSE);
          }  /* if */
        }  /* if */
        complete_type = derived_type;
      }  /* if */
    }  /* if */
    remove_stop_token(tok_lbracket);
    if (ptr_to_member_scanned &&
        check_for_vla_in_pointer_to_member(complete_type, &state->start_pos)) {
      /* Complete type is or contains a pointer-to-member to a variably
         modified type, which is an error. */
      complete_type = error_type();
    }  /* if */
    state->type = state->declared_type = complete_type;
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (end_pos_set) {
    /* End position set already, for parenthesized case. */
  } else if (decl_pos_block.declarator_range.end.seq != 0) {
    curr_construct_end_position = decl_pos_block.declarator_range.end;
  } else {
    curr_construct_end_position = decl_pos_block.specifiers_range.end;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  check_pending_qualifiers_used(state);
  if (any_cfront_mode() &&
      check_member_function_typedef(state->type, &state->start_pos)) {
    /* The type is a cfront-style member function typedef -- it is an error
       to use it anywhere but in a pointer-to-member declaration. */
    invalidate_type(state);
  }  /* if */
  run_end_of_parse_actions(state, /*more_declarators=*/FALSE);
  db_exit();
}  /* new_type_name */

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED

a_type_ptr simple_type_specifier_sequence(void)
/*
Scan a sequence of simple-type-specifiers and return a pointer to the
resulting type.  This is called in Microsoft and GNU modes for function-
style casts where the type involves more than one token -- e.g.,
"enum E(x)".
*/
{
  a_type_ptr              type_ptr;
  a_decl_pos_block        decl_pos_block;
  a_decl_parse_state      state;

  check_assertion(microsoft_mode || gpp_mode);
  init_decl_parse_state(&state);
  clear_decl_pos_block(&decl_pos_block);
  decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED | DSI_NO_REAL_DECLARATOR, &state,
                  &decl_pos_block);
  type_ptr = state.specifiers_type;
  check_pending_qualifiers_used(&state);
  /* Set error_position to the start of the type-specifier sequence. */
  error_position = state.start_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = decl_pos_block.specifiers_range.end;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return type_ptr;
}  /* simple_type_specifier_sequence */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */

a_boolean scan_conversion_operator(
                             a_boolean                        is_class_member,
                             a_parent_class_or_namespace_ptr  parent,
                             a_type_ptr                       field_sel_type)
/*
The current token is "operator".  If it is followed by the start of a type
name we have an identifier for a conversion operator -- scan the whole
construct, update the locator, and return TRUE.  If it doesn't, return FALSE.
If the class or namespace pointed to by parent is not NULL then push a class
or namespace reactivation scope before scanning the type name of the
conversion operator.  is_class_member is TRUE if the parent points to a class,
it is FALSE if parent points to a namespace or if there is no parent.  If
field_sel_type is not NULL, it is the type of the left operand of a field
selection operation associated with this operator function reference.
*/
{
  a_type_ptr              complete_type;
  a_source_position       type_pos, start_pos;
  a_boolean               is_conversion_operator;
  a_boolean               class_reactivated = FALSE;
  a_boolean               namespace_reactivated = FALSE;
  a_decl_pos_block        decl_pos_block;
  a_scope_stack_entry_ptr ssep;

  db_enter(3, "scan_conversion_operator");
  start_pos = pos_curr_token;
  /* Push a class or namespace reactivation scope if the class or namespace
     pointed to by parent is not NULL.  This is used when scanning conversion
     operators such as "A::operator B" where B needs to be looked up within
     A.  This is not needed for overloaded operator routines, but we
     don't know what kind of operator we are scanning until we call
     is_type_start, and the class needs to be reactivated before
     is_type_start is called. */
  if (is_class_member) {
    if (field_sel_type == NULL &&
        parent->class_type != NULL &&
        !is_incomplete_type(parent->class_type)) {
      a_symbol_ptr	sym;
      sym = symbol_for(parent->class_type);
      /* In valid usage, the class type will always be either a complete
         real class type or a prototype instantiation.  In other cases,
         suppress the reactivation because incomplete and nonreal classes
         cannot be reactivated.  An error will be issued elsewhere for these
         cases.  The class is not reactivated for references that follow
         field selections.  Instead, a dual lookup of the next identifier
         is done. */
      if (sym != NULL && 
          (is_real_class_symbol(sym) ||
           is_prototype_instantiation_symbol(sym))) {
        push_class_reactivation_scope(parent->class_type,
                                      /*extend_namespace=*/FALSE);
        class_reactivated = TRUE;
      }  /* if */
    }  /* if */
  } else if (parent != NULL && parent->namespace_ptr != NULL) {
    push_namespace_reactivation_scope(parent->namespace_ptr);
    namespace_reactivated = TRUE;
  }  /* if */
  /* If this is part of a field selection operation, record the type of the
     left hand side in the scope stack entry.  This is needed to do the dual
     lookup of conversion operator names. */
  if (field_sel_type != NULL) {
    ssep = &scope_stack[depth_scope_stack];
    ssep->conversion_parent_type = field_sel_type;
    ssep->qualified_conversion_operator = is_class_member && parent != NULL;
  }  /* if */
  /* Bypass the "operator" keyword. */
  (void)get_token();
  if (is_type_start(/*is_expr_context=*/FALSE) ||
      (gpp_mode && curr_token == tok_attribute)) {
    /* It is the start of a type name. */
    a_boolean           ptr_to_member_scanned;
    a_decl_parse_state  state;
    a_decl_flag_set     input_flags;
    is_conversion_operator = TRUE;
    set_err_pos_to_curr_token();
    copy_source_position(pos_curr_token, type_pos);
    init_decl_parse_state(&state);
    clear_decl_pos_block(&decl_pos_block);
    input_flags = DSI_TYPE_SPECIFIER_ALLOWED |
                  DSI_NO_REAL_DECLARATOR |
                  DSI_NO_TAG_DEFINITION;
    if (std_attributes_enabled) input_flags |= DSI_STD_ATTRIBUTES_ALLOWED;
    if (gnu_attributes_enabled) input_flags |= DSI_GNU_ATTRIBUTES_ALLOWED;
    if (deduced_return_types_enabled) {
      state.auto_type_allowed = TRUE;
    }  /* if */
    decl_specifiers(input_flags, &state, &decl_pos_block);
    if (state.dso_flags & DSO_DEFINES_SOMETHING) {
      /* Definition of a class, struct, union, or enum type is not allowed. */
      pos_error(ec_type_definition_not_allowed, &type_pos);
    } else if (!(state.dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
      /* Missing type specifier. */
      report_implicit_int(&error_position, state.specifiers_type);
    }  /* if */
    /* Reset the conversion parent information in the scope stack in case
       the type did not involve an identifier. */
    ssep = &scope_stack[depth_scope_stack];
    ssep->conversion_parent_type = NULL;
    ssep->qualified_conversion_operator = FALSE;
    complete_type = pointer_declarator(state.specifiers_type, &state,
                                       /*reference_allowed=*/TRUE,
                                       (a_call_conv_descr_ptr)NULL,
                                       (a_call_conv_descr_ptr)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       &ptr_to_member_scanned,
                                       &decl_pos_block);
    if (any_cfront_mode() &&
        check_member_function_typedef(complete_type, &type_pos)) {
      /* The type is a cfront-style member function typedef -- it is an error
         to use it anywhere but in a pointer-to-member declaration. */
      complete_type = error_type();
    } else if (ptr_to_member_scanned &&
               check_for_vla_in_pointer_to_member(complete_type, &type_pos)) {
      /* Complete type is or contains a pointer-to-member to a variably
         modified type, which is an error. */
      complete_type = error_type();
    }  /* if */
    unget_token();
    curr_token = tok_identifier;
    pos_curr_token = error_position = start_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* After backing up one token, the end position needs to be reset too. */
    if (decl_pos_block.declarator_range.end.seq != 0) {
      end_pos_curr_token = decl_pos_block.declarator_range.end;
    } else {
      end_pos_curr_token = decl_pos_block.specifiers_range.end;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (is_qualified_type(complete_type)) {
      a_boolean  err = FALSE;
      report_bad_return_type_qualifier(complete_type, &state, &error_position,
                                       &err);
      if (err) complete_type = error_type();
    }  /* if */
    check_pending_qualifiers_used(&state);
    make_type_conversion_locator(complete_type, &locator_for_curr_id,
                                 &start_pos);
  } else {
    is_conversion_operator = FALSE;
  }  /* if */
  /* Pop the class or namespace reactivation scope if one was
     pushed earlier. */
  if (class_reactivated) {
    pop_class_reactivation_scope();
  } else if (namespace_reactivated) {
    pop_namespace_reactivation_scope();
  } else {
    /* Reset the scope stack fields.  This will normally have been done
       already when scanning the type above, but just in case, it is
       reset here. */
    ssep = &scope_stack[depth_scope_stack];
    ssep->conversion_parent_type = NULL;
    ssep->qualified_conversion_operator = FALSE;
  }  /* if */
  db_exit();
  return is_conversion_operator;
}  /* scan_conversion_operator */


a_type_ptr type_keyword(void)
/*
If the current token is a type keyword (e.g., int, long); return the type
indicated by the keyword, otherwise, return NULL.  This is used in scanning
a simple-type-name for C++ functional-notation casts.  The current token is
not advanced.  Note that this routine does not deal with identifiers that
are defined as types, only keywords; it also does not accept multi-token
types, e.g., "unsigned int".  See ARM 7.1.6 and 5.2.3.
*/
{
  a_type_ptr type;

  check_assertion(!C_mode());
  switch (curr_token) {
    case tok_char:
      type = integer_type((an_integer_kind)ik_char);
      break;
    case tok_short:
      type = integer_type((an_integer_kind)ik_short);
      break;
    case tok_int:
    case tok_signed:
      type = integer_type((an_integer_kind)ik_int);
      break;
    case tok_long:
      type = integer_type((an_integer_kind)ik_long);
      break;
    case tok_unsigned:
      type = integer_type((an_integer_kind)ik_unsigned_int);
      break;
    case tok_float:
      type = float_type((a_float_kind)fk_float);
      break;
    case tok_double:
      type = float_type((a_float_kind)fk_double);
      break;
    case tok_void:
      type = void_type();
      break;
    case tok_wchar_t:
      type = wchar_t_type();
      break;
    case tok_bool:
      type = bool_type();
      break;
    case tok_char16_t:
      type = char16_t_type();
      break;
    case tok_char32_t:
      type = char32_t_type();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The following tokens may or may not be defined, but for each that is,
       the corresponding integer kind will be set to something besides
       ik_none. */
    case tok_int8:
      type = integer_type(targ_int8_int_kind);
      break;
    case tok_int16:
      type = integer_type(targ_int16_int_kind);
      break;
    case tok_int32:
      type = integer_type(targ_int32_int_kind);
      break;
    case tok_int64:
      type = integer_type(targ_int64_int_kind);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
    case tok_int128:
      type = integer_type((an_integer_kind)ik_int128);
      break;
#endif /* INT128_EXTENSIONS_ALLOWED */
    default:
      type = NULL;
      break;
  }  /* switch */
  return type;
}  /* type_keyword */


void record_lint_argsused_and_varargs_state(a_symbol_ptr  rout_sym)
/*
Set fields in the routine type to reflect the current argsused and varargs
state, as indicated by a comment immediately preceding the current function
definition.
*/
{
  a_pending_pragma_ptr           ppp;
  a_routine_type_supplement_ptr  rtsp = NULL;
  
  rtsp = routine_symbol_type(rout_sym)->variant.routine.extra_info;
  /* Determine whether a lint argsused comment immediately preceded this
     function definition. */
  ppp = extract_specific_pragmas((a_pragma_kind)pk_lint_argsused, rout_sym,
                                 (a_statement_ptr)NULL,
                                 /*curr_scope_only=*/FALSE);
  if (ppp != NULL) {
    /* There is a currently active argsused comment. */
    rtsp->lint_argsused_flag = TRUE;
    /* The pending-pragma entry has been unlinked from the scope stack entry
       list, but it still must be returned to the available list. */
    free_pending_pragma_list(ppp);
  }  /* if */
  if (!rtsp->prototyped) {
    /* Determine whether a lint varargs count comment immediately preceded this
       function definition. */
    ppp = extract_specific_pragmas((a_pragma_kind)pk_lint_varargs_count,
                                   rout_sym, (a_statement_ptr)NULL,
                                   /*curr_scope_only=*/FALSE);
    if (ppp != NULL) {
      /* There is a currently active varargs comment. */
      rtsp->lint_varargs_count = ppp->variant.lint_varargs_count;
      /* The pending-pragma entry has been unlinked from the scope stack entry
         list, but it still must be returned to the available list. */
      free_pending_pragma_list(ppp);
    }  /* if */
  }  /* if */
}  /* record_lint_argsused_and_varargs_state */


/* ARGSUSED */ /* sp is required for pragma processing functions of type
                  a_next_construct_pragma_function. */
void record_arg_pragma(a_pending_pragma_ptr  ppp,
                       a_symbol_ptr          sym,
                       a_statement_ptr       sp)
/*
A pragma indicating special argument checking (e.g., for printf args) has
been specified immediately before the current declaration.  If the current
declaration declares a function, remember the pragma kind in the function
type, so that it can be referenced during argument processing.
*/
{
  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    routine_symbol_type(sym)->variant.routine.extra_info->arg_pragma =
                                                        ppp->descr_ptr->kind;
  } else {
    /* Diagnostic? */
  }  /* if */
}  /* record_arg_pragma */


#if C_ANACHRONISMS_ALLOWED

static a_boolean is_initializer_start(void)
/*
Return TRUE if the current token appears to be the start of an initializer.
This is used in pcc mode to decide whether or not an old-style initializer
is present when a "=" is not there.
*/
{
  a_boolean    is_init_start = FALSE;
  a_symbol_ptr assoc_symbol;

  if (curr_token == tok_semicolon ||
      curr_token == tok_comma     ||
      curr_token == tok_rbrace    ||
      is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
    /* No initializer present. */
  } else if (curr_token == tok_identifier) {
    /* Identifier -- only consider as start of an initializer if defined
       as something that might be part of an expression. */
    assoc_symbol = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
    if (assoc_symbol != NULL) {
      if (assoc_symbol->kind == (a_symbol_kind)sk_constant ||
          assoc_symbol->kind == (a_symbol_kind)sk_routine  ||
          assoc_symbol->kind == (a_symbol_kind)sk_variable) {
        /* This identifier is defined as an enumeration constant, a routine,
           or a variable, so it might be an initializer.  Note that a variable
           or routine is allowed in that it might get implicitly converted
           to a pointer to that entity. */
        is_init_start = TRUE;
      }  /* if */
    }  /* if */
  } else {
    /* Other tokens (for example, "("); assume this is an initializer. */
    is_init_start = TRUE;
  }  /* if */
  return is_init_start;
}  /* is_initializer_start */

#endif /* C_ANACHRONISMS_ALLOWED */


static a_boolean scan_name_linkage_string(a_name_linkage_kind  *kind)
/*
Scan the string portion of a linkage specification (extern "C", extern "C++",
etc.).  The current token is the string.  Look it up in the set of strings
that may appear in a linkage specification, and if the lookup is successful
return TRUE and set *kind to the corresponding name-linkage kind.
*/
{
  a_boolean            err = FALSE;
  a_name_linkage_kind  local_kind;

  /* ARM 7.4 specifies that the strings "C" and "C++" must be supported,
     but that implementations are permitted to add others, such as "Ada"
     or "FORTRAN".  If changes are made here to support other strings, be
     sure to update the name linkage kind enumeration. */
  if (is_error_constant(&const_for_curr_token)) {
    /* There must have been an error in scanning the string literal (e.g.,
       no closing '"'. */
    err = TRUE;
  } else {
    /* Look for the predefined string ("C++", "C", ...) which the current
       token matches. */
    for (local_kind = (a_name_linkage_kind)nlk_cplusplus_external;
         (int)local_kind < (int)nlk_last;
         local_kind = (a_name_linkage_kind)(local_kind + 1)) {
      if (eq_constants(&const_for_curr_token,
                       &name_linkage_constants[(int)local_kind])) {
        /* Found a matching linkage kind string. */
        break;
      }  /* if */
    }  /* for */
    if (local_kind != (a_name_linkage_kind)nlk_last) {
      /* Return the name-linkage kind to the caller. */
      *kind = local_kind;
    } else {
      /* Bad linkage kind. */
      error(ec_bad_linkage_specifier);
      err = TRUE;
    }  /* if */
  }  /* if */
  /* Return success status to the caller. */
  return !err;
}  /* scan_name_linkage_string */


static void linkage_specification(a_decl_parse_state  *dps)
/*
The caller has determined that we are at the start of a C++ linkage
specification -- that is, the current token is "extern" and it is followed
by a string literal.  The syntax is:

  linkage-specification:
      extern string-literal { declaration-list    }
                                              opt
      extern string-literal declaration

Since linkage specifications nest, the current linkage specifier is saved
in a local variable, the new one is established by updating a global
variable, the declaration(s) are processed, and then the original linkage
specifier is restored.  dps describes the linkage-specification declaration.
*/
{
  a_name_linkage_kind  kind;
  a_boolean            err = FALSE;
  a_source_range       linkage_spec_range;
#if GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS
  a_constant           string_constant;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS */

  db_enter(3, "linkage_specification");
  if (decl_scope_level != depth_innermost_namespace_scope) {
    error(ec_linkage_specifier_not_allowed);
    err = TRUE;
  }  /* if */
  linkage_spec_range.start = dps->start_pos;
  /* Advance to the string literal. */
  (void)get_token();
  check_assertion(curr_token == tok_string_literal);
#if GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS
  string_constant = const_for_curr_token;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  linkage_spec_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* ARM 7.4 specifies that the strings "C" and "C++" must be supported,
     but that implementations are permitted to add others, such as "Ada"
     or "FORTRAN".  If changes are made here to support other strings, be
     sure to update the name linkage kind enumeration. */
  /* Record the new default linkage in the scope stack. */
  if (!scan_name_linkage_string(&kind) || err) {
    /* If there's an error on this linkage-specification declaration, leave
       the default name linkage kind unchanged.  But to simplify processing,
       the push and pop will still be done. */
    kind = scope_stack[depth_scope_stack].default_name_linkage;
  }  /* if */
  /* Save the current default linkage and set the new one. */
  push_name_linkage(kind);
  /* Advance past the string token. */
  (void)get_token();
  /* If a brace enclosed declaration list follows, call declaration
     repeatedly.  If no brace follows, call declaration just once to pick
     up the rest of the current declaration. */
  if (curr_token == tok_lbrace) {
#if GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS
    /* Add a source sequence entry to indicate the start of the block (with a
       matching end-of-construct entry to follow below). */
    a_linkage_spec_block_ptr  lsbp = alloc_linkage_spec_block();
    lsbp->name_string = alloc_unshared_constant(&string_constant);
    lsbp->name_linkage = kind;
    lsbp->position = linkage_spec_range.start;
    add_to_source_sequence_list((char*)lsbp,
                                (an_il_entry_kind)iek_linkage_spec_block);  
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS */
    /* Issue diagnostics on pragmas that are trying to bind to the
       extern "C" (or whatever) construct. */
    cannot_bind_to_curr_construct();
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    /* Go through the declarations. */
    while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
      declaration(dps->function_definition_allowed,
                  dps->is_old_style_param_decl,
                  /*is_top_level_declaration=*/FALSE,
                  /*marked_as_gnu_extension=*/FALSE, dps->param_id_list,
                  (a_source_range *)NULL);
    }  /* while */
    /* Restore the default linkage to the value it had before the declaration
       (or declaration list) was processed.  Note that this must be done
       before advancing past the closing brace -- there is a dependency in
       precompiled header processing on the state maintained in the scope
       stack entry. */
    pop_name_linkage();
#if GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS
    /* Add a source sequence entry marking the end of the namespace
       definition. */
    add_end_of_construct_source_sequence_entry(
                  (char *)lsbp, (a_byte_il_entry_kind)iek_linkage_spec_block);
    lsbp->end_position = pos_curr_token;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && GENERATE_LINKAGE_SPEC_BLOCKS */
    /* Check for the final right brace of the linkage specification block,
       but don't advance past it -- that is handled in translation_unit. */
    remove_stop_token(tok_rbrace);
    if (curr_token != tok_rbrace) {
      pos_error(ec_exp_rbrace, &pos_curr_token);
    } else {
      /* Advance past right brace.  If the current declaration is a top-level
         declaration, set a global flag to enable checking for a header
         stop. */
      if (dps->is_top_level_declaration) {
        next_token_is_top_level_decl_start = TRUE;
      }  /* if */
      (void)get_token();
      next_token_is_top_level_decl_start = FALSE;
    }  /* if */
  } else {
    if (curr_token == tok_end_of_source) {
      /* Missing declaration. */
      error(ec_exp_declaration);
      pop_name_linkage();
    } else {
      /* Just one declaration is governed by this linkage specifier.  If no
         storage class is specified it is as though "extern" were specified --
         this is an interpretation of the sentence in ARM 7.4 asserting, "An
         object defined withing an `extern "C" {...}' construct is still
         defined and not just declared," and of the example following it,
         where without the braces the variable is not defined. */
      a_decl_parse_state  *saved_dps = scope_stack_top().decl_parse_state;
      scope_stack_top().decl_parse_state = dps;
      declaration(dps->function_definition_allowed,
                  dps->is_old_style_param_decl, dps->is_top_level_declaration,
                  /*marked_as_gnu_extension=*/FALSE, dps->param_id_list,
                  &linkage_spec_range);
      scope_stack_top().decl_parse_state = saved_dps;
      /* pop_name_linkage will already have been called in declaration
         (before advancing past the end of the declaration, because there
         is a dependency in precompiled header processing on the state
         maintained in the scope stack entry). */
    }  /* if */
  }  /* if */

  db_exit();
}  /* linkage_specification */


static a_boolean is_invalid_catch_type(a_type_ptr         type,
                                       a_source_position  *pos)
/*
Check whether the given type is a valid catch type: incomplete types, pointers
and references to incomplete types and abstract class types are not valid.
Issue diagnostics as appropriate at the given position.  Return TRUE if the
given type should not be used for error recovery purposes.
*/
{
  a_boolean result = FALSE;

  if (vla_enabled && is_variably_modified_type(type)) {
    pos_error(ec_vla_not_allowed, pos);
    result = TRUE;
  } else if (is_incomplete_type(type)) {
    pos_error(incomplete_type_err_code(type), pos);
    result = TRUE;
  } else if (is_rvalue_reference_type(type) &&
             !is_template_param_type(type_pointed_to(type))) {
    /* An rvalue reference to a template-parameter-like type could end up
       being an lvalue reference due to the "reference collapsing rules".  We
       therefore treat such cases like lvalue references. */
    pos_error(ec_rvalue_reference_catch_type, pos);
    result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && 
             (is_managed_class_type(type) || 
              (is_tracking_reference_type(type) && 
               is_managed_class_type(type_pointed_to(type))))) {
    /* In C++/CLI, managed types can only be thrown and caught by handle. */
    pos_error(ec_managed_object_not_caught_by_handle, pos);
    result = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (is_any_ptr_or_ref_type(type)) {
    type = type_pointed_to(type);
    /* Force instantiation of template class. */
    complete_type_is_needed(type);
    if (is_incomplete_type(type) && !is_void_type(type)) {
      pos_diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                     ec_ptr_or_ref_to_incomplete_type, pos);
      /* We might have only issued a warning.  Hence proceed with normal
         processing (i.e., result remains FALSE).  The code generators can
         handle it. */
    }  /* if */
  } else if (is_abstract_class_type(type)) {
    abstract_class_diagnostic(es_error, ec_abstract_class_catch_type, type,
                              pos);
    result = TRUE;
  }  /* if */
  return result;
}  /* is_invalid_catch_type */


void handler_declaration(a_statement_ptr     try_block_stmt,
                         a_source_position*  catch_pos,
			 a_boolean	     is_function_try_block)
/*
Process a handler declaration:

  "catch" "(" exception-declaration ")" compound-statement

try_block_stmt is a pointer to the try-block statement to which the catch
clause is to be attached.  catch_pos is the source position of "catch".
is_function_try_block is TRUE if this is a function try block, FALSE for
a normal try.
*/
{
  a_handler_ptr                handler, prev_handler;
  a_type_ptr                   type_ptr = NULL;
  a_symbol_locator             locator;
  a_source_position            decl_pos;
  a_routine_ptr                cctor, dtor;
  a_param_type_ptr             ptp;
  a_dynamic_init_ptr           dip;

  db_enter(3, "handler_declaration");
  /* Push the scope for the handler before processing the exception
     declaration to assure that the scope of the handler's parameter is the
     same as that of the handler's compound statement block. */
  (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL);
  scope_stack[depth_scope_stack].is_catch_in_function_try =
                                                         is_function_try_block;
  /* Allocate the handler. */
  handler = alloc_handler();
  /* Set the assoc_handler field of the IL scope entry. */
  set_block_scope_handler(handler);
  set_stmt_source_position(handler->catch_position, *catch_pos);
  if (required_token(tok_lparen, ec_exp_lparen)) {
    a_decl_parse_state  state;
    init_decl_parse_state(&state);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (struct_stmt_stack != NULL) {
      struct_stmt_stack_top().in_handler_parameter_declaration = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    decl_pos = pos_curr_token;
    state.prefix_attributes = scan_attributes(al_prefix);
    if (curr_token == tok_ellipsis) {
      /* NULL parameter. */
      disallow_attributes(&state.prefix_attributes);
      (void)get_token();
    } else {
      if (curr_token != tok_identifier &&
          !is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
        add_stop_token(tok_rparen);
        syntax_error(ec_missing_exception_declaration);
        type_ptr = error_type();
        set_to_error_locator(locator);
        remove_stop_token(tok_rparen);
      } else {
        a_decl_pos_block  decl_pos_block;
        state.auto_type_allowed = FALSE;
        state.trailing_return_type_allowed = trailing_return_types_enabled;
        clear_decl_pos_block(&decl_pos_block);
        decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_EMPTY_DECL_SPECIFIERS_ALLOWED),
                        &state, &decl_pos_block);
        if (state.dso_flags & DSO_DEFINES_SOMETHING) {
          /* Definition of a class, struct, union, or enum type is not
             allowed. */
          pos_error(ec_type_definition_not_allowed, &decl_pos);
        } else if (state.dso_flags & DSO_NO_DECL_SPECIFIERS) {
          /* Missing type specifier. */
          pos_error(ec_missing_exception_declaration, &decl_pos);
          invalidate_type(&state);
        } else if (!(state.dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
          /* Implicit int. */
          report_implicit_int(&pos_curr_token, state.specifiers_type);
        }  /* if */
        if (is_abstract_or_real_declarator_start()) {
          a_decl_flag_set  di_flags = (DI_REAL_DECLARATOR_ALLOWED |
                                       DI_ABSTRACT_DECLARATOR_ALLOWED);
          if (vla_enabled) {
            di_flags |= DI_VLA_ALLOWED;
          }  /* if */
          declarator(di_flags, &state, /*member_parent_type=*/(a_type_ptr)NULL,
                     &locator, (a_func_info_block_ptr)NULL, &decl_pos_block);
        }  /* if */
        check_use_of_auto_type(&state);
        check_pending_qualifiers_used(&state);
        if (!exceptions_enabled) {
          /* Don't bother with the semantic checks on the handler type.  Set
             state.type to error type to avoid inappropriate errors
             downstream. */
          invalidate_type(&state);
        } else if (!is_error_type(state.type)) {
          /* Force instantiation of template class. */
          complete_type_is_needed(state.type);
          /* Adjust the type if necessary (for example, "array of x" becomes
             "pointer to x"). */
          adjust_parameter_type(&state.type);
          if (is_invalid_catch_type(state.type, &decl_pos)) {
            /* An appropriate error message will have been issued by
               invalid_catch_type. */
            invalidate_type(&state);
          } else {
            /* Mark the type as having been used in an exception.  (Also,
               if it "contains" any classes, they are marked as requiring
               external linkage.) */
            set_used_in_exception_or_rtti_flag(state.type);
            if (is_or_contains_local_type(state.type)) {
              /* Exception types, if they have linkage at all, must have
                 external linkage; however, it is possible to write a useful
                 program in which a local type is thrown and caught -- e.g.,
                   void f() {
                     class A { ... };
                     try { ... throw A ... }
                     catch (A) { ... }
                   }
                 We still issue a diagnostic, since there are other cases
                 (not necessarily detectable by the compiler) in which local
                 types would be problematic. */
              pos_remark(ec_local_type_used_in_exception, &decl_pos);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Create a variable for the handler parameter, even if there's no
           explicit name. */
        handler->parameter = make_handler_parameter(state.type);
        /* Update the symbol, if there is one. */
        if (state.do_flags & DO_REAL_DECLARATOR_SCANNED) {
          state.sym = enter_symbol((a_symbol_kind)sk_variable, &locator,
                                   decl_scope_level,
                                   /*suppress_redecl_error=*/FALSE);
          state.sym->variant.variable.ptr = handler->parameter;
          set_source_corresp(&(handler->parameter->source_corresp), state.sym);
          record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION,
                                    state.sym, &state.sym->decl_position,
                                    state.source_sequence_entry);
#if GENERATE_SOURCE_SEQUENCE_LISTS
          state.sym->variant.variable.ptr->declared_type = state.declared_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          mark_variable_value_set(state.sym);
        }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        /* Record additional source-range information in the variable entry
           for the handler parameter. */
        if (state.sym != NULL) {
          update_decl_pos_info(&handler->parameter->source_corresp,
                               &decl_pos_block);
        } else {
          /* Since set_source_corresp is not called for unnamed entities,
             create the associated decl-pos supplement directly. */
          handler->parameter->source_corresp.decl_pos_info =
                          make_decl_pos_supplement(/*at_file_scope=*/FALSE,
                                                   &decl_pos_block);
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Set the is_local_to_function flag after returning from
           set_source_corresp. */
        handler->parameter->source_corresp.is_local_to_function = TRUE;
        attach_decl_attributes_to_entity(&state, iek_variable,
                                         (char*)handler->parameter,
                                         /*primary_decl=*/TRUE);
        /* A handler parameter is initialized by the run-time when the
           handler is invoked.  Create the dynamic init entry to represent
           the initialization. */
        if (is_class_struct_union_type(state.type)) {
          /* Classes may require the use of a copy constructor. */
          a_boolean          bitwise_copy;
          a_source_position  pos;
          a_boolean          allow_suppressed_ctor =
                                  (microsoft_mode && microsoft_version < 1310);
          if (state.sym != NULL) {
            pos = state.sym->decl_position;
          } else {
            pos = state.specifiers_pos;
          }  /* if */
          /* Both the copy constructor and destructor must be accessible in
             the context of the handler.  (However, the Microsoft compiler
             doesn't enforce accessibility of copy constructors; see
             reference_to_implicitly_invoked_function.) */
          cctor = select_copy_constructor(state.type,
                                          (a_type_qualifier_set)TQ_NONE,
                                          /*source_is_rvalue=*/FALSE,
                                          &pos, state.type, &bitwise_copy,
                                          allow_suppressed_ctor);
          /* Only an implicit or defaulted copy constructor can correspond
             to a bitwise copy.  However, cctor can also be NULL if the
             copy constructor was ambiguous.  For Microsoft versions earlier
             than 7.1, it can also be NULL (without a diagnostic) if the
             declaration of the copy constructor was suppressed because of
             an inability to generate its definition. */
          check_assertion(cctor != NULL || bitwise_copy ||
                          total_errors != 0 ||
                          (allow_suppressed_ctor &&
                           skip_typerefs(state.type)
                                        ->variant.class_struct_union
                                                 .copy_ctor_decl_suppressed));
          dtor = select_destructor(state.type, state.type, &pos);
        } else {
          /* Non classes require only bitwise copying. */
          cctor = dtor = NULL;
        }  /* if */
        if (cctor != NULL) {
          /* A copy constructor was located.  Create a dynamic-init entry
             to point to it. */
          ptp = (skip_typerefs(cctor->type))->
                                   variant.routine.extra_info->param_type_list;

          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = cctor;
          /* We need to copy the default arg expressions of the second and
             subsequent parameters (if any) of the copy constructor.  The
             first param is ignored even if it is declared to have a default
             arg. */
          ptp = ptp->next;
          dip->variant.constructor.args =
           copy_default_arg_expr_list(cctor, ptp,
                                      /*inside_conditional_expression=*/FALSE,
                                      /*potentially_evaluated=*/TRUE,
                                      /*evaluated=*/TRUE);
          /* Only at runtime is the source known. */
          dip->variant.constructor.
                             is_copy_constructor_with_implied_source = TRUE;
        } else {
          /* A bitwise copy is all that is required. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_bitwise_copy);
        }  /* if */
        dip->variable = handler->parameter;
        dip->destructor = dtor;
        record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                           /*block_lifetime=*/TRUE);
        handler->dynamic_init = dip;
        type_ptr = state.type;
        check_use_of_auto_type(&state);
        run_end_of_parse_actions(&state, /*more_declarators=*/FALSE);
      }  /* if */
    }  /* if */
    prev_handler = try_block_stmt->variant.try_block->handlers;
    if (prev_handler == NULL) {
      /* This is the first handler declared for this try block. */
      try_block_stmt->variant.try_block->handlers = handler;
    } else {
      a_boolean  masked = FALSE;
      /* Make a pass over the previously declared handlers in this try block
         to do error checking and locate the end of the list, where the new
         handler will be added. */
      for (;;) {
        if (!exceptions_enabled) {
          /* Don't bother with semantic checks. */
        } else if (type_ptr != NULL && is_immediate_error_type(type_ptr)) {
          /* No need to check for masking in this case. */
        } else if (prev_handler->parameter == NULL) {
          /* Default handler has already been declared.  If it's the last on
             the list, issue an error; if not, an error will already have
             been issued, and further checking is suppressed. */
          if (prev_handler->next == NULL) {
            /* Anything following a default handler is masked by it, but we
               only issue an error on the first handler that follows. */
            pos_error(ec_masked_by_default_handler, &decl_pos);
          }  /* if */
        } else if (masked) {
          /* One "masking" warning has already been issued -- there's no
             point in putting out another. */
        } else if (handler->parameter == NULL) {
          /* Current handler is a default handler -- it can only be masked by
             another default handler. */
        } else if (prev_handler->parameter->type != NULL &&
                   is_immediate_error_type(prev_handler->parameter->type)) {
          /* No need to check for masking in this case. */
        } else if (type_masks_handler_param_type(prev_handler->parameter->type,
                                                 type_ptr)) {
          /* The type of prev_handler assures that handler will never be
             called, because it masks current handler's type.  See ARM 15.4. */
          pos_ty_warning(ec_masked_by_handler, &decl_pos,
                         prev_handler->parameter->type);
          masked = TRUE;
        }  /* if */
        if (prev_handler->next == NULL) {
          /* End of the list.  Append the new handler. */
          prev_handler->next = handler;
          break;
        } else {
          /* Keep looping. */
          prev_handler = prev_handler->next;
        }  /* if */
      }  /* for */
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (struct_stmt_stack != NULL) {
      struct_stmt_stack_top().in_handler_parameter_declaration = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Parse the body of the handler. */
  handler->statement = compound_statement(/*at_function_level=*/FALSE,
                                          /*explicit_return_type=*/FALSE,
                                          /*is_catch_clause=*/TRUE,
                                          /*is_statement_expr=*/FALSE);
  /* pop_scope is called from compound_statement processing. */
  db_exit();
}  /* handler_declaration */


#if !GENERATE_SOURCE_SEQUENCE_LISTS && !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* is_asm_statement is not referenced.*/
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS && !MICROSOFT_EXTENSIONS_ALLOWED */
an_asm_entry_ptr asm_declaration(a_boolean         asm_decl_allowed,
                                 a_boolean         is_asm_statement,
                                 an_attribute_ptr  *p_attributes)
/*
Scan an asm declaration, create an entry to represent it in the IL, and
return a pointer to the asm entry.  asm_decl_allowed is FALSE if an error
should be issued.  is_asm_statement is TRUE if this asm declaration appears
inside a function (and will therefore be associated an stmk_asm statement
entry), FALSE otherwise.  An asm declaration is specified as follows in the
ARM:

  asm ( string-literal ) ;

It can appear at file scope, function scope, and block scope.  In C mode,
where declarations and executable statements may not be mingled, an asm
"declaration" at function or block scope is always treated as executable.

In Microsoft mode support is provided for additional syntax:

  __asm { asm-instruction-list } ;opt
  __asm asm-instruction ;opt

where an asm-instruction-list is a semi-colon-delimited list of asm
instructions (unquoted).

Microsoft asm blocks are converted into a string when the __asm token
is encountered.  The string is pointed to by curr_token_asm_string and
is saved and restored as needed by the token caching mechanism.

In GNU C and C++ modes support is provided for additional syntax:

  asm volatile    goto    ( string-literal : operand-spec )
              opt     opt

This may appear only at function or block scope.  The operand-spec tells
the compiler how to map C/C++ variables into and out of the assembly
instruction's operands.

*p_attributes points to any prefix attributes (NULL if none).  Such attributes
are invalid: If *p_attributes is non-NULL issue an error and set *p_attributes
to NULL.
*/
{
  a_constant                asm_string;
  an_asm_entry_ptr          ap = NULL;
  a_source_position         asm_pos;
#if GNU_EXTENSIONS_ALLOWED
  a_boolean                 gnu_asm_form = FALSE;
  a_boolean                 is_volatile = FALSE;
  a_boolean                 has_volatile_keyword = FALSE;
  a_boolean                 is_asm_goto = FALSE;
  an_asm_operand_ptr        operands = NULL;
  a_named_register_list_ptr clobbers = NULL;
  a_label_list_ptr          labels = NULL;
  int                       number_of_constraints = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_boolean                 seen_tok_colon_colon = FALSE;
  a_boolean                 err = FALSE;

  db_enter(3, "asm_declaration");
  check_assertion(curr_token == tok_asm || curr_token == tok_microsoft_asm);
  if (!asm_decl_allowed) {
    /* An asm declaration is not allowed in the current scope. */
    error(ec_asm_decl_not_allowed);
    discard_curr_construct_pragmas();
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an asm
       declaration. */
    cannot_bind_to_curr_construct();
    /* Prefix attributes are not allowed on asm declarations. */
    disallow_attributes(p_attributes);
  }  /* if */
  copy_source_position(pos_curr_token, asm_pos);
  if (curr_token == tok_microsoft_asm) {
    clear_constant(&asm_string, (a_constant_repr_kind)ck_string);
    /* The asm string is already allocated in IL memory. */
    asm_string.variant.string.value = curr_token_asm_string;
    asm_string.variant.string.length =
                            (a_targ_size_t)(strlen(curr_token_asm_string)) + 1;
    asm_string.type = string_type(asm_string.variant.string.length);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Bypass the Microsoft asm token. */
    (void)get_token();
  } else {
    /* Skip past the "asm". */
    (void)get_token();
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_mode && is_type_qualifier()) {
      /* Scan a volatile and/or const qualifier.  The const qualifier is
         ignored with a warning.  Type qualifiers other than volatile and
         const elicit an error. */
      a_source_position     cv_pos;
      a_decl_pos_block      ext_cv_pos;
      a_type_qualifier_set  qualifiers;

      cv_pos = pos_curr_token;
      qualifiers = collect_type_qualifiers(&ext_cv_pos,
                                           (a_upc_block_size *)NULL);
      if (qualifiers & ~(TQ_CONST | TQ_VOLATILE)) {
        /* Other qualifiers (e.g., "restrict") should be rejected. */
        pos_error(ec_invalid_asm_qualifiers, &cv_pos);
      } else if (qualifiers & TQ_CONST) {
        pos_warning(ec_const_ignored, &cv_pos);
      }  /* if */
      if (qualifiers & TQ_VOLATILE) {
        report_gnu_extension_if_needed(&cv_pos,
                                       ec_volatile_asm_is_gnu_extension);
        is_volatile = TRUE;
        has_volatile_keyword = TRUE;
      }  /* if */
    }  /* if */
    if (gnu_mode && gnu_version >= 40500 && curr_token == tok_goto) {
      is_asm_goto = TRUE;
      /* Bypass the goto. */
      (void)get_token();
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Check for and skip the opening parenthesis. */
    (void)required_token(tok_lparen, ec_exp_lparen);
    add_stop_token(tok_rparen);
    /* Scan the enclosed string. */
    if (curr_token != tok_string_literal) {
      syntax_error(ec_exp_asm_string);
      err = TRUE;
    } else if (gnu_mode &&
               !is_normal_character_kind(const_for_curr_token.character_kind)){
      /* GNU only allows narrow string literals. */
      syntax_error(ec_wide_string_invalid_in_asm);
      err = TRUE;
    } else {
      copy_constant(&const_for_curr_token, &asm_string);
      (void)get_token_with_colon_separation(&seen_tok_colon_colon);
    }  /* if */
    if (err) {
      set_error_constant(&asm_string);
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    /* Check for operands spec. */
    if (gnu_mode && is_asm_statement) {
      a_boolean  outputs;
      if (curr_token == tok_colon) {
        gnu_asm_form = TRUE;
        /* Process the input and output operand lists (each preceded by
           a colon). */
        operands = asm_operands_spec(&seen_tok_colon_colon,
                                     &number_of_constraints);
        if (number_of_constraints == -1) number_of_constraints = 0;
        /* Process the clobbers spec (which follows a colon). */
        clobbers = asm_clobbers_spec(&seen_tok_colon_colon);
        if (is_asm_goto) {
          /* The GNU documentation specifies that there should be no
             operands in an "asm goto", but also that this restriction may
             be lifted in the future, so no check is made here. */
          labels = asm_labels_spec(&seen_tok_colon_colon);
        } else {
          if (curr_token != tok_rparen) {
            syntax_error(ec_exp_rparen);
          }  /* if */
        }  /* if */
      }  /* if */
      /* An asm() with no outputs is automatically volatile. */
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
      outputs = operands != NULL && operands->is_output_operand;
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
      outputs = operands != NULL &&
                (operands->modifiers & (an_asm_operand_modifier)aom_output);
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
      if (!outputs) {
        is_volatile = TRUE;
      }  /* if */
    } else {
      /* This is not a situation where operand specs are accepted (e.g., not
         GNU C mode).  Mark the entry as "volatile" to indicate the fact that
         we do not know the effect on operands. */
      is_volatile = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Check for and skip the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for and skip the semicolon. */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
  /* Update the IL. */
  if (asm_decl_allowed) {
    /* Allocate and set the asm-entry. */
    ap = alloc_asm_entry();
    ap->asm_string = alloc_unshared_constant(&asm_string);
    copy_source_position(asm_pos, ap->source_corresp.decl_position);
#if GNU_EXTENSIONS_ALLOWED
    ap->gnu_asm_form = gnu_asm_form;
    ap->is_volatile = is_volatile;
    ap->has_volatile_keyword = has_volatile_keyword;
    ap->is_asm_goto = is_asm_goto;
    ap->operands = operands;
    ap->clobbers = clobbers;
    ap->labels = labels;
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
    ap->number_of_constraints =
                           (a_targ_size_t)number_of_constraints; /*lint !e571*/
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
    if (gnu_asm_form) {
      validate_operands_and_clobbers(ap);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (!is_asm_statement) {
      /* Add the asm entry to the list for the current scope.  This is only
         done for asm declarations that do not appear in an executable context
         and so have no statement entry to point at them. */
      add_to_asm_entries_list(ap);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* There's no name or symbol for the asm declaration, so call
         update_source_sequence_list directly. */
      add_to_source_sequence_list((char *)ap, (an_il_entry_kind)iek_asm_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  }  /* if */

  db_exit();
  return ap;
}  /* asm_declaration */


static a_variable_ptr condition_or_for_each_declaration(
                                            a_statement_ptr for_each_statement)
/*
Scan a condition declaration (when for_each_statement is NULL) or an iterator
declaration in a "for each" statement (when for_each_statement is non-NULL).
"for each" statements only occur in Microsoft mode (and some types are
further restricted to C++/CLI mode).  Syntax for a condition declaration:

  type-specifier-seq declarator = assignment-expression

Syntax for an iterator declaration in a "for each" statement:

  type-specifier-seq declarator in assignment-expression

(But the "in assignment-expression" part is not scanned here.)

Return a pointer to the variable that is declared.
*/
{
  a_decl_flag_set              dsi_flags;
  a_symbol_ptr                 sym;
  a_variable_ptr               vp;
  a_symbol_locator             locator;
  a_boolean                    incomplete_type_error_reported = FALSE;
  a_boolean                    missing_declarator = FALSE;
  a_symbol_reference_kind      srk_flags;
  a_decl_pos_block             decl_pos_block;
  a_decl_parse_state           state;

  db_enter(3, "condition_or_for_each_declaration");
  /* Scan the declaration specifiers.  "typedef" is not allowed and may
     not introduce a new class or enumeration. */
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
              DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
              DSI_IS_CONDITION_DECL;
  if (std_attributes_enabled) dsi_flags |= DSI_STD_ATTRIBUTES_ALLOWED;
  if (gnu_attributes_enabled) dsi_flags |= DSI_GNU_ATTRIBUTES_ALLOWED;
  init_decl_parse_state(&state);
  state.prefix_attributes = scan_attributes(al_prefix);
  state.is_definition = TRUE;
  state.auto_type_allowed = auto_type_specifier_enabled;
  clear_decl_pos_block(&decl_pos_block);
  decl_specifiers(dsi_flags, &state, &decl_pos_block);
  if (state.dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &state.start_pos);
  } else if (!(state.dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Implicit int. */
    report_implicit_int(&pos_curr_token, state.specifiers_type);
  }  /* if */
  if (state.storage_class == (a_storage_class)sc_unspecified) {
    state.storage_class = (a_storage_class)sc_auto;
  }  /* if */
  if (is_declarator_start()) {
    /* Scan the declarator, which is not allowed to specify a function or an
       array (although Microsoft mode does allow arrays). */
    declarator(DI_REAL_DECLARATOR_ALLOWED, &state,
               /*member_parent_type=*/(a_type_ptr)NULL, &locator,
               (a_func_info_block_ptr)NULL, &decl_pos_block);
  } else {
    /* No declarator.  Issue a single diagnostic on this malformed
       condition declaration. */
    missing_declarator = TRUE;
    set_to_error_locator(locator);
    error_position = pos_curr_token;
  }  /* if */
  check_pending_qualifiers_used(&state);
  complete_type_is_needed(state.type);
  if (is_function_type(state.type)) {
    /* Function type is disallowed. */
    pos_error(ec_function_type_not_allowed, &state.start_pos);
    invalidate_type(&state);
  } else if (is_array_type(state.type)) {
    /* Array type is disallowed, except in Microsoft mode. */
    if (microsoft_mode) {
      pos_warning(ec_array_condition_always_true, &state.start_pos);
    } else {
      pos_error(ec_array_type_not_allowed, &state.start_pos);
      invalidate_type(&state);
    }  /* if */
  }  /* if */
  if ((state.dso_flags & DSO_CONSTEXPR) != 0 &&
      !is_const_qualified_type(state.type)) {
    /* constexpr variables are implicitly const. */
    state.type = make_qualified_type(state.type,
                                     (a_type_qualifier_set)TQ_CONST);
  }  /* if */
  /* Enter the symbol in the current scope, which should be an sck_condition
     scope. */
  sym = enter_symbol((a_symbol_kind)sk_variable, &locator, decl_scope_level,
                     /*suppress_redecl_error=*/FALSE);
  state.sym = sym;
  state.is_definition = TRUE;
  /* Allocate the variable and bind the symbol to it. */
  vp = make_variable(state.type, state.storage_class, decl_scope_level);
  sym->variant.variable.ptr = vp;
  set_source_corresp(&vp->source_corresp, sym);
  if (state.decltype_auto_specifier_seen) {
    vp->declared_with_decltype_auto = TRUE;
  } else if (state.auto_type_specifier_seen) {
    vp->declared_with_auto_type_specifier = TRUE;
  }  /* if */
  if (state.dso_flags & DSO_CONSTEXPR) {
    vp->is_constexpr = TRUE;
  }  /* if */
  attach_decl_attributes(&state, /*primary_decl=*/TRUE);
  /* Copy the decl-modifiers into the variable entry. */
  update_variable_decl_modifiers(&state);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sym->variant.variable.ptr->declared_type = state.type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  srk_flags = SRK_DECLARATION | SRK_DEFINITION;
  if (!missing_declarator &&
      (curr_token == tok_assign ||
       (list_init_enabled && curr_token == tok_lbrace))) {
    srk_flags |= SRK_INITIALIZATION;
  }  /* if */
  record_symbol_declaration(srk_flags, sym, &sym->decl_position,
                            state.source_sequence_entry);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&sym->variant.variable.ptr->source_corresp,
                       &decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (missing_declarator) {
    /* Now issue the error for the missing declarator. */
    if (for_each_statement != NULL) {
      /* Add tok_identifier so we will stop on "in" if it appears. */
      add_stop_token(tok_identifier);
      syntax_error(ec_exp_id_in_for_each_decl);
      remove_stop_token(tok_identifier);
    } else {
      syntax_error(ec_exp_declarator_in_condition_decl);
    }  /* if */
  } else {
    decl_pos_block.var_init_range.start = pos_curr_token;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (for_each_statement != NULL) {
      /* C++/CLI "for each" statement.  The "in" and collection expression
         are not scanned here. */
      a_for_each_loop_ptr felp = 
                          for_each_statement->variant.for_each_loop.extra_info;
      vp->is_enhanced_for_iterator = TRUE;
      felp->uses_prev_decl_iterator = FALSE;
      felp->iterator.variable = vp;
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    {
      /* The C++03 syntax for condition explicitly requires the "= expr"
         syntax for initialization (that is, parenthesized initializers are
         disallowed, as is implicit initialization of objects with default
         constructors).  C++11 adds list-initialization syntax as a valid
         option. */
      if (curr_token == tok_assign) {
        /* Advance past the "=". */
        (void)get_token();
      } else if (!C_mode() && curr_token == tok_lbrace) {
        /* This looks like a C++11-style direct list initializer. */
        state.has_direct_initializer = TRUE;
      } else {
        (void)required_token(tok_assign, ec_exp_assign);
      }  /* if */
      initializer(&state, &locator.source_position, idl_none,
                  /*parenthesized_initializer=*/FALSE,
                  &incomplete_type_error_reported, &decl_pos_block);
    }  /* if */
    /* Reset the error position to the source position of the declarator. */
    error_position = locator.source_position;
  }  /* if */
  if (is_incomplete_type(vp->type)) {
    /* Incomplete type is not allowed.  (This test is delayed until after
       having seen the initializer so we can handle the Microsoft extension
       that permits "if (char s[] = "x") ...".) */
    if (!incomplete_type_error_reported) {
      pos_error(incomplete_type_err_code(vp->type), &state.start_pos);
    }  /* if */
    vp->type = error_type();
  }  /* if */
  /* Both in the error and normal case consider the variable set.  Don't
     do this earlier so we can catch "if (int x = x);". */
  mark_variable_value_set(sym);
  run_end_of_parse_actions(&state, /*more_declarators=*/FALSE);
  db_exit();
  /* Return a pointer to the variable. */
  return vp;
}  /* condition_or_for_each_declaration */


a_variable_ptr condition_declaration(void)
/*
Scan a condition declaration.  Syntax:

  type-specifier-seq declarator = assignment-expression

Return a pointer to the variable that is declared.
*/
{
  a_variable_ptr vp;

  db_enter(3, "condition_declaration");
  vp = condition_or_for_each_declaration((a_statement_ptr)NULL);
  db_exit();
  return vp;
}  /* condition_declaration */

#if MICROSOFT_EXTENSIONS_ALLOWED

void for_each_iterator_declaration(a_statement_ptr sp)
/*
Scan a "for each" iteration variable declaration.  "for each" statements are
allowed only in Microsoft mode and some types of "for each" are allowed only
in C++/CLI mode.  Syntax:

  for each (type-specifier-seq declarator in assignment-expression)
            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Only the part marked is scanned in this routine.
*/
{
  db_enter(3, "for_each_iterator_declaration");
  (void)condition_or_for_each_declaration(sp);
  db_exit();
}  /* for_each_iterator_declaration */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static void make_static_assert_string_for_output(void)
/*
Create a character string from the string constant entry associated with the
current token.  The string constant entry may represent a wide-character
literal, but the output will be restricted to the basic source character
set -- other characters are replaced by a '?'.  The generated string (which
is meant to be used in a diagnostic) is stored in temp_text_buffer.
*/
{
  a_constant_ptr  con = &const_for_curr_token;
  unsigned int    char_size;
  a_const_char    *ptr;
  a_targ_size_t    con_byte_len, msg_len, k;

  check_assertion(con->kind == (a_constant_repr_kind)ck_string);
  char_size = (unsigned int)character_size[con->character_kind];
  /* Allocate the number of characters needed (plus one in case the constant
     doesn't include a trailing NULL). */
  con_byte_len = con->variant.string.length;
  msg_len = con_byte_len/char_size;
  ensure_temp_text_buffer_space(msg_len+1);
  /* Extract the characters from the constant. */
  ptr = con->variant.string.value;
  for (k = 0; k < msg_len; ++k, ptr += char_size) {
    unsigned long  char_val = extract_character_from_string(ptr, char_size);
    if (char_val == 0) {
      break;
    } else if (char_val > CHAR_MAX ||
               is_nonstandard_character((char)char_val)) {
      temp_text_buffer[k] = '?';
    } else {
      temp_text_buffer[k] = (char)char_val;
    }  /* if */
  }  /* for */
  /* Append a null character. */
  temp_text_buffer[k] = '\0';
}  /* make_static_assert_string_for_output */


void static_assert_declaration(a_boolean  leave_semicolon)
/*
Parse a construct of the form
	static_assert ( <constant-expression> , <string-literal> ) ;
Issue an error incorporating the string literal if the constant-expression
is "false".  If leave_semicolon is TRUE, do not consume the final token.
*/
{
  a_constant         assert_con;
  a_source_position  pos;

  cannot_bind_to_curr_construct();
  /* Record the construct's position and verify the introductory tokens. */
  pos = pos_curr_token;
  check_assertion(curr_token == tok_static_assert);
  (void)get_token();
  add_stop_token(tok_semicolon);
  add_stop_token(tok_rparen);
  add_stop_token(tok_comma);
  (void)required_token(tok_lparen, ec_exp_lparen);
  /* Scan the first argument, which must be a constant expression convertible
     to bool. */
  scan_bool_constant_expression(&assert_con);
  /* Scan the second argument, which must be a string literal. */
  remove_stop_token(tok_comma);
  (void)required_token(tok_comma, ec_exp_comma);
  if (curr_token != tok_string_literal) {
    syntax_error(ec_exp_string_literal);
  } else {
    /* We've seen enough of the construct to evaluate it (if it is
       nondependent), and (in some configurations) record it. */
    /* In Microsoft mode, we do not check the assertion in "nonreal
       instantiations". */
    if (is_error_constant(&assert_con) ||
        is_error_constant(&const_for_curr_token)) {
      /* An error should already have been issued. */
      expect_error();
    } else if (assert_con.kind != (a_constant_repr_kind)ck_template_param &&
               is_false_constant(&assert_con) &&
               !(microsoft_mode &&
                 scope_stack_top().in_nonreal_instantiation)) {
      /* The assertion failed: Issue an error. */
      make_static_assert_string_for_output();
      pos_st_error(ec_static_assert, &pos, temp_text_buffer);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    } else {
      /* Record the assertion in the IL. */
      a_static_assertion_ptr  entry = alloc_static_assertion();
      entry->condition = alloc_shareable_constant(&assert_con);
      entry->string_literal = alloc_shareable_constant(&const_for_curr_token);
      entry->position = pos;
      add_to_source_sequence_list((char*)entry,
                                  (an_il_entry_kind)iek_static_assertion);  
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    (void)get_token();
  }  /* if */
  /* Verify the closing tokens. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  if (!leave_semicolon) {
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
  remove_stop_token(tok_semicolon);
}  /* static_assert_declaration */


void add_to_inline_namespace_list(a_scope_stack_entry_ptr	ssep,
				  a_using_decl_ptr		udp)
/*
Add the using-directive specified by udp to the inline namespace list of the
namespace scope specified by ssep.
*/
{
  a_namespace_list_entry_ptr         nlep = alloc_namespace_list_entry();
  a_scope_pointers_block_ptr         spbp;

  check_assertion(is_file_or_namespace_scope(ssep));
  spbp = get_pointers_block_for_scope(ssep->il_scope);
  nlep->ptr = (a_namespace_ptr)udp->entity.ptr;
  nlep->next = spbp->inline_namespaces;
  spbp->inline_namespaces = nlep;
}  /* add_to_inline_namespace_list */


#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* <-- attributes is not used in this case. */
#endif /* !GNU_EXTENSIONS_ALLOWED */
void make_using_directive(a_namespace_ptr    nsp,
			  a_scope_depth	     depth,
                          a_source_position  *pos,
			  a_boolean	     compiler_generated,
			  a_boolean	     inline_namespace,
			  an_attribute_ptr   attributes)
/*
Create a using-decl entry for a using-directive that specifies the indicated
namespace, add it to the list of using-decl entries for the scope specified
by depth, and "activate" it to assure that inactive-list symbols belonging
to the namespace will be found during name lookup.

compiler_generated is TRUE for implicit using-directives created for
unnamed namespaces, and for certain using-directives created to emulate
a Microsoft bug.  inline_namespace is TRUE if this using-directive is
being created to indicate that the specified namespace is an inline namespace
of the current namespace.  attributes is a list of attributes specified on
this using-directive.
*/
{
  a_using_decl_ptr		udp;
  a_scope_stack_entry_ptr	ssep;

  /* Create the using-directive entry. */
  udp = alloc_using_decl();
  udp->position = *pos;
  udp->entity.kind = (a_byte_il_entry_kind)iek_namespace;
  udp->entity.ptr = (char *)nsp;
  udp->is_using_directive = TRUE;
  udp->compiler_generated = compiler_generated;
  udp->inline_namespace = inline_namespace;
  ssep = &scope_stack[depth_scope_stack];
  if (inline_namespace) {
    /* For inline namespaces, add the using-directive to the inline namespace
       list of the enclosing namespace. */
    add_to_inline_namespace_list(ssep, udp);
  }  /* if */
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension ||
      ssep->kind == (a_scope_kind)sck_file) {
    udp->decl_sequence_number = ++decl_seq_counter;
  } else {
    /* For local scope using-directives, assign an effective declaration
       sequence number that makes the using-directive always visible. */
    udp->decl_sequence_number = FIRST_DECL_SEQUENCE_NUMBER;
  }  /* if */
  attach_attributes(attributes, (char*)udp, iek_using_decl);
  add_to_using_decls_list(udp, depth);
  /* Activate it. */
  add_active_using_directive(udp, depth);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!compiler_generated) {
    /* Not a compiler-generated using directive for an unnamed namespace. */
    add_to_source_sequence_list((char *)udp, (an_il_entry_kind)iek_using_decl);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* make_using_directive */


static void add_implicit_using_directive(
				a_namespace_ptr		nsp,
				a_boolean		inline_namespace,
				a_boolean		namespace_pushed)
/*
Add an implicit using-directive for an unnamed or inline namespace.  nsp
is the namespace to be made visible by the using-directive.  inline_namespace
is TRUE for an inline namespace, FALSE for an unnamed namespace.
namespace_pushed is TRUE if the namespace scope for nsp has already been
pushed.
*/
{
  /* The model for the initial definition of an unnamed namespace
       namespace { ... }
     is this:
       namespace UNIQUE { }
       using namespace UNIQUE;
       namespace UNIQUE { ... }
     This enables this sort of code to work:
       namespace {
         int i;
         int j = ::i;       // lookup rules find UNIQUE::i
       }
     The model is implemented by immediately popping the
     original definition of the unnamed namespace, inserting the
     implicit using directive, and then reopening the namespace as
     as an extension.  A similar process is used for inline
     namespaces -- an inline namespace using-directive is created in
     the namespace containing the inline namespace. */
  if (namespace_pushed) pop_scope();
  /* Do an implicit "using" directive of the unnamed namespace. */
  make_using_directive(nsp, depth_scope_stack, &pos_curr_token,
                       /*compiler_generated=*/TRUE, inline_namespace,
                       (an_attribute_ptr)NULL);
  if (namespace_pushed) {
    (void)push_namespace_scope((a_scope_kind)sck_namespace_extension, nsp);
    scope_stack_top().explicitly_declared_namespace_extension = TRUE;
  }  /* if */
}  /* add_implicit_using_directive */


static void namespace_declaration(a_token_kind  *final_token)
/*
Scan a namespace declaration, which may be an original namespace definition,
an extension namespace definition, an unnamed namespace definition, or a
namespace alias definition.  The syntax is:

  original-namespace-definition:
    inline opt namespace identifier { namespace-body }

  extension-namespace-definition:
    inline opt namespace original-namespace-name { namespace-body }

  unnamed-namespace:
    inline opt namespace { namespace-body }

  namespace-alias-definition:
    namespace identifier = qualified-namespace-specifier;

*final_token is set to tok_semicolon if this is a namespace alias definition
and to tok_brace otherwise; the final token is swallowed by the caller.
*/
{
  a_source_position           namespace_pos;
  a_source_position           start_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position           identifier_end_pos, def_start_pos;
  a_decl_position_supplement_ptr
                              decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_namespace_ptr             nsp = NULL;
  a_symbol_ptr                ns_sym = NULL, sym;
  a_symbol_locator            locator;
  a_boolean                   is_unnamed_namespace = TRUE;
  a_boolean                   is_namespace_alias = FALSE;
  a_scope_pointers_block_ptr  pointers_block;
  a_boolean                   err = FALSE;
  a_symbol_reference_kind     srk_flags = SRK_DECLARATION;
  a_boolean                   bad_scope_for_namespace_def = FALSE;
  a_source_sequence_entry_ptr namespace_ssep = NULL;
  a_boolean		      namespace_scope_pushed = FALSE;
  a_boolean		      initial_decl_of_namespace_std = FALSE;
  an_attribute_ptr            attributes = NULL;
  a_boolean	              is_inline = FALSE;

  db_enter(3, "namespace_declaration");
  /* Save the source position of the start of the declaration. */
  start_pos = pos_curr_token;
  if (curr_token == tok_inline) {
    /* This is an inline namespace declaration. */
    is_inline = TRUE;
    (void)get_token();
    check_assertion(curr_token == tok_namespace);
  }  /* if */
  /* Save the source position of the namespace keyword. */
  namespace_pos = pos_curr_token;
  /* A namespace declaration is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_namespaces_in_embedded_cplusplus);
  /* Bypass "namespace". */
  (void)get_token();
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
  if (!source_sequence_entries_disallowed &&
      (debug_level >= 4 || db_flag_is_set("dump_ss_full"))) {
    fputs("namespace_declaration: adding empty ss entry\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  { /* Namespace entries are always allocated in file scope memory.  Any
       source sequence entry pointing to it must therefore also be allocated
       there. */
    a_memory_region_number  region_to_switch_back_to;
    switch_to_file_scope_region(&region_to_switch_back_to);
    namespace_ssep = add_empty_source_sequence_entry();
    switch_back_to_original_region(region_to_switch_back_to);
  }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (is_generalized_identifier_start(GID_NO_OPTIONS)) {
    /* Save the identifier's locator before bypassing it. */
    locator = locator_for_curr_id;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    identifier_end_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Issue an error if this is not a simple identifier name. */
    if (locator.is_qualified_name) {
      error(ec_qualified_name_not_allowed);
      set_to_error_locator(locator);
      err = TRUE;
    } else if (locator.is_operator_name || locator.is_conversion_name ||
               locator.is_udl_operator_name) {
      error(ec_operator_name_not_allowed);
      set_to_error_locator(locator);
      err = TRUE;
    }  /* if */
    is_unnamed_namespace = FALSE;
    (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  } else {
    identifier_end_pos = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (curr_token == tok_attribute && gnu_attributes_enabled &&
      (!gpp_mode || gnu_version >= 40200)) {
    attributes = scan_gnu_attribute_groups(al_namespace);
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* A namespace or namespace-extension definition. */
  } else if (curr_token == tok_assign && !is_unnamed_namespace) {
    /* This must be a namespace alias definition. */
    is_namespace_alias = TRUE;
    if (attributes != NULL) {
      pos_error(ec_attribute_not_allowed, &attributes->position);
      attributes = NULL;
    }  /* if */
    if (is_inline) {
      /* The inline specifier cannot be used on a namespace alias. */
      pos_error(ec_inline_on_alias, &start_pos);
    }  /* if */
  } else {
    /* A syntax error */
    add_stop_token(tok_semicolon);
    add_stop_token(tok_lbrace);
    /* A missing brace error will be issued below in most cases (and in
       the remaining cases another error will be issued).  If we haven't
       seen a name or attribute yet, we diagnose that at this point. */
    if (!is_unnamed_namespace || attributes != NULL) {
      expect_error();
    } else {
      (void)required_token(tok_identifier, ec_exp_identifier);
    }  /* if */
    remove_stop_token(tok_lbrace);
    remove_stop_token(tok_semicolon);
    set_to_error_locator(locator);
    /* Silently ignore any attributes. */
    attributes = NULL;
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  def_start_pos = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (depth_scope_stack != depth_innermost_namespace_scope) {
    /* The current scope is not the file scope or a namespace scope. */
    if (!is_namespace_alias) {
      /* This is a namespace definition of some sort, which can only occur
         in a file or namespace scope. */
      pos_error(ec_namespace_def_not_allowed, &namespace_pos);
      set_to_error_locator(locator);
      err = TRUE;
      bad_scope_for_namespace_def = TRUE;
    } else {
      /* This is a namespace alias definition, which can also occur in
         function and block scopes. */
      if (scope_stack[depth_scope_stack].kind != (a_scope_kind)sck_function &&
          scope_stack[depth_scope_stack].kind != (a_scope_kind)sck_block) {
        pos_error(ec_namespace_alias_def_not_allowed, &namespace_pos);
        set_to_error_locator(locator);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_unnamed_namespace) {
    /* No identifier -- this is an unnamed namespace definition. */
    if (!err) {
      /* See if this is its first definition in the current scope or if this
         is an extension. */
      pointers_block =
                  assoc_pointers_block_of(&scope_stack[depth_scope_stack]);
      ns_sym = pointers_block->unnamed_namespace_sym;
      if (ns_sym == NULL) {
        /* This is the first definition.  Create the symbol. */
        ns_sym = make_unnamed_namespace_symbol(&pos_curr_token);
        pointers_block->unnamed_namespace_sym = ns_sym;
      } else {
        /* A definition for the unnamed namespace has already appeared.  This
           definition will extend it, so reuse the symbol that was found. */
      }  /* if */
      make_locator_for_symbol(ns_sym, &locator);
    }  /* if */
  } else {
    /* A named namespace definition or a namespace alias.  Look up the
       identifier (which should be the current token) and see if it is
       already a namespace name in the current scope. */
    if (!err) {
      ns_sym = curr_scope_id_lookup(&locator, IDL_NO_OPTIONS);
      if (ns_sym != NULL) {
        /* A name was found in the current scope. */
        a_boolean  ns_sym_was_alias =
                       ns_sym->kind == (a_symbol_kind)sk_namespace &&
                       ns_sym->variant.namespace_info.ptr->is_namespace_alias; 

        if (microsoft_mode && !is_namespace_alias && ns_sym_was_alias) {
          /* In Microsoft mode, a namespace alias name can be used to define
             a namespace extension for the aliased namespace. */
          ns_sym = (a_symbol_ptr)
             skip_namespace_aliases(ns_sym->variant.namespace_info.ptr)->
                                                    source_corresp.assoc_info;
        } else if (gnu_namespace_and_class_in_same_scope &&
                   is_tag_symbol(ns_sym) && !locator.is_template_id) {
          /* Versions of g++ before 4.3 allow a namespace and class to be
             declared with the same name. */
          ns_sym = NULL;
        } else if (ns_sym->kind != (a_symbol_kind)sk_namespace ||
                   is_namespace_alias != ns_sym_was_alias) {
          /* The namespace name should not conflict with the declaration of
             another entity.  Furthermore, an alias should not be redeclared
             as a namespace name, nor should a plain namespace name be
             redeclared as an alias. */
          pos_st_error(ec_id_already_declared, &locator.source_position,
                       locator.symbol_header->identifier);
          ns_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* The closing right brace or semicolon will be swallowed by the caller. */
  *final_token = is_namespace_alias ? tok_semicolon : tok_rbrace;
  if (is_namespace_alias) {
    /* Bypass the "=". */
    (void)get_token();
    add_stop_token(tok_semicolon);
    if (!is_decl_qualified_name_start()) {
      /* A namespace alias definition requires a (possibly qualified)
         namespace or class name to the right of the "=". */
      discard_curr_construct_pragmas();
      syntax_error(ec_exp_identifier);
    } else {
      /* Look up the namespace specifier. */
      sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
                                                       ilm_namespace, &err);
      if (!err) {
        if (sym != NULL &&
            locator_for_curr_id.specific_symbol->kind ==
                                  (a_symbol_kind)sk_namespace_projection &&
            locator_for_curr_id.specific_symbol->ambiguous) {
          /* The name for which an alias is being declared is ambiguous. */
          sym_error(ec_ambiguous_name, locator_for_curr_id.specific_symbol);
        } else if (sym == NULL || sym->kind != (a_symbol_kind)sk_namespace) {
          /* Either nothing was found or what was found was not a namespace. */
          error(ec_missing_namespace_name);
        } else {
          if (ns_sym != NULL &&
              ns_sym->variant.namespace_info.ptr != NULL) {
            nsp = ns_sym->variant.namespace_info.ptr;
            if (skip_namespace_aliases(nsp) ==
                     skip_namespace_aliases(sym->variant.namespace_info.ptr)) {
              /* Redefining the alias to the same thing. */
              record_symbol_declaration(srk_flags, ns_sym,
                                        &locator.source_position,
                                        namespace_ssep);
            } else {
              pos_sy_error(ec_already_defined, &locator.source_position,
                           ns_sym);
            }  /* if */
          } else {
            if (ns_sym == NULL) {
              /* Create a namespace symbol to represent the alias.  Its
                 creation was delayed till all the error cases had been
                 dispensed with, to avoid creating a symbol with no namespace
                 to bind to. */
              ns_sym = enter_symbol((a_symbol_kind)sk_namespace, &locator,
                                    depth_scope_stack,
                                    /*suppress_redecl_error=*/TRUE);
            }  /* if */
            /* Now create a namespace entry.  It will point to the namespace
               entry that was just looked up. */
            nsp = alloc_namespace(/*is_alias=*/TRUE);
            nsp->variant.assoc_namespace = sym->variant.namespace_info.ptr;
            set_source_corresp(&nsp->source_corresp, ns_sym);
            set_namespace_membership(ns_sym, &nsp->source_corresp,
                                     (a_namespace_ptr)NULL);
            nsp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
            ns_sym->variant.namespace_info.ptr = nsp;
            add_to_namespaces_list(nsp);
            record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION,
                                      ns_sym, &locator.source_position,
                                      namespace_ssep);
          }  /* if */
          mark_referenced(sym, &pos_curr_token);
        }  /* if */
      }  /* if */
      if (!err && ns_sym != NULL) {
        /* Do processing required for any pragmas bound to the current
           declaration. */
        process_curr_construct_pragmas(ns_sym, (a_statement_ptr)NULL);
      } else {
        discard_curr_construct_pragmas();
      }  /* if */
      /* Bypass the identifier. */
      (void)get_token();
    }  /* if */
    remove_stop_token(tok_semicolon);
    if (required_token_no_advance(tok_semicolon, ec_exp_semicolon)) {
      /* Closing semicolon was found. */
      cannot_bind_to_curr_construct();
    } else {
      discard_curr_construct_pragmas();
    }  /* if */
  } else if (bad_scope_for_namespace_def) {
    /* Attempting to define a namespace within something other than the
       file scope or a namespace scope.  Ignore all the declarations between
       the braces. */
    discard_curr_construct_pragmas();
    if (curr_token == tok_lbrace) {
      /* Ignore the namespace definition. */
      flush_until_matching_token_full(/*limit_flush=*/FALSE);
      /* The closing right brace will be swallowed by the caller. */
      *final_token = tok_rbrace;
    }  /* if */
  } else {
    /* Namespace definition. */
    if (ns_sym == NULL) {
      if (locator.symbol_header == symbol_for_namespace_std->header &&
          depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
          !locator.is_error) {
        /* This is the initial explicit declaration of namespace "std".
           Reuse the predeclared symbol. */
        ns_sym = symbol_for_namespace_std;
        enter_symbol_for_namespace_std(&locator);
        initial_decl_of_namespace_std = TRUE;
        srk_flags |= SRK_DEFINITION;
#if IA64_ABI
      } else if (locator.symbol_header == symbol_for_namespace_abi->header &&
                 depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
                 !locator.is_error) {
        /* This is the initial explicit declaration of namespace "__cxxabiv1".
           Reuse the predeclared symbol. */
        ns_sym = symbol_for_namespace_abi;
        enter_symbol_for_namespace_abi(&locator);
        srk_flags |= SRK_DEFINITION;
#endif /* IA64_ABI */
      } else {
        /* Create a namespace symbol. */
        ns_sym = enter_symbol((a_symbol_kind)sk_namespace, &locator,
                              depth_scope_stack,
                              /*suppress_redecl_error=*/TRUE);
      }  /* if */
    }  /* if */
    if (ns_sym->variant.namespace_info.ptr == NULL) {
      /* Original definition -- allocate the namespace entry. */
      if (attributes != NULL) mark_primary_decl_attributes(attributes);
      nsp = alloc_namespace(/*is_alias=*/FALSE);
      set_source_corresp(&nsp->source_corresp, ns_sym);
      if (is_unnamed_namespace) nsp->source_corresp.name = NULL;
      set_namespace_membership(ns_sym, &nsp->source_corresp,
                               (a_namespace_ptr)NULL);
      nsp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
      nsp->is_inline = is_inline;
      ns_sym->variant.namespace_info.ptr = nsp;
      /* Set a flag indicating that this namespace is itself an unnamed
         namespace or is enclosed by an unnamed namespace. */
      if (is_unnamed_namespace ||
          (sym_is_namespace_member(ns_sym) &&
           symbol_supplement_for_namespace(sym_parent_namespace(ns_sym))
                                              ->within_unnamed_namespace)) {
        ns_sym->variant.namespace_info.extra_info->
                                           within_unnamed_namespace = TRUE;
      }  /* if */
      add_to_namespaces_list(nsp);
      /* Do processing required for any pragmas bound to the current
         declaration. */
      process_curr_construct_pragmas(ns_sym, (a_statement_ptr)NULL);
      if (!ignore_std_namespace || ns_sym != symbol_for_namespace_std) {
        /* Push a scope for the scanning the namespace body.  This is not done
           when using the g++ compatibility feature that makes "std" a
           synonym for the global namespace. */
        (void)push_namespace_scope((a_scope_kind)sck_namespace, nsp);
        nsp->variant.assoc_scope->variant.assoc_namespace = nsp;
        namespace_scope_pushed = TRUE;
      }  /* if */
      if (is_unnamed_namespace || is_inline) {
        /* Create the using-directive to make the unnamed or inline namespace
           visible. */
        add_implicit_using_directive(nsp, is_inline,
                                     /*namespace_pushed=*/TRUE);
      }  /* if */
      srk_flags |= SRK_DEFINITION;
    } else {
      /* An extension of the original definition of this namespace -- push
         a scope for scanning the namespace body. */
      /* Do processing required for any pragmas bound to the current
         declaration. */
      process_curr_construct_pragmas(ns_sym, (a_statement_ptr)NULL);
      nsp = ns_sym->variant.namespace_info.ptr;
      /* If any declaration of a namespace is inline, all declarations
         must be. */
      if (nsp->is_inline != is_inline) {
        an_error_code	error_code = is_inline ? ec_prev_ns_not_inline
                                               : ec_prev_ns_inline;
        pos_sy_diagnostic(strict_ansi_discretionary_severity, error_code,
                          &start_pos, ns_sym);
        if (gpp_mode && is_inline) {
          /* g++ allows a namespace extension to make a namespace inline. */
          nsp->is_inline = TRUE;
          /* Create the using-directive to make the unnamed or inline namespace
             visible. */
          add_implicit_using_directive(nsp, is_inline,
                                       /*namespace_pushed=*/FALSE);
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* User code is not permitted to extend the cli namespace. */
      if (cli_or_cx_enabled && !scanning_generated_code_from_metadata && 
          ns_sym == cli_symbol_from_kind(csk_cli_namespace)) {
        pos_error(cppcx_enabled ? ec_namespace_default_cannot_be_extended
                                : ec_namespace_cli_cannot_be_extended,
                  &locator.source_position);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (!ignore_std_namespace ||
          ns_sym != symbol_for_namespace_std) {
        /* Push a scope for the scanning the namespace body.  This is not done
           when using the g++ compatibility feature that makes "std" a
           synonym for the global namespace. */
        (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                   skip_namespace_aliases(nsp));
        scope_stack[depth_scope_stack].
                               explicitly_declared_namespace_extension = TRUE;
        scope_stack[depth_scope_stack].initial_decl_of_namespace_std =
                                                 initial_decl_of_namespace_std;
        namespace_scope_pushed = TRUE;
      }  /* if */
    }  /* if */
    record_symbol_declaration(srk_flags, ns_sym, &locator.source_position,
                              namespace_ssep);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (namespace_ssep != NULL &&
        ss_entry_kind(namespace_ssep) == iek_src_seq_secondary_decl) {
      ss_entry_ptr(namespace_ssep, a_src_seq_secondary_decl_ptr)->attributes =
                                           copy_of_attributes_list(attributes);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    attach_attributes(attributes, (char*)nsp, iek_namespace);
    if (!required_token(tok_lbrace, ec_exp_lbrace)) {
      discard_curr_construct_pragmas();
    } else {
      /* Scan the namespace body. */
      add_stop_token(tok_rbrace);
      while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
        declaration(/*function_definition_allowed=*/TRUE,
                    /*is_old_style_param_decl=*/FALSE,
                    /*is_top_level_declaration=*/FALSE,
                    /*marked_as_gnu_extension=*/FALSE,
                    (a_param_id_ptr)NULL, (a_source_range *)NULL);
      }  /* while */
      remove_stop_token(tok_rbrace);
      /* Process pragmas associated with the closing brace before the current
         scope is popped and before add_end_of_construct_source_sequence_entry
         is called. */
      process_curr_token_pragmas();
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry marking the end of the namespace
       definition. */
    add_end_of_construct_source_sequence_entry(
                                        (char *)nsp,
                                        (a_byte_il_entry_kind)iek_namespace);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (required_token_no_advance(tok_rbrace, ec_exp_rbrace)) {
      /* Closing right brace was found. */
      cannot_bind_to_curr_construct();
    } else {
      discard_curr_construct_pragmas();
    }  /* if */
    if (namespace_scope_pushed) {
#if GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      if (scope_stack_top().ELF_visibility !=
                                    (an_ELF_visibility_kind)evk_unspecified) {
        /* The namespace had a visibility attribute, which implied an entry was
           pushed on the ELF visibility stack.  Pop an entry now that the
           namespace scope is being popped.  (A warning will be issued if this
           pop operation doesn't match the push operation implied by the
           visibility attribute). */
        pop_ELF_visibility(/*namespace_attribute=*/TRUE);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      /* Pop the namespace or namespace-extension scope. */
      pop_namespace_scope();
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* If (because of an error) an empty source-sequence entry was left in the
     list, remove it now. */
  if (namespace_ssep != NULL &&
      namespace_ssep->entity.kind == (a_byte_il_entry_kind)iek_none) {
    remove_from_src_seq_list(namespace_ssep);
    namespace_ssep = NULL;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Record extended position information for the namespace or namespace
     alias definition.  If this is a namespace extension definition or a
     repeated namespace alias, the extended position information is recorded
     in the associated secondary source sequence entry (if available). */
  if (nsp == NULL) {
    check_assertion(total_errors != 0);
  } else if (namespace_ssep == NULL) {
    /* No source sequence entry was recorded.  Only record position information
       if none was recorded before.  Predeclared namespaces may not yet have a
       position supplement block. */
    if (nsp->source_corresp.decl_pos_info == NULL) {
      nsp->source_corresp.decl_pos_info =
                       alloc_decl_position_supplement(/*at_file_scope=*/TRUE);
    }  /* if */
    if (nsp->source_corresp.decl_pos_info->specifiers_range.start.seq == 0) {
      decl_pos_info = nsp->source_corresp.decl_pos_info;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  } else if (ss_entry_kind(namespace_ssep) == iek_namespace) {
    if (nsp->source_corresp.decl_pos_info == NULL) {
      nsp->source_corresp.decl_pos_info =
                       alloc_decl_position_supplement(/*at_file_scope=*/TRUE);
    }  /* if */
    decl_pos_info = nsp->source_corresp.decl_pos_info;
  } else if (ss_entry_kind(namespace_ssep) == iek_src_seq_secondary_decl) {
    decl_pos_info = alloc_decl_position_supplement(/*at_file_scope=*/TRUE);
    ss_entry_ptr(namespace_ssep, a_src_seq_secondary_decl_ptr)->decl_pos_info =
                                                                 decl_pos_info;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else {
    check_assertion(total_errors != 0);
  }  /* if */
  if (decl_pos_info != NULL) {
    decl_pos_info->specifiers_range.start = namespace_pos;
    decl_pos_info->specifiers_range.end = identifier_end_pos;
    decl_pos_info->identifier_range.start = locator.source_position;
    decl_pos_info->identifier_range.end = identifier_end_pos;
    decl_pos_info->variant.namespace_definition_range.start = def_start_pos;
    decl_pos_info->variant.namespace_definition_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* namespace_declaration */


static void using_directive(a_decl_parse_state  *dps,
                            a_source_position   *using_pos)
/*
Scan a using directive.  Its syntax is:

  using namespace namespace-name

The caller has consumed the "using" token (whose position is given by
using_pos): The current token is "namespace".  A using-directive entry is
created and activated for the current scope.
*/
{
  a_symbol_ptr		sym;
  a_boolean		err = FALSE;
  an_attribute_ptr	attributes = dps->prefix_attributes;

  db_enter(3, "using_directive");
  /* A using-directive is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_namespaces_in_embedded_cplusplus);
  /* Bypass "namespace". */
  (void)get_token();
  add_stop_token(tok_semicolon);
  if (!is_decl_qualified_name_start()) {
    syntax_error(ec_exp_identifier);
    /* Ignore pragma declarations. */
    discard_curr_construct_pragmas();
  } else {
    /* Scan the namespace name. */
    sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
                                                     ilm_namespace, &err);
    if (err) {
      /* A diagnostic has already been issued. */
    } else if (sym == NULL || sym->kind != (a_symbol_kind)sk_namespace) {
      error(ec_missing_namespace_name);
      err = TRUE;
    } else if (locator_for_curr_id.specific_symbol->kind ==
                     (a_symbol_kind)sk_namespace_projection &&
               locator_for_curr_id.specific_symbol->ambiguous) {
      /* Note: the lookup returns the projection symbol, if there is one,
         in the locator, so that's what needed to be tested for ambiguity. */
      sym_error(ec_ambiguous_name, locator_for_curr_id.specific_symbol);
      err = TRUE;
    }  /* if */
    (void)get_token();
    if (curr_token == tok_attribute) {
      *f_last_attribute_link(&attributes) =
                                        scan_gnu_attribute_groups(al_postfix);
    }  /* if */
    if (err) {
      /* Ignore pragma declarations. */
      discard_curr_construct_pragmas();
    } else {
      /* Pragmas cannot bind to a using declaration. */
      cannot_bind_to_curr_construct();
      mark_referenced(sym, &locator_for_curr_id.source_position);
      /* Allocate a using-directive entry specifying this namespace and
         activate it. */
      make_using_directive(sym->variant.namespace_info.ptr, depth_scope_stack,
                           using_pos, /*compiler_generated=*/FALSE,
                           /*inline_namespace=*/FALSE, attributes);
    }  /* if */
  }  /* if */
  remove_stop_token(tok_semicolon);
  /* Check for final semicolon in the caller. */
  db_exit();
}  /* using_directive */


a_using_decl_ptr make_using_decl(a_symbol_ptr      sym,
                                 a_source_position *pos,
				 a_scope_depth	   scope_depth)
/*
Allocate a using-decl entry, set its fields based on sym, add it to the list
for the current scope, and return a pointer to it.  This routine is used for
class member using-declarations and nonmember using-declarations; similar
processing is done for using-directives by make_using_directive.  scope_depth
is the scope depth of the list on which the using-declaration should be
placed.
*/
{
  a_using_decl_ptr  udp;
  an_il_entry_kind  kind;
  char              *entity;

  /* Determine the IL entity and the entity-kind, based on the symbol. */
  entity = il_entry_for_symbol(sym, &kind);
  check_assertion_str(kind != (an_il_entry_kind)iek_none,
                      "make_using_decl: no IL entry for symbol");
  /* Allocate a using-decl entry, and make it point to the IL entity. */
  udp = alloc_using_decl();
  udp->entity.kind = (a_byte_il_entry_kind)kind;
  udp->entity.ptr = entity;
  udp->position = *pos;
  /* Attach it the list for the current scope. */
  add_to_using_decls_list(udp, scope_depth);

  return udp;
}  /* make_using_decl */


static void create_nonmember_using_declaration(
                                       a_symbol_ptr     sym,
                                       a_symbol_ptr     *overload_sym_ptr,
                                       a_symbol_ptr     other_decl,
                                       a_namespace_ptr  nsp,
                                       a_type_ptr       class_type,
                                       a_using_decl_ptr *prev_udp,
                                       a_boolean        is_list,
                                       a_boolean        suppress_redecl_error)
/*
Create a projection for symbol "sym" from namespace "nsp" (or, in
Microsoft bugs mode, from the class "class_type").  If this is part of
an overload set being imported, "is_list" will be TRUE and
"*overload_sym_ptr" will point to the overload symbol.  Even when it
is not part of an overload set, we may need to create such a set
because existing declarations in the scope are being overloaded.
"*prev_udp" is the previous using-declaration structure for the
using-declaration construct that is currently being processed (NULL if
none).
*/
{
  a_symbol_locator   locator;
  a_symbol_ptr       new_sym;
  a_symbol_ptr       overload_sym = *overload_sym_ptr;
  a_symbol_ptr       fund_sym = fundamental_symbol_of(sym);
  a_source_position  decl_pos;

  locator = locator_for_curr_id;
  clear_specific_symbol(locator);
  decl_pos = locator_for_curr_id.source_position;
  if (fund_sym->kind == (a_symbol_kind)sk_undefined) {
    /* Undefined symbols have no IL entries, so don't create an
      IL entry for this using-declaration. */
  } else {
    /* Create a using-decl entry to represent this declaration in
       the IL. */
    a_using_decl_ptr  udp = make_using_decl(fund_sym, &decl_pos,
                                            depth_scope_stack);
    /* Record the namespace (or class) that was actually specified in the
       qualified name in the source.  Nonmember using-declarations generally
       refer to nonmember entities, but in Microsoft bugs mode a nonmember
       using-declaration can refer to a member type. */
    if (class_type != NULL) {
      udp->is_class_member = TRUE;
      udp->qualifier.class_type = class_type;
    } else {
      udp->qualifier.namespace_ptr = nsp;
    }  /* if */
    /* Update cross-reference and source-sequence info, if required. */
    record_using_decl(fund_sym, &decl_pos, udp, *prev_udp);
    /* Set *prev_udp for a possible subsequent call to this function. */
    *prev_udp = udp;
  }  /* if */
  if (overload_sym == NULL) {
    /* If we bring in a type that was declared previously, suppress a
       redeclaration error.  This is similar to the case
       "typedef struct S {} S;". */
    if (!suppress_redecl_error && other_decl != NULL &&
        is_type_symbol(other_decl) && is_type_symbol(fund_sym)) {
      a_type_ptr  type1 = type_symbol_type(other_decl),
                  type2 = type_symbol_type(fund_sym);
      suppress_redecl_error = identical_types(type1, type2);
    }  /* if */
    /* No overloading. */
    new_sym = enter_namespace_projection_symbol(fund_sym,
                                                /*is_using_decl=*/TRUE,
                                                &locator,
                                                depth_scope_stack,
                                                suppress_redecl_error);
    /* If is_list is TRUE, there will be overloading on the next
       iteration of this loop. */
    if (is_list) { *overload_sym_ptr = new_sym; }
  } else if (already_in_lookup_set(overload_sym, sym,
                                   /*is_using_dir=*/FALSE, IDL_NO_OPTIONS)) {
    /* Don't try to add a symbol that is already pointed to by
       overload_sym. */
    goto done;
  } else if (conflicts_with_previous_function_decl(
                                         fund_sym, overload_sym, &decl_pos)) {
    /* A function introduced by a using declaration cannot have the
       same type as a function already declared in the scope
       (WP 7.3.3 [namespace.udecl] paragraph 12).  The diagnostic
       will have been issued by the subroutine; don't create a
       projection symbol. */
    /* In Microsoft mode, two distinct IL entries may have been created for
       declarations of an extern "C" function in different namespaces.
       Microsoft compilers allow one of these to be brought into the scope
       of the other one with a using-declaration (though an attempt to call
       the function will result in an overload ambiguity). */
    goto done;
  } else {
    /* Add a new symbol to the overload set. */
    new_sym = make_namespace_projection_symbol(fund_sym,
                                               &locator.source_position,
                                               depth_scope_stack);
    overload_sym = add_symbol_to_overload_list(new_sym, overload_sym,
                                               /*use_namespace=*/FALSE,
                                               (a_namespace_ptr)NULL);
    if (overload_sym != *overload_sym_ptr) {
      *overload_sym_ptr = overload_sym;
      set_namespace_membership(overload_sym, (a_source_correspondence *)NULL,
                               (a_namespace_ptr)NULL);
    }  /* if */
  }  /* if */
  set_namespace_membership(new_sym, (a_source_correspondence *)NULL,
                           (a_namespace_ptr)NULL);
done:;
}  /* create_nonmember_using_declaration */


static void import_any_hidden_tags(a_symbol_ptr      other_decl,
                                   a_namespace_ptr   nsp,
                                   a_using_decl_ptr  *prev_udp,
                                   a_boolean         *redecl_error)
/*
A name that is imported by a using-declaration can refer to both tag names and
non-tag names.  In those cases, ordinary namespace-qualified lookup will only
find the non-tag, and this routine is used to also import the tag.  other_decl
is a symbol representing a declaration already found in the current scope.
nsp is the namespace from which to import the hidden tag.  *prev_udp is set to
the using-declaration structure that is created (if any).  *redecl_error is
TRUE if and only if a redeclaration error is issued.
*/
{
  /* Check if we missed a tag symbol; it should be imported too. */
  a_symbol_ptr      null_sym_ptr = NULL, tag_sym;
  a_symbol_locator  locator;

  *redecl_error = FALSE;
  locator = locator_for_curr_id;
  clear_specific_symbol(locator);
  /* Look for a tag symbol in the namespace referenced by the
     using-declaration. */
  if (nsp == NULL) {
    tag_sym = file_scope_id_lookup(il_header.primary_scope,
                                   &locator,
                                   IDL_MUST_BE_TAG |
                                   IDL_DIRECT_NAMESPACE_MEMBERS_ONLY);
  } else {
    tag_sym = namespace_qualified_id_lookup(
                          &locator, nsp,
                          IDL_MUST_BE_TAG | IDL_DIRECT_NAMESPACE_MEMBERS_ONLY);
  }  /* if */
  /* Tag lookups can sometimes return typedefs.  If we got a typedef back,
     ignore it. */
  if (tag_sym != NULL && !is_tag_symbol(tag_sym)) tag_sym = NULL;
  if (tag_sym != NULL) {
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
    if (other_decl != NULL && !is_type_symbol(other_decl)) {
      /* The current scope already contains a homonym for the imported tag,
         but if it's a nontype, it won't conflict.  However, such a nontype
         entity may be hiding another tag of the same name: Look it up and
         update other_decl accordingly. */
      clear_specific_symbol(locator);
      (void)curr_scope_id_lookup(&locator, IDL_MUST_BE_TAG |
                                           IDL_PROJ_SYMBOL_ALLOWED);
      other_decl = locator.specific_symbol;
    }  /* if */
    if (!is_class_template_symbol(tag_sym) &&
        !(other_decl != NULL && is_file_or_namespace_scope(ssep) &&
          symbols_are_lookup_equivalent(fundamental_symbol_of(tag_sym),
                                        fundamental_symbol_of(other_decl),
                                        /*merge_gpp_c_routines=*/FALSE,
                                        IDL_NO_OPTIONS))) {
      /* We found a tag that was masked by another declaration (sym),
         and importing it is not just a redeclaration. */
      create_nonmember_using_declaration(tag_sym, &null_sym_ptr,
                                         other_decl, nsp, (a_type_ptr)NULL,
                                         prev_udp, /*is_list=*/FALSE,
                                         /*suppress_redecl_error=*/FALSE);
      clear_specific_symbol(locator);
      *redecl_error = (curr_scope_id_lookup(
                          &locator, IDL_MUST_BE_TAG | IDL_PROJ_SYMBOL_ALLOWED)
                         != NULL);
    }  /* if */
  }  /* if */
}  /* import_any_hidden_tags */


static void nonmember_using_declaration(a_decl_parse_state  *dps)
/*
Scan a using_declaration in a nonclass scope.  Its syntax is:

  using qualified-name ;

The "using" token was consumed by the caller: The current token is (presumably)
a qualified-name.
A sk_namespace_projection is created and added to the symbol table for the
current scope.
*/
{
  a_symbol_ptr             sym, fund_sym, overload_sym, other_decl,
                             fund_other_decl;
  a_boolean                err = FALSE;
  a_symbol_locator         locator;
  a_boolean                is_list = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  db_enter(3, "nonmember_using_declaration");
  /* A using declaration is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &dps->start_pos,
                                          ec_using_decl_in_embedded_cplusplus);
  add_stop_token(tok_semicolon);
  if (!is_decl_qualified_name_start() && curr_token != tok_typename) {
    syntax_error(ec_exp_identifier);
    /* Ignore pragma declarations. */
    discard_curr_construct_pragmas();
  } else {
    if (curr_token == tok_typename) {
      /* A "using typename ..." declaration.  Process the typename
         specifier. */
      a_type_ptr  tp;
      typename_specifier(&tp, &sym, /*within_using_decl=*/TRUE,
                         /*is_decl_specifier=*/FALSE,
                         (a_decl_pos_block_ptr)NULL);
      /* An error type will be returned if an error was detected by
         typename_specifier. */
      if (is_error_type(tp)) {
        err = TRUE;
      }  /* if */
    } else {
      sym = coalesce_and_lookup_generalized_identifier(
                                 GID_TEMPLATE_ARGS_OPTIONAL, ilm_normal, &err);
    }  /* if */
    if (err) {
      /* Diagnostic has already been issued. */
    } else if (sym == NULL) {
      str_error(ec_undefined_identifier,
                locator_for_curr_id.symbol_header->identifier);
      err = TRUE;
    } else if (!locator_for_curr_id.is_qualified_name &&
               !nonstandard_using_decl_allowed) {
      /* An unqualified name is not allowed here.  This is optionally
         permitted because the Sun 5.0 compiler accepts an unqualified
         name in a using-declaration. */
      error(ec_namespace_qualified_name_required);
      err = TRUE;
    } else if (locator_for_curr_id.is_class_member &&
               !(microsoft_bugs && microsoft_version <= 1310 &&
                 is_type_symbol(sym))) {
      /* A class-qualified name is not allowed here.  Such a name is permitted
         in Microsoft bugs mode (with microsoft_version <= 1310) if it refers
         to a type.  The Microsoft compilers (through 7.1) permit such using-
         declarations. */
      error(ec_class_qualified_name_not_allowed);
      err = TRUE;
    } else if (locator_for_curr_id.is_template_id) {
      /* A template-id (that is, template-name<template-args>) is not allowed
         here. */
      error(ec_template_id_not_allowed);
      err = TRUE;
    } else if (sym->kind == (a_symbol_kind)sk_namespace) {
      pos_error(ec_namespace_name_not_allowed,
                &locator_for_curr_id.source_position);
      err = TRUE;
    }  /* if */
    if (err) {
      /* Ignore pragma declarations. */
      discard_curr_construct_pragmas();
    } else {
      a_namespace_ptr  nsp;
      a_type_ptr       class_type;
      /* Pragmas cannot bind to a using declaration. */
      cannot_bind_to_curr_construct();
      nsp = qualifier_namespace_ptr(locator_for_curr_id);
      class_type = qualifier_class_type(locator_for_curr_id);
      if (nsp != NULL &&
          ssep->il_scope != NULL &&
          ssep->il_scope->kind == (a_scope_kind)sck_namespace &&
          ssep->il_scope->variant.assoc_namespace ==
                                               skip_namespace_aliases(nsp)) {
        /* Attempting a using-declaration with a namespace qualifier that is
           the same as the current namespace:
             namespace N { int i; using N::i; }
           Issue a warning and ignore the using-declaration. */
        warning(ec_useless_using_declaration);
      } else if (depth_scope_stack == DEPTH_OF_FILE_SCOPE && nsp == NULL &&
                 class_type == NULL) {
        /* Attempting a using declaration at file scope with name already
           declared in the file scope -- e.g.,
             int i; using ::i;
           Issue a warning and ignore the using-declaration.  When using the
           g++ compatibility feature that treats "std" as a synonym for the
           global namespace, suppress this processing. */
        if (!ignore_std_namespace) {
          check_assertion(locator_for_curr_id.is_global_qualified_name ||
                          nonstandard_using_decl_allowed);
          warning(ec_useless_using_declaration);
        }  /* if */
      } else {
        check_assertion(nsp != NULL ||
                        class_type != NULL || 
                        locator_for_curr_id.is_global_qualified_name ||
                        nonstandard_using_decl_allowed ||
                        ignore_std_namespace);
        locator = locator_for_curr_id;
        clear_specific_symbol(locator);
        /* Look for a declaration of the same name in the current scope. */
        (void)curr_scope_id_lookup(&locator, IDL_PROJ_SYMBOL_ALLOWED);
        other_decl = locator.specific_symbol;
        fund_other_decl = (other_decl == NULL) ?
                                     NULL : fundamental_symbol_of(other_decl);
        overload_sym = NULL;
        if (is_function_symbol(sym) ||
            sym->kind == (a_symbol_kind)sk_function_template) {
          /* The specified name represents a function or function template (or
             overload set thereof) so we need to create or add to an overload
             set in the current scope, too. */
          if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
            /* Using an overload set. */
            is_list = TRUE;
            sym = sym->variant.overloaded_function.symbols;
          }  /* if */
          if (other_decl != NULL) {
            if (is_function_symbol(fund_other_decl) ||
                fund_other_decl->kind == (a_symbol_kind)sk_function_template) {
              /* Overloading is okay. */
              overload_sym = other_decl;
            } else {
              /* There is no function symbol in the current scope with which
                 the new symbol should be overloaded. */
            }  /* if */
          }  /* if */
        }  /* if */
        fund_sym = fundamental_symbol_of(sym);
        if (other_decl != NULL && overload_sym == NULL &&
            other_decl->decl_position.seq != 0 &&
            is_file_or_namespace_scope(ssep) &&
            symbols_are_lookup_equivalent(fund_sym, fund_other_decl,
                                          /*merge_gpp_c_routines=*/FALSE,
                                          IDL_NO_OPTIONS)) {
          /* This is a duplicate using declaration of something other than a
             function or function template.  7.3.3 [namespace.udecl] para 7
             says duplicates are allowed in file or namespace scope, so ignore
             the declaration.  (However, do not ignore the using declaration
             if it duplicates a built-in declaration, i.e. seq == 0). */
        } else {
          a_using_decl_ptr  prev_udp = NULL;
          a_boolean         suppress_redecl_error = FALSE;
          /* Create the new sk_namespace_projection symbol(s). */
          if (!is_tag_symbol(fund_sym)) {
            /* Check if we missed a tag symbol; it should be imported too. */
            import_any_hidden_tags(other_decl, nsp, &prev_udp,
                                     &suppress_redecl_error);
          }  /* if */
          /* If we're importing a typedef that redeclares an existing type
             to the same name, inhibit the declaration error. */
          if (fund_other_decl != NULL &&
              fund_sym->kind == (a_symbol_kind)sk_type) {
            a_symbol_ptr  prev_tag_sym = NULL;
            if (is_tag_symbol(fund_other_decl)) {
              /* If the previous declaration was a tag name, and the new
                 declaration is also a tag name, we should have caught the
                 duplicate earlier. */
              check_assertion(!is_tag_symbol(fund_sym));
              prev_tag_sym = fund_other_decl;
            } else {
              /* Look up a tag in the current scope: */
              clear_specific_symbol(locator);
              prev_tag_sym = curr_scope_id_lookup(
                                   &locator,
                                   IDL_MUST_BE_TAG | IDL_PROJ_SYMBOL_ALLOWED);
            }  /* if */
            if (prev_tag_sym != NULL) {
              /* There was a previous tag.  If the newly imported type is
                 identical to the tagged type, suppress the redeclaration
                 error. */
              a_type_ptr  tp1 = type_symbol_type(prev_tag_sym);
              a_type_ptr  tp2 = type_symbol_type(fund_sym);
              if (identical_types(tp1, tp2)) { suppress_redecl_error = TRUE; }
            }  /* if */
          }  /* if */
          for (; sym != NULL; sym = is_list ? sym->next : NULL) {
            create_nonmember_using_declaration(sym, &overload_sym, other_decl,
                                               nsp, class_type,
                                               &prev_udp, is_list,
                                               suppress_redecl_error);
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* if */
    /* Bypass the identifier. */
    (void)get_token();
  }  /* if */
  remove_stop_token(tok_semicolon);
  /* Check for final semicolon in the caller. */
  db_exit();
}  /* nonmember_using_declaration */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/  /* p_end_of_using_pos is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
void alias_declaration(a_decl_parse_state  *dps,
                       a_source_position   *p_end_of_using_pos)
/*
Handle a declaration of the form:

	using <identifier> = <type-id> ;

which is essentially equivalent to a typedef.

The caller has consumed the "using" token: The <identifier> is the current
token.  The caller is also responsible for checking and consuming the final
semicolon.

*dps describes the declaration (which can be a class member or not).
*p_end_of_using_pos is the end position of the "using" token.
*/
{
  a_symbol_locator  loc;
  a_decl_pos_block  decl_pos_block;

  clear_decl_pos_block(&decl_pos_block);
  add_stop_token(tok_semicolon);
  check_assertion(curr_token == tok_identifier);
  decl_pos_block.decl_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  decl_pos_block.specifiers_range.start = dps->start_pos;
  decl_pos_block.specifiers_range.end = *p_end_of_using_pos;
  decl_pos_block.identifier_range.start = pos_curr_token;
  decl_pos_block.identifier_range.end = end_pos_curr_token;
  decl_pos_block.declarator_range.start = pos_curr_token;
  decl_pos_block.declarator_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  loc = locator_for_curr_id;
  if (loc.is_qualified_name || loc.is_operator_name ||
      loc.is_udl_operator_name) {
    pos_error(loc.is_qualified_name ? ec_qualified_name_not_allowed
                                    : ec_operator_name_not_allowed,
              &pos_curr_token);
    set_to_error_locator(loc);
  }  /* if */
  (void)get_token();
  /* Although the alias name is not technically a "declarator-id", it has
     exactly the same function and relation to any subsequent attributes.
     We therefore record the attributes with al_declarator_id. */
  dps->id_attributes = scan_attributes(al_declarator_id);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (dps->id_attributes != NULL) {
    decl_pos_block.declarator_range.end = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  dps->auto_type_allowed = FALSE;
  dps->is_alias = TRUE;
  if (required_token(tok_assign, ec_exp_assign)) {
    a_type_ptr  parent_type = NULL;
    if (dps->in_class_scope) {
      check_assertion(scope_stack_top().kind ==
                                        (a_scope_kind)sck_class_struct_union);
      parent_type = scope_stack_top().assoc_type;
    }  /* if */
    type_name_full(dps);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* It is tempting to emit the source sequence entry for the alias
       declaration when the name of the alias is first encountered (above),
       since that name has some similarity to a declarator.  However, doing so
       could create complications if type_name_full parses a non-autonomous
       tag declaration: Such a declaration may have to be promoted to become
       autonomous later on, and that promotion process requires that the
       source sequence entry for the top-level declaration follows the entry
       for the non-autonomous tag declaration. */
    dps->source_sequence_entry = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    decl_typedef(&loc, dps, parent_type, &decl_pos_block);
    if (dps->sym != NULL && dps->sym->kind == (a_symbol_kind)sk_type) {
      a_type_ptr  tp = dps->sym->variant.type.ptr;
      if (type_is_typedef(tp)) {
        /* Record that the typedef was expressed via an alias declaration.
           For primary declarations, this is done directly in the a_type entry;
           for secondary declarations, the flag is in the associated
           a_src_seq_secondary_decl entry. */
        if (dps->first_decl) {
          tp->variant.typeref.is_alias = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        } else if (dps->source_sequence_entry != NULL &&
                   ss_entry_kind(dps->source_sequence_entry) ==
                                                 iek_src_seq_secondary_decl) {
          ss_entry_ptr(dps->source_sequence_entry,
                       a_src_seq_secondary_decl_ptr)->is_alias = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        /* Record the location of the type-id. */
        tp->variant.typeref.extra_info->type_id_range.start = dps->start_pos;
        tp->variant.typeref.extra_info->type_id_range.end =
                                                  curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
    }  /* if */
  }  /* if */
  record_entity_in_decl_stmt_if_needed(dps->sym);
  remove_stop_token(tok_semicolon);
  /* Check for final semicolon in the caller. */
}  /* alias_declaration */


void check_prefix_attributes_without_a_declarator(a_decl_parse_state  *dps)
/*
*dps describes a simple declaration without a declarator.  If the declaration
includes prefix attributes, those attributes appertain to no entity: Issue a
diagnostic as appropriate in such cases.
*/
{
  diagnose_unattached_attributes(dps->prefix_attributes);
}  /* check_prefix_attributes_without_a_declarator */


static a_boolean check_for_missing_declarator(a_decl_parse_state  *state)
/*
The decl-specifiers have been scanned.  Check for the case in which a
declarator is missing, in which case return TRUE after issuing appropriate
diagnostics.  (It is not always an error -- for example, an "autonomous"
class definition like "struct S { int i; };".)

state describes the declaration parsed so far.
*/
{
  a_decl_flag_set    dso_flags = state->dso_flags;
  a_boolean          declarator_omitted = FALSE;
  a_boolean          declares_something;
  a_boolean          defines_something;
  a_boolean          inline_specified = ((dso_flags & DSO_INLINE) != 0);
  an_error_severity  severity;
  a_type_ptr         type_ptr = state->specifiers_type;
  a_type_ptr         tp = skip_typerefs(type_ptr);

  declares_something = ((dso_flags & DSO_DECLARES_SOMETHING) != 0);
  if (curr_token == tok_semicolon) {
    defines_something = ((dso_flags & DSO_DEFINES_SOMETHING) != 0);
    declarator_omitted = TRUE;
    if (state->decl_specifiers_error) {
      /* Don't issue further errors on this declaration. */
    } else if (state->is_old_style_param_decl &&
               (declares_something || defines_something)) {
      /* ANSI C does not allow freestanding declarations (as of structs)
         within an old-style parameter list.  pcc, on the other hand,
         will allow something like
            int f(a)
            struct s {int b;};
            struct s a;
            { ... }
      */
      if (C_dialect != C_dialect_pcc) {
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_decl_should_be_of_param);
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (defines_something) {
        tp->autonomous_primary_tag_decl = TRUE;
      } else {
        (void)set_src_seq_secondary_decl_fields((char *)tp, (a_type_ptr)NULL,
                                                (a_name_reference_ptr)NULL,
                                                SSSD_AUTONOMOUS_TAG_DECL);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (!declares_something && C_dialect == C_dialect_cplusplus &&
               defines_something &&
               (type_ptr->kind == (a_type_kind)tk_union ||
                ((gpp_mode || microsoft_mode) &&
                 tp->kind == (a_type_kind)tk_union)) &&
               state->declared_storage_class != (a_storage_class)sc_typedef) {
      /* Special C++ case:  the declaration of an anonymous union.   Do the
         required error checking and special processing, including creation
         of a variable which will represent the anonymous union and with
         which its fields will be aliased. */
      check_assertion(is_unnamed_tag_symbol(symbol_for(tp)));
      decl_anonymous_union_variable(state);
      /* The anonymous union variable is marked as referenced, as are all
         unnamed entities.  So its type is also marked referenced. */
      tp->source_corresp.referenced = TRUE;
    } else if (state->is_linkage_spec_decl && is_immediate_enum_type(tp)) {
      /* This is a declaration like
                      extern "C" enum E { e1, e2, e3 };
         which is not allowed (inference from ARM 7.4). */
      pos_error(ec_enum_not_allowed, &state->start_pos);
    } else {
      if (state->declared_storage_class == (a_storage_class)sc_typedef) {
        /* Typedef declaration with no declarator. */
        severity = es_warning;
        if (declares_something ||
            (C_mode() && defines_something && is_immediate_enum_type(tp))) {
          /* No error on a case like "typedef struct S { int i; };" or
             "typedef enum { red, green, blue };" -- see first constraint,
             Section 3.5 of the ANSI C standard.  However, a warning should
             be issued, since the "typedef" is superfluous. */
        } else {
          /* A case like "typedef int;" or "typedef struct { int i; };" --
             gets a warning by default but may get an error in strict ANSI
             mode. */
          if (strict_ansi_mode) severity = strict_ansi_error_severity;
        }  /* if */
        set_err_pos_to_curr_token();
        diagnostic(severity, ec_missing_typedef_name);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (defines_something) {
          tp->autonomous_primary_tag_decl = TRUE;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ASM_FUNCTION_ALLOWED
      } else if (state->declared_storage_class == (a_storage_class)sc_asm) {
        pos_error(ec_bad_asm_function_def, &pos_curr_token);
#endif /* ASM_FUNCTION_ALLOWED */
      } else {
        if (!declares_something) {
          /* The specifiers should have declared something or this declaration
             is pointless.  Examples would be
                int ;
                struct { int i; };
             whereas, despite the missing declarator,
                struct x {int a;};
             is not useless since it declares something (namely x). */
          /* ANSI probably thinks of this as an error, but that seems a bit
             extreme, especially since pcc allows it.  Normally we issue a
             warning, unless the -A option is selected. */
          severity = strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning;
          diagnostic(severity, ec_useless_decl);
        }  /* if */
        /* A storage class can only be specified for an object or a function
           (ARM 7.1.1).  For Embedded C we also issue a strict error if a
           named-register storage class was specified. */
        if (state->declared_storage_class != (a_storage_class)sc_unspecified) {
          severity = ((C_mode() && state->register_id == 0) ||
                      any_cfront_mode() || microsoft_mode) ?
                                           es_warning : es_discretionary_error;
          diagnostic(severity, ec_storage_class_requires_function_or_variable);
        }  /* if */
        /* ARM 7.1.6 implies that the absence of an object in this declaration
           makes it ill-formed.  Is the implication strong enough to justify
           an error here? */
        if (is_qualified_type(type_ptr)) {
          severity = (C_dialect == C_dialect_cplusplus && strict_ansi_mode) ?
                       strict_ansi_error_severity : es_warning;
          pos_diagnostic(severity, ec_useless_type_qualifiers,
                         &state->start_pos);
        }  /* if */
        /* Inline can only be specified for a function (ARM 7.1.2). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (defines_something || declares_something) {
          /* This is a class/struct/union or enum declaration. */
          if (defines_something) {
            tp->autonomous_primary_tag_decl = TRUE;
          } else {
            (void)set_src_seq_secondary_decl_fields((char *)tp,
                                                    (a_type_ptr)NULL,
                                                    (a_name_reference_ptr)NULL,
                                                    SSSD_AUTONOMOUS_TAG_DECL);
          }  /* if */
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
    cannot_bind_to_curr_construct();
  } else if ((dso_flags & DSO_DANGLING_TYPE_SPECIFIER) != 0 ||
             ((dso_flags & DSO_DEFINES_SOMETHING) != 0 && 
              !(curr_token == tok_identifier || is_declarator_start() ||
                curr_token == tok_ptr_to_member)) ||
             (!C_mode() && identifier_is_template_id() &&
              locator_for_curr_id.specific_symbol != NULL &&
              is_type_symbol(locator_for_curr_id.specific_symbol))) {
    /* The "dangling type specifier" case -- a class, struct, union, or
       enum definition was followed by a type specifier keyword.  This is
       treated as a missing-semicolon error, since the type specifier can
       be taken as introducing a new declaration.  In addition, if a class or
       enumeration type was defined, and what follows does not look like a
       declarator the error is also handled as a missing-semicolon case (some
       non-declarators, like template-ids and pointer-to-member constants are
       treated as "looking like a declarator").  Finally, if a template-id
       that denotes a type follows, the code is also treated like a dangling
       type specifier. */
    declarator_omitted = TRUE;
    if (state->decl_specifiers_error) {
      /* Don't issue further errors on this declaration. */
    } else if (declares_something) {
      if (state->is_old_style_param_decl) {
        /* An old style param declaration that introduces a named struct or
           enum type but has no declarator for the parameter. */
        pos_error(ec_decl_should_be_of_param, &state->start_pos);
      } else {
        /* Maybe something "struct A { ... } int i;", where the declaration
           is okay and the problem is that a semicolon is missing. */
      }  /* if */
    } else {
      /* A declaration that introduces an unnamed struct or enum type but has
         no declarator.  May or may not be in an old-style param list. */
      pos_error(ec_exp_identifier, &pos_curr_token);
    }  /* if */
    /* Issue the missing-semicolon error. */
    pos_error(ec_exp_semicolon, &pos_curr_token);
    discard_curr_construct_pragmas();
  }  /* if */
  if (declarator_omitted) {
    if (inline_specified && !state->decl_specifiers_error) {
      /* GNU C (but not GNU C++) allows this. */
      pos_diagnostic(gcc_mode ? es_warning : es_error,
                     ec_inline_and_nonfunction, &state->start_pos);
    }  /* if */
    /* Prefix attributes require a declarator. */
    check_prefix_attributes_without_a_declarator(state);
  }  /* if */
  return declarator_omitted;
}  /* check_for_missing_declarator */


static void report_member_function_redeclaration(a_symbol_locator    *locator,
                                                 a_decl_parse_state  *state)
/*
A declaration of a class member function that is not also a definition
can not appear outside the class.  Upon entering this routine we already know
we encountered such a declaration: Issue the appropriate diagnostic (except
for some situations in Microsoft and GNU C++ modes).  locator and state
describe the current declaration.
*/
{
  a_symbol_ptr  sym = locator->specific_symbol;
  a_source_position_ptr  pos = &state->declarator_pos;

  if (is_member_function_symbol(locator->specific_symbol)) {
    /* If this is a member function, but one with a type that doesn't
       match a previously declared member, see if it matches an
       instance of a member template.  If it does, assume that it is
       an attempt to declare a specialization with the incorrect
       old-style specialization syntax. */
    a_boolean   is_member_redecl;
    a_boolean   is_template_instance;
    a_type_ptr  type = state->type;
    is_member_redecl = member_function_redecl_sym(
          sym, state, (a_template_param_ptr)NULL, (a_symbol_ptr*)NULL) != NULL;
    is_template_instance = has_matching_template_instance(
                                       sym, type, locator->template_arg_list);
    if (!is_member_redecl && is_template_instance) {
      pos_sy_error(ec_old_specialization_not_allowed,
                   &locator->source_position, sym);
      set_to_error_locator(*locator);
    } else {
      pos_sy_error(ec_member_function_redecl_outside_class, pos, sym);
      set_to_error_locator(*locator);
    }  /* if */
  } else {
    pos_sy_error(ec_not_compatible_with_previous_decl, pos, sym);
    set_to_error_locator(*locator);
  }  /* if */
}  /* report_member_function_redeclaration */


static void remove_all_local_stop_tokens(a_decl_parse_state  *state)
/*
Remove stop tokens as suggested by *state (and clear the associated flags).
*/
{
  if (state->need_semicolon_remove_stop_token) {
    remove_stop_token(tok_semicolon);
    state->need_semicolon_remove_stop_token = FALSE;
  }  /* if */
  if (state->need_comma_remove_stop_token) {
    remove_stop_token(tok_comma);
    state->need_comma_remove_stop_token = FALSE;
  }  /* if */
  if (state->need_assign_remove_stop_token) {
    remove_stop_token(tok_assign);
    state->need_assign_remove_stop_token = FALSE;
  }  /* if */
  if (state->need_lbrace_remove_stop_token) {
    remove_stop_token(tok_lbrace);
    state->need_lbrace_remove_stop_token = FALSE;
  }  /* if */
}  /* remove_all_local_stop_tokens */


void check_main_function(a_func_info_block_ptr  func_info,
                         a_type_ptr             type,
                         a_decl_parse_state     *dps,
                         a_boolean              *is_inline,
                         a_source_position_ptr  pos)
/*
Check some constraints on the main() function whose declaration is described
by func_info, type, and dps.  If "*is_inline" is TRUE, the function was
declared "inline" (or was defined as an in-class friend).  Any diagnostics are
issued at the given position.
*/
{
  a_routine_type_supplement_ptr  rtsp;
  a_type_ptr                     return_type;
  a_type_ptr                     int_type =
                                        integer_type((an_integer_kind)ik_int);

  /* Diagnose return types other than "int". */
  return_type = skip_typerefs(type)->variant.routine.return_type;
  if (!identical_types(return_type, int_type)) {
    /* main must return "int" (3.6.1). */
    pos_diagnostic(strict_ansi_mode ?
                     strict_ansi_discretionary_severity : es_warning,
                   ec_bad_return_type_on_main, pos);
  }  /* if */
  if (c99_mode || !C_mode()) {
    /* Perform some error checking that is specific to C++ and/or C99: main()
       cannot be deleted and cannot be inline. */
    if (func_info->is_deleted) {
      pos_error(ec_deleted_main, pos);
      *is_inline = FALSE;
    } else if (*is_inline) {
      pos_error(ec_inline_main, pos);
      *is_inline = FALSE;
    }  /* if */
  }  /* if */
  if ((dps->dso_flags & DSO_CONSTEXPR) != 0) {
    /* main() cannot be declared constexpr (because that would imply that
       it is inline). */
    pos_error(ec_constexpr_main, pos);
    dps->dso_flags &= ~(a_decl_flag_set)DSO_CONSTEXPR;
  }  /* if */
  rtsp = skip_typerefs(type)->variant.routine.extra_info;
  if (!C_mode()) {
    if (rtsp->routine_name_linkage_is_explicit) {
      pos_warning(ec_linkage_specifier_not_allowed, pos);
      rtsp->routine_name_linkage_is_explicit = FALSE;
    }  /* if */
    rtsp->routine_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    if (rtsp->exception_specification != NULL) {
      /* main() cannot have a throw specification, since there's no
         call stack to unwind from main. */
      pos_warning(ec_exception_specification_not_allowed,
                  &func_info->throw_position);
      rtsp->exception_specification = NULL;
    }  /* if */
    /* "static" is not allowed (ARM 3.4). */
    if (dps->storage_class == (a_storage_class)sc_static) {
      pos_error(ec_static_not_allowed, pos);
      dps->storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
  }  /* if */
  /* Check for one of the standard signatures and issue remarks if
     declared differently. */
  if (rtsp->param_type_list != NULL) {
    a_param_type_ptr ptp = rtsp->param_type_list;
    if (!identical_types(ptp->type, int_type)) {
      pos_ty_remark(ec_main_first_param_not_int, pos, ptp->declared_type);
    }  /* if */
    ptp = ptp->next;
    if (ptp == NULL) {
      pos_remark(ec_main_wrong_num_params, pos);
    } else {
      /* The type of the second parameter should be declared as char *[]
         or char **, both of which appear as char ** in the transformed
         type. */
      a_boolean  p2type_is_correct = FALSE;
      if (is_pointer_type(ptp->type)) {
        a_type_ptr targ_type = type_pointed_to(ptp->type);
        if (is_pointer_type(targ_type)) {
          a_type_ptr char_type = integer_type(plain_char_int_kind);
          targ_type = type_pointed_to(targ_type);
          if (identical_types(targ_type, char_type)) {
            p2type_is_correct = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!p2type_is_correct) {
        pos_ty_remark(ec_main_second_param_wrong_type, pos,
                      ptp->declared_type);
      }  /* if */
      if (ptp->next != NULL) {
        pos_remark(ec_main_wrong_num_params, pos);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_main_function */

#if GNU_EXTENSIONS_ALLOWED

void scan_gnu_declarator_attributes(a_decl_parse_state  *dps)
/*
Scan GNU attribute groups and append them to dps->id_attributes.  The syntactic
location recorded for the attributes is al_postfix.
*/
{
  if (gnu_attributes_enabled && curr_token == tok_attribute) {
    *last_attribute_link(&dps->id_attributes) = scan_gnu_attribute_groups(
                                                                  al_postfix);
  }  /* if */
}  /* scan_gnu_declarator_attributes */


void gnu_attributes_after_parenthesized_initializer(a_variable_ptr      var,
                                                    a_decl_parse_state  *dps)
/*
Some versions of GNU C++ accept attributes appearing after a parenthesized
initializer.  Other versions ignore such attributes with a warning.  var is
a variable with such an initializer (which was just scanned; *dps describes
the declaration).  Scan any attributes that follow and apply them to the
variable (if applicable).
*/
{
  if (gpp_mode && curr_token == tok_attribute) {
    an_attribute_ptr  attributes = scan_attributes(al_post_initializer);
    an_attribute_ptr  ap = attributes;
    a_boolean         warning_emitted = FALSE, error_emitted = FALSE;
    for (ap = attributes; ap != NULL; ap = ap->next) {
      if (ap->family != (a_byte_attribute_family)af_gnu) {
        if (!error_emitted) {
          pos_error(ec_attribute_after_parenthesized_initializer,
                    &ap->position);
          error_emitted = TRUE;
        }  /* if */
        make_attr_unrecognized(ap);
      } else if (gnu_version < 30100 || gnu_version >= 30400) {
        /* GCC 3.1.x through 3.3.x recognized attributes after parenthesized
           initializers. */
        if (!warning_emitted) {
          pos_warning(ec_attribute_after_parenthesized_initializer,
                      &ap->position);
          warning_emitted = TRUE;
        }  /* if */
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* for */
    /* A declaration with an initializer is a primary declaration. */
    mark_primary_decl_attributes(attributes);
    attach_parse_state_to_attributes(dps);
    attach_attributes(attributes, (char*)var, iek_variable);
    detach_parse_state_from_attributes(dps);
  }  /* if */
}  /* gnu_attributes_after_parenthesized_initializer */


void report_gnu_postfix_attributes_on_function_definition(
                                                     a_decl_parse_state  *dps)
/*
If the function definition represented by *dps includes postfix GNU attributes
issue an error.
*/
{
  if (dps->id_attributes != NULL && gnu_attributes_enabled) {
    an_attribute_ptr  ap = dps->id_attributes;
    for (; ap != NULL; ap = ap->next) {
      if (ap->family == (a_byte_attribute_family)af_gnu &&
          ap->syntactic_location == (a_byte_attribute_location)al_postfix) {
        pos_error(ec_attributes_in_rout_defn, &ap->group->position);
        break;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* report_gnu_postfix_attributes_on_function_definition */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE

static void check_variable_decl_modifiers(a_variable_ptr      var_ptr,
                                          a_decl_parse_state  *dps)
/*
Check that the given variable's initialization (if any) is compatible with the
declaration modifiers recorded in *dps.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (var_ptr->decl_modifiers & DM_THREAD) {
    an_init_kind        init_kind;
    an_initializer_ptr  init;
    get_variable_initializer(var_ptr, (a_scope_ptr)NULL, &init_kind, &init);
    if (init_kind == (an_init_kind)initk_dynamic) {
      pos_error(ec_bad_init_for_thread_local, &dps->declarator_pos);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_... */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && microsoft_version < 1300 &&
      (var_ptr->decl_modifiers & DM_SELECTANY)) {
    /* In Microsoft versions prior to 1300, the "selectany" decl-modifier
       cannot appear with dynamic initialization nor with no initialization. */
    an_attribute_ptr  ap = find_decl_attribute(ak_selectany, dps);
    if (ap != NULL &&
        (var_ptr->init_kind == (an_init_kind)initk_dynamic ||
         var_ptr->init_kind == (an_init_kind)initk_none)) {
      pos_st_diagnostic(es_discretionary_error,
                        ec_decl_modifiers_invalid_for_this_decl,
                        &dps->declarator_pos, ap->name);
    }  /* if */        
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* check_variable_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

typedef enum an_end_of_decl_action {
  /* An enumeration type to represent the final steps in parsing a declaration.
     Used for a switch-statement in declaration(...). */
  eoda_not_at_end,	/* The end of the declaration hasn't been reached. */
  eoda_deferred_actions,
			/* Before scanning the final declaration token (a
			   semicolon), some deferred actions are needed in
			   Microsoft bugs mode (e.g., processing of in-class
			   member function definitions). */
  eoda_check_semicolon,	/* Check that the last token is a semicolon and
			   consume it. */
  eoda_skip_final_token,
			/* Consume the last token (without checking it). */
  eoda_done		/* The declaration is fully parsed. */
} an_end_of_decl_action;

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void promote_prototype_scope_ss_list(a_func_info_block  *func_info)
/*
Move the source sequence list (if any) that had been entered into the function
prototype scope associated with func_info to the current scope.
*/
{
  if (func_info->prototype_scope_ss_list != NULL) {
    a_source_sequence_entry_ptr  head, tail;
    /* Identify the head and tail of the list that is pointed to from the
       func_info block. */
    head = func_info->prototype_scope_ss_list;
    for (tail = head;; tail = tail->next) {
      if (tail->next == NULL) break;
    }  /* for */
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("declaration: moving ss list from func info to curr scope\n",
            f_debug);
    }  /* if */
#endif /* DEBUG */
    /* Append the list to the list for the current scope. */
    insert_src_seq_list(head, tail, depth_scope_stack,
                        (a_source_sequence_entry_ptr)NULL);
    /* Just to be neat. */
    func_info->prototype_scope_ss_list = NULL;
  }  /* if */
}  /* promote_prototype_scope_ss_list */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/  /* func_info is not used in some configurations. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
static void prep_old_style_param_decl(a_decl_parse_state  *state,
                                      a_func_info_block   *func_info,
                                      a_symbol_locator    *locator)
/*
The current declaration -- described by state, func_info, and locator -- is
(apparently) an old-style C parameter declaration.  Perform various checks
and updates prior to handling it as a variable declaration in the
"variable_declaration()" function (or as a typedef declaration in some error
cases).
*/
{
  a_param_id_ptr  param_id;

#if GENERATE_SOURCE_SEQUENCE_LISTS
  promote_prototype_scope_ss_list(func_info);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (state->declared_storage_class == (a_storage_class)sc_typedef) {
    /* Instead of a parameter declaration, we found a typedef declaration:
       Issue a diagnostic.  By setting param_id to NULL, we ensure that it
       will be treated as a function-scope typedef declaration. */
    if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
      pos_error(ec_decl_should_be_of_param, &state->start_pos);
      set_to_error_locator(*locator);
    } else {
      pos_warning(ec_decl_should_be_of_param, &state->start_pos);
    }  /* if */
    param_id = NULL;
  } else {
    /* Check that the name declared was mentioned in the parameter list. */
    param_id = param_id_on_list(locator, state->param_id_list);
    if (param_id == NULL) {
      /* The identifier was not found on the list.  Issue an error.  Leaving
         param_id set to NULL, ensures the declaration will be treated as a
         function-scope variable declaration for error-recovery purposes. */
      error(ec_decl_should_be_of_param);
    } else if (param_id->type != NULL) {
      /* Parameter has already been declared. */
      str_error(ec_id_already_declared, locator->symbol_header->identifier);
    } else {
      /* When the parameter name was listed (but not yet actually declared) the
         sk_parameter symbol was created but not entered in the symbol table.
         Now that it is explicitly declared, add it to the function prototype
         scope; it will later be moved to the function scope. */
      reenter_symbol(param_id->symbol, decl_scope_level,
                     /*suppress_error=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      param_id->source_sequence_entry = state->source_sequence_entry;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Set the declared_type field in the param_id entry before the type is
         adjusted (e.g., decays from array to pointer). */
      param_id->declared_type = state->declared_type;
    }  /* if */
    /* Check that the type is legal, and do required adjustments. */
    check_and_adjust_parameter_type(state, /*param_num=*/0,
                                    &state->start_pos);
    /* For pcc compatibility, promote float parameters to double. */
    if (C_dialect == C_dialect_pcc) {
      promote_float_to_double(state->type);
    }  /* if */
  }  /* if */
  state->param_id = param_id;
}  /* prep_old_style_param_decl */


static void check_for_definition_in_return_type(a_decl_parse_state  *state)
/*
Defining a type in a function return type is normally not allowed in C++ (the
exception is Microsoft C++ mode).  This is taken to apply to pointer-to-
function type declarations as well to the function declarations.  Issue an
error if an inappropriate definition was encountered.  state describes the
declaration being parsed.
*/
{
  if (C_dialect == C_dialect_cplusplus &&
      !microsoft_mode && (state->dso_flags & DSO_DEFINES_SOMETHING) != 0) {
    a_type_ptr  tp = state->declared_type;
    for (;;) {
      tp = skip_typerefs(tp);
      switch (tp->kind) {
        case tk_routine:
          /* The error case we are checking for: Issue an error. */
          pos_error(ec_type_def_not_allowed_in_func_type_decl,
                    &state->start_pos);
          goto done;
        case tk_pointer:
          /* Get type pointed to and continue. */
          tp = type_pointed_to(tp);
          break;
        case tk_ptr_to_member:
          /* Get member type and continue. */
          tp = pm_member_type(tp);
          break;
        default:
          /* No function type can be involved.  Stop looping. */
          goto done;
      }  /* switch */
    }  /* for */
done:;
  }  /* if */
}  /* check_for_definition_in_return_type */


static void check_missing_type_specifiers_in_decl(
                                               a_decl_parse_state  *state,
                                               a_func_info_block   *func_info,
                                               a_symbol_locator    *locator)
/*
state, func_info, and locator describe a declaration being scanned by the
function "declaration".  This declaration (which is not a function definition)
has no explicit type specifier.  Issue a diagnostic if appropriate.  func_info
is non-NULL only if this is called for a function declaration.
*/
{
  if (!(state->do_flags & (DO_IS_CONSTRUCTOR | DO_IS_DESTRUCTOR |
                           DO_IS_FINALIZER)) &&
      !locator->is_conversion_name &&
      !state->range_based_for &&
#if GNU_EXTENSIONS_ALLOWED
      /* "typedef foo = 3;" is an old GNU C extension, not a use of implicit
         int (this extension is not present in the GNU C++ compiler, nor in
         newer GNU C compilers). */
      !(gcc_mode && gnu_version < 30100 && curr_token == tok_assign && 
        state->storage_class == (a_storage_class)sc_typedef) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
      !locator->is_error) {
    a_boolean  is_main_func = (func_info != NULL &&
                               func_info->is_main_function);
    report_missing_type_specifier(&state->declarator_start_pos, state->type,
                                  (func_info != NULL),
                                  /*is_function_def=*/FALSE, is_main_func,
                                  !state->decl_specifiers_omitted);
  }  /* if */
}  /* check_missing_type_specifiers_in_decl */


static void diagnose_initializer_on_function(a_boolean          paren_form,
                                             a_symbol_ptr       sym,
                                             a_source_position  *init_pos)
/*
Report what seems to be an attempt to specify an initializer (at the given
position) on the declaration of a function described by sym.  If the
initializer started with an assignment token ("="), it is natural to issue
the diagnostic in terms of an invalid initialization.  If a parenthesized
initializer was found paren_form will be TRUE: That is more likely a
consequence of an error in declarator syntax than an actual attempt to
initialize a function.  In such cases, we issue the diagnostic in terms of
the encountered token and do not attempt to fully parse an initializer.
*/
{
  if (paren_form) {
    /* The parenthesis was already consumed during parsing of the declarator,
       but init_pos points to its position. */
    pos_sy_error(ec_lparen_after_function, init_pos, sym);
    flush_to_closing_paren();
    (void)get_token();
  } else {
    pos_sy_error(ec_cannot_initialize, init_pos, sym);
    /* Skip the assignment operator. */
    (void)required_token(tok_assign, ec_exp_assign);
    /* Scan (and discard) the expression that follows. */
    scan_and_discard_init_component((a_decl_parse_state*)NULL);
  }  /* if */
}  /* diagnose_initializer_on_function */


a_boolean deleted_or_defaulted_def_next(a_boolean  *defaulted)
/*
Return TRUE if the next two tokens correspond to an "= delete" or "= default"
function definition (if the associated language feature is enabled).  Set
*defaulted to TRUE in the "= default" case, and to FALSE otherwise.  This
routine also works in Microsoft modes that do not have a keyword "default".
*/
{
  a_boolean  result = FALSE;

  *defaulted = FALSE;
  if (curr_token == tok_assign &&
      (deleted_functions_enabled || defaulted_special_members_enabled)) {
    /* "=" in a mode when "= delete" and/or "= default" is permitted: Examine
       the next token. */
    if (microsoft_mode && microsoft_version >= 1400 &&
        defaulted_special_members_enabled) {
      /* "default" is not a keyword: Use an explicit token cache to examine
         the spelling of the second token if necessary. */
      a_token_cache  cache;
      clear_token_cache(&cache, /*reusable=*/FALSE);
      cache_curr_token(&cache);
      (void)get_token();
      if (deleted_functions_enabled && curr_token == tok_delete) {
        result = TRUE;
      } else if (defaulted_special_members_enabled &&
                 (curr_token == tok_default ||
                  (curr_token == tok_identifier &&
                   check_context_sensitive_keyword(tok_default, "default")))) {
        result = TRUE;
        *defaulted = TRUE;
      }  /* if */
      rescan_cached_tokens(&cache);
    } else {
      /* Both "delete" and "default" are keywords: Just call next_token() to
         examine the token that follows the "=". */
      a_token_kind  next_tok = next_token();
      if (deleted_functions_enabled && next_tok == tok_delete) {
        result = TRUE;
      } else if (defaulted_special_members_enabled &&
                 next_tok == tok_default) {
        result = TRUE;
        *defaulted = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* deleted_or_defaulted_def_next */


a_boolean decltype_auto_tokens_next(void)
/*
Return TRUE if the upcoming tokens in the token stream are "decltype(auto)".
The caller already ensured the current token is "decltype".
*/
{
  a_boolean      result = FALSE;
  a_token_cache  cache;

  check_assertion(curr_token == tok_decltype);
  clear_token_cache(&cache, /*reusable=*/FALSE);
  cache_curr_token(&cache);
  (void)get_token();
  if (curr_token == tok_lparen) {
    cache_curr_token(&cache);
    (void)get_token();
    if (curr_token == tok_auto) {
      cache_curr_token(&cache);
      (void)get_token();
      if (curr_token == tok_rparen) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Restore the token state. */
  rescan_cached_tokens(&cache);
  return result;
}  /* decltype_auto_tokens_next */


static an_end_of_decl_action function_declaration(
                                          a_decl_parse_state  *state,
                                          a_func_info_block   *func_info,
                                          a_symbol_locator    *locator,
                                          a_decl_pos_block    *decl_pos_block,
                                          a_token_kind        *final_token)
/*
Process a function declaration not directly appearing in a class scope (i.e.,
ordinary namespace scope declarations, local variable declarations, block-
extern declarations, and out-of- class definitions of member functions).  The
declaration is described by state, func_info, locator, and decl_pos_block.
*final_token (which should be set to tok_semicolon by the caller) may be set
to tok_rbrace in a function definition is encountered.  This function is
called from "declaration" and its return value indicates how processing should
proceed after the call.
*/
{
  an_end_of_decl_action
                end_of_decl_action = eoda_not_at_end;
  a_symbol_ptr  ext_sym;
  a_type_ptr    type = state->type, prev_type = NULL;
  a_boolean     inline_specified = ((state->dso_flags & DSO_INLINE) != 0);
  a_boolean     out_of_class_redecl = FALSE;
  an_id_linkage_kind
                linkage = idl_none;
  a_boolean     has_initializer = FALSE;

  if (state->routine_fixup != NULL) {
    /* The fixup was created while scanning the parameters and at that time
       the func_info information was incomplete.  Now that it is complete,
       copy that information so that it will be available when scanning
       default arguments later on. */
    copy_func_info_to_fixup(state, func_info);
  }  /* if */
  /* Check for "= default" or "= delete". */
  if (curr_token == tok_assign) {
    a_boolean  defaulted;
    if (deleted_or_defaulted_def_next(&defaulted)) {
      /* "= delete" or "= default". */
      if (defaulted) {
        /* The current token is "default" (keyword or identifier). */
        if (locator->is_class_member) {
          func_info->is_defaulted = TRUE;
        } else {
          /* "= default" on a nonmember function: Issue an error (and ignore
             the tokens). */
          (void)get_token();
          pos_error(ec_invalid_function_to_be_defaulted, &pos_curr_token);
          (void)get_token();
        }  /* if */
      } else {
        func_info->is_deleted = TRUE;
        report_gnu_cpp11_extension_if_needed(
                              &pos_curr_token, ec_deleted_functions_is_cpp11);
      }  /* if */
    } else {
      has_initializer = TRUE;
    }  /* if */
  } else {
    has_initializer = (state->do_flags & DO_PARENTHESIZED_INITIALIZER) != 0;
  }  /* if */
  if (C_mode() && is_function_type(type)) {
    /* Issue a warning on something like "typedef int F(); F const g;" in C
       mode.  (This is undefined behavior according to the C standard.) */
    report_qualifiers_as_useless(&type, &state->declarator_pos);
  }  /* if */
  if (!is_error_locator(*locator) &&
      locator->symbol_header->identifier != NULL &&
      (strcmp(locator->symbol_header->identifier, "main") == 0)) {
    a_boolean  is_main_func = FALSE;
    /* Recognizing a declaration of function "main" is more than checking
       the identifier. */
    if (C_mode()) {
      if (state->declared_storage_class == (a_storage_class)sc_unspecified ||
          state->declared_storage_class == (a_storage_class)sc_extern) {
        /* Not a static function named "main".  This is not an option
           in C++ (ARM 3.4). */
        func_info->is_main_function = is_main_func = TRUE;
      }  /* if */
    } else {
      /* C++ mode. */
      if (locator->is_qualified_name ?
            !locator->is_file_scope_qualified_name :
            depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
        /* A declaration that's a qualified name (except ::main), or one
           that's unqualified but in a namespace scope, can't refer to
           global main. */
      } else {
        check_assertion(locator->specific_symbol == NULL ||
                        (!locator->specific_symbol->is_class_member &&
                         sym_is_namespace_member(locator->specific_symbol)));
        func_info->is_main_function = is_main_func = TRUE;
      }  /* if */
    }  /* if */
    if (is_main_func) {
      check_main_function(func_info, type, state, &inline_specified,
                          &locator->source_position);
    }  /* if */
  }  /* if */
#if ASM_FUNCTION_ALLOWED
  if (state->declared_storage_class == (a_storage_class)sc_asm) {
    func_info->is_asm_function = TRUE;
    /* Issue a diagnostic about using a nonstandard feature. */
    if (strict_ansi_mode) {
      pos_diagnostic(strict_ansi_error_severity, ec_nonstd_asm_function,
                     &state->start_pos);
    }  /* if */
  } else
#endif /* ASM_FUNCTION_ALLOWED */
  /* Do not insert code here. */
  {
    if ((state->storage_class != (a_storage_class)sc_unspecified &&
         state->storage_class != (a_storage_class)sc_extern &&
         state->storage_class != (a_storage_class)sc_static) ||
        state->register_id != 0) {
      /* The storage class of a function must be extern or static. */
      pos_error(ec_bad_function_storage_class,
                &decl_pos_block->storage_class_pos);
      state->storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
    if (locator->specific_symbol != NULL &&
        locator->specific_symbol->is_class_member) {
      /* This is the definition of a static member function.  No storage
         class specifier (not even "static") is permitted. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled &&
          (state->do_flags & DO_IS_STATIC_CONSTRUCTOR) != 0) {
        /* C++/CLI static constructors are an exception. */
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
      if (state->declared_storage_class != (a_storage_class)sc_unspecified) {
        an_error_severity  severity = es_error;
        if (!extern_inline_allowed && inline_specified &&
            state->storage_class == (a_storage_class)sc_static) {
          /* Just give a warning on this.  The storage class designation
             is taken to be redundant, since all "inline" member functions
             (both static and nonstatic, in the sense applied to member
             functions) are "static" (in the sense of having internal
             linkage). */
          severity = es_warning;
        }  /* if */
        pos_diagnostic(severity, ec_storage_class_not_allowed,
                       &decl_pos_block->storage_class_pos);
      }  /* if */
      /* Set the storage class to sc_unspecified for now.  It will be
         checked and reset if necessary in define_member_function. */
      state->storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
  }  /* if */
  func_info->is_inline = inline_specified;
  /* If except for type qualifiers the specifiers type and the declared type
     are both function types, then the function type must have come from a
     typedef.  E.g., "typedef int F(); F g;". */
  func_info->function_type_from_typedef =
        (skip_typerefs(state->type) == skip_typerefs(state->specifiers_type));
  /* If the current token looks like it could be part of a function
     definition, go scan that.  A very special case are Microsoft out-of-class
     member redeclarations (that are not definitions); they are handled by the
     code for out-of-class definitions (even though no actual definition is
     involved).  Early GNU C++ versions have a similar construct for
     specializations. */
  if (locator->is_class_member && curr_token == tok_semicolon) {
    if (microsoft_mode) {
      out_of_class_redecl = TRUE;
    } else if (gpp_mode && gnu_version < 30400) {
      a_type_ptr  pt = qualifier_class_type(*locator);
      if (pt->variant.class_struct_union.is_template_class &&
          !pt->variant.class_struct_union.is_nonreal_class &&
          !pt->variant.class_struct_union.is_specialized) {
        out_of_class_redecl = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (state->function_definition_allowed || out_of_class_redecl) {
    if ((curr_token != tok_semicolon || out_of_class_redecl) &&
        curr_token != tok_comma &&
#if GNU_EXTENSIONS_ALLOWED
        /* asm names are only allowed on function declarations, not on
           function definitions. */
        curr_token != tok_asm &&
#endif /* GNU_EXTENSIONS_ALLOWED */
        curr_token != tok_end_of_source &&
        !has_initializer) {
      a_boolean  is_function_try_block = curr_token == tok_try;
      if ((state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
        /* Function with no explicitly specified return type.  Issue a
           remark (except in pcc mode and except for C++ constructors,
           destructors, finalizers, and conversion operators). */
        if (C_dialect != C_dialect_pcc &&
            !(state->do_flags & (DO_IS_CONSTRUCTOR | DO_IS_STATIC_CONSTRUCTOR |
                                 DO_IS_DESTRUCTOR | DO_IS_FINALIZER)) &&
            !locator->is_conversion_name &&
            !(locator->is_error && looks_like_ctor_or_dtor(locator))) {
          report_missing_type_specifier(&state->declarator_start_pos,
                                        state->type,
                                        /*is_function=*/TRUE,
                                        /*is_function_def=*/TRUE,
                                        func_info->is_main_function,
                                        !state->decl_specifiers_omitted);
        }  /* if */
      }  /* if */
      remove_all_local_stop_tokens(state);
      func_info->is_definition = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      func_info->declarator_ssep = state->source_sequence_entry;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if USER_CONTROL_OF_STRUCT_PACKING
      /* Record the current setting of the maximum alignment for local
         class members (an adjustment may be required for packing). */
      func_info->max_member_alignment =
                         current_max_alignment_for_class_members();
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      if (!C_mode()) {
        /* Issue diagnostic on an incomplete-type in an exception
           specification.  (It wasn't done when the exception
           specification was scanned because definitions and declarations
           are treated differently. */
        report_exception_spec_errors(func_info);
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
      report_gnu_postfix_attributes_on_function_definition(state);
      /* GNU C doesn't allow "void f() asm("bar") {}". */
      if (state->asm_name != NULL) {
        pos_error(ec_asm_name_in_rout_defn, &state->asm_name_pos);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Do processing required for a function definition, including
         scanning the function body.  Note that the closing '}' will not
         been consumed -- that will be done by the caller. */
      function_definition(locator, state, func_info, decl_pos_block);
      done_with_func_info(*func_info);
      if (is_function_try_block) {
        /* Checking for the closing brace will already have been done. */
        end_of_decl_action = eoda_done;
        goto done;
      }  /* if */
      /* Usually, a right brace is expected, but some cases end with a
         semicolon: C++11 deleted and defaulted functions, as well as the
         Microsoft/GNU extension that allows a nondefining out-of-class member
         declaration. */
      if (func_info->is_deleted || func_info->is_defaulted ||
          out_of_class_redecl) {
        *final_token = tok_semicolon;
      } else {
        *final_token = tok_rbrace;
        check_assertion(curr_token == tok_rbrace ||
                        curr_token == tok_end_of_source ||
                        total_errors != 0);
      }  /* if */
      end_of_decl_action = eoda_skip_final_token;
      goto done;
#if ASM_FUNCTION_ALLOWED
    } else if (state->declared_storage_class == (a_storage_class)sc_asm) {
      /* Not a function definition. */
      pos_error(ec_bad_asm_function_def, &pos_curr_token);
      state->storage_class = (a_storage_class)sc_unspecified;
      set_to_named_error_locator(*locator);
#endif /* ASM_FUNCTION_ALLOWED */
    }  /* if */
  }  /* if */
  /* After a declaration has been scanned, it is no longer possible that the
     next thing is a function definition. */
  state->function_definition_allowed = FALSE;
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->is_class_member) {
    /* A qualified name that identifies a function is allowed only when the
       function body is present (or for some special Microsoft and GNU cases
       handled like function definitions). */
    report_member_function_redeclaration(locator, state);
  }  /* if */
  if (!C_mode()) {
    /* Issue diagnostic on an incomplete-type in an exception specification.
       (It wasn't done when the exception specification was scanned because
       definitions and declarations are treated differently. */
    report_exception_spec_errors(func_info);
  }  /* if */
  if ((state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
    check_missing_type_specifiers_in_decl(state, func_info, locator);
  }  /* if */
  if (!func_info->function_type_from_typedef) {
    if (func_info->param_id_list != NULL) {
      /* If the function has a non-empty old-style identifier list of
         parameters, a body should have been present. */
      if (!skip_typerefs(state->type)
                                   ->variant.routine.extra_info->prototyped) {
        if (microsoft_mode && C_mode()) {
          /* No diagnostic in Microsoft C mode. */
        } else {
          diagnostic(gcc_mode ? es_warning : es_error,
                     ec_param_id_list_needs_function_def);
        }  /* if */
      }  /* if */
    }  /* if */
    /* Update xref info on param ids.  Do this even if there are no parameters
       because some source sequence entries might have been created (e.g., for
       pragmas inside the empty parameter list). */
    record_param_id_list_declarations(func_info);
  }  /* if */
  /* A function with block scope (i.e., within an sck_function or sck_block
     scope) can only have an explicit storage class of extern (3.5.1). */
  if ((scope_stack[decl_scope_level].kind == (a_scope_kind)sck_function ||
       scope_stack[decl_scope_level].kind == (a_scope_kind)sck_block) &&
      state->storage_class != (a_storage_class)sc_unspecified &&
      state->storage_class != (a_storage_class)sc_extern) {
    /* Allow "static" in all non-GNU C modes (an extension) except in strict
       mode.  The function will be entered at the file scope as static.  
       GCC 3.3.x and earlier ignore the "static", GCC 3.4.x behaves like our
       default mode, and GCC 4.x disallows "static" in this context.
       Do not allow at all in C++ mode. */
    if (state->storage_class == (a_storage_class)sc_static) {
      an_error_severity  severity;
      if (C_mode()) {
        /* This is an extension to ANSI C so produce a diagnostic in strict
           ANSI C mode. */
        severity = strict_ansi_mode ? strict_ansi_error_severity : es_none;
        if (gcc_mode) {
          if (gnu_version < 30400) {
            state->storage_class = (a_storage_class)sc_unspecified;
          } else if (gnu_version >= 40000) {
            severity = es_discretionary_error;
          }  /* if */
        }  /* if */
      } else { /* C++ mode */
        /* The downstream call to id_linkage doesn't expect block level
           statics in C++ mode. */
        severity = es_error;
        state->storage_class = (a_storage_class)sc_extern;
      }  /* if */
      if (severity != es_none) {
        pos_diagnostic(severity, ec_block_scope_function_must_be_extern,
                       &decl_pos_block->storage_class_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  if (vla_enabled) {
    if (func_info->vla_fixup_list != NULL) {
      /* Throw away VLA info created for the function prototype.  By doing
         this we discard the details of the VLAs' dimension expressions.  The
         type of those VLAs is then "as if" they had been declared with "[*]".
         (Had this been a definition, we would have kept a record of the
         expressions through a call to process_vla_parameters.) */
      check_assertion(C_mode() || total_errors != 0);
      free_vla_fixup_list(func_info->vla_fixup_list);
      func_info->vla_fixup_list = NULL;
    }  /* if */
    if (is_variably_modified_type(state->type)) {
      /* This can only occur when a block-extern function declaration
         has a variably-modified return type. */
      pos_error(ec_variably_modified_type_not_allowed,
                &locator->source_position);
    }  /* if */
  }  /* if */          
  decl_routine(locator, state, func_info, SRK_DECLARATION, &linkage,
               &prev_type, &ext_sym, decl_pos_block);
  record_entity_in_decl_stmt_if_needed(state->sym);
  /* Diagnose attempts to initialize a function entity. */
  if (has_initializer) {
    a_boolean  paren_form =
                        (state->do_flags & DO_PARENTHESIZED_INITIALIZER) != 0;
    a_source_position
               *init_pos = paren_form ? &decl_pos_block->var_init_range.start
                                      : &pos_curr_token;
    diagnose_initializer_on_function(paren_form, state->sym, init_pos);
  }  /* if */
done:
  return end_of_decl_action;
}  /* function_declaration */


void check_nonfunction_declaration_errors(a_decl_parse_state  *state,
                                          a_symbol_locator    *locator)
/*
Check for function declaration features incorrectly used in variable or
typedef declarations.  state and locator describe the declaration.
*/
{
  /* The "inline" specifier should only appear on function declarations. */
  if (state->dso_flags & DSO_INLINE) {
    pos_diagnostic(gcc_mode ? es_warning : es_error, ec_inline_and_nonfunction,
                   &state->inline_pos);
  }  /* if */
  if ((state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
    check_missing_type_specifiers_in_decl(state, (a_func_info_block*)NULL,
                                          locator);
  }  /* if */
}  /* check_nonfunction_declaration_errors */

#if GENERATE_SOURCE_SEQUENCE_LISTS

void add_src_seq_end_of_variable_if_needed(a_decl_parse_state  *dps)
/*
A variable declaration has been processed.  If that declaration triggered the
creation of source sequence entries for constructs it embeds in its declarator
or initializer, insert an end-of-construct source sequence entry.  This allows
source sequence entries associated with a declarator or initializer to be
distinguished from those that follow the initializer.  For example:
    void *p = (union { char c; int x; }*) 0;  // Accepted in g++ mode.
      // The source sequence entries for the anonymous union will be
      // separated from those of S by an end-of-construct entry.
    struct S {} s;
Note that a different mechanism is used if the variable declaration is a
function parameter declaration: This routine does nothing in that case.
*/
{
  a_source_sequence_entry_ptr  ssep = dps->source_sequence_entry;

  check_assertion(dps->sym != NULL);
  if (ssep != NULL && ssep->next != NULL &&
      dps->sym->kind != (a_symbol_kind)sk_parameter) {
    /* The source sequence entry associated with the variable declaration is
       followed by entries for embedded constructs. */
    a_variable_ptr  vp;
    if (dps->sym->kind == (a_symbol_kind)sk_variable) {
      vp = dps->sym->variant.variable.ptr;
    } else if (dps->sym->kind == (a_symbol_kind)sk_static_data_member) {
      vp = dps->sym->variant.static_data_member.variable;
    } else {
      unexpected_condition();
    }  /* if */
    if (ss_entry_kind(ssep) == iek_variable) {
      vp->embedded_source_sequence_entries = TRUE;
    } else {
      check_assertion(ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
      ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr)
                                    ->embedded_source_sequence_entries = TRUE;
    }  /* if */
    add_end_of_construct_source_sequence_entry(
                              (char *)vp, (a_byte_il_entry_kind)iek_variable);
  }  /* if */
}  /* add_src_seq_end_of_variable_if_needed */


void add_src_seq_end_of_routine_if_needed(a_decl_parse_state  *dps)
/*
A function or member function declaration has been processed.  If that
declaration triggered the creation of source sequence entries for constructs
it embeds in its declarator, insert an end-of-construct source sequence entry.
This allows source sequence entries associated with a declarator to be
distinguished from those that follow the declarator.  For example:
    int (*f())[sizeof(struct { int x; })];
And end-of-construct entry is added after the source sequence entries for the
embedded struct declaration.
*/
{
  a_source_sequence_entry_ptr  ssep = dps->source_sequence_entry;

  check_assertion(dps->sym != NULL);
  if (ssep != NULL && ssep->next != NULL &&
      is_simple_function_symbol(dps->sym)) {
    /* The source sequence entry associated with the routine declaration is
       followed by entries for embedded constructs. */
    a_routine_ptr  rp = dps->sym->variant.routine.ptr;
    if (ss_entry_kind(ssep) == iek_routine) {
      rp->embedded_source_sequence_entries = TRUE;
    } else {
      check_assertion(ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
      ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr)
                                    ->embedded_source_sequence_entries = TRUE;
    }  /* if */
    add_end_of_construct_source_sequence_entry(
                               (char *)rp, (a_byte_il_entry_kind)iek_routine);
  }  /* if */
}  /* add_src_seq_end_of_routine */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if USER_CONTROL_OF_STRUCT_PACKING

void record_std_alignment_attr(a_decl_parse_state_ptr  dps)
/*
If applicable, record the explicit alignment specified by standard alignment
attributes on the declaration described by *dps in the corresponding variable
entry.  Issue an error if this alignment is invalid (e.g., inconsistent with
previous declarations).
If no attribute was specified and dps->is_definition is TRUE, issue an error
if prior declarations specified an alignment attribute.
*/
{
  a_variable_ptr  vp = NULL;

  if (dps->sym->kind == (a_symbol_kind)sk_variable) {
    vp = dps->sym->variant.variable.ptr;
  } else if (dps->sym->kind == (a_symbol_kind)sk_static_data_member) {
    vp = dps->sym->variant.static_data_member.variable;
  }  /* if */
  if (dps->alignment != 0) {
    /* At least one standard attribute was specified. */
    an_attribute_ptr  ap = find_decl_attribute(ak_align, dps);
    check_assertion(ap != NULL || vp == NULL);
    /* Check that the specified alignment is consistent with any previously
       specified alignments for the declared variable, and, if so, record that
       alignment in the variable entry. */
    if (vp == NULL) {
      expect_error();
    } else if (alignment_of_type(vp->type) > dps->alignment) {
      /* The alignment (as specified by standard attributes) cannot be weaker
         than the default alignment of the variable's type. */
      pos_error(ec_invalid_alignment_reducing_attr, &ap->position);
    } else if (vp->alignment == 0) {
      /* This is the first time an alignment attribute is explicitly specified
         for this variable.  If a definition appeared previously, this is an
         error. */
      if (!dps->is_definition && dps->sym->defined) {
        pos2_diagnostic(es_error, ec_variable_align_attr_not_on_definition,
                        &ap->position, &dps->sym->decl_position);
      } else {
        vp->alignment = dps->alignment;
      }  /* if */
    } else if (vp->alignment != dps->alignment) {
      /* A previous declaration specified an explicit alignment that is
         inconsistent with the current declaration.  Issue an error. */
      char  orig_align_str[100], new_align_str[100];
      (void)sprintf(orig_align_str, "%d", vp->alignment);
      (void)sprintf(new_align_str, "%d", dps->alignment);
      pos_st2_error(ec_inconsistent_alignment, &ap->group->position,
                    new_align_str, orig_align_str);
    }  /* if */
    /* Clear the alignment so that additional callbacks on this declaration
       will have no effect. */
    dps->alignment = 0;
  } else if (dps->is_definition && vp != NULL && vp->alignment != 0) {
    /* The current declaration is a definition with no explicit alignment
       specified through a standard attribute, but a prior variable
       declaration did specify an explicit alignment.  If that explicit
       alignment was the result of an attribute, issue an error. */
    an_attribute_ptr  ap = find_attribute(ak_align,
                                          vp->source_corresp.attributes);
    if (ap != NULL && is_std_attribute(ap)) {
      pos2_diagnostic(es_error, ec_variable_align_attr_not_on_definition,
                      &ap->position, &dps->declarator_pos);
    }  /* if */
  }  /* if */
}  /* record_std_alignment_attr */

#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if CHECKING

static void check_consistent_init_type(a_variable_ptr  var)
/*
If the given variable it initialized by a constant or an expression, check
that the type of the initializer is consistent with the type of the variable.
*/
{
  if (var != NULL && var->init_kind != (an_init_kind)initk_none &&
      !is_template_dependent_context()) {
    /* Verify that the type of an initializer matches that of the variable
       (only some common initializer kinds are checked). */
    a_type_ptr  init_type = NULL;
    if (var->init_kind == (an_init_kind)initk_static) {
      if (var->initializer.constant->kind == (a_constant_repr_kind)ck_string) {
        /* String literal initializations allow for all kinds of mismatches
           in various modes.  So we don't check those here. */
      } else {
        init_type = var->initializer.constant->type;
      }  /* if */
    } else if (var->init_kind == (an_init_kind)initk_dynamic) {
      a_dynamic_init_ptr  dip = var->initializer.dynamic;
      switch (dip->kind) {
        case dik_constant:
          if (dip->variant.constant->kind == (a_constant_repr_kind)ck_string) {
            /* String literal initializations allow for all kinds of mismatches
               in various modes.  So we don't check those here. */
            break;
          }  /* if */
          /*FALLTHROUGH*/
        case dik_nonconstant_aggregate:
          init_type = dip->variant.constant->type;
          break;
        case dik_expression:
          init_type = dip->variant.expression->type;
          break;
        default:
          /* Other initialization types are not checked. */
          break;
      }  /* switch */
    }  /* if */
    if (init_type != NULL) {
      a_type_ptr  var_type = var->type;
      if (is_any_reference_type(init_type) &&
          is_any_reference_type(var_type)) {
        /* Reference type kinds (lvalue vs. rvalue; tracking vs. standard)
           don't always have to match. */
        init_type = type_pointed_to(init_type);
        var_type = type_pointed_to(var_type);
      }  /* if */
      if (is_array_type(init_type) && is_array_type(var_type)) {
        /* Array initializers can have mismatched lengths. */
        init_type = array_element_type(init_type);
        var_type = array_element_type(var_type);
      }  /* if */
      check_assertion(f_types_are_compatible(
                                    var_type, init_type,
                                    TCF_REDECLARATION |
                                    TCF_IGNORE_TYPE_QUALIFIERS |
                                    TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING));

    }  /* if */
  }  /* if */
}  /* check_consistent_init_type */

#endif /* CHECKING */

static void variable_declaration(a_decl_parse_state  *state,
                                 a_symbol_locator    *locator,
                                 a_decl_pos_block    *decl_pos_block)
/*
Process a variable declaration not directly appearing in a class scope (i.e.,
ordinary namespace scope declarations, block-extern declarations, and out-of-
class definitions of static data members).  The declaration is described by
state, func_info, and locator.  This routine also processes an initializer
if one is present.
*/
{
  a_variable_ptr      var_ptr = NULL;
  a_symbol_ptr        ext_sym;
  a_type_ptr          type = state->type;
  a_boolean           is_static_data_member = FALSE;
  a_boolean           has_initializer = FALSE;
  a_boolean           is_variable_def = FALSE, is_tentative_def = FALSE;
  a_boolean           has_parenthesized_initializer =
                       ((state->do_flags & DO_PARENTHESIZED_INITIALIZER) != 0);
  a_boolean           incomplete_type_error_reported = FALSE;
  an_id_linkage_kind  linkage = idl_none;

  if (is_void_type(type)) {
    /* Issue a warning on something like "extern void const x;": The qualifier
       is useless in such cases. */
    report_qualifiers_as_useless(&type, &state->declarator_pos);
#if UPC_EXTENSIONS_ALLOWED
  } else if (upc_mode && (state->dso_flags & DSO_UPC_SHARED_LAYOUT) &&
      is_pointer_type(type) && is_void_type(state->specifiers_type)) {
  /* A layout qualifier cannot be used to qualify the target type of a
     pointer to shared void. */
    error(ec_bad_upc_shared_void_pointer_layout_qualifier);
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
#if ASM_FUNCTION_ALLOWED
  if (state->declared_storage_class == (a_storage_class)sc_asm) {
    /* This use of "asm" is reserved for function declarations.  Issue an
       error. */
    pos_error(ec_bad_asm_function_def, &state->declarator_pos);
    state->storage_class = (a_storage_class)sc_unspecified;
    set_to_named_error_locator(*locator);
  }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  check_nonfunction_declaration_errors(state, locator);
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->is_class_member) {
    is_static_data_member = TRUE;
  }  /* if */
  /* auto and register may not appear in a file-scope level declaration. */
  if (decl_scope_level == depth_innermost_namespace_scope &&
      (state->storage_class == (a_storage_class)sc_auto ||
       (
#if GNU_EXTENSIONS_ALLOWED
        /* The register keyword is allowed if there is an explicit register
           name for a variable (but that is not possible for static data
           members). */
        (state->asm_name == NULL || is_static_data_member) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
        state->storage_class == (a_storage_class)sc_register))) {
    pos_error(ec_bad_file_scope_storage_class,
              &decl_pos_block->storage_class_pos);
    state->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  if (!is_static_data_member &&
      state->storage_class == (a_storage_class)sc_unspecified) {
    /* For ordinary variables and parameters (but not for static data members
       or variable declarations erroneously using a qualified-id), not
       specifying a storage class implies "auto" storage. */
    if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
        state->param_id != NULL) {
      /* We are inside a function body or this is an old-style parameter
         declaration, so an unspecified storage class means "auto", unless
         thread_local was specified, in which case "static" is implied. */
      if (state->dso_flags & DSO_THREAD_LOCAL) {
        state->storage_class = (a_storage_class)sc_static;
      } else {
        state->storage_class = (a_storage_class)sc_auto;
      }  /* if */
    } else if (state->is_linkage_spec_decl) {
      /* This must be part of an linkage specification declaration.  An
         "extern" storage class is implied (ARM 7.4, comment on p. 118). */
      state->storage_class = (a_storage_class)sc_extern;
    }  /* if */
  }  /* if */
  if (state->range_based_for) {
    /* The for-init declaration of a range-based "for" loop: Stop here and let
       statement processing handle the colon and what follows. */
    if (curr_token != tok_colon) {
      /* The current token is not a colon, but since disambiguation caused
         state->range_base_for to be set, we know a colon is upcoming. */
      push_stop_token_stack();
      add_stop_token(tok_colon);
      error_position = pos_curr_token;
      syntax_error(ec_exp_colon);
      remove_stop_token(tok_colon);
      pop_stop_token_stack();
    }  /* if */
  } else if (has_parenthesized_initializer) {
    has_initializer = TRUE;
  } else if (curr_token == tok_assign) {
    has_initializer = TRUE;
    decl_pos_block->var_init_range.start = pos_curr_token;
  } else if (!C_mode() && curr_token == tok_lbrace) {
    /* A C++11-style "direct" braced initializer.  It may not be valid in the
       current C++ mode, but the diagnostic will be issued when processing the
       initializer. */
    has_initializer = TRUE;
    decl_pos_block->var_init_range.start = pos_curr_token;
#if C_ANACHRONISMS_ALLOWED
  } else if (C_dialect == C_dialect_pcc && is_initializer_start()) {
    /* In pcc mode, the "=" may be omitted (K&R first edition, Appendix A,
       section 17 (Anachronisms)). */
    has_initializer = TRUE;
    warning(ec_old_fashioned_initializer);
#endif /* C_ANACHRONISMS_ALLOWED */
  }  /* if */
  if (has_initializer) state->has_initializer = TRUE;
  if (state->param_id != NULL) {
    a_param_id_ptr  param_id = state->param_id;
    state->sym = param_id->symbol;
    copy_source_position(locator->source_position, state->sym->decl_position);
    param_id->type = state->type;
    copy_source_position(state->start_pos, param_id->type_pos);
    param_id->storage_class = state->storage_class;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Update extra source position information in the param-id entry so
       that it can be transferred to the variable entry later. */
    param_id->specifiers_range = decl_pos_block->specifiers_range;
    param_id->declarator_range = decl_pos_block->declarator_range;
    param_id->identifier_range = decl_pos_block->identifier_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Note that the creation of the parameter variable, etc., is done
       in decl_parameter, called when the function body is scanned. */
  } else if (is_static_data_member) {
    /* A static data member definition. */
    define_static_data_member(locator, state, has_initializer, &linkage,
                              decl_pos_block);
    var_ptr = state->sym->variant.static_data_member.variable;
    /* Fetch the type of the symbol again, since it might have been
       changed when reconciled with the original declaration. */
    state->type = var_ptr->type;
    /* All static data member declarations that pass through this
       code are definitions. */
    is_variable_def = TRUE;
    /* Copy the decl-modifiers into the variable entry. */
    update_variable_decl_modifiers(state);
  } else {
    /* An ordinary variable declaration. */
    a_symbol_reference_kind  srk_flags = SRK_DECLARATION;
    if (vla_enabled && !state->function_definition_allowed) {
      /* Local declaration. */
      if (state->storage_class == (a_storage_class)sc_extern ||
          state->storage_class == (a_storage_class)sc_static) {
        if (is_vla_type(state->type)) {
          /* An object with static storage duration cannot be a VLA. */
          pos_error(ec_vla_is_not_auto, &locator->source_position);
        } else if (state->storage_class == (a_storage_class)sc_extern &&
                   is_variably_modified_type(state->type)) {
          /* An entity with linkage cannot have a variably modified
             type. */
          pos_error(ec_variably_modified_type_not_allowed,
                    &locator->source_position);
        }  /* if */
      }  /* if */
    }  /* if */
    /* Set a flag marking this as a defining declaration, if that's
       appropriate. */
    if (state->is_old_style_param_decl) {
      /* This flag is TRUE when state->param_id is NULL in the error case
         where a name appears in an old-style param declaration but for which
         no corresponding param-id was created.  For example:
           void f(i,j) int i, j, k; { }      // Error on "k"
         Treat this as a definition. */
      is_variable_def = TRUE;
    } else if (has_initializer) {
      /* A variable declaration involving an initializer is a definition. */
      is_variable_def = TRUE;
      srk_flags |= SRK_INITIALIZATION;
      if (decl_scope_level != depth_innermost_namespace_scope &&
          state->storage_class == (a_storage_class)sc_extern) {
        /* A block-extern declaration with an initializer is invalid.  For
           recovery purposes, treat the variable as a local static declaration
           instead (this avoids having the initializer associated with another
           declaration of the same variable but with a mismatched type). */
        pos_error(state->register_id == 0 ?
                              ec_block_extern_initializer_not_allowed :
                              ec_local_named_register_initializer_not_allowed,
                    &pos_curr_token);
        state->storage_class = (a_storage_class)sc_static;
        state->type = error_type();
      }  /* if */
    } else if (C_dialect == C_dialect_cplusplus) {
      /* Variable declaration in C++ mode with no explicit initializer. */
      if (microsoft_mode &&
          state->storage_class == (a_storage_class)sc_unspecified &&
          is_incomplete_array_type(state->type) &&
          !is_const_qualified_type(state->type)) {
        /* In Microsoft C++ mode, a non-const variable at file scope
           that is a zero-length array is treated like a C-mode tentative
           definition. */
        is_tentative_def = TRUE;
        srk_flags |= SRK_TENTATIVE_DEF;
      } else if (state->range_based_for) {
        /* The variable declaration in a for-init declaration for a range-based
           "for" loop is always a definition and it implies initialization. */
        is_variable_def = TRUE;
        srk_flags |= SRK_INITIALIZATION;
      } else if (state->storage_class != (a_storage_class)sc_extern) {
        /* In C++ all other variable declarations are definitions, except
           those with a storage class of extern. */
        is_variable_def = TRUE;
        /* Even without an explicit initializer this is an initializing
           declaration if the variable is nontrivially constructible
           -- i.e., if it is a class object (or array of class) and the
           class has a nontrivial default constructor (which must be a
           user-declared default constructor if the variable's type is
           const qualified). */
        if (is_const_qualified_type(state->type) ?
              type_has_user_provided_default_constructor(state->type) :
              type_has_nontrivial_default_constructor(state->type)) {
          srk_flags |= SRK_INITIALIZATION;
        }  /* if */
      }  /* if */
    } else {
      /* C mode. */
      if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
        if (state->storage_class == (a_storage_class)sc_unspecified ||
#if NAMED_REGISTERS_ALLOWED
            (state->storage_class == (a_storage_class)sc_extern &&
             state->register_id != 0) ||
#endif /* NAMED_REGISTERS_ALLOWED */
            state->storage_class == (a_storage_class)sc_static) {
          /* In C a file scope variable declaration with no storage class
             or static storage class is called a tentative definition.
             Variables declared in file scope with a named-register storage
             class specifier (an Embedded C extension) are also treated as
             tentative definitions. */
          is_tentative_def = TRUE;
          srk_flags |= SRK_TENTATIVE_DEF | SRK_DEFINITION;
        }  /* if */
      } else {
        /* In C all local variable declarations are definitions. */
        if (state->storage_class != (a_storage_class)sc_extern) {
          is_variable_def = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (is_variable_def) srk_flags |= SRK_DEFINITION;
    decl_variable(locator, state, srk_flags, &linkage, &ext_sym,
                  decl_pos_block);
    var_ptr = state->sym->variant.variable.ptr;
    record_entity_in_decl_stmt_if_needed(state->sym);
    /* Fetch the type of the symbol again, since it might have been
       changed when reconciled with the original declaration. */
    state->type = var_ptr->type;
    state->storage_class = var_ptr->storage_class;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (is_variable_def) {
      /* The "declared_storage_class" field is updated only for variable
         definitions. */
      var_ptr->declared_storage_class = state->declared_storage_class;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (state->is_old_style_param_decl) {
      /* Error case (described above).  Mark the symbol referenced, to
         suppress subsequent "declared and not referenced" warnings. */
      mark_symbol_to_suppress_warnings(state->sym);
    }  /* if */
  }  /* if */
  if (var_ptr != NULL) {
    if (state->decltype_auto_specifier_seen) {
      var_ptr->declared_with_decltype_auto = TRUE;
    } else if (state->auto_type_specifier_seen) {
      var_ptr->declared_with_auto_type_specifier = TRUE;
    }  /* if */
  }  /* if */
  if (is_variable_def || is_tentative_def) {
    /* In C++ mode, check whether a template class type needs to be
       instantiated.  If appropriate, record that a complete type is required
       in this context (both C and C++).  This test may already have been done
       for certain variable declarations. */
    complete_type_is_needed(state->type);
  }  /* if */
  if (!C_mode() && var_ptr != NULL) {
    if (is_abstract_class_type(state->type)) {
      /* Abstract class objects are prohibited. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled && is_cli_interface_type(var_ptr->type)) {
        /* Diagnose the C++/CLI interface case separately. */
        pos_error(ec_variable_with_interface_type, &locator->source_position);
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        abstract_class_diagnostic(es_error,
                                  ec_abstract_class_object_not_allowed,
                                  state->type, &locator->source_position);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cppcli_enabled &&
               var_has_static_or_thread_storage_duration(var_ptr)) {
      /* Variables with static storage duration cannot have a C++/CLI type
         that may require tracking.  (An exception are static data members of
         managed class types, but those are not handled here since they must
         be defined inside a class.) */
      if (is_ref_class_type(var_ptr->type)) {
        pos_error(ec_static_storage_variable_with_ref_class_type,
                  &locator->source_position);
      } else if (is_handle_or_tracking_ref_type(var_ptr->type)) {
        pos_error(ec_static_storage_variable_with_handle_or_tracking_ref_type,
                  &locator->source_position);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  /* Set the error position to the start of the initializer (that is, to
     the "=" if there is one) or to where the initializer should be in
     case there ought to be one. */
  set_err_pos_to_curr_token();
  if (has_initializer) {
    /* In some Microsoft and GNU modes, the name of the variable being
       initialized is not visible while parsing a parenthesized initializer.
       (Unless it had been previously declared, which is the case for static
       data members or when state->prev_type is set).  To emulate this, we
       temporarily mark the associated symbol as invisible.*/
    a_boolean  decl_invisible_to_initializer =
                  has_parenthesized_initializer && !is_static_data_member &&
                  (microsoft_bugs || (gpp_mode && gnu_version < 30400)) &&
                  state->prev_type == NULL;
    if (decl_invisible_to_initializer && !state->sym->is_error) {
      state->sym->is_invisible = TRUE;
    }  /* if */
    if (!has_parenthesized_initializer && curr_token == tok_assign) {
      /* Advance past the "=". */
      (void)get_token();
    } else {
      state->has_direct_initializer = TRUE;
    }  /* if */
    if (state->sym->kind == (a_symbol_kind)sk_variable &&
        !state->is_old_style_param_decl) {
      /* Set the storage class of a file-scope initialized variable to
         unspecified (meaning external) or static (meaning internal). */
      if (decl_scope_level == depth_innermost_namespace_scope) {
        check_assertion(var_ptr != NULL);
        if (var_ptr->storage_class == (a_storage_class)sc_extern) {
          var_ptr->storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Now scan the initializer. */
    /* If the symbol is a parameter, the subroutine will generate the error.
       This is done rather than flagging the error here because the subroutine
       can scan over the initializer expression neatly. */
    initializer(state, &locator->source_position, linkage,
                has_parenthesized_initializer, &incomplete_type_error_reported,
                decl_pos_block);
    if (decl_invisible_to_initializer && !state->sym->is_error) {
      /* Mark the symbol as visible now that the initializer is complete. */
      state->sym->is_invisible = FALSE;
    }  /* if */
    if (var_ptr != NULL) {
#if GNU_EXTENSIONS_ALLOWED
      if (gpp_mode && has_parenthesized_initializer &&
          curr_token == tok_attribute) {
        gnu_attributes_after_parenthesized_initializer(var_ptr, state);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (state->sym->kind == (a_symbol_kind)sk_variable) {
        /* All initialized variables are considered defined.  This flag may
           have already been set based on storage class and scope level.  Be
           sure to check this after the initializer is scanned, so that
           "int x = x;" can be caught. */
        mark_variable_value_set(state->sym);
      }  /* if */
      if (!is_error_type(state->type)) {
        /* Fetch the type of the symbol again, since it might have been changed
           if it was an incomplete array and was initialized. */
        state->type = var_ptr->type;
      }  /* if */
    }  /* if */
  } else if (state->is_old_style_param_decl) {
    /* Don't worry about a missing initializer. */
  } else if (state->range_based_for) {
    /* Initialization is handled by statement processing. */
  } else if (is_variable_def && !is_error_locator(*locator) &&
             var_ptr->init_kind == (an_init_kind)initk_none) {
    /* An uninitialized variable or static data member is being defined, but
       no explicit initializer was provided.  Do default initialization if
       appropriate (e.g., if a default constructor exists).  In g++ mode, the
       variable being initialized should not be visible during the generation
       of the default initializer.  In particular, the variable should not be
       visible to any instantiations that might result from the processing of
       the initializer. */
    a_boolean	def_init_okay, sym_invisible = state->sym->is_invisible;
    if (gpp_mode) state->sym->is_invisible = TRUE;
    def_init_okay = def_initializer(state->sym, &locator->source_position);
    if (gpp_mode) state->sym->is_invisible = sym_invisible;
    if (def_init_okay) {
      /* Default initialization was successful. */
      check_constant_valued_variable(state);
      if (state->sym->kind == (a_symbol_kind)sk_variable) {
        /* Unless this variable has non-static storage duration and is
           default-initialized by a trivial default constructor (which is a
           no-op), mark it as having a value. */
        if (var_has_static_or_thread_storage_duration(var_ptr)) {
          /* Objects with static storage duration are zero-initialized, so
             they always have some value. */
          mark_variable_value_set(state->sym);
        } else {
          a_type_ptr  tp = skip_typerefs(var_ptr->type);
          if (is_array_type(tp)) {
            tp = underlying_array_element_type(tp);
            tp = skip_typerefs(tp);
          }  /* if */
          if (is_immediate_class_type(tp) &&
              symbol_supplement_for_class(tp)->
                            trivial_default_constructor != NULL) {
            /* Must have been initialized by a trivial default constructor.
               Since such constructors would do nothing even if they were
               actually called, don't regard them as setting the value of
               the variable. */
          } else {
            mark_variable_value_set(state->sym);
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (symbol_is(state->sym, sk_variable) ||
               symbol_is(state->sym, sk_static_data_member)) {
      /* No default initialization, so do some additional checking. */
      a_boolean  explicitly_internal = symbol_is(state->sym, sk_variable) &&
                                       state->declared_storage_class ==
                                                   (a_storage_class)sc_static;
      check_for_missing_initializer_full(state->sym, state->type,
                                         explicitly_internal,
                                         (a_boolean*)NULL);
      if (symbol_is(state->sym, sk_variable)) {
        if (!var_ptr->source_corresp.is_local_to_function ||
            var_ptr->storage_class == (a_storage_class)sc_static ||
            is_nullptr_type(state->type)) {
          /* If a variable is not automatic it presumably has an initial value
             (e.g., it may be zeroed through static initialization).  Nullptr_t
             variables can only have one value and should therefore always be
             treated as having a value. */
          mark_variable_value_set(state->sym);
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (cli_or_cx_enabled && is_handle_type(state->type)) {
          /* Handles are initialized to null by the CLI virtual machine. */
          mark_variable_value_set(state->sym);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (state->sym->kind == (a_symbol_kind)sk_variable &&
             (state->storage_class == (a_storage_class)sc_extern ||
              is_tentative_def)) {
    /* Either:  This is not a definition of a variable but rather an extern
       declaration.  Such a variable may be assumed to be initialized at the
       point of definition, so flag it as "set" (even if it is not actually
       set at the current declaration). */
    /* Or else:  This is a tentative definition, which should be treated as
       though it were a definition. */
    mark_variable_value_set(state->sym);
  }  /* if */
  if (var_ptr != NULL) {
#if DECL_MODIFIERS_IN_USE
    check_variable_decl_modifiers(var_ptr, state);
#endif /* DECL_MODIFIERS_IN_USE */
#if USER_CONTROL_OF_STRUCT_PACKING
    record_std_alignment_attr(state);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
    if (is_variable_def && is_static_data_member) {
      fprintf(f_debug, "decl-pos info for static data member def\n");
      db_decl_pos_info(state->sym);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  copy_source_position(locator->source_position, error_position);
  if (var_ptr != NULL && !is_error_locator(*locator) &&
      is_incomplete_type(var_ptr->type)) {
    /* Issue an error on a variable for which this is the defining declaration
       but whose type is incomplete.  Also, in C mode, issue an error on a
       static variable with incomplete type (6.7.2 para 3) or an externally
       linked variable with a tentative definition but an uncompletable type
       (a case like "void i;" at file scope).  And in C++ mode, since no
       object may be of void type, issue the error for cases like
       "extern void i;" even though it is not a defining declaration. */
    if (is_variable_def ||
        (!C_mode() && is_void_type(state->type)) ||
        (is_tentative_def && is_void_type(state->type))) {
      if (!incomplete_type_error_reported) {
        pos_error(incomplete_type_err_code(var_ptr->type),
                  &locator->source_position);
      }  /* if */
      var_ptr->type = error_type();
    } else if (strict_ansi_mode && is_tentative_def && 
               state->storage_class == (a_storage_class)sc_static) {
      /* The C standard prohibits tentative declarations with incomplete type
         and internal linkage in 6.7.2 para 3, but a reading of 6.1.2.5 may
         lead to the conclusion that the prohibition does not exist: issue a
         discretionary error instead of a "hard" error. */
      if (!incomplete_type_error_reported) {
        pos_diagnostic(strict_ansi_discretionary_severity,
                       ec_incomplete_type_not_allowed,
                       &locator->source_position);
      }  /* if */
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  add_src_seq_end_of_variable_if_needed(state);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  check_use_of_auto_type(state);
#if CHECKING
  check_consistent_init_type(var_ptr);
#endif /* CHECKING */
#if MODULE_ID_NEEDED
  if (var_ptr != NULL && is_variable_def) {
    /* See if the variable that is being defined can be used as the basis
       for a unique module id for the current module. */
    use_variable_or_routine_for_module_id_if_needed(&var_ptr->source_corresp,
                                                    iek_variable);
  }  /* if */
#endif /* MODULE_ID_NEEDED */
}  /* variable_declaration */


static void typedef_declaration(a_decl_parse_state  *state,
                                a_symbol_locator    *locator,
                                a_decl_pos_block    *decl_pos_block)
/*
Process a typedef declaration in function/block scope, namespace scope, or
file scope.  The declaration is described by state, locator, and
decl_pos_block.
*/
{
  if (state->do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
    /* This looked like a cfront-style member function typedef.  Be sure
       the type was a function type. */
    if (is_function_type(state->type)) {
      /* Issue a warning on the extension. */
      pos_warning(ec_ptr_to_member_typedef, &locator->source_position);
    } else {
      /* No function type, so what looked like a qualified name really
         was -- but they aren't allowed. */
      pos_error(ec_qualified_name_not_allowed, &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
  check_nonfunction_declaration_errors(state, locator);
  decl_typedef(locator, state, (a_type_ptr)NULL, decl_pos_block);
  record_entity_in_decl_stmt_if_needed(state->sym);
#if GNU_EXTENSIONS_ALLOWED
  if (curr_token == tok_assign && gcc_mode && gnu_version < 30100 &&
      (state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
    /* In early versions of GNU C (but not in GNU C++) a typedef can be
       defined with
           typedef <type_name> = <expr> ;
       where the type of the given expression becomes the type of the given
       type name.  (GNU C 3.1 and GNU C 3.2 crash on such constructs and later
       versions report a normal error: We therefore only emulate this feature
       when gnu_version < 30100.) */
    set_err_pos_to_curr_token();
    (void)get_token();
    typedef_initializer(state->sym);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_pos_block->var_init_range.end = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* typedef_declaration */


static an_end_of_decl_action
              check_special_declaration_form(a_decl_parse_state  *state,
                                             a_token_kind        *final_token)
/*
A helper routine for "declaration(...)" (see below) that handles various forms
of declarations (like templates, namespaces, etc.) that do not start with a
decl-specifier or a declarator.  The declaration is described by state.
*final_token (which should be set to tok_semicolon by the caller) may be set
to a different token if a form not ending with a semicolon is processed.  This
function is called from "declaration" and its return value indicates how
processing should proceed after the call.
*/
{
  an_end_of_decl_action  end_of_decl_action = eoda_not_at_end;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_attribute_tokens_next()) {
    /* A Microsoft attribute of the form "[ ... ]". */
    state->ms_attributes =
                        scan_microsoft_attributes(/*is_param_or_base=*/FALSE);
    if (curr_token == tok_semicolon) {
      /* This is a standalone attribute block.  Make sure all of the specified
         attributes are standalone attributes.  This also sets ms_attributes
         to NULL. */
      verify_standalone_attributes(&state->ms_attributes);
      cannot_bind_to_curr_construct();
      end_of_decl_action = eoda_skip_final_token;
      goto done;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (curr_token == tok_static_assert) {
    static_assert_declaration(/*leave_semicolon=*/TRUE);
    state->decl_okay_in_constexpr_body = TRUE;
    end_of_decl_action = eoda_check_semicolon;
  } else if (C_dialect == C_dialect_cplusplus) {
    a_boolean	is_generic = FALSE;
    if (curr_token == tok_extern && next_token() == tok_string_literal) {
      /* This looks like a C++ linkage specification, which is "extern"
         followed by a string literal (e.g., "C++" or "C"). */
      if (state->prefix_attributes != NULL) {
        /* Attributes can normally not precede a linkage specification, but
           Microsoft compilers appear to just ignore them instead of issuing
           an error. */
        if (microsoft_mode) {
          pos_warning(ec_attributes_ignored,
                      &state->prefix_attributes->group->position);
        } else {
          pos_error(ec_invalid_attribute_location,
                    &state->prefix_attributes->group->position);
        }  /* if */
        state->prefix_attributes = NULL;
      }  /* if */
      linkage_specification(state);
      end_of_decl_action = eoda_done;
    } else if (curr_token == tok_template ||
               curr_token == tok_export ||
               (((extern_template_allowed && curr_token == tok_extern) ||
                 (inline_template_allowed && curr_token == tok_inline)) &&
                next_token() == tok_template) ||
                (cli_or_cx_enabled &&
                 (is_generic = is_start_of_generic_decl()) != FALSE)) {
      /* Do the processing required for a template declaration.  If this is
         a top level declaration, the subroutine should not advance past the
         final token of the declaration. */
      a_template_decl_options_set  td_flags = TDO_NO_OPTIONS;
      a_source_position	           directive_start_pos = pos_curr_token;
      /* Attributes cannot precede the "template" keyword. */
      disallow_attributes(&state->prefix_attributes);
      if (curr_token == tok_extern) {
        /* In some modes "extern template ..." is permitted. */
        (void)get_token();
        td_flags = TDO_EXTERN;
      } else if (curr_token == tok_inline) {
        /* In some modes "inline template ..." is permitted. */
        (void)get_token();
        td_flags = TDO_INLINE;
      } else if (is_generic) {
        /* A C++/CLI generic declaration. */
        td_flags |= TDO_GENERIC;
      }  /* if */
      template_directive_or_declaration(final_token, td_flags,
                                        &directive_start_pos);
      /* The terminating token will be either a semicolon or a right
         brace.  The latter has already been checked for, but the former
         has not. */
      if (*final_token == tok_semicolon) {
        (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
      }  /* if */
      /* Swallow the current token if it is the same as *final_token, then
         return. */
      end_of_decl_action = eoda_skip_final_token;
    } else if (curr_token == tok_namespace ||
               (inline_namespaces_enabled && curr_token == tok_inline &&
                next_token() == tok_namespace)) {
      /* "namespace" or "inline namespace".  Attributes cannot begin
          a "namespace" declaration. */
      disallow_attributes(&state->prefix_attributes);
      /* Process a namespace definition or a namespace alias declaration. */
      namespace_declaration(final_token);
      if (gpp_mode) {
        /* The C++11 standard doesn't allow namespace alias declarations in
           constexpr function definition, but GCC does. */
        state->decl_okay_in_constexpr_body = TRUE;
      }  /* if */
      /* Swallow the current token if it is the same as *final_token, then
         return. */
      end_of_decl_action = eoda_skip_final_token;
    } else if (curr_token == tok_using) {
      /* An alias-declaration ("using <identifier> = ... ", C++11 only), a
         using-directive (which has the form "using namespace N;"), or a
         using-declaration ("using N::x;" or "using ::x;"). */
      a_source_position  using_pos, end_of_using_pos;
      using_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_of_using_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Skip over the "using" token. */
      (void)get_token();
      /* Attributes cannot precede a using-declaration or using-directive. */
      disallow_attributes(&state->prefix_attributes);
      if (curr_token == tok_namespace) {
        using_directive(state, &using_pos);
        state->decl_okay_in_constexpr_body = TRUE;
      } else {
        a_token_kind  next_tok;
        if (alias_declarations_enabled &&
            is_generalized_identifier_start(GID_NO_OPTIONS) &&
            ((next_tok = next_token()) == tok_assign ||
             (std_attributes_enabled && next_tok == tok_lbracket))) {
          /* An identifier followed by a "=" or a bracket (presumably the
             start of C++11-style attributes): This looks like an alias
             declaration. */
          alias_declaration(state, &end_of_using_pos);
        } else {
          nonmember_using_declaration(state);
          state->decl_okay_in_constexpr_body = TRUE;
        }  /* if */
      }  /* if */
      cannot_bind_to_curr_construct();
      end_of_decl_action = eoda_check_semicolon;
    } else if (cpp11_mode && curr_token == tok_semicolon) {
      /* C++11 allows empty declarations. */
      cannot_bind_to_curr_construct();
      attach_decl_attributes(state, /*primary_decl=*/FALSE);
      end_of_decl_action = eoda_check_semicolon;
    } else if (check_for_overload_anachronism()) {
      /* We check for and discard declarations of the form "overload f;" --
         issue diagnostics on pragmas that are trying to bind to an overload
         declaration. */
      cannot_bind_to_curr_construct();
      end_of_decl_action = eoda_check_semicolon;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled &&
               is_file_or_namespace_scope(&scope_stack_top()) &&
               check_for_cli_delegate_definition()) {
      /* Scan a C++/CLI delegate definition. */
      scan_and_record_cli_delegate_definition(state);
      cannot_bind_to_curr_construct();
      end_of_decl_action = eoda_check_semicolon;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  if (end_of_decl_action != eoda_not_at_end) {
    /* Nothing more to do in this routine. */
  } else if (curr_token == tok_asm || curr_token == tok_microsoft_asm) {
    /* Check whether this is an asm declaration or an asm function.  The
       latter is only possible in contexts that permit function definitions. */
    a_boolean  is_asm_decl = curr_token == tok_microsoft_asm ||
                             !state->function_definition_allowed;
    if (!is_asm_decl) {
      a_token_cache  cache;
      clear_token_cache(&cache, /*reusable=*/FALSE);
      cache_curr_token(&cache);
      (void)get_token();
      if (curr_token == tok_lparen) {
        is_asm_decl = TRUE;
      } else if (gnu_mode && curr_token == tok_volatile) {
        /* GNU compilers permit "asm volatile (...)". */
        cache_curr_token(&cache);
        (void)get_token();
        if (curr_token == tok_lparen) is_asm_decl = TRUE;
      }  /* if */
      rescan_cached_tokens(&cache);
    }  /* if */
    if (is_asm_decl) {
      /* Scan the asm declaration. */
      add_stop_token(tok_semicolon);
      (void)asm_declaration(!state->is_old_style_param_decl,
                            /*is_asm_statement=*/FALSE,
                            &state->prefix_attributes);
      remove_stop_token(tok_semicolon);
      end_of_decl_action = eoda_done;
    } else if (state->function_definition_allowed) {
      /* Not "asm (...)" or "asm volatile (...)", so assume we have an asm
         function declaration -- something like "asm void f(void) { ... }". */
      state->is_asm_function = TRUE;
    }  /* if */
  } else if (!is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
    /* Consider potential error cases. */
    if ((state->function_definition_allowed || state->range_based_for) &&
        is_declarator_start()) {
      /* At file or namespace scope, a declarator with no decl-specifiers,
         apparently.  In some C++ modes, this could be the start of a
         "range-based" for declaration (e.g., the x in "for (x: v) ...").
         In C mode this could be a legal function definition.  Otherwise,
         in C++ mode a diagnostic will be issued (usually just a warning). */
    } else {
      /* Look for some cases that are obviously not the start of a declaration,
         and give a more specific "Expected a declaration" message. */
      if (curr_token == tok_semicolon) {
        if (state->is_linkage_spec_decl) {
          /* Something like: ``extern "C";'' -- Issue an error. */
          diagnostic(es_discretionary_error, ec_exp_declaration);
        } else if (strict_ansi_mode || state->prefix_attributes != NULL) {
          /* An empty declaration is ignored (as an extension in ANSI mode). */
          diagnostic(strict_ansi_discretionary_severity,
                     (state->prefix_attributes != NULL) ? ec_exp_declaration
                                                        : ec_extra_semicolon);
        } else {
          remark(ec_extra_semicolon);
        }  /* if */
        cannot_bind_to_curr_construct();
      } else if (curr_token == tok_lbrace) {
        /* Special error recovery on encountering an open brace: it
           may be the start of a routine. */
        add_stop_token(tok_semicolon);
        error(ec_exp_declaration);
        flush_until_matching_token();
        remove_stop_token(tok_semicolon);
        if (curr_token == tok_rbrace) (void)get_token();
        if (is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
          goto done;
        }  /* if */
      } else {
        add_stop_token(tok_semicolon);
        syntax_error(ec_exp_declaration);
        discard_curr_construct_pragmas();
        remove_stop_token(tok_semicolon);
      }  /* if */
      /* Give up on scanning a declaration (assume we're at the end of one). */
      end_of_decl_action = eoda_skip_final_token;
    }  /* if */
  }  /* if */
done:
  return end_of_decl_action;
}  /* check_special_declaration_form */


static a_decl_flag_set get_decl_specifiers_flags(a_decl_parse_state  *state)
/*
Return the appropriate "input_flags" value for a call to "decl_specifiers"
based on the current mode and the given declaration parsing state.
*/
{
  a_decl_flag_set  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
                               DSI_REGISTER_ID_ALLOWED;

  if (!state->is_asm_function) {
    dsi_flags |= DSI_STORAGE_CLASS_SPECIFIER_ALLOWED;
    dsi_flags |= DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
    /* Within a non-block linkage specification no storage class (except
       typedef?) is allowed (inferred from ARM 7.4). */
    if (state->is_linkage_spec_decl) {
      dsi_flags |= DSI_IS_LINKAGE_SPEC_DECL;
    }  /* if */
  }  /* if */
  if (state->is_old_style_param_decl) {
    dsi_flags |= DSI_IS_PARAMETER;
    dsi_flags |= DSI_IS_OLD_STYLE_PARAM_DECL;
  } else {
    /* A "vacuous declaration" of a class, struct, or union is allowed, but
       only has an effect when not at file scope. */
    dsi_flags |= DSI_VACUOUS_TAG_DECL_ALLOWED;
    /* In C++, "inline" is normally allowed only on function declarations in
       nonlocal scopes.  In C99 and GNU C modes, it is allowed on all function
       declarations.  We also accept the inline specifier on block-extern
       function declarations in Microsoft bugs mode. */
    if (state->function_definition_allowed) {
      dsi_flags |= DSI_EMPTY_DECL_SPECIFIERS_ALLOWED;
      dsi_flags |= DSI_INLINE_ALLOWED;
#if ASM_FUNCTION_ALLOWED
      /* "asm" is allowed only on function definitions at file scope. */
      dsi_flags |= DSI_ASM_ALLOWED;
#endif /* ASM_FUNCTION_ALLOWED */
    } else if (c99_mode || microsoft_bugs) {
      dsi_flags |= DSI_INLINE_ALLOWED;
    }  /* if */
  }  /* if */
  if (std_attributes_enabled) dsi_flags |= DSI_STD_ATTRIBUTES_ALLOWED;
  if (gnu_attributes_enabled) dsi_flags |= DSI_GNU_ATTRIBUTES_ALLOWED;
  if (gnu_mode) {
    if (state->marked_as_gnu_extension) {
      dsi_flags |= DSI_MARKED_AS_GNU_EXTENSION;
    }  /* if */
  }  /* if */
  if (microsoft_mode && microsoft_version >= 1700) {
    /* Newer versions of the Microsoft compiler appear to allow COM-style
       attributes among decl-specifier tokens. */
    dsi_flags |= DSI_MICROSOFT_ATTRIBUTES_ALLOWED;
  }  /* if */
  return dsi_flags;
}  /* get_decl_specifiers_flags */


static an_end_of_decl_action prep_for_declarator(
                                    a_decl_parse_state  *state,
                                    a_decl_flag_set     *p_di_flags)
/*
A helper routine for "declaration" (see below) that determines whether to scan
declarators.  If they are to be scanned, set up the initial value for the
input flags (pointed to by p_di_flags) in the call to "declarator".  state
describes the declaration being processed.  This function is called from
"declaration" and its return value indicates how processing should proceed
after the call.
*/
{
  an_end_of_decl_action   end_of_decl_action = eoda_not_at_end;
  a_decl_flag_set         dso_flags = state->dso_flags;
  a_decl_flag_set         di_flags = DI_NO_INPUT_FLAGS;
  a_boolean               declarator_omitted = FALSE;

  if (dso_flags & DSO_LINKAGE_SPEC_DECL) {
    /* A linkage-specifier will be found among the decl_specifiers only in
       Microsoft mode -- e.g., for a case like this:
         extern "C" __declspec(dllexport) void f();
       Moreover, it is allowed only if this is not already a linkage-specifier
       declaration -- i.e., an error should have been issued on
         extern "C" __declspec(dllexport) extern "C" void f();
    */
    check_assertion(microsoft_mode && !state->is_linkage_spec_decl);
    /* Set a flag to treat this as a normal linkage-specification
       declaration. */
    state->is_linkage_spec_decl = TRUE;
    /* push_name_linkage was called in decl_specifiers, and the corresponding
       pop must be done before exiting this routine. */
    state->restore_name_linkage = TRUE;
  } else if (state->is_linkage_spec_decl) {
    /* Record the fact that this declaration has a linkage specifier attached
       directly to it (as opposed to just being inside a linkage block). */
    state->decl_modifiers.direct_linkage_specifier = TRUE;
  }  /* if */
  if (!state->decl_specifiers_omitted) {
    /* Check for cases without a declarator and issue a diagnostic if it's
       invalid. */
    declarator_omitted = check_for_missing_declarator(state);
  }  /* if */
#if DECL_MODIFIERS_IN_USE
  if (declarator_omitted ||
      state->declared_storage_class == (a_storage_class)sc_typedef) {
    /* A class type or typedef declaration. */
    if (state->decl_modifiers.flags != 0) {
      /* Most declaration modifiers are not valid on type declarations. */
      diagnose_decl_modifiers_on_type_declaration(state);
    }  /* if */
  }  /* if */
#endif /* DECL_MODIFIERS_IN_USE */
  /* The declaration can end at this point (";" is next). */
  if (!state->decl_specifiers_omitted && declarator_omitted) {
    if (curr_token != tok_semicolon) {
      /* This must be a "dangling type specifier", and an error has already
         been issued on the missing semicolon.  required_token is not called
         because it would flush what is assumed to be the next declaration. */
      end_of_decl_action = eoda_skip_final_token;
    } else {
      end_of_decl_action = eoda_deferred_actions;
    }  /* if */
  } else if (curr_token == tok_void && C_dialect == C_dialect_pcc && 
             state->declared_storage_class == (a_storage_class)sc_typedef &&
             next_token() == tok_semicolon) {
    /* "typedef <something> void;" in pcc mode.  Usually "typedef int void;".
       Shows up in old pre-void-keyword code.  Ignored in pcc mode. */
    set_err_pos_to_curr_token();
    warning(ec_decl_of_void_ignored);
    cannot_bind_to_curr_construct();
    /* Advance past "void" to the semicolon. */
    (void)get_token();
    end_of_decl_action = eoda_skip_final_token;
  } else {
    /* Set the various flags for declarator processing. */
    di_flags = DI_REAL_DECLARATOR_ALLOWED;
    if (C_dialect == C_dialect_cplusplus) {
      di_flags |= DI_OPERATOR_NAME_ALLOWED;
      if (state->declared_storage_class != (a_storage_class)sc_typedef) {
        di_flags |= DI_PARENTHESIZED_INITIALIZER_ALLOWED;
        /* A qualified-id is only allowed for namespace-scope declarations,
           but some Microsoft and GNU compilers also them in local scopes. */
        if (decl_scope_level == depth_innermost_namespace_scope ||
            ((microsoft_mode || (gpp_mode && gnu_version < 40101)) &&
             depth_innermost_namespace_scope != NO_SCOPE_DEPTH)) {
          di_flags |= DI_QUALIFIED_NAME_ALLOWED;
        }  /* if */
      }  /* if */
    }  /* if */
    if (state->is_old_style_param_decl) {
      di_flags |= DI_IS_PARAMETER_DECL;
      /* A variable length array declaration is permitted in an old-style
         parameter declaration. */
      if (vla_enabled) di_flags |= DI_VLA_ALLOWED;
    } else if (vla_enabled) {
      if (!state->function_definition_allowed &&
          state->declared_storage_class != (a_storage_class)sc_asm) {
        /* Not at file scope, so a VLA may appear on some declarations. */
        di_flags |= DI_VARIABLY_MODIFIED_DECL_ALLOWED;
        if (!strict_ansi_mode ||
            (state->declared_storage_class != (a_storage_class)sc_static &&
             state->declared_storage_class != (a_storage_class)sc_extern)) {
            /* When allowing VLAs in nonstrict modes, we first assume a non-
               constant array bound is allowed on any declaration, and later
               verify that the scanned expression was in fact constant if the
               array did not have automatic storage duration.  Doing so,
               however, causes us to e.g. accept "(int)(3.0+4.0)" as a bound
               for a local static array.  However, in strict mode, that bound
               is not a valid integral constant-expression (even though it is
               "constant").  To diagnose those cases in strict mode, we must
               therefore force the scanning of integral constant-expression
               bounds by a priori excluding VLAs. */
          di_flags |= DI_VLA_ALLOWED;
        }  /* if */
      }  /* if */
    }  /* if */
    if ((state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0 &&
         state->qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
  }  /* if */
  *p_di_flags = di_flags;
  return end_of_decl_action;
}  /* prep_for_declarator */


void start_secondary_declarator(a_decl_parse_state  *ps)
/*
*ps describes specifiers and a declarator from a declaration that contains
multiple declarators (like "int i, *p;").  Initialize various declarator-
related-fields of *ps prior to scanning the next declarator.
*/
{
  clear_decl_parse_state_fields(ps, /*secondary_declarator=*/TRUE);
}  /* start_secondary_declarator */

#if GENERATE_SOURCE_SEQUENCE_LISTS

void mark_decl_after_first_in_comma_list(a_decl_parse_state  *dps)
/*
The declaration described by *dps is associated with a declarator that appears
after the first one in a list (e.g. "j" in "int i, j;").  Record that fact.
*/
{
  a_source_sequence_entry_ptr  ssep = dps->source_sequence_entry;

  if (ssep != NULL) {
    if (ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
      ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr)
                                   ->is_decl_after_first_in_comma_list = TRUE;
    } else if (ss_entry_kind(ssep) != iek_none) {
      check_assertion(ss_entry_kind(ssep) == iek_routine ||
                      ss_entry_kind(ssep) == iek_variable ||
                      ss_entry_kind(ssep) == iek_constant ||
                      ss_entry_kind(ssep) == iek_field ||
                      ss_entry_kind(ssep) == iek_type);
      ss_entry_ptr(ssep, a_source_correspondence_ptr)
                                   ->is_decl_after_first_in_comma_list = TRUE;
    } else if (dps->is_old_style_param_decl) {
      check_assertion(dps->sym != NULL &&
                      dps->sym->kind == (a_symbol_kind)sk_parameter);
      dps->sym->variant.param_id->is_decl_after_first_in_comma_list = TRUE;
    } else {
      expect_error();
    }  /* if */
  }  /* if */
}  /* mark_decl_after_first_in_comma_list */


void wrapup_sse_for_simple_decl(a_decl_parse_state  *dps)
/*
*dps describes a "simple declaration" (a declaration involving type specifiers
followed by a declarator) of a function, variable, or typedef.  Make any
required updates in the source sequence entry for this declaration.
*/
{
  a_source_sequence_entry_ptr  ssep = dps->source_sequence_entry;

  if (ssep != NULL) {
    if (dps->secondary_declarator) {
      mark_decl_after_first_in_comma_list(dps);
    }  /* if */
    if (ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
      /* Attach a copy of the attributes to the secondary source sequence
         entry.  Since the attributes have already been attached to the main
         IL entry, any id attributes on the id_attributes list will be
         followed by any prefix attributes: We therefore do not copy the
         prefix_attributes list if the id_attributes list is non-empty (since
         that would result in duplicate attributes). */
      a_src_seq_secondary_decl_ptr
                     sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
      if (dps->id_attributes != NULL) {
        sssdp->attributes = copy_of_attributes_list(dps->id_attributes);
      } else if (dps->prefix_attributes != NULL) {
        sssdp->attributes = copy_of_attributes_list(dps->prefix_attributes);
      }  /* if */
      if (dps->declared_storage_class != (a_storage_class)sc_unspecified) {
        sssdp->explicit_storage_class = TRUE;
        sssdp->declared_storage_class = dps->declared_storage_class;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* wrapup_sse_for_simple_decl */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void check_deduced_auto_type(a_decl_parse_state  *dps)
/*
*dps describes a declaration involving an "auto" type specifier and the type
of the declaration has already been deduced from the initializer.  Check that
this type is consistent with any previous declarations of the entity, and emit
a diagnostic if that isn't the case.
*/
{
  if (dps->prev_type != NULL) {
    if (!check_variable_redecl_compatible(dps)) {
      dps->auto_type_specifier_seen = FALSE;
      dps->specifiers_type = dps->deduced_auto_type = dps->type = error_type();
    }  /* if */
  }  /* if */
}  /* check_deduced_auto_type */


void check_use_of_auto_type(a_decl_parse_state  *dps)
/*
Check that if the "auto" or "decltype(auto)" type specifier was used in the
current declaration, an initializer enabled the deduction of an actual type.
Issue an error if that was not the case and set dps->specifiers_type to an
error type to avoid repeating the diagnostic if additional declarators follow.
Also diagnose invalid uses of "auto" and "decltype(auto)" in other contexts
(e.g., casts).  This does not apply to the placeholder used to introduce a
trailing return type (including invalid cases like "decltype(auto) f()->int",
which are diagnosed elsewhere).
*/
{
  a_boolean  err = FALSE;

  if (!dps->auto_type_specifier_seen || dps->has_trailing_return_type ||
      (deduced_return_types_enabled &&
       ((dps->type != NULL && dps->type->kind == (a_type_kind)tk_routine) ||
        dps->is_trailing_return_type))) {
    /* Not a declaration that requires this checking. */
  } else if (dps->type != NULL && is_error_type(dps->type)) {
    /* Some error already occurred.  Additional diagnostics are unlikely to
       be helpful. */
    expect_error();
  } else if (!dps->range_based_for &&
             !(dps->assoc_func_decl_state != NULL && dps->auto_type_allowed) &&
             (!dps->has_initializer || !dps->auto_type_allowed)) {
    /* "auto"/"decltype(auto)" was seen, but we never saw an initializer or
       else the specifier is not allowed at all in this context. */
    err = TRUE;
    if (!dps->auto_type_allowed) {
      /* A context where an "auto" type is simply not allowed.  (E.g., an
         exception handler parameter.) */
      pos_error(dps->decltype_auto_specifier_seen ?
                  ec_decltype_auto_not_allowed_here : ec_auto_not_allowed_here,
                &dps->auto_pos);
    } else if (dps->sym != NULL && !dps->sym->is_error) {
      /* A named entity was declared: Issue the error on the declarator (there
         could be more than one sharing the same auto specifier). */
      pos_error(dps->decltype_auto_specifier_seen ?
                  ec_decltype_auto_type_requires_initializer :
                  ec_auto_type_requires_initializer,
                &dps->declarator_pos);
    } else {
      /* An unnamed entity (e.g., bit field) or a severe syntax error.
         Issuing the error on the auto specifier is usually more helpful. */
      pos_error(dps->decltype_auto_specifier_seen ?
                  ec_decltype_auto_type_requires_initializer :
                  ec_auto_type_requires_initializer,
                &dps->auto_pos);
    }  /* if */
  } else if (dps->decltype_auto_specifier_seen &&
             !identical_types(dps->declared_type, dps->auto_type)) {
    /* "decltype(auto)" was seen, but that type is modified in some way; e.g.,
       "decltype(auto) *p = &x;".  That is not permitted. */
    pos_error(ec_modified_decltype_auto_type, &dps->auto_pos);
  }  /* if */
  if (err) {
    dps->auto_type_specifier_seen = FALSE;
    dps->auto_type = NULL;
    invalidate_type(dps);
    if (dps->sym != NULL) {
      /* Update the IL entry.  Normally it should be a variable or static data
         member, but erroneous uses of "auto"/"decltype(auto)" can get here for
         other entities (e.g., fields) as well.  Recording an error type in the
         IL entry avoids error cascades later on and prevents aborts in code
         that isn't expecting a template parameter type. */
      a_variable_ptr  vp = NULL;
      a_type_ptr      *p_type = NULL;
      switch (dps->sym->kind) {
        case sk_constant:
          p_type = &dps->sym->variant.constant->type;
          break;
        case sk_variable:
          vp = dps->sym->variant.variable.ptr;
          p_type = &vp->type;
          break;
        case sk_field:
          p_type = &dps->sym->variant.field.ptr->type;
          break;
        case sk_static_data_member:
          vp = dps->sym->variant.static_data_member.variable;
          p_type = &vp->type;
          break;
        case sk_routine:
        case sk_member_function:
          p_type = &dps->sym->variant.routine.ptr->type;
          break;
        default:
          unexpected_condition_str("check_use_of_auto_type: bad symbol");
      }  /* switch */
      if (p_type != NULL) {
        *p_type = dps->type;
      }  /* if */
      if (vp != NULL) {
        vp->declared_with_auto_type_specifier = FALSE;
        vp->declared_with_decltype_auto = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_use_of_auto_type */


static a_boolean is_terse_range_based_for_declaration(void)
/*
The current token is the token following "for (" in a range-based for
statement.  Return TRUE if the expected declaration that follows has the
"terse" form omitting decl-specifiers.  I.e., return TRUE if it has the form
	<identifier> <opt-attributes> :
*/
{
  a_boolean  result = FALSE;

  if (curr_token == tok_identifier) {
    a_token_cache            cache;
    a_token_sequence_number  start_tsn = curr_token_sequence_number;
    begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
    (void)get_token();
    if (curr_token == tok_lbracket) {
      /* Skip attributes. */
      flush_until_matching_token_full(/*limit_flush=*/FALSE);
      (void)get_token();
    }  /* if */
    result = curr_token == tok_colon;
    end_caching_fetched_tokens();
    clear_token_cache(&cache, /*reusable=*/FALSE);
    /* Get the tokens that were fetched by this routine from the cache
       that has been accumulated and rescan them. */
    copy_tokens_from_cache(curr_lexical_state_cache(), start_tsn,
                           last_token_sequence_number_of_token,
                           /*include_last_token=*/TRUE, &cache);
    f_rescan_cached_tokens(
               &cache, /*discard_curr_token=*/curr_token != tok_end_of_source);
  }  /* if */
  return result;
}  /* is_terse_range_based_for_declaration */


void scan_nonmember_declaration(a_decl_parse_state  *dps,
                                a_source_range      *linkage_spec_range_ptr)
/*
This routine scans declarations in the following scope kinds: file scope,
namespace scope, function scope, and block scope.  It is also used to scan
old-style C parameter declarations (in which case dps->is_old_style_param_decl
is TRUE and dps->param_id_list will list the parameter names in the associated
function declarator).

*dps tracks the properties of the declaration being parsed.  The caller can
set some of its fields to direct processing (e.g., dps->is_old_style_param_decl
should be set to TRUE and dps->param_id_list should list the parameter names in
the associated function declarator to scan old-style parameter definitions),
while other fields will be set so the caller can inspect the outcome of the
declaration (e.g., dps->sym points to the principal symbol, if any, entered for
the declaration).

linkage_spec_range_ptr is non-NULL when this declaration includes an explicit
linkage specification, but is otherwise NULL, even when it is part of a block
of declarations governed by a linkage specification (i.e., it is non-NULL for
`extern "C" void f()' and NULL for `extern "C" { void f() }'); when it is
non-NULL, it indicates the source range of the linkage specifier.

Broadly speaking, three kinds of declarations are handled here:
  1. Declarations consisting of "declarators" optionally preceded by some
     "specifiers".  This includes declarations of variables, functions, and
     typedefs, as well as out-of-class definitions of member functions and
     static data members.
     A single declaration may sometimes introduce multiple comma-separated
     declarators: This is handled by the main do-while loop containing a
     call to "declarator"; the loop is preceded by a call to "decl_specifiers"
     which scans the specifiers (if any).
  2. Declarations that consist only of "specifiers".  (Normally, these are
     declarations of class types or enumeration types, but various error cases
     also follow this path.)
  3. Declarations that do not fall in the previous two categories.  Examples
     include namespace declarations, using-declarations, template declarations,
     static_assert constructs, and so forth.  These cases are treated by the
     call to "check_special_declaration_form".

Declarations in class scope are handled by the similar routine
"class_member_declaration".  Ordinary (as opposed to old-style) function
parameter declarations are scanned by function_declarator, and template
parameters are scanned by scan_a_template_parameter_declaration.
*/
{
  a_decl_flag_set              dsi_flags, di_flags;
  a_boolean                    is_function;
  a_symbol_locator             locator;
  a_func_info_block            func_info;
  a_boolean                    first_declarator = TRUE;
  a_boolean                    access_checks_deferred = FALSE;
  a_token_kind                 final_token = tok_semicolon;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_statement_ptr              decl_stmt = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_decl_pos_block             decl_pos_block;
  a_type_qualifier_set         saved_qualifiers;
  a_source_position            saved_qualifiers_pos;
  a_boolean                    is_old_style_param_decl =
                                                 dps->is_old_style_param_decl;

  set_err_pos_to_curr_token();
  dps->auto_type_allowed = auto_type_specifier_enabled;
  copy_source_position(pos_curr_token, dps->start_pos);
  clear_decl_pos_block(&decl_pos_block);
  if (gnu_mode && !dps->marked_as_gnu_extension &&
      curr_token == tok_extension) {
    /* Record the GNU C __extension__ annotation. */
    (void)get_token();
    dps->marked_as_gnu_extension = TRUE;
  }  /* if */
  if (depth_stmt_stack >= 0 &&
      struct_stmt_stack_top().record_declared_entities &&
      (scope_is(&scope_stack_top(), sck_function) ||
       scope_is(&scope_stack_top(), sck_block))) {
    /* This is the declaration in a declaration statement. */
    /* Set up a pointer to entities declared from this point on. */
    an_il_entity_list_entry_ptr
                             *p = struct_stmt_stack_top().p_declared_entities;
    /* Skip to the end of the list. */
    while (*p != NULL) p = &(*p)->next;
    dps->p_postfix_entities = p;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Keep a pointer to the statement so that its end position can be updated
       below. */
    decl_stmt = struct_stmt_stack_top().last_dep_statement;
    check_assertion(decl_stmt != NULL &&
                    decl_stmt->kind == (a_statement_kind)stmk_decl);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (decl_scope_level == depth_innermost_namespace_scope) {
    /* For each declaration at namespace scope, reset the source-sequence
       insert point for instantiations to NULL -- it will be set to point
       to the first source sequence entry that
       add_source_sequence_entry_to_list sees, which should be the first
       entry associated with the current declaration. */
    reset_ss_list_instantiation_insert_point();
  }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (linkage_spec_range_ptr != NULL) {
    /* The caller has already scanned the linkage specifier. */
    dps->is_linkage_spec_decl = TRUE;
    dps->restore_name_linkage = TRUE;
    /* The caller already has a declaration parse state.  It may have Microsoft
       attributes; if so, move them to the current state. */
    check_assertion(scope_stack_top().decl_parse_state != NULL);
#if MICROSOFT_EXTENSIONS_ALLOWED
    dps->ms_attributes = scope_stack_top().decl_parse_state->ms_attributes;
    scope_stack_top().decl_parse_state->ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Called in the midst of an ``extern "C"'' declaration, so
       select_curr_construct_pragmas has already been called. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Adjust the specifiers-range to reflect the fact that there is a linkage
       specification (which was scanned by the caller; in Microsoft mode, the
       linkage specification may be scanned by the call to decl_specifiers
       below).  The specifiers_range.end component will be updated if more
       specifiers follow. */
    decl_pos_block.specifiers_range.start = linkage_spec_range_ptr->start;
    decl_pos_block.specifiers_range.end = linkage_spec_range_ptr->end;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  } else if (!dps->function_definition_allowed) {
    /* Called while processing a routine -- select_curr_construct_pragmas
       will already have been called. */
  } else {
    /* Move cached #pragma declarations (if any) to the current scope stack
       entry so they can be examined and acted upon in subsequent
       processing. */
    (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
  }  /* if */
  if (dps->function_definition_allowed) {
    /* This is a file scope or namespace scope declaration.  Indicate
       that access checking should be deferred until the declarator has
       been scanned. */
    begin_deferral_of_access_checks();
    access_checks_deferred = TRUE;
  }  /* if */
  dps->prefix_attributes = scan_attributes(al_prefix);
  /* Handle any cases that don't start with a decl-specifier or a
     declarator. */
  switch (check_special_declaration_form(dps, &final_token)) {
    case eoda_not_at_end:        break;
    case eoda_done:              goto return_point;
    case eoda_skip_final_token:  goto advance_past_final_token;
    case eoda_check_semicolon:   goto check_for_semicolon;
    default:                     unexpected_condition();
  }  /* switch */
  add_stop_token(tok_semicolon);
  dps->need_semicolon_remove_stop_token = TRUE;
  /* Set the flags for calling decl_specifiers. */
  dsi_flags = get_decl_specifiers_flags(dps);
  if (dps->range_based_for && terse_range_based_for_enabled &&
      curr_token == tok_identifier &&
      is_terse_range_based_for_declaration()) {
    /* A C++17-style "terse" range-based for declaration.  I.e., something
       like the "x: v" in "for (x: v) { f(x); }". */
    if (dps->prefix_attributes != NULL) {
      pos_error(ec_attribute_not_allowed, &dps->prefix_attributes->position);
    }  /* if */
    dps->auto_type = make_auto_type(&pos_curr_token,
                                    /*is_decltype_auto=*/FALSE);
    dps->specifiers_type = make_rvalue_reference_type(dps->auto_type);
    dps->type = dps->specifiers_type;
    dps->declared_type = dps->type;
    dps->auto_type_specifier_seen = TRUE;
  } else {
    /* Scan the initial declaration specifiers (including storage class,
       type specifiers, and type qualifiers).  For a function definition,
       the specifiers can be omitted entirely. */
    decl_specifiers(dsi_flags, dps, &decl_pos_block);
  }  /* if */
  switch (prep_for_declarator(dps, &di_flags)) {
    case eoda_not_at_end:        break;
    case eoda_skip_final_token:  goto advance_past_final_token;
    case eoda_deferred_actions:  goto deferred_fixups;
    default:                     unexpected_condition();
  }  /* switch */
  /* Save some state that must be restored for each declarator. */
  saved_qualifiers = dps->qualifiers;
  saved_qualifiers_pos = dps->qualifiers_pos;
  /* Scan the declarator list. */
  do {
    an_il_entity_list_entry_ptr  saved_entities = NULL;
    if (!first_declarator) {
      /* We've just skipped a comma separating two declarators. */
      /* Before parsing the next declaration, run any end-of-parse actions
         needed for the previous declarator. */
      run_end_of_parse_actions(dps, /*more_declarators=*/TRUE);
      /* Reinitialize the declarator-specific parts of the parse state. */
      start_secondary_declarator(dps);
      dps->qualifiers = saved_qualifiers;
      dps->qualifiers_pos = saved_qualifiers_pos;
      /* Re-initialize dps->is_old_style_param_decl for every declarator,
         because it might have been modified during the processing of the prior
         declarator (e.g., as an error recovery strategy). */
      dps->is_old_style_param_decl = is_old_style_param_decl;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && !C_mode() && microsoft_version >= 1000 &&
          !is_abstract_or_real_declarator_start() &&
          is_decl_start(IDS_MS_ATTRIB_NOT_ALLOWED)) {
        /* Microsoft C++ compilers allow decl-specifiers to appear after the
           comma separating two declarators.  E.g.: "int i, char *s;" */
        dps->auto_type_allowed = FALSE;
        scan_microsoft_secondary_decl_specifiers(dsi_flags, dps,
                                                 &decl_pos_block);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      if (depth_scope_stack == depth_innermost_namespace_scope) {
        /* This is a declaration at file scope, and not the first declarator
           in the declarator list.  As for the start of the declaration, set
           the source-sequence insert point for instantiations to NULL. */
        reset_ss_list_instantiation_insert_point();
      }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
      /* Scan prefix declarator attributes.  Note that those can only
         appear after a comma separating two declarators.  Any attributes
         prefixing a leading declarator will have been parsed as part of
         the specifier attributes.  GNU versions prior to 3.1 treated all
         prefix attributes as specifier attributes; we emulate the more
         recent (GNU C/C++ 3.1 and later) behavior. */
      scan_gnu_declarator_attributes(dps);
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    add_stop_token(tok_comma);
    dps->need_comma_remove_stop_token = TRUE;
    add_stop_token(tok_assign);
    dps->need_assign_remove_stop_token = TRUE;
    if (dps->function_definition_allowed) {
      add_stop_token(tok_lbrace);
      dps->need_lbrace_remove_stop_token = TRUE;
    }  /* if */
    clear_func_info(&func_info);
#if ASM_FUNCTION_ALLOWED
    if (dps->declared_storage_class == (a_storage_class)sc_asm) {
      func_info.is_asm_function = TRUE;
    }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
    declarator(di_flags, dps, /*member_parent_type=*/(a_type_ptr)NULL,
               &locator, &func_info, &decl_pos_block);
    is_function = (dps->declared_storage_class !=
                                                (a_storage_class)sc_typedef &&
                   !is_old_style_param_decl &&
                   is_function_type(dps->type));
#if GNU_EXTENSIONS_ALLOWED
    scan_gnu_asm_name(dps);
    scan_gnu_declarator_attributes(dps);
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (dps->p_postfix_entities != NULL) {
      /* If we are in a declaration statement, temporarily put aside any
         associated entities that were declared after the last declarator-id.
         They will be appended (below) after the entity associated with that
         declarator-id. */
      saved_entities = *dps->p_postfix_entities;
      *dps->p_postfix_entities = NULL;
    }  /* if */
    if (is_old_style_param_decl) {
      prep_old_style_param_decl(dps, &func_info, &locator);
    }  /* if */
    check_for_definition_in_return_type(dps);
    if (is_function && any_cfront_mode()) {
      /* Check for the declaration with a "member function typedef" type -- it
         is only supposed to be used for pointer-to-member declarations (only
         in cfront compatibility mode). */
      if (check_member_function_typedef(dps->type, &dps->start_pos)) {
        is_function = FALSE;
        invalidate_type(dps);
      }  /* if */
    }  /* if */
    if (dps->need_lbrace_remove_stop_token) {
      remove_stop_token(tok_lbrace);
      dps->need_lbrace_remove_stop_token = FALSE;
    }  /* if */
    remove_stop_token(tok_assign);
    dps->need_assign_remove_stop_token = FALSE;
    if (is_function) {
      switch (function_declaration(dps, &func_info, &locator,
                                   &decl_pos_block, &final_token)) {
        case eoda_not_at_end:        break;
        case eoda_skip_final_token:  goto advance_past_final_token;
        case eoda_done:              goto return_point;
        default:                     unexpected_condition();
      }  /* switch */
    } else if (dps->declared_storage_class != (a_storage_class)sc_typedef) {
      variable_declaration(dps, &locator, &decl_pos_block);
      if (dps->range_based_for) {
        check_assertion_or_expect_error(curr_token == tok_colon);
        goto advance_past_final_token;
      }  /* if */
    } else {
      typedef_declaration(dps, &locator, &decl_pos_block);
    }  /* if */
    done_with_func_info(func_info);
    if (saved_entities != NULL) {
      /* Append entities declared after the declarator-id (e.g., in an array
         dimension) to the list of entities associated with the current
         declaration statement. */
      while (*dps->p_postfix_entities != NULL) {
        dps->p_postfix_entities = &(*dps->p_postfix_entities)->next;
      }  /* while */
      *dps->p_postfix_entities = saved_entities;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    wrapup_sse_for_simple_decl(dps);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    remove_stop_token(tok_comma);
    dps->need_comma_remove_stop_token = FALSE;
    first_declarator = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (dps->ms_attributes != NULL) {
      /* Microsoft attributes were specified, but they were not applicable
         to this declaration.  Issue an error and clean up as needed. */
      dispose_of_unapplied_attributes(&dps->ms_attributes,
                                      ec_ms_attr_not_allowed);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Keep scanning the list of declarators. */
  } while (loop_token(tok_comma));
deferred_fixups:
  if (microsoft_bugs) {
    /* In Microsoft bugs mode, the typedef is processed before member function
       bodies etc. are rescanned.  This makes e.g. the following legal:
          typedef struct {
            enum { e };
            void f() { S::e; PS p; }
          } S, *PS;
    */
    process_deferred_class_fixups_and_instantiations(
                                                  /*for_instantiation=*/FALSE);
  }  /* if */
check_for_semicolon:
  /* Check for a final semicolon. */
  if (!required_token_no_advance(tok_semicolon, ec_exp_semicolon)) {
    goto return_point;
  }  /* if */
advance_past_final_token:
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_stmt != NULL) decl_stmt->end_position = end_pos_curr_token;
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (dps->is_linkage_spec_decl) {
    pop_name_linkage();
    dps->restore_name_linkage = FALSE;
  }  /* if */
  if (curr_token == final_token) {
    /* Advance past the final token of the declaration (which should be a
       ';' or '}').  However, if the current declaration is a top-level
       declaration, set a global flag to enable checking for a header stop. */
    if (dps->is_top_level_declaration) {
      next_token_is_top_level_decl_start = TRUE;
    }  /* if */
    (void)get_token();
    next_token_is_top_level_decl_start = FALSE;
  }  /* if */
return_point:
  run_end_of_parse_actions(dps, /*more_declarators=*/FALSE);
  check_pending_qualifiers_used(dps);
  if (access_checks_deferred) {
    /* We are processing a declaration for which access checks were deferred.
       In some cases (like out-of-class member definitions) deferred checks
       will have already been performed.  For the remaining cases, do them
       now. */
    end_deferral_of_access_checks();
  }  /* if */
  if (dps->is_linkage_spec_decl) {
    /* Unless restore_name_linkage is TRUE, pop_name_linkage will already
       have been called. */
    if (dps->restore_name_linkage) pop_name_linkage();
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (dps->ms_attributes != NULL) {
    /* Microsoft attributes were specified, but they were not applicable
       to this declaration.  Issue an error and clean up as needed. */
    dispose_of_unapplied_attributes(&dps->ms_attributes,
                                    ec_ms_attr_not_allowed);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do necessary remove_stop_tokens.  Even when there is no error, this
     does the remove_stop_token for tok_semicolon. */
  remove_all_local_stop_tokens(dps);
}  /* scan_nonmember_declaration */


void declaration(a_boolean       function_definition_allowed,
                 a_boolean       is_old_style_param_decl,
                 a_boolean       is_top_level_declaration,
                 a_boolean       marked_as_gnu_extension,
                 a_param_id_ptr  param_id_list,
                 a_source_range  *linkage_spec_range_ptr)
/*
Wrapper function for scan_nonmember_declaration that sets up a declaration
parse state with fields described by the corresponding given parameters.
*/
{
  a_decl_parse_state  dps;

  db_enter(3, "declaration");
  /* Initialize a structure tracking the state of declaration processing. */
  init_decl_parse_state(&dps);
  dps.function_definition_allowed = function_definition_allowed;
  dps.is_old_style_param_decl = is_old_style_param_decl;
  dps.is_top_level_declaration = is_top_level_declaration;
  dps.marked_as_gnu_extension = marked_as_gnu_extension;
  dps.param_id_list = param_id_list;
  scan_nonmember_declaration(&dps, linkage_spec_range_ptr);
  db_exit();
  return;
}  /* declaration */


void translation_unit(void)
/*
Scan a translation-unit (3.7).  This is the topmost syntactic entity in
a compilation.  The syntax is

3.7    translation-unit:
		external-declaration
		translation-unit external-declaration

In C++, however, the declaration list is optional (3.4):

       translation-union:
                declaration-seq
                               opt
*/
{
  if (using_a_pch_file) {
    /* When using a precompiled header, do any special processing needed
       for a preincluded file. */
    pch_prefix_processing_for_preinclude();
  }  /* if */
  /* If the preinclude_macros option was used, scan the files that provide
     macro definitions. */
  if (macro_preinclude_file_list != NULL) process_macro_preincludes();
  /* Set the global flag to enable the check for a header stop. */
  next_token_is_top_level_decl_start = TRUE;
  (void)get_token();
  next_token_is_top_level_decl_start = FALSE;
  if (next_event_resumes_compilation) {
     /* We are done skipping the file prefix when making use of a PCH (i.e.,
        to skip over the part of the file that is being replaced by
        information from the PCH).  We've encountered the first token of
        the normal compilation.  Do any fixup required. */
    pch_fixup_part_2();
  }  /* if */
  if (curr_token == tok_end_of_source) {
    /* Empty translation unit -- okay in C++ mode. */
    if (C_mode()) {
      /* A translation unit cannot be empty.  Note that this can happen not
         only for an empty file, but also for a file containing only
         preprocessing directives.  pcc allows an empty source file.
         In ANSI mode, it's allowed as an extension. */
      if (strict_ansi_mode) {
        diagnostic(strict_ansi_error_severity, ec_empty_translation_unit);
      }  /* if */
    }  /* if */
  } else {
    while (curr_token != tok_end_of_source) {
      /* A C99 predefined pragma in the file scope must appear between
         top-level declarations. */
      if (c99_mode || fixed_point_enabled) check_for_stdc_pragmas();
      declaration(/*function_definition_allowed=*/TRUE,
                  /*is_old_style_param_decl=*/FALSE,
                  /*is_top_level_declaration=*/TRUE,
                  /*marked_as_gnu_extension=*/FALSE,
                  (a_param_id_ptr)NULL, (a_source_range *)NULL);
    } /* while */
  }  /* if */
  check_assertion_str2(!header_stop_position_pending, "translation_unit:",
                       "header stop position not found");
  /* Do any end-of-translation unit pragma processing that may be required. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  /* First reset the point for instantiations to NULL. */
  reset_ss_list_instantiation_insert_point();
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* A C99 predefined pragma in the file scope must appear between
     top-level declarations. */
  if (c99_mode) check_for_stdc_pragmas();
  process_pragmas_at_end_of_source();
}  /* translation_unit */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
void scan_implicitly_included_template_definition_file(void)
/*
Scan an implicitly included template definition file.  It is just like
scanning a translation-unit, except there's no diagnostic on the empty file.
*/
{
  (void)get_token();
  while (curr_token != tok_end_of_source) {
    declaration(/*function_definition_allowed=*/TRUE,
                /*is_old_style_param_decl=*/FALSE,
                /*is_top_level_declaration=*/FALSE,
                /*marked_as_gnu_extension=*/FALSE,
                (a_param_id_ptr)NULL, (a_source_range *)NULL);
  }  /* if */
  process_pragmas_at_end_of_source();
}  /* scan_implicitly_included_template_definition_file */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


#if MICROSOFT_EXTENSIONS_ALLOWED
void scan_top_level_metadata_declarations(a_const_char      *buffer,
                                          an_assembly_index assembly_index)
/*
Scan the top level declarations generated by the metadata reader given
by *buffer.  assembly_index is the index of the assembly from which the
metadata originated (or zero if the buffer did not come from an assembly).
*/
{
  a_token_cache     cache;
  a_boolean         saved_scanning_generated_code_from_metadata;
  a_boolean         saved_next_token_is_top_level_decl_start;
  a_source_position insert_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean         saved_source_sequence_entries_disallowed;

  /* Don't generate source sequence entries for injected the code from
     the metadata. */ 
  saved_source_sequence_entries_disallowed =
                                            source_sequence_entries_disallowed;
  source_sequence_entries_disallowed = TRUE;
  scope_stack_top().source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  saved_scanning_generated_code_from_metadata 
                                       = scanning_generated_code_from_metadata;
  saved_next_token_is_top_level_decl_start =
                                            next_token_is_top_level_decl_start;
  scanning_generated_code_from_metadata = TRUE;
  check_assertion(scope_stack[depth_scope_stack].kind 
                                                    == (a_scope_kind)sck_file);
  if (assembly_index != 0) {
    /* Determine the position information to use for this assembly file. */
    a_cli_metadata_file_ptr cmfp = map_assembly_index_to_cmfp(assembly_index);
    check_assertion(cmfp != NULL);
    insert_position = cmfp->inserted_position;
  } else {
    /* Most likely code that originates in the front end itself; use a
       NULL source position. */
    insert_position = null_source_position;
  }  /* if */
  /* Inject an end-of-source token into the token stream to prevent
     any over reading the token stream. */
  clear_token_cache(&cache, /*reusable=*/FALSE);
  terminate_token_cache(&cache);
  rescan_cached_tokens(&cache);
  /* Insert the generated code into the token stream. */
  insert_string_into_token_stream(buffer, /*insert_after=*/FALSE,
                                  /*p_expand_macros=*/FALSE,
                                  insert_position);
  while (curr_token != tok_end_of_source) {
    declaration(/*function_definition_allowed=*/TRUE,
                /*is_old_style_param_decl=*/FALSE,
                /*is_top_level_declaration=*/TRUE,
                /*marked_as_gnu_extension=*/FALSE,
                (a_param_id_ptr)NULL, (a_source_range *)NULL);
  }  /* while */
  /* Get the injected end of source token. */
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
  /* Restore the flags. */
  scanning_generated_code_from_metadata 
                                 = saved_scanning_generated_code_from_metadata;
  next_token_is_top_level_decl_start =
                                      saved_next_token_is_top_level_decl_start;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* scan_top_level_metadata_declarations */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


void decls_trans_unit_init(void)
/*
Initialize variables that are specific to a given translation unit.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  scanning_generated_code_from_metadata = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* decls_trans_unit_init */


void decls_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
#if !NULL_POINTER_IS_ZERO
  clear_init_state_fields(&null_init_state);
  clear_decl_parse_state_fields(&null_decl_parse_state,
                                /*secondary_declarator=*/FALSE);
#endif /* !NULL_POINTER_IS_ZERO */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_decl_parse_states),
      pch_saved_var_array_elem(avail_decl_parse_callbacks),
      pch_saved_var_array_elem(avail_auto_param_descriptions),
#if DEBUG
      pch_saved_var_array_elem(num_decl_parse_states_allocated),
      pch_saved_var_array_elem(num_decl_parse_callbacks_allocated),
      pch_saved_var_array_elem(num_auto_param_descriptions_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* decls_one_time_init */


void decls_init(void)
/*
Do initialization of static variables defined in this file that require
initialization for each compilation.
*/
{
#if DEBUG
  num_decl_parse_states_allocated = 0;
  num_decl_parse_callbacks_allocated = 0;
  num_auto_param_descriptions_allocated = 0;
#endif /* DEBUG */
  avail_decl_parse_states = NULL;
  avail_decl_parse_callbacks = NULL;
  avail_auto_param_descriptions = NULL;
}  /* decls_init */

#if DEBUG

unsigned long show_decl_space_used(void)
/*
Display and return the amount of space used for various GNU attribute-related
entities.
*/
{
  unsigned long grand_total = 0;
  unsigned long num, size, total;

  db_space_used_header("Declaration parsing:");
  db_space_used_lost("decl-parse states",
                     avail_decl_parse_states,
                     num_decl_parse_states_allocated,
                     a_decl_parse_state);
  db_space_used_lost("decl-parse callbacks",
                     avail_decl_parse_callbacks,
                     num_decl_parse_callbacks_allocated,
                     a_decl_parse_callback);
  db_space_used_lost("auto param descriptions",
                     avail_auto_param_descriptions,
                     num_auto_param_descriptions_allocated,
                     an_auto_param_descr);
  return grand_total;
}  /* show_decl_space_used */

#endif /* DEBUG */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
