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

trans_corresp.c -- Routines related to matching entities across
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

/* Pointers to canonical built-in types. */
static a_type_ptr canonical_int_types[(int)ik_last];
static a_type_ptr canonical_signed_int_types[(int)ik_last];
#if MICROSOFT_EXTENSIONS_ALLOWED
static a_type_ptr canonical_microsoft_sized_int_types[(int)ik_last];
static a_type_ptr canonical_microsoft_sized_signed_int_types[(int)ik_last];
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
static a_type_ptr canonical_float_types[(int)fk_last];
#if C99_IL_EXTENSIONS_SUPPORTED
static a_type_ptr canonical_complex_types[(int)fk_last];
static a_type_ptr canonical_imaginary_types[(int)fk_last];
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
static a_type_ptr canonical_il_void_type;
static a_type_ptr canonical_il_wchar_t_type;
static a_type_ptr canonical_il_bool_type;

/* Forward declarations. */
static void clear_scope_correspondence(a_scope_ptr  scope,
                                       a_boolean    visited);
static void clear_type_correspondence(a_type_ptr  type,
                                      a_boolean   visited);
static void find_type_correspondence(a_type_ptr  type,
                                     a_boolean   parent_found);
static void find_template_correspondence(a_template_ptr  templ,
                                         a_boolean       parent_found);
static a_boolean verify_type_correspondence(a_type_ptr  type);
static a_boolean verify_template_correspondence(a_template_ptr  templ);
static void verify_trans_unit_correspondences_for_scope(a_scope_ptr  scope);
static void establish_instantiation_correspondences(a_template_ptr  templ);


static a_boolean type_has_definition(a_type_ptr  type)
/*
Return TRUE if type has a definition.
*/
{
  a_boolean  result;

  if (is_immediate_class_type(type)) {
    result = class_type_has_body(type);
  } else {
    result = !is_incomplete_type(type);
  }  /* if */
  return result;
}  /* type_has_definition */


#define has_correspondence(ptr)                                        \
  (trans_unit_corresp_pointer_of(ptr) != NULL &&                       \
   trans_unit_corresp_pointer_of(ptr) != (char*)(ptr))


char* f_canonical_il_entry_of(char *il_entry)
/*
Return the canonical IL entry for the given entry.  This is the entry itself
if the entry is from a primary translation unit or if it has no corresponding
entry in another translation unit.
*/
{
  for (;;) {
    if (!in_secondary_trans_unit(il_entry)) {
      break;
    } else {
      char *canonical_ptr = checked_trans_unit_corresp_pointer_of(il_entry);
      if (canonical_ptr == NULL || canonical_ptr == il_entry) {
        break;
      } else {
        il_entry = canonical_ptr;
      }  /* if */
    }  /* if */
  }  /* for */
  return il_entry;
}  /* f_canonical_il_entry_of */


/*
Return the symbol list on which an entry corresponding to the given sym may
be expected.
*/
#define corresp_symbol_list(sym)					\
  ((sym)->header->inactive_symbols)


#if DEBUG
static void *trace_corresp_ptr = NULL;

static void corresp_intercept(void)
/*
This routine's main purpose is to have a breakpoint set on it from a symbolic
debugger.  The routine is called if the correspondence pointer for the address
pointed to by trace_corresp_ptr is modified.
*/
{
  fprintf(f_debug, "Modifying correspondence for node at %x.\n",
          (unsigned)trace_corresp_ptr);
}  /* corresp_intercept */

#define trace_corresp_check(ptr)                                       \
  if ((void*)(ptr) == trace_corresp_ptr) { corresp_intercept(); }

void db_corresp(void *ptr)
/*
Report correspondence pointer for given entry.
*/
{
  fprintf(f_debug, "Correspondence for 0x%x is 0x%x",
          (unsigned)ptr, (unsigned)trans_unit_corresp_pointer_of(ptr));
}  /* db_corresp */

#else /* !DEBUG */

#define trace_corresp_check(ptr)  /* Nothing */

#endif /* DEBUG */


static void f_set_trans_unit_corresp(char *entity1,
                                     char *entity2)
/*
Make the translation unit correspondence entry of the IL node pointed to by
entity1 point to the address indicated by entity2.
*/
{
  trace_corresp_check(entity1);
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
    if (line != 0) {
      fprintf(f_debug, " in file %s (line %ld) ", file_name, line);
    } else {
      fprintf(f_debug, " (built-in; line %ld) ", line);
    }  /* if */
    if (entity2 == NULL) {
      fprintf(f_debug, "has undetermined correspondence.\n");
    } else if (entity1 == entity2) {
      fprintf(f_debug, "has no correspondence.\n");
    } else {
      conv_seq_to_file_and_line(scp2->decl_position.seq, &file_name,
                                &full_name, &line, &at_end_of_source);
      fprintf(f_debug, "should correspond to ");
      if (line != 0) {
        fprintf(f_debug, "entity in file %s (line %ld).\n", file_name, line);
      } else {
        fprintf(f_debug, "built-in entity (line %ld).\n", line);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  trans_unit_corresp_pointer_of(entity1) = entity2;
}  /* f_set_trans_unit_corresp */

#define set_trans_unit_corresp(entity1, entity2)                    \
  f_set_trans_unit_corresp((char*)(entity1), (char*)(entity2))


/*
Mark the given IL entry as having no correspondence in another translation
unit.  This is done by having the correspondence pointer point to the IL entry
itself.  In contrast, a NULL correspondence pointer indicates that the entry
has not yet been examined for a matching entry in another translation unit.
*/
#define set_no_trans_unit_corresp(ptr)                                  \
  set_trans_unit_corresp((char*)(ptr), (char*)(ptr))

#define set_unvisited_trans_unit_corresp(ptr)                           \
  set_trans_unit_corresp((ptr), NULL)

#define clear_trans_unit_corresp(ptr, visited)                          \
  /*lint --e(506)*/                                                     \
  ((visited) ? set_no_trans_unit_corresp(ptr)                           \
             : set_unvisited_trans_unit_corresp(ptr))

static void f_record_trans_unit_corresp(char *entity1,
                                        char *entity2)
/*
Make the translation unit correspondence entry of the IL node pointed to by
entity1 point to the IL node pointed to by entity2.
*/
{
  check_assertion_str(entity1 != entity2, "correspondence loop attempted");
  set_trans_unit_corresp(entity1, entity2);
}  /* f_record_trans_unit_corresp */

#define record_trans_unit_corresp(entity1, entity2)                    \
  f_record_trans_unit_corresp((char*)(entity1), (char*)(entity2))


static void report_corresp_error(char                   *entity1,
                                 a_source_position_ptr  pos2,
                                 an_error_code          same_src_error,
                                 an_error_code          distinct_src_error)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence.  If the two corresponding
IL indentities result from the same source construct (e.g., because the same
header file was included in two translation units), use the message associated
with same_src_error; otherwise, use distinct_src_error.  The position of the
corresponding entity is pos2.
*/
{
  a_symbol_ptr   sym = (a_symbol_ptr)((a_source_correspondence_ptr)entity1)
                                                                  ->assoc_info;
  a_source_position_ptr
                 pos1 = &sym->decl_position;
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
    pos_stsy_error(same_src_error, &sym->decl_position,
                   primary_file2->name_as_written, sym);
  } else {
    /* The corresponding entities result from distinct source constructs. */
    pos_sy_start_error(distinct_src_error, &sym->decl_position, sym);
    add_diag_info_with_pos_insert(ec_corresp_decl_at, pos2);
    end_error();
  }  /* if */
}  /* report_corresp_error */


void f_report_bad_trans_unit_corresp(char                   *entity1,
                                     a_source_position_ptr  pos2)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence, but it points to a node
that does not actually correspond to the given entity.  Therefore, issue a
diagnostic.
*/
{
  report_corresp_error(entity1, pos2,
                       ec_entity_differs_in_other_trans_unit,
                       ec_corresp_decl_incompatible);
}  /* f_report_bad_trans_unit_corresp */


static void f_process_bad_trans_unit_corresp(char  *entity)
/*
Same as report_bad_trans_unit_corresp but also clear the correspondence
pointer.
*/
{
  report_bad_trans_unit_corresp(entity);
  set_no_trans_unit_corresp(entity);
}  /* process_bad_trans_unit_corresp */

#define process_bad_trans_unit_corresp(entity)                        \
  f_process_bad_trans_unit_corresp((char*)(entity))


static void f_report_multiple_definitions(char                   *entity1,
                                          a_source_position_ptr  pos2)
/*
The given IL node has a source correspondence and an associated symbol.  It
also has a non-NULL translation unit correspondence.  Issue a diagnostic
reporting that both entries are definitions.
*/
{
  report_corresp_error(entity1, pos2,
                       ec_entity_defined_in_other_trans_unit,
                       ec_entity_defined_twice);
}  /* f_report_multiple_definitions */

#define report_multiple_definitions(entity)                               \
  f_report_multiple_definitions(                                          \
    (char*)(entity),                                                        \
    &((a_source_correspondence_ptr)trans_unit_corresp_pointer_of(entity))   \
      ->decl_position)

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
    result = same_type_entities(parent1, parent2);
  } else {
    a_namespace_ptr              parent1 = sym1->parent.namespace_ptr;
    a_namespace_ptr              parent2 = sym2->parent.namespace_ptr;
    an_il_entry_kind             kind;
    a_source_correspondence_ptr  scp1, scp2;
    scp1 = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                  sym1, &kind);
    scp2 = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                  sym2, &kind);
    /* Block-extern declarations can have a NULL parent pointer in the symbol
       entry even though they declared an entity in a namespace.  Use the IL
       entry in those cases.  (See function add_namespace_parent_pointer.) */
    if (parent1 == NULL && scp1 != NULL) {
      parent1 = scp1->parent.namespace_ptr;
    }  /* if */
    if (parent2 == NULL && scp2 != NULL) {
      parent2 = scp2->parent.namespace_ptr;
    }  /* if */
    if (parent1 != NULL || parent2 != NULL) {
      if (!microsoft_bugs && scp1 != NULL && scp2 != NULL &&
          scp1->name_linkage == (a_name_linkage_kind)nlk_external &&
          scp2->name_linkage == (a_name_linkage_kind)nlk_external) {
        /* extern "C" entities match even if they are declared in different
           namespaces.  (But not in Microsoft bugs mode.) */
        result = TRUE;
      } else {
        result = same_namespace_entities(parent1, parent2);
      }  /* if */
    } else {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* same_parents */


static a_boolean known_same_parents(a_symbol_ptr  sym1,
                                    a_symbol_ptr  sym2)
/*
Return TRUE if and only if the parent (namespace or class) entities of the
given symbols have the same canonical entry.  (This differs from 
"same_parents"in that no attempt is made to establish a canonical entry.
*/
{
  a_boolean  result;

  if (sym1->is_class_member != sym2->is_class_member) {
    result = FALSE;
  } else if (sym1->is_class_member) {
    result = (canonical_il_entry_of(sym1->parent.class_type) ==
                              canonical_il_entry_of(sym2->parent.class_type));
  } else {
    result = (canonical_il_entry_of(sym1->parent.namespace_ptr) ==
                           canonical_il_entry_of(sym2->parent.namespace_ptr));
  }  /* if */
  return result;
}  /* known_same_parents */


static a_boolean may_have_correspondence(a_symbol_ptr sym)
/*
Return TRUE if the given symbol is associated with an entity that may appear
in multiple translation units so that correspondence pointers between them
need to be determined.
*/
{
  a_boolean        result;

  switch (sym->kind) {
    case sk_class_or_struct_tag:
    case sk_enum_tag:
    case sk_union_tag:
      if (C_mode()) {
        /* In C mode structs, unions and enums will be matched up if they're
           identical, but if they're not no error should be emitted.  Assume
           they may have a correspondence at first. */
        result = TRUE;
      } else {
        an_il_entry_kind             kind;
        a_source_correspondence_ptr  scp;
        scp = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                   sym, &kind);
        if (scp != NULL &&
            (scp->name_linkage == (a_name_linkage_kind)nlk_external ||
             scp->name_linkage ==
                                (a_name_linkage_kind)nlk_cplusplus_external)) {
          result = TRUE;
        } else {
          result = FALSE;
        }  /* if */
      }
      break;
    case sk_class_template:
    case sk_constant:
    case sk_field:
    case sk_function_template:
    case sk_member_function:
    case sk_namespace:
    case sk_static_data_member:
    case sk_routine:
    case sk_variable:
      {
        an_il_entry_kind             kind;
        a_source_correspondence_ptr  scp;
        scp = (a_source_correspondence_ptr)il_entry_for_symbol_null_okay(
                                                                   sym, &kind);
        if (scp != NULL &&
            (scp->name_linkage == (a_name_linkage_kind)nlk_external ||
             scp->name_linkage ==
                                (a_name_linkage_kind)nlk_cplusplus_external)) {
          result = TRUE;
        } else {
          result = FALSE;
        }  /* if */
      }
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
    case sk_type:
      {
        a_type_ptr  type = type_symbol_type(sym);
        result = FALSE;
        if (type->kind == (a_type_kind)tk_typeref &&
            typeref_is_typedef(type)) {
          /* A typedef of an unnamed class or enum has linkage. */
          type = skip_typerefs(type);
          result = (is_immediate_class_type(type) &&
                    type->variant.class_struct_union.originally_unnamed) ||
                   (is_immediate_enum_type(type) &&
                    type->variant.integer.originally_unnamed);
        }  /* if */
      }
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


static void set_builtin_type_corresp(a_type_ptr  *record,
                                     a_type_ptr  type)
/*
*record holds a record of the canonical builtin type equivalent to the given
type.  If *record is NULL, the type is to become that canonical type.
Otherwise, type's correspondence should be set to *record.
*/
{
  if (*record == NULL) {
    *record = type;
    if (in_secondary_trans_unit(type)) {
      trans_unit_corresp_pointer_of(type) = (char*)type;
    }  /* if */
  } else {
    if (in_secondary_trans_unit(type)) {
      trans_unit_corresp_pointer_of(type) = (char*)*record;
    } else {
      /* Sometimes we switch back to the primary translation unit (e.g.,
         to perform IL lowering) and create there a builtin type that was
         already created in a secondary translation unit. */
      checked_trans_unit_corresp_pointer_of(*record) = (char*)type;
      *record = type;
    }  /* if */
  }  /* if */
}  /* set_builtin_type_corresp */


void record_builtin_type(a_type_ptr  type)
/*
This routine is called whenever a basic builtin type is created.  If this is
the first time this particular type is created, the type is recorded in an
array so that it can be found when an equivalent type is created in a future
translation unit.  If the type had already been created, its correspondence
is set to point to the first created type.
*/
{
  switch (type->kind) {
    case tk_void:
      set_builtin_type_corresp(&canonical_il_void_type, type);
      break;
    case tk_integer:
      if (type->variant.integer.bool_type) {
        set_builtin_type_corresp(&canonical_il_bool_type, type);
      } else if (type->variant.integer.wchar_t_type) {
        set_builtin_type_corresp(&canonical_il_wchar_t_type, type);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (type->variant.integer.microsoft_sized_int_type) {
        if (type->variant.integer.explicitly_signed) {
          set_builtin_type_corresp(
               &canonical_microsoft_sized_signed_int_types[
                                              type->variant.integer.int_kind],
               type);
        } else {
          set_builtin_type_corresp(
               &canonical_microsoft_sized_int_types[
                                              type->variant.integer.int_kind],
               type);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        if (type->variant.integer.explicitly_signed) {
          set_builtin_type_corresp(
                  &canonical_signed_int_types[type->variant.integer.int_kind],
                  type);
        } else {
          set_builtin_type_corresp(
                  &canonical_int_types[type->variant.integer.int_kind], type);
        }  /* if */
      }  /* if */
      break;
    case tk_float:
      set_builtin_type_corresp(
                      &canonical_float_types[type->variant.float_kind], type);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      set_builtin_type_corresp(
                  &canonical_imaginary_types[type->variant.float_kind], type);
      break;
    case tk_complex:
      set_builtin_type_corresp(
                    &canonical_complex_types[type->variant.float_kind], type);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    default:
      unexpected_condition_str("record_builtin_type: bad type kind");
  }  /* switch */
}  /* record_builtin_type */


static a_field_ptr skip_generated_field(a_field_ptr  field)
/*
If the given field was generated by the front end (e.g., a virtual function
table pointer), skip to the next field that was actually declared in the
source.  Fields that represent anonymous union objects are not skipped (even
though in some sense they are "generated").
*/
{
  while (field != NULL && field->source_corresp.decl_position.seq == 0 &&
         !field->is_anonymous_parent_object) {
    field = field->next;
  }  /* while */
  return field;
}  /* skip_generated_field */


static a_routine_ptr skip_generated_routine(a_routine_ptr  routine)
/*
If the given routine was generated by the front end, skip to the next routine
that was actually declared in the source.  Here a routine generated from a
function or member function template is considered "generated", but a member
of a class template is not.  (The point is that "generated" routines do not
always appear in the same order on the routines list of a class scope.)
*/
{
  while (routine != NULL && (
#if NEED_NAME_MANGLING
         /* Some routines are generated as part of prelowering. */
         routine->source_corresp.name_has_been_mangled ||
#endif /* NEED_NAME_MANGLING */
         /* Ordinary members of template classes have a NULL template argument
            list. */ 
          (routine->is_template_function &&
           routine->template_arg_list != NULL))) {
    routine = routine->next;
  }  /* while */
  return routine;
}  /* skip_generated_routine */


#define is_placeholder_type(type)                                       \
  ((type)->kind == (a_type_kind)tk_typeref &&                           \
    ((type)->variant.typeref.is_placeholder_for_class_instantiation ||  \
     (type)->variant.typeref.is_placeholder_for_namespace_type ||       \
     (type)->variant.typeref.is_placeholder_for_nested_class_def))

static a_type_ptr skip_generated_type(a_type_ptr  type)
/*
If the given type was generated by the front end, skip to the next type
that was actually declared in the source.  Here a type generated from a
class or member class template is considered "generated", but a member
of a class template is not.  (The point is that "generated" types do not
always appear in the same order on the types list of a class scope.)
*/
{
  a_type_ptr  result = type;

  while (result != NULL && (
#if NEED_NAME_MANGLING
         /* Some types are generated as part of prelowering. */
         result->source_corresp.name_has_been_mangled ||
#endif /* NEED_NAME_MANGLING */
         is_placeholder_type(result) ||
          /* Nonprototype instantiations can differ from one translation unit
             to another.  (The check on template_arg_list ensures that we
             only skip actual instantiations as opposed to members of
             instantiations.) */
         (is_immediate_class_type(result) &&
          result->variant.class_struct_union.is_template_class &&
          !result->variant.class_struct_union.is_prototype_instantiation &&
          result->variant.class_struct_union.extra_info->template_arg_list
                                                                  != NULL))) {
    result = result->next;
  }  /* while */
  return result;
}  /* skip_generated_type */


static void add_instantiation(a_template_symbol_supplement_ptr  tssp,
                              a_symbol_ptr                      inst)
/*
Add the given instantiation symbol to the list of all instantiations
associated with tssp.  If tssp is associated with a partial specialization,
its corresponding primary template supplement will be used instead.
*/
{
  a_symbol_list_entry_ptr  slep = alloc_symbol_list_entry();

  if (is_type_symbol(inst)) {
    tssp = primary_template_of((a_symbol_ptr)tssp->il_template_entry
                                             ->source_corresp.assoc_info)
                                                      ->variant.template_info;
  }  /* if */
  slep->next = tssp->all_instantiations;
  tssp->all_instantiations = slep;
  slep->symbol = inst;
#if DEBUG
  if (db_flag_is_set("trans_corresp")) {
    a_line_number  line;
    char           *file_name, *full_name;
    a_boolean      at_end_of_source;
    fprintf(f_debug, "DBG> ! Adding ");
    db_symbol_name(inst);
    fprintf(f_debug, " (%s) to all_instantiations list for ",
            symbol_kind_names[(int)inst->kind]);
    if (tssp->il_template_entry != NULL) {
      a_symbol_ptr   templ_sym = (a_symbol_ptr)tssp->il_template_entry
                                                   ->source_corresp.assoc_info;
      db_symbol_name(templ_sym);
      conv_seq_to_file_and_line(templ_sym->decl_position.seq, &file_name,
                                &full_name, &line, &at_end_of_source);
      if (line != 0) {
        fprintf(f_debug, " in file %s (line %ld)\n", file_name, line);
      } else {
        fprintf(f_debug, " (built-in; line %ld)\n", line);
      }  /* if */
    } else {
      fprintf(f_debug, "unknown symbol\n");
    }  /* if */
  }  /* if */
#endif /* DEBUG */
}  /* add_instantiation */


static void clear_instantations_correspondence(a_template_ptr  templ,
                                               a_boolean       visited)
/*
Mark all instantiations associated with the given template as having no
correspondences.  If visited is TRUE, also record those instantiations of the
all_instantiations list of the associated template symbol supplement.
*/
{
  a_symbol_ptr  templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  a_template_symbol_supplement_ptr
                tssp = templ_sym->variant.template_info;

  if (is_class_template_symbol(templ_sym)) {
    a_symbol_ptr  inst = tssp->variant.class_template.instantiations,
                  proto = tssp
                             ->variant.class_template.prototype_instantiation;
    a_type_ptr    class_type;
    if (proto != NULL) {
      class_type = type_symbol_type(proto);
      clear_type_correspondence(class_type, visited);
      if (visited) {
        add_instantiation(tssp, proto);
      }  /* if */
    }  /* if */
    for (; inst != NULL; inst = next_instance_sym(inst)) {
      if (inst != proto) {
        class_type = type_symbol_type(inst);
        clear_type_correspondence(class_type, visited);
        if (visited) {
          add_instantiation(tssp, inst);
        }  /* if */
      }  /* if */
    }  /* for */
  } else {
    a_template_instance_ptr  inst = tssp->variant.function.instantiations;
    for (; inst != NULL; inst = inst->next) {
      a_routine_ptr   routine = inst->instance_sym->variant.routine.ptr;
      clear_trans_unit_corresp(routine, visited);
      if (visited) {
        add_instantiation(tssp, inst->instance_sym);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* clear_instantations_correspondence */


static void clear_enum_type_correspondence(a_type_ptr  type,
                                           a_boolean   visited)
/*
Clear the correspondence pointers in the substructure of an enum type.
*/
{
  a_constant_ptr  enumerator = type->variant.integer.enum_info.constant_list;

  for (; enumerator != NULL; enumerator = enumerator->next) {
    clear_trans_unit_corresp(enumerator, visited);
  }  /* for */
}  /* clear_enum_type_correspondence */

#define set_no_enum_type_correspondence(type)                       \
  clear_enum_type_correspondence((type), /*visited=*/TRUE)


static void clear_class_type_correspondence(a_type_ptr  type,
                                            a_boolean   visited)
/*
Clear the correspondence pointers in the substructure of a class type.
*/
{
  if (class_type_has_body(type)) {
    /* Traverse fields: (both C and C++) */
    a_field_ptr  field = type->variant.class_struct_union.field_list;
    for (; field != NULL; field = field->next) {
      clear_trans_unit_corresp(field, visited);
    }  /* for */
  
    if (!C_mode()) {
      /* Traverse entities only available in C++ mode. */
      a_scope_ptr  scope = type->
                           variant.class_struct_union.extra_info->assoc_scope;
      a_base_class_ptr  base = type->variant.class_struct_union.extra_info
                                                               ->base_classes;
      clear_scope_correspondence(scope, visited);
      for (; base != NULL; base = base->next) {
        trans_unit_corresp_pointer_of(base) = visited ? (char*)base : NULL;
      }  /* for */
    }  /* if */
  }  /* if */
}  /* clear_class_type_correspondence */

#define set_no_class_type_correspondence(type)                       \
  clear_class_type_correspondence((type), /*visited=*/TRUE)


static void clear_type_correspondence(a_type_ptr  type,
                                      a_boolean   visited)
/*
Clear the correspondence of the given type and (if applicable) its
substructure.
*/
{
  if (trans_unit_corresp_pointer_of(type) == NULL) {
    /* Mark this type as visited. */
    clear_trans_unit_corresp(type, visited);
    /* Also mark inner structure if applicable. */
    if (is_immediate_class_type(type)) {
      if (class_type_has_body(type)) {
        clear_class_type_correspondence(type, visited);
      }  /* if */
    } else if (is_immediate_enum_type(type)) {
      clear_enum_type_correspondence(type, visited);
    }  /* if */
  }  /* if */
}  /* clear_type_correspondence */


static void clear_namespace_correspondence(a_namespace_ptr  nsp,
                                           a_boolean        visited)
/*
Clear the correspondence pointers in the members of a namespace.
*/
{
  if (!nsp->is_namespace_alias) {
    clear_scope_correspondence(nsp->variant.assoc_scope, visited);
  }  /* if */
}  /* clear_namespace_correspondence */

#define set_no_namespace_correspondence(nsp)                       \
  clear_namespace_correspondence((nsp), /*visited=*/TRUE)


static void clear_scope_correspondence(a_scope_ptr  scope,
                                       a_boolean    visited)
/*
Mark the IL entries in the given scope as having no correspondence in other
translation units.  If visited is TRUE, the entries are marked as having been
visited; otherwise, they may yet be set to correspond to another entry.
*/
{
  /* Traverse namespaces: */
  {
    a_namespace_ptr  nsp = scope->namespaces;
    for (; nsp != NULL; nsp = nsp->next) {
      clear_namespace_correspondence(nsp, visited);
      clear_trans_unit_corresp(nsp, visited);
    }  /* for */
  }

  /* Traverse templates: */
  {
    a_template_ptr  templ = scope->templates;
    for (; templ != NULL; templ = templ->next) {
      clear_trans_unit_corresp(templ, visited);
      clear_instantations_correspondence(templ, visited);
    }  /* for */
  }

  /* Traverse types: */
  {
    a_type_ptr  type = scope->types;
    for (; type != NULL; type = type->next) {
      clear_type_correspondence(type, visited);
    }  /* for */
  }
  /* Traverse routines: */
  {
    a_routine_ptr  routine = scope->routines;
    for (;routine != NULL; routine = routine->next) {
      clear_trans_unit_corresp(routine, visited);
    }  /* for */
  }

  /* Traverse variables/static data members: */
  {
    a_variable_ptr  variable = scope->variables;
    for (; variable != NULL; variable = variable->next) {
      clear_trans_unit_corresp(variable, visited);
    }  /* for */
  }

  /* Traverse constants: */
  {
    a_constant_ptr  constant = scope->constants;
    for (; constant != NULL; constant = constant->next) {
      clear_trans_unit_corresp(constant, visited);
    }  /* for */
  }
}  /* clear_scope_correspondence */


static a_boolean f_same_name(char  *entity1,
                             char  *entity2)
/*
Return whether the given entities have the same name (in most cases, their
associated symbols are listed under the same header).
*/
{
  a_boolean            match;
  a_source_correspondence_ptr
                       scp1 = (a_source_correspondence_ptr)entity1,
                       scp2 = (a_source_correspondence_ptr)entity2;
  a_symbol_header_ptr  sh1, sh2;

  if (scp1->name == scp2->name) {
    match = TRUE;
  } else if (scp1->assoc_info == NULL || scp2->assoc_info == NULL) {
    /* A mismatch in which one of the entities is unnamed. */
    check_assertion(!(scp1->assoc_info == NULL && scp2->assoc_info == NULL));
    match = FALSE;
  } else {
    sh1 = ((a_symbol_ptr)scp1->assoc_info)->header;
    sh2 = ((a_symbol_ptr)scp2->assoc_info)->header;
    match = (sh1 == sh2);
    if (!match) {
      /* This is possible if the associated symbol is not part of the symbol
         table (which is TRUE of template instances). */
      match = !strncmp(sh1->identifier, sh2->identifier,
                       (sh1->identifier_length < sh2->identifier_length) ?
                                             (size_t)sh1->identifier_length :
                                             (size_t)sh2->identifier_length);
    }  /* if */
  }  /* if */
  return match;
}  /* f_same_name */

#define same_name(ptr1, ptr2)                                       \
  f_same_name((char*)(ptr1), (char*)(ptr2))


static a_boolean f_verify_name_correspondence(char  *entity1)
/*
Verify that the given entity and the one pointed to by its translation unit
correspondence pointer have the same name (effectively, that their associated
symbols are listed under the same header).
*/
{
  char       *entity2 = trans_unit_corresp_pointer_of(entity1);
  a_boolean  match = same_name(entity1, entity2);
  if (!match) {
    /* Only class members have a correspondence pointer set without testing
       whether the names match.  If the names don't match, an error
       contrasting the two member entities would not make much sense.
       Instead, report the error on the parent type.  Note that the IL entries
       for some template members may not indicate that they are class members;
       in those cases, the associated symbol entry should be examined.
       */
    a_source_correspondence_ptr  scp1 = (a_source_correspondence_ptr)entity1;
    if (scp1->is_class_member) {
      if (!C_mode()) {
        /* In C mode, two structs with the same name (and file scope) but with
           incompatible fields can coexist.  The correspondence will be cleared
           in that case, but no diagnostic should be produced. */
        report_bad_trans_unit_corresp(scp1->parent.class_type);
      }  /* if */
    } else {
      /* Normally, this happens only for certain template entries that
         represent members of templates.  No diagnostic is issued here,
         because one will be issued on the prototype instantiation. */
      a_symbol_ptr  sym1 = (a_symbol_ptr)scp1->assoc_info;
      check_assertion(sym1->is_class_member ||
                      sym1->kind == (a_symbol_kind)sk_member_function);
      expect_error();
    }  /* if */
  }  /* if */
  return match;
}  /* f_verify_name_correspondence */

#define verify_name_correspondence(ptr)                                \
  f_verify_name_correspondence((char*)(ptr))

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean same_str(char *s1,
                          char *s2)
/*
Determine whether the given character strings are the same.  Also handle NULL
pointers.
*/
{
  a_boolean  result;

  if (s1 == s2) {
    result = TRUE;
  } else if (s1 == NULL || s2 == NULL) {
    result = FALSE;
  } else {
    result = (strcmp(s1, s2) == 0);
  }  /* if */
  return result;
}  /* same_str */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean verify_field_correspondence(a_field_ptr  field)
/*
Check that the recorded translation unit correspondence for the given field
is in fact valid.
*/
{
  a_boolean    match = TRUE;
  if (has_correspondence(field)) {
    a_field_ptr  corresp_field = (a_field_ptr)canonical_il_entry_of(field);
    a_source_correspondence_ptr
                 scp = &field->source_corresp,
                 corresp_scp = &corresp_field->source_corresp;
  
    match = verify_name_correspondence(field);
    if (match &&
        (!types_are_redecl_compatible(field->type, corresp_field->type) ||
         !same_exception_spec(field->type, corresp_field->type) ||
         field->offset != corresp_field->offset ||
         field->offset_bit_remainder != corresp_field->offset_bit_remainder ||
         field->bit_size != corresp_field->bit_size ||
         field->is_bit_field != corresp_field->is_bit_field ||
         field->bit_field_is_signed != corresp_field->bit_field_is_signed ||
         field->is_anonymous_parent_object !=
                                   corresp_field->is_anonymous_parent_object ||
         field->is_mutable != corresp_field->is_mutable ||
#if MICROSOFT_EXTENSIONS_ALLOWED
         !same_str(field->get_property_name,
                                           corresp_field->get_property_name) ||
         !same_str(field->put_property_name,
                                           corresp_field->put_property_name) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      if (!C_mode()) {
        if (scp->assoc_info != NULL &&
            scp->assoc_info != (char*)unnamed_field_symbol()) {
          /* A named field: */
          process_bad_trans_unit_corresp(field);
        } else {
          /* An unnamed field has a meaningless associated symbol.  Report
             the error on the associated class instead. */
          report_bad_trans_unit_corresp(scp->parent.class_type);
          set_no_trans_unit_corresp(field);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* verify_field_correspondence */


static a_boolean verify_routine_correspondence(a_routine_ptr  routine)
/*
Check that the recorded translation unit correspondence for the given routine
is in fact valid.
*/
{
  a_boolean      match = TRUE;

  if (has_correspondence(routine)) {
    a_routine_ptr  corresp_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
    a_source_correspondence_ptr
                   scp = &routine->source_corresp,
                   corresp_scp = &corresp_routine->source_corresp;
    match = verify_name_correspondence(routine);
    if (match &&
        (!types_are_redecl_compatible(routine->type, corresp_routine->type) ||
         !same_exception_spec(routine->type, corresp_routine->type) ||
         routine->is_virtual != corresp_routine->is_virtual ||
         routine->pure_virtual != corresp_routine->pure_virtual ||
         /* The inline attribute isn't set on nonprototype template functions
            until the template is actually instantiated.  Also, it need not
            match in C99 mode. */
         (!C_mode() && routine->is_inline != corresp_routine->is_inline &&
          (routine->is_prototype_instantiation || routine->is_specialized ||
           !routine->is_template_function)) ||
         routine->is_explicit_constructor !=
                                    corresp_routine->is_explicit_constructor ||
#if DECL_MODIFIERS_IN_USE
         routine->decl_modifiers != corresp_routine->decl_modifiers ||
#endif /* DECL_MODIFIERS_IN_USE */
         (routine->defined && corresp_routine->defined &&
          (routine->fp_contract != corresp_routine->fp_contract ||
           routine->fenv_access != corresp_routine->fenv_access ||
           routine->cx_limited_range != corresp_routine->cx_limited_range)) ||
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      process_bad_trans_unit_corresp(routine);
    }  /* if */
    if (match && !trans_unit_test_mode && !routine->is_inline &&
        routine->defined && corresp_routine->defined) {
      /* Multiple definition. */
      report_multiple_definitions(routine);
    }  /* if */
  }  /* if */
  return match;
}  /* verify_routine_correspondence */


static a_boolean verify_variable_correspondence(a_variable_ptr  var)
/*
Check that the recorded translation unit correspondence for the given variable
is in fact valid.
*/
{
  a_boolean       match = TRUE;

  if (has_correspondence(var)) {
    a_variable_ptr  corresp_var = (a_variable_ptr)canonical_il_entry_of(var);
    a_source_correspondence_ptr
                    scp = &var->source_corresp,
                    corresp_scp = &corresp_var->source_corresp;
    match = verify_name_correspondence(var);
    if (match &&
        (!f_types_are_compatible(var->type, corresp_var->type,
                                 TCF_SEEK_CORRESP |
                                 TCF_REDECLARATION |
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) ||
         !same_exception_spec(var->type, corresp_var->type) ||
         var->is_member_constant != corresp_var->is_member_constant ||
         /* In-class static member initializations must be equivalent. */
         (var->is_member_constant &&
          !eq_constants(var->initializer.constant,
                        corresp_var->initializer.constant)) ||
#if DECL_MODIFIERS_IN_USE
         var->decl_modifiers != corresp_var->decl_modifiers ||
#endif /* DECL_MODIFIERS_IN_USE */
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      process_bad_trans_unit_corresp(var);
    }  /* if */
    if (match && !trans_unit_test_mode &&
        var->storage_class == (a_storage_class)sc_unspecified &&
        corresp_var->storage_class == (a_storage_class)sc_unspecified &&
        (!C_mode() ||
         (var->init_kind != (an_init_kind)initk_none &&
          corresp_var->init_kind != (an_init_kind)initk_none))) {
      /* Two nontentative definitions. */
      report_multiple_definitions(var);
    }  /* if */
  }  /* if */
  return match;
}  /* verify_variable_correspondence */


static a_boolean verify_constant_correspondence(a_constant_ptr  constant)
/*
Check that the recorded translation unit correspondence for the given constant
is in fact valid.
*/
{
  a_boolean       match = TRUE;

  if (has_correspondence(constant)) {
    a_constant_ptr  corresp_constant =
                               (a_constant_ptr)canonical_il_entry_of(constant);
    a_source_correspondence_ptr
                    scp = &constant->source_corresp,
                    corresp_scp = &corresp_constant->source_corresp;
    match = verify_name_correspondence(constant);
    if (match &&
        (!identical_types(constant->type, corresp_constant->type) ||
         !same_exception_spec(constant->type, corresp_constant->type) ||
         !eq_constants(constant, corresp_constant) ||
         scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage)) {
      match = FALSE;
      if (!C_mode()) {
        /* In C mode, constants have no linkage. */
        process_bad_trans_unit_corresp(constant);
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* verify_constant_correspondence */


static void check_for_enumerator_conflicts(a_type_ptr  type)
/*
Check whether the enumerators attached to the given enum type conflict with
other entities.  (Not significant in C mode: C enumerators have no linkage.)
*/
{
  check_assertion(is_immediate_enum_type(type));
  if (!type->source_corresp.is_class_member && !C_mode()) {
    a_constant_ptr  enumerator = type->variant.integer.enum_info.constant_list;
    for (; enumerator != NULL; enumerator = enumerator->next) {
      a_symbol_ptr  enum_sym = (a_symbol_ptr)enumerator
                                                   ->source_corresp.assoc_info,
                    sym = corresp_symbol_list(enum_sym);
      /* Look through the symbol table for any entities with linkage that may
         conflict with an enumerator. */
      for (; sym != NULL; sym = sym->next) {
        if (sym->decl_scope != enum_sym->decl_scope &&
            may_have_correspondence(sym) &&
            same_parents(sym, enum_sym)) {
          f_report_bad_trans_unit_corresp((char*)enumerator,
                                          &sym->decl_position);
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
}  /* check_for_enumerator_conflicts */


static a_boolean verify_enum_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given enum
type is in fact valid.
*/
{
  a_boolean       match = verify_name_correspondence(type);
  a_boolean       report_error = FALSE;
  a_type_ptr      corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (!match) {
    /* An error was already issued. */
  } else if (!is_immediate_enum_type(corresp_type)) {
    match = FALSE;
    report_error = TRUE;
  } else {
    a_constant_ptr  enumerator = type->variant.integer.enum_info.constant_list,
                    corresp_enumerator =
                        corresp_type->variant.integer.enum_info.constant_list;
    for (; enumerator != NULL && corresp_enumerator != NULL;
         enumerator = enumerator->next,
                              corresp_enumerator = corresp_enumerator->next) {
      if (!same_name(enumerator, corresp_enumerator)) {
        /* The error should be issued on the enum type since there is not
           much in common between the enumerators if even their names don't
           match. */
        match = FALSE;
        report_error = TRUE;
        break;
      } else if (!verify_constant_correspondence(enumerator)) {
        match = FALSE;
        break;
      }  /* if */
    }  /* for */
    if ((enumerator != NULL && corresp_enumerator == NULL) ||
        (corresp_enumerator != NULL && enumerator == NULL)) {
      match = FALSE;
      report_error = TRUE;
    }  /* if */
    if (match && 
        (enumerator != NULL ||
#if MICROSOFT_EXTENSIONS_ALLOWED
         !same_str(type->variant.integer.uuid_string,
                   corresp_type->variant.integer.uuid_string) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         type->variant.integer.int_kind !=
                                     corresp_type->variant.integer.int_kind)) {
      match = FALSE;
      report_error = TRUE;
    }  /* if */
  }  /* if */
  if (!match) {
    if (report_error && !C_mode()) {
      /* In C mode, a correspondence mismatch is not an error.  (It just means
         the types are unrelated.) */
      report_bad_trans_unit_corresp(type);
    }  /* if */
    set_no_enum_type_correspondence(type);
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
  a_boolean   both_defined = TRUE;
  a_type_ptr  corresp_type = (a_type_ptr)trans_unit_corresp_pointer_of(type);

  if (!match) {
    /* An error was already issued. */
  } else if (!is_immediate_class_type(corresp_type)) {
    match = FALSE;
    report_error = TRUE;
  } else if (!class_type_has_body(type) ||
             !class_type_has_body(corresp_type)) {
    /* The types are assumed to match in their inner structure since at least
       one is incomplete and therefore has no inner structure to conflict
       with. */
    both_defined = FALSE;
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
        goto done;
      }  /* if */
    }  /* for */
    if ((field != NULL && corresp_field == NULL) ||
        (corresp_field != NULL && field == NULL)) {
      /* In C mode, we simply ignore the correspondence.  In C++ mode, this
         is an error. */
      report_error = !C_mode();
      match = FALSE;
      goto done;
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
            goto done;
          }  /* if */
        }  /* for */
        if ((templ != NULL && corresp_templ == NULL) ||
            (corresp_templ != NULL && templ == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = skip_generated_type(scope->types);
        a_type_ptr  corresp_mem_type = skip_generated_type(
                                                        corresp_scope->types);
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = skip_generated_type(mem_type->next),
             corresp_mem_type = skip_generated_type(corresp_mem_type->next)) {
          if (!verify_type_correspondence(mem_type)) {
            match = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        if ((mem_type != NULL && corresp_mem_type == NULL) ||
            (corresp_mem_type != NULL && mem_type == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse member routines: */
      {
        a_routine_ptr  routine = skip_generated_routine(scope->routines);
        a_routine_ptr  corresp_routine = skip_generated_routine(
                                                      corresp_scope->routines);
        for (; routine != NULL && corresp_routine != NULL;
             routine = skip_generated_routine(routine->next),
             corresp_routine = skip_generated_routine(corresp_routine->next)) {
          if (!verify_routine_correspondence(routine)) {
            match = FALSE;
            goto done;
          }  /* if */
        }  /* for */
        if ((routine != NULL && corresp_routine == NULL) ||
            (corresp_routine != NULL && routine == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
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
            goto done;
          }  /* if */
        }  /* for */
        if ((variable != NULL && corresp_variable == NULL) ||
            (corresp_variable != NULL && variable == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
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
            goto done;
          }  /* if */
        }  /* for */
        if ((constant != NULL && corresp_constant == NULL) ||
            (corresp_constant != NULL && constant == NULL)) {
          report_error = TRUE;
          match = FALSE;
          goto done;
        }  /* if */
      }
      /* Traverse base classes. */
      {
        /* Ensure that the list of base classes is compatible across
           translation units.  If so, we set the correspondence pointers
           for the convenience of later processing stages, but note that
           this is an unusual case in that a correspondence is set for an
           entity without source correspondence. */
        a_base_class_ptr  base = type->variant.class_struct_union.extra_info
                                                                ->base_classes;
        a_base_class_ptr  corresp_base = corresp_type
                                        ->variant.class_struct_union.extra_info
                                        ->base_classes;
        for (; base != NULL && corresp_base != NULL;
             base = base->next, corresp_base = corresp_base->next) {
          if (!base->direct) {
            /* Only consider direct bases.  A mismatch in an indirect base can
               be reported on the class for which it is a direct base. */
          } else if (!identical_types(base->type, corresp_base->type) ||
                     base->is_virtual != corresp_base->is_virtual ||
                     base->derivation->access !=
                                            corresp_base->derivation->access) {
            match = FALSE;
            report_error = TRUE;
            goto done;
          }  /* if */
          /* Set source correspondence: */
          trans_unit_corresp_pointer_of(base) = (char*)corresp_base;
        }  /* for */
        if ((base == NULL && corresp_base != NULL) ||
            (base != NULL && corresp_base == NULL)) {
          match = FALSE;
          report_error = TRUE;
          goto done;
        }  /* if */
      }
      /* Traverse member using declarations. */
      {
        /* Like base class entries, member using declaration enties do not have
           a correspondence pointer set.  However, they must match across
           translation units. */
        a_using_decl_ptr  ud = scope->using_decls;
        a_using_decl_ptr  corresp_ud = corresp_scope->using_decls;
        for (; ud != NULL && corresp_ud != NULL;
             ud = ud->next, corresp_ud = corresp_ud->next) {
          if (ud->is_using_directive != corresp_ud->is_using_directive ||
              ud->access != corresp_ud->access ||
              ud->entity.kind != corresp_ud->entity.kind ||
              canonical_il_entry_of(ud->entity.ptr) !=
                               canonical_il_entry_of(corresp_ud->entity.ptr) ||
              canonical_il_entry_of(ud->qualifier.class_type) !=
                     canonical_il_entry_of(corresp_ud->qualifier.class_type)) {
            match = FALSE;
            report_error = TRUE;
            goto done;
          }  /* if */
        }  /* for */
        if ((ud == NULL && corresp_ud != NULL) ||
            (ud != NULL && corresp_ud == NULL)) {
          match = FALSE;
          report_error = TRUE;
          goto done;
        }  /* if */
      }
    }  /* if */
  }  /* if */
  if (match) {
    /* Check various properties of the type. */
#define class_info type->variant.class_struct_union
#define corresp_info corresp_type->variant.class_struct_union
    a_class_type_supplement_ptr
        sup = class_info.extra_info, corresp_sup = corresp_info.extra_info;
    if ((both_defined &&
         (class_info.any_const_member != corresp_info.any_const_member ||
          class_info.any_mutable_member != corresp_info.any_mutable_member ||
          class_info.any_virtual_base_classes !=
                                       corresp_info.any_virtual_base_classes ||
          class_info.abstract != corresp_info.abstract ||
          class_info.any_virtual_functions !=
                                          corresp_info.any_virtual_functions ||
          class_info.any_pure_virtual_functions !=
                                     corresp_info.any_pure_virtual_functions ||
          class_info.any_virtual_functions_including_in_base_classes !=
                corresp_info.any_virtual_functions_including_in_base_classes ||
          class_info.originally_unnamed != corresp_info.originally_unnamed ||
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
          class_info.is_nonstd_anonymous_union_type !=
                                 corresp_info.is_nonstd_anonymous_union_type ||
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
          class_info.contains_flexible_array_member !=
                                 corresp_info.contains_flexible_array_member ||
#if USER_CONTROL_OF_STRUCT_PACKING
          class_info.max_member_alignment !=
                                           corresp_info.max_member_alignment ||
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
          class_info.is_empty_class != corresp_info.is_empty_class ||
          (sup != NULL &&
           (sup->virtual_function_info_offset !=
                                   corresp_sup->virtual_function_info_offset ||
            sup->anonymous_union_kind != corresp_sup->anonymous_union_kind
#if MICROSOFT_EXTENSIONS_ALLOWED
            || sup->inheritance_kind != corresp_sup->inheritance_kind
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                  )))) ||
        class_info.is_template_class != corresp_info.is_template_class ||
        class_info.is_nonreal_class != corresp_info.is_nonreal_class ||
        class_info.is_prototype_instantiation !=
                                     corresp_info.is_prototype_instantiation ||
        class_info.is_specialized != corresp_info.is_specialized ||
        (sup != NULL &&
         (
#if NEAR_AND_FAR_ALLOWED
          sup->qualifiers != corresp_sup->qualifiers ||
#endif /* NEAR_AND_FAR_ALLOWED */
#if DECL_MODIFIERS_IN_USE
          sup->decl_modifiers != corresp_sup->decl_modifiers ||
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
          !same_str(sup->uuid_string, corresp_sup->uuid_string) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          !same_field_entities(sup->anonymous_union_field,
                                       corresp_sup->anonymous_union_field)))) {
      match = FALSE;
      report_error = TRUE;
    }  /* if */
#undef class_info
#undef corresp_info
  }  /* if */
done:
  if (!match) {
    if (report_error) {
      report_bad_trans_unit_corresp(type);
    }  /* if */
    set_no_class_type_correspondence(type);
  }  /* if */
  return match;
}  /* verify_class_type_correspondence */


static a_boolean verify_type_correspondence(a_type_ptr  type)
/*
Check that the recorded translation unit correspondence for the given type
is in fact valid.
*/
{
  a_boolean     match;
  a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  a_type_ptr    corresp_type = (a_type_ptr)trans_unit_corresp_pointer_of(type);
  a_boolean     both_defined = type_has_definition(type) &&
                               type_has_definition(corresp_type);
  a_source_correspondence_ptr
                scp = &type->source_corresp,
                corresp_scp = &corresp_type->source_corresp;

  if (!has_correspondence(type)) {
    match = TRUE;
    if (trans_unit_corresp_pointer_of(type) == NULL) {
      set_no_trans_unit_corresp(type);
    }  /* if */
    if (is_immediate_enum_type(type)) {
      check_for_enumerator_conflicts(type);
    }  /* if */
  } else if (type_sym == NULL) {
    /* This must be a placeholder type. */
    check_assertion(is_placeholder_type(type));
    match = FALSE;
    set_no_trans_unit_corresp(type);
  } else {
    if (!verify_name_correspondence(type)) {
      match = FALSE;
      set_no_trans_unit_corresp(type);
    } else if (is_immediate_class_type(type)) {
      /* corresp_type is also a class type since the type kinds are
         identical. */
      match = verify_class_type_correspondence(type);
      if (!match && C_mode()) {
        set_no_trans_unit_corresp(type);
      }  /* if */
    } else if (is_immediate_enum_type(type)) {
      match = verify_enum_type_correspondence(type);
      if (!match && C_mode()) {
        set_no_trans_unit_corresp(type);
      }  /* if */
    } else {
      match = identical_types(type, corresp_type) &&
              same_exception_spec(type, corresp_type);
      if (!match && scp->is_class_member) {
        process_bad_trans_unit_corresp(type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (match &&
      ((type->kind != corresp_type->kind &&
        /* "class" and "struct" are interchangeable if not both entries are
           definitions. */
        !(!both_defined && is_class_or_struct(type) &&
                           is_class_or_struct(corresp_type))) ||
       (both_defined && (type->size != corresp_type->size ||
                         type->alignment != corresp_type->alignment)) ||
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
       type->use_cfront_transitional_nested_type_name_mangling !=
             corresp_type->use_cfront_transitional_nested_type_name_mangling ||
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
       type->is_builtin_va_list != corresp_type->is_builtin_va_list ||
       scp->access != corresp_scp->access ||
       scp->name_linkage != corresp_scp->name_linkage)) {
    match = FALSE;
    process_bad_trans_unit_corresp(type);
  }
  return match;
}  /* verify_type_correspondence */


static a_boolean verify_template_correspondence(a_template_ptr  templ)
/*
Check that the recorded translation unit correspondence for the given template
is in fact valid.
*/
{
  a_boolean       match = TRUE;
  a_symbol_ptr    templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;

  if (has_correspondence(templ)) {
    a_template_ptr  corresp_templ =
                          (a_template_ptr)trans_unit_corresp_pointer_of(templ);
    a_symbol_ptr    corresp_sym =
                        (a_symbol_ptr)corresp_templ->source_corresp.assoc_info;
    a_template_symbol_supplement_ptr
                    tssp = NULL, corresp_tssp = NULL;
    a_source_correspondence_ptr
                    scp = &templ->source_corresp,
                    corresp_scp = &corresp_templ->source_corresp;
    match = verify_name_correspondence(templ);
    if (match && is_template_symbol(templ_sym)) {
      /* templ_sym could also be an ordinary member function. */
      tssp = templ_sym->variant.template_info;
      corresp_tssp = corresp_sym->variant.template_info;
    }  /* if */
    if (match &&
        (scp->access != corresp_scp->access ||
         scp->name_linkage != corresp_scp->name_linkage ||
         (tssp != NULL &&
          !equiv_template_param_lists(
                                    corresp_tssp->cache.decl_info->parameters,
                                    tssp->cache.decl_info->parameters,
                                    /*issue_errors=*/FALSE,
                                    &templ_sym->decl_position)))) {
      match = FALSE;
      process_bad_trans_unit_corresp(templ);
    }  /* if */
    if (!match) {
      /* The templates don't seem to match, so don't try to verify the
         instantiations. */
    } else if (is_class_template_symbol(templ_sym)) {
      /* A class template. Verify the instantiations (if any). */
      a_type_ptr  proto = prototype_template_of(templ_sym)
                              ->variant.template_info
                              ->variant.class_template.prototype_instantiation
                              ->variant.class_struct_union.type,
                  corresp_proto = prototype_template_of(corresp_sym)
                              ->variant.template_info
                              ->variant.class_template.prototype_instantiation
                              ->variant.class_struct_union.type;
      /* For partial specializations we must also verify the template arguments
         (attached to the prototype instantiations). */
      if (!equiv_template_arg_lists(
             proto->variant.class_struct_union.extra_info->template_arg_list,
             corresp_proto->
                    variant.class_struct_union.extra_info->template_arg_list,
             ETA_NO_OPTIONS)) {
        match = FALSE;
        process_bad_trans_unit_corresp(templ);
      } else {
        a_symbol_ptr  inst = tssp->variant.class_template.instantiations;
        /* First process the prototype instantiation. */
        match = verify_type_correspondence(proto);
        if (match) {
          /* Only check real instantiations if the prototype instantiation
             matched. */
          for (; inst != NULL; inst = next_instance_sym(inst)) {
            a_type_ptr  inst_type = type_symbol_type(inst);
            if (!inst_type->variant.class_struct_union.is_specialized) {
              /* Specializations appear on the types list of their scope. */
              (void)verify_type_correspondence(inst_type);
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    } else if (templ_sym->kind == (a_symbol_kind)sk_function_template) {
      /* A function template.  Verify the instantiations (if any). */
      a_template_instance_ptr  inst = tssp->variant.function.instantiations;
      for (; inst != NULL; inst = inst->next) {
        (void)verify_routine_correspondence(
                                      inst->instance_sym->variant.routine.ptr);
      }  /* for */
      /* Also process prototype instantiation. */
      (void)verify_routine_correspondence(tssp->variant.function.routine);
    }  /* if */
  }  /* if */
  return match;
}  /* verify_template_correspondence */


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
      report_bad_trans_unit_corresp(nsp);
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
        set_no_trans_unit_corresp(nsp);
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
  for (type = skip_generated_type(scope->types);
       type != NULL;
       type = skip_generated_type(type->next)) {
    a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;

    /* Note that placeholder types do not have an associated symbol. */
    if (type_sym != NULL && may_have_correspondence(type_sym)) {
      (void)verify_type_correspondence(type);
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
      set_no_trans_unit_corresp(routine);
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
      set_no_trans_unit_corresp(variable);
    }  /* if */
  }  /* for */
}  /* verify_variable_correspondences_for_scope */


static void verify_template_correspondences_for_scope(a_scope_ptr  scope)
/*
Traverse the list of templates of the given scope and verify a translation
unit correspondence pointer for each of them.
*/
{
  a_template_ptr  templ;

  /* Visit all templates. */
  for (templ = scope->templates;
       templ != NULL;
       templ = templ->next) {
    if (trans_unit_corresp_pointer_of(templ) != NULL &&
        !verify_template_correspondence(templ)) {
      /* Some error occurred---clear the association. */
      set_no_trans_unit_corresp(templ);
    }  /* if */
  }  /* for */
}  /* verify_template_correspondences_for_scope */


static void establish_trans_unit_correspondences_for_enum(a_type_ptr  type)
/*
Establish correspondences for the list of constants associated with the
given enum type.
*/
{
  a_type_ptr      corresp_type = (a_type_ptr)canonical_type_entry_of(type);

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


void establish_trans_unit_correspondences_for_class(a_type_ptr  type)
/*
Set the correspondence pointers in the members of a type.  The members' types
are not checked.
*/
{
  a_type_ptr  corresp_type = in_secondary_trans_unit(type) ?
                               (a_type_ptr)trans_unit_corresp_pointer_of(type)
                             : NULL;

  if (corresp_type != NULL  && corresp_type != type &&
      is_immediate_class_type(corresp_type) &&
      class_type_has_body(type) && class_type_has_body(corresp_type)) {
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
        if (C_mode()) {
          /* Special handling is needed for unnamed types defined as part of
             field declarations.  In C mode, such types go into the file scope
             but since they have no name find_type_correspondence will not
             find a correspondence. */
          a_type_ptr  field_type = field->type,
                      corresp_field_type = corresp_field->type;
          if (is_immediate_class_type(field_type) && !has_name(field_type) &&
              is_immediate_class_type(corresp_field_type) &&
              !has_name(corresp_field_type) &&
              !has_correspondence(field_type)) {
            record_trans_unit_corresp(field_type, corresp_field->type);
            establish_trans_unit_correspondences_for_class(field_type);
          }  /* if */
        }  /* if */
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
          a_symbol_ptr  templ_sym =
                               (a_symbol_ptr)templ->source_corresp.assoc_info;
          templ_sym = is_class_template_symbol(templ_sym) ?
                                      prototype_template_of(templ_sym) : NULL;
          if (templ_sym != NULL && templ_sym->defined) {
            /* Use the symbol table to see if there is a canonical definition
               of this template. */
            find_template_correspondence(templ, /*parent_found=*/TRUE);
          } else {
            record_trans_unit_corresp(templ, corresp_templ);
          }  /* if */
          if (!has_correspondence(templ) ||
              templ->kind != corresp_templ->kind) {
            /* Could only be due to an error.  Record the correspondence
               so a diagnostic can be issued. */
            set_unvisited_trans_unit_corresp(templ);
            record_trans_unit_corresp(templ, corresp_templ);
            process_bad_trans_unit_corresp(templ);
          } else {
            establish_instantiation_correspondences(templ);
          }  /* if */
        }  /* for */
      }
      /* Traverse member types: */
      {
        a_type_ptr  mem_type = skip_generated_type(scope->types);
        a_type_ptr  corresp_mem_type =
                                    skip_generated_type(corresp_scope->types);
        for (; mem_type != NULL && corresp_mem_type != NULL;
             mem_type = skip_generated_type(mem_type->next),
             corresp_mem_type = skip_generated_type(corresp_mem_type->next)) {
          if ((is_immediate_class_type(mem_type) ||
               is_immediate_enum_type(mem_type)) &&
              type_has_definition(mem_type) && has_name(mem_type)) {
            /* If this is a defined class or enum type, its correspondence
               should be the "canonical definition".  corresp_mem_type may
               not be a definition at all or it may not be the definition
               against all others are compared (i.e., the canonical one). */
            find_type_correspondence(mem_type, /*parent_found=*/TRUE);
          } else {
            record_trans_unit_corresp(mem_type, corresp_mem_type);
          }  /* if */
          if (!has_correspondence(mem_type)) {
            /* Could only be due to an error.  Record the correspondence
               so a diagnostic will be issued. */
            set_unvisited_trans_unit_corresp(mem_type);
            record_trans_unit_corresp(mem_type, corresp_mem_type);
            process_bad_trans_unit_corresp(mem_type);
          } else if (is_immediate_class_type(mem_type)) {
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
        a_routine_ptr  routine = skip_generated_routine(scope->routines);
        a_routine_ptr  corresp_routine = skip_generated_routine(
                                                      corresp_scope->routines);
        for (; routine != NULL && corresp_routine != NULL;
             routine = skip_generated_routine(routine->next),
             corresp_routine = skip_generated_routine(corresp_routine->next)) {
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


a_boolean seek_class_type_corresp(a_type_ptr  type_1,
                                  a_type_ptr  type_2)
/*
Check if the given class types are in fact the same and, if so, record all
the needed correspondence pointers for type_1 and return TRUE.  Otherwise,
return FALSE.
*/
{
  a_boolean result;

  if (has_correspondence(type_1)) {
    result = (canonical_il_entry_of(type_1) == canonical_il_entry_of(type_2));
  } else {
    a_boolean  visited =
                     (trans_unit_corresp_pointer_of(type_1) == (char*)type_1);
    clear_type_correspondence(type_1, /*visited=*/FALSE);
    record_trans_unit_corresp(type_1, type_2);
    establish_trans_unit_correspondences_for_class(type_1);
    result = verify_class_type_correspondence(type_1);
    if (!result && !visited) {
      clear_type_correspondence(type_1, /*visited=*/FALSE);
    }  /* if */
  }  /* if */
  return result;
}  /* seek_class_type_corresp */


static void find_namespace_correspondence(a_namespace_ptr  nsp)
/*
Look for the given namespace in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  nsp_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;

  check_assertion(nsp_sym != NULL);
  if (nsp_sym == symbol_for_namespace_std) {
    /* The standard namespace always exists (in C++), but its symbol in the
       primary translation unit is not linked into the symbol table if no
       member of std has been declared yet.  However, we do know that it is
       the first entry on the list of namespaces for the primary file scope. */
    a_namespace_ptr  primary_std_namespace = translation_units
                                                  ->primary_scope->namespaces;
    check_assertion(primary_std_namespace != NULL &&
                    primary_std_namespace
                              ->source_corresp.parent.namespace_ptr == NULL &&
                    strncmp(primary_std_namespace->source_corresp.name,
                            "std", 3) == 0);
    record_trans_unit_corresp(nsp, primary_std_namespace);
  } else {
    a_symbol_ptr  sym = corresp_symbol_list(nsp_sym);
    if (trans_unit_corresp_pointer_of(nsp) == NULL) {
      /* Mark this namespace as visited to avoid infinite recursion. */
      set_no_trans_unit_corresp(nsp);
    }  /* if */
    for (; sym != NULL; sym = sym->next) {
      if (sym->decl_scope != nsp_sym->decl_scope &&
          may_have_correspondence(sym) &&
          same_parents(sym, nsp_sym)) {
        /* Two different declarations in the same namespace and with the same
           name: they should probably match up. */
        if (is_namespace_symbol(sym) &&
            sym->variant.namespace_info.ptr->is_namespace_alias ==
                                                    nsp->is_namespace_alias) {
          /* Mark as unvisited. */
          trans_unit_corresp_pointer_of(nsp) = NULL;
          /* Record the correspondence. */
          record_trans_unit_corresp(nsp, sym->variant.namespace_info.ptr);
          break;
        } else {
          /* An error since the conflicting entity has external linkage. */
          f_report_bad_trans_unit_corresp((char*)nsp, &sym->decl_position);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* find_namespace_correspondence */


static void find_type_correspondence(a_type_ptr  type,
                                     a_boolean   parent_found)
/*
Look for the given type in another translation unit and set the translation
unit correspondence pointer if one is found.  When parent_found is TRUE,
this procedure should not attempt to seek correspondences for parent
entities.
*/
{
  a_symbol_ptr  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  a_symbol_ptr  sym, corresp_sym = NULL;

  if (has_name(type) &&
      type_sym != NULL && may_have_correspondence(type_sym)) {
    a_boolean     first_tag_definition = type_sym->defined && 
                   (type_sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
                    type_sym->kind == (a_symbol_kind)sk_enum_tag);
    sym = corresp_symbol_list(type_sym);
    for (; sym != NULL; sym = sym->next) {
      /* Don't consider symbols in the same file. */
      if (sym->decl_scope != type_sym->decl_scope &&
          may_have_correspondence(sym) &&
          (parent_found ? known_same_parents(sym, type_sym)
                        : same_parents(sym, type_sym))) {
        /* Two different declarations in the same namespace and with the same
           name: they should probably match up. */
        if (sym->kind == type_sym->kind ||
            /* "class" and "struct" are interchangeable if not both entries
               are definitions. */
            (sym->kind == (a_symbol_kind)sk_class_or_struct_tag &&
             type_sym->kind == (a_symbol_kind)sk_class_or_struct_tag &&
             sym->defined != type_sym->defined)) {
          if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
              sym->kind == (a_symbol_kind)sk_enum_tag) {
            /* Definitions of class and enum types are preferably matched up
               with other such definitions so that their members can be
               traversed for correspondence checking. */
            if (!type_sym->defined) {
              /* We are processing a declaration that is not a definition. */
              corresp_sym = sym;
              break;
            } else if (sym->defined) {
              /* Both are defined.  Check if sym corresponds to the "canonical
                 definition": the definition at the end of the correspondence
                 chain or the only definition whose correspondence is the
                 nondefining declaration at the end of the chain. */
              a_type_ptr  canonical_def = type_symbol_type(sym);
              first_tag_definition = FALSE;
              corresp_sym = sym;
              if (!in_secondary_trans_unit(canonical_def) ||
                  !has_correspondence(canonical_def)) {
                /* A definition in a primary translation is always a canonical
                   definition.  So is a definition in a secondary translation
                   unit that doesn't have a correspondence (i.e., the first
                   encountered definition with no declaration in the primary
                   translation unit). */
                break;
              } else {
                /* Check if the canonical definition candidate does indeed
                   point to a nondefining declaration (which should be the
                   root of the correspondence tree). */
                a_type_ptr  next = (a_type_ptr)
                                 trans_unit_corresp_pointer_of(canonical_def);
                if (type_has_definition(next)) {
                  /* next is the canonical definition. */
                  corresp_sym = (a_symbol_ptr)next->source_corresp.assoc_info;
                  canonical_def = next;
#if CHECKING
                  next = (a_type_ptr)
                                 trans_unit_corresp_pointer_of(canonical_def);
                  check_assertion(!(in_secondary_trans_unit(canonical_def) &&
                                    has_correspondence(canonical_def)) ||
                                  (!(in_secondary_trans_unit(next) &&
                                     has_correspondence(next)) &&
                                   !type_has_definition(next)));
#endif /* CHECKING */
                  break;
                } else {
                  /* canonical_def corresponds to the canonical definition
                     since it has a correspondence pointer that points to a
                     nondefining declaration that is the end of the chain.
                     */
                  check_assertion(!(in_secondary_trans_unit(next) &&
                                    has_correspondence(next)));
                  break;
                }  /* if */
              }  /* if */
              /* sym did not correspond to a canonical definition: continue
                 to look for one. */
            } else if (corresp_sym == NULL) {
              /* type is defined, but the candidate sym is not.  Remember that
                 candidate, but continue to look for a defined candidate. */
              corresp_sym = sym;
            } /* if */
          } else {
            /* Not a class or enum type: no need to worry about a "canonical
               definition" concept. */
            corresp_sym = sym;
            break;
          }  /* if */
        } else if (!type_sym->is_class_member &&
                   (!is_tag_symbol(type_sym) ||
                    (is_type_symbol(sym) ||
                     is_template_symbol(sym) ||
                     is_namespace_symbol(sym)))) {
          /* Not a match, but record the correspondence so that the error
             reporting code knows which entity conflicts.  (Errors are
             reported elsewhere for class members.) */
          a_type_ptr  corresp_type = type_symbol_type(sym);
          record_trans_unit_corresp(type, corresp_type);
          process_bad_trans_unit_corresp(type);
        }  /* if */
      }  /* if */
    }  /* for */
    if (corresp_sym != NULL) {
       /* Record the correspondence. */
      a_type_ptr  corresp_type = type_symbol_type(corresp_sym);
      if (first_tag_definition) {
        /* This is the first definition of a class or enum type.  Make it
           point to the root. */
        corresp_type = (a_type_ptr)canonical_il_entry_of(corresp_type);
      }  /* if */
      record_trans_unit_corresp(type, corresp_type);
      if (corresp_sym->kind == (a_symbol_kind)sk_type) {
        /* A typedef of an originally unnamed type. */
        type = skip_typerefs(type);
        check_assertion(
           (is_immediate_class_type(type) &&
                   type->variant.class_struct_union.originally_unnamed) ||
           (is_immediate_enum_type(type) &&
                   type->variant.integer.originally_unnamed));
        record_trans_unit_corresp(type, skip_typerefs(corresp_type));
      }  /* if */
      if (is_immediate_class_type(type)) {
        establish_trans_unit_correspondences_for_class(type);
      } else if (is_immediate_enum_type(type)) {
        establish_trans_unit_correspondences_for_enum(type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (trans_unit_corresp_pointer_of(type) == NULL) {
    clear_type_correspondence(type, /*visited=*/TRUE);
  }  /* if */
}  /* find_type_correspondence */


static a_symbol_list_entry_ptr find_class_template_instantiation(
                                       a_template_symbol_supplement_ptr  tssp,
                                       a_symbol_ptr                      inst)
/*
Search the list of instantiations attached to the given template symbol
supplement for an instantiation that matches inst.
*/
{
  a_type_ptr  class_type = type_symbol_type(inst);
  a_symbol_list_entry_ptr
              sym_entry = tssp->all_instantiations;
  a_class_type_supplement_ptr
              ctsp = class_type->variant.class_struct_union.extra_info;

  for (; sym_entry != NULL; sym_entry = sym_entry->next) {
    a_type_ptr  corresp_type = type_symbol_type(sym_entry->symbol);
    a_class_type_supplement_ptr
                corresp_ctsp =
                      corresp_type->variant.class_struct_union.extra_info;
    /* Check that the template arguments and possibly the partial
       specialization arguments are equivalent.  The ETA_IS_NONREAL_MEMBER
       option allows differing length for the argument lists.  Do not confuse
       a prototype instantiation with a similar nonreal instantiation. */
    if (class_type->variant.class_struct_union.is_prototype_instantiation ==
           corresp_type
                    ->variant.class_struct_union.is_prototype_instantiation &&
        equiv_template_arg_lists(ctsp->template_arg_list,
                                 corresp_ctsp->template_arg_list,
                                 ETA_IS_NONREAL_MEMBER) &&
        ((ctsp->partial_spec_template_arg_list == NULL &&
          corresp_ctsp->partial_spec_template_arg_list == NULL) ||
         equiv_template_arg_lists(
                             ctsp->partial_spec_template_arg_list,
                             corresp_ctsp->partial_spec_template_arg_list,
                             ETA_IS_NONREAL_MEMBER))) {
      break;
    }  /* if */
  }  /* for */
  return sym_entry;
}  /* find_class_template_instantiation */


static void record_class_template_instantiation(a_symbol_ptr  inst)
/*
Search for an instantiation that corresponds to inst in a prior translation
unit.  If there is one, record a correspondence pointer; otherwise, add
the instantiation to the list of instantiations in the associated template
symbol supplement.
*/
{
  a_type_ptr      class_type = type_symbol_type(inst);
  a_symbol_ptr    templ_sym = primary_template_of(
                                   inst->variant.class_struct_union.extra_info
                                       ->class_template);
  a_template_symbol_supplement_ptr
                  tssp = templ_sym->variant.template_info, corresp_tssp;
  a_template_ptr  templ = tssp->il_template_entry,
                  corresp_templ = canonical_template_entry_of(templ);

  /* Note that the call to canonical_template_entry_of may have resulted in a
     correspondence value being set already. */
  if (trans_unit_corresp_pointer_of(class_type) == NULL) {
    a_symbol_list_entry_ptr
                    sym_entry = NULL;
    corresp_tssp = ((a_symbol_ptr)corresp_templ->source_corresp.assoc_info)
                         ->variant.template_info;
    /* Mark the type as visited to avoid infinite recursion. */
    set_no_trans_unit_corresp(class_type);
    if (has_correspondence(templ)) {
      sym_entry = find_class_template_instantiation(corresp_tssp, inst);
    }  /* if */
    if (sym_entry == NULL) {
      /* The instantiation was not found on the canonical list.  Add it now. */
      add_instantiation(corresp_tssp, inst);
      set_no_class_type_correspondence(class_type);
    } else {
      /* Restore the type to an unvisited state before setting the
         correspondence (which will effectively remark it as visited). */
      a_type_ptr  corresp_type = type_symbol_type(sym_entry->symbol);
      trans_unit_corresp_pointer_of(class_type) = NULL;
      record_trans_unit_corresp(class_type, corresp_type);
      establish_trans_unit_correspondences_for_class(class_type);
      if (!sym_entry->symbol->defined && inst->defined) {
        /* Prefer a definition as the representative. */
        sym_entry->symbol = inst;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* record_class_template_instantiation */


static a_symbol_list_entry_ptr find_function_template_instantiation(
                                       a_template_symbol_supplement_ptr  tssp,
                                       a_symbol_ptr                      inst)
/*
Search the list of instantiations attached to the given template symbol
supplement for an instantiation that matches inst.
*/
{
  a_symbol_list_entry_ptr  sym_entry = tssp->all_instantiations;
  a_routine_ptr            routine = inst->variant.routine.ptr;
  a_template_arg_ptr       templ_args = routine->template_arg_list;

  for (; sym_entry != NULL; sym_entry = sym_entry->next) {
    a_routine_ptr  corresp_routine = sym_entry->symbol->variant.routine.ptr;
    if (identical_types(routine->type, corresp_routine->type) &&
        /* The ETA_IS_NONREAL_MEMBER option allows comparisons between
           template argument lists that are not known to match the same
           template. */
        equiv_template_arg_lists(corresp_routine->template_arg_list,
                                 templ_args, ETA_IS_NONREAL_MEMBER)) {
      break;
    }  /* if */
  }  /* for */
  return sym_entry;
}  /* find_function_template_instantiation */


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
                  corresp_templ =
                            (a_template_ptr)canonical_template_entry_of(templ);
  a_template_symbol_supplement_ptr
                  corresp_tssp =
                       ((a_symbol_ptr)corresp_templ->source_corresp.assoc_info)
                         ->variant.template_info;
  a_routine_ptr   routine = inst->instance_sym->variant.routine.ptr;
  a_symbol_list_entry_ptr
                  sym_entry;

  sym_entry = find_function_template_instantiation(corresp_tssp,
                                                   inst->instance_sym);
  if (sym_entry == NULL) {
    /* The instantiation was not found on the canonical list.  Add it now. */
    add_instantiation(corresp_tssp, inst->instance_sym);
    /* There was no correspondence for that routine. */
    set_no_trans_unit_corresp(routine);
  } else {
    record_trans_unit_corresp(routine,
                              sym_entry->symbol->variant.routine.ptr);
  }  /* if */
}  /* record_function_template_instantiation */


void record_instantiation(a_symbol_ptr                      inst,
                          a_template_symbol_supplement_ptr  tssp)
/*
Check if the given instantiation has a corresponding entry in another
translation unit and record the correspondence if so.  If not, add the
instantiation to the list of all instantiations of the corresponding
template.
*/
{
  if (total_errors != 0) {
    /* Once errors have been detected correspondence checking is no
       longer done so there's no need to maintain this list. */
  } else if (is_primary_translation_unit) {
    if (is_class_struct_union_symbol(inst)) {
      a_type_ptr               prim = type_symbol_type(inst);
      if (prim->variant.class_struct_union.is_prototype_instantiation) {
        /* Nothing to be done. */
      } else {
        a_symbol_list_entry_ptr
                         slep = find_class_template_instantiation(tssp, inst);
        if (slep == NULL) {
          add_instantiation(tssp, inst);
        } else if (slep->symbol != inst) {
          /* This template class was presumably first instantiated in a
             secondary translation unit, but now it is instantiated in the
             primary translation unit.  The new instantiation should become
             the canonical correspondence. */
          a_type_ptr  sec = type_symbol_type(slep->symbol);
          check_assertion(in_secondary_trans_unit(sec));
          (void)seek_class_type_corresp(sec, prim);
          if (inst->defined) {
            slep->symbol = inst;
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (is_function_symbol(inst)) {
       a_symbol_list_entry_ptr
                      slep = find_function_template_instantiation(tssp, inst);
      if (slep == NULL) {
        add_instantiation(tssp, inst);
      } else {
        /* This template class was presumably first instantiated in a
           secondary translation unit, but now it is instantiated in the
           primary translation unit.  The new instantiation should become
           the canonical correspondence. */
        a_routine_ptr  prim = inst->variant.routine.ptr,
                       sec = slep->symbol->variant.routine.ptr;
        check_assertion(in_secondary_trans_unit(sec));
        set_trans_unit_corresp(sec, prim);
      }  /* if */
    }  /* if */
  } else if (correspondence_checking_done) {
    /* This is an instantiation in a secondary translation unit added
       after correspondence checking has been completed, so do catch-up
       correspondence processing. */
    if (is_class_struct_union_symbol(inst)) {
      record_class_template_instantiation(inst);
    } else if (is_function_symbol(inst)) {
      record_function_template_instantiation(
                                          inst->variant.routine.instance_ptr);
    }  /* if */
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

  if (templ != tssp->il_template_entry) {
    /* There can be multiple a_template entries for the same template.  Only
       process the instantiations when encountering the a_template entry that
       is recorded in the template symbol supplement. */
  } else if (templ_sym->kind == (a_symbol_kind)sk_class_template) {
    a_symbol_ptr  inst = tssp->variant.class_template.instantiations,
                  proto_inst;
    for (; inst != NULL; inst = next_instance_sym(inst)) {
      record_class_template_instantiation(inst);
    }  /* for */
    /* Also process the prototype instantiation. */
    proto_inst = ((a_symbol_ptr)
                    ((a_template_ptr)trans_unit_corresp_pointer_of(templ))
                                                   ->source_corresp.assoc_info)
                   ->variant.template_info
                   ->variant.class_template.prototype_instantiation;
    /* For instantiations from template template parameters proto_inst will
       be NULL.  It will also be NULL for nonprototype templates (the
       prototype instantiation is attached to the corresponding prototype
       template). */
    if (proto_inst != NULL) {
      a_type_ptr    class_type = tssp
                              ->variant.class_template.prototype_instantiation
                              ->variant.class_struct_union.type;
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
                              ((a_symbol_ptr)canonical_template_entry_of(templ)
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
       specialization with the same set of parameters and arguments.
       First, however, we must check that they come from corresponding
       primary templates. */
    a_symbol_ptr    prim_templ_sym =
                            tssp->variant.class_template.primary_template_sym;
    a_template_ptr  prim_templ =
                           template_supplement_for_symbol(prim_templ_sym)
                                                          ->il_template_entry;
    a_template_ptr  corresp_prim_templ = corresp_tssp->il_template_entry;
    if (same_template_entities(prim_templ, corresp_prim_templ)) {
      for (sym = corresp_tssp->variant.class_template.partial_specializations;
           sym != NULL;
           sym = sym->next) {
        corresp_tssp = template_supplement_for_symbol(sym);
        if (equiv_template_param_lists(
                                    corresp_tssp->cache.decl_info->parameters,
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
    }  /* if */
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


static void find_template_correspondence(a_template_ptr  templ,
                                         a_boolean       parent_found)
/*
Look for the given type in another translation unit and set the translation
unit correspondence pointer if one is found.  When parent_found is TRUE,
this procedure should not attempt to seek correspondences for parent
entities.
*/
{
  a_boolean     conflict = FALSE;
  a_symbol_ptr  templ_sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(templ_sym != NULL);
  if (is_template_symbol(templ_sym) && !templ_sym->is_template_param) {
    /* Template definitions for nontemplate members of class templates should
       not get here.  Nor should template template parameters. */
    a_template_ptr  corresp_templ = NULL, candidate;
    a_boolean       class_template = is_class_template_symbol(templ_sym);
    a_boolean       first_definition = templ_sym->defined;
    sym = corresp_symbol_list(templ_sym);
    for (; sym != NULL; sym = sym->next) {
      if (sym->decl_scope != templ_sym->decl_scope &&
          may_have_correspondence(sym) &&
          (parent_found ? known_same_parents(sym, templ_sym)
                        : same_parents(sym, templ_sym))) {
        /* Two different declarations in the same namespace and with the same
           name: they should probably match up. */
        if ((is_template_symbol(sym) &&
             is_class_template_symbol(sym) == class_template) ||
             (sym->kind == (a_symbol_kind)sk_overloaded_function &&
                                                           !class_template)) {
          if (class_template) {
            candidate = find_corresp_class_template(templ, sym);
          } else {
            candidate = find_corresp_function_template(templ, sym);
          }  /* if */
          if (candidate == NULL) {
            /* Continue searching for a match. */
          } else if (!class_template || !templ_sym->defined) {
            /* No need to find a "canonical definition". */
            corresp_templ = candidate;
            break;
          } else {
            a_symbol_ptr  corresp_sym =
                           (a_symbol_ptr)candidate->source_corresp.assoc_info;
            if (corresp_sym->defined) {
              /* Both templ and candidate are defined.  Check to see if the
                 candidate is the "canonical definition": the definition at
                 the end of the correspondence chain, or if that is a
                 nondefining declaration of the primary translation unit, the
                 definition whose correspondence pointer points to the end of
                 the correspondence chain. */
              first_definition = FALSE;
              if (!in_secondary_trans_unit(candidate) ||
                  !has_correspondence(candidate)) {
                /* A definition in a primary translation is always a canonical
                   definition.  So is a definition in a secondary translation
                   unit that doesn't have a correspondence (i.e., the first
                   encountered definition with no declaration in the primary
                   translation unit). */
                corresp_templ = candidate;
                break;
              } else {
                /* Check if the canonical definition candidate does indeed
                   point to a nondefining declaration (which should be the
                   root of the correspondence tree). */
                a_template_ptr  cand_root = (a_template_ptr)
                                     trans_unit_corresp_pointer_of(candidate);
                a_symbol_ptr    cand_root_sym = (a_symbol_ptr)
                                         cand_root->source_corresp.assoc_info;
                if (!cand_root_sym->defined) {
                  /* candidate corresponds to the canonical definition since
                     it has a correspondence pointer that points to a
                     nondefining declaration. */
                  corresp_templ = candidate;
                  break;
                }  /* if */
              }  /* if */
              /* sym did not correspond to a canonical definition: continue
                 to look for one. */
            } else if (corresp_templ == NULL) {
              /* templ is defined, but the candidate correspondence is not.
                 Remember the candidate in case there are no others, but
                 continue searching for a canonical definition. */
              corresp_templ = candidate;
            }  /* if */
          }  /* if */
        } else if (!is_class_template_symbol(templ_sym) &&
                   is_function_symbol(sym)) {
          /* A function template can always be overloaded with a nontemplate
             function: no conflict. */
        } else {
          /* An error if the conflicting entity has external linkage. */
          conflict = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
    if (conflict) {
      f_report_bad_trans_unit_corresp((char*)templ, &sym->decl_position);
    } else if (corresp_templ != NULL) {
      /* Record the correspondence. */
      if (class_template && first_definition) {
        /* This is the first definition of a class template.  Make it
           point to the root. */
        corresp_templ = (a_template_ptr)canonical_il_entry_of(corresp_templ);
      }  /* if */
      record_trans_unit_corresp(templ, corresp_templ);
      establish_instantiation_correspondences(templ);
    }  /* if */
    if (trans_unit_corresp_pointer_of(templ) == NULL) {
      /* Mark all instantiations as visited and record them for later lookup.
         */
      clear_instantations_correspondence(templ, /*visited=*/TRUE);
      /* Mark this template as visited. */
      set_no_trans_unit_corresp(templ);
    }  /* if */
  }  /* if */
}  /* find_template_correspondence */


static a_boolean is_main_function(a_routine_ptr  routine)
/*
Return TRUE if and only if the given entry describes a global scope "main"
routine.
*/
{
  a_boolean                    result = FALSE;
  a_source_correspondence_ptr  scp = &routine->source_corresp;
  a_symbol_ptr                 sym = (a_symbol_ptr)scp->assoc_info;

  if (sym != NULL &&
      !scp->is_class_member && scp->parent.namespace_ptr == NULL) {
    result = (strcmp(sym->header->identifier, "main") == 0);
  }  /* if */
  return result;
}  /* is_main_function */


static void find_routine_correspondence(a_routine_ptr  routine)
/*
Look for the given routine in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  routine_sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  check_assertion(routine_sym != NULL);
  sym = corresp_symbol_list(routine_sym);
  if (may_have_correspondence(routine_sym)) {
    for (; sym != NULL; sym = sym->next) {
      /* Don't consider symbols in the same file. */
      if (sym->decl_scope != routine_sym->decl_scope) {
        /* The matching symbol may be part of an overload set. */
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
                  a_routine_ptr  corresp_routine =
                                                  sub_sym->variant.routine.ptr;
                  a_type_ptr     sym_type = corresp_routine->type;
                  if (routine == corresp_routine) {
                    /* Skip this symbol. */
                  } else if (param_types_are_compatible(routine->type,
                                                        sym_type,
                                                        TCF_REDECLARATION |
                                                        TCF_SEEK_CORRESP) ||
                             /* The function ::main doesn't overload. */
                             (is_main_function(routine) &&
                              is_main_function(corresp_routine))) {
                    /* Record the correspondence. */
                    record_trans_unit_corresp(routine,
                                              sub_sym->variant.routine.ptr);
                  } else if (routine->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
                             corresp_routine->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
                    f_report_bad_trans_unit_corresp((char*)routine,
                                                    &sub_sym->decl_position);
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
                f_report_bad_trans_unit_corresp((char*)routine,
                                                &sub_sym->decl_position);
            }  /* switch */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
  if (trans_unit_corresp_pointer_of(routine) == NULL) {
    /* Mark this routine as visited. */
    set_no_trans_unit_corresp(routine);
  }  /* if */
}  /* find_routine_correspondence */


static void find_variable_correspondence(a_variable_ptr  var)
/*
Look for the given variable in another translation unit and set the
translation unit correspondence pointer if one is found.
*/
{
  a_symbol_ptr  var_sym = (a_symbol_ptr)var->source_corresp.assoc_info;
  a_symbol_ptr  sym;

  if (has_name(var) &&
      var_sym != NULL && may_have_correspondence(var_sym)) {
    sym = corresp_symbol_list(var_sym);
    for (; sym != NULL; sym = sym->next) {
      /* Don't consider symbols in the same file. */
      if (sym->decl_scope != var_sym->decl_scope &&
          may_have_correspondence(sym) &&
          same_parents(sym, var_sym)) {
        /* Two different declarations in the same namespace or class, and
           with the same name: they should probably match up. */
        switch (sym->kind) {
          case sk_variable:
            {
              a_variable_ptr  corresp_var = sym->variant.variable.ptr;
              if (var != corresp_var) {
                /* Record the correspondence. */
                record_trans_unit_corresp(var, corresp_var);
                /* If the variable has an anonymous type, assume it matches
                   that of the corresponding entity. */
                if (!has_correspondence(var->type) &&
                    !has_name(var->type) && !has_name(corresp_var->type) &&
                    (is_immediate_class_type(var->type) ||
                     is_immediate_enum_type(var->type))) {
                  record_trans_unit_corresp(var->type, corresp_var->type);
                }  /* if */
              }  /* if */
            }
            break;
          case sk_class_or_struct_tag:
          case sk_union_tag:
          case sk_enum_tag:
            break;
          case sk_type:
            if (sym->variant.type.is_injected_class_name ||
                is_template_param_type_symbol(sym)) break;
            /* FALLTHROUGH */
          default:
            f_report_bad_trans_unit_corresp((char*)var, &sym->decl_position);
        }  /* switch */
      }  /* if */
    }  /* for */
  }  /* if */
  if (trans_unit_corresp_pointer_of(var) == NULL) {
    /* Mark this variable as visited. */
    set_no_trans_unit_corresp(var);
  }  /* if */
}  /* find_variable_correspondence */


static void determine_correspondence(a_source_correspondence_ptr  scp,
                                     an_il_entry_kind             kind)
/*
The given source correspondence is part of an IL entry of the given kind.
If it has not been done already and correspondence checking is still under
way, determine to which other IL entry this might correspond.
*/
{
  /* If we're in the process of establishing correspondences, this particular
     entry may need to be processed now.  Otherwise, it should already have
     been done or no correspondence can be expected. */
  if (correspondence_checking_underway &&
      trans_unit_corresp_pointer_of(scp) == NULL) {
    a_type_ptr  root = NULL;
    /* Class members usually have their correspondence set when their parent
       type is processed.  In those cases we look for the outermost parent
       type. */
    if (scp->is_class_member) {
      root = scp->parent.class_type;
      if (kind == (an_il_entry_kind)iek_type &&
          has_name((a_type_ptr)scp) &&
          ((a_type_ptr)scp)
                    ->variant.class_struct_union.is_prototype_instantiation) {
        /* Prototype instantiations are not always recorded in the IL.
           Therefore, set root to NULL so that the symbol table will be used
           to find the named member instead. */
        root = NULL;
      } else {
        while (root->source_corresp.is_class_member &&
               trans_unit_corresp_pointer_of(root) == NULL) {
          root = root->source_corresp.parent.class_type;
        }  /* while */
      }  /* if */
    }  /* if */
    if (root == NULL) {
      /* Not a class member. */
      switch (kind) {
        case iek_namespace:
          find_namespace_correspondence((a_namespace_ptr)scp);
          break;
        case iek_routine:
          {
            a_routine_ptr  routine = (a_routine_ptr)scp;
            if (routine->is_template_function) {
              record_function_template_instantiation(
               ((a_symbol_ptr)scp->assoc_info)->variant.routine.instance_ptr);
            } else {
              find_routine_correspondence((a_routine_ptr)scp);
            }  /* if */
          }
          break;
        case iek_variable:
          find_variable_correspondence((a_variable_ptr)scp);
          break;
        case iek_type:
          {
            a_type_ptr  type = (a_type_ptr)scp;
            if (is_immediate_class_type(type) &&
                type->variant.class_struct_union.is_template_class &&
                type->variant.class_struct_union.extra_info
                                                ->template_arg_list != NULL) {
              record_class_template_instantiation(
                              (a_symbol_ptr)type->source_corresp.assoc_info);
            } else {
              find_type_correspondence((a_type_ptr)scp,
                                       (a_boolean)scp->is_class_member);
            }  /* if */
          }
          break;
        case iek_template:
          find_template_correspondence((a_template_ptr)scp,
                                       (a_boolean)scp->is_class_member);
          break;
        default:
          unexpected_condition_str("Unexpected IL entry kind");
      }  /* switch */
    } else if (trans_unit_corresp_pointer_of(root) == NULL) {
      /* A member of a class that was not yet visited. */
      if (root->variant.class_struct_union.is_template_class &&
          root->variant.class_struct_union.extra_info
                                                ->template_arg_list != NULL) {
        record_class_template_instantiation(
                              (a_symbol_ptr)root->source_corresp.assoc_info);
      } else {
        find_type_correspondence(root, /*parent_found=*/FALSE);
      }  /* if */
    }  /* if */
    if (trans_unit_corresp_pointer_of(scp) == NULL) {
      /* A correspondence error at an outer level prevent this entry from
         having a correspondence.  Mark it and its unvisited ancestors as
         having no correspondence. */
      set_no_trans_unit_corresp(scp);
      if (scp->is_class_member) {
        a_type_ptr  parent = scp->parent.class_type;
        while (parent != root) {
          set_no_trans_unit_corresp(parent);
          parent = parent->source_corresp.parent.class_type;
        }  /* while */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* determine_correspondence */


a_namespace_ptr canonical_namespace_entry_of(a_namespace_ptr nsp)
/*
Return the canonical entry established for the given namespace entry.
(Should not be called until the namespaces have already been visited for
correspondences with other translation units.)
*/
{
  a_namespace_ptr  result = nsp;

  if (nsp != NULL && in_secondary_trans_unit(nsp)) {
    /* If we're in the process of establishing correspondences, this particular
       entry may need to be processed now.  Otherwise, it should already have
       been done or no correspondence can be expected. */
    determine_correspondence(&nsp->source_corresp, iek_namespace);
    result = (a_namespace_ptr)canonical_il_entry_of(nsp);
  }  /* if */
  return result;
}  /* canonical_namespace_entry_of */


a_field_ptr canonical_field_entry_of(a_field_ptr field)
/*
If the given field entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established canonical
entry.
*/
{
  a_field_ptr  result = field;

  if (field != NULL && in_secondary_trans_unit(field)) {
    /* If we're in the process of establishing correspondences, this particular
       entry may need to be processed now.  Otherwise, it should already have
       been done or no correspondence can be expected. */
    determine_correspondence(&field->source_corresp, iek_field);
    result = (a_field_ptr)canonical_il_entry_of(field);
  }  /* if */
  return result;
}  /* canonical_field_entry_of */


a_routine_ptr canonical_routine_entry_of(a_routine_ptr routine)
/*
If the given routine entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established canonical
entry.
*/
{
  a_routine_ptr  result = routine;

  if (routine != NULL && in_secondary_trans_unit(routine)) {
    determine_correspondence(&routine->source_corresp, iek_routine);
    result = (a_routine_ptr)canonical_il_entry_of(routine);
  }  /* if */
  return result;
}  /* canonical_routine_entry_of */


a_variable_ptr canonical_variable_entry_of(a_variable_ptr var)
/*
If the given variable entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established canonical
entry.
*/
{
  a_variable_ptr              result = var;

  if (var != NULL && in_secondary_trans_unit(var)) {
    determine_correspondence(&var->source_corresp, iek_variable);
    result = (a_variable_ptr)canonical_il_entry_of(var);
  }  /* if */
  return result;
}  /* canonical_variable_entry_of */


a_type_ptr canonical_type_entry_of(a_type_ptr type)
/*
If the given type entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established
canonical entry.
*/
{
  a_type_ptr              result = type;

  if (type != NULL && in_secondary_trans_unit(type) &&
      /* Do not attempt to find a match for a type instantiated from a
         template template parameter. */
      !(is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL &&
        assoc_template_of(type) != NULL &&
        assoc_template_of(type)->kind ==
                           (a_template_kind)templk_template_template_param)) {
    determine_correspondence(&type->source_corresp, iek_type);
    result = (a_type_ptr)canonical_il_entry_of(type);
  }  /* if */
  return result;
}  /* canonical_type_entry_of */


a_template_ptr canonical_template_entry_of(a_template_ptr templ)
/*
If the given template entry has not yet been examined for a corresponding entry
in another translation unit, do so now.  Then return the established
canonical entry.
*/
{
  a_template_ptr              result = templ;

  if (templ != NULL && in_secondary_trans_unit(templ)) {
    determine_correspondence(&templ->source_corresp, iek_template);
    result = (a_template_ptr)canonical_il_entry_of(templ);
  }  /* if */
  return result;
}  /* canonical_template_entry_of */


static void establish_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
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
      } else {
        set_no_namespace_correspondence(nsp);
        set_no_trans_unit_corresp(nsp);
      }  /* if */
    }  /* for */
  }

  /* Visit all types. */
  {
    a_type_ptr  type;
    for (type = skip_generated_type(scope->types);
         type != NULL;
         type = skip_generated_type(type->next)) {
      if (is_immediate_class_type(type) &&
          type->variant.class_struct_union.is_template_class &&
          type->variant.class_struct_union.is_specialized) {
        /* Explicit specializations are handled with the template they
           specialize. */
      } else {
        find_type_correspondence(type, /*parent_found=*/FALSE);
      }  /* if */
    }  /* for */
  }

  /* Visit all routines. */
  {
    a_routine_ptr  routine;
    for (routine = skip_generated_routine(scope->routines);
         routine != NULL;
         routine = skip_generated_routine(routine->next)) {
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

  /* Visit all templates.  This will also examine instantiations and must
     therefore occur after the other entities since they can appear in
     template arguments. */
  {
    a_template_ptr  templ;
    for (templ = scope->templates; templ != NULL; templ = templ->next) {
      find_template_correspondence(templ,
                                   /*parent_found=*/FALSE);
    }  /* for */
  }
}  /* establish_trans_unit_correspondences_for_scope */


static void verify_trans_unit_correspondences_for_scope(a_scope_ptr  scope)
/*
Verify correspondences for all the applicable entities in the given
scope.  The process is repeated in nested scopes.
*/
{
  verify_namespace_correspondences_for_scope(scope);
  verify_type_correspondences_for_scope(scope);
  verify_routine_correspondences_for_scope(scope);
  verify_variable_correspondences_for_scope(scope);
  verify_template_correspondences_for_scope(scope);
}  /* verify_trans_unit_correspondences_for_scope */


void set_trans_unit_correspondences(void)
/*
Establish correspondences between entities with linkage in the
current (secondary) translation unit and ones in other translation
units.
*/
{
  a_scope_ptr  file_scope = curr_translation_unit->primary_scope;

  correspondence_checking_underway = TRUE;
  establish_trans_unit_correspondences_for_scope(file_scope);
  verify_trans_unit_correspondences_for_scope(file_scope);
  correspondence_checking_underway = FALSE;
  correspondence_checking_done = TRUE;
}  /* set_trans_unit_correspondences */


static a_boolean is_corresponding_sym_in_trans_unit(
				char			*canonical_entry,
				a_symbol_ptr		candidate_sym,
				a_translation_unit_ptr	tup)
/*
Return TRUE if candidate_sym is defined in the translation unit specified
by tup and refers to an IL entry whose canonical entry is canonical_entry.
*/
{
  char			*entry;
  an_il_entry_kind	il_kind;
  a_boolean		result;

  entry = il_entry_for_symbol(candidate_sym, &il_kind);
  entry = canonical_il_entry_of(entry);
  result = entry == canonical_entry &&
           symbol_is_from_trans_unit(candidate_sym, tup);
  return result;
}  /* is_corresponding_sym_in_trans_unit */


a_symbol_ptr find_corresponding_class_instance_in_trans_unit(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup)
/*
sym_to_find is a template class instance.  Find the corresponding
instance in the translation unit specified by "tup".  Return the
corresponding instance, or NULL if no corresponding instance is found.
*/
{
  a_symbol_ptr				result_sym = NULL;
  a_symbol_ptr				template_sym;
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				candidate_sym;
  char					*canonical_entry;
  an_il_entry_kind			il_kind;

  /* Get the canonical IL entry associated with sym_to_find. */
  canonical_entry = il_entry_for_symbol(sym_to_find, &il_kind);
  canonical_entry = canonical_il_entry_of(canonical_entry);
  check_assertion(canonical_entry != NULL);
  /* Get the corresponding template in the specified translation unit.
     Note that it is possible that there is no such corresponding template. */
  template_sym = template_symbol_for_class_symbol(sym_to_find);
  template_sym = find_corresponding_symbol_in_trans_unit(template_sym, tup);
  if (template_sym != NULL) {
    tssp = template_supplement_for_symbol(template_sym);
    candidate_sym = tssp->variant.class_template.prototype_instantiation;
    /* First check whether the prototype instantiation is a match. */
    if (is_corresponding_sym_in_trans_unit(canonical_entry,
                                           candidate_sym, tup)) {
      result_sym = candidate_sym;
    } else {
      /* Check whether a partial specialization prototype instantiation is
         a match. */
      a_symbol_ptr	ps_sym;
      for (ps_sym = tssp->variant.class_template.partial_specializations;
           ps_sym != NULL; ps_sym = ps_sym->next) {
        candidate_sym = ps_sym->variant.template_info->
                                variant.class_template.prototype_instantiation;
        if (is_corresponding_sym_in_trans_unit(canonical_entry,
                                               candidate_sym, tup)) {
          result_sym = candidate_sym;
          break;
        }  /* if */
      }  /* for */
      if (result_sym == NULL) {
        /* We still haven't found a match.  Go through the instantiations
           list. */
        a_symbol_ptr	inst_sym;
        for (inst_sym = tssp->variant.class_template.instantiations;
             inst_sym != NULL; inst_sym = inst_sym->next) {
          if (is_corresponding_sym_in_trans_unit(canonical_entry,
                                                 inst_sym, tup)) {
            result_sym = inst_sym;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (result_sym == NULL) {
      /* No symbol was found.  Instantiate the class in the other translation
         unit. */
      a_template_arg_ptr	templ_arg_list;
      a_type_ptr		class_type;
      class_type = sym_to_find->variant.class_struct_union.type;
      /* This routine cannot create a new prototype instantiation in the other
         translation unit. */
      check_assertion(!class_type->
                        variant.class_struct_union.is_prototype_instantiation);
      templ_arg_list = copy_template_arg_list(
                                         templ_arg_list_for_class(class_type));
      result_sym = find_template_class(template_sym, &templ_arg_list,
                                       /*any_prototype_allowed=*/FALSE,
                                       (a_symbol_ptr)NULL);
    }  /* if */
  }  /* if */
  return result_sym;
}  /* find_corresponding_class_instance_in_trans_unit */


static a_symbol_ptr find_corresponding_symbol_on_symbol_list(
				a_symbol_ptr		sym_to_find,
				a_symbol_ptr		symbols,
				a_boolean		is_routine,
				a_type_ptr		parent_class,
				a_namespace_ptr		parent_namespace,
				char			*canonical_entry,
				a_translation_unit_ptr	tup)
/*
Look through the list of symbols specified by "symbols" to find one
from the translation unit "tup" that corresponds to "sym_to_find".
is_routine is TRUE if sym_to_find is a routine symbol.  "parent_class" and
"parent_namespace" are used to indicate the class or namespace (if any)
of the corresponding symbol in the other translation unit.
*/
{
  a_symbol_ptr		result_sym = NULL;
  a_symbol_ptr		sym;

  /* If we did not decide to use a special symbol list above, use the
     inactive list. */
  if (symbols == NULL) symbols = corresp_symbol_list(sym_to_find);
  for (sym = symbols; sym != NULL; sym = sym->next) {
    a_boolean		is_list;
    a_symbol_ptr	sym_to_check;
    /* Check the kind of the symbol to see if it is a potential match. */
    if (is_routine) {
      if (!is_function_or_template_symbol(sym)) continue;
    } else {
      if (sym->kind != sym_to_find->kind) continue;
    }  /* if */
    /* The kind matches, if there is a parent pointer, make sure the parents
       match. */
    if (parent_class != NULL) {
      /* We are looking for a class member.  Make sure it is a member of the
         right class. */
      if (!sym->is_class_member ||
          sym->parent.class_type != parent_class) continue;
    } else if (parent_namespace != NULL) {
      /* We are looking for a namespace member.  Make sure it is a member of
         the right namespace. */
      if (sym->is_class_member ||
          sym->parent.namespace_ptr != parent_namespace) continue;
    }  else {
      /* We are looking for a symbol that is not a class or namespace
         member. */
      if (sym->is_class_member || sym->parent.namespace_ptr != NULL) continue;
    }  /* if */
    /* If the symbol is an overload set we must check each member of the
       set. */
    is_list = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    sym_to_check = is_list ? sym->variant.overloaded_function.symbols : sym;
    for (; sym_to_check != NULL;
         sym_to_check = is_list ? sym_to_check->next : NULL) {
      if (is_corresponding_sym_in_trans_unit(canonical_entry,
                                             sym_to_check, tup)) {
        result_sym = sym_to_check;
        break;
      }  /* if */
    }  /* for */
    /* Exit the loop if we found a match. */
    if (result_sym != NULL) break;
  }  /* for */
  return result_sym;
}  /* find_corresponding_symbol_on_symbol_list */


static a_symbol_ptr find_corresponding_symbol(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup)
/*
Find the symbol in the translation unit specified by tup that
corresponds to sym_to_find.  Return a pointer to the symbol found, or
NULL if none is found.
*/
{
  a_symbol_ptr		result_sym = NULL;
  a_boolean		is_routine;
  a_symbol_ptr		parent_sym;
  a_type_ptr		parent_class = NULL;
  a_namespace_ptr	parent_namespace = NULL;
  char			*canonical_entry;
  an_il_entry_kind	il_kind;
  a_symbol_ptr		symbols = NULL;
  a_symbol_list_entry_ptr
			symbol_list = NULL;

  /* When searching for a routine symbol, we may have to inspect overload
     sets. */
  is_routine = is_function_or_template_symbol(sym_to_find);
  /* If this is a class or namespace member, get the corresponding parent. */
  if (sym_to_find->is_class_member) {
    /* Find the corresponding parent class. */
    a_class_symbol_supplement_ptr	cssp;
    a_special_function_kind		special_kind;
    parent_sym = (a_symbol_ptr)sym_to_find->
                                  parent.class_type->source_corresp.assoc_info;
    parent_sym = find_corresponding_symbol_in_trans_unit(parent_sym, tup);
    if (parent_sym != NULL) {
      parent_class = parent_sym->variant.class_struct_union.type;
      complete_class_type_is_needed(parent_class);
      special_kind = special_function_kind_for_symbol(sym_to_find);
      cssp = parent_sym->variant.class_struct_union.extra_info;
      /* Constructor and destructor routines are not entered into the symbol
         table, so are found using the appropriate symbols from the class
         symbols supplement. */
      switch (special_kind) {
        case sfk_constructor:
          symbols = cssp->constructor;
          break;
        case sfk_destructor:
          symbols = cssp->destructor;
          break;
        case sfk_conversion:
          /* There are two lists of conversion operators.  One for templates
             and one for non-templates. */
          if (sym_to_find->kind == (a_symbol_kind)sk_function_template) {
            symbol_list = cssp->conversion_template_list;
          } else {
            symbol_list = cssp->conversion_list;
          }  /* if */
          break;
        default:
          break;
      }  /* switch */
    }  /* if */
  } else if (sym_to_find->parent.namespace_ptr != NULL) {
    /* Find the corresponding parent namespace. */
    parent_sym = (a_symbol_ptr)sym_to_find->
                               parent.namespace_ptr->source_corresp.assoc_info;
    parent_sym = find_corresponding_symbol_in_trans_unit(parent_sym, tup);
    if (parent_sym != NULL) {
      parent_namespace = parent_sym->variant.namespace_info.ptr;
    }  /* if */
  }  /* if */
  /* Get the canonical IL entry associated with sym_to_find. */
  canonical_entry = il_entry_for_symbol(sym_to_find, &il_kind);
  canonical_entry = canonical_il_entry_of(canonical_entry);
  check_assertion(canonical_entry != NULL);
  if (symbol_list != NULL) {
    /* Find the symbol on a list of symbol list entries. */
    a_symbol_list_entry_ptr	slep;
    for (slep = symbol_list; slep != NULL; slep = slep->next) {
      a_symbol_ptr	sym_to_check = slep->symbol;
      if (is_corresponding_sym_in_trans_unit(canonical_entry,
                                             sym_to_check, tup)) {
        result_sym = sym_to_check;
        break;
      }  /* if */
    }  /* for */
  } else {
    /* Find the symbol on a list of symbols linked by the next
       pointer of the symbol. */
    result_sym = find_corresponding_symbol_on_symbol_list(
                      sym_to_find, symbols, is_routine, parent_class,
                      parent_namespace, canonical_entry, tup);
  }  /* if */
  /* Make sure the canonical entry didn't change during this processing.
     The canonical entry can change if a entity that originally did not
     exist in the primary translation unit is later added.  This routine
     is structured such that the canonical entry pointer is fetched at a
     point after the point at which new entities may have been added. */
  check_assertion(canonical_entry == canonical_il_entry_of(canonical_entry));
  return result_sym;
}  /* find_corresponding_symbol */


a_symbol_ptr find_corresponding_symbol_in_trans_unit(
					a_symbol_ptr		sym_to_find,
					a_translation_unit_ptr	tup)
/*
Find a symbol associated with the translation unit specified by tup
that is refers to an entity that corresponds to sym_to_find.
*/
{
  a_symbol_ptr		result_sym = NULL;

  if (is_template_class_symbol(sym_to_find)) {
    /* For a class instance, we need to look through the instantiations
       list. */
    result_sym = find_corresponding_class_instance_in_trans_unit(
                                            sym_to_find, tup);
  } else {
    /* The normal case -- look for the corresponding symbol on the inactive
       list. */
    result_sym = find_corresponding_symbol(sym_to_find, tup);
  }  /* if */
  return result_sym;
}  /* find_corresponding_symbol_in_trans_unit */


void corresp_one_time_init(void)
/*
Do one-time initialization of variables related to correspondence
checking.
*/
{
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(correspondence_checking_underway);
  register_trans_unit_variable(correspondence_checking_done);
}  /* corresp_one_time_init */


void corresp_trans_unit_init(void)
/* 
Initialize things related to correspondence checking that must be
re-initialized for each translation unit.
*/
{
  correspondence_checking_underway = FALSE;
  correspondence_checking_done = FALSE;
}  /* corresp_trans_unit_init */


void corresp_init(void)
/* 
Initialize things related to correspondence checking that must be initialized
for each compilation.
*/
{
  /* Static variables declared in trans_corresp.c.
     We depend on NULL being represented as zero bits here. */
  memzero((char *)canonical_int_types, sizeof(canonical_int_types));
  memzero((char *)canonical_signed_int_types,
          sizeof(canonical_signed_int_types));
#if MICROSOFT_EXTENSIONS_ALLOWED
  memzero((char *)canonical_microsoft_sized_int_types,
          sizeof(canonical_microsoft_sized_int_types));
  memzero((char *)canonical_microsoft_sized_signed_int_types,
          sizeof(canonical_microsoft_sized_signed_int_types));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  memzero((char *)canonical_float_types, sizeof(canonical_float_types));
#if C99_IL_EXTENSIONS_SUPPORTED
  memzero((char *)canonical_complex_types, sizeof(canonical_complex_types));
  memzero((char *)canonical_imaginary_types,
          sizeof(canonical_imaginary_types));
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  canonical_il_void_type = NULL;
  canonical_il_wchar_t_type = NULL;
  canonical_il_bool_type = NULL;
}  /* corresp_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
