/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

trans_corresp.c -- Routines  related to matching entities across
                   translation units.

*/

#include "basic_hdrs.h"

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
#include "trans_corresp.h"

/* Forward declarations. */
static a_boolean verify_type_correspondence(a_type_ptr  type);


char* f_canonical_il_entry_of(char *il_entry)
/*
Return the canonical IL entry for the given entry.  This is the entry itself
if the entry is from a primary translation unit or if it has no corresponding
entry in another translation unit.
*/
{
  for (;;) {
    an_il_entry_prefix_ptr  prefix = &il_entry_prefix_of(il_entry);
    if (!prefix->secondary_trans_unit) {
      break;
    } else {
      char *canonical_ptr = trans_unit_corresp_pointer_of(il_entry);
      if (canonical_ptr == NULL) {
        break;
      } else {
        il_entry = canonical_ptr;
      }  /* if */
    }  /* if */
  }  /* for */
  return il_entry;
}  /* f_canonical_il_entry_of */


static void f_clear_trans_unit_corresp(char *ptr)
/*
Mark the given IL entry as having no correspondence in another translation
unit.
FIXME: maybe this should set the pointer to ptr itself (to distinguish
unexamined nodes).
*/
{
  trans_unit_corresp_pointer_of(ptr) = NULL;
}  /* f_clear_trans_unit_corresp */

#define clear_trans_unit_corresp(ptr)                                  \
  f_clear_trans_unit_corresp((char*)ptr)


static void f_record_trans_unit_corresp(char *entity1,
                                        char *entity2)
/*
Make the translation unit correspondence entry of the IL node pointed to by
entity1 point to the IL node pointed to by entity2.
*/
{
  entity2 = canonical_il_entry_of(entity2);
  trans_unit_corresp_pointer_of(entity1) = entity2;
#if DEBUG
  if (db_flag_is_set("trans_corresp")) {
    a_source_correspondence_ptr  scp1 = (a_source_correspondence_ptr)entity1;
    a_source_correspondence_ptr  scp2 = (a_source_correspondence_ptr)entity2;
    a_line_number  line;
    char           *file_name, *full_name;
    a_boolean      at_end_of_source;

    fprintf(f_debug, "DBG> ");
    if (scp1->assoc_info != NULL) {
      a_symbol_ptr  sym = (a_symbol_ptr)scp1->assoc_info;
      db_symbol_name(sym);
      fprintf(f_debug, " (%s)", symbol_kind_names[(int)sym->kind]);
    } else {
      db_name(scp1);
    }  /* if */
    conv_seq_to_file_and_line(scp1->decl_position.seq, &file_name, &full_name,
                              &line, &at_end_of_source);
    fprintf(f_debug, " in file %s (line %ld) should correspond to ",
            file_name, line);
    conv_seq_to_file_and_line(scp2->decl_position.seq, &file_name, &full_name,
                              &line, &at_end_of_source);
    fprintf(f_debug, "entity in file %s (line %ld).\n", file_name, line);
  }  /* if */
#endif /* DEBUG */
}  /* f_record_trans_unit_corresp */

#define record_trans_unit_corresp(entity1, entity2)                    \
  f_record_trans_unit_corresp((char*)entity1, (char*)entity2)


static void f_report_bad_trans_unit_corresp(char *entity1)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence, but it points to a node
that does not actually correspond to the given entity.  Therefore, issue a
diagnostic.
*/
{
  a_symbol_ptr   sym = (a_symbol_ptr)((a_source_correspondence_ptr)entity1)->
                                                                    assoc_info;
  char           *entity2 = trans_unit_corresp_pointer_of(entity1);
  a_source_correspondence_ptr
                 scp2 = (a_source_correspondence_ptr)entity2;
  a_source_position_ptr
                 pos1 = &sym->decl_position,
                 pos2 = &scp2->decl_position;
  a_line_number  line1, line2;
  unsigned long  nesting_depth;
  a_boolean      at_end_of_source;
  a_source_file_ptr
                 src_file1 = source_file_for_seq(
                                pos1->seq, &line1, &at_end_of_source,
                                &nesting_depth, /*physical_line=*/TRUE),
                 src_file2 = source_file_for_seq(
                                pos2->seq, &line2, &at_end_of_source,
                                &nesting_depth, /*physical_line=*/TRUE);

  if (src_file1 != NULL && src_file2 != NULL &&
      src_file1->full_name != NULL && src_file2->full_name != NULL &&
      strcmp(src_file1->full_name, src_file2->full_name) == 0) {
    /* The entities correspond to the same source construct, but resulted in
       incompatible IL (perhaps due to preprocessor effects). */
    a_source_file_ptr  primary_file2 = primary_source_file_for_seq(pos2->seq);
    pos_stsy_error(ec_entity_differs_in_other_trans_unit, &sym->decl_position,
                   primary_file2->name_as_written, sym);
  } else {

    pos_sy_start_error(ec_corresp_decl_incompatible, &sym->decl_position, sym);
    add_diag_info_with_pos_insert(ec_corresp_decl_at, &scp2->decl_position);
    end_error();
  }  /* if */
}  /* f_report_bad_trans_unit_corresp */

#define report_bad_trans_unit_corresp(entity)                        \
  f_report_bad_trans_unit_corresp((char*)entity)


static void f_process_bad_trans_unit_corresp(char  *entity)
/*
Same as f_report_bad_trans_unit_corresp but also clear the correspondence
pointer.
*/
{
  f_report_bad_trans_unit_corresp(entity);
  clear_trans_unit_corresp(entity);
}  /* process_bad_trans_unit_corresp */

#define process_bad_trans_unit_corresp(entity)                        \
  f_process_bad_trans_unit_corresp((char*)entity)


static a_boolean same_parents(a_symbol_ptr  sym1,
                              a_symbol_ptr  sym2)
/*
Return TRUE if and only if the parent (namespace or class) entities of the
given symbols are identical.
*/
{
  a_boolean  result;

  if (sym1->is_class_member != sym2->is_class_member) {
    result = FALSE;
  } else if (sym1->is_class_member) {
    a_type_ptr  parent1 = sym1->parent.class_type;
    a_type_ptr  parent2 = sym2->parent.class_type;
    check_assertion(parent1 != NULL && parent2 != NULL);
    result = (canonical_il_entry_of(parent1) ==
                                              canonical_il_entry_of(parent2));
  } else {
    a_namespace_ptr  parent1 = sym1->parent.namespace_ptr;
    a_namespace_ptr  parent2 = sym2->parent.namespace_ptr;
    if (parent1 == parent2) {
      result = TRUE;
    } else if (parent1 == NULL || parent2 == NULL) {
      result = FALSE;
    } else {
      result = (canonical_il_entry_of(parent1) ==
                                              canonical_il_entry_of(parent2));
    }  /* if */
  }  /* if */
  return result;
}  /* same_parents */


static a_boolean may_have_correspondence(a_symbol_ptr sym)
/*
Return TRUE if the given symbol is associated with an entity that may appear
in multiple translation units so that correspondence pointers between them
need to be determined.
*/
{
  a_boolean        result;
  a_storage_class  storage_class;

  switch (sym->kind) {
    case sk_class_or_struct_tag:
    case sk_class_template:
    case sk_constant:
    case sk_enum_tag:
    case sk_field:
    case sk_function_template:
    case sk_member_function:
    case sk_namespace:
    case sk_static_data_member:
    case sk_type:
    case sk_union_tag:
      result = TRUE;
      break;
    case sk_extern_routine:
    case sk_extern_variable:
    case sk_keyword:
    case sk_label:
    case sk_macro:
    case sk_projection:
    case sk_namespace_projection:
    case sk_undefined:
      result = FALSE;
      break;
    case sk_routine:
      storage_class = sym->variant.routine.ptr->storage_class;
      result = (storage_class == (a_storage_class)sc_extern ||
                storage_class == (a_storage_class)sc_unspecified);
      break;
    case sk_variable:
      storage_class = sym->variant.variable.ptr->storage_class;
      result = (storage_class == (a_storage_class)sc_extern ||
                storage_class == (a_storage_class)sc_unspecified);
      break;
    case sk_overloaded_function:
      result = FALSE;
      {
        /* If any member of the overload set may have a correspondence,
           the whole set should be treated as such. */
        a_symbol_ptr  sub_sym = sym->variant.overloaded_function.symbols;
        for (; sub_sym != NULL; sub_sym = sub_sym->next) {
          if (may_have_correspondence(sub_sym)) {
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
      }
      break;
    default:
      unexpected_condition_str("may_have_correspondence: bad symbol kind");
  }  /* switch */
  return result;
}  /* may_have_correspondence */


static a_boolean type_has_body(a_type_ptr  type)
/*
Return whether a class type has a definition.  Note that is_incomplete_type
returns FALSE for nonprototype nonreal template instantiations even though
no definition is available for such types.
*/
{
  a_boolean  result;

  check_assertion(is_immediate_class_type(type));
  if (C_mode()) {
    result = !is_incomplete_type(type);
  } else {
    result = type->variant.class_struct_union.field_list != NULL ||
             type->variant.class_struct_union.extra_info->assoc_scope != NULL;
  }  /* if */
  return result;
}  /* type_has_body */


static a_field_ptr skip_generated_field(a_field_ptr  field)
/*
If the given field was generated by the front end (e.g., a virtual function
table pointer), skip to the next field that was actually declared in the
source.
*/
{
  
  while (field != NULL && field->source_corresp.decl_position.seq == 0) {
    field = field->next;
  }  /* while */
  return field;
}  /* skip_generated_field */


static a_type_ptr skip_template_types(a_type_ptr  type)
/*
Return the first nontemplate type in the list that starts with the given one
(NULL if none). 
*/
{
  a_type_ptr  result = type;
  while (result != NULL &&
         is_immediate_class_type(result) &&
         is_unspecialized_template_class(result)) {
    result = result->next;
  }  /* while */
  return result;
}  /* skip_template_types */


static a_routine_ptr skip_template_routines(a_routine_ptr  routine)
/*
Return the first nontemplate routine in the list that starts with the given
one (NULL if none). 
*/
{
  a_routine_ptr  result = routine;
  while (result != NULL &&
         result->is_template_function &&
         !result->is_specialized) {
    result = result->next;
  }  /* while */
  return result;
}  /* skip_template_routines */


static void clear_enum_type_correspondence(a_type_ptr  type)
/*
Clear the correspondence pointers in the substructure of an enum type.
*/
{
  a_constant_ptr  enumerator = type->variant.integer.enum_info.constant_list;

  for (; enumerator != NULL; enumerator = enumerator->next) {
    clear_trans_unit_corresp(enumerator);
  }  /* for */
}  /* clear_enum_type_correspondence */


static void clear_class_type_correspondence(a_type_ptr  type)
/*
Clear the correspondence pointers in the substructure of a class type.
*/
{
  if (type_has_body(type)) {
    /* Traverse fields: (both C and C++) */
    a_field_ptr  field = type->variant.class_struct_union.field_list;
    for (; field != NULL; field = field->next) {
      clear_trans_unit_corresp(field);
    }  /* for */
  
    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      /* Traverse member templates: */
      {
        a_template_ptr  templ = scope->templates;
        for (; templ != NULL; templ = templ->next) {
          clear_trans_unit_corresp(templ);
        }  /* for */
      }
    
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = scope->types;
        for (; mem_type != NULL; mem_type = mem_type->next) {
          if (is_immediate_enum_type(mem_type)) {
            clear_enum_type_correspondence(mem_type);
          } else if (is_immediate_class_type(mem_type)) {
            clear_class_type_correspondence(mem_type);
          }  /* if */
          clear_trans_unit_corresp(mem_type);
        }  /* for */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = scope->routines;
        for (;routine != NULL; routine = routine->next) {
          clear_trans_unit_corresp(routine);
        }  /* for */
      }
    
      /* Traverse static data members: */
      {
        a_variable_ptr  variable = scope->variables;
        for (; variable != NULL; variable = variable->next) {
          clear_trans_unit_corresp(variable);
        }  /* for */
      }
    
      /* Traverse member constants: */
      {
        a_constant_ptr  constant = scope->constants;
        for (; constant != NULL; constant = constant->next) {
          clear_trans_unit_corresp(constant);
        }  /* for */
      }
    }  /* if */
  }  /* if */
}  /* clear_class_type_correspondence */


static a_boolean f_verify_name_correspondence(char  *entity1)
/*
Verify that the given entity and the one pointed to by its translation unit
correspondence pointer have the same name (effectively, that their associated
symbols are listed under the same header).
*/
{
  a_boolean  match;
  char       *entity2 = trans_unit_corresp_pointer_of(entity1);
  a_source_correspondence_ptr
             scp1 = (a_source_correspondence_ptr)entity1,
             scp2 = (a_source_correspondence_ptr)entity2;
  a_symbol_header_ptr
             sh1, sh2;
  check_assertion(scp1->assoc_info != NULL && scp2->assoc_info != NULL);
  sh1 = ((a_symbol_ptr)scp1->assoc_info)->header;
  sh2 = ((a_symbol_ptr)scp2->assoc_info)->header;
  match = (sh1 == sh2);
  if (!match) {
    /* This is possible if the associated symbol is not part of the symbol
       table (which is TRUE of template instances). */
    match = !strncmp(sh1->identifier, sh2->identifier,
                     (sh1->identifier_length < sh2->identifier_length) ?
                       sh1->identifier_length : sh2->identifier_length);
  }  /* if */
  return match;
}  /* f_verify_name_correspondence */

#define verify_name_correspondence(ptr)                                \
  f_verify_name_correspondence((char*)ptr)


static a_boolean verify_field_correspondence(a_field_ptr  field)
/*
Check that the recorded translation unit correspondence for the given field
is in fact valid.
*/
{
  a_boolean    match = verify_name_correspondence(field);
  a_field_ptr  corresp_field = (a_field_ptr)canonical_il_entry_of(field);

  if (!match || !identical_types(field->type, corresp_field->type)) {
    match = FALSE;
    process_bad_trans_unit_corresp(field);
  }  /* if */
  return match;
}  /* verify_field_correspondence */


static a_boolean verify_template_correspondence(a_template_ptr  templ)
/*
Check that the recorded translation unit correspondence for the given template
is in fact valid.
*/
{
  a_boolean  match = verify_name_correspondence(templ);
  /* FIXME */
  return match;
}  /* verify_template_correspondence */


static a_boolean verify_routine_correspondence(a_routine_ptr  routine)
/*
Check that the recorded translation unit correspondence for the given routine
is in fact valid.
*/
{
  a_boolean      match = verify_name_correspondence(routine);
  a_routine_ptr  corresp_routine =
                                (a_routine_ptr)canonical_il_entry_of(routine);

  if (!match || !identical_types(routine->type, corresp_routine->type)) {
    match = FALSE;
    process_bad_trans_unit_corresp(routine);
  }  /* if */
  return match;
}  /* verify_routine_correspondence */


static a_boolean verify_variable_correspondence(a_variable_ptr  var)
/*
Check that the recorded translation unit correspondence for the given variable
is in fact valid.
*/
{
  a_boolean       match = verify_name_correspondence(var);
  a_variable_ptr  corresp_var = (a_variable_ptr)canonical_il_entry_of(var);

  if (!match || !identical_types(var->type, corresp_var->type)) {
    match = FALSE;
    process_bad_trans_unit_corresp(var);
  }  /* if */
  return match;
}  /* verify_variable_correspondence */


static a_boolean verify_constant_correspondence(a_constant_ptr  constant)
/*
Check that the recorded translation unit correspondence for the given constant
is in fact valid.
*/
{
  a_boolean       match = verify_name_correspondence(constant);
  a_constant_ptr  corresp_constant =
                              (a_constant_ptr)canonical_il_entry_of(constant);

  if (!match ||
      !identical_types(constant->type, corresp_constant->type) ||
      !eq_constants(constant, corresp_constant)) {
    match = FALSE;
    process_bad_trans_unit_corresp(constant);
  }  /* if */
  return match;
}  /* verify_constant_correspondence */


static a_boolean verify_enum_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given enum
type is in fact valid.
*/
{
  a_boolean       match = TRUE;
  a_type_ptr      corresp_type = (a_type_ptr)canonical_il_entry_of(type);
  a_constant_ptr  enumerator = type->variant.integer.enum_info.constant_list;
  a_constant_ptr  corresp_enumerator = 
                        corresp_type->variant.integer.enum_info.constant_list;

  for (; enumerator != NULL && corresp_enumerator != NULL;
       enumerator = enumerator->next,
                              corresp_enumerator = corresp_enumerator->next) {
    if (!verify_constant_correspondence(enumerator)) {
      process_bad_trans_unit_corresp(enumerator);
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (enumerator != NULL || corresp_enumerator != NULL) {
    report_bad_trans_unit_corresp(type);
    match = FALSE;
  }  /* if */
  if (!match) {
    clear_enum_type_correspondence(type);
  }  /* if */
  return match;
}  /* verify_enum_type_correspondence */


static a_boolean verify_class_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given class
type is in fact valid.
*/
{
  a_boolean   match = verify_name_correspondence(type);
  a_boolean   report_error = FALSE;
  a_type_ptr  corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (!match || !is_immediate_class_type(corresp_type)) {
    match = FALSE;
    report_error = TRUE;
  } else if (!type_has_body(type) || !type_has_body(corresp_type)) {
    /* The types are matching since at least one is incomplete and therefore
       has no inner structure to conflict with. */
  } else if (corresp_type != NULL) {
    /* Traverse fields: (both C and C++) */
    a_field_ptr  field = skip_generated_field(
                                 type->variant.class_struct_union.field_list);
    a_field_ptr  corresp_field = skip_generated_field(corresp_type->
                                       variant.class_struct_union.field_list);
    for (; field != NULL && corresp_field != NULL;
         field = skip_generated_field(field->next),
           corresp_field = skip_generated_field(corresp_field->next)) {
      if (!verify_field_correspondence(field)) {
        match = FALSE;
        break;
      }  /* if */
    }  /* for */
    if ((field != NULL && corresp_field == NULL) ||
        (corresp_field != NULL && field == NULL)) {
      report_error = TRUE;
      match = FALSE;
    }  /* if */

    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      a_scope_ptr  corresp_scope = corresp_type->
                           variant.class_struct_union.extra_info->assoc_scope;
  
      /* Traverse member templates: */
      {
        a_template_ptr  templ = scope->templates;
        a_template_ptr  corresp_templ = corresp_scope->templates;
        for (; templ != NULL && corresp_templ != NULL;
             templ = templ->next, corresp_templ = corresp_templ->next) {
          if (!verify_template_correspondence(templ)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if ((templ != NULL && corresp_templ == NULL) ||
            (corresp_templ != NULL && templ == NULL)) {
          report_error = TRUE;
          match = FALSE;
        }  /* if */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = scope->types;
        a_type_ptr  corresp_mem_type = corresp_scope->types;
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = skip_template_types(mem_type->next),
             corresp_mem_type = skip_template_types(corresp_mem_type->next)) {
          if (!verify_type_correspondence(mem_type)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if ((mem_type != NULL && corresp_mem_type == NULL) ||
            (corresp_mem_type != NULL && mem_type == NULL)) {
          report_error = TRUE;
          match = FALSE;
        }  /* if */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = scope->routines;
        a_routine_ptr  corresp_routine = corresp_scope->routines;
        for (; routine != NULL && corresp_routine != NULL;
             routine = skip_template_routines(routine->next),
             corresp_routine = skip_template_routines(corresp_routine->next)) {
          if (!verify_routine_correspondence(routine)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if ((routine != NULL && corresp_routine == NULL) ||
            (corresp_routine != NULL && routine == NULL)) {
          report_error = TRUE;;
          match = FALSE;
        }  /* if */
      }
      /* Traverse static data members: */
      {
        a_variable_ptr  variable = scope->variables;
        a_variable_ptr  corresp_variable = corresp_scope->variables;
        for (; variable != NULL && corresp_variable != NULL;
             variable = variable->next,
                                  corresp_variable = corresp_variable->next) {
          if (!verify_variable_correspondence(variable)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if ((variable != NULL && corresp_variable == NULL) ||
            (corresp_variable != NULL && variable == NULL)) {
          report_error = TRUE;;
          match = FALSE;
        }  /* if */
      }
      /* Traverse member constants: */
      {
        a_constant_ptr  constant = scope->constants;
        a_constant_ptr  corresp_constant = corresp_scope->constants;
        for (; constant != NULL && corresp_constant != NULL;
             constant = constant->next,
                                  corresp_constant = corresp_constant->next) {
          if (!verify_constant_correspondence(constant)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if ((constant != NULL && corresp_constant == NULL) ||
            (corresp_constant != NULL && constant == NULL)) {
          report_error = TRUE;;
          match = FALSE;
        }  /* if */
      }
    }  /* if */
  }  /* if */
  if (!match) {
    if (report_error) {
      report_bad_trans_unit_corresp(type);
    }  /* if */
    clear_class_type_correspondence(type);
  }  /* if */
  return match;
}  /* verify_class_type_correspondence */


static a_boolean verify_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given type
is in fact valid.
*/
{
  a_boolean   match;
  a_type_ptr  corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (type->kind != corresp_type->kind || !verify_name_correspondence(type)) {
    match = FALSE;
    process_bad_trans_unit_corresp(type);
  } else if (is_immediate_class_type(type)) {
    /* corresp_type is also a class type since the type kinds are identical. */
    match = verify_class_type_correspondence(type);
  } else if (is_immediate_enum_type(type) &&
             is_immediate_enum_type(corresp_type)) {
    match = verify_enum_type_correspondence(type);
  } else {
    match = identical_types(type, corresp_type);
  }  /* if */
  return match;
}  /* verify_type_correspondence */


static a_boolean verify_namespace_correspondence(a_namespace_ptr  nsp)
/*
If the given namespace has a canonical entry in another translation unit,
and assuming it already matches in name, kind and scope attributes, verify
that any other significant attributes also match.  Only namespace aliases
have such an attribute: the aliased namespace.
*/
{
  a_boolean  result = TRUE;

  if (nsp->is_namespace_alias) {
    a_namespace_ptr  other_nsp = (a_namespace_ptr)canonical_il_entry_of(nsp);
    if (canonical_il_entry_of(skip_namespace_aliases(nsp)) !=
                   canonical_il_entry_of(skip_namespace_aliases(other_nsp))) {
      a_symbol_ptr  nsp_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
      a_symbol_ptr  sym = (a_symbol_ptr)other_nsp->source_corresp.assoc_info;

      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &nsp_sym->decl_position, sym);
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* verify_namespace_correspondence */


static void verify_namespace_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of namespaces of the given scope and verify a
translation unit correspondence pointer for each of them.
*/
{
  a_namespace_ptr  nsp;

  /* Visit all namespaces. */
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    if (has_name(nsp)) {
      if (!verify_namespace_correspondence(nsp)) {
        /* Some error occurred---clear the association. */
        clear_trans_unit_corresp(nsp);
      } else if (!nsp->is_namespace_alias) {
        /* Verify correspondences for nested namespaces. */
        verify_trans_unit_correspondences_for_scope(
                                                    nsp->variant.assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* verify_namespace_correspondences_for_scope */


static void verify_type_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of types of the given scope and verify a translation unit
correspondence pointer for each of them.
*/
{
  a_type_ptr  type;

  /* Visit all types. */
  for (type = scope->types; type != NULL; type = type->next) {
    a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;

    /* Note that placeholder types do not have an associated symbol. */
    if (type_sym != NULL && may_have_correspondence(type_sym)) {
      if (trans_unit_corresp_pointer_of(type) != NULL &&
          !verify_type_correspondence(type)) {
        /* Some error occurred---clear the association. */
        clear_trans_unit_corresp(type);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* verify_type_correspondences_for_scope */


static void verify_routine_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of routines of the given scope and verify a translation
unit correspondence pointer for each of them.
*/
{
  a_routine_ptr  routine;

  /* Visit all routines. */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    if (trans_unit_corresp_pointer_of(routine) != NULL &&
        !verify_routine_correspondence(routine)) {
      /* Some error occurred---clear the association. */
      clear_trans_unit_corresp(routine);
    }  /* if */
  }  /* for */
}  /* verify_routine_correspondences_for_scope */


static void verify_variable_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of variables of the given scope and verify a translation
unit correspondence pointer for each of them.
*/
{
  a_variable_ptr  variable;

  /* Visit all variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    if (trans_unit_corresp_pointer_of(variable) != NULL &&
        !verify_variable_correspondence(variable)) {
      /* Some error occurred---clear the association. */
      clear_trans_unit_corresp(variable);
    }  /* if */
  }  /* for */
}  /* verify_variable_correspondences_for_scope */


static void establish_trans_unit_correspondences_for_enum(a_type_ptr  type)
/*
Establish correspondences for the list of constants associated with the
given enum type.
*/
{
  a_type_ptr      corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (corresp_type != NULL) {
    a_constant_ptr  enumerator = type->variant.integer.enum_info.constant_list;
    a_constant_ptr  corresp_enumerator = 
                        corresp_type->variant.integer.enum_info.constant_list;

    for (; enumerator != NULL && corresp_enumerator != NULL;
         enumerator = enumerator->next,
                              corresp_enumerator = corresp_enumerator->next) {
      record_trans_unit_corresp(enumerator, corresp_enumerator);
    }
  }  /* for */
}  /* establish_trans_unit_correspondences_for_enum */


static void establish_trans_unit_correspondences_for_class(a_type_ptr  type)
/*
Set the correspondence pointers in the members of a type.  The members' types
are not checked.
*/
{
  a_type_ptr  corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (corresp_type != NULL && is_immediate_class_type(corresp_type) &&
      type_has_body(type) && type_has_body(corresp_type)) {
    /* Traverse fields: */
    {
      a_field_ptr  field = skip_generated_field(
                                 type->variant.class_struct_union.field_list);
      a_field_ptr  corresp_field = skip_generated_field(corresp_type->
                                       variant.class_struct_union.field_list);
      for (; field != NULL && corresp_field != NULL;
           field = skip_generated_field(field->next),
             corresp_field = skip_generated_field(corresp_field->next)) {
        record_trans_unit_corresp(field, corresp_field);
      }  /* for */
    }

    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      a_scope_ptr  corresp_scope = corresp_type->
                           variant.class_struct_union.extra_info->assoc_scope;
    
      /* Traverse member templates: */
      {
        a_template_ptr  templ = scope->templates;
        a_template_ptr  corresp_templ = corresp_scope->templates;
        for (; templ != NULL && corresp_templ != NULL;
             templ = templ->next, corresp_templ = corresp_templ->next) {
          record_trans_unit_corresp(templ, corresp_templ);
        }  /* for */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = scope->types;
        a_type_ptr  corresp_mem_type = corresp_scope->types;
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = mem_type->next,
                                  corresp_mem_type = corresp_mem_type->next) {
          record_trans_unit_corresp(mem_type, corresp_mem_type);
          if (is_immediate_class_type(mem_type)) {
            establish_trans_unit_correspondences_for_class(mem_type);
            /* This could be a member of a template class.  If we're dealing
               with a prototype instantiation, this is a good opportunity to
               get to any a_template entry associated with an out-of-class
               definition of the nested class. */
            if (mem_type
                    ->variant.class_struct_union.is_prototype_instantiation) {
              a_symbol_ptr  sym = (a_symbol_ptr)mem_type
                                                  ->source_corresp.assoc_info;
              a_symbol_ptr  corresp_sym = (a_symbol_ptr)corresp_mem_type
                                                  ->source_corresp.assoc_info;
              a_template_symbol_supplement_ptr
                            tssp = template_supplement_for_symbol(sym),
                            corresp_tssp =
                                  template_supplement_for_symbol(corresp_sym);
              if (tssp != NULL && corresp_tssp != NULL &&
                  tssp->il_template_entry != NULL &&
                  corresp_tssp->il_template_entry != NULL) {
                record_trans_unit_corresp(tssp->il_template_entry,
                                          corresp_tssp->il_template_entry);
              }  /* if */
            }  /* if */
          } else if (is_immediate_enum_type(mem_type)) {
            establish_trans_unit_correspondences_for_enum(mem_type);
          }  /* if */
        }  /* for */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = scope->routines;
        a_routine_ptr  corresp_routine = corresp_scope->routines;
        for (; routine != NULL && corresp_routine != NULL;
             routine = routine->next,
                                    corresp_routine = corresp_routine->next) {
          record_trans_unit_corresp(routine, corresp_routine);
          /* This could be a member of a template class.  If we're dealing
             with a prototype instantiation, this is a good opportunity to
             get to any a_template entry associated with an out-of-class
             definition of the member function. */
          if (routine->is_prototype_instantiation &&
              corresp_routine->is_prototype_instantiation) {
            a_symbol_ptr  sym = (a_symbol_ptr)routine
                                                  ->source_corresp.assoc_info,
                          corresp_sym = (a_symbol_ptr)corresp_routine
                                                  ->source_corresp.assoc_info;
            a_template_symbol_supplement_ptr
                          tssp = template_supplement_for_symbol(sym),
                          corresp_tssp =
                                  template_supplement_for_symbol(corresp_sym);
            if (tssp != NULL && corresp_tssp != NULL &&
                tssp->il_template_entry != NULL &&
                corresp_tssp->il_template_entry != NULL) {
              record_trans_unit_corresp(tssp->il_template_entry,
                                        corresp_tssp->il_template_entry);
            }  /* if */
          }  /* if */
        }  /* for */
      }
      /* Traverse static data members: */
      {
        a_variable_ptr  var = scope->variables;
        a_variable_ptr  corresp_var = corresp_scope->variables;
        for (; var != NULL && corresp_var != NULL;
             var = var->next, corresp_var = corresp_var->next) {
          record_trans_unit_corresp(var, corresp_var);
          if (type->variant.class_struct_union.is_prototype_instantiation) {
            a_symbol_ptr  sym = (a_symbol_ptr)var->source_corresp.assoc_info;
            a_symbol_ptr  corresp_sym = (a_symbol_ptr)corresp_var
                                                  ->source_corresp.assoc_info;
            a_template_symbol_supplement_ptr
                          tssp = template_supplement_for_symbol(sym),
                          corresp_tssp =
                                  template_supplement_for_symbol(corresp_sym);
            if (tssp != NULL && corresp_tssp != NULL &&
                tssp->il_template_entry != NULL &&
                corresp_tssp->il_template_entry != NULL) {
              record_trans_unit_corresp(tssp->il_template_entry,
                                        corresp_tssp->il_template_entry);
            }  /* if */
          }  /* if */
        }  /* for */
      }
      /* Traverse member constants: */
      {
        a_constant_ptr  constant = scope->constants;
        a_constant_ptr  corresp_constant = corresp_scope->constants;
        for (; constant != NULL && corresp_constant != NULL;
             constant = constant->next,
                                  corresp_constant = corresp_constant->next) {
          record_trans_unit_corresp(constant, corresp_constant);
        }  /* for */
      }
    }  /* if */
  }  /* if */
}  /* establish_trans_unit_correspondences_for_class */


static void find_namespace_correspondence(a_namespace_ptr  nsp)
/*
Look for the given namespace in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  nsp_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(nsp_sym != NULL);
  sym = nsp_sym->header->inactive_symbols;
  for (; sym != NULL; sym = sym->next) {
    if (sym->decl_scope != nsp_sym->decl_scope &&
        may_have_correspondence(sym) &&
        same_parents(sym, nsp_sym)) {
      /* Two different declarations in the same namespace and with the same
         name: they should probably match up. */
      if (is_namespace_symbol(sym) &&
          sym->variant.namespace_info.ptr->is_namespace_alias ==
                                                    nsp->is_namespace_alias) {
        /* Record the correspondence. */
        record_trans_unit_corresp(nsp, sym->variant.namespace_info.ptr);
        break;
      } else {
        /* An error since the conflicting entity has external linkage. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &nsp_sym->decl_position, sym);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* find_namespace_correspondence */


static void find_type_correspondence(a_type_ptr  type)
/*
Look for the given type in another translation unit and set the translation
unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(type_sym != NULL);
  sym = type_sym->header->inactive_symbols;
  for (; sym != NULL; sym = sym->next) {
    if (sym->decl_scope != type_sym->decl_scope &&
        may_have_correspondence(sym) &&
        same_parents(sym, type_sym)) {
      /* Two different declarations in the same namespace and with the same
         name: they should probably match up. */
      if (sym->kind == type_sym->kind) {
        /* Record the correspondence. */
        record_trans_unit_corresp(type, type_symbol_type(sym));
        if (is_immediate_class_type(type)) {
          establish_trans_unit_correspondences_for_class(type);
        } else if (is_immediate_enum_type(type)) {
          establish_trans_unit_correspondences_for_enum(type);
        }  /* if */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* find_type_correspondence */


static void record_class_template_instantiation(a_symbol_ptr  inst)
/*
Search for an instantiation that corresponds to inst in a prior translation
unit.  If there is one, record a correspondence pointer; otherwise, add
the instantiation to the list of instantiations in the associated template
symbol supplement.
*/
{
  a_template_symbol_supplement_ptr
                  tssp = inst->variant.class_struct_union.extra_info
                             ->class_template
                             ->variant.template_info;
  a_template_ptr  templ = tssp->il_template_entry,
                  corresp_templ = (a_template_ptr)canonical_il_entry_of(templ);
  a_template_symbol_supplement_ptr
                  corresp_tssp =
                       ((a_symbol_ptr)corresp_templ->source_corresp.assoc_info)
                         ->variant.template_info;
  a_symbol_list_entry_ptr
                  sym_entry = corresp_tssp->all_instantiations;
  a_type_ptr      class_type = type_symbol_type(inst);
  a_template_arg_ptr
                  templ_args = class_type
                    ->variant.class_struct_union.extra_info->template_arg_list;

  if (sym_entry == NULL &&
      corresp_tssp->variant.class_template.instantiations != NULL) {
    /* FIXME This should only happen when the corresponding template is in
       the primary translation unit.  Probably a pass should be made to
       copy the instantiations list if necessary. */
    a_symbol_ptr  sym = corresp_tssp->variant.class_template.instantiations;
    for (; sym != NULL; sym = next_instance_sym(sym)) {
      sym_entry = alloc_symbol_list_entry();
      sym_entry->next = corresp_tssp->all_instantiations;
      corresp_tssp->all_instantiations = sym_entry;
      sym_entry->symbol = sym;
    }  /* for */
  }  /* if */
  for (; sym_entry != NULL; sym_entry = sym_entry->next) {
    a_type_ptr  corresp_type = type_symbol_type(sym_entry->symbol);
    if (equiv_template_arg_lists(corresp_type
                                   ->variant.class_struct_union.extra_info
                                   ->template_arg_list,
                                 templ_args, ETA_NO_OPTIONS)) {
      record_trans_unit_corresp(class_type, corresp_type);
      establish_trans_unit_correspondences_for_class(class_type);
      break;
    }  /* if */
  }  /* for */
  if (sym_entry == NULL) {
    /* The instantiation was not found on the canonical list.  Add it now. */
    a_symbol_list_entry_ptr  slep = alloc_symbol_list_entry();
    slep->next = corresp_tssp->all_instantiations;
    corresp_tssp->all_instantiations = slep;
    slep->symbol = inst;
  }  /* if */
}  /* record_class_template_instantiation */


static void record_function_template_instantiation(
                                                a_template_instance_ptr  inst)
/*
Search for an instantiation that corresponds to inst in a prior translation
unit.  If there is one, record a correspondence pointer; otherwise, add
the instantiation to the list of instantiations in the associated template
symbol supplement.
*/
{
  a_template_symbol_supplement_ptr
                  tssp = inst->template_sym->variant.template_info;
  a_template_ptr  templ = tssp->il_template_entry,
                  corresp_templ = (a_template_ptr)canonical_il_entry_of(templ);
  a_template_symbol_supplement_ptr
                  corresp_tssp =
                       ((a_symbol_ptr)corresp_templ->source_corresp.assoc_info)
                         ->variant.template_info;
  a_symbol_list_entry_ptr
                  sym_entry = corresp_tssp->all_instantiations;
  a_routine_ptr   routine = inst->instance_sym->variant.routine.ptr;
  a_template_arg_ptr
                  templ_args = routine->template_arg_list;

  if (sym_entry == NULL &&
      corresp_tssp->variant.function.instantiations != NULL) {
    /* FIXME This should only happen when the corresponding template is in
       the primary translation unit.  Probably a pass should be made to
       copy the instantiations list if necessary. */
    a_template_instance_ptr  ip = corresp_tssp
                                            ->variant.function.instantiations;
    for (; ip != NULL; ip = ip->next) {
      sym_entry = alloc_symbol_list_entry();
      sym_entry->next = corresp_tssp->all_instantiations;
      corresp_tssp->all_instantiations = sym_entry;
      sym_entry->symbol = ip->instance_sym;
    }  /* for */
  }  /* if */
  for (; sym_entry != NULL; sym_entry = sym_entry->next) {
    a_routine_ptr  corresp_routine = sym_entry->symbol->variant.routine.ptr;
    if (identical_types(routine->type, corresp_routine->type) &&
        equiv_template_arg_lists(corresp_routine->template_arg_list,
                                 templ_args, ETA_NO_OPTIONS)) {
      record_trans_unit_corresp(routine, corresp_routine);
      break;
    }  /* if */
  }  /* for */
  if (sym_entry == NULL) {
    /* The instantiation was not found on the canonical list.  Add it now. */
    a_symbol_list_entry_ptr  slep = alloc_symbol_list_entry();
    slep->next = corresp_tssp->all_instantiations;
    corresp_tssp->all_instantiations = slep;
    slep->symbol = inst->instance_sym;
  }  /* if */
}  /* record_function_template_instantiation */


void record_instantiation(a_symbol_ptr  inst)
/*
Check if the given instantiation has a corresponding entry in another
translation unit and record the correspondence if so.  If not, add the
instantiation to the list of all instantiations of the corresponding
template.
*/
{
  if (is_primary_translation_unit) {
    a_template_symbol_supplement_ptr  tssp;
    a_symbol_list_entry_ptr           sym_entry;

    if (is_class_struct_union_symbol(inst)) {
      a_symbol_ptr  proto_sym = inst->variant.class_struct_union.extra_info
                                    ->corresp_prototype_sym;
      tssp = template_supplement_for_symbol(proto_sym);
    } else if (is_function_symbol(inst)) {
      tssp = template_supplement_for_symbol(inst->variant.routine.instance_ptr
                                                ->template_sym);
    } else {
      check_assertion(inst->kind == (a_symbol_kind)sk_static_data_member);
      tssp = template_supplement_for_symbol(
                 inst->variant.static_data_member.instance_ptr->template_sym);
    }  /* if */
    sym_entry = alloc_symbol_list_entry();
    sym_entry->next = tssp->all_instantiations;
    tssp->all_instantiations = sym_entry;
    sym_entry->symbol = inst;
  } else {
#if 0 /* FIXME: need code to handle members of templates. */
    if (is_class_struct_union_tag(inst)) {
      record_class_template_instantiation(inst);
    } else if (is_function_symbol(inst)) {
      record_function_template_instantiation(
                                          inst->variant.routine.instance_ptr);
    }  /* if */
#endif /* FIXME */
  }  /* if */
}  /* record_instantiation */


static void establish_instantiation_correspondences(a_template_ptr  templ)
/*
Find correspondences for every instantiation of the given template.
This routine should only be called for templates that have an associated
sk_class_template or sk_function_template symbol.  Other template entries
correspond to class members (e.g., a member function of a class template)
and are handled elsewhere.
*/
{
  a_symbol_ptr    templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  a_template_symbol_supplement_ptr
                  tssp = templ_sym->variant.template_info;

  if (templ_sym->kind == (a_symbol_kind)sk_class_template) {
    a_type_ptr    class_type = tssp
                              ->variant.class_template.prototype_instantiation
                              ->variant.class_struct_union.type;
    a_symbol_ptr  inst = tssp->variant.class_template.instantiations,
                  proto_inst;
    for (; inst != NULL; inst = next_instance_sym(inst)) {
      record_class_template_instantiation(inst);
    }  /* for */
    /* Also process the prototype instantiation. */
    proto_inst = ((a_symbol_ptr)((a_template_ptr)canonical_il_entry_of(templ))
                   ->source_corresp.assoc_info)
                     ->variant.template_info
                     ->variant.class_template.prototype_instantiation;
    /* For instantiations from template template parameters proto_inst will
       be NULL. */
    if (proto_inst != NULL) {
      record_trans_unit_corresp(class_type,
                                proto_inst->variant.class_struct_union.type);
      establish_trans_unit_correspondences_for_class(class_type);
    }  /* if */
  } else if (templ_sym->kind == (a_symbol_kind)sk_function_template) {
    a_template_instance_ptr  inst = tssp->variant.function.instantiations;
    for (; inst != NULL; inst = inst->next) {
      record_function_template_instantiation(inst);
    }  /* for */
    /* Also process prototype instantiation. */
    record_trans_unit_corresp(tssp->variant.function.routine,
                              ((a_symbol_ptr)
                                ((a_template_ptr)canonical_il_entry_of(templ))
                                  ->source_corresp.assoc_info)
                                ->variant.template_info
                                ->variant.function.routine);
  } else {
    unexpected_condition_str("Bad symbol");
  }  /* if */
}  /* establish_instantiation_correspondences */


static a_template_ptr find_corresp_class_template(a_template_ptr  templ,
                                                  a_symbol_ptr    sym)
/*
Find a class template from another translation unit corresponding to the given
class template templ.  However, only consider sym and its subordinate symbols
when looking up a correspondence: if none is found, return NULL.
*/
{
  a_template_ptr  corresp_templ = NULL;
  a_symbol_ptr    templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  a_template_symbol_supplement_ptr
                  tssp = template_supplement_for_symbol(templ_sym),
                  corresp_tssp = template_supplement_for_symbol(sym);

  /* The symbol "sym" always corresponds to a primary symbol. */
  check_assertion(
           corresp_tssp->variant.class_template.primary_template_sym == NULL);
  if (tssp->variant.class_template.primary_template_sym != NULL) {
    /* The given template is a partial specialization: look for a partial
       specialization with the same set of parameters and arguments. */
    for (sym = corresp_tssp->variant.class_template.partial_specializations;
         sym != NULL;
         sym = sym->next) {
      corresp_tssp = template_supplement_for_symbol(sym);
      if (equiv_template_param_lists(corresp_tssp->cache.decl_info->parameters,
                                     tssp->cache.decl_info->parameters,
                                     /*issue_errors=*/FALSE,
                                     &templ_sym->decl_position)) {
        /* The template parameters correspond; now check the arguments: they
           are attached to the prototype instantiation. */
        a_symbol_ptr  proto, corresp_proto;
        proto = tssp->variant.class_template.prototype_instantiation;
        corresp_proto =
                 corresp_tssp->variant.class_template.prototype_instantiation;
        if (equiv_template_arg_lists(
                  type_symbol_type(proto)->
                     variant.class_struct_union.extra_info->template_arg_list,
                  type_symbol_type(corresp_proto)->
                     variant.class_struct_union.extra_info->template_arg_list,
                  ETA_NO_OPTIONS)) {
          corresp_templ = corresp_tssp->il_template_entry;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  } else {
    /* This is a primary template: the template parameters must match. */
    if (equiv_template_param_lists(corresp_tssp->cache.decl_info->parameters,
                                   tssp->cache.decl_info->parameters,
                                   /*issue_errors=*/TRUE,
                                   &templ_sym->decl_position)) {
      corresp_templ = corresp_tssp->il_template_entry;
    }  /* if */
  }  /* if */
  return corresp_templ;
}  /* find_corresp_class_template */


static a_template_ptr find_corresp_function_template(a_template_ptr  templ,
                                                     a_symbol_ptr    sym)
/*
Find a function template from another translation unit corresponding to the
given class template templ.  However, only consider sym and its subordinate
symbols when looking up a correspondence: if none is found, return NULL.
*/
{
  a_template_ptr  corresp_templ = NULL;
  a_boolean       is_list =
                         (sym->kind == (a_symbol_kind)sk_overloaded_function);
  a_symbol_ptr    sub_sym = is_list ? sym->variant.overloaded_function.symbols
                                    : sym;
  a_symbol_ptr    templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  a_template_symbol_supplement_ptr
                  tssp = template_supplement_for_symbol(templ_sym),
                  corresp_tssp;
  a_routine_ptr   routine = tssp->variant.function.routine,
                  corresp_routine;

  /* If necessary, iterate over every overloaded function template. */
  for (; sub_sym != NULL; sub_sym = is_list ? sub_sym->next : NULL) {
    if (sub_sym->kind != (a_symbol_kind)sk_function_template) continue;
    corresp_tssp = template_supplement_for_symbol(sub_sym);
    corresp_routine = corresp_tssp->variant.function.routine;
    if (equiv_template_param_lists(corresp_tssp->cache.decl_info->parameters,
                                   tssp->cache.decl_info->parameters,
                                   /*issue_errors=*/FALSE,
                                   &templ_sym->decl_position) &&
        identical_types(routine->type, corresp_routine->type) &&
        equiv_template_arg_lists(routine->template_arg_list,
                                 corresp_routine->template_arg_list,
                                 ETA_NO_OPTIONS)) {
      
      corresp_templ = corresp_tssp->il_template_entry;
      break;
    }  /* if */
  }  /* for */
  return corresp_templ;
}  /* find_corresp_function_template */


static void find_template_correspondence(a_template_ptr  templ)
/*
Look for the given type in another translation unit and set the translation
unit correspondence pointer if one is found.
*/
{
  a_boolean     conflict = FALSE;
  a_symbol_ptr  templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(templ_sym != NULL);
  sym = templ_sym->header->inactive_symbols;
  for (; sym != NULL; sym = sym->next) {
    if (sym->decl_scope != templ_sym->decl_scope &&
        may_have_correspondence(sym) &&
        same_parents(sym, templ_sym)) {
      /* Two different declarations in the same namespace and with the same
         name: they should probably match up. */
      if (is_class_template_symbol(sym) ==
                                        is_class_template_symbol(templ_sym)) {
        a_template_ptr  corresp_templ;
        if (is_class_template_symbol(templ_sym)) {
          corresp_templ = find_corresp_class_template(templ, sym);
        } else {
          corresp_templ = find_corresp_function_template(templ, sym);
        }  /* if */
        if (corresp_templ != NULL) {
          /* Record the correspondence. */
          record_trans_unit_corresp(templ, corresp_templ);
          establish_instantiation_correspondences(templ);
          break;
        }  /* if */
      } else {
        /* An error if the conflicting entity has external linkage. */
        conflict = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  if (conflict) {
    pos_sy_error(ec_not_compatible_with_previous_decl,
                 &templ_sym->decl_position, sym);
  }  /* if */
}  /* find_template_correspondence */


static void find_routine_correspondence(a_routine_ptr  routine)
/*
Look for the given routine in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  routine_sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(routine_sym != NULL);
  sym = routine_sym->header->inactive_symbols;
  for (; sym != NULL; sym = sym->next) {
    /* Don't consider symbols in the same file. */
    if (sym->decl_scope != routine_sym->decl_scope) {
      a_boolean  is_list = (sym->kind ==
                                       (a_symbol_kind)sk_overloaded_function);
      a_symbol_ptr  sub_sym = is_list ?
                               sym->variant.overloaded_function.symbols : sym;
      for (; sub_sym != NULL; sub_sym = is_list ? sub_sym->next : NULL) {
        if (may_have_correspondence(sub_sym) &&
            same_parents(sub_sym, routine_sym)) {
          /* Two different declarations in the same namespace or class, and
             with the same name: they should probably match up. */
          switch (sub_sym->kind) {
            case sk_routine:
            case sk_member_function:
              {
                a_type_ptr  sym_type = routine_symbol_type(sub_sym);
                if (identical_types(sym_type, routine->type)) {
                  /* Record the correspondence. */
                  record_trans_unit_corresp(routine,
                                            sub_sym->variant.routine.ptr);
                }  /* if */
              }
              break;
            case sk_function_template:
            case sk_class_or_struct_tag:
            case sk_union_tag:
            case sk_enum_tag:
              /* No conflict. */
              break;
            case sk_type:
              if (sym->variant.type.is_injected_class_name) break;
              /* FALLTHROUGH */
            default:
              pos_sy_error(ec_not_compatible_with_previous_decl,
                           &routine_sym->decl_position, sub_sym);
          }  /* switch */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
}  /* find_routine_correspondence */


static void find_variable_correspondence(a_variable_ptr  var)
/*
Look for the given variable in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  var_sym = (a_symbol_ptr)var->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(var_sym != NULL);
  sym = var_sym->header->inactive_symbols;
  for (; sym != NULL; sym = sym->next) {
    /* Don't consider symbols in the same file. */
    if (sym->decl_scope != var_sym->decl_scope &&
        may_have_correspondence(sym) &&
        same_parents(sym, var_sym)) {
      /* Two different declarations in the same namespace or class, and
         with the same name: they should probably match up. */
      switch (sym->kind) {
        case sk_variable:
          /* Record the correspondence. */
          record_trans_unit_corresp(var, sym->variant.variable.ptr);
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
        case sk_enum_tag:
          break;
        case sk_type:
          if (sym->variant.type.is_injected_class_name) break;
          /* FALLTHROUGH */
        default:
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &var_sym->decl_position, sym);
      }  /* switch */
    }  /* if */
  }  /* for */
}  /* find_variable_correspondence */


void establish_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
/*
Establish correspondences for all the applicable entities in the given
scope.  The process is repeated in nested class and namespace scopes.
*/
{
  /* Visit all namespaces. */
  {
    a_namespace_ptr  nsp;
    for (nsp = scope->namespaces;
         nsp != NULL;
         nsp = nsp->next) {
      if (has_name(nsp)) {
        find_namespace_correspondence(nsp);
        if (!nsp->is_namespace_alias) {
          /* Establish correspondences for nested namespaces. */
          establish_trans_unit_correspondences_for_scope(
                                                    nsp->variant.assoc_scope);
        }  /* if */
      }  /* if */
    }  /* for */
  }

  /* Visit all templates. */
  {
    a_template_ptr  templ;
    for (templ = scope->templates; templ != NULL; templ = templ->next) {
      find_template_correspondence(templ);
    }  /* for */
  }

  /* Visit all types. */
  {
    a_type_ptr  type;
    for (type = scope->types;
         type != NULL;
         type = skip_template_types(type->next)) {
      a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;

      /* Note that placeholder types do not have an associated symbol. */
      if (type_sym != NULL && may_have_correspondence(type_sym)) {
        find_type_correspondence(type);
      }  /* if */
    }  /* for */
  }

  /* Visit all routines. */
  {
    a_routine_ptr  routine;
    for (routine = scope->routines;
         routine != NULL;
         routine = skip_template_routines(routine->next)) {
      find_routine_correspondence(routine);
    }  /* for */
  }

  /* Visit all variables. */
  {
    a_variable_ptr  var;
    for (var = scope->variables; var != NULL; var = var->next) {
      find_variable_correspondence(var);
    }  /* for */
  }
}  /* establish_trans_unit_correspondences_for_scope */


void verify_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
/*
Verify correspondences for all the applicable entities in the given
scope.  The process is repeated in nested scopes.
*/
{
  verify_namespace_correspondences_for_scope(scope);
  verify_type_correspondences_for_scope(scope);
  verify_routine_correspondences_for_scope(scope);
  verify_variable_correspondences_for_scope(scope);
}  /* establish_trans_unit_correspondences_for_scope */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
