/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il.c -- Construction of intermediate language trees.

*/

#include "basics.h"
#include "host_envir.h"
#include "il.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "target.h"
#include "lang_feat.h"
#include "symbol_tbl.h"
#include "error.h"
#include "types.h"
#include "cmd_line.h"
#include "float_pt.h"
#include "exprutil.h"
#include "folding.h"

#if ALTERNATE_IL_FILE_FORMAT
#include "il_file.h"
#endif /* ALTERNATE_IL_FILE_FORMAT */

#if !STANDALONE_UTILITY_PROGRAM
/*
Pointers to shared types.  These are cleared by il_init.
*/
static a_type_ptr int_types[(int)ik_last];
static a_type_ptr float_types[(int)fk_last];
#define MAX_TRACKED_STRING_TYPE_LENGTH 80
static a_type_ptr string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
static a_type_ptr wide_string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
static a_type_ptr il_signed_int_type;
static a_type_ptr il_error_type;
static a_type_ptr il_unknown_type;
static a_type_ptr il_void_type;

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if DEBUG
#if !STANDALONE_UTILITY_PROGRAM
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_constants_allocated,
		num_param_types_allocated,
		num_routine_type_supplements_allocated,
		num_based_type_list_members_allocated,
                num_access_adjustments_allocated,
                num_class_list_entries_allocated,
		num_class_type_supplements_allocated,
                num_overriding_virtual_functions_allocated,
                num_derivation_steps_allocated,
                num_base_classes_allocated,
		num_types_allocated,
		num_dynamic_inits_allocated,
		num_variables_allocated,
		num_fields_allocated,
		num_routines_allocated,
		num_asm_entries_allocated,
		num_labels_allocated,
		num_expr_nodes_allocated,
		num_switch_clauses_allocated,
		num_blocks_allocated,
		num_statements_allocated,
                num_constructor_inits_allocated,
		num_scopes_allocated,
		string_literal_text_space_allocated;
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if ALTERNATE_IL_FILE_FORMAT
static unsigned long
		num_il_entry_numbers_allocated;
#endif /* ALTERNATE_IL_FILE_FORMAT */
#if !STANDALONE_UTILITY_PROGRAM

/*
Number of times the based_types lists of types are searched for related types.
*/
static unsigned long
		num_get_based_type_calls;
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* DEBUG */
#if !STANDALONE_UTILITY_PROGRAM

/*
Hash table containing shareable constants (i.e., constants that can be
reused when necessary, representing simple literal constants, so that
for example there would only be one constant for the literal "0").
*/
#define SIZE_SHAREABLE_CONSTANTS_TABLE 293
			/* Size of the table; should be about twice
			   the expected number of entries for a big
			   program, and must be prime. */
typedef unsigned int a_constant_hash_value;
static a_constant_ptr
		shareable_constants_table[SIZE_SHAREABLE_CONSTANTS_TABLE];
			/* Each entry in the table points to a linear
			   linked list of constant entries.  The function
			   hash_constant is used to determine the table
			   entry/list that corresponds to a given constant. */

#if DEBUG
static unsigned long
		num_shareable_constants,
		num_func_shareable_constants,
		num_used_shareable_constant_buckets,
		num_searches_for_shareable_constants,
		num_compares_for_shareable_constants;
#endif /* DEBUG */


/*
Data structure used to keep track of the rewriting of temporaries when
copying expression trees (when an expression tree is copied, the temporaries
must be rewritten so that those in the original expression do not conflict
with those in the copy).
*/
typedef struct a_rewritten_temporary *a_rewritten_temporary_ptr;
typedef struct a_rewritten_temporary {
  a_rewritten_temporary_ptr
		next;	/* Pointer to the next entry on the list of rewritten
			   temporaries, or NULL if this is the last entry. */
  a_variable_ptr
		old_temp,
		new_temp;
			/* The old temporary and the new temporary it's
			   changed into. */
} a_rewritten_temporary;
static a_rewritten_temporary_ptr
		avail_rewritten_temporaries;
			/* List of rewritten temporary entries that have
			   been freed and are available for reuse. */
static a_rewritten_temporary_ptr
		rewritten_temporaries;
			/* While copying an expression tree, this is the
			   list of rewrites to be done. */
static a_boolean
		clone_temps_on_expr_copy;
			/* If TRUE, temporary variables should be "cloned"
			   (replaced by new equivalent temporaries) during
			   expression copying. */


/* Forward declarations needed because of mutual recursion: */
static a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr dip);
static an_expr_node_ptr internal_copy_expr_tree(an_expr_node_ptr expr);
static an_expr_node_ptr internal_copy_list_of_expr_trees(
                                                   an_expr_node_ptr expr_list);

#if DEBUG
/* Forward declaration needed because of mutual recursion. */
void db_type(a_type *tp);


static char *db_int_type_name(an_integer_kind kind)
/*
Return a pointer to a string describing the integer type indicated by kind.
*/
{
  char *p;

  switch (kind) {
    case ik_char:            p = "char";            break;
    case ik_signed_char:     p = "signed char";     break;
    case ik_unsigned_char:   p = "unsigned char";   break;
    case ik_short:           p = "short";           break;
    case ik_unsigned_short:  p = "unsigned short";  break;
    case ik_int:             p = "int";             break;
    case ik_unsigned_int:    p = "unsigned int";    break;
    case ik_long:            p = "long";            break;
    case ik_unsigned_long:   p = "unsigned long";   break;
    default:                 p = "<bad integer kind>";
  }  /* switch */
  return p;
}  /* db_int_type_name */


static char *db_float_type_name(a_float_kind kind)
/*
Return a pointer to a string describing the float type indicated by kind.
*/
{
  char *p;

  switch (kind) {
    case fk_float:           p = "float";           break;
    case fk_double:          p = "double";          break;
    case fk_long_double:     p = "long double";     break;
    default:                 p = "<bad float kind>";
  }  /* switch */
  return p;
}  /* db_float_type_name */


void db_name(a_source_correspondence *sc)
/*
Dump the name from a source correspondence (if any).
*/
{
  if (sc->name != NULL) {
    if (sc->class_of_which_a_member != NULL) {
      db_name(&sc->class_of_which_a_member->source_corresp);
      fputs("::", f_debug);
    }  /* if */
    fputs(sc->name, f_debug);
  } else {
    fputs("(null)", f_debug);
  }  /* if */
}  /* db_name */


static void db_name_linkage(a_source_correspondence *sc)
/*
Dump the name linkage from a source correspondence.
*/
{
  char *str;

  switch (sc->name_linkage) {
    case nlk_none:                str = "no"; break;
    case nlk_internal:            str = "int'l"; break;
    case nlk_external:            str = "ext'l"; break;
    case nlk_cplusplus_external:  str = "C++"; break;
    default:                      str = "<BAD KIND>"; break;
  }  /* switch */
  fprintf(f_debug, "%s linkage", str);
}  /* db_name_linkage */


void db_abbreviated_type(a_type *tp)
/*
Dump a type in abbreviated form.  This is particularly required for
classes, structs, unions, which may contain fields that point to
objects of their own type.
*/
{
  if (tp == NULL) {
    fputs("<null type pointer>", f_debug);
  } else {
    switch (tp->kind) {
      case tk_class:
        fputs("class ", f_debug);
        goto print_name;
      case tk_struct:
        fputs("struct ", f_debug);
        goto print_name;
      case tk_union:
        fputs("union ", f_debug);
print_name:
        db_name(&tp->source_corresp);
        break;
      default:
        db_type(tp);
        break;
    }  /* switch */
  }  /* if */
}  /* db_abbreviated_type */


void db_access_control(an_access_specifier as)
/*
Dump an access control specifier.
*/
{
  switch (as) {
    case as_public:	  fputs("public", f_debug);	  break;
    case as_protected:	  fputs("protected", f_debug);	  break;
    case as_private:	  fputs("private", f_debug);	  break;
    case as_inaccessible: fputs("inaccessible", f_debug); break;
    default:              fputs("<bad access kind>", f_debug); break;
  }  /* switch */
}  /* db_access_control */


static void db_field(a_field *fp,
                     int     depth)
/*
Dump a field entry, for debug purposes.
*/
{
  a_targ_size_t  byte_offset;
  int            i, bit_offset_at_byte;

  fputs("\n  ", f_debug);
  if (depth > 0) for (i = depth; i > 0; --i) fputs("  ", f_debug);
  if (C_dialect == C_dialect_cplusplus) {
    db_access_control(fp->source_corresp.access);
    (void)fputc(' ', f_debug);
  }  /* if */
  fputs("field \"", f_debug);
  db_name(&fp->source_corresp);
  fputs("\", type = ", f_debug);
  db_abbreviated_type(fp->type);
  byte_offset = fp->bit_offset / TARG_CHAR_BIT;
  bit_offset_at_byte = fp->bit_offset - byte_offset * TARG_CHAR_BIT;
  if (bit_offset_at_byte > 0 || fp->bit_size > 0) {
    fprintf(f_debug, ", bit offset %lu", fp->bit_offset);
    if (byte_offset > 0) {
      fprintf(f_debug, " (%lu+%lu)", byte_offset, bit_offset_at_byte);
    }  /* if */
    if (fp->bit_size > 0) {
      fprintf(f_debug, ", bit size %d", fp->bit_size);
    }  /* if */
  } else {
    fprintf(f_debug, ", offset %lu", byte_offset);
  }  /* if */
}  /* db_field */


static void db_static_data_member(a_variable_ptr vp)
/*
Dump a static data member (a variable entry), for debug purposes.
*/
{
  fputs("\n  ", f_debug);
  db_access_control(vp->source_corresp.access);
  fputs(" static data member \"", f_debug);
  db_name(&vp->source_corresp);
  fputs("\" (", f_debug);
  db_name_linkage(&vp->source_corresp);
  fprintf(f_debug, "), sc_%s, type = ",
                   db_storage_class_names[(int)vp->storage_class]);
  db_abbreviated_type(vp->type);
}  /* db_static_data_member */


static void db_member_function(a_routine_ptr rp)
/*
Dump a member function (a routine entry), for debug purposes.
*/
{
  fputs("\n  ", f_debug);
  db_access_control(rp->source_corresp.access);
  if (!routine_type_is_nonstatic_member_function(rp->type)) {
    fputs(" static", f_debug);
  }  /* if */
  if (rp->is_virtual) {
    if (rp->pure_virtual) fputs(" pure", f_debug);
    fprintf(f_debug, " virtual (%d)", rp->virtual_function_number);
  }  /* if */
  fputs(" member function \"", f_debug);
  db_name(&rp->source_corresp);
  fputs("\" (", f_debug);
  db_name_linkage(&rp->source_corresp);
  fprintf(f_debug, ")%s, sc_%s,\n    type = ",
                   (rp->is_inline) ? ", inline" : "",
                   db_storage_class_names[(int)rp->storage_class]);
  db_abbreviated_type(rp->type);
}  /* db_member_function */


static void db_virtual_function_info(a_class_type_supplement_ptr ctsp,
                                     int                         depth)
/*
Dump the virtual_function_info_offset field of a class_type_supplement, for
debug purposes.
*/
{
  int i;

  if (ctsp->virtual_function_count > 0) {
    if (depth == -1) {
      fputs("  ", f_debug);
    } else {
      fputs("\n    ", f_debug);
      for (i = depth; i > 0; --i) fputs("  ", f_debug);
    }  /* if */
    fprintf(f_debug, "byte offset for virtual function table ptr = %lu",
                     ctsp->virtual_function_info_offset);
    if (depth == -1) (void)fputc('\n', f_debug);
  }  /* if */
}  /* db_virtual_function_info */


static void db_virtual_base_class_ptr(a_base_class *bcp,
                                      int          depth)
{
  int        i;

  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fputs("[[ virtual ", f_debug);
  db_access_control(bcp->access);
  fprintf(f_debug, " base class %s", bcp->type->source_corresp.name);
  fprintf(f_debug, " (pointer offset = %lu", bcp->pointer_offset);
  if (bcp->pointer_base_class != NULL) {
    fprintf(f_debug, ", in %s",
            bcp->pointer_base_class->type->source_corresp.name);
  }  /* if */
  fputs(") ]]", f_debug);
}  /* db_virtual_base_class_ptr */


static void db_virtual_base_class(a_base_class *bcp,
                                  int          depth);

static void db_direct_base_class(a_base_class *bcp,
                                 int          depth)
/*
Dump a direct base class entry, for debug purposes.
*/
{
  a_type     *tp = bcp->type;
  a_field    *fp;
  int        i;
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
  a_boolean  complete_subobject = bcp->complete_subobject;
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */

  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fputs("[[ ", f_debug);
  if (bcp->is_virtual) {
    fputs("virtual ", f_debug);
  }  /* if */
  db_access_control(bcp->access);
  fprintf(f_debug, " base class %s", tp->source_corresp.name);
  if (bcp->is_virtual) {
    fprintf(f_debug, " (pointer offset = %lu", bcp->pointer_offset);
    if (bcp->pointer_base_class != NULL) {
      fprintf(f_debug, ", in %s",
              bcp->pointer_base_class->type->source_corresp.name);
    }  /* if */
    fputc(')', f_debug);
  } else {
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
    if (complete_subobject) fputs(" (complete subobj)", f_debug);
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
    fprintf(f_debug, " (offset = %lu)", bcp->offset);
    for (bcp = tp->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->direct && !bcp->is_virtual) {
        db_direct_base_class(bcp, depth+1);
      }  /* if */
    }  /* for */
    fp = tp->variant.class_struct_union.field_list;
    while (fp != NULL) {
      db_field(fp, depth+1);
      fp = fp->next;
    }  /* while */
    /* Put out the virtual base class pointers. */
    for (bcp = tp->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->direct && bcp->is_virtual) {
        db_direct_base_class(bcp, depth+1);
      }  /* if */
    }  /* for */
    db_virtual_function_info(tp->variant.class_struct_union.extra_info, depth);
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
    if (complete_subobject) {
      /* Put out the virtual base class data sections. */
      for (bcp = tp->variant.class_struct_union.extra_info->base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->direct && bcp->is_virtual &&
            bcp->data_section_base_class == NULL) {
          db_virtual_base_class(bcp, depth+1);
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
  }  /* if */
  fputs(" ]]", f_debug);
}  /* db_direct_base_class */


static void db_indirect_base_class(a_base_class *bcp)
/*
Dump an indirect base class entry, for debug purposes.
*/
{
  a_derivation_step_ptr  dsp;

  fprintf(f_debug, "\n    %s", bcp->type->source_corresp.name);
  if (bcp->is_virtual) fputs(", is_virtual", f_debug);
  if (bcp->ambiguous) fputs(", ambiguous", f_debug);
  if (bcp->any_virtual_steps_in_derivation) fputs (", virtual steps", f_debug);
  fputs(", path = ", f_debug);
  dsp = bcp->derivation;
  if (dsp == NULL) {
    fputs("<null>", f_debug);
  } else {
    for (; dsp != NULL; dsp = dsp->next) {
      fprintf(f_debug, "==>%s",
              (dsp->base_class == NULL || dsp->base_class->type == NULL) ?
                  "<???>" : dsp->base_class->type->source_corresp.name);
    }  /* for */
  }  /* if */
}  /* db_indirect_base_class */


static void db_virtual_base_class(a_base_class *bcp,
                                  int          depth)
/*
Dump a virtual base class entry, for debug purposes.
*/
{
  a_type       *tp = bcp->type;
  a_field      *fp;
  int          i;
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
  a_boolean    complete_subobject = bcp->complete_subobject;
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
  
  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fprintf(f_debug, "[( virtual base class %s (offset = %lu",
		   tp->source_corresp.name, bcp->offset);
  if (bcp->data_section_base_class != NULL) {
    fprintf(f_debug, ", in %s",
            bcp->data_section_base_class->type->source_corresp.name);
  }  /* if */
  fputc(')', f_debug);
  if (bcp->data_section_base_class == NULL) {
    for (bcp = tp->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->direct && !bcp->is_virtual) {
        db_direct_base_class(bcp, depth+1);
      }  /* if */
    }  /* for */
    fp = tp->variant.class_struct_union.field_list;
    while (fp != NULL) {
      db_field(fp, depth+1);
      fp = fp->next;
    }  /* while */
    for (bcp = tp->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->direct && bcp->is_virtual) {
        db_direct_base_class(bcp, depth+1);
      }  /* if */
    }  /* for */
    db_virtual_function_info(tp->variant.class_struct_union.extra_info, depth);
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
    if (complete_subobject) {
      /* Put out the virtual base class data sections. */
      for (bcp = tp->variant.class_struct_union.extra_info->base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->direct && bcp->is_virtual &&
            bcp->data_section_base_class == NULL) {
          db_virtual_base_class(bcp, depth+1);
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
  }  /* if */
  fputs(" )]", f_debug);
}  /* db_virtual_base_class */


static void db_access_adjustment(an_access_adjustment_ptr aap)
/*
*/
{
  a_source_correspondence  *sc;
  char                     *str;

  switch (aap->kind) {
    case aak_variable:
      sc = &aap->variant.variable->source_corresp;
      str = "static data member";
      break;
    case aak_field:
      sc = &aap->variant.field->source_corresp;
      str = "field";
      break;
    case aak_routine:
      sc = &aap->variant.routine->source_corresp;
      str = "member function";
      break;
    case aak_type:
      sc = &aap->variant.type->source_corresp;
      str = "member type";
      break;
    case aak_constant:
      sc = &aap->variant.constant->source_corresp;
      str = "member constant";
      break;
    default:
      sc = NULL;
  }  /* switch */
  fputs("\n    ", f_debug);
  if (sc == NULL) {
    fputs("<bad access adjustment kind>", f_debug);
  } else {
    db_access_control(aap->access);
    fprintf(f_debug, " \"%s\" = %s ", sc->name, str);
    db_name(sc);
  }  /* if */
}  /* db_access_adjustment */


void db_type(a_type *tp)
/*
Dump the contents of the indicated type entry, for debug purposes.
*/
{
  a_field_ptr                   fp;
  a_param_type_ptr              ptp;
  a_class_type_supplement_ptr	ctsp;
  a_boolean	                comma_required;

  switch (tp->kind) {
    case tk_error:
      fputs("<error type>", f_debug);
      break;
    case tk_unknown:
      fputs("<unknown type>", f_debug);
      break;
    case tk_void:
      fputs("void", f_debug);
      break;
    case tk_integer:
      fprintf(f_debug, "%s", db_int_type_name(tp->variant.integer.int_kind));
      if (tp->variant.integer.enum_type) fputs(" enum", f_debug);
      break;
    case tk_float:
      fprintf(f_debug, "%s", db_float_type_name(tp->variant.float_kind));
      break;
    case tk_pointer:
      if (tp->variant.pointer.is_reference) {
        fputs("ref to ", f_debug);
      } else {
        fputs("ptr to ", f_debug);
      }  /* if */
      db_abbreviated_type(tp->variant.pointer.type);
      break;
    case tk_array:
      fprintf(f_debug, "array [%lu] of ",
                       tp->variant.array.number_of_elements);
      db_abbreviated_type(tp->variant.array.element_type);
      break;
    case tk_struct:
      fputs("struct", f_debug);
      goto class_struct_union;
    case tk_union:
      fputs("union", f_debug);
      goto class_struct_union;
    case tk_class:
      fputs("class", f_debug);
class_struct_union:
      fputs(" \"", f_debug);
      db_name(&tp->source_corresp);
      fputc('"', f_debug);
      ctsp = tp->variant.class_struct_union.extra_info;
      if (tp->variant.class_struct_union.field_list == NULL &&
          (ctsp == NULL || ctsp->assoc_scope == NULL)) {
        fputs(" (undefined)", f_debug);
      } else {
        a_base_class_ptr  bcp = NULL;
        a_boolean         any_virtual_base_classes = FALSE;
        a_boolean         any_indirect_base_classes = FALSE;

        if (tp->variant.class_struct_union.abstract) {
          fputs(" (abstract)", f_debug);
        }  /* if */
        fputs(" {", f_debug);
        if (ctsp != NULL) bcp = ctsp->base_classes;
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct) {
            if (!bcp->is_virtual) db_direct_base_class(bcp, 0);
          } else {
            any_indirect_base_classes = TRUE;
          }  /* if */
          if (bcp->is_virtual) any_virtual_base_classes = TRUE;
        } /* for */
        fp = tp->variant.class_struct_union.field_list;
        for (; fp != NULL; fp = fp->next) db_field(fp, 0);
        if (any_virtual_base_classes) {
          if (ctsp != NULL) bcp = ctsp->base_classes;
          for (; bcp != NULL; bcp = bcp->next) {
            if (bcp->is_virtual) {
#if !CFRONT_CLASS_LAYOUT_COMPATIBILITY
              if (!bcp->direct) continue;
#endif /* !CFRONT_CLASS_LAYOUT_COMPATIBILITY */
              db_virtual_base_class_ptr(bcp, 0);
            }  /* if */
          } /* for */
        }  /* if */
        if (ctsp != NULL && ctsp->assoc_scope != NULL) {
          a_variable_ptr           vp = ctsp->assoc_scope->variables;
          a_routine_ptr            rp = ctsp->assoc_scope->routines;
          an_access_adjustment_ptr aap = ctsp->access_adjustments;

          db_virtual_function_info(ctsp, /*nesting_depth=*/-1);
          if (any_virtual_base_classes) {
            fputs("\n  collected virtual base classes:", f_debug);
            for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
              if (bcp->is_virtual) db_virtual_base_class(bcp, 0);
            }  /* for */
          }  /* if */
          if (any_indirect_base_classes) {
            fputs("\n  indirect base classes:", f_debug);
            for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
              if (!bcp->direct) db_indirect_base_class(bcp);
            }  /* for */
          }  /* if */
          if (vp != NULL) {
            fputs("\n  static data members:", f_debug);
            for (; vp != NULL; vp = vp->next) db_static_data_member(vp);
          }  /* if */
          if (rp != NULL) {
            fprintf(f_debug, "\n  member functions (%d virtual):",
                             ctsp->virtual_function_count);
            for (; rp != NULL; rp = rp->next) db_member_function(rp);
          }  /* if */
          if (aap != NULL) {
            fputs("\n  access adjustments:", f_debug);
            for (; aap != NULL; aap = aap->next) db_access_adjustment(aap);
          }  /* if */
        }  /* if */
        fputc('\n', f_debug);
        if (ctsp != NULL) {
          db_all_virtual_function_override_lists(tp);
        }  /* if */
        fprintf(f_debug, "} : size = %lu, alignment = %d",
                tp->size, tp->alignment);
        if (any_virtual_base_classes) {
          fprintf(f_debug, "; w/o virtuals: size = %lu, alignment = %d",
                              ctsp->size_without_virtual_base_classes,
                              ctsp->alignment_without_virtual_base_classes);
        }  /* if */
      }
      break;
    case tk_routine:
      fputs("function ", f_debug);
      if (tp->variant.routine.extra_info->assoc_routine != NULL) {
	db_name(&tp->variant.routine.extra_info->
			    	assoc_routine->source_corresp);
      }  /* if */
      if (!tp->variant.routine.extra_info->prototyped) {
        fputs(" old-style", f_debug);
      }  /* if */
      fputs("(", f_debug);
      ptp = tp->variant.routine.extra_info->param_type_list;
      if (tp->variant.routine.extra_info->implicit_this_param_type != NULL) {
	fputs("this: ", f_debug);
        db_abbreviated_type(tp->variant.routine.extra_info->
						implicit_this_param_type);
        if (ptp != NULL || tp->variant.routine.extra_info->has_ellipsis) {
          fputs("; ", f_debug);
        }  /* if */
      }  /* if */
      comma_required = FALSE;
      while (ptp != NULL) {
	if (comma_required) fputs(", ", f_debug);
        db_abbreviated_type(ptp->type);
        if (ptp->default_arg_expr != NULL) {
          an_expr_node_ptr expr = ptp->default_arg_expr;
          fputs(" (= ", f_debug);
          switch (expr->kind) {
            case enk_constant:
              db_constant(expr->variant.constant);
              break;
            case enk_variable:
              db_name(&expr->variant.variable->source_corresp);
              break;
            case enk_error:
              fputs("<error>", f_debug);
              break;
            default:
              fputs("<expr>", f_debug);
              break;
          }  /* switch */
          fputs(")", f_debug);
        }  /* if */
	comma_required = TRUE;
        ptp = ptp->next;
      }  /* while */
      if (tp->variant.routine.extra_info->has_ellipsis) {
	if (comma_required) fputs(", ", f_debug);
        fputs("...", f_debug);
      }  /* if */
      fputs(") returning ", f_debug);
      db_abbreviated_type(tp->variant.routine.return_type);
      break;
    case tk_typeref:
      if (tp->variant.typeref.is_function_scope_tag) {
        fputs("local tag for ", f_debug);
      } else if (!tp->variant.typeref.is_const &&
                 !tp->variant.typeref.is_volatile) {
        fputs("typeref ", f_debug);
      } else {
        if (tp->variant.typeref.is_const) fputs("const ", f_debug);
        if (tp->variant.typeref.is_volatile) fputs("volatile ", f_debug);
      }  /* if */
      db_abbreviated_type(tp->variant.typeref.type);
      break;
    case tk_ptr_to_member:
      fputs("ptr-to-member of ", f_debug);
      db_abbreviated_type(tp->variant.ptr_to_member.class_of_which_a_member);
      fputs(" of type ", f_debug);
      db_abbreviated_type(tp->variant.ptr_to_member.type);
      break;
    default:
      fputs("<bad type>", f_debug);
  }  /* switch */
}  /* db_type */


void db_constant(a_constant *cp)
/*
Dump the contents of the indicated constant, for debug purposes.
*/
{
  long           i;
  char           c;
  a_constant_ptr cp2;
  a_variable_ptr vp;
  a_routine_ptr  rp;
  a_type_ptr     con_type;
  a_float_kind   fkind;

  con_type = cp->type;
  if (con_type != NULL) {
    /* Dump the type preceding the constant, looking like a type cast. */
    (void)fputc('(', f_debug);
    db_type(con_type);
    (void)fputc(')', f_debug);
  }  /* if */

  con_type = skip_typerefs(con_type);
  switch (cp->kind) {
    case ck_error:
      fputs("<error constant>", f_debug);
      break;
    case ck_integer:
      fprintf(f_debug, "%ld", cp->variant.integer_value);
      break;
    case ck_string:
      fputs("\"", f_debug);
      for (i = 0; i < cp->variant.string.length; i++) {
        c = cp->variant.string.value[i];
        if (isprint(c)) {
          (void)fputc(c, f_debug);
        } else {
          /* Print non-printable character in octal form.  Truncate
             to right number of bits to avoid problems with signed chars. */
          fprintf(f_debug, "\\%03o", (unsigned int)(c&((1<<TARG_CHAR_BIT)-1)));
        }  /* if */
      }  /* for */
      fputs("\"", f_debug);
      break;
    case ck_float:
      fkind = skip_typerefs(cp->type)->variant.float_kind;
      fputs(fp_to_string(fkind, &cp->variant.float_value), f_debug);
      break;
    case ck_address:
      fputs("(&", f_debug);
      switch (cp->variant.address.kind) {
        case abk_routine:
          rp = cp->variant.address.variant.routine;
          db_name(&rp->source_corresp);
          break;
        case abk_variable:
          vp = cp->variant.address.variant.variable;
          db_name(&vp->source_corresp);
          break;
        case abk_constant:
          db_constant(cp->variant.address.variant.constant);
          break;
#if CHECKING
        default:
          internal_error("db_constant: bad address constant kind");
#endif /* CHECKING */
      }  /* switch */
      fprintf(f_debug, " + %ld)", cp->variant.address.offset);
      break;
    case ck_ptr_to_member:
      /* C++ pointer-to-member. */
      fprintf(f_debug, "(&-member ");
      if (cp->variant.ptr_to_member.is_function_ptr) {
        db_name(&cp->variant.ptr_to_member.variant.routine->source_corresp);
      } else {
        db_name(&cp->variant.ptr_to_member.variant.field->source_corresp);
      }  /* if */
      fprintf(f_debug, ")");
      break;
    case ck_aggregate:
      (void)fputc('{', f_debug);
      cp2 = cp->variant.aggregate.first_constant;
      while (cp2 != NULL) {
        db_constant(cp2);
        cp2 = cp2->next;
        if (cp2 != NULL) (void)fputc(',', f_debug);
      }  /* while */
      (void)fputc('}', f_debug);
      break;
    default:
      fputs("<bad constant>", f_debug);
  }  /* switch */
}  /* db_constant */


void db_variable(a_variable_ptr var_ptr)
/*
Dump the contents of the indicated variable for debug purposes.
*/
{
  fputs("name = ", f_debug);
  db_name(&var_ptr->source_corresp);
  fputs(", type = ", f_debug);
  db_abbreviated_type(var_ptr->type);
}  /* db_variable */


static void db_expr_node(an_expr_node_ptr node,
		         int              level)
/*
Dump the contents of the indicated expression node for debug purposes.
*/
{
  register an_expr_node_ptr operand;
  a_constant_ptr            const_ptr;
  int                       a;

  for (a = 0; a < level; a++) fputs(" ", f_debug);
  switch ((int)node->kind) {
    case enk_operation:
      fprintf(f_debug, "operator: %s",
              db_operator_names[(int)node->variant.operation.kind]);
      fputs(", result type: ", f_debug);
      db_abbreviated_type(node->type);
      fputs("\n", f_debug);
      operand = node->variant.operation.operands;
      while (operand != NULL) {
        db_expr_node(operand, level + 2);
        operand = operand->next;
      }  /* while */
      break;
    case enk_constant:
      const_ptr = node->variant.constant;
      if (const_ptr->source_corresp.name == NULL) {
        fputs("constant: value = ", f_debug);
      } else {
	fprintf(f_debug, "constant (%s): value = ",
		const_ptr->source_corresp.name);
      }  /* if */
      db_constant(const_ptr);
      fputs("\n", f_debug);
      break;
    case enk_variable_address:
      fputs("address of variable: ", f_debug);
      db_variable(node->variant.variable);
      fputs("\n", f_debug);
      break;
    case enk_variable:
      /* For now. */
      fputs("variable: ", f_debug);
      db_variable(node->variant.variable);
      fputs("\n", f_debug);
      break;
    case enk_routine_address:
      fprintf(f_debug, "address of routine: %s\n",
	      node->variant.routine->source_corresp.name);
      break;
    case enk_field:
      fputs("field node\n", f_debug);
      break;
    case enk_temp_init:
      fputs("temp init: ", f_debug);
      goto initializer;
    case enk_new_init:
      fputs("new init: ", f_debug);
initializer:
      db_dynamic_initializer(node->variant.init.dynamic_init, level);
      break;
    case enk_error:
      fputs("error node\n", f_debug);
      break;
#if CHECKING
    default:
      fputs("UNKNOWN EXPR KIND\n", f_debug);
#endif /* CHECKING */
  }  /* switch */
}  /* db_expr_node */


void db_expression(an_expr_node_ptr node)
{
  fputs("*** start of expression ***\n", f_debug);
  db_expr_node(node, 0);
  fputs("*** end of expression ***\n", f_debug);
}  /* db_expression */


static void db_static_initializer(a_constant_ptr  con)
{
  if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    fputs("{ ", f_debug);
    con = con->variant.aggregate.first_constant;
    for (; con != NULL; con = con->next) {
      db_static_initializer(con);
      if (con->next != NULL) fputs(", ", f_debug);
    }  /* for */
    fputs(" }", f_debug);
  } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    fprintf(f_debug, "%lu repetitions of: ",
		     (unsigned long)con->variant.init_repeat.count);
    db_static_initializer(con->variant.init_repeat.constant);
  } else {
    db_constant(con);
  }  /* if */
}  /* db_static_initializer */


static void db_destructor(a_routine_ptr  dtor)
{
  fputs("destructor: ", f_debug);
  db_name(&dtor->source_corresp);
  fputs("()", f_debug);
}  /* db_destructor */


static void db_constructor_initializer(a_dynamic_init_ptr  dip,
                                       int                 level)
{
  an_expr_node_ptr  arg;
  a_param_type_ptr  ptp;

  fputs("constructor ", f_debug);
  db_name(&dip->variant.constructor.routine->source_corresp);
  (void)fputc('(', f_debug);
  ptp = dip->variant.constructor.routine->type->
                          variant.routine.extra_info->param_type_list;
  if (ptp != NULL) {
    db_abbreviated_type(ptp->type);
    for (ptp = ptp->next; ptp != NULL; ptp = ptp->next) {
      fputs(", ", f_debug);
      db_abbreviated_type(ptp->type);
    }  /* for */
  }  /* if */
  (void)fputc(')', f_debug);
  if ((arg = dip->variant.constructor.args) == NULL) {
    if (dip->destructor != NULL) {
      fputs("; ", f_debug);
      db_destructor(dip->destructor);
    }  /* if */
    (void)fputc('\n', f_debug);
  } else {
    fputs(", args =\n", f_debug);
    for (; arg != NULL; arg = arg->next) {
      db_expr_node(arg, level + 2);
    }  /* if */
    if (dip->destructor != NULL) {
      int a;
      for (a = 0; a < level; a++) fputs(" ", f_debug);
      db_destructor(dip->destructor);
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
}  /* db_constructor_initializer */


static void db_nonconstant_aggregate(a_constant_ptr  con,
                                     int             level)
{
  int  a;

  for (; con != NULL; con = con->next) {
    if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
      a_dynamic_init_ptr  dip = con->variant.dynamic_init;
      switch (dip->kind) {
        case dik_expression:
          db_expr_node(dip->variant.expression, level);
          if (dip->destructor != NULL) {
            for (a = 0; a < level; a++) fputs(" ", f_debug);
            db_destructor(dip->destructor);
            (void)fputc('\n', f_debug);
          }  /* if */
          break;
        case dik_constructor:
          for (a = 0; a < level; a++) fputs(" ", f_debug);
          db_constructor_initializer(dip, level);
          break;
#if CHECKING
        default:
          fputs("**UNEXPECTED DYNAMIC INIT KIND**\n", f_debug);
#endif /* CHECKING */
      }  /* switch */
    } else {
      for (a = 0; a < level; a++) fputs(" ", f_debug);
      if (con->kind == (a_constant_repr_kind)ck_aggregate) {
        fputs("aggregate:\n", f_debug);
        db_nonconstant_aggregate(con->variant.aggregate.first_constant,
                                 level + 2);
      } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
        fprintf(f_debug, "%lu repetitions of:\n",
                         (unsigned long)con->variant.init_repeat.count);
        db_nonconstant_aggregate(con->variant.init_repeat.constant,
                                 level + 2);
      } else {
        db_constant(con);
        (void)fputc('\n', f_debug);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* db_nonconstant_aggregate */


void db_dynamic_initializer(a_dynamic_init_ptr  dip,
                            int                 level)
{
  int a;

  switch (dip->kind) {
    case dik_constant:
      db_static_initializer(dip->variant.constant);
      if (dip->destructor != NULL) {
        fputs("; ", f_debug);
        db_destructor(dip->destructor);
      }  /* if */
      (void)fputc('\n', f_debug);
      break;
    case dik_expression:
      fputs("expression:\n", f_debug);
      db_expr_node(dip->variant.expression, level);
      goto destructor_on_next_line;
    case dik_nonconstant_aggregate:
      fputs("aggregate with non-constants:\n", f_debug);
      db_nonconstant_aggregate(dip->variant.constant->
                                        variant.aggregate.first_constant,
                               level);
destructor_on_next_line:
      if (dip->destructor != NULL) {
        for (a = 0; a < level; a++) fputs(" ", f_debug);
        db_destructor(dip->destructor);
        (void)fputc('\n', f_debug);
      }  /* if */
      break;
    case dik_constructor:
      db_constructor_initializer(dip, level);
      break;
    case dik_member_copy:
      fputs("<bitwise member copy>", f_debug);
      goto destructor_on_this_line;
    case dik_base_class_copy:
      fputs("<bitwise base class copy>", f_debug);
      goto destructor_on_this_line;
    case dik_none:
      fputs("<none>", f_debug);
destructor_on_this_line:
      if (dip->destructor != NULL) {
        fputs(", ", f_debug);
        db_destructor(dip->destructor);
      }  /* if */
      (void)fputc('\n', f_debug);
      break;
  }  /* switch */
}  /* db_dynamic_initializer */


void db_initializer(a_variable_ptr  var,
                    int             level)
{
  int  a;

  if (var->init_kind != (an_init_kind)initk_none) {
    for (a = 0; a < level; a++) fputs(" ", f_debug);
    if (var->init_kind == (an_init_kind)initk_static) {
      fputs("static init: ", f_debug);
      db_static_initializer(var->initializer.constant);
      (void)fputc('\n', f_debug);
    } else {
      fputs("dynamic init: ", f_debug);
      db_dynamic_initializer(var->initializer.dynamic, level + 2);
    }  /* if */
  }  /* if */
}  /* db_initializer */
#endif /* DEBUG */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Macro to increment the entry number allocation count only if DEBUG
is TRUE.  Used in do_alloc.
*/
#if DEBUG
#define incr_num_il_entry_numbers_allocated()                         \
  num_il_entry_numbers_allocated++
#else /* !DEBUG */
#define incr_num_il_entry_numbers_allocated() /* Do nothing */
#endif /* DEBUG */

#if ALTERNATE_IL_FILE_FORMAT
/*
For the alternate IL file format, each entry's allocation must be preceded
by an entry number, which is initialized to zero here.
*/
#define do_alloc(ptr, region_number, size)                            \
{ ptr = alloc_in_region((region_number),                              \
                         (sizeof_t)((size)+sizeof(an_il_entry_number))); \
  *(an_il_entry_number *)ptr = 0;                                     \
  incr_num_il_entry_numbers_allocated();                              \
  ptr += sizeof(an_il_entry_number);                                  \
}  /* do_alloc */
#else /* !ALTERNATE_IL_FILE_FORMAT */
/*
For the usual IL file format, or when no IL file is written, no extra space
is required.
*/
#define do_alloc(ptr, region_number, size)                            \
  ptr = alloc_in_region((region_number), (size))
#endif /* ALTERNATE_IL_FILE_FORMAT */

#if ORPHAN_PROCESSING_NEEDED
/*
Orphaned file scope IL entries need to be chained together to ensure that
they will be visited when the IL is walked during writing, reading
or display.  That pointer used to chain like IL entry types together
precedes the IL entry (and follows the IL entry number if there is one).
The pointer is only needed in file-scope allocations.
*/
#define do_fs_alloc(ptr, size)                                        \
{ do_alloc(ptr, FILE_SCOPE_REGION_NUMBER, size+sizeof(char *));       \
  *(char **)ptr = (char *)NULL;                                       \
  ptr += sizeof(char *);                                              \
}  /* do_alloc */
#define do_any_alloc(ptr, region_number, size)                        \
{ if (region_number == FILE_SCOPE_REGION_NUMBER) {                    \
    do_fs_alloc(ptr, size);                                           \
  } else {                                                            \
    do_alloc(ptr, region_number, size);                               \
  }  /* if */                                                         \
}  /* do_any_alloc */
#else /* !ORPHAN_PROCESSING_NEEDED */
/* No space needed for next-orphan pointer.  Allocation in the file scope
   is the same as allocation in any other region. */
#define do_fs_alloc(ptr, size)                                        \
  do_alloc(ptr, FILE_SCOPE_REGION_NUMBER, size)
#define do_any_alloc(ptr, region_number, size)                        \
  do_alloc(ptr, region_number, size)
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

char *alloc_cil(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the current IL memory region.
*/
{
  char *ptr;
  do_any_alloc(ptr, curr_il_region_number, size);
  return ptr;
}  /* alloc_cil */


void switch_il_region(a_memory_region_number region_number)
/*
Change the current IL memory region to "region_number".
*/
{
  curr_il_region_number = region_number;
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Switching to IL region %d.\n", curr_il_region_number);
  }  /* if */
#endif /* DEBUG */
}  /* switch_il_region */


void switch_to_file_scope_region(
                              a_memory_region_number *region_to_switch_back_to)
/*
Switch to the file-scope memory region if not already there.  Set
region_to_switch_back_to for use later by switch_back_to_original_region.
*/
{
  if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
    *region_to_switch_back_to = curr_il_region_number;
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
  } else {
    *region_to_switch_back_to = NULL_region_number;
  }  /* if */
}  /* switch_to_file_scope_region */


void switch_to_function_scope_region(
                              a_memory_region_number *region_to_switch_back_to)
/*
Switch to the function-scope memory region if not already there.  Set
region_to_switch_back_to for use later by switch_back_to_original_region.
*/
{
  a_memory_region_number  region;

  region = scope_stack[depth_scope_stack].il_memory_region;
#if CHECKING
  if (region == FILE_SCOPE_REGION_NUMBER) {
    internal_error("switch_to_function_scope_region: no func scope region");
  }  /* if */
#endif /* CHECKING */
  if (curr_il_region_number != region) {
    *region_to_switch_back_to = curr_il_region_number;
    switch_il_region(region);
  } else {
    *region_to_switch_back_to = NULL_region_number;
  }  /* if */
}  /* switch_to_function_scope_region */


void switch_back_to_original_region(
                               a_memory_region_number region_to_switch_back_to)
/*
Switch back to the memory region that was current when
switch_to_file_scope_region was called.
*/
{
  if (region_to_switch_back_to != NULL_region_number) {
    switch_il_region(region_to_switch_back_to);
  }  /* if */
}  /* switch_back_to_original_region */


a_scope_ptr new_il_region(a_scope_kind   kind,
                          a_scope_number scope_number,
                          a_routine_ptr  assoc_routine)
/*
Start a new IL memory region for the IL for the file scope or a function.
kind indicates the kind of scope (file or function); scope_number gives
the scope number; and if the scope is for a function, assoc_routine points to
its routine entry.  Establish this new region as the current IL region.
Allocate a scope entry in the new region and return a pointer to it.
*/
{
  a_scope_ptr sp;

  if (kind == (a_scope_kind)sck_file) {
    /* The file scope memory region is created in initialization. */
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
  } else {
#if CHECKING
    if (kind != (a_scope_kind)sck_function) {
      internal_error("new_il_region: bad scope kind");
    }  /* if */
#endif /* CHECKING */
    /* Create a new region for a function scope. */
    switch_il_region(new_memory_region());
  }  /* if */
  /* Allocate the IL scope entry. */
  sp = alloc_scope(kind, scope_number, assoc_routine);
  /* Remember the location of the primary scope entry. */
  il_header.region_scope_entry[curr_il_region_number] = sp;
  return sp;
}  /* new_il_region */
  

void record_start_of_source_file(a_source_file_ptr parent_file,
			         a_seq_number      seq_number,
				 a_line_number     line_number,
			         char	           *file_name,
			         char	           *full_name,
			         a_source_file_ptr *new_file)
/*
Create a source file entry in the intermediate language, to record the
start of a new source file (either the primary source file or an include file).
Line line_number of the new file is sequence number seq_number of the
compilation, and the new file's short and long form names are file_name
and full_name.  The parent file for this file is parent_file; if there
is no parent file (i.e., for the primary source file), parent_file == NULL.
For entries generated by #line directives, full_name == NULL.
*/
{
  register a_source_file_ptr sfp;

  db_enter(5, "record_start_of_source_file");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "file = \"%s\", seq = %lu\n", file_name, seq_number);
  }  /* if */
#endif /* DEBUG */
  /* Allocate the new file block. */
  *new_file = sfp = (a_source_file_ptr)alloc_il(sizeof(a_source_file));
  sfp->file_name        = file_name;
  sfp->full_name        = full_name;
  sfp->first_seq_number = seq_number;
  sfp->last_seq_number  = MAX_SEQ_NUMBER;  /* Not yet entered. */
  sfp->first_line_number= line_number;
  sfp->first_child_file = NULL;
  sfp->last_child_file  = NULL;
  sfp->next             = NULL;
  /* Link the parent or the preceding sibling file to this one. */
  if (parent_file == NULL) {
    /* No parent, so link the il_header to this primary source file. */
    il_header.primary_source_file = sfp;
  } else {
    if (parent_file->first_child_file == NULL) {
      /* First child for this parent. */
      parent_file->first_child_file = sfp;
    } else {
      /* Not first child, link previous child to this one. */
      parent_file->last_child_file->next = sfp;
    }  /* if */
    parent_file->last_child_file = sfp;
  }  /* if */
  db_exit();
} /* record_start_of_source_file */


void record_end_of_source_file(a_source_file_ptr curr_file,
			       a_seq_number      seq_number)
/*
Finish the source file entry for the source file indicated by curr_file,
by recording that the last sequence number contained therein is seq_number.
*/
{
  db_enter(5, "record_end_of_source_file");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "seq = %lu\n", seq_number);
  }  /* if */
#endif /* DEBUG */
  curr_file->last_seq_number = seq_number;
  db_exit();
}  /* record_end_of_source_file */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static a_source_file_ptr source_file_for_seq(a_seq_number   seq_number,
                                             a_line_number  *line_number,
                                             a_boolean      *at_end_of_source,
                                             unsigned long  *nesting_depth,
                                             a_source_file_ptr
                                                            *parent_file)
/*
Find the source file entry within which the sequence number seq_number falls,
and return a pointer to it.  Return NULL if the sequence number falls
outside of any file.  If the sequence number falls within a file, also
return *line_number set to the line number in the file.  Return
*at_end_of_source TRUE if the line number is the special number indicating
the end-of-file line (one more than the last line in the primary input file).
Return the file nesting depth in *nesting_depth (0 => not inside any file,
1 => in primary source file, 2 => inside one level of #include, etc.).  If
the sequence number falls within a file, also return a pointer to that 
source file entry's parent source file entry.
*/
{
  register a_source_file_ptr curr_file, child_file;
  unsigned long              lines_in_children;

  *at_end_of_source = FALSE;
  *line_number = 0;
  *nesting_depth = 0;
  *parent_file = NULL;
  curr_file = il_header.primary_source_file;
  if (curr_file == NULL) {
    /* No files, so any sequence number is not within any file. */
  } else if (seq_number == 0) {
    /* Unknown position. */
    curr_file = NULL;
#if CHECKING
  } else if (seq_number   < curr_file->first_seq_number ||
             seq_number-1 > curr_file->last_seq_number) {
#if DEBUG
    if (debug_level > 0) fprintf(f_debug, "seq number = %lu\n", seq_number);
#endif /* DEBUG */
    internal_error("source_file_for_seq: bad seq number");
#endif /* CHECKING */
  } else {
    if (seq_number-1 == curr_file->last_seq_number) {
      /* At end of source.  Use the last line of the primary source file. */
      *at_end_of_source = TRUE;
      seq_number--;
    }  /* if */
    /* See if the sequence number falls within any child file. */
examine_children:
    (*nesting_depth)++;
    lines_in_children = 0;
    child_file = curr_file->first_child_file;
    /* Check the sequence number against each child.  The children are
       in order by sequence number. */
    while (child_file != NULL) {
      if (seq_number < child_file->first_seq_number) {
        /* Sequence number falls before the start of this child, and
           therefore must be in the current file. */
        break;
      } else if (seq_number <= child_file->last_seq_number) {
        /* The sequence number falls within this child (or one of its
           children). */
        *parent_file = curr_file;
        curr_file = child_file;
        goto examine_children;
      }  /* if */
      /* The sequence number falls after this child, so keep looking. */
      lines_in_children += child_file->last_seq_number -
                           child_file->first_seq_number + 1;
      child_file = child_file->next;
    }  /* while */
    *line_number = seq_number - curr_file->first_seq_number + 
                   curr_file->first_line_number - lines_in_children;
  }  /* if */
  return curr_file;
}  /* source_file_for_seq */


void conv_seq_to_file_and_line(a_seq_number  seq_number,
	     		       char	     **file_name,
			       char          **full_name,
			       a_line_number *line_number,
                               a_boolean     *at_end_of_source)
/*
For the sequence number given by seq_number, find the corresponding
file and line number.  Return the short and long forms of the file name
in *file_name and *full_name, and the line number in *line_number.
If the sequence number indicates the end-of-source line, the file names
will be set to the primary source file, the line number to the last
line in that file, and *at_end_end_of_source will be set TRUE (it is
set to FALSE in all other cases).  If the sequence number indicates an
unknown position, the file names will be set to zero-length strings, and 
the line number to 0.
*/
{
  a_source_file_ptr proper_file, parent_file;
  unsigned long     nesting_depth;

  db_enter(5, "conv_seq_to_file_and_line");

  /* Find out which file the sequence number is in. */
  proper_file = source_file_for_seq(seq_number, line_number, at_end_of_source,
                                    &nesting_depth, &parent_file);
  if (proper_file == NULL) {
    /* Strange or unknown position. */
    *file_name = *full_name = "";
    *line_number = 0;
  } else {
    *file_name = proper_file->file_name;
    *full_name = proper_file->full_name;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    if (*line_number == 0) {
      fprintf(f_debug, "seq %lu is outside of all source.\n", seq_number);
    } else {
      fprintf(f_debug, "seq %lu maps into line %lu of file \"%s\".\n",
              seq_number, *line_number, *file_name);
      if (*at_end_of_source) fprintf(f_debug, "(really: at end of source)\n");
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* conv_seq_to_file_and_line */

#if !STANDALONE_UTILITY_PROGRAM

void conv_seq_to_physical_file_and_line(a_seq_number      seq_number,
                                        a_source_file_ptr *src_file,
                                        a_line_number     *physical_line,
                                        a_boolean         *at_end_of_source)
/*
For the sequence number given by seq_number, find the corresponding
physical file and line number.  A pointer to the IL source file entry is
returned in *src_file , and the physical line number is returned in
*physical_line.  If the sequence number indicates the end-of-source line,
the source file pointer will be set to the primary source file,
the line number to the last line in that file, and *at_end_of_source will be
set TRUE (it is set to FALSE in all other cases).  If the sequence number
indicates an unknown position, the source file pointer  will be set to
NULL, and the line number to 0.
*/
{
  a_source_file_ptr proper_file, parent_file, child_file;
  unsigned long     nesting_depth;
  unsigned long     lines_in_children;
  a_line_number     line_number;

  db_enter(5, "conv_seq_to_physical_file_and_line");

  /* Find out which file the sequence number is in. */
  proper_file = source_file_for_seq(seq_number, &line_number, at_end_of_source,
                                    &nesting_depth, &parent_file);
  if (proper_file == NULL) {
    /* Strange or unknown position. */
    *src_file = NULL;
    *physical_line = 0;
  } else {
    if (proper_file->full_name != NULL) {
      /* The desired sequence number is in an actual file */
      *src_file = proper_file;
      *physical_line = line_number - proper_file->first_line_number + 1;
    }  else {
      /* The desired sequence number is in a sequence of source governed
         by a #line directive.  The actual file is the parent file, but
         the physical line within that file is dependent upon the number
         of source lines in any include files that fall between the  first
         sequence number of the parent and the target sequence number. */
      *src_file = parent_file;
      lines_in_children = 0;
      child_file = parent_file->first_child_file;
      /* Check the sequence number against each child (in order by sequence
         number); it must be within one of the children. */
      while (child_file != NULL) {
        if (seq_number <= child_file->last_seq_number) {
          /* The sequence number falls within this child. */
          break;
        }  /* if */
        if (child_file->full_name != NULL) {
          /* This is a "real" include file, count the number of sequence
             lines in this file. */
          lines_in_children += child_file->last_seq_number -
                               child_file->first_seq_number + 1;
        }  /* if */
        child_file = child_file->next;
      }  /* while */
      *physical_line = seq_number - parent_file->first_seq_number -
                       lines_in_children + 1;
    }  /* if */
  }  /* if */

  db_exit();
}  /* conv_seq_to_physical_file_and_line */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

a_boolean seq_is_in_include_file(a_seq_number seq_number)
/*
Return TRUE if the sequence number seq_number falls within an include file.
*/
{
  a_boolean         in_include_file;
  a_source_file_ptr proper_file, primary_file, first_file_under_primary,
                    parent_file;
  a_line_number     line_number;
  a_boolean         at_end_of_source;
  unsigned long     nesting_depth;

  primary_file = il_header.primary_source_file;
  proper_file = source_file_for_seq(seq_number, &line_number,
                                    &at_end_of_source, &nesting_depth,
                                    &parent_file);
  if (proper_file == NULL) {
    /* Sequence number is not in a file, so it's not in an include file. */
    in_include_file = FALSE;
  } else if (proper_file == primary_file) {
    /* Sequence number is in the primary source file. */
    in_include_file = FALSE;
  } else if (proper_file->full_name == NULL && nesting_depth == 2) {
    /* Sequence number is in a section headed by a #line directive,
       and it's not within an #include.  The position might be in
       an #include in the original source; we have to examine the
       file name from the #line to find out.  We determine the effective
       primary source file name by looking for a #line directive that
       is the first line in the input, and compare it with the file name
       associated with the present #line directive.  If they match, the
       sequence number is in the primary source file; otherwise, it's
       in an include file. */
    in_include_file = TRUE;
    if (primary_file != NULL) {
      first_file_under_primary = primary_file->first_child_file;
      if (first_file_under_primary != NULL) {
        /* See if the first file entry under the primary file entry is for
           a #line directive that is the first line of the input and
           specifies a line number of 1. */
        if (first_file_under_primary->full_name == NULL &&
            first_file_under_primary->first_seq_number == 1+1 &&
            first_file_under_primary->first_line_number == 1) {
          /* Compare the effective primary file name (from the #line directive
             that's first in the input) with the file name from the
             #line associated with the sequence number we're investigating. */
          if (strcmp(first_file_under_primary->file_name,
                     proper_file->file_name) == 0) {
            /* Match -- consider the sequence number to be in the primary
               source file. */
            in_include_file = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* Sequence number is in an include file. */
    in_include_file = TRUE;
  }  /* if */
  return in_include_file;
}  /* seq_is_in_include_file */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if ORPHAN_PROCESSING_NEEDED

void add_orphaned_file_scope_il_entry (char             *entry_ptr,
                                       an_il_entry_kind entry_kind)
/*
Link the specified file scope IL entry onto the orphaned_file_scope_il_entries
linked list for the designated IL entry kind.  Only IL entries in the
file scope memory region have the necessary additional pointer space 
allocated immediately preceding the entry.
*/
{
  char **last_entry_ptr;

#if CHECKING
  if (!in_file_scope(entry_ptr)) {
    internal_error(
 "add_orphaned_file_scope_il_entry: IL entry not in file scope memory region");
  }  /* if */
#endif /* CHECKING */
  /* Check if this IL entry is already on the orphaned entry list. */
  last_entry_ptr = &orphaned_file_scope_il_entries[(int)entry_kind].last_entry;
  if (*(char **)(entry_ptr - sizeof(char *)) == NULL &&
      entry_ptr != *last_entry_ptr) {
    /* This entry is not in the existing list; add it to the end of the
       list. */
    if (*last_entry_ptr == NULL) {
      /* This is the first entry on this list */
      orphaned_file_scope_il_entries[(int)entry_kind].first_entry = 
                                                               entry_ptr;
    } else {
      /* Add to the tail of the existing list. */
      *(char **)(*last_entry_ptr - sizeof (char *)) = entry_ptr;
    }  /* if */
    *last_entry_ptr = entry_ptr;
  }  /* if */
}  /* add_orphaned_file_scope_il_entry */

#endif /* ORPHAN_PROCESSING_NEEDED */
#if ORPHAN_PROCESSING_NEEDED

void add_orphaned_file_scope_il_list(a_type_ptr     types,
                                     a_variable_ptr variables)
/*
Create an_orphaned_il_list IL entry to track of local types and local static
variable IL lists in a function scope IL entry.  Each list, connected by
their respective "next" pointers, will have been allocated in the
file scope memory region.  This IL entry, part of a chain pointed to by 
"il_header", will be used later to remap the "next" pointer in the type
list and static variable list, as needed.  Each IL entry will be individually
added to the orphaned_file_scope_il_entries array.
*/
{
  an_orphaned_il_list_ptr
		oil_ptr;
  a_type_ptr	local_type;
  a_variable_ptr
		local_static_variable;

  if (types != NULL || variables != NULL) {
    /* At least one of the IL pointers is not NULL; create an_orphaned_il_list
       entry in the file scope region. */
    oil_ptr = (an_orphaned_il_list_ptr)alloc_il(sizeof(an_orphaned_il_list));
    oil_ptr->orphaned_types = types;
    oil_ptr->orphaned_variables = variables;
    oil_ptr->next = il_header.orphaned_il_list;
    il_header.orphaned_il_list = oil_ptr;

    /* Add each type IL entry on the "types" list to the
       orphaned_file_scope_il_entries array. */
    for (local_type = types;
         local_type != NULL;
         local_type = local_type->next) {
      add_orphaned_file_scope_il_entry((char *)local_type, iek_type);
    }  /* for */

    /* Add each variable IL entry on the "variables" list to the
       orphaned_file_scope_il_entries array. */
    for (local_static_variable = variables;
         local_static_variable != NULL;
         local_static_variable = local_static_variable->next) {
      add_orphaned_file_scope_il_entry((char *)local_static_variable,
                                       iek_variable);
    }  /* for */
  }  /* if */
}  /* add_orphaned_file_scope_il_list */

#endif /* ORPHAN_PROCESSING_NEEDED */
#if !STANDALONE_UTILITY_PROGRAM

static void add_to_scopes_list(a_scope_ptr             scope_ptr,
                               a_scope_stack_entry_ptr ssep)
/*
Add the given IL scope to the scopes list for the scope stack entry pointed
to by ssep.
*/
{
  /* The list of scope entries is maintained using the first_scope/last_scope
     pointers in the scope stack entry.  This is because there may not be
     an allocated IL scope at this point.  When the scope stack is popped,
     the list of scopes will be transferred to the IL scope entry if there
     is one; otherwise, it will be added to the scopes list for the parent 
     scope.   This is done so that unneeded empty scopes are not allocated
     even when they are sandwiched between non-empty scopes. */
  if (ssep->first_scope == NULL) {
    ssep->first_scope = scope_ptr;
  } else {
    ssep->last_scope->next = scope_ptr;
  }  /* if */
  ssep->last_scope = scope_ptr;
  scope_ptr->next = NULL;
}  /* add_to_scopes_list */


/*
Macro to set the il_walk_flag in an IL entry to the appropriate initial
value based on whether it is in the file scope memory region or a
function scope memory region.
*/
#define set_il_walk_flag_to_initial_value(entry_ptr, at_file_scope)   \
  ((entry_ptr)->source_corresp.il_walk_flag = (at_file_scope) ?       \
          curr_fs_initial_il_walk_flag_setting :                      \
          curr_initial_il_walk_flag_setting)

/*
Similar macro based on the setting of curr_il_region_number.
*/
#define set_il_walk_flag_based_on_curr_il_region_number(entry_ptr)    \
  (set_il_walk_flag_to_initial_value(entry_ptr,                       \
                 (curr_il_region_number == FILE_SCOPE_REGION_NUMBER)))


static void set_default_source_corresp(a_source_correspondence *sc)
/*
Set the given source correspondence struct to default values.
*/
{
  sc->assoc_info              = NULL;
  sc->name                    = NULL;
  sc->class_of_which_a_member = NULL;
  sc->decl_position.seq       = 0;
  sc->decl_position.column    = SP_COL_UNKNOWN;
  /* access is set to "public" because "no access restriction" is the default
     for everything except class members.  For the latter the field must be
     set manually. */
  sc->access                  = (an_access_specifier)as_public;
  /* referenced is set TRUE because so far this is an entity not associated
     with one in the source program.  All unassociated entities are assumed
     to be referenced (otherwise, they wouldn't be created).  This does away
     with the difficult job of setting the referenced flag in a lot of
     different places for unassociated entities. set_source_corresp resets
     the flag to FALSE for associated entities, for which the flag is then
     set to TRUE (for an actual reference) by mark_referenced. */
  sc->referenced              = TRUE;
  /* Set the IL walk flag to a default setting.  Usually, this is overridden
     almost immediately, but the value here is important when an IL constant
     entry is created somewhere other than an IL memory region (e.g., in
     an expression operand) and then copied into an IL entry.  We use the
     default setting; if the value matters, the caller must adjust it. */
  sc->il_walk_flag            = curr_initial_il_walk_flag_setting;
  sc->name_linkage            = (a_name_linkage_kind)nlk_none;
}  /* set_default_source_corresp */


void break_source_corresp(a_source_correspondence *sc)
/*
If the indicated source correspondence is attached to a source entity,
break the correspondence.
*/
{
  sc->assoc_info = NULL;
  sc->name       = NULL;
  /* Note in particular that il_walk_flag is not changed. */
}  /* break_source_corresp */


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
      cp->variant.integer_value = 0;
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
      cp->variant.ptr_to_member.class_of_which_a_member = NULL;
      cp->variant.ptr_to_member.is_function_ptr = FALSE;
      cp->variant.ptr_to_member.variant.field   = NULL;
      break;
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
  set_default_source_corresp(&(cp->source_corresp));
  cp->next           = NULL;
  cp->type           = NULL;
  cp->implicit_cast  = FALSE;
  cp->non_arithmetic = FALSE;
  set_constant_kind(cp, kind);
}  /* clear_constant */


void set_error_constant(a_constant *cp)
/*
Set the indicated constant to make it an error constant.  Such a constant
is used for cases where there is an error, and a valid constant cannot be
produced.
*/
{
  clear_constant(cp, (a_constant_repr_kind)ck_error);
  cp->type = error_type();
}  /* set_error_constant */


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
  set_il_walk_flag_based_on_curr_il_region_number(cp);

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


void copy_constant(a_constant *from,
                   a_constant *to)
/*
Copy a constant entry from "from" to "to".
*/
{
  a_boolean il_walk_flag = to->source_corresp.il_walk_flag;

  /* Do the copy.  Preserve the il_walk_flag setting. */
  *to = *from;
  to->source_corresp.il_walk_flag = il_walk_flag;
}  /* copy_constant */


a_constant_ptr alloc_unshared_constant(a_constant *cp)
/*
Allocate a constant in the current IL memory region, copy the value of *cp
into it, and return a pointer to the allocated constant.  This routine
is used when a constant cannot be shared, as when it is an initializer
value.  Several fields are cleared or adjusted.
*/
{
  a_constant_ptr ucp;

  ucp = alloc_constant(cp->kind);
  copy_constant(cp, ucp);
  /* Clear the "next" field; it might have come from a shared version
     of the constant. */
  ucp->next = NULL;
  /* Clear the source correspondence information.  This version of the
     constant isn't the one directly associated with the source entity,
     if any. */
  break_source_corresp(&ucp->source_corresp);
  return ucp;
}  /* alloc_unshared_constant */


static a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant)
/*
Make a copy of an unshared constant and return pointer to the copy.
This is meant to be called only during the process of copying an expression
tree (the top-level routines like copy_expr_tree set up some important
global variables).
*/
{
  a_constant_ptr new_constant, old_aggr_con, new_aggr_con;

  new_constant = alloc_unshared_constant(old_constant);
  new_constant->next = NULL;
  if (new_constant->kind == (a_constant_repr_kind)ck_aggregate) {
    /* For aggregate constants, copy the subtree also. */
    new_constant->variant.aggregate.first_constant = NULL;
    new_constant->variant.aggregate.last_constant = NULL;
    for (old_aggr_con = old_constant->variant.aggregate.first_constant;
         old_aggr_con != NULL;
         old_aggr_con = old_aggr_con->next) {
      new_aggr_con = copy_unshared_constant(old_aggr_con);
      /* Add the constant to the aggregate list. */
      if (new_constant->variant.aggregate.first_constant == NULL) {
        new_constant->variant.aggregate.first_constant = new_aggr_con;
      } else {
        new_constant->variant.aggregate.last_constant->next = new_aggr_con;
      }  /* if */
      new_constant->variant.aggregate.last_constant = new_aggr_con;
    }  /* for */
  } else if (new_constant->kind == (a_constant_repr_kind)ck_init_repeat) {
    /* For ck_init_repeat constants, copy the subtree also. */
    new_constant->variant.init_repeat.constant =
            copy_unshared_constant(old_constant->variant.init_repeat.constant);
  } else if (new_constant->kind == (a_constant_repr_kind)ck_dynamic_init) {
    /* For ck_dynamic_init constants, copy the subtree also. */
    new_constant->variant.dynamic_init =
                         copy_dynamic_init(old_constant->variant.dynamic_init);
  }  /* if */
  return new_constant;
}  /* copy_unshared_constant */


static a_constant_hash_value hash_constant(a_constant *cp)
/*
Return the hash value for the indicated constant, which gives the proper
bucket of the shareable_constants_table to use for the constant.
*/
{
  a_constant_hash_value hash_value;
  a_targ_size_t         length;
  char                  *cptr;
  sizeof_t              n;

  /* Compute a hash value from the constant.  The hash doesn't have to
     be perfect, but it should spread the expected constants fairly widely. */
  switch (cp->kind) {
    case ck_integer:
      hash_value = (a_constant_hash_value)cp->variant.integer_value;
      break;
    case ck_string:
      length = cp->variant.string.length;
      hash_value = (a_constant_hash_value)(
                            100 + length + (*(cp->variant.string.value) << 6) +
                                         *(cp->variant.string.value+length-1));
      break;
    case ck_float:
      /* It's hard to do something machine-independent for floats.  Add 
         together the bytes that make up the float.  Note that the whole float
         was zeroed in initialization, so any gaps have predictable values. */
      hash_value = 500;
      cptr = (char *)&cp->variant.float_value;
      for (n = sizeof(an_internal_float_value); n > 0; n--) {
        hash_value += (a_constant_hash_value)*cptr++;
      }  /* for */
      break;
    case ck_address:
      switch (cp->variant.address.kind) {
        case abk_routine:
          hash_value =
                    (a_constant_hash_value)cp->variant.address.variant.routine;
          break;
        case abk_variable:
          hash_value =
                   (a_constant_hash_value)cp->variant.address.variant.variable;
          break;
        case abk_constant:
          hash_value =
                   (a_constant_hash_value)cp->variant.address.variant.constant;
          break;
#if CHECKING
        default:
          internal_error("hash_constant: bad address constant kind");
#endif /* CHECKING */
      }  /* switch */
      hash_value += (a_constant_hash_value)(cp->variant.address.offset + 1000);
      break;
    case ck_ptr_to_member:
      if (cp->variant.ptr_to_member.is_function_ptr) {
        hash_value =
              (a_constant_hash_value)cp->variant.ptr_to_member.variant.routine;
      } else {
        hash_value =
                (a_constant_hash_value)cp->variant.ptr_to_member.variant.field;
      }  /* if */
      break;
    default:
      hash_value = (a_constant_hash_value)(200 + cp->kind);
      break;
  }  /* switch */
  /* Reduce the value modulo the table size. */
  hash_value %= SIZE_SHAREABLE_CONSTANTS_TABLE;
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "hash_constant, hash_value = %u\n",
                     (unsigned int)hash_value);
  }  /* if */
#endif /* DEBUG */
  return hash_value;
}  /* hash_constant */


static a_boolean eq_constants(a_constant *cp1,
                              a_constant *cp2)
/*
Return TRUE if the two constants are identical.
*/
{
  a_boolean eq = FALSE;

  if (cp1 == cp2) {
    /* Same pointer implies same constant. */
    eq = TRUE;
  } else if (cp1->kind          == cp2->kind &&
             cp1->type          == cp2->type &&
             cp1->implicit_cast == cp2->implicit_cast) {
    switch (cp1->kind) {
      case ck_error:
        /* No further field to check. */
        eq = TRUE;
        break;
      case ck_integer:
        eq = (cp1->variant.integer_value == cp2->variant.integer_value);
        break;
      case ck_string:
        if (cp1->variant.string.length == cp2->variant.string.length) {
          eq = (memcmp(cp1->variant.string.value, cp2->variant.string.value,
                       (int)cp1->variant.string.length) == 0);
        }  /* if */
        break;
      case ck_float:
        /* Note that set_constant_kind zeroes the entire float_value
           so a memcmp can be used.  We assume that the internal 
           representation always represents the same constant as the
           same set of bits. */
        eq = (memcmp((char *)&cp1->variant.float_value,
                     (char *)&cp2->variant.float_value,
                     sizeof(cp1->variant.float_value)) == 0);
        break;
      case ck_address:
        if (cp1->variant.address.kind   == cp2->variant.address.kind &&
            cp1->variant.address.offset == cp2->variant.address.offset) {
          switch (cp1->variant.address.kind) {
            case abk_routine:
              eq = (cp1->variant.address.variant.routine ==
                    cp2->variant.address.variant.routine);
              break;
            case abk_variable:
              eq = (cp1->variant.address.variant.variable ==
                    cp2->variant.address.variant.variable);
              break;
            case abk_constant:
              eq = (cp1->variant.address.variant.constant ==
                    cp2->variant.address.variant.constant);
              break;
#if CHECKING
            default:
              internal_error("eq_constants: bad address constant kind");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        break;
      case ck_ptr_to_member:
        if (cp1->variant.ptr_to_member.is_function_ptr ==
                                  cp2->variant.ptr_to_member.is_function_ptr) {
          if (cp1->variant.ptr_to_member.is_function_ptr) {
            eq = (cp1->variant.ptr_to_member.variant.routine ==
                  cp2->variant.ptr_to_member.variant.routine);
          } else {
            eq = (cp1->variant.ptr_to_member.variant.field ==
                  cp2->variant.ptr_to_member.variant.field);
          }  /* if */
        }  /* if */
        break;
#if CHECKING
      default:
        internal_error("eq_constants: bad constant kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return eq;
}  /* eq_constants */


static a_boolean has_non_file_scope_ref(a_constant *cp)
/*
Return TRUE if the constant pointer to by cp includes a reference to something
that's not in the file scope.  If it does, the constant cannot be allocated
at the file scope (it would contain a pointer down into a function scope).
This routine should not be called with constants of kind ck_aggregate (they
aren't shared, so they should always be allocated in the current memory 
region).
*/
{
  a_boolean has_nfs_ref = FALSE;

  /* cp->type is always in the file scope. */
  switch (cp->kind) {
    case ck_error:
    case ck_integer:
    case ck_float:
      /* No references. */
      break;
    case ck_string:
      /* String texts are always in the file scope. */
      break;
    case ck_address:
      switch (cp->variant.address.kind) {
        case abk_routine:
          /* Routines are always in the file scope. */
          break;
        case abk_variable:
          /* Static variables are always allocated in the file scope, and
             they are the only kind of variables whose address can be used
             in a constant address. */
#if CHECKING
          if (!has_static_storage_duration(
                        cp->variant.address.variant.variable->storage_class)) {
            internal_error("has_non_file_scope_ref: non-static var");
          }  /* if */
#endif /* CHECKING */
          break;
        case abk_constant:
          has_nfs_ref =
                  !in_file_scope((char *)cp->variant.address.variant.constant);
          break;
#if CHECKING
        default:
          internal_error("has_non_file_scope_ref: bad addr constant kind");
#endif /* CHECKING */
      }  /* switch */
      break;
    case ck_ptr_to_member:
      /* The class type and the object (if any) pointed to must be in the
         file scope. */
      break;
#if CHECKING
    case ck_aggregate:
      /* Aggregates shouldn't be shared, so we don't expect them here. */
    default:
      internal_error("has_non_file_scope_ref: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
  return has_nfs_ref;
}  /* has_non_file_scope_ref */


a_constant_ptr alloc_shareable_constant(a_constant *cp)
/*
Find or allocate a constant with the indicated value.  This constant
is shareable (it can be reused by others), and therefore the caller cannot
modify it (change its value or type, set its source correspondence, or
put it on a list of constants).
*/
{
  a_constant_ptr        scp, prev_scp;
  a_symbol_ptr          assoc_symbol;
  a_constant_hash_value hash_value;
  a_boolean             alloc_in_function_scope;
  a_constant_ptr        *list_ptr;

#if DEBUG
  num_searches_for_shareable_constants++;
#endif /* DEBUG */
  /* For constants with a source correspondence indicated, find the
     "master" copy by going up the source correspondence link and back
     down again. */
  if ((assoc_symbol = ((a_symbol_ptr)cp->source_corresp.assoc_info)) != NULL) {
    if (assoc_symbol->kind == (a_symbol_kind)sk_constant) {
      /* Constant (enumeration). */
      scp = assoc_symbol->variant.constant;
    } else {
      /* Macro defined as a manifest constant. */
#if CHECKING
      if (assoc_symbol->kind != (a_symbol_kind)sk_macro) {
        internal_error("alloc_shareable_constant: bad assoc_symbol kind");
      } else if (!assoc_symbol->variant.macro_def->is_manifest_constant) {
        internal_error("alloc_shareable_constant: macro not manifest const");
      }  /* if */
#endif /* CHECKING */
      /* Fetch the reference copy of the manifest constant value. */
      scp = assoc_symbol->variant.macro_def->constant_value;
#if CHECKING
      if (scp == NULL) {
        internal_error("alloc_shareable_constant: macro con not allocated");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
#if CHECKING
    if (cp->implicit_cast != scp->implicit_cast) {
      /* Someone did an implicit cast on the constant without clearing the
         source association. */
      internal_error(
           "alloc_shareable_constant: implicitly-cast const has assoc_info");
    }  /* if */
#endif /* CHECKING */
  } else {
    /* The constant has no source correspondence. */
    /* If the current IL region is not the file scope region (i.e., it's
       a function scope), and the constant has one or more references to 
       things that are in the function scope, the constant cannot be
       allocated and shared at the file scope. */
    alloc_in_function_scope = (curr_il_region_number !=
                               FILE_SCOPE_REGION_NUMBER) &&
                              has_non_file_scope_ref(cp);
    if (alloc_in_function_scope) {
      /* The constant cannot be shared at the file scope.  Look for a copy
         on the list of shared constants for the current function.  Bear
         in mind that the only constants likely to be on this list
         are those that represent the address of a local variable, so the
         list is going to be fairly short. */
      list_ptr = &scope_stack[depth_innermost_function_scope].
                                                      shareable_constants_list;
    } else {
      /* The constant can be shared at the file scope. */
      /* Look for a copy of the constant value in the
         shareable_constants_table. */
      /* Determine the bucket of the hash table to use. */
      hash_value = hash_constant(cp);
      list_ptr = &shareable_constants_table[hash_value];
    }  /* if */
    if (!string_literals_shared &&
        (cp->kind == (a_constant_repr_kind)ck_string ||
         (cp->kind == (a_constant_repr_kind)ck_address &&
          cp->variant.address.kind == (an_address_base_kind)abk_constant &&
          cp->variant.address.variant.constant->kind ==
                                           (a_constant_repr_kind)ck_string))) {
      /* We're not sharing string literals, and the constant is a string
         or the address of a string.  There's no point in putting such an 
         entry on the shared list since no one else will need the same
         constant anyway. */
      list_ptr = NULL;  /* No list. */
    }  /* if */
    if (list_ptr != NULL) {
      /* Search the entries in the list, if any. */
      for (prev_scp = NULL, scp = *list_ptr;
           scp != NULL;
           prev_scp = scp, scp = scp->next) {
#if DEBUG
        num_compares_for_shareable_constants++;
#endif /* DEBUG */
        /* Compare the constant in the list with the desired constant. */
        if (eq_constants(scp, cp)) {
          /* The constants are the same, so we have found a reusable
             constant. */
          /* Remove the constant from the list.  It will be re-added at the
             front of the list below.  This is so that common constants stay
             near the front of the bucket list, to speed lookup. */
          if (prev_scp == NULL) {
            *list_ptr = scp->next;
          } else {
            prev_scp->next = scp->next;
          }  /* if */
          break;
        }  /* if */
      }  /* for */
    } else {
      /* Not shareable. */
      scp = NULL;
    }  /* if */
    if (scp == NULL) {
      /* No identical constant exists in the table, so create one.  Note that
         in fe_wrapup shareable constants are moved onto the file-scope
         constant list.  Here, they cannot be, since the "next" field is needed
         to link them in the shareable constants table. */
      if (alloc_in_function_scope) {
        scp = alloc_constant(cp->kind);
      } else {
        scp = fs_constant(cp->kind);
      }  /* if */
      copy_constant(cp, scp);
#if DEBUG
      if (list_ptr != NULL) {
        if (alloc_in_function_scope) {
          num_func_shareable_constants++;
        } else {
          num_shareable_constants++;
          if (*list_ptr == NULL) {
            num_used_shareable_constant_buckets++;
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    if (list_ptr != NULL) {
      /* Add the shareable constant to the front of the proper list. */
      scp->next = *list_ptr;
      *list_ptr = scp;
    }  /* if */
  }  /* if */
  return scp;
}  /* alloc_shareable_constant */


void empty_shareable_constants_table(void)
/*
Empty out the file-scope shareable constants table and liberate the
constants therein by clearing their "next" fields.  This is called at
the end of compilation, when the shareable constants table is no
longer needed.  Note that the information in the table is not needed
to produce the debug space summary for the shareable constants table --
there are separate variables that are set already.
*/
{
  a_constant_ptr        scp, next_scp;
  a_constant_hash_value hash_value;

  /* For each bucket of the hash table ... */
  for (hash_value = 0;
       hash_value < SIZE_SHAREABLE_CONSTANTS_TABLE;
       hash_value++) {
    /* For each entry on the linked list for that bucket ... */
    for (scp = shareable_constants_table[hash_value];
         scp != NULL;
         scp = next_scp) {
      next_scp = scp->next;
      scp->next = NULL;
    }  /* for */
    shareable_constants_table[hash_value] = NULL;
  }  /* for */
}  /* empty_shareable_constants_table */


void empty_func_shareable_constants_table(void)
/*
Empty out the function-scope shareable constants table (actually, it's a
list), and liberate the constants therein by clearing their "next" fields.
*/
{
  a_constant_ptr scp, next_scp;

  scp = scope_stack[depth_innermost_function_scope].shareable_constants_list;
  for (; scp != NULL; scp = next_scp) {
    next_scp = scp->next;
    scp->next = NULL;
  }  /* for */
  scope_stack[depth_innermost_function_scope].shareable_constants_list = NULL;
}  /* empty_func_shareable_constants_table */


void add_to_constants_list(a_constant_ptr con_ptr)
/*
Add the given constant to the constants list for the file scope (not the
current scope).  This is used for manifest constant macros and (at the
end of compilation) for shareable constants.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;

  /* Get pointer to current scope entry. */
  ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
  sp = ssep->il_scope;
#if CHECKING
  if (sp == NULL) internal_error("add_to_constants_list: NULL IL scope");
#endif /* CHECKING */
  if (sp->constants == NULL) {
    sp->constants = con_ptr;
  } else {
    ssep->last_constant->next = con_ptr;
  }  /* if */
  ssep->last_constant = con_ptr;
  con_ptr->next = NULL;
}  /* add_to_constants_list */


void set_integer_constant(a_constant      *cp,
                          long            value,
                          an_integer_kind kind)
/*
Set the constant entry *cp to the integer constant given by value.
Its integer kind is as given by kind.
*/
{
  db_enter(5, "set_integer_constant");
  clear_constant(cp, (a_constant_repr_kind)ck_integer);
  cp->type = integer_type(kind);
  cp->variant.integer_value = value;
  db_exit();
}  /* set_integer_constant */


void make_zero_of_proper_type(a_type_ptr desired_type,
                              a_constant *zero_constant)
/*
Make a zero constant of type desired_type (a scalar type) and put it in
*zero_constant.  No IL allocation is done.  This routine is also handy
for making NULL pointer constants.
*/
{
  a_boolean did_not_fold;

  /* Make an integer zero and convert it to the desired type. */
  set_integer_constant(zero_constant, 0L, (an_integer_kind)ik_int);
  type_change_constant(zero_constant, desired_type,
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE, &did_not_fold,
                       &error_position);
}  /* make_zero_of_proper_type */


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


void set_arg_transfer_method_flag(a_param_type_ptr ptp)
/*
Set the flag in the indicated parameter type entry to indicate whether
or not the parameter should be passed using a copy constructor.
*/
{
  a_type_ptr param_type;

  if (C_dialect == C_dialect_cplusplus) {
    param_type = ptp->type;
    param_type = skip_typerefs(param_type);
    if (is_class_struct_union_type(param_type)) {
      /* The parameter is a class passed by value.  See if the class
         has a copy constructor. */
      if (symbol_supplement_for_class(param_type)->has_copy_constructor) {
        /* Yes. */
        ptp->passed_via_copy_constructor = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_arg_transfer_method_flag */


a_param_type_ptr alloc_param_type(a_type_ptr type)
/*
Allocate a new parameter type entry and return a pointer to it.  Set its
fields to default values and its type to "type".  It is always allocated
at file scope.
*/
{
  a_param_type_ptr ptp;

  db_enter(5, "alloc_param_type");

  ptp = (a_param_type_ptr)alloc_il(sizeof(a_param_type));
#if DEBUG
  num_param_types_allocated++;
#endif /* DEBUG */
  ptp->next = NULL;
  ptp->type = type;
  /* param_type entries are in the file scope memory region, so use the
     initial il_walk_flag setting for that region. */
  ptp->il_walk_flag = curr_fs_initial_il_walk_flag_setting;
  ptp->has_default_arg = FALSE;
  ptp->default_arg_expr = NULL;
  ptp->passed_via_copy_constructor = FALSE;
  set_arg_transfer_method_flag(ptp);
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
#endif
  bcp->next                            = NULL;
  bcp->type                            = NULL;
  bcp->is_virtual                      = FALSE;
  bcp->direct                          = FALSE;
  bcp->ambiguous                       = FALSE;
  bcp->any_virtual_steps_in_derivation = FALSE;
  bcp->access                          = (an_access_specifier)as_public;
  bcp->offset                          = 0;
  bcp->pointer_offset                  = 0;
  bcp->pointer_base_class              = NULL;
  bcp->derivation                      = NULL;
  bcp->overriding_virtual_functions    = NULL;
  bcp->base_class_with_same_virtual_function_info
                                       = NULL;
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
  bcp->complete_subobject              = FALSE;
  bcp->pointer_offset_is_set           = FALSE;
  bcp->data_section_base_class         = NULL;
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
#if DO_IL_LOWERING
  bcp->virtual_function_table_var      = NULL;
#endif /* DO_IL_LOWERING */

  return bcp;
}  /* alloc_base_class */


an_access_adjustment_ptr alloc_access_adjustment(an_access_adjustment_kind kind)
/*
Allocate an access-adjustment entry, initialize its fields, and return a
pointer to it.
*/
{
  an_access_adjustment_ptr aap;

  aap = (an_access_adjustment_ptr)alloc_il(sizeof(an_access_adjustment));
#if DEBUG
  num_access_adjustments_allocated++;
#endif /* DEBUG */
  aap->next    = NULL;
  aap->access  = (an_access_specifier)as_public;
  aap->kind    = kind;
  switch (kind) {
    case aak_field:     aap->variant.field    = NULL;  break;
    case aak_variable:  aap->variant.variable = NULL;  break;
    case aak_routine:   aap->variant.routine  = NULL;  break;
    case aak_constant:  aap->variant.constant = NULL;  break;
    case aak_type:      aap->variant.type     = NULL;  break;
  }  /* switch */

  return aap;
}  /* alloc_access_adjustment */


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
  ctsp->virtual_function_count            = 0;
  ctsp->virtual_function_info_offset      = 0;
  ctsp->anonymous_union_kind              = (an_anonymous_union_kind)auk_none;
  ctsp->anonymous_union_field             = NULL;
  ctsp->access_adjustments                = NULL;
  ctsp->befriending_classes               = NULL;
  ctsp->assoc_scope                       = NULL;
#if ASSIGNMENT_TO_THIS_ALLOWED
  ctsp->assoc_operator_new_routine        = NULL;
  ctsp->assoc_operator_delete_routine     = NULL;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
#if DO_IL_LOWERING
  ctsp->virtual_function_table_var        = NULL;
  ctsp->type_as_subobject                 = NULL;
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
      pte->variant.integer.enum_constant_list = NULL;
      break;
    case tk_float:
      pte->variant.float_kind = (a_float_kind)fk_float;
      break;
    case tk_pointer:
      pte->variant.pointer.type = NULL;
      pte->variant.pointer.is_reference = FALSE;
      break;
    case tk_array:
      pte->variant.array.element_type = NULL;
      pte->variant.array.number_of_elements = 0;
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      pte->variant.class_struct_union.field_list       = NULL;
      pte->variant.class_struct_union.any_const_member = FALSE;
      pte->variant.class_struct_union.any_virtual_base_classes = FALSE;
      pte->variant.class_struct_union.abstract         = FALSE;
      /* The class type supplement is only allocated in C++ mode. */
      pte->variant.class_struct_union.extra_info       = 
                                           (C_dialect == C_dialect_cplusplus) ?
                                                alloc_class_type_supplement() :
                                                NULL;
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
      rtsp->implicit_this_param_type = NULL;
      rtsp->caller_provides_place_to_put_return_value
                                     = NULL;
      rtsp->prototype_scope          = NULL;
      rtsp->assoc_routine            = NULL;
      rtsp->prototyped               = FALSE;
      rtsp->has_ellipsis             = FALSE;
      rtsp->lint_argsused_flag       = FALSE;
      rtsp->constructor_or_destructor= FALSE;
      rtsp->lint_varargs_count       = NOT_LINT_VARARGS;
      rtsp->arg_pragma               = (an_arg_pragma_kind)apk_none;
      break;
    case tk_typeref:
      pte->variant.typeref.type        = NULL;
#if DO_IL_LOWERING
      pte->variant.typeref.orig_member_type = NULL;
#endif /* DO_IL_LOWERING */
      pte->variant.typeref.is_const    = FALSE;
      pte->variant.typeref.is_volatile = FALSE;
      pte->variant.typeref.is_function_scope_tag = FALSE;
      break;
    case tk_ptr_to_member:
      pte->variant.ptr_to_member.class_of_which_a_member = FALSE;
      pte->variant.ptr_to_member.type                    = FALSE;
      break;
#if CHECKING
    default:
      internal_error("set_type_kind: bad type kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_type_kind */


static void clear_type(a_type_ptr  pte,
                       a_type_kind kind)
/*
Clear the indicated type entry, set the kind as given, and set the associated
variant fields to default values.
*/
{
  set_default_source_corresp(&(pte->source_corresp));
  pte->next = NULL;
  pte->based_types = NULL;
  pte->size = 0;
  pte->alignment = 1;
  set_type_kind(pte, kind);
}  /* clear_type */


static a_scope_ptr ensure_il_scope_exists(a_scope_stack_entry_ptr ssep)
/*
Make sure that the scope stack entry pointed to by ssep points to an IL
scope.  For block scopes, create the scope now if necessary.
*/
{
  a_scope_ptr            sp = ssep->il_scope;
  a_memory_region_number region_to_switch_back_to;

  if (sp == NULL) {
    /* There is no IL scope. */
    if (ssep->kind == (a_scope_kind)sck_block) {
      /* Create the IL scope in a block scope. */
      region_to_switch_back_to = curr_il_region_number;
      switch_il_region(ssep->il_memory_region);
      ssep->il_scope = sp = alloc_scope((a_scope_kind)sck_block, ssep->number,
                                        (a_routine_ptr)NULL);
      switch_il_region(region_to_switch_back_to);
      /* Add it to the scopes list for the scope enclosing the scope indicated
         by ssep. */
      add_to_scopes_list(sp, ssep-1);
#if CHECKING
    } else if (ssep->kind != (a_scope_kind)sck_func_prototype) {
      internal_error("ensure_il_scope_exists: NULL IL scope");
#endif /* CHECKING */
    }  /* if */
  }  /* if */
  return sp;
}  /* ensure_il_scope_exists */


void add_to_types_list(a_type_ptr     type_ptr,
                       a_scope_depth  scope_level,
                       a_boolean      in_old_style_param_decl_list)
/*
Add the given type to the types list for the current scope, or at file scope
if at_file_scope is TRUE, or in a prototype scope if
in_old_style_param_decl_list is TRUE.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;
  a_type_ptr              routine_type;
  a_type_ptr              last_type_ptr;
  a_type_ptr              *last_type_ptr_ptr;
  a_memory_region_number  region_to_switch_back_to;

  /* Get a pointer to the current or file scope entry. */
  ssep = &scope_stack[scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  last_type_ptr_ptr = &ssep->last_type;
  /* If we are currently inside the declaration list for the old-style
     parameters of a function (e.g., in the "struct" line in

     int f(a)
     struct s {int b;} a;
     {
     }

     ), the type should be entered in the prototype scope; that makes
     the type available in the memory region of the function's parent,
     which is necessary for type-compatibility checking of parameters.
     A similar situation applies for type declarations that are part
     of function prototypes, as in

     int f(struct s {int b;} a);

     The prototype scope is hardly ever needed, and therefore it is not
     allocated by default.  It is allocated here in this routine the first
     time it is needed.  For old-style parameters, the prototype scope
     is saved under the type of the routine being defined.  For prototypes,
     the prototype scope is saved in il_scope of the current scope stack
     entry and also under the associated routine type. */
  if (in_old_style_param_decl_list) {
    /* We are in the declaration list for the old-style parameters of
       a function. */
#if CHECKING
    if (sp == NULL) internal_error("add_to_types_list: missing il_scope");
    if (sp->variant.routine.ptr == NULL) {
      internal_error("add_to_types_list: missing assoc_routine");
    }  /* if */
#endif /* CHECKING */
    routine_type = sp->variant.routine.ptr->type;
#if CHECKING
    if (routine_type == NULL) {
      internal_error("add_to_types_list: NULL routine type");
    } else if (routine_type->kind != (a_type_kind)tk_routine) {
      internal_error("add_to_types_list: bad routine type");
    }  /* if */
#endif /* CHECKING */
    /* Get the prototype scope pointer from the routine type supplement.
       It may already have been created. */
    sp = routine_type->variant.routine.extra_info->prototype_scope;
    /* Find the last type on the list, since we have no last pointer we can
       use directly.  These lists are not likely to be long, so this is not
       a big deal. */
    last_type_ptr_ptr = &last_type_ptr;
    if (sp == NULL) {
      last_type_ptr = NULL;
    } else {
      last_type_ptr = sp->types;
      if (last_type_ptr != NULL) {
        while (last_type_ptr->next != NULL) {
          last_type_ptr = last_type_ptr->next;
        }  /* while */
      }  /* if */
    }  /* if */
  }  /* if */
  if (sp == NULL) {
    /* A prototype scope must be allocated.  add_to_scopes_list is not
       called because this is not a scope for a statement block.  It is
       allocated in the file scope memory region because it is pointed to
       from the routine type supplement, which is always at file scope. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    sp = alloc_scope((a_scope_kind)sck_func_prototype, ssep->number,
                     (a_routine_ptr)NULL);
    switch_back_to_original_region(region_to_switch_back_to);
    if (!in_old_style_param_decl_list) {
      /* Function prototype scope. */
      ssep->il_scope = sp;
      routine_type = ssep->assoc_type;
#if CHECKING
      if (routine_type == NULL) {
        internal_error("add_to_types_list: assoc_routine_type is NULL");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
    /* Link the routine type to the prototype scope entry. */
    routine_type->variant.routine.extra_info->prototype_scope = sp;
    /* Link the prototype scope entry to the routine type. */
    sp->variant.assoc_type = routine_type;
  }  /* if */
  /* Add the type to the list of types for this scope. */
  if (sp->types == NULL) {
    sp->types = type_ptr;
  } else {
    (*last_type_ptr_ptr)->next = type_ptr;
  }  /* if */
  *last_type_ptr_ptr = type_ptr;
  type_ptr->next = NULL;
}  /* add_to_types_list */


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
  /* Type entries are always in the file scope, so use the il_walk_flag
     value for the file scope memory region. */
  tp->source_corresp.il_walk_flag = curr_fs_initial_il_walk_flag_setting;
  db_exit();
  return tp;
}  /* alloc_type */


a_type_ptr integer_type(an_integer_kind kind)
/*
Make or find a type entry for an integer type of the indicated kind, and
return a pointer to it.
*/
{
  a_type_ptr pit;

  if (int_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pit = int_types[kind];
  } else {
    /* The type must be created. */
    int_types[kind] = pit = alloc_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = kind;
    set_type_size(pit);
  }  /* if */
  return pit;
}  /* integer_type */


a_type_ptr signed_int_type(void)
/*
Make or find a type entry for an explicitly signed "int" type.  Keeping
track of the difference between a plain "int" and a "signed int" is
necessary because the two may be different for bit fields.  Return a
pointer to the type entry.
*/
{
  if (il_signed_int_type == NULL) {
    /* The type must be created. */
    il_signed_int_type = alloc_type((a_type_kind)tk_integer);
    il_signed_int_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    il_signed_int_type->variant.integer.explicitly_signed = TRUE;
    set_type_size(il_signed_int_type);
  }  /* if */
  return il_signed_int_type;
}  /* signed_int_type */


a_type_ptr float_type(a_float_kind kind)
/*
Make or find a type entry for a float type of the indicated kind, and
return a pointer to it.
*/
{
  a_type_ptr pft;

  if (float_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pft = float_types[kind];
  } else {
    /* The type must be created. */
    float_types[kind] = pft = alloc_type((a_type_kind)tk_float);
    pft->variant.float_kind = kind;
    set_type_size(pft);
  }  /* if */
  return pft;
}  /* float_type */


a_type_ptr string_type(a_targ_size_t num_chars)
/*
Make or find an entry for a type that is an array of num_char characters,
and return a pointer to it.
*/
{
  a_type_ptr pst;

  if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH &&
      string_types[num_chars] != NULL) {
    /* The type has previously been created, and can be reused. */
    pst = string_types[num_chars];
  } else {
    /* The type must be created. */
    pst = alloc_type((a_type_kind)tk_array);
    pst->variant.array.element_type = integer_type(plain_char_int_kind);
    pst->variant.array.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      string_types[num_chars] = pst;
    }  /* if */
  }  /* if */
  return pst;
}  /* string_type */


a_type_ptr wide_string_type(a_targ_size_t num_chars)
/*
Make or find an entry for a type that is an array of num_char wchar_t elements,
and return a pointer to it.
*/
{
  a_type_ptr pst;

  if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH &&
      wide_string_types[num_chars] != NULL) {
    /* The type has previously been created, and can be reused. */
    pst = wide_string_types[num_chars];
  } else {
    /* The type must be created. */
    pst = alloc_type((a_type_kind)tk_array);
    pst->variant.array.element_type =
                          integer_type((an_integer_kind)TARG_WCHAR_T_INT_KIND);
    pst->variant.array.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      wide_string_types[num_chars] = pst;
    }  /* if */
  }  /* if */
  return pst;
}  /* wide_string_type */


an_integer_kind char_int_kind_from_string_type(a_type_ptr str_type)
/*
Return the character element integer kind (char or wchar_t) from the
indicated string type.
*/
{
  a_type_ptr elem_type;
  an_integer_kind int_kind;

#if CHECKING
  if (!is_array_type(str_type)) {
    internal_error("char_int_kind_from_string_type: bad type");
  }  /* if */
#endif /* CHECKING */
  elem_type = array_element_type(str_type);
#if CHECKING
  if (!is_integral_type(elem_type)) {
    internal_error("char_int_kind_from_string_type: bad elem type");
  }  /* if */
#endif /* CHECKING */
  elem_type = skip_typerefs(elem_type);
  int_kind = elem_type->variant.integer.int_kind;
  return int_kind;
}  /* char_int_kind_from_string_type */


a_type_ptr error_type(void)
/*
Make or find a type entry for an error type, and return a pointer to it.
*/
{
  if (il_error_type == NULL) {
    il_error_type = alloc_type((a_type_kind)tk_error);
    set_type_size(il_error_type);
  }  /* if */
  return il_error_type;
}  /* error_type */


a_type_ptr unknown_type(void)
/*
Make or find a type entry for an unknown type, and return a pointer to it.
Such a type is only used in the front end; it does not survive into the back
end.
*/
{
  if (il_unknown_type == NULL) {
    il_unknown_type = (a_type_ptr)alloc_fe(sizeof(a_type));
    clear_type(il_unknown_type, (a_type_kind)tk_unknown);
    set_type_size(il_unknown_type);
  }  /* if */
  return il_unknown_type;
}  /* unknown_type */


a_type_ptr void_type(void)
/*
Make or find a type entry for an void type, and return a pointer to it.
*/
{
  if (il_void_type == NULL) {
    il_void_type = alloc_type((a_type_kind)tk_void);
  }  /* if */
  return il_void_type;
}  /* void_type */


a_type_ptr get_based_type(a_type_ptr        base_type,
                          a_based_type_kind kind,
                          a_type_ptr        class_type)
/*
Search the based_types list of base_type to see if it contains a based type
of the kind indicated by "kind".  If the kind is "btk_ptr_to_member", the
specified "class_type" must also match "class_of_which_a_member" of that
based type. Return a pointer to the type if such an entry exists, or NULL
if no such entry exists.  The based_types list is used to hold pointers to
types based on the base type, so that only one copy of pointer-to that type,
reference-to that type, etc., is allocated.  As a simple optimization to
based type lookup, move the desired based type, if found, to the front of
the list.  This will tend to keep frequently asked for based types at the
front of the list.
*/
{
  register a_type_ptr                   ptr = NULL;
  register a_based_type_list_member_ptr btlmp;
  register a_based_type_list_member_ptr prev_btlmp;

#if DEBUG
  num_get_based_type_calls++;
#endif /* DEBUG */
  /* Search the based_types list looking for an entry of the right kind. */
  for (btlmp = base_type->based_types, prev_btlmp = NULL;
       btlmp != NULL;
       prev_btlmp = btlmp, btlmp = btlmp->next) {
    if (btlmp->kind == kind) {
      if (kind != (a_based_type_kind)btk_ptr_to_member ||
          btlmp->based_type->variant.ptr_to_member.class_of_which_a_member
                                                        == class_type) {
        ptr = btlmp->based_type;
        /* Move the found based type list member to the front of the list. */
        if (prev_btlmp != NULL) {
          /* There is at least one base type list member preceding this one;
             move the current based type member to the front of the list. */
          prev_btlmp->next = btlmp->next;
          btlmp->next = base_type->based_types;
          base_type->based_types = btlmp;
        }  /* if */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return ptr;
}  /* get_based_type */


void add_based_type_list_member(a_type_ptr        base_type,
                                a_based_type_kind kind,
                                a_type_ptr        based_type)
/*
Add a based type list member to the based_types list of base_type
indicating that based_type is a type based on base_type, and the relationship
between the types is described by kind.  The entry is allocated in the
same memory region as base_type.  This routine does not check to see if
there is already an entry of the indicated kind on the list.
*/
{
  a_based_type_list_member_ptr btlmp;

  btlmp = (a_based_type_list_member_ptr)alloc_il(
                                             sizeof(a_based_type_list_member));
#if DEBUG
  num_based_type_list_members_allocated++;
#endif /* DEBUG */
  /* Add the entry to the front of the existing based_types list. */
  btlmp->next = base_type->based_types;
  base_type->based_types = btlmp;
  /* Initialize the entry. */
  btlmp->kind = kind;
  btlmp->based_type = based_type;
}  /* add_based_type_list_member */


a_type_ptr ptr_to_member_type(a_type_ptr  member_type,
                              a_type_ptr  class_type)
/*
Allocate and return a pointer-to-member type, initializing its fields based
on the specified member and class types.  Attempt to find and reuse an
existing type entry.
*/
{
  register a_type_ptr tp;

  /* Check if this is an incomplete type being formed. */
  if (member_type != NULL) {
    /* See if a pointer-to-member type such as the one being requested has
       already been allocated.  If one was allocated, a pointer to it is
       stored in the based_types list of the member type, and the pointer
       can be reused. */
    tp = get_based_type(member_type, (a_based_type_kind)btk_ptr_to_member,
                        class_type);
  }  /* if */
  if (member_type == NULL || tp == NULL) {
    /* No member type (as of yet) or no previously allocated entry, need
       to allocate one. */
    tp = alloc_type((a_type_kind)tk_ptr_to_member);
    tp->variant.ptr_to_member.type = member_type;
    tp->variant.ptr_to_member.class_of_which_a_member = class_type;
    /* If member_type is NULL we are creating an incomplete type; otherwise,
       set its type and alignment. */
    if (member_type != NULL) {
      set_type_size(tp);
      add_based_type_list_member(member_type,
                                 (a_based_type_kind)btk_ptr_to_member, tp);
    }  /* if */
  }  /* if */
  return tp;
}  /* ptr_to_member_type */


a_type_ptr make_pointer_type(a_type_ptr type_pointed_to)
/*
Allocate a pointer type record and initialize it.  Attempt to find and reuse
an existing entry if possible.
*/
{
  register a_type_ptr ptr;

  /* See if a pointer type for the type pointed to has already been allocated.
     If one was allocated, a pointer to it is stored in the based_types list
     for the base type, and the pointer type can be reused. */
  ptr = get_based_type(type_pointed_to, (a_based_type_kind)btk_pointer,
                       /*class_type=*/ NULL);
  if (ptr == NULL) {
    /* No allocated entry, need to allocate one. */
    ptr = alloc_type((a_type_kind)tk_pointer);
    ptr->variant.pointer.type = type_pointed_to;
    set_type_size(ptr);
    /* Remember the existence of this pointer type by putting a pointer
       to it in the based_types list. */
    add_based_type_list_member(type_pointed_to, (a_based_type_kind)btk_pointer,
                               ptr);
  }  /* if */

  return ptr;
}  /* make_pointer_type */


a_type_ptr make_reference_type(a_type_ptr type_pointed_to)
/*
Allocate a reference type record and initialize it.  Attempt to find and reuse
an existing entry if possible.
*/
{
  register a_type_ptr ptr;

  /* See if a reference type for the type pointed to has already been
     allocated.  If one was allocated, a pointer to it is stored in the
     based_types list for the base type, and the reference type can be
     reused. */
  ptr = get_based_type(type_pointed_to, (a_based_type_kind)btk_reference,
                       /*class_type=*/ NULL);
  if (ptr == NULL) {
    /* No allocated entry, need to allocate one. */
    ptr = alloc_type((a_type_kind)tk_pointer);
    ptr->variant.pointer.type = type_pointed_to;
    ptr->variant.pointer.is_reference = TRUE;
    set_type_size(ptr);
    /* Remember the existence of this reference type by putting a pointer
       to it in the based_types list. */
    add_based_type_list_member(type_pointed_to,
                               (a_based_type_kind)btk_reference, ptr);
  }  /* if */

  return ptr;
}  /* make_reference_type */


a_type_ptr make_qualified_type(a_type_ptr base_type,
                               a_boolean  is_const,
                               a_boolean  is_volatile)
/*
Make a version of the type base_type with the additional type qualifiers
indicated by is_const and is_volatile.  Attempt to find and reuse
an existing entry if possible.  The qualifiers are added only if
they are not already present.
*/
{
  a_type_ptr        orig_base_type, ptr, prev_new_array, top_new_array;
  a_type_ptr        old_array, new_array;
  a_based_type_kind kind;
  a_boolean         base_type_const_qualified, base_type_volatile_qualified;
  a_boolean         set_const_qualified, set_volatile_qualified;

  orig_base_type = base_type;
  /* According to ANSI C 3.5.3: "If the specification of an array type
     includes any type qualifiers, the element type is so-qualified,
     not the array type.", and this is interpreted recursively
     for arrays of arrays.  The type qualifiers therefore apply
     to the ultimate element type.  This can only happen with typedefs,
     as in "typedef int A[2][3]; const A a;", which makes "a" an
     array of array of const int. */
  while (is_array_type(base_type)) {
    base_type = array_element_type(base_type);
  }  /* while */
  base_type_const_qualified    = is_const_qualified_type(base_type);
  base_type_volatile_qualified = is_volatile_qualified_type(base_type);
  set_const_qualified  = is_const && !base_type_const_qualified;
  set_volatile_qualified = is_volatile && !base_type_volatile_qualified;
  if (set_const_qualified || set_volatile_qualified) {
    /* Some qualifiers need to be added. */
    /* Type qualifiers are added by adding a typeref entry which includes
       the type qualifiers.  The original type is not modified. */
    /* See if a typeref for the base type has already been allocated.
       If one was allocated, a pointer to it is stored in the based_types
       list for the base type, and the pointer type can be reused. */
    /* Determine the based type kind. */
    if (is_const) {
      if (is_volatile) {
        kind = (a_based_type_kind)btk_const_volatile;
      } else {
        kind = (a_based_type_kind)btk_const;
      }  /* if */
    } else {
      kind = (a_based_type_kind)btk_volatile;
    }  /* if */
    ptr = get_based_type(base_type, kind, /*class_type=*/ NULL);
    if (ptr == NULL) {
      /* No allocated entry, need to allocate one. */
      ptr = alloc_type((a_type_kind)tk_typeref);
      ptr->variant.typeref.type        = base_type;
      ptr->variant.typeref.is_const    = (is_const != 0);
      ptr->variant.typeref.is_volatile = (is_volatile != 0);
      /* Remember the existence of this typeref type by putting a pointer
         to it in the based_types list. */
      add_based_type_list_member(base_type, kind, ptr);
    }  /* if */
    /* For the strange array case, the array type entries must be
       copied in order to avoid changing the typedef type. */
    if (base_type != orig_base_type) {
      prev_new_array = NULL;
      for (old_array = orig_base_type;
           old_array != base_type;
           old_array = old_array->variant.array.element_type) {
        /* Drop typedefs; there shouldn't be any typerefs. */
        old_array = skip_typerefs(old_array);
#if CHECKING
        if (old_array->kind != (a_type_kind)tk_array) {
          internal_error("make_qualified_type: not array in loop");
        }  /* if */
#endif /* CHECKING */
        new_array = alloc_type((a_type_kind)tk_array);
        copy_type(old_array, new_array);
        break_source_corresp(&new_array->source_corresp);
        if (prev_new_array == NULL) {
          top_new_array = new_array;
        } else {
          prev_new_array->variant.array.element_type = new_array;
        }  /* if */
        prev_new_array = new_array;
      }  /* for */
      /* The new qualified type is attached to the bottom of the new chain
         of array types. */
      prev_new_array->variant.array.element_type = ptr;
      ptr = top_new_array;
    }  /* if */
  } else {
    /* No qualifiers to add, so return the original type. */
    ptr = orig_base_type;
  }  /* if */

  return ptr;
}  /* make_qualified_type */


a_type_ptr make_identically_qualified_type(a_type_ptr type,
                                           a_type_ptr model_type)
/*
Make a version of type that has the same qualifiers as model_type, and return
a pointer to it.  The original qualifiers on type, if any, are ignored.
Note that type and model_type need not be the same (or even similar) types
under the qualifiers.
*/
{
  a_type_ptr new_type;

  new_type = make_qualified_type(skip_typerefs(type),
                                 is_const_qualified_type(model_type),
                                 is_volatile_qualified_type(model_type));
  return new_type;
}  /* make_identically_qualified_type */


a_type_ptr type_plus_qualifiers_from_second_type(a_type_ptr type,
                                                 a_type_ptr model_type)
/*
Make a version of type that has the same qualifiers as model_type, and return
a pointer to it.  The original qualifiers on type, if any, are preserved,
which means that the result type has all the qualifiers of both types.
Note that type and model_type need not be the same (or even similar) types
under the qualifiers.
*/
{
  a_type_ptr new_type;

  new_type = make_qualified_type(type,
                                 is_const_qualified_type(model_type),
                                 is_volatile_qualified_type(model_type));
  return new_type;
}  /* type_plus_qualifiers_from_second_type */


a_type_ptr make_unqualified_type(a_type_ptr type)
/*
Return a type that is the unqualified version of the type given by type.
*/
{
  /* Remove the minimum number of typerefs that will produce an unqualified
     type, in order to save typedefs if possible. */
  while (is_qualified_type(type)) {
    type = type->variant.typeref.type;
  }  /* while */

  return type;
}  /* make_unqualified_type */


void set_routine_calling_method_flag(a_type_ptr routine_type)
/*
Set the calling-method flag in the indicated routine type; that flag
is used when the function result is returned to a temporary provided by
the caller.  This routine may be called more than once, since the
information on the return type can be incomplete at the original
declaration of the function and must be completed by the point of call.
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_type_ptr                    return_type;

  routine_type = skip_typerefs(routine_type);
  rtsp = routine_type->variant.routine.extra_info;
  if (rtsp->assoc_routine != NULL) {
    /* The routine has been defined, so the flag is set correctly. */
  } else if (C_dialect != C_dialect_cplusplus) {
    /* The flags cannot be set in C mode. */
  } else {
    /* If the function returns a class object that has a copy
       constructor, make the caller provide a temporary for the result. */
    return_type = routine_type->variant.routine.return_type;
    return_type = skip_typerefs(return_type);
    if (is_class_struct_union_type(return_type)) {
      if (symbol_supplement_for_class(return_type)->has_copy_constructor) {
        rtsp->caller_provides_place_to_put_return_value = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_routine_calling_method_flag */


void copy_type(a_type_ptr from,
               a_type_ptr to)
/*
Copy the type entry "from" to "to".
*/
{
  a_type_kind                   from_kind;
  a_routine_type_supplement_ptr extra_info;
  a_type_ptr                    next_ptr;
  a_boolean                     il_walk_flag;

  from_kind = from->kind;
  if (from_kind == (a_type_kind)tk_routine) {
    /* For a routine type, preserve the type supplement pointer for the
       copy below. */
    extra_info = to->variant.routine.extra_info;
  }  /* if */
  /* Preserve the "next" pointer in the "to" entry. */
  next_ptr = to->next;
  /* Preserve the IL walk flag. */
  il_walk_flag = to->source_corresp.il_walk_flag;
  /* Copy the type entry. */
  *to = *from;
  to->next = next_ptr;
  to->source_corresp.il_walk_flag = il_walk_flag;
  to->based_types = NULL;
  if (from_kind == (a_type_kind)tk_array) {
    /* For an array type, check for an array based on an incomplete struct
       or union type.  Such a type must be placed on the fixup list. */
    add_if_necessary_to_array_fixup_list(to);
  } else if (from_kind == (a_type_kind)tk_routine) {
    /* For a routine type, the type supplement must also be copied. */
    *extra_info = *from->variant.routine.extra_info;
    to->variant.routine.extra_info = extra_info;
  }  /* if */
}  /* copy_type */


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
    case dik_member_copy:
    case dik_base_class_copy:
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
      dip->variant.constant = NULL;
      break;
    case dik_expression:
      dip->variant.expression = NULL;
      break;
    case dik_constructor:
      dip->variant.constructor.routine = NULL;
      dip->variant.constructor.args = NULL;
      dip->variant.constructor.is_copy_constructor_for_subobject = FALSE;
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
  dip->next       = NULL;
  dip->variable   = NULL;
  dip->destructor = NULL;
  dip->follows_an_exec_statement = FALSE;
  set_dynamic_init_kind(dip, kind);
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


a_dynamic_init_ptr alloc_dtor_dynamic_init(a_dynamic_init_kind kind,
                                           a_type_ptr          type)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind, and return a pointer to it.  If type is a type that
requires a destructor, put the destructor routine pointer into the dynamic
initialization entry.
*/
{
  a_dynamic_init_ptr            dip = alloc_dynamic_init(kind);
  a_class_symbol_supplement_ptr cssp;

  if (is_class_struct_union_type(type)) {
    /* The class is a class. */
    cssp = symbol_supplement_for_class(type);
    if (cssp != NULL) {
      /* The class is a C++ class. */
      if (cssp->destructor != NULL) {
        /* The class has a destructor. */
        dip->destructor = cssp->destructor->variant.routine;
        reference_to_implicitly_invoked_function(cssp->destructor,
                                                 &error_position);
      }  /* if */
    }  /* if */
  }  /* if */
  return dip;
}  /* alloc_dtor_dynamic_init */


void add_to_dynamic_inits_list(a_dynamic_init_ptr dip)
/*
Add the given dynamic initialization entry to the dynamic_inits list for
the current scope.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;

  /* Get pointer to current scope entry. */
  ssep = &scope_stack[decl_scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  if (sp->dynamic_inits == NULL) {
    sp->dynamic_inits = dip;
  } else {
    ssep->last_dynamic_init->next = dip;
  }  /* if */
  ssep->last_dynamic_init = dip;
  dip->next = NULL;
}  /* add_to_dynamic_inits_list */


static void add_rewritten_temporary(a_variable_ptr old_temp,
                                    a_variable_ptr new_temp)
/*
Allocate an entry to record the fact that during an expression copy
the temporary variable old_temp should be replaced by the temporary variable
new_temp.  Add the entry to the beginning of the rewritten_temporaries list.
*/
{
  a_rewritten_temporary_ptr rtp;

  if (avail_rewritten_temporaries != NULL) {
    /* Reuse a previously-freed entry. */
    rtp = avail_rewritten_temporaries;
    avail_rewritten_temporaries = avail_rewritten_temporaries->next;
  } else {
    /* Allocate a new entry. */
    rtp = (a_rewritten_temporary_ptr)alloc_fe(sizeof(a_rewritten_temporary));
  }  /* if */
  /* Initialize the entry and put in on the front of the list. */
  rtp->old_temp = old_temp;
  rtp->new_temp = new_temp;
  rtp->next = rewritten_temporaries;
  rewritten_temporaries = rtp;
}  /* add_rewritten_temporary */


static void free_rewritten_temporaries(void)
/*
Free the entries on the rewritten_temporaries list by putting them on
the avail_rewritten_temporaries list.
*/
{
  a_rewritten_temporary_ptr rtp;

  if (rewritten_temporaries != NULL) {
    /* Find the last entry on the list. */
    rtp = rewritten_temporaries;
    while (rtp->next != NULL) rtp = rtp->next;
    /* Put the list of entries on the front of the available list. */
    rtp->next = avail_rewritten_temporaries;
    avail_rewritten_temporaries = rewritten_temporaries;
    rewritten_temporaries = NULL;
  }  /* if */
}  /* free_rewritten_temporaries */


static a_boolean is_temporary_var(a_variable_ptr var)
/*
Return TRUE if the indicated variable is a temporary.
*/
{
  a_boolean                   is_temporary;
  a_type_ptr                  var_type;
  a_class_type_supplement_ptr ctsp;

  if (var->source_corresp.name != NULL) {
    /* The variable has a name, so it's not a temporary. */
    is_temporary = FALSE;
  } else {
    is_temporary = TRUE;
    /* Check for anonymous union variables. */
    if (C_dialect == C_dialect_cplusplus) {
      var_type = var->type;
      if (is_class_struct_union_type(var_type)) {
        /* The variable has a class type. */
        var_type = skip_typerefs(var_type);
        ctsp = var_type->variant.class_struct_union.extra_info;
        if (ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_none) {
          /* The variable is an anonymous union, so it's not a temporary. */
          is_temporary = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_temporary;
}  /* is_temporary_var */


static a_variable_ptr rewrite_if_temporary(a_variable_ptr var)
/*
If old_temp points to a temporary variable, and temporaries are being
cloned during an expression copy, determine the new temporary variable
with which it should be replaced, and return a pointer to that new
variable.  Otherwise (i.e., if temporaries are not being cloned or
if the variable is not a temporary), return the original variable.
*/
{
  a_rewritten_temporary_ptr   rtp;
  a_variable_ptr              new_var;

  if (clone_temps_on_expr_copy && is_temporary_var(var)) {
    /* The variable is a temporary and should be rewritten. */
    /* See if the temporary already appears on the list of rewritten
       temporaries.  If so, we've already assigned the corresponding new
       temporary. */
    for (rtp = rewritten_temporaries; rtp != NULL; rtp = rtp->next) {
      if (rtp->old_temp == var) {
        /* Found a match.  Use it. */
        var = rtp->new_temp;
        goto done;
      }  /* if */
    }  /* for */
    /* This temporary has not been seen before.  Allocate a new temporary
       and remember the correspondence. */
    new_var = alloc_temporary_variable(var->type);
    add_rewritten_temporary(var, new_var);
    var = new_var;
  }  /* if */
done:
  return var;
}  /* rewrite_if_temporary */


static a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr dip)
/*
Make a copy of a dynamic initialization entry and return a pointer to the copy.
This is not a general-purpose routine -- it is meant to be called from
copy_expr_tree for the kinds of dynamic initializations done under an
expression node.
*/
{
  a_dynamic_init_ptr new_dip;

  new_dip = alloc_dynamic_init(dip->kind);
  *new_dip = *dip;
  /* Rewrite the variable if there is one and it's a temporary. */
  if (dip->variable != NULL) {
    new_dip->variable = rewrite_if_temporary(dip->variable);
  }  /* if */
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_expression:
      new_dip->variant.expression =
                              internal_copy_expr_tree(dip->variant.expression);
      break;
    case dik_constructor:
      new_dip->variant.constructor.args =
               internal_copy_list_of_expr_trees(dip->variant.constructor.args);
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
      /* The constant pointed to is unshared and must be copied. */
      new_dip->variant.constant =
                                 copy_unshared_constant(dip->variant.constant);
      break;
#if CHECKING
    case dik_member_copy:
    case dik_base_class_copy:
      /* These kinds are not expected under expression nodes. */
    default:
      internal_error("copy_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return new_dip;
}  /* copy_dynamic_init */


a_variable_ptr alloc_variable(a_storage_class  storage_class)
/*
Allocate a variable entry, clear it to default values, and return a pointer
to it.
*/
{
  a_variable_ptr vp;
  a_boolean      at_file_scope = FALSE;

  db_enter(5, "alloc_variable");

  if (storage_class == (a_storage_class)sc_extern ||
      storage_class == (a_storage_class)sc_unspecified ||
      storage_class == (a_storage_class)sc_static) {
    /* Variable that will have static storage should always be allocated in
       the file scope memory region. */
    vp = (a_variable_ptr)alloc_il(sizeof(a_variable));
    at_file_scope = TRUE;
  } else {
    vp = (a_variable_ptr)alloc_cil(sizeof(a_variable));
    at_file_scope = (curr_il_region_number == FILE_SCOPE_REGION_NUMBER);
  }  /* if */
#if DEBUG
  num_variables_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(&(vp->source_corresp));
  set_il_walk_flag_to_initial_value(vp, at_file_scope);
  vp->next                        = NULL;
  vp->type                        = NULL;
  vp->assoc_param_type            = NULL;
  vp->storage_class               = storage_class;
  vp->address_taken               = FALSE;
  vp->is_parameter                = FALSE;
  vp->init_kind                   = (an_init_kind)initk_none;
#ifdef CIL
  vp->referenced_non_locally      = FALSE;
#endif /* ifdef CIL */
#ifdef FIL
  vp->by_address                  = FALSE;
  vp->base_var                    = NULL;
  vp->association_offset          = 0;
  vp->function_result_var_function= NULL;
#endif /* ifdef FIL */

  db_exit();
  return vp;
}  /* alloc_variable */


void add_to_variables_list(a_variable_ptr var_ptr,
                           a_boolean      at_file_scope)
/*
Add the given variable to the variables list for the scope at the indicated
scope depth.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  /* Get pointer to current or file scope entry. */
  if (at_file_scope) {
    ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
    sp = ssep->il_scope;
#if CHECKING
    if (sp == NULL) internal_error("add_to_variables_list: NULL IL scope");
    if (var_ptr->storage_class != (a_storage_class)sc_static &&
        var_ptr->storage_class != (a_storage_class)sc_extern &&
        var_ptr->storage_class != (a_storage_class)sc_unspecified) {
      internal_error(
              "add_to_variables_list: bad storage class for file scope list");
    }  /* if */
#endif /* CHECKING */
  } else {
    ssep = &scope_stack[decl_scope_level];
    /* Create the IL scope if necessary (for block scopes). */
    sp = ensure_il_scope_exists(ssep);
  }  /* if */
  /* Variables requiring static allocation go on one list, those for stack
     and register allocation on another. */
  if (at_file_scope ||
      var_ptr->storage_class == (a_storage_class)sc_static ||
      var_ptr->storage_class == (a_storage_class)sc_extern ||
      var_ptr->storage_class == (a_storage_class)sc_unspecified) {
#if CHECKING
    /* Variables with static storage will always be allocated in file scope
       memory region, regardless of which scope's list they are on. */
    if (!in_file_scope(var_ptr)) {
      internal_error("add_to_variables_list: var not in file scope region");
    }  /* if */
#endif /* CHECKING */
    if (sp->variables == NULL) {
      sp->variables = var_ptr;
    } else {
      ssep->last_variable->next = var_ptr;
    }  /* if */
    ssep->last_variable = var_ptr;
  } else {
#if CHECKING
    /* Variables with nonstatic storage will never be allocated in file scope
       memory region. */
    if (in_file_scope(var_ptr)) {
      internal_error("add_to_variables_list: var in file scope region");
    }  /* if */
#endif /* CHECKING */
    if (sp->nonstatic_variables == NULL) {
      sp->nonstatic_variables = var_ptr;
    } else {
      ssep->last_nonstatic_variable->next = var_ptr;
    }  /* if */
    ssep->last_nonstatic_variable = var_ptr;
  }  /* if */
  var_ptr->next = NULL;
}  /* add_to_variables_list */


void add_to_parameters_list(a_variable_ptr param_ptr)
/*
Add the given parameter to the parameters list for the current scope.
*/
{
  a_scope_stack_entry_ptr
		 ssep;
  a_scope_ptr    sp;

  /* Get pointer to current scope entry. */
  ssep = &scope_stack[decl_scope_level];
  sp = ssep->il_scope;
#if CHECKING
  if (sp == NULL) internal_error("add_to_parameters_list: NULL IL scope");
#endif /* CHECKING */
  if (sp->variant.routine.parameters == NULL) {
    sp->variant.routine.parameters = param_ptr;
  } else {
    ssep->last_parameter->next = param_ptr;
  }  /* if */
  ssep->last_parameter = param_ptr;
  param_ptr->next = NULL;
}  /* add_to_parameters_list */


a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type)
/*
Make a temporary variable whose type is temp_type.  Return a pointer to it.
*/
{
  a_variable_ptr   temp_var;
  a_boolean        at_file_scope;
  a_storage_class  storage_class;

  /* Typically, a temporary variable will be have automatic storage class,
     since it will appear in an expression in a function or block scope.
     However, if the temp is involved in an expression at file scope (within
     a class scope, for instance, or a default argument expression), it
     should be static and be allocated in file scope memory region. */
  if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
    storage_class = (a_storage_class)sc_auto;
    at_file_scope = FALSE;
  } else {
    storage_class = (a_storage_class)sc_static;
    at_file_scope = TRUE;
  }  /* if */
  /* alloc_variable uses the appropriate memory region, based on storage
     class.*/
  temp_var = alloc_variable(storage_class);
  temp_var->type = temp_type;
  /* Name linkage stays nlk_none. */
  add_to_variables_list(temp_var, at_file_scope);
  return temp_var;
}  /* alloc_temporary_variable */


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
  set_default_source_corresp(&(fp->source_corresp));
  /* Field entries are always in the file scope, so use the il_walk_flag
     value for the file scope memory region. */
  fp->source_corresp.il_walk_flag = curr_fs_initial_il_walk_flag_setting;
  fp->next       = NULL;
  fp->type       = NULL;
  fp->bit_offset = 0;
  fp->bit_size   = 0;

  db_exit();
  return fp;
}  /* alloc_field */


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
  set_default_source_corresp(&(rp->source_corresp));
  /* Routine entries are always in the file scope, so use the il_walk_flag
     value for the file scope memory region. */
  rp->source_corresp.il_walk_flag = curr_fs_initial_il_walk_flag_setting;
  rp->next                    = NULL;
  rp->type                    = NULL;
  rp->assoc_scope             = NULL_region_number;
  rp->storage_class           = (a_storage_class)sc_unspecified;
  rp->special_kind            = (a_special_function_kind)sfk_none;
  rp->opname_kind             = (an_opname_kind)onk_none;
  rp->is_virtual              = FALSE;
  rp->pure_virtual            = FALSE;
  rp->is_inline               = FALSE;
  rp->compiler_generated      = FALSE;
#if ASSIGNMENT_TO_THIS_ALLOWED
  rp->assignment_to_this_done = FALSE;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  rp->befriending_classes     = NULL;
  rp->virtual_function_number = 0;
#ifdef FIL
  rp->is_fortran_entry        = FALSE;
  rp->local_routine_scope     = NULL;
  rp->intrinsic_func_code     = (an_intrinsic_function_code)ifc_none;
#endif /* ifdef FIL */

  db_exit();
  return rp;
}  /* alloc_routine */


void remove_from_routines_list(a_routine_ptr rout_ptr)
/*
Unlink the given routine from the routines list for the file scope.  This
is done so the routine can be added again at the end of the list, to keep
the routines in order of appearance of their definitions (bodies).
*/
{
  a_routine_ptr  prev_routine, routine;
  a_scope_stack_entry_ptr
		 ssep;
  a_scope_ptr    sp;

  /* Get pointer to the file scope entry. */
  ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
  sp = ssep->il_scope;
#if CHECKING
  if (sp == NULL) internal_error("remove_from_routines_list: NULL IL scope");
#endif /* CHECKING */
  /* Find the routine on the current list in order to find the previous entry
     so we can unlink. */
  for (prev_routine = NULL, routine = sp->routines;
       routine != NULL;
       prev_routine = routine, routine = routine->next) {
    if (routine == rout_ptr) goto found_routine;
  }  /* for */
#if CHECKING
  internal_error("remove_from_routines_list: routine not found on list");
#endif /* CHECKING */
found_routine:
  /* Link the previous entry to the entry following this one. */
  if (prev_routine == NULL) {
    sp->routines = rout_ptr->next;
  } else {
    prev_routine->next = rout_ptr->next;
  }  /* if */
  /* If the entry being removed was the last on the list, update the
     last_routine pointer. */
  if (rout_ptr == ssep->last_routine) {
    ssep->last_routine = prev_routine;
  }  /* if */
}  /* remove_from_routines_list */


void add_to_routines_list(a_routine_ptr rout_ptr,
                           a_boolean    at_file_scope)
/*
Add the given routine to the routines list for the current scope, or
for the file scope if at_file_scope is TRUE.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  /* Get pointer to current or file scope entry. */
  ssep = &scope_stack[at_file_scope ? DEPTH_OF_FILE_SCOPE : decl_scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  if (sp->routines == NULL) {
    sp->routines = rout_ptr;
  } else {
    ssep->last_routine->next = rout_ptr;
  }  /* if */
  ssep->last_routine = rout_ptr;
  rout_ptr->next = NULL;
}  /* add_to_routines_list */


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
  set_default_source_corresp(&(ap->source_corresp));
  ap->next = NULL;
  ap->asm_string = NULL;

  db_exit();
  return ap;
}  /* alloc_asm_entry */


void add_to_asm_entries_list(an_asm_entry_ptr  asm_entry_ptr)
/*
Add the given routine to the asm entries list for the current scope.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  ssep = &scope_stack[decl_scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  if (sp->asm_entries == NULL) {
    sp->asm_entries = asm_entry_ptr;
  } else {
    ssep->last_asm_entry->next = asm_entry_ptr;
  }  /* if */
  ssep->last_asm_entry = asm_entry_ptr;
  asm_entry_ptr->next = NULL;
}  /* add_to_asm_entries_list */


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
  set_default_source_corresp(&(lp->source_corresp));
  /* il_walk_flag is set correctly by set_default_source_corresp. */
  lp->next = NULL;
  lp->variant.exec_stmt = NULL;
  lp->parent_block = NULL;
#ifdef FIL
  lp->kind = (a_label_kind)lk_executable;
  lp->used_in_assign = FALSE;
#endif /* ifdef FIL */

  db_exit();
  return lp;
}  /* alloc_label */


void add_to_labels_list(a_label_ptr label_ptr)
/*
Add the given label to the labels list for the function (not current) scope.
*/
{
  a_scope_stack_entry_ptr
		 ssep;
  a_scope_ptr    sp;

  /* Get pointer to the current function scope entry. */
#if DEBUG
  if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    internal_error("add_to_labels_list: not inside function");
  }  /* if */
#endif /* DEBUG */
  ssep = &scope_stack[depth_innermost_function_scope];
  sp = ssep->il_scope;
#if CHECKING
  if (sp == NULL) internal_error("add_to_labels_list: NULL IL scope");
#endif /* CHECKING */
  if (sp->labels == NULL) {
    sp->labels = label_ptr;
  } else {
    ssep->last_label->next = label_ptr;
  }  /* if */
  ssep->last_label = label_ptr;
  label_ptr->next = NULL;
}  /* add_to_labels_list */


void set_expr_node_kind(an_expr_node_ptr  node,
                        an_expr_node_kind kind)
/*
Set the kind of the indicated expression node.  Also set associated variant
fields to default values.
*/
{
  node->kind = kind;
  switch (kind) {
    case enk_error:
      /* No variant fields. */
      break;
    case enk_operation:
      node->variant.operation.kind = (an_expr_operator_kind)eok_last;
      node->variant.operation.assignment_returns_lvalue = FALSE;
      node->variant.operation.new_or_delete_call_for_array = FALSE;
      node->variant.operation.compiler_generated = FALSE;
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
    case enk_new_init:
      node->variant.init.dynamic_init = NULL;
      node->variant.init.expr         = NULL;
      break;
#if CHECKING
    default:
      internal_error("set_expr_node_kind: bad kind");
#endif /* CHECKING */
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
#ifdef FIL
  node->allow_reordering = FALSE;
#endif /* ifdef FIL */
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


void set_node_operator(an_expr_node_ptr      node,
                       an_expr_operator_kind kind,
	   	       a_type_ptr            type,
		       an_expr_node_ptr      operands)
/*
Set the operator, type, and operand list in an operator expression node.
*/
{
  node->type = type;
  node->variant.operation.kind = kind;
  node->variant.operation.assignment_returns_lvalue = FALSE;
  node->variant.operation.new_or_delete_call_for_array = FALSE;
  node->variant.operation.operands = operands;
}  /* set_node_operator */


an_expr_node_ptr make_operator_node(an_expr_operator_kind kind,
			   	    a_type_ptr            type,
			   	    an_expr_node_ptr      operands)
/*
Allocate and initialize an operator node, give it a type and kind, and link
an operands list to it.
*/
{
  register an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_operation);
  set_node_operator(node, kind, type, operands);

  return node;
}  /* make_operator_node */


an_expr_node_ptr error_node(void)
/*
Make and return an expression node indicating an error.
*/
{
  register an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_error);
  node->type = error_type();

  return node;
}  /* error_node */


an_expr_node_ptr alloc_node_for_constant(a_constant *constant)
/*
Allocate an expression node for a constant.  Usually, this is a node
of kind enk_constant, but it can be of kind enk_variable_address or
enk_routine_address if the constant was the address of a variable or the
address of a routine.  If a normal enk_constant node is built, the
constant is allocated (as a shareable constant).
*/
{
  register an_expr_node_ptr node;
  an_address_base_kind      abkind;

  db_enter(5, "alloc_node_for_constant");

  /* If the implicit_cast flag is set, the constant is implicitly cast to
     another type, and the straight enk_variable_address or enk_routine_address
     cannot represent that case.  Ditto if the offset is non-zero. */
  if (constant->kind == (a_constant_repr_kind)ck_address &&
      !constant->implicit_cast && constant->variant.address.offset == 0) {
    abkind = constant->variant.address.kind;
    if (abkind == (an_address_base_kind)abk_routine) {
      /* The constant is the address of a function; use an address-of-function
         expression node. */
      node = alloc_expr_node((an_expr_node_kind)enk_routine_address);
      node->variant.routine = constant->variant.address.variant.routine;
      goto have_node;
    } else if (abkind == (an_address_base_kind)abk_variable) {
      /* The constant is the address of a variable; use an address-of-variable
         expression node. */
      node = alloc_expr_node((an_expr_node_kind)enk_variable_address);
      node->variant.variable = constant->variant.address.variant.variable;
      goto have_node;
    }  /* if */
  }  /* if */
  /* Normal case: allocate a shareable constant and make an expression node
     that points to it. */    
  node = alloc_expr_node((an_expr_node_kind)enk_constant);
  node->variant.constant = alloc_shareable_constant(constant);
have_node:
  node->type = constant->type;

  db_exit();
  return node;
}  /* alloc_node_for_constant */


an_expr_node_ptr node_for_integer_constant(long            value,
                                           an_integer_kind kind)
/*
Make a node for an integer constant with value "value" and kind "kind",
and return a pointer to it.
*/
{
  an_expr_node_ptr node;
  a_constant       constant;

  set_integer_constant(&constant, value, kind);
  node = alloc_node_for_constant(&constant);

  return node;
}  /* node_for_integer_constant */


an_expr_node_ptr copy_node(an_expr_node_ptr expr)
/*
Make a copy of an expression node and return a pointer to it.
*/
{
  an_expr_node_ptr expr_copy;

  expr_copy = alloc_expr_node(expr->kind);
  *expr_copy = *expr;
  expr_copy->next = NULL;
  return expr_copy;
}  /* copy_node */


static an_expr_node_ptr internal_copy_list_of_expr_trees(
                                                    an_expr_node_ptr expr_list)
/*
Make a copy of a list of expression trees and return a pointer to it.
This routine does the real work for copy_list_of_expr_trees and should not be
called directly.
*/
{
  an_expr_node_ptr expr, expr_copy, prev_expr_copy, expr_list_copy;

  expr_list_copy = prev_expr_copy = NULL;
  for (expr = expr_list; expr != NULL; expr = expr->next) {
    expr_copy = internal_copy_expr_tree(expr);
    if (expr_list_copy == NULL) {
      expr_list_copy = expr_copy;
    } else {
      prev_expr_copy->next = expr_copy;
    }  /* if */
    prev_expr_copy = expr_copy;
  }  /* for */
  return expr_list_copy;
}  /* internal_copy_list_of_expr_trees */


static an_expr_node_ptr internal_copy_expr_tree(an_expr_node_ptr expr)
/*
Make a copy of an expression tree and return a pointer to it.
This routine does the real work for copy_expr_tree and should not be
called directly.
*/
{
  an_expr_node_ptr expr_copy;

  expr_copy = copy_node(expr);
  if (expr->kind == (an_expr_node_kind)enk_operation) {
    /* Copy the operands of the operation. */
    expr_copy->variant.operation.operands =
            internal_copy_list_of_expr_trees(expr->variant.operation.operands);
  } else if (expr->kind == (an_expr_node_kind)enk_variable ||
             expr->kind == (an_expr_node_kind)enk_variable_address) {
    /* Rewrite references to temporary variables. */
    expr_copy->variant.variable = rewrite_if_temporary(expr->variant.variable);
  } else if (expr->kind == (an_expr_node_kind)enk_temp_init ||
             expr->kind == (an_expr_node_kind)enk_new_init) {
    /* Copy the subtree and dynamic init for a dynamic initialization. */
    expr_copy->variant.init.expr =
                              internal_copy_expr_tree(expr->variant.init.expr);
    expr_copy->variant.init.dynamic_init =
                            copy_dynamic_init(expr->variant.init.dynamic_init);
  }  /* if */
  return expr_copy;
}  /* internal_copy_expr_tree */


an_expr_node_ptr copy_list_of_expr_trees(an_expr_node_ptr expr_list)
/*
Make a copy of a list of expression trees and return a pointer to it.
Temporaries referenced in the expressions are not changed.
*/
{
  an_expr_node_ptr expr_list_copy;

  /* Set some global variables that affect the copying. */
  rewritten_temporaries = NULL;
  clone_temps_on_expr_copy = FALSE;
  /* Do the copying. */
  expr_list_copy = internal_copy_list_of_expr_trees(expr_list);
  /* Clean up. */
  free_rewritten_temporaries();
  return expr_list_copy;
}  /* copy_list_of_expr_trees */


an_expr_node_ptr copy_expr_tree(an_expr_node_ptr expr,
                                a_boolean        clone_temps)
/*
Make a copy of an expression tree and return a pointer to it.  If clone_temps
is TRUE, replace all temporaries referenced by the expression by new
equivalent temporaries; that is needed when making copies of an expression
that might be executed at the same time as the original expression, e.g.,
for default argument expressions.  clone_temps cannot be TRUE when this routine
is called from IL lowering, because the wrong temp-allocation routine would be
called.
*/
{
  an_expr_node_ptr expr_copy;

  /* Set some global variables that affect the copying. */
  rewritten_temporaries = NULL;
  clone_temps_on_expr_copy = clone_temps;
  /* Do the copying. */
  expr_copy = internal_copy_expr_tree(expr);
  /* Clean up. */
  free_rewritten_temporaries();
  return expr_copy;
}  /* copy_expr_tree */


an_expr_node_ptr copy_default_arg_expr_list(a_param_type_ptr ptp)
/*
Make an expression list containing copies of the default argument expressions
for the parameter indicated by ptp and all parameters following that.
If ptp is non-NULL, it must point to a parameter with a default argument
expression.  Replace all temporaries referenced by the expression by new
equivalent temporaries.  This routine may not be called from IL lowering,
because the wrong temp-allocation routine would be called.
*/
{
  an_expr_node_ptr first_node = NULL, last_node = NULL, arg_node;

  if (ptp != NULL) {
#if CHECKING
    if (ptp->default_arg_expr == NULL) {
      internal_error("copy_default_arg_expr_list: param has no default arg");
    }  /* if */
#endif /* CHECKING */
    /* Copy the default argument expressions. */
    do {
      /* Watch out for cases where a default argument is followed by
         a non-default argument.  An error will have been issued at
         the point of declaration of the function, but the problem
         could not be corrected there because the function type may
         have come from a typedef (i.e., it might be shared). */
      if (ptp->default_arg_expr == NULL) {
        arg_node = error_node();
      } else {
        arg_node = copy_expr_tree(ptp->default_arg_expr, /*clone_temps=*/TRUE);
      }  /* if */
      if (first_node == NULL) {
        first_node = arg_node;
      } else {
        last_node->next = arg_node;
      }  /* if */
      last_node = arg_node;
    } while ((ptp = ptp->next) != NULL);
  }  /* if */
  return first_node;
}  /* copy_default_arg_expr_list */


an_expr_node_ptr var_lvalue_expr(a_variable_ptr var)
/*
Build an expression node that represents the lvalue address of var and
return a pointer to it.
*/
{
  an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_variable_address);
  node->type = make_pointer_type(var->type);
  node->variant.variable = var;
  return node;
}  /* var_lvalue_expr */


an_expr_node_ptr var_rvalue_expr(a_variable_ptr var)
/*
Build an expression node that represents the rvalue value of var and
return a pointer to it.
*/
{
  an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_variable);
  /* Drop any type qualifiers on the variable type because rvalues do not have
     type qualifiers. */
  node->type = make_unqualified_type(var->type);
  node->variant.variable = var;
  return node;
}  /* var_rvalue_expr */


an_expr_node_ptr function_addr_expr(a_routine_ptr rout)
/*
Build an expression node that represents the address of the function rout
and return a pointer to it.
*/
{
  an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_routine_address);
  node->type = make_pointer_type(rout->type);
  node->variant.routine = rout;
  return node;
}  /* function_addr_expr */


an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node)
/*
Add an indirection on top of the given node (or make a change that produces
the same effect), and return a pointer to the new expression.
*/
{
  an_expr_operator_kind op;
  a_type_ptr            new_type;
  a_boolean             optimized_case;
  an_expr_node_ptr      op1, op2, op3;

  if (is_variable_address_node(node)) {
    /* Address of variable becomes value of variable. */
    node->kind = (an_expr_node_kind)enk_variable;
    node->type = node->variant.variable->type;
  } else if (is_error_node(node)) {
    /* Error node -- leave alone. */
  } else {
    optimized_case = FALSE;
    new_type = type_pointed_to(node->type);
    if (is_operation_node(node)) {
      /* An operator node. */
      op = node->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_padd ||
          op == (an_expr_operator_kind)eok_padd_subsc) {
        /* A pointer addition; change to a subscripting operation. */
        optimized_case = TRUE;
        node->variant.operation.kind = (an_expr_operator_kind)eok_subscript;
        node->type = new_type;
      } else if (op == (an_expr_operator_kind)eok_bit_field) {
        /* The value is the "address" of a bit-field.  Therefore, the
           indirect version is an extract of the bit-field. */
        optimized_case = TRUE;
        node->variant.operation.kind =
                                  (an_expr_operator_kind)eok_extract_bit_field;
        node->type = new_type;
      } else if (op == (an_expr_operator_kind)eok_question) {
        /* "?" operator.  Add an indirection to each branch.  This is
           particularly useful for a case like
             &(i ? j : k)
           (only valid in C++). */
        optimized_case = TRUE;
        op1 = node->variant.operation.operands;
        op2 = op1->next;
        op3 = op2->next;
        op1->next = op2 = add_indirection_to_node(op2);
        op2->next = add_indirection_to_node(op3);
        node->type = new_type;
      } else if (node->variant.operation.assignment_returns_lvalue) {
        /* The operation is an assignment that returns an lvalue.
           Change it to one that returns an rvalue. */
        optimized_case = TRUE;
        node->variant.operation.assignment_returns_lvalue = FALSE;
        node->type = new_type;
      }  /* if */
    }  /* if */
    if (!optimized_case) {
      /* For other cases, add an indirection operator. */
      node = make_operator_node((an_expr_operator_kind)eok_indirect,
                                new_type, node);
    }  /* if */
  }  /* if */
  return node;
}  /* add_indirection_to_node */


an_expr_node_ptr this_param_value_expr(void)
/*
Return an expression for the value of the "this" parameter of the current
function (which must have such a parameter).  This routine is used only
in C++ mode.
*/
{
  an_expr_node_ptr expr;
  a_variable_ptr   this_param_var;
  a_scope_ptr      curr_scope;

#if CHECKING
  if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    internal_error("this_param_value_expr: not inside function");
  }  /* if */
#endif /* CHECKING */
  curr_scope = scope_stack[depth_innermost_function_scope].il_scope;
  this_param_var = curr_scope->variant.routine.this_param_variable;
#if CHECKING
  if (this_param_var == NULL) {
    internal_error("this_param_value_expr: no this param");
  }  /* if */
#endif /* CHECKING */
  expr = alloc_expr_node((an_expr_node_kind)enk_variable);
  expr->type = this_param_var->type;
  expr->variant.variable = this_param_var;
  return expr;
}  /* this_param_value_expr */


an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                             a_field_ptr      field)
/*
Make an expression for an lvalue reference to field "field" of "node" and
return a pointer to it.
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      field_node;
  a_type_ptr            selection_type;

  /* Make the expression node for the field. */
  field_node = alloc_expr_node((an_expr_node_kind)enk_field);
  field_node->type = field->type;
  field_node->variant.field = field;
  node->next = field_node;
  /* Use a different operator for bit field references. */
  op = (field->bit_size != 0) ? (an_expr_operator_kind)eok_bit_field :
                                (an_expr_operator_kind)eok_field;
  /* The selected field has all the type qualifiers of both the field
     and the selecting pointer. */
  selection_type = type_plus_qualifiers_from_second_type(field->type,
                                                  type_pointed_to(node->type));
  selection_type = make_pointer_type(selection_type);
  /* Make the field selection node. */
  node = make_operator_node(op, selection_type, node);
  return node;
}  /* field_lvalue_selection_expr */


an_expr_node_ptr field_rvalue_selection_expr(an_expr_node_ptr node,
                                             a_field_ptr      field)
/*
Make an expression for an rvalue reference to field "field" of "node" and
return a pointer to it.
*/
{
  /* Make the expression node for an lvalue reference. */
  node = field_lvalue_selection_expr(node, field);
  /* Add an indirection to turn the lvalue into an rvalue. */
  node = add_indirection_to_node(node);
  /* Drop any type qualifiers on the type because rvalues do not have
     type qualifiers. */
  node->type = make_unqualified_type(node->type);
  return node;
}  /* field_rvalue_selection_expr */


an_expr_node_ptr base_class_selection_expr(an_expr_node_ptr node,
                                           a_base_class_ptr bcp)
/*
Create an expression node that selects the base class indicated from
the object pointed to by node, and return a pointer to it.  The base
class need not be an immediate base class.
*/
{
  a_derivation_step_ptr dsp;

  /* Add a base class cast for each step in the derivation. */
  for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
    node = make_operator_node((an_expr_operator_kind)eok_base_class_cast,
                              make_pointer_type(dsp->base_class->type), node);
  }  /* for */
  return node;
}  /* base_class_selection_expr */


a_variable_ptr create_expr_temporary(a_type_ptr       temp_type,
                                     a_boolean        force_temp_init,
                                     an_expr_node_ptr *temp_init_node)
/*
Allocate a temporary variable of type temp_type and return a pointer to it.
If force_temp_init is TRUE or temp_type is a type that requires a destructor,
also allocate an enk_temp_init node pointing to a dynamic init entry
and return a pointer to the enk_temp_init node in *temp_init_node; otherwise,
set *temp_init_node to NULL.
*/
{
  a_variable_ptr     temp_var;
  a_dynamic_init_ptr dip;

  *temp_init_node = NULL;
  /* Allocate the temporary. */
  temp_var = alloc_temporary_variable(temp_type);
  if (force_temp_init ||
      (C_dialect == C_dialect_cplusplus &&
       is_class_struct_union_type(temp_type) &&
       symbol_supplement_for_class(temp_type)->destructor != NULL)) {
    /* force_temp_init is TRUE, or the temp_type is a class with a
       destructor. */
    dip = alloc_dtor_dynamic_init((a_dynamic_init_kind)dik_none, temp_type);
#if 0
    add_to_dynamic_inits_list(dip);
#endif
    dip->variable = temp_var;
    /* Make an enk_temp_init node that points at the dynamic init
       entry. */
    (*temp_init_node) = alloc_expr_node((an_expr_node_kind)enk_temp_init);
    (*temp_init_node)->variant.init.dynamic_init = dip;
    /* Note that the type and expression are not set yet, as we do not
       know what expression will be attached to the enk_temp_init. */
  }  /* if */
  return temp_var;
}  /* create_expr_temporary */


void attach_expr_under_temp_init(an_expr_node_ptr *node,
                                 an_expr_node_ptr temp_init_node)
/*
temp_init_node points to an enk_temp_init.  Attach the expression pointed to
by *node under the enk_temp_init node, and update *node to point to the
enk_temp_node.
*/
{
  temp_init_node->variant.init.expr = *node;
  temp_init_node->type = (*node)->type;
  *node = temp_init_node;
}  /* attach_expr_under_temp_init */


an_expr_node_ptr func_call_expr(an_expr_node_ptr  function_node,
                                a_type_ptr        function_type,
                                a_boolean         is_virtual,
                                a_boolean         new_or_delete_call_for_array,
                                a_source_position *err_pos)
/*
Make an expression for a call of the function indicated by function_node,
whose type is function_type, and which is virtual if is_virtual is TRUE or
a pointer-to-member-function call if the type of function_node is
pointer-to-member-function.  new_or_delete_call_for_array is TRUE
if the call is of a C++ new or delete routine to allocate or free an
array.  The arguments of the call are already attached to function_node.
A skip_typerefs need not have been done on function_type.  Return
a pointer to the call node.  *err_pos gives an error position for the
case where the function return type is invalid (i.e., incomplete);
an error node is returned for that case.
*/
{
  an_expr_operator_kind         op;
  an_expr_node_ptr              call_node;
  a_type_ptr                    return_type, call_type;
  a_routine_type_supplement_ptr rtsp;
  a_variable_ptr                temp_var = NULL;
  an_expr_node_ptr              temp_init_node = NULL, temp_node, prev_node;

  function_type = skip_typerefs(function_type);
  /* Any type qualifiers on the return type are dropped because rvalues
     do not have qualified types. */
  call_type = return_type =
                     skip_typerefs(function_type->variant.routine.return_type);
  /* The function return type must be void or object type and not array
     type.  Half of this check is in add_to_derived_type_list.
     The check here is necessary because it is valid to declare a
     function returning a class/struct/enum type that is incomplete
     at the point of declaration of the function so long as it is completed
     by the time the function is defined or called (if it is). */
#if CHECKING
  if (is_array_type(return_type) || is_function_type(return_type)) {
    internal_error("func_call_expr: function returns array or function");
  }  /* if */
#endif /* CHECKING */
  /* Return type may not be incomplete (but void is okay). */
  if (is_incomplete_type(return_type) && !is_void_type(return_type)) {
    pos_error(ec_incomplete_return_type_not_allowed, err_pos);
    call_node = error_node();
  } else {
    rtsp = function_type->variant.routine.extra_info;
    /* If the function is one for which the caller must supply a place for
       the result, allocate a temporary for that and insert it into the
       argument list. */
    set_routine_calling_method_flag(function_type);
    if (rtsp->caller_provides_place_to_put_return_value) {
      /* Allocate the temporary for the return value. */
      temp_var = create_expr_temporary(return_type, /*force_temp_init=*/FALSE,
                                       &temp_init_node);
      /* Make an expression for the address of the temporary. */
      temp_node = var_lvalue_expr(temp_var);
      /* Put the expression into the argument list.  If there is a "this"
         parameter, the temporary is added after it. */
      prev_node = function_node;
      if (rtsp->implicit_this_param_type != NULL) prev_node = prev_node->next;
      temp_node->next = prev_node->next;
      prev_node->next = temp_node;
      /* The return type of the call is a pointer to the temporary. */
      call_type = make_pointer_type(return_type);
    } else if (is_reference_type(call_type)) {
      /* If the function returns a reference type, make the result a
         pointer. */
      call_type = return_type = make_pointer_type(type_pointed_to(call_type));
    }  /* if */
    if (is_ptr_to_member_type(function_node->type)) {
      /* Call using a pointer-to-member-function. */
      op = (an_expr_operator_kind)eok_pm_call;
    } else if (is_virtual) {
      /* Call of a virtual function. */
      op = (an_expr_operator_kind)eok_virtual_call;
    } else {
      op = (an_expr_operator_kind)eok_call;
    }  /* if */
    /* Make an expression for the function call. */
    call_node = make_operator_node(op, call_type, function_node);
    if (new_or_delete_call_for_array) {
      /* Remember that this is a new or delete call for an array. */
      call_node->variant.operation.new_or_delete_call_for_array = TRUE;
    }  /* if */
    /* If a temporary was allocated to hold the returned value, add
       a comma expression to pick up the temporary value, like
         (f(&T, a1, a2), T)
       Note that find_class_rvalue_var_node looks for the form
       of the expressions generated here to do an optimization.
    */
    if (temp_var != NULL) {
      temp_node = var_rvalue_expr(temp_var);
      call_node->next = temp_node;
      call_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                     return_type, call_node);
      /* If the object involved requires a destructor, an enk_temp_init
         node was created above.  Attach it above the function call to
         request the destructor invocation. */
      if (temp_init_node != NULL) {
        attach_expr_under_temp_init(&call_node, temp_init_node);
      }  /* if */
    }  /* if */
  }  /* if */
  return call_node;
}  /* func_call_expr */


a_statement_ptr make_assignment_statement(an_expr_node_ptr dest,
                                          an_expr_node_ptr source)
/*
Create an expression statement pointing to an assignment operator that
assigns the rvalue "source" to the lvalue "dest".  Return a pointer to
the statement.  May not be used for array types.
*/
{
  a_statement_ptr  stmt = alloc_statement((a_statement_kind)stmk_expr);
  an_expr_node_ptr node;

  /* Make the assignment node. */
  node = make_operator_node(which_binary_operator(tok_assign, source->type),
                            source->type, dest);
  dest->next = source;
  /* Put the assignment node under the statement. */
  stmt->expr = node;
  return stmt;
}  /* make_assignment_statement */


a_statement_ptr make_array_assignment_statement(an_expr_node_ptr dest,
                                                an_expr_node_ptr source)
/*
Create an expression statement pointing to an assignment operator that
assigns the array lvalue "source" to the lvalue "dest".  Return a pointer to
the statement.  This doesn't come up directly in programs, but does
in IL lowering and in generated routines (like assignment operator functions).
*/
{
  a_statement_ptr  stmt = alloc_statement((a_statement_kind)stmk_expr);
  an_expr_node_ptr node;

  /* Make the assignment node. */
  node = make_operator_node((an_expr_operator_kind)eok_bassign,
                            type_pointed_to(dest->type), dest);
  dest->next = source;
  /* Put the assignment node under the statement. */
  stmt->expr = node;
  return stmt;
}  /* make_array_assignment_statement */


a_statement_ptr make_call_assignment_statement(a_routine_ptr     rout,
                                               an_expr_node_ptr  dest,
                                               an_expr_node_ptr  source,
                                               a_source_position *err_pos)
/*
Create an expression statement pointing to a call operator that
calls "rout" to assign the lvalue "source" to the lvalue "dest".  Return
a pointer to the statement.  *err_pos is a source position to be used
for errors (e.g., the function has an invalid return type).
*/
{
  a_statement_ptr  stmt = alloc_statement((a_statement_kind)stmk_expr);
  an_expr_node_ptr node, func_addr_node;

  /* Make a node for the address of the function. */
  func_addr_node = function_addr_expr(rout);
  /* Link the operands to the function address node. */
  func_addr_node->next = dest;
  dest->next = source;
  /* Make the call node. */
  node = func_call_expr(func_addr_node, rout->type,
                        (a_boolean)rout->is_virtual,
                        /*new_or_delete_call_for_array=*/FALSE,
                        err_pos);
  /* Put the call node under the statement. */
  stmt->expr = node;
  return stmt;
}  /* make_call_assignment_statement */


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
  scp->next             = NULL;
  scp->constant_list    = NULL;
  scp->statements       = NULL;
  scp->break_seq_number = 0;
  return scp;
}  /* alloc_switch_clause */


void set_statement_kind(a_statement_ptr  sp,
                        a_statement_kind stmt_kind)
/*
Set the kind of the statement sp to stmt_kind, and set the associated variant
fields to default values.
*/
{
  a_block_ptr bp;

  sp->kind = stmt_kind;
  sp->expr = NULL;
  switch (stmt_kind) {
    case stmk_expr:
    case stmk_return:
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
    case stmk_switch:
      sp->variant.switch_stmt.clause_list    = NULL;
      sp->variant.switch_stmt.body_statement = NULL;
      break;
    case stmk_goto:
    case stmk_label:
      sp->variant.label = NULL;
      break;
    case stmk_block:
      sp->variant.block.statements = NULL;
      sp->variant.block.extra_info = bp =
                        (a_block_ptr)alloc_cil(sizeof(a_block));
#if DEBUG
      num_blocks_allocated++;
#endif /* DEBUG */
      bp->final_seq_number = 0;
      bp->assoc_scope      = NULL;
      bp->parent_block     = NULL;
      bp->end_of_block_reachable = TRUE;
      break;
    case stmk_init:
      sp->variant.dynamic_init = NULL;
      break;
    case stmk_asm:
      sp->variant.asm_entry = NULL;
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
  sp->seq_number       = 0;
  sp->next             = NULL;
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
  sp->next                = NULL;
  sp->number              = number;
  sp->kind                = kind;
  switch (kind) {
    case sck_file:
    case sck_block:
      /* No variant fields. */
      break;
    case sck_func_prototype:
    case sck_class_struct_union:
      sp->variant.assoc_type = NULL;
      break;
    case sck_function:
      sp->variant.routine.ptr                 = assoc_routine;
      sp->variant.routine.parameters          = NULL;
      sp->variant.routine.constructor_inits   = NULL;
      sp->variant.routine.this_param_variable = NULL;
      sp->variant.routine.return_value_pointer_variable = NULL;
#ifdef FIL
      sp->variant.routine.function_result_var = NULL;
#endif /* ifdef FIL */
      break;
#if CHECKING
    default:
      internal_error("alloc_scope: bad scope kind");
#endif /* CHECKING */
  }  /* switch */
  sp->assoc_block         = NULL;
  sp->constants           = NULL;
  sp->types               = NULL;
  sp->variables           = NULL;
  sp->nonstatic_variables = NULL;
  sp->labels              = NULL;
  sp->routines            = NULL;
  sp->asm_entries         = NULL;
  sp->scopes              = NULL;
  sp->dynamic_inits       = NULL;
#ifdef FIL
  sp->entries             = NULL;
  sp->namelist_groups     = NULL;
#endif /* ifdef FIL */

  db_exit();
  return sp;
}  /* alloc_scope */


#if DEBUG
unsigned long show_il_space_used(void)
/*
Display and return the amount of space used for various IL tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  fprintf(f_debug, "\nIL table use:\n");
  fprintf(f_debug, "%25s %8s %8s %8s\n", "Table", "Number", "Each", "Total");

#define write_one(name, counter, type)                                \
{ num = counter; size = sizeof(type); total = num*size;               \
  fprintf(f_debug, "%25s %8lu %8lu %8lu\n", name, num, size, total);  \
  grand_total += total;                                               \
}  /* write_one */

  write_one("constant", num_constants_allocated, a_constant);
  write_one("String literal text", string_literal_text_space_allocated, char);
  write_one("param type", num_param_types_allocated, a_param_type);
  write_one("routine type supplement", num_routine_type_supplements_allocated,
                                       a_routine_type_supplement);
  write_one("based type list member", num_based_type_list_members_allocated,
            a_based_type_list_member);
  write_one("class type supplement", num_class_type_supplements_allocated,
                                     a_class_type_supplement);
  write_one("access adjustment", num_access_adjustments_allocated,
                                 an_access_adjustment);
  write_one("class list entry", num_class_list_entries_allocated,
                                a_class_list_entry);
  write_one("overriding virtual func",
                                num_overriding_virtual_functions_allocated,
                                an_overriding_virtual_function_ptr);
  write_one("derivation steps", num_derivation_steps_allocated,
                                a_derivation_step);
  write_one("base class", num_base_classes_allocated, a_base_class);
  write_one("type", num_types_allocated, a_type);
  write_one("dynamic init", num_dynamic_inits_allocated, a_dynamic_init);
  write_one("variable", num_variables_allocated, a_variable);
  write_one("field", num_fields_allocated, a_field);
  write_one("routine", num_routines_allocated, a_routine);
  write_one("asm entry", num_asm_entries_allocated, an_asm_entry);
  write_one("label", num_labels_allocated, a_label);
  write_one("expr node", num_expr_nodes_allocated, an_expr_node);
  write_one("switch clause", num_switch_clauses_allocated, a_switch_clause);
  write_one("block", num_blocks_allocated, a_block);
  write_one("statement", num_statements_allocated, a_statement);
  write_one("constructor init", num_constructor_inits_allocated,
                                a_constructor_init);
  write_one("scope", num_scopes_allocated, a_scope);
#if ALTERNATE_IL_FILE_FORMAT
  write_one("IL entry numbers", num_il_entry_numbers_allocated,
            an_il_entry_number);
#endif /* ALTERNATE_IL_FILE_FORMAT */

  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total", "", "", grand_total);

  (void)fputc('\n', f_debug);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "get_based_type calls", "", "",
                                          num_get_based_type_calls);  
  
  (void)fputc('\n', f_debug);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "num_shareable_constants", "", "",
                                          num_shareable_constants);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Percent of buckets used", "", "",
                   (100 * num_used_shareable_constant_buckets) /
                                               SIZE_SHAREABLE_CONSTANTS_TABLE);
  if (num_used_shareable_constant_buckets != 0) {
    fprintf(f_debug, "%25s %8s %8s %8.2f\n", "Avg non-empty bucket len","", "",
                     (double)num_shareable_constants /
                     (double)num_used_shareable_constant_buckets);
  }  /* if */
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "num func shareable consts", "", "",
                                          num_func_shareable_constants);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Number of searches", "", "",
                                         num_searches_for_shareable_constants);
  if (num_searches_for_shareable_constants != 0) {
    fprintf(f_debug, "%25s %8s %8s %8.2f\n", "Avg compares/search", "", "",
                     (double)num_compares_for_shareable_constants /
                     (double)num_searches_for_shareable_constants);
  }  /* if */

  return grand_total;
}  /* show_il_space_used */
#endif /* DEBUG */


void il_init(void)
/*
Initialize static variables related to the IL.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in il.h: */
  curr_il_region_number = NULL_region_number;
  curr_initial_il_walk_flag_setting = curr_fs_initial_il_walk_flag_setting =
                                                   0;  /* Arbitrary: 0 or 1. */
  /* Variable in il_def.h: */
#if CHECKING && DEBUG
  /* Check that the table of storage class names is correctly initialized.
     This guards against someone changing the enumeration and forgetting to
     update db_storage_class_names. */

  if (db_storage_class_names[(int)sc_last] == NULL ||
      strcmp(db_storage_class_names[(int)sc_last], "last") != 0) {
    internal_error(
                "il_init: incorrect initialization of db_storage_class_names");
  }  /* if */
  /* Check that the table of operator names is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to update
     db_operator_names. */
  if (db_operator_names[(int)eok_last] == NULL ||
      strcmp(db_operator_names[(int)eok_last], "last") != 0) {
    internal_error("il_init: incorrect initialization of db_operator_names");
  }  /* if */
#endif /* CHECKING && DEBUG */

  /* Static variables in il.c: */
  /* Depending on NULL represented as zero bits here. */
  memzero((char *)int_types, sizeof(int_types));
  memzero((char *)float_types, sizeof(float_types));
  memzero((char *)string_types, sizeof(string_types));
  memzero((char *)wide_string_types, sizeof(wide_string_types));
  il_signed_int_type = il_error_type = il_unknown_type = il_void_type = NULL;
  memzero((char *)shareable_constants_table,
          sizeof(shareable_constants_table));
#if DEBUG
  num_constants_allocated                = 0;
  num_param_types_allocated              = 0;
  num_routine_type_supplements_allocated = 0;
  num_based_type_list_members_allocated  = 0;
  num_access_adjustments_allocated       = 0;
  num_class_list_entries_allocated       = 0;
  num_class_type_supplements_allocated   = 0;
  num_derivation_steps_allocated         = 0;
  num_base_classes_allocated             = 0;
  num_types_allocated                    = 0;
  num_dynamic_inits_allocated            = 0;
  num_variables_allocated                = 0;
  num_fields_allocated                   = 0;
  num_routines_allocated                 = 0;
  num_asm_entries_allocated              = 0;
  num_labels_allocated                   = 0;
  num_expr_nodes_allocated               = 0;
  num_switch_clauses_allocated           = 0;
  num_blocks_allocated                   = 0;
  num_statements_allocated               = 0;
  num_constructor_inits_allocated        = 0;
  num_scopes_allocated                   = 0;
  string_literal_text_space_allocated    = 0;
  num_shareable_constants                = 0;
  num_func_shareable_constants           = 0;
  num_used_shareable_constant_buckets    = 0;
  num_searches_for_shareable_constants   = 0;
  num_compares_for_shareable_constants   = 0;
  num_get_based_type_calls               = 0;
#if ALTERNATE_IL_FILE_FORMAT
  num_il_entry_numbers_allocated         = 0;
#endif /* ALTERNATE_IL_FILE_FORMAT */
#endif /* DEBUG */
  avail_rewritten_temporaries = NULL;
}  /* il_init */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
