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

trans_unit.c -- Translation unit management routines.

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
if the entry is from a primary translation unit of if it has not corresponding
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
}  /*  */

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

    conv_seq_to_file_and_line(scp1->decl_position.seq, &file_name, &full_name,
                              &line, &at_end_of_source);
    fprintf(f_debug, "DBG> %s in file %s (line %ld) should correspond to ",
            (scp1->name != NULL) ? scp1->name : "<unnamed>", file_name, line);
    conv_seq_to_file_and_line(scp2->decl_position.seq, &file_name, &full_name,
                              &line, &at_end_of_source);
    fprintf(f_debug, "%s in file %s (line %ld).\n",
            (scp2->name != NULL) ? scp1->name : "<unnamed>", file_name, line);
  }  /* if */
#endif /* DEBUG */
}  /* f_record_trans_unit_corresp */

#define record_trans_unit_corresp(entity1, entity2)                    \
  f_record_trans_unit_corresp((char*)entity1, (char*)entity2)


static void f_process_bad_trans_unit_corresp(char *entity)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence, but it points to a node
that does not actually correspond to the given entity.  Therefore, issue a
diagnostic and clear the correspondence pointer.
*/
{
  a_symbol_ptr  sym = (a_symbol_ptr)((a_source_correspondence_ptr)entity)->
                                                                   assoc_info;
  char          *corresp_entity = trans_unit_corresp_pointer_of(entity);

  pos_sy_start_error(ec_corresp_decl_incompatible, &sym->decl_position, sym);
  add_diag_info_with_pos_insert(
               ec_corresp_decl_at,
               &((a_source_correspondence_ptr)corresp_entity)->decl_position);
  end_error();
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
    case sk_keyword:
    case sk_label:
    case sk_macro:
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
Clear the correspondence pointers in the substructure of an enum type.
*/
{
  a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;

  /* Traverse fields: */
  {
    a_field_ptr  field = type->variant.class_struct_union.field_list;
    for (; field != NULL; field = field->next) {
      clear_trans_unit_corresp(field);
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
}  /* clear_class_type_correspondence */


static a_boolean f_verify_name_correspondence(char  *entity1)
/*
Verify that the given entity and the one pointed to by its translation unit
correspondence pointer have the same name (effectively, that their associated
symbols are listed under the same header).
*/
{
  char  *entity2 = trans_unit_corresp_pointer_of(entity1);
  a_source_correspondence_ptr
        scp1 = (a_source_correspondence_ptr)entity1,
        scp2 = (a_source_correspondence_ptr)entity2;

  check_assertion(scp1->assoc_info != NULL && scp2->assoc_info != NULL);
  return ((a_symbol_ptr)scp1->assoc_info)->header ==
                                     ((a_symbol_ptr)scp2->assoc_info)->header;
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
    record_trans_unit_corresp(enumerator, corresp_enumerator);
    if (!verify_constant_correspondence) {
      process_bad_trans_unit_corresp(enumerator);
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (enumerator != NULL || corresp_enumerator != NULL) {
    process_bad_trans_unit_corresp(type);
    match = FALSE;
  }  /* if */
  if (!match) {
    clear_enum_type_correspondence(type);
  }  /* if */
  return match;
}  /* verify_enum_type_correspondence */


static a_boolean verify_class_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given enum
type is in fact valid.
*/
{
  a_boolean   match = verify_name_correspondence(type);
  a_type_ptr  corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (!match) {
     process_bad_trans_unit_corresp(type);
  } else {
    if (corresp_type != NULL && is_immediate_class_type(corresp_type)) {
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      a_scope_ptr  corresp_scope = corresp_type->
                           variant.class_struct_union.extra_info->assoc_scope;
  
      /* Traverse fields: */
      {
        a_field_ptr  field = type->variant.class_struct_union.field_list;
        a_field_ptr  corresp_field = corresp_type->
                                        variant.class_struct_union.field_list;
        for (; field != NULL && corresp_field != NULL;
             field = field->next, corresp_field = corresp_field->next) {
          record_trans_unit_corresp(field, corresp_field);
          if (!verify_field_correspondence(field)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (field != NULL && corresp_field == NULL) {
          match = FALSE;
        } else if (corresp_field != NULL && field == NULL) {
          match = FALSE;
        }  /* if */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = scope->types;
        a_type_ptr  corresp_mem_type = corresp_scope->types;
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = mem_type->next,
                                  corresp_mem_type = corresp_mem_type->next) {
          record_trans_unit_corresp(mem_type, corresp_mem_type);
          if (!verify_type_correspondence(mem_type)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (mem_type != NULL && corresp_mem_type == NULL) {
          match = FALSE;
        } else if (corresp_mem_type != NULL && mem_type == NULL) {
          match = FALSE;
        }  /* if */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = scope->routines;
        a_routine_ptr  corresp_routine = corresp_scope->routines;
        for (; routine != NULL && corresp_routine != NULL;
             routine = routine->next,
                                    corresp_routine = corresp_routine->next) {
          record_trans_unit_corresp(routine, corresp_routine);
          if (!verify_routine_correspondence(routine)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (routine != NULL && corresp_routine == NULL) {
          match = FALSE;
        } else if (corresp_routine != NULL && routine == NULL) {
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
          record_trans_unit_corresp(variable, corresp_variable);
          if (!verify_variable_correspondence(variable)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (variable != NULL && corresp_variable == NULL) {
          match = FALSE;
        } else if (corresp_variable != NULL && variable == NULL) {
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
          record_trans_unit_corresp(constant, corresp_constant);
          if (!verify_constant_correspondence(constant)) {
            match = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (constant != NULL && corresp_constant == NULL) {
          match = FALSE;
        } else if (corresp_constant != NULL && constant == NULL) {
          match = FALSE;
        }  /* if */
      }
    }  /* if */
  }  /* if */
  if (!match) {
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
  } else if (is_immediate_class_type(type) &&
             is_immediate_class_type(corresp_type)) {
    match = verify_class_type_correspondence(type);
  } else if (is_immediate_enum_type(type) &&
             is_immediate_enum_type(corresp_type)) {
    match = verify_enum_type_correspondence(type);
  } else {
    match = TRUE;
  }  /* if */
  return match;
}  /* verify_type_correspondence */


static a_boolean verify_namespace_correspondence(a_namespace_ptr  nsp)
/*
If the given namespace has a canonical entry in another translation unit,
check that the two indeed match up beyond name, kind and scope attributes.
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
        /* An error if the conflicting entity has external linkage. */
        if (may_have_correspondence(sym)) {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &nsp_sym->decl_position, sym);
        }  /* if */
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
        record_trans_unit_corresp(type, sym->variant.namespace_info.ptr);
        break;
      } else if (is_tag_symbol(type_sym) && !is_type_symbol(sym)) {
        /* This is not a conflict. */
      } else if (is_tag_symbol(sym) != is_tag_symbol(type_sym)) {
        /* An error if the conflicting entity has external linkage. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &type_sym->decl_position, sym);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* find_type_correspondence */


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
        if (same_parents(sub_sym, routine_sym)) {
          /* Two different declarations in the same namespace or class, and
             with the same name: they should probably match up. */
          switch (sub_sym->kind) {
            case sk_routine:
            case sk_member_function:
              if (identical_types(routine_symbol_type(sub_sym),
                                  routine->type) &&
                  may_have_correspondence(sub_sym)) {
                /* Record the correspondence. */
                record_trans_unit_corresp(routine,
                                          sub_sym->variant.routine.ptr);
              }  /* *if */
              break;
            case sk_class_or_struct_tag:
            case sk_union_tag:
            case sk_enum_tag:
              break;
            case sk_type:
              if (sym->variant.type.is_injected_class_name) break;
            default:
              if (may_have_correspondence(sub_sym)) {
                pos_sy_error(ec_not_compatible_with_previous_decl,
                             &routine_sym->decl_position, sub_sym);
              }  /* if */
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
        same_parents(sym, var_sym)) {
      /* Two different declarations in the same namespace or class, and
         with the same name: they should probably match up. */
      switch (sym->kind) {
        case sk_variable:
          if (may_have_correspondence(sym)) {
            /* Record the correspondence. */
            record_trans_unit_corresp(var, sym->variant.variable.ptr);
          }  /* *if */
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
        case sk_enum_tag:
          break;
        case sk_type:
          if (sym->variant.type.is_injected_class_name) break;
        default:
          if (may_have_correspondence(sym)) {
            pos_sy_error(ec_not_compatible_with_previous_decl,
                         &var_sym->decl_position, sym);
          }  /* if */
      }  /* switch */
    }  /* if */
  }  /* for */
}  /* find_variable_correspondence */


static void establish_namespace_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of namespaces of the given scope and establish a
translation unit correspondence pointer for each of them.
*/
{
  a_namespace_ptr  nsp;

  /* Visit all namespaces. */
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    if (has_name(nsp)) {
      find_namespace_correspondence(nsp);
      if (!verify_namespace_correspondence(nsp)) {
        /* Some error occurred---clear the association. */
        clear_trans_unit_corresp(nsp);
      } else if (!nsp->is_namespace_alias) {
        /* Establish correspondences for nested namespaces. */
        establish_trans_unit_correspondences_for_scope(
                                                    nsp->variant.assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* establish_namespace_correspondences_for_scope */


static void establish_type_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of types of the given scope and establish a translation unit
correspondence pointer for each of them.
*/
{
  a_type_ptr  type;

  /* Visit all types. */
  for (type = scope->types; type != NULL; type = type->next) {
    a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;

    /* Note that placeholder types do not have an associated symbol. */
    if (type_sym != NULL && may_have_correspondence(type_sym)) {
      find_type_correspondence(type);
      if (trans_unit_corresp_pointer_of(type) != NULL &&
          !verify_type_correspondence(type)) {
        /* Some error occurred---clear the association. */
        clear_trans_unit_corresp(type);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* establish_type_correspondences_for_scope */


static void establish_routine_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of routines of the given scope and establish a translation
unit correspondence pointer for each of them.
*/
{
  a_routine_ptr  routine;

  /* Visit all routines. */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    find_routine_correspondence(routine);
    if (trans_unit_corresp_pointer_of(routine) != NULL &&
        !verify_routine_correspondence(routine)) {
      /* Some error occurred---clear the association. */
      clear_trans_unit_corresp(routine);
    }  /* if */
  }  /* for */
}  /* establish_routine_correspondences_for_scope */


static void establish_variable_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of variables of the given scope and establish a translation
unit correspondence pointer for each of them.
*/
{
  a_variable_ptr  variable;

  /* Visit all variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    find_variable_correspondence(variable);
    if (trans_unit_corresp_pointer_of(variable) != NULL &&
        !verify_variable_correspondence(variable)) {
      /* Some error occurred---clear the association. */
      clear_trans_unit_corresp(variable);
    }  /* if */
  }  /* for */
}  /* establish_variable_correspondences_for_scope */


void establish_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
/*
Establish correspondences for all the applicable entities in the given
scope.  The process is repeated in nested scopes.
*/
{
  establish_namespace_correspondences_for_scope(scope);
  establish_type_correspondences_for_scope(scope);
  establish_routine_correspondences_for_scope(scope);
  establish_variable_correspondences_for_scope(scope);
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
