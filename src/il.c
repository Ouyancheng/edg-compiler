/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il.c -- Construction of intermediate language trees.

*/

#include "basics.h"
#include "host_envir.h"
#include "il.h"
#include "il_walk.h"
#include "debug.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "target.h"
#include "lang_feat.h"
#include "symbol_tbl.h"
#include "error.h"
#include "types.h"
#include "cmd_line.h"
#include "float_pt.h"
#include "const_ints.h"
#include "exprutil.h"
#include "folding.h"
#include "lexical.h"
#include "il_to_str.h"
#include "pch.h"

#if ALTERNATE_IL_FILE_FORMAT
#include "il_file.h"
#endif /* ALTERNATE_IL_FILE_FORMAT */

#if !STANDALONE_UTILITY_PROGRAM
#include "func_def.h"
#include "templates.h"

/*
Pointers to shared types.  These are cleared by il_init.
*/
static a_type_ptr int_types[(int)ik_last];
static a_type_ptr signed_int_types[(int)ik_last];
static a_type_ptr float_types[(int)fk_last];
#define MAX_TRACKED_STRING_TYPE_LENGTH 80
static a_type_ptr string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
static a_type_ptr wide_string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
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
		num_source_files_allocated,
		num_constants_allocated,
		num_param_types_allocated,
		num_routine_type_supplements_allocated,
		num_based_type_list_members_allocated,
		num_class_type_supplements_allocated,
		num_access_adjustments_allocated,
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
		num_accessible_base_classes_allocated,
		num_switch_clauses_allocated,
		num_handlers_allocated,
		num_blocks_allocated,
		num_for_loops_allocated,
		num_statements_allocated,
		num_constructor_inits_allocated,
		num_pragmas_allocated,
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

/*
Number of times the based_types lists of types are searched for related types.
*/
static unsigned long
		num_get_based_type_calls;
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* DEBUG */

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
static a_scope_orphaned_list_header_ptr
		last_scope_orphaned_list_header;
			/* End of list for
			   il_header.scope_orphaned_list_headers. */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

#if RECORD_MACROS_IN_IL
static a_macro_ptr
		last_macro;
			/* End of list of il_header.macros. */
#endif /* RECORD_MACROS_IN_IL */

/*
Data structure used to save information about the last source sequence
to file/line number conversion that was done so that subsequent
conversions can be done more quickly.
*/
typedef struct a_source_sequence_cache_entry {
  a_seq_number
		first_seq_number;
			/* First sequence number for which the cached
			   information is valid. */
  a_seq_number
		last_seq_number;
			/* Last sequence number for which the cached
			   information is valid. */
  long
		line_offset;
			/* Value to be subtracted from a sequence number to
			   convert it to a line number of the source file
			   associated with this cached information. */
  a_boolean	physical_line;
			/* TRUE if the cached information reflects a physical
			   line lookup. */
  unsigned long
		nesting_depth;
			/* Cached nesting depth. */
  a_source_file_ptr
		source_file;
			/* Cached source file pointer. */
} a_source_sequence_cache_entry;

static a_source_sequence_cache_entry seq_cache;

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

static a_template_arg_ptr
		avail_template_args;
			/* List of freed template arg entries that are
			   available for reuse. */


/* Static variable and macro for quickly initializing the source_corresp
   field of an IL entry to default values. */
static a_source_correspondence
		def_source_corresp;
#define set_default_source_corresp(sc) (sc) = def_source_corresp;


/* Forward declarations needed because of mutual recursion: */
static a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr dip);
static a_constant_hash_value hash_constant(a_constant *cp);

#if DEBUG
/* Forward declaration needed because of mutual recursion. */
void db_type(a_type *tp);


static void db_template_arg_list(a_template_arg_ptr tap)
/*
Dump a list of template arguments, enclosed by angle brackets.
*/
{
  if (tap != NULL) {
    fputs("<", f_debug);
    do {
      if (tap->is_type) {
        db_abbreviated_type(tap->variant.type);
      } else {
        db_constant(tap->variant.constant);
      }  /* if */
      tap = tap->next;
      if (tap != NULL) fputs(",", f_debug);
    } while (tap != NULL);
    fputs(">", f_debug);
  }  /* if */
}  /* if */


void db_type_name(a_type_ptr  tp)
/*
Dump the name of a type.  If it's a class generated on the basis of a
template, dump the template arguments, too.
*/
{
  a_class_type_supplement_ptr ctsp;

  db_name(&tp->source_corresp);
  if (is_immediate_class_type(tp)) {
    ctsp = tp->variant.class_struct_union.extra_info;
    if (ctsp != NULL) db_template_arg_list(ctsp->template_arg_list);
  }  /* if */
}  /* db_type_name */


void db_name(a_source_correspondence *sc)
/*
Dump the name from a source correspondence (if any).
*/
{
  if (sc->class_of_which_a_member != NULL) {
    db_type_name(sc->class_of_which_a_member);
    fputs("::", f_debug);
  }  /* if */
  if (sc->name != NULL) {
    fputs(sc->name, f_debug);
  } else {
    fputs("<NULL>", f_debug);
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
        db_type_name(tp);
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
  int            i;

  fputs("\n  ", f_debug);
  if (depth > 0) for (i = depth; i > 0; --i) fputs("  ", f_debug);
  if (C_dialect == C_dialect_cplusplus) {
    db_access_control((an_access_specifier)fp->source_corresp.access);
    (void)fputc(' ', f_debug);
  }  /* if */
  fputs("field \"", f_debug);
  db_name(&fp->source_corresp);
  fputs("\", type = ", f_debug);
  db_abbreviated_type(fp->type);
  fprintf(f_debug, ", offset = %lu", (unsigned long)fp->offset);
  if (fp->is_bit_field) {
    fprintf(f_debug, "+%d, size = %d bit%s", (int)fp->offset_bit_remainder,
            (int)fp->bit_size, fp->bit_size == 1 ? "" : "s");
  }  /* if */
}  /* db_field */


static void db_static_data_member(a_variable_ptr vp)
/*
Dump a static data member (a variable entry), for debug purposes.
*/
{
  fputs("\n  ", f_debug);
  db_access_control((an_access_specifier)vp->source_corresp.access);
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
  db_access_control((an_access_specifier)rp->source_corresp.access);
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


static void db_virtual_function_info(a_type_ptr  tp,
                                     int         depth)
/*
Dump the virtual_function_info_offset field of a class_type_supplement, for
debug purposes.
*/
{
  a_class_type_supplement_ptr  ctsp;
  int                          i = depth;

  if (tp->variant.class_struct_union.any_virtual_functions) {
    fputs("\n  ", f_debug);
    for (; i > 0; --i) fputs("  ", f_debug);
    ctsp = tp->variant.class_struct_union.extra_info;
    fprintf(f_debug, "byte offset for virtual function table ptr = %lu",
                     ctsp->virtual_function_info_offset);
    if (ctsp->virtual_function_info_base_class != NULL) {
      fputs(", in ", f_debug);
      db_name(&ctsp->virtual_function_info_base_class->type->source_corresp);
    }  /* if */
  }  /* if */
}  /* db_virtual_function_info */


static void db_virtual_base_class_ptr(a_base_class *bcp,
                                      int          depth)
/*
Dump information on a virtual base class pointer (the pointer, not the
base class itself), for debug purposes.
*/
{
  int        i;

  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fputs("[[ virtual ", f_debug);
  db_access_control(preferred_derivation_of(bcp)->access);
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
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  a_boolean  complete_subobject = bcp->complete_subobject;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fputs("[[ ", f_debug);
  if (bcp->is_virtual) {
    fputs("virtual ", f_debug);
  }  /* if */
  db_access_control(direct_derivation_of(bcp)->access);
  fprintf(f_debug, " base class %s", tp->source_corresp.name);
  if (bcp->is_virtual) {
    fprintf(f_debug, " (pointer offset = %lu", bcp->pointer_offset);
    if (bcp->pointer_base_class != NULL) {
      fprintf(f_debug, ", in %s",
              bcp->pointer_base_class->type->source_corresp.name);
    }  /* if */
    fputc(')', f_debug);
  } else {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    if (complete_subobject) fputs(" (complete subobj)", f_debug);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
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
    db_virtual_function_info(tp, depth+1);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
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
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  fputs(" ]]", f_debug);
}  /* db_direct_base_class */


static void db_indirect_base_class(a_base_class *bcp)
/*
Dump an indirect base class entry, for debug purposes.
*/
{
  a_derivation_step_ptr        dsp;
  a_base_class_derivation_ptr  bcdp;

  fputs("\n    ", f_debug);
  db_type_name(bcp->type);
  fprintf(f_debug, ", at offset %lu", bcp->offset);
  if (bcp->is_virtual) fputs(", virtual", f_debug);
  if (bcp->ambiguous) fputs(", ambig", f_debug);
  bcdp = bcp->derivation;
  if (bcdp == NULL) {
    fputs(", null derivation", f_debug);
  } else {
    fprintf(f_debug, ", path%s = ", bcdp->next != NULL ? "s" : "");
    for (;;) {
      dsp = bcdp->path;
      if (dsp == NULL) {
        fputs("<null>", f_debug);
      } else {
        for (; dsp != NULL; dsp = dsp->next) {
          fputs("==>", f_debug);
          if (dsp->base_class == NULL || dsp->base_class->type == NULL) {
            fputs("<???>", f_debug);
          } else {
            db_type_name(dsp->base_class->type);
          }  /* if */
        }  /* for */
      }  /* if */
      if (bcp->is_virtual && bcdp->preferred &&
          bcp->derivation->next != NULL) {
        fputs(" (pref'd)", f_debug);
      }  /* if */
      bcdp = bcdp->next;
      if (bcdp == NULL) break;
      fputs("; ", f_debug);
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
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  a_boolean    complete_subobject = bcp->complete_subobject;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  
  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fprintf(f_debug, "[( virtual base class %s (offset = %lu",
		   tp->source_corresp.name, bcp->offset);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  if (bcp->data_section_base_class != NULL) {
    fprintf(f_debug, ", in %s",
            bcp->data_section_base_class->type->source_corresp.name);
  }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  fputc(')', f_debug);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  if (bcp->data_section_base_class == NULL) {
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
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
    db_virtual_function_info(tp, depth+1);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
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
  }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  fputs(" )]", f_debug);
}  /* db_virtual_base_class */


static void db_access_adjustment(an_access_adjustment_ptr aap)
/*
Dump information on an access adjustment entry, for debug purposes.
*/
{
  a_source_correspondence  *sc;
  char                     *str;

  switch (aap->entity.kind) {
    case iek_variable:   str = "static data member";  break;
    case iek_field:      str = "field";               break;
    case iek_routine:    str = "member function";     break;
    case iek_type:       str = "member type";         break;
    case iek_constant:   str = "member constant";     break;
    default:             str = NULL;                  break;
  }  /* switch */
  fputs("\n    ", f_debug);
  if (str == NULL) {
    fputs("<bad access adjustment kind>", f_debug);
  } else {
    db_access_control(aap->access);
    sc = source_corresp_for_il_entry(aap->entity.ptr,
                                     (an_il_entry_kind)aap->entity.kind);
    check_assertion(sc != NULL);
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
      fprintf(f_debug, "%s", int_kind_name(tp->variant.integer.int_kind));
      if (tp->variant.integer.enum_type) fputs(" enum", f_debug);
      break;
    case tk_float:
      fprintf(f_debug, "%s", float_kind_name(tp->variant.float_kind));
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
      fputs("array [", f_debug);
      if (tp->variant.array.is_variable_size_array) {
        fputs("**EXPR**", f_debug);
      } else {
        fprintf(f_debug, "%lu", tp->variant.array.variant.number_of_elements);
      }  /* if */
      fputs("] of ", f_debug);
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
      db_type_name(tp);
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
        if (ctsp != NULL &&
            ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_none) {
          fprintf(f_debug, " (anonymous, %s)",
                  ctsp->anonymous_union_kind ==
                        (an_anonymous_union_kind)auk_field ? "field" : "var");
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
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
              if (!bcp->direct) continue;
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
              db_virtual_base_class_ptr(bcp, 0);
            }  /* if */
          } /* for */
        }  /* if */
        if (ctsp != NULL && ctsp->assoc_scope != NULL) {
          a_variable_ptr           vp = ctsp->assoc_scope->variables;
          a_routine_ptr            rp = ctsp->assoc_scope->routines;
          an_access_adjustment_ptr aap = ctsp->access_adjustments;

          db_virtual_function_info(tp, /*nesting_depth=*/0);
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
            fprintf(f_debug,
                    "\n  member functions (highest virtual func number = %d):",
                    ctsp->highest_virtual_function_number);
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
        if (ptp->has_default_arg) {
          an_expr_node_ptr expr = ptp->default_arg_expr;
          fputs(" (= ", f_debug);
	  if (expr == NULL) {
	    /* Can be NULL for template parameter based default arguments. */
	    fputs("<NULL>", f_debug);
	  } else {
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
	  }  /* if */
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
      if (!tp->variant.typeref.is_const && !tp->variant.typeref.is_volatile) {
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
    case tk_template_param:
      fputs("template-param", f_debug);
      if (tp->variant.template_param.kind ==
                   (a_template_param_type_kind)tptk_type_of_member_constant) {
        fputs(" <unknown-type>", f_debug);
      } else {
        if (tp->variant.template_param.kind ==
                   (a_template_param_type_kind)tptk_param) {
          fprintf(f_debug, "#%lu ",
              (unsigned long)tp->variant.template_param.list_position);
        } else {
          fputc(' ', f_debug);
        }  /* if */
        db_name(&tp->source_corresp);
      }  /* if */
      break;
    default:
      fputs("<bad type>", f_debug);
  }  /* switch */
}  /* db_type */


static void put_str_to_f_debug(char *str)
/*
Output the indicated string to f_debug.  This is used as an output routine
when using the il_to_str routines.
*/
{
  fputs(str, f_debug);
}  /* put_str_to_f_debug */


void db_constant(a_constant *cp)
/*
Dump the contents of the indicated constant, for debug purposes.
*/
{
  an_il_to_str_output_control_block octl;

  /* Set up for use of form_constant. */
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_f_debug;
  octl.gen_pcc_code = (C_dialect == C_dialect_pcc);

  /* Output the constant. */
  form_constant(cp, /*need_parens=*/FALSE, &octl);
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
  register an_expr_node_ptr   operand;
  a_constant_ptr              const_ptr;
  int                         a;
  a_new_delete_supplement_ptr ndsp;
  a_throw_supplement_ptr      tsp;

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
      if (!has_name(const_ptr)) {
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
      fprintf(f_debug, "field ");
      db_name(&node->variant.field->source_corresp);
      fputs("\n", f_debug);
      break;
    case enk_temp_init:
      fprintf(f_debug, "temp init (%s of temporary): ",
              node->variant.init.result_is_addr ? "addr" : "value");
      db_dynamic_initializer(node->variant.init.dynamic_init, level);
      break;
    case enk_new_delete:
      ndsp = node->variant.new_delete;
      fprintf(f_debug, "%s: routine = %s, type = ",
                       ndsp->is_new ? "new" : "delete",
                       (ndsp->routine != NULL) ?
                                ndsp->routine->source_corresp.name : "(null)");
      db_abbreviated_type(ndsp->type);
      fputs("\n", f_debug);
      for (operand = ndsp->arg; operand != NULL; operand = operand->next) {
        db_expr_node(operand, level + 2);
      }  /* for */
      if (ndsp->dynamic_init != NULL) {
        for (a = 0; a < level; a++) fputs(" ", f_debug);
        fprintf(f_debug, "dynamic_init: ");
        db_dynamic_initializer(ndsp->dynamic_init, level + 2);
      }  /* if */
      break;
    case enk_throw:
      fputs("throw: ", f_debug);
      tsp = node->variant.throw_info;
      if (tsp == NULL) {
        fputs("rethrow", f_debug);
      } else {
        fprintf(f_debug, "type = ");
        db_abbreviated_type(tsp->type);
        fprintf(f_debug, ", dynamic_init = ");
        if (tsp->dynamic_init == NULL) {
          fprintf(f_debug, "<null>");
        } else {
          db_dynamic_initializer(tsp->dynamic_init, level + 2);
        }  /* if */
      }  /* if */
      fputs("\n", f_debug);
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
/*
Dump debug information on a constant that is the initial value in a dynamic
initialization entry or the static initial value of a variable.
*/
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
/*
Dump debug information on the destructor part of a dynamic initialization
entry.
*/
{
  fputs("destructor: ", f_debug);
  db_name(&dtor->source_corresp);
  fputs("()", f_debug);
}  /* db_destructor */


static void db_constructor_initializer(a_dynamic_init_ptr  dip,
                                       int                 level)
/*
Dump debug information on a dynamic initialization entry of type
dik_constructor.
*/
{
  an_expr_node_ptr  arg;
  a_param_type_ptr  ptp;

  fputs("constructor ", f_debug);
  db_name(&dip->variant.constructor.ptr->source_corresp);
  (void)fputc('(', f_debug);
  ptp = f_skip_typerefs(dip->variant.constructor.ptr->type)->
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
/*
Dump debug information on a dynamic initialization entry of kind
dik_nonconstant_aggregate.
*/
{
  int  a;

  for (; con != NULL; con = con->next) {
    if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
      a_dynamic_init_ptr  dip = con->variant.dynamic_init;
      switch (dip->kind) {
        case dik_expression:
        case dik_call_returning_class_via_cctor:
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
        case dik_none:
        case dik_zero:
          for (a = 0; a < level; a++) fputs(" ", f_debug);
          fputs(dip->kind == (a_dynamic_init_kind)dik_none ?
		    "no initializer" : "zero initializer",
                f_debug);
          if (dip->destructor != NULL) {
            fputs(", ", f_debug);
            db_destructor(dip->destructor);
          }  /* if */
          (void)fputc('\n', f_debug);
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
/*
Dump a dynamic initializer entry for debug purposes.
*/
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
    case dik_call_returning_class_via_cctor:
      fputs("call returning class via cctor:\n", f_debug);
      db_expr_node(dip->variant.expression, level);
      goto destructor_on_next_line;
    case dik_nonconstant_aggregate:
      fputs("nonconstant aggregate:\n", f_debug);
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
    case dik_bitwise_copy:
      fputs("<bitwise copy>", f_debug);
      goto destructor_on_this_line;
    case dik_none:
      fputs("<none>", f_debug);
      goto destructor_on_this_line;
    case dik_zero:
      fputs("<zero>", f_debug);
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
/*
Dump the initializer of a variable for debug purposes.
*/
{
  int  a;
  char *partial;

  if (var->init_kind != (an_init_kind)initk_none) {
    partial = var->is_partially_initialized ? " (partial)" : "";
    for (a = 0; a < level; a++) fputs(" ", f_debug);
    if (var->init_kind == (an_init_kind)initk_static) {
      fprintf(f_debug, "static init%s: ", partial);
      db_static_initializer(var->initializer.constant);
      (void)fputc('\n', f_debug);
    } else if (var->init_kind == (an_init_kind)initk_zero) {
      fprintf(f_debug, "zero init%s\n", partial);
    } else {
      fprintf(f_debug, "dynamic init%s: ", partial);
      db_dynamic_initializer(var->initializer.dynamic, level + 2);
    }  /* if */
  }  /* if */
}  /* db_initializer */
#endif /* DEBUG */

#endif /* !STANDALONE_UTILITY_PROGRAM */

#define TEMP_TEXT_BUFFER_INCREMENTAL_ALLOCATION 2000
			/* Initial and incremental allocation size for
			   temp_text_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */

void expand_temp_text_buffer(sizeof_t size_needed)
/*
Expand the temp_text_buffer by reallocating it, so that its total size
is at least size_needed.  Called by ensure_temp_text_buffer_space.
*/
{
  sizeof_t new_size;

  new_size = size_temp_text_buffer + TEMP_TEXT_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  temp_text_buffer = realloc_general(temp_text_buffer, size_temp_text_buffer,
                                     new_size);
  size_temp_text_buffer = new_size;
}  /* expand_temp_text_buffer */


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

static void reset_seq_cache(void)
/*
Clear the cached information used to convert sequence number to source
file and line number.
*/
{
  /* Reset the entry that stores the status of the last sequence number to
     source file/line conversion. */
  seq_cache.first_seq_number = 0;
  seq_cache.last_seq_number = 0;
  seq_cache.line_offset = 0;
  seq_cache.physical_line = FALSE;
  seq_cache.nesting_depth = 0;
  seq_cache.source_file = NULL;
}  /* reset_seq_cache */
  

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

#if GENERATE_SOURCE_SEQUENCE_LISTS
static void switch_to_scope_region(
                              a_scope_depth          scope_depth,
                              a_memory_region_number *region_to_switch_back_to)
/*
Switch to the function-scope memory region if not already there.  Set
region_to_switch_back_to for use later by switch_back_to_original_region.
*/
{
  a_memory_region_number region;

  region = scope_stack[scope_depth].il_memory_region;
  if (curr_il_region_number != region) {
    *region_to_switch_back_to = curr_il_region_number;
    switch_il_region(region);
  } else {
    *region_to_switch_back_to = NULL_region_number;
  }  /* if */
}  /* switch_to_scope_region */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

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
                                 char              *name_as_written,
			         a_source_file_ptr *new_file,
				 a_boolean	   is_system_include)
/*
Create a source file entry in the intermediate language, to record the
start of a new source file (either the primary source file or an include file).
Line line_number of the new file is sequence number seq_number of the
compilation, and the new file's short and long form names are file_name
and full_name.  The parent file for this file is parent_file; if there
is no parent file (i.e., for the primary source file), parent_file == NULL.
For entries generated by #line directives, full_name == NULL and
name_as_written == NULL.  When the name is manufactured as part of automatic
instantiation, name_as_written is also NULL.  is_system_include is TRUE for
files included with the #include <file.h> notation and FALSE for all other
files.
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
#if DEBUG
  num_source_files_allocated++;
#endif /* DEBUG */
  sfp->file_name        = file_name;
  sfp->full_name        = full_name;
  sfp->name_as_written  = name_as_written;
  sfp->first_seq_number = seq_number;
  sfp->last_seq_number  = MAX_SEQ_NUMBER;  /* Not yet entered. */
  sfp->first_line_number= line_number;
  sfp->first_child_file = NULL;
  sfp->last_child_file  = NULL;
  sfp->next             = NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  sfp->related_file_implicit_include_done = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  sfp->included_by_system_include = is_system_include;
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
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (parent_file == il_header.primary_source_file) {
      /* Update the parent file's last sequence number.  This is required
         when an implicitly included template definition file is started
         after the end of the primary source file has already been
         recorded. */
      parent_file->last_seq_number = MAX_SEQ_NUMBER;
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  }  /* if */
  /* Clear the cached sequence number conversion information. */
  reset_seq_cache();
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
  /* Clear the cached sequence number conversion information. */
  reset_seq_cache();
  db_exit();
}  /* record_end_of_source_file */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_source_file_ptr source_file_for_seq(a_seq_number   seq_number,
                                      a_line_number  *line_number,
                                      a_boolean      *at_end_of_source,
                                      unsigned long  *nesting_depth,
                                      a_boolean      physical_line)
/*
Find the source file entry within which the sequence number seq_number falls,
and return a pointer to it.  Return NULL if the sequence number falls
outside of any file.  If the sequence number falls within a file, also
return *line_number set to the line number in the file.  Return
*at_end_of_source TRUE if the line number is the special number indicating
the end-of-file line (one more than the last line in the primary input file).
Return the file nesting depth in *nesting_depth (0 => not inside any file,
1 => in primary source file, 2 => inside one level of #include, etc.).  If
physical_line is TRUE, ignore #line directive information and return the
physical line position for the sequence number.
*/
{
  a_source_file_ptr curr_file, child_file, grandchild_file, phys_curr_file;
  unsigned long     lines_in_children;
  a_seq_number	    first_seq_for_cache;
  long		    line_offset;

  *at_end_of_source = FALSE;
  *line_number = 0;
  *nesting_depth = 0;
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
  } else if (physical_line == seq_cache.physical_line &&
             seq_number >= seq_cache.first_seq_number &&
             seq_number <= seq_cache.last_seq_number &&
             (seq_number-1 < curr_file->last_seq_number)) {
    /* See whether this sequence number falls into the range of lines
       associated with the information saved by the last lookup.  If so,
       use the information saved last time to speed up the conversion.
       Note that the "at end of source" line will not fall into this range
       and will be handled by a normal conversion below. */
    *line_number = seq_number + seq_cache.line_offset;
    *nesting_depth = seq_cache.nesting_depth;
    curr_file = seq_cache.source_file;
  } else {
    if (seq_number-1 == curr_file->last_seq_number) {
      /* At end of source.  Use the last line of the primary source file. */
      *at_end_of_source = TRUE;
      seq_number--;
    }  /* if */
    lines_in_children = 0;
    /* See if the sequence number falls within any child file. */
examine_children:
    (*nesting_depth)++;
    if (!physical_line) {
      /* #line directives are just as valid as #includes. */
      lines_in_children = 0;
    } else {
      /* We want the physical line number, so #line directives count less than
         #includes. */
      if (curr_file->full_name != NULL) {
        /* Examining a real file, rather than an entry for a #line directive.
           Remember the physical file information in case what we're descending
           to is an entry for a #line directive. */
        phys_curr_file = curr_file;
        lines_in_children = 0;
      }  /* if */
    }  /* if */
    child_file = curr_file->first_child_file;
    /* Record the first sequence number of the current file as the first
       sequence number for which the cached information applies.  This will
       be updated below if necessary to reflect child files. */
    first_seq_for_cache = curr_file->first_seq_number;
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
        curr_file = child_file;
        goto examine_children;
      }  /* if */
      /* The sequence number falls after this child, so keep looking.
         Keep track of the number of lines in children.  If the entry we
         are skipping over is for a #line directive, only count the lines
         in its #include children, not those of the current file spanned
         by the #line directive. */
      if (child_file->full_name != NULL) {
        /* Real file. */
        lines_in_children += child_file->last_seq_number -
                             child_file->first_seq_number + 1;
      } else {
        /* #line directive.  Note that typically when #line directives
           appear there are no #includes, so the loop here does nothing. */
        for (grandchild_file = child_file->first_child_file;
             grandchild_file != NULL;
             grandchild_file = grandchild_file->next) {
          lines_in_children += grandchild_file->last_seq_number -
                               grandchild_file->first_seq_number + 1;
        }  /* for */
      }  /* if */
      /* Record the sequence number following this child as the first
         sequence number for which the cached information applies. */
      first_seq_for_cache = child_file->last_seq_number + 1;
      child_file = child_file->next;
    }  /* while */
    if (physical_line) {
      /* If we want to ignore #line directives, go back to the last entry
         for a real file that we saw. */
      curr_file = phys_curr_file;
    }  /* if */
    /* Save information about the file in which this sequence number was found
       so that subsequent lines may be found more quickly. */
    line_offset = -(long)(curr_file->first_seq_number) + 
                            curr_file->first_line_number - lines_in_children;
    /* Save information about this conversion so that subsequent conversions
       can be done more quickly. */
    seq_cache.first_seq_number = first_seq_for_cache;
    if (child_file != NULL) {
      /* This cache entry is valid up to the first line of the next
         child file. */
      seq_cache.last_seq_number = child_file->first_seq_number - 1;
    } else {
      /* This cached entry is valid through the end of the current file. */
      seq_cache.last_seq_number = curr_file->last_seq_number;
    }  /* if */
    seq_cache.line_offset = line_offset;
    seq_cache.source_file = curr_file;
    seq_cache.nesting_depth = *nesting_depth;
    seq_cache.physical_line = physical_line;
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "Cached source sequence conversion information:\n");
      fprintf(f_debug, "  file=%s\n", curr_file->file_name);
      fprintf(f_debug, "  first_seq_number: %lu\n",
                       seq_cache.first_seq_number);
      fprintf(f_debug, "  last_seq_number: %lu\n", seq_cache.last_seq_number);
      fprintf(f_debug, "  line_offset: %ld\n", seq_cache.line_offset);
      fprintf(f_debug, "  physical_line: %d\n", seq_cache.physical_line);
      fprintf(f_debug, "  seq number requested=%lu\n", seq_number);
    }  /* if */
#endif /* DEBUG */
    /* Compute the line number to be returned to the caller. */
    *line_number = seq_number + line_offset;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "File=%s, Line=%lu\n",
                     curr_file->file_name, *line_number);
  }  /* if */
#endif /* DEBUG */
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
  a_source_file_ptr proper_file;
  unsigned long     nesting_depth;

  db_enter(5, "conv_seq_to_file_and_line");

  /* Find out which file the sequence number is in. */
  proper_file = source_file_for_seq(seq_number, line_number, at_end_of_source,
                                    &nesting_depth, /*physical_line=*/FALSE);
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
  unsigned long nesting_depth;

  db_enter(5, "conv_seq_to_physical_file_and_line");

  /* Find out which file the sequence number is in. */
  *src_file = source_file_for_seq(seq_number, physical_line, at_end_of_source,
                                  &nesting_depth, /*physical_line=*/TRUE);

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
  a_source_file_ptr proper_file, primary_file, first_file_under_primary;
  a_line_number     line_number;
  a_boolean         at_end_of_source;
  unsigned long     nesting_depth;

  primary_file = il_header.primary_source_file;
  proper_file = source_file_for_seq(seq_number, &line_number,
                                    &at_end_of_source, &nesting_depth,
                                    /*physical_line=*/FALSE);
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

void f_add_orphaned_file_scope_il_entry(char             *entry_ptr,
                                        an_il_entry_kind entry_kind)
/*
Link the specified file scope IL entry onto the orphaned_file_scope_il_entries
linked list for the designated IL entry kind.  Only IL entries in the
file scope memory region have the necessary additional pointer space 
allocated immediately preceding the entry.  This routine is called by
the macro add_orphaned_file_scope_il_entry, which checks that
fs_orphan_pointer_of(entry_ptr) == NULL for speed (this routine does
not check that again, so it should not be called directly).
*/
{
  char **last_entry_ptr;

  check_assertion_str(in_file_scope(entry_ptr),
    "f_add_orphaned_file_scope_...: IL entry not in file scope memory region");
  /* Check if this IL entry is already on the orphaned entry list. */
  last_entry_ptr = &orphaned_file_scope_il_entries[(int)entry_kind].last_entry;
  /* The following check was done by the macro that guards entry to this
     routine: fs_orphan_pointer_of(entry_ptr) == NULL.  If one wants this
     routine to be directly callable, the test should be done again here. */
  if (entry_ptr != *last_entry_ptr) {
    /* This entry is not in the existing list; add it to the end of the
       list. */
    if (*last_entry_ptr == NULL) {
      /* This is the first entry on this list */
      orphaned_file_scope_il_entries[(int)entry_kind].first_entry = 
                                                               entry_ptr;
    } else {
      /* Add to the tail of the existing list. */
      fs_orphan_pointer_of(*last_entry_ptr) = entry_ptr;
    }  /* if */
    *last_entry_ptr = entry_ptr;
  }  /* if */
}  /* f_add_orphaned_file_scope_il_entry */

#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

static void r_add_scope_orphaned_il_lists(a_scope_ptr   scope,
                                          a_routine_ptr routine)
/*
If the indicated scope (a function or block scope) contains non-empty
lists that are in the file scope memory region (e.g., local types or
static variables), create a_scope_orphaned_list_header entry to hold
those pointers in the file scope so that orphan processing can be
done on the lists later.  Also use recursion to visit all block scopes
under this scope to do the same processing.  routine indicates the
function this scope is part of.  This routine does the recursive
processing for add_scope_orphaned_il_lists.
*/
{
  a_type_ptr            types = scope->types;
  a_variable_ptr        variables = scope->variables;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_src_seq_sublist_ptr sublists = scope->src_seq_sublist_list;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_scope_ptr           block_scope;

  if (types != NULL || variables != NULL
#if GENERATE_SOURCE_SEQUENCE_LISTS
                                         || sublists != NULL
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
                                                            ) {
    /* At least one of the IL pointers is not NULL; create
       a_scope_orphaned_list_header in the file scope region and add it to
       the list headed by il_header.scope_orphaned_list_headers. */
    a_scope_orphaned_list_header_ptr solhp =
                        (a_scope_orphaned_list_header_ptr)
                                alloc_il(sizeof(a_scope_orphaned_list_header));
#if DEBUG && !STANDALONE_UTILITY_PROGRAM
    num_scope_orphaned_list_headers_allocated++;
#endif /* DEBUG  && !STANDALONE_UTILITY_PROGRAM */
    solhp->assoc_routine = routine;
    solhp->scope_number = scope->number;
    solhp->orphaned_types = types;
    solhp->orphaned_variables = variables;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    solhp->orphaned_src_seq_sublists = sublists;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    solhp->next = NULL;
    if (il_header.scope_orphaned_list_headers == NULL) {
      il_header.scope_orphaned_list_headers = solhp;
    } else {
      last_scope_orphaned_list_header->next = solhp;
    }  /* if */
    last_scope_orphaned_list_header = solhp;
  }  /* if */
  /* Process subscopes of this scope. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    r_add_scope_orphaned_il_lists(block_scope, routine);
  }  /* for */
}  /* r_add_scope_orphaned_il_lists */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

void add_scope_orphaned_il_lists(a_scope_ptr scope)
/*
If the indicated scope (a function scope) contains non-empty lists
that are in the file scope memory region (e.g., local types or static
variables), create a_scope_orphaned_list_header entry to hold those
pointers in the file scope so that orphan processing can be done on
the lists later.  Also visit all block scopes attached to this scope
and do the same processing.
*/
{
  r_add_scope_orphaned_il_lists(scope, scope->variant.routine.ptr);
}  /* add_scope_orphaned_il_lists */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
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


void break_source_corresp(a_source_correspondence *sc)
/*
If the indicated source correspondence is attached to a source entity,
break the correspondence.
*/
{
  sc->assoc_info = NULL;
  sc->name       = NULL;
}  /* break_source_corresp */


a_source_correspondence *source_corresp_for_il_entry(
                                                 char              *entity_ptr,
                                                 an_il_entry_kind  kind)
/*
Given an IL entry pointer (entity_ptr) of a given kind (kind), return a
pointer to its source correspondence entry if it has one and NULL if it does
not.
*/
{
  a_source_correspondence *scp;

  switch (kind) {
    case iek_constant:
    case iek_type:
    case iek_variable:
    case iek_field:
    case iek_routine:
    case iek_asm_entry:
    case iek_label:
#if RECORD_TEMPLATES_IN_IL
    case iek_template:
#endif /* RECORD_TEMPLATES_IN_IL */
#if RECORD_MACROS_IN_IL
    case iek_macro:
#endif /* RECORD_MACROS_IN_IL */
      scp = &((a_constant_ptr)entity_ptr)->source_corresp;
      break;
    default:
      scp = NULL;
  }  /* switch */
  return scp;
}  /* source_corresp_for_il_entry */


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
#if DO_IL_LOWERING
  cp->assoc_var_assigned = FALSE;
#endif /* DO_IL_LOWERING */
#if CHECKING
  cp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
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


void set_routine_address_constant(a_routine_ptr routine,
                                  a_constant    *con,
                                  a_boolean     set_address_taken_flag)
/*
Fill in the constant "con" as a ck_address constant for the address of
the indicated routine.  If set_address_taken_flag is TRUE, the
address_taken flag in the routine is set (it's not always set
because the constant might be used in a way that doesn't really take
the address of the routine, e.g., a call).
*/
{
  clear_constant(con, (a_constant_repr_kind)ck_address);
  con->variant.address.kind = (an_address_base_kind)abk_routine;
  con->variant.address.variant.routine = routine;
  con->type = make_pointer_type(routine->type);
  if (set_address_taken_flag) routine->address_taken = TRUE;
}  /* set_routine_address_constant */


void set_variable_address_constant(a_variable_ptr variable,
                                   a_constant     *con,
                                   a_boolean      set_address_taken_flag)
/*
Fill in the constant "con" as a ck_address constant for the address of
the indicated variable.  If set_address_taken_flag is TRUE, the
address_taken flag in the variable is set (it's not always set
because the constant might be used in a way that doesn't really take
the address of the variable, e.g., an lvalue).
*/
{
  clear_constant(con, (a_constant_repr_kind)ck_address);
  con->variant.address.kind = (an_address_base_kind)abk_variable;
  con->variant.address.variant.variable = variable;
  con->type = make_pointer_type(variable->type);
  if (set_address_taken_flag) variable->address_taken = TRUE;
}  /* set_variable_address_constant */


void set_constant_address_constant(a_constant_ptr constant,
                                   a_constant    *con)
/*
Fill in the constant "con" as a ck_address constant for the address of
the indicated constant.
*/
{
  clear_constant(con, (a_constant_repr_kind)ck_address);
  con->variant.address.kind = (an_address_base_kind)abk_constant;
  con->variant.address.variant.constant = constant;
  con->type = make_pointer_type(constant->type);
}  /* set_constant_address_constant */


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


void copy_constant(a_constant *from,
                   a_constant *to)
/*
Copy a constant entry from "from" to "to".
*/
{
  *to = *from;
  /* *from might be a shared constant, an enum constant, etc., so clear
     the "next" field. */
  to->next = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Same holds for source_sequence pointers. */
  to->source_corresp.source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
  /* Clear the source correspondence information.  This version of the
     constant isn't the one directly associated with the source entity,
     if any. */
  break_source_corresp(&ucp->source_corresp);
  return ucp;
}  /* alloc_unshared_constant */


a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant)
/*
Make a copy of an unshared constant and return pointer to the copy.
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


static a_constant_hash_value hash_type(a_type_ptr type)
/*
Return a hash value for the indicated type.  This is used in some cases
to refine the hash value developed in hash_constant.
*/
{
  a_constant_hash_value       hash_value;
  a_class_type_supplement_ptr ctsp;
  a_template_arg_ptr          tap;

  /* Only pointers to class types are particularly important here. */
  /* Note that the address of the type or its subtypes should not be
     used in determining the hash value (see comment in hash_constant). */
  switch (type->kind) {
    case tk_integer:
      hash_value = type->variant.integer.int_kind + 53;
      break;
    case tk_float:
      hash_value = type->variant.float_kind + 87;
      break;
    case tk_pointer:
      hash_value = hash_type(type->variant.pointer.type) + 107;
      break;
    case tk_array:
      hash_value = hash_type(type->variant.array.element_type) + 307;
      if (!type->variant.array.is_variable_size_array) {
        hash_value += (a_constant_hash_value)
                            (type->variant.array.variant.number_of_elements);
      }  /* if */
      break;
    case tk_struct:
    case tk_class:
    case tk_union:
      hash_value = (a_constant_hash_value)type->kind;
      ctsp = type->variant.class_struct_union.extra_info;
      if (ctsp != NULL) {
        if (ctsp->assoc_scope != NULL) {
          /* Use the scope number as the hash value. */
          hash_value = ctsp->assoc_scope->number;
        } else {
          /* No definition for the class. */
          /* Work in the template arguments if there are any. */
          for (tap = ctsp->template_arg_list; tap != NULL; tap = tap->next) {
            if (tap->is_type) {
              hash_value += hash_type(tap->variant.type) + 37;
            } else {
              hash_value += hash_constant(tap->variant.constant) + 43;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case tk_typeref:
      hash_value = hash_type(type->variant.typeref.type) + 17;
      break;
    default:
      hash_value = (a_constant_hash_value)type->kind;
  }  /* switch */
  return hash_value;
}  /* hash_type */


static a_constant_hash_value hash_name(a_source_correspondence *scp)
/*
Return a hash value developed from the name in the indicated source
correspondence entry.
*/
{
  a_constant_hash_value hash_value = 0;
  char                  *cptr = scp->name;

  if (cptr != NULL) {
    for (; *cptr != '\0'; cptr++) {
      hash_value = (hash_value << 6) + *cptr;
    }  /* for */
  }  /* if */
  return hash_value;
}  /* hash_name */


static a_constant_hash_value hash_constant(a_constant *cp)
/*
Return the hash value for the indicated constant, which gives the proper
bucket of the shareable_constants_table to use for the constant.
*/
{
  a_constant_hash_value hash_value;
  a_targ_size_t         length;
  a_boolean             ovflo;

  /* Compute a hash value from the constant.  The hash doesn't have to
     be perfect, but it should spread the expected constants fairly widely.
     The hash should *not* involve the address of the constant or its type;
     such an approach was tried, and works well from a hashing point of
     view, but has the undesirable property that it makes the order of
     the shared constants in the IL highly dependent on the host machine,
     which means the same version of the front end compiled on two different
     hosts will generate different IL and perhaps different object code
     on those two machines. */
  switch (cp->kind) {
    case ck_integer:
      /* Integer.  Use the constant itself as the hash value. */
      hash_value = (a_constant_hash_value)value_of_integer_constant(cp,&ovflo);
      break;
    case ck_string:
      /* String.  Use the first and last characters and the length to make
         a hash value. */
      length = cp->variant.string.length;
      hash_value = (a_constant_hash_value)(
                            100 + length + (*(cp->variant.string.value) << 6) +
                                         *(cp->variant.string.value+length-1));
      break;
    case ck_float:
      /* Use a host-dependent routine for floating-point constants. */
      hash_value = 500 + fp_hash(&cp->variant.float_value);
      break;
    case ck_address:
      /* Address constant.  If the thing pointed to is named, hash the name;
         otherwise (for the address of a constant), hash the constant pointed
         to. */
      switch (cp->variant.address.kind) {
        case abk_routine:
          hash_value =
               hash_name(&cp->variant.address.variant.routine->source_corresp);
          break;
        case abk_variable:
          hash_value =
              hash_name(&cp->variant.address.variant.variable->source_corresp);
          break;
        case abk_constant:
          /* Hash the name if the constant has a name; otherwise, hash the
             constant pointed to. */
          if (has_name(cp->variant.address.variant.constant)) {
            hash_value =
              hash_name(&cp->variant.address.variant.constant->source_corresp);
          } else {
            hash_value = hash_constant(cp->variant.address.variant.constant);
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("hash_constant: bad address constant kind");
#endif /* CHECKING */
      }  /* switch */
      /* Add the offset in the address constant into the hash value. */
      hash_value += (a_constant_hash_value)(cp->variant.address.offset + 1000);
      break;
    case ck_ptr_to_member:
      /* Hash the name of the member in a pointer-to-member constant. */
      hash_value = 0;
      if (cp->variant.ptr_to_member.is_function_ptr) {
        a_routine_ptr rp = cp->variant.ptr_to_member.variant.routine;
        if (rp != NULL) hash_value = hash_name(&rp->source_corresp);
      } else {
        a_field_ptr fp = cp->variant.ptr_to_member.variant.field;
        if (fp != NULL) hash_value = hash_name(&fp->source_corresp);
      }  /* if */
      hash_value += 250;
      break;
    default:
      hash_value = (a_constant_hash_value)(200 + cp->kind);
      break;
  }  /* switch */
  if (cp->implicit_cast ||
      cp->kind == (a_constant_repr_kind)ck_ptr_to_member) {
    /* Work the type into the hash.  This is important when you have lots of
       NULL pointer constants for a lot of different types. */
    hash_value += hash_type(cp->type);
  }  /* if */
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


static a_boolean compare_template_param_constant_expressions(
                                                     an_expr_node_ptr  node1,
                                                     an_expr_node_ptr  node2)
/*
Return TRUE if node1 and node2 are equivalent expression trees.
*/
{
  a_boolean         eq = FALSE;

  if (node1->kind == node2->kind) {
    switch (node1->kind) {
      case enk_operation:
        if (node1->variant.operation.kind == node2->variant.operation.kind) {
          an_expr_node_ptr   op1 = node1->variant.operation.operands;
          an_expr_node_ptr   op2 = node2->variant.operation.operands;

          check_assertion(op1 != NULL && op2 != NULL);
          do {
            if (!compare_template_param_constant_expressions(op1, op2)) {
              /* Operands are not equivalent. */
              break;
            } else {
              /* Operands are equivalent -- check other operands, if any. */
              op1 = op1->next;
              op2 = op2->next;
              if (op1 == NULL && op2 == NULL) {
                /* Both operand lists are exhausted, so we have a match. */
                eq = TRUE;
                break;
              }  /* if */
            }  /* if */
          } while (op1 != NULL && op2 != NULL);
          /* Falling though with one list incomplete means eq remains FALSE. */
        }  /* if */
        break;
      case enk_constant:
        eq = eq_constants(node1->variant.constant, node2->variant.constant);
        break;
      case enk_variable_address:
        eq = (node1->variant.variable == node2->variant.variable);
        break;
      case enk_routine_address:
        eq = (node1->variant.routine == node2->variant.routine);
        break;
      case enk_error:
        /* Nonequivalence is assumed. */
        break;
#if CHECKING
      default:
        internal_error("compare_template_param_constant_expr: bad expr kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return eq;
}  /* compare_template_param_constant_expressions */


static a_boolean compare_constants(a_constant_ptr  cp1,
                                   a_constant_ptr  cp2,
                                   a_boolean       strictly_identical)
/*
Return TRUE if the two constants are identical.  If strictly_identical
is FALSE the qualifiers are stripped from the constant type before they
are compared; otherwise, a "const int 5" and an "int 5" are treated as
nonidentical.
*/
{
  a_boolean  eq = FALSE, unordered;
  a_type_ptr cp1_type = cp1->type, cp2_type = cp2->type;

  check_assertion(cp1 != cp2);
  check_assertion(cp1->kind == cp2->kind);
  if (!strictly_identical) {
    cp1_type = skip_typerefs(cp1_type);
    cp2_type = skip_typerefs(cp2_type);
  }  /* if */
  /* If strict identity is required, the types must be pointer-identical.
     Otherwise, it is sufficient that they be identical. */
  if (strictly_identical ? (cp1_type == cp2_type) :
                           identical_types(cp1_type, cp2_type)) {
    switch (cp1->kind) {
      case ck_error:
        /* No further field to check. */
        eq = TRUE;
        break;
      case ck_integer:
        eq = (cmp_integer_constants(cp1, cp2) == 0);
        break;
      case ck_string:
        if (cp1->variant.string.length == cp2->variant.string.length) {
          eq = (memcmp(cp1->variant.string.value, cp2->variant.string.value,
                       size_t_arg(cp1->variant.string.length)) == 0);
        }  /* if */
        break;
      case ck_float:
        cp1_type = skip_typerefs(cp1_type);
        if (is_floating_type(cp1_type)) {
          eq = (fp_compare(cp1_type->variant.float_kind,
                           &cp1->variant.float_value,
                           &cp2->variant.float_value,
                           &unordered) == 0 && !unordered);
        }  /* if */
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
              internal_error("compare_constants: bad address constant kind");
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
      case ck_template_param:
        /* Note that the constant types have been compared above. */
        if (cp1->variant.template_param.kind ==
                                      cp2->variant.template_param.kind) {
          switch (cp1->variant.template_param.kind) {
            case tpck_param:
              eq = (cp1->variant.template_param.variant.list_position ==
                            cp2->variant.template_param.variant.list_position);
              break;
            case tpck_expression:
              eq = compare_template_param_constant_expressions(
                                    cp1->variant.template_param.variant.expr,
                                    cp2->variant.template_param.variant.expr);
              break;
            case tpck_member:
              check_assertion(cp1->source_corresp.assoc_info != NULL);
              check_assertion(cp2->source_corresp.assoc_info != NULL);
              eq = (cp1->source_corresp.assoc_info ==
                    cp2->source_corresp.assoc_info);
              break;
#if CHECKING
            default:
              internal_error("compare_constants: bad templ param const kind");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        break;
#if CHECKING
      default:
        internal_error("compare_constants: bad constant kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return eq;
}  /* compare_constants */


static a_boolean identical_constants(a_constant *cp1,
                                     a_constant *cp2)
/*
Return TRUE if the two constants are identical.  This routine is called
to decide whether two constant entries are sufficiently alike to be
shared, such that only one of them need appear in the IL.
*/
{
  a_boolean  eq = FALSE;

  if (cp1 == cp2) {
    /* Same pointer implies same constant. */
    eq = TRUE;
  } else if (cp1->kind == cp2->kind) {
    check_assertion(cp1->kind != (a_constant_repr_kind)ck_template_param);
    eq = compare_constants(cp1, cp2, /*strictly_identical=*/TRUE);
  }  /* if */
  return eq;
}  /* identical_constants */


a_boolean eq_constants(a_constant *cp1,
                       a_constant *cp2)
/*
Return TRUE if the two constants are equivalent, i.e., represent the same
value.  Thus, "(int)5" and "(const int)5" are equivalent -- even though they
would not be considered "identical", since the type qualifiers are different.
*/
{
  a_boolean  eq = FALSE;

  if (cp1 == cp2) {
    /* Same pointer implies same constant. */
    eq = TRUE;
  } else if (cp1->kind == cp2->kind) {
    eq = compare_constants(cp1, cp2, /*strictly_identical=*/FALSE);
  }  /* if */
  return eq;
}  /* eq_constants */


a_boolean expr_tree_contains_template_param_constant(an_expr_node_ptr  node,
                                                     a_constant_ptr    cp)
/*
cp is either a pointer to a simple template parameter constant or else
NULL (indicating any template param constant will do).  If cp is NULL, return
TRUE if node is or contains any template parameter constant.  If it is not
NULL, return TRUE if node refers to that particular constant directly or
contains it among its operands.
*/
{
  a_boolean         found = FALSE;
  a_constant_ptr    cp2;
  an_expr_node_ptr  op;

  check_assertion(cp == NULL ||
                  cp->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_param);
  if (node->kind == (an_expr_node_kind)enk_constant) {
    cp2 = node->variant.constant;
    if (cp2->kind == (a_constant_repr_kind)ck_template_param) {
      if (cp == NULL) {
        found = TRUE;
      } else {
        switch (cp2->variant.template_param.kind) {
          case tpck_param:
            found = eq_constants(cp, cp2);
            break;
          case tpck_expression:
            found = expr_tree_contains_template_param_constant(
                                cp2->variant.template_param.variant.expr, cp);
            break;
          default:;
        }  /* switch */
      }  /* if */
    }  /* if */
  } else if (node->kind == (an_expr_node_kind)enk_operation) {
    for (op = node->variant.operation.operands; op != NULL; op = op->next) {
      if (expr_tree_contains_template_param_constant(op, cp)) {
        found = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return found;
}  /* expr_tree_contains_template_param_constant */


static a_boolean has_non_file_scope_ref(a_constant *cp)
/*
Return TRUE if the constant pointed to by cp includes a reference to something
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
    case ck_template_param:
      /* Aggregates and template parameters shouldn't be shared, so we don't
         expect them here. */
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
        if (identical_constants(scp, cp)) {
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


a_scope_ptr ensure_il_scope_exists(a_scope_stack_entry_ptr ssep)
/*
Make sure that the scope stack entry pointed to by ssep points to an IL
scope.  If it does not and the current scope stack entry is for a block
scope, create the scope entry and return it.  (Note that a NULL pointer
is returned only for scopes that might appear in exceptional cases; the
caller is responsible for sorting that out.)
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
    } else if (ssep->kind == (a_scope_kind)sck_func_prototype) {
      a_type_ptr              routine_type;

      /* A prototype scope must be allocated.  It is always allocated in the
         file scope memory region because it is pointed to from the routine
         type supplement, which is always at file scope. */
      check_assertion(curr_il_region_number == FILE_SCOPE_REGION_NUMBER);
      sp = alloc_scope((a_scope_kind)sck_func_prototype, ssep->number,
                       (a_routine_ptr)NULL);
      ssep->il_scope = sp;
      /* Call add_to_scopes_list only if this is a function prototype nested
         within another function prototype.  A function prototype scope that
         is not nested is just pointed to from the routine type, not from the
         function scope entry (which will not exist if the function is not
         defined). */
      if ((ssep-1)->kind == (a_scope_kind)sck_func_prototype) {
        add_to_scopes_list(sp, ssep-1);
      }  /* if */
      /* Link the routine type and the prototype scope entry to each other. */
      routine_type = ssep->assoc_type;
      check_assertion_str(routine_type != NULL,
                          "ensure_il_scope_exists: routine_type is NULL");
      routine_type->variant.routine.extra_info->prototype_scope = sp;
      sp->variant.assoc_type = routine_type;
    }  /* if */
    if (sp != NULL) {
      /* Set the scope-stack-entry depth. */
      sp->depth_in_scope_stack = (scope_stack - ssep);
    } else {
      check_assertion_str(ssep->kind == (a_scope_kind)sck_pragma,
                          "ensure_il_scope_exists: NULL IL scope");
    }  /* if */
  }  /* if */
  return sp;
}  /* ensure_il_scope_exists */


void add_to_constants_list(a_constant_ptr con_ptr,
                           a_boolean      at_file_scope)
/*
Add the given constant to the constants list for the file scope (for
manifest constant macros and (at the end of compilation) for shareable
constants) or the current scope (for member constants, which are not shared).
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;

  /* Get pointer to current or file scope entry. */
  ssep = &scope_stack[at_file_scope ? DEPTH_OF_FILE_SCOPE : decl_scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  check_assertion_str(sp != NULL, "add_to_constants_list: NULL IL scope");
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
Its integer kind is as given by kind.  Note that the kind is not restricted
to be a signed kind.
*/
{
  clear_constant(cp, (a_constant_repr_kind)ck_integer);
  cp->type = integer_type(kind);
  set_integer_value(&cp->variant.integer_value, value);
}  /* set_integer_constant */


void set_unsigned_integer_constant(a_constant      *cp,
                                   unsigned long   value,
                                   an_integer_kind kind)
/*
Set the constant entry *cp to the integer constant given by value.
Its integer kind is as given by kind.  Note that the kind is not restricted
to be an unsigned kind.
*/
{
  clear_constant(cp, (a_constant_repr_kind)ck_integer);
  cp->type = integer_type(kind);
  set_unsigned_integer_value(&cp->variant.integer_value, value);
}  /* set_unsigned_integer_constant */


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
                       /*constant_context=*/TRUE,
                       /*evaluated_context=*/TRUE,
                       /*fold_constant_addr_exprs=*/TRUE,
                       &did_not_fold, &error_position);
}  /* make_zero_of_proper_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_enum_constant(a_constant_ptr con)
/*
Return TRUE if the indicated constant is an enum constant, i.e., it is
a constant that appears on the constant list of an enum type.
*/
{
  a_boolean is_enum = FALSE;

  if (con->kind == (a_constant_repr_kind)ck_integer && has_name(con)) {
    /* The constant is a named constant with an integral representation. */
    a_type_ptr con_type = con->type;
    if (con_type->kind == (a_type_kind)tk_integer) {
      /* The constant has an integral or enum type. */
      /* In C, enumerators have "int" type (but an affiliated type that
         is the enumeration); in C++, enumerators have the enum type. */
      if (il_header.source_language == sl_C ?
          (!con_type->variant.integer.enum_type &&
           con_type->variant.integer.enum_info.affiliated_type != NULL) :
          con_type->variant.integer.enum_type) {
        is_enum = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_enum;
}  /* is_enum_constant */


a_boolean is_wide_string_constant(a_constant_ptr constant)
/*
Return TRUE if the indicated constant is a wide string constant (L"abc").
*/
{
  a_boolean  is_wide_string = FALSE;
  a_type_ptr con_type, elem_type;

  if (constant->kind == (a_constant_repr_kind)ck_string) {
    con_type = skip_typerefs(constant->type);
    elem_type = con_type->variant.array.element_type;
    elem_type = skip_typerefs(elem_type);
    /* Check for element type that is not some variety of char. */
    is_wide_string = !is_character_type(elem_type);
  }  /* if */
  return is_wide_string;
}  /* is_wide_string_constant */

#if !STANDALONE_UTILITY_PROGRAM

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


void set_arg_transfer_method_flag(a_param_type_ptr   ptp,
                                  a_source_position  *err_pos)
/*
Set the flag in the indicated parameter type entry to indicate whether or
not the parameter should be passed using a copy constructor.  Also issue a
diagnostic if the parameter type is an abstract class.
*/
{
  a_type_ptr param_type;

  /* Do not set the flag in C mode.  Also, once the flag is set to TRUE
     it can never be reset. */
  if (C_dialect == C_dialect_cplusplus && !ptp->passed_via_copy_constructor) {
    param_type = ptp->type;
    param_type = skip_typerefs(param_type);
    if (is_immediate_class_type(param_type)) {
      /* The parameter is a class passed by value.  See if the class
         has a "real" copy constructor. */
      if (is_incomplete_type(param_type)) {
        /* Delay setting the flag till the class is defined -- add it to the
           fixup list. */
        add_to_dependent_type_fixup_list(param_type,
                                         (a_dependent_type_fixup_kind)
                                                 dtfk_arg_transfer_method,
                                         (char *)ptp,
                                         (a_byte_il_entry_kind)iek_param_type,
                                         err_pos);
      } else {
        a_class_symbol_supplement_ptr cssp =
                                       symbol_supplement_for_class(param_type);
        if (!cssp->construction_by_bitwise_copy_allowed ||
            (!any_cfront_mode() && cssp->destructor != NULL)) {
          /* The class has a "real" copy constructor, or it has a
             destructor, so a copy of an object of this class type must be
             made when it is passed as an argument. */
          ptp->passed_via_copy_constructor = TRUE;
          /* If the parameter type is an abstract class, issue an error.  Note
             that construction_by_bitwise_copy_allowed will never be TRUE
             for abstract classes.  Also note that this logic assumes that
             passed_via_copy_constructor will never be set elsewhere. */
          if (param_type->variant.class_struct_union.abstract) {
            if (err_pos->seq == 0) {
              /* A null error position indicates a parameter type for which
                 there is no corresponding source position -- e.g., a type
                 is being copied for some reason.  Issue no diagnostic in
                 such cases. */
            } else {
              pos_error(ec_abstract_class_param_type, err_pos);
            }  /* if */
          }  /* if */
        }  /* if */
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
#if CHECKING
  ptp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  ptp->default_arg_expr = NULL;

  db_exit();
  return ptp;
}  /* alloc_param_type */


a_param_type_ptr make_param_type(a_type_ptr         tp,
                                 a_source_position  *decl_pos)
{
  a_param_type_ptr  ptp;

  ptp = alloc_param_type(tp);
  set_arg_transfer_method_flag(ptp, decl_pos);
  return ptp;
}  /* make_param_type */


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
  db_exit();
  return bcdp;
}  /* alloc_base_class_derivation */


a_base_class_derivation_ptr preferred_virtual_derivation_of(
                                                     a_base_class_ptr  bcp)
/*
Return a pointer to the base class derivation entry associated with base
class bcp that is marked "preferred", namely, the one with the greatest
accessibility of a public member in the context of the most derived class.
There must be a derivation so marked.
*/
{
  a_base_class_derivation_ptr  bcdp = bcp->derivation;

  while (!bcdp->preferred) {
    bcdp = bcdp->next;
    check_assertion_str(bcdp != NULL,
                   "preferred_virtual_derivation_of: no preferred derivation");
  }  /* while */
  return bcdp;
}  /* preferred_virtual_derivation_of */


a_base_class_derivation_ptr direct_virtual_derivation_of(a_base_class_ptr  bcp)
/*
bcp is virtual base class assumed to have its direct flag set to TRUE.  Return
a pointer to the associated base class derivation entry that also has direct
set to TRUE.
*/
{
  a_base_class_derivation_ptr  bcdp = bcp->derivation;

  while (!bcdp->direct) {
    bcdp = bcdp->next;
    /* Assertion will fail if direct flag has not been set. */
    check_assertion_str(bcdp != NULL,
                       "direct_virtual_derivation_of: no direct derivation");
  }  /* while */
  return bcdp;
}  /* direct_virtual_derivation_of */


a_derivation_step_ptr cast_virtual_derivation_path_of(a_base_class_ptr bcp)
/*
Return the derivation path that should be used for a cast to the (virtual)
base class bcp.  This path has a single step to the virtual base class.
*/
{
  a_derivation_step_ptr dsp;

  /* Step to the last derivation step on the path of the first derivation.
     The last step is always to the virtual base class. */
  for (dsp = bcp->derivation->path; dsp->next != NULL; dsp = dsp->next) {}
  return dsp;
}  /* cast_virtual_derivation_path_of */


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

  return bcp;
}  /* alloc_base_class */


an_access_adjustment_ptr alloc_access_adjustment(an_il_entry_kind kind)
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
  aap->next        = NULL;
  aap->access      = (an_access_specifier)as_public;
  aap->entity.kind = (a_byte_il_entry_kind)kind;
  aap->entity.ptr  = (char *)NULL;

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
  ctsp->anonymous_union_kind              = (an_anonymous_union_kind)auk_none;
  ctsp->anonymous_union_field             = NULL;
  ctsp->access_adjustments                = NULL;
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
      rtsp->lint_varargs_count       = NOT_LINT_VARARGS;
      rtsp->arg_pragma               = (a_pragma_kind)pk_none;
      rtsp->implicit_this_param_type = NULL;
      rtsp->prototype_scope          = NULL;
      rtsp->exception_specification  = NULL;
      break;
    case tk_typeref:
      pte->variant.typeref.type        = NULL;
#if DO_IL_LOWERING
      pte->variant.typeref.orig_type   = NULL;
#endif /* DO_IL_LOWERING */
      pte->variant.typeref.is_const    = FALSE;
      pte->variant.typeref.is_volatile = FALSE;
      pte->variant.typeref.is_placeholder_for_file_scope_type = FALSE;
#if CHECKING
      pte->variant.typeref.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
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


static void clear_type(a_type_ptr  pte,
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


void add_to_types_list(a_type_ptr     type_ptr,
                       a_scope_depth  scope_level)
/*
Add the given type to the types list for the scope corresponding to
scope_level.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;

  /* Get a pointer to the current or file scope entry. */
  ssep = &scope_stack[scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  /* If we are currently inside the declaration list for the old-style
     parameters of a function -- e.g., in the "struct" declaration in

       int f(a) struct s {int b;} a; { }

     -- the type should be entered in the prototype scope; that makes the
     type available in the memory region of the function's parent, which is
     necessary for type-compatibility checking of parameters. A similar
     situation applies for type declarations that are part of function
     prototypes, as in

       int f(struct s {int b;} a);

     The prototype scope is hardly ever needed, and therefore it is not
     allocated by default.  It is allocated in ensure_il_scope_exists. */
  sp = ensure_il_scope_exists(ssep);
  if (sp == NULL) {
    /* May be an error case. */
  } else {
    /* Add the type to the list of types for this scope. */
    if (sp->types == NULL) {
      sp->types = type_ptr;
    } else {
      ssep->last_type->next = type_ptr;
    }  /* if */
    ssep->last_type = type_ptr;
  }  /* if */
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
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pit, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pit;
}  /* integer_type */


a_type_ptr signed_integer_type(an_integer_kind kind)
/*
Make or find a type entry for an explicitly signed integer type of the
indicated kind, and return a pointer to it.  Keeping track of the difference
between, e.g., a plain "int" and a "signed int" is necessary because the two
may be handled differently for bit fields.  Should only be called for kinds
ik_short, ik_int, ik_long, and ik_long_long.
*/
{
  a_type_ptr pit;

  if (signed_int_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pit = signed_int_types[kind];
  } else {
    /* The type must be created. */
#if CHECKING
    if (kind != (an_integer_kind)ik_short &&
        kind != (an_integer_kind)ik_int &&
        kind != (an_integer_kind)ik_long
#if LONG_LONG_ALLOWED
        && kind != (an_integer_kind)ik_long_long
#endif /* LONG_LONG_ALLOWED */
                                                ) {
      internal_error("signed_integer_type: bad int kind");
    }  /* if */
#endif /* CHECKING */
    signed_int_types[kind] = pit = alloc_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = kind;
    pit->variant.integer.explicitly_signed = TRUE;
    set_type_size(pit);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pit, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pit;
}  /* signed_integer_type */


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
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pft, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
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
    pst->variant.array.variant.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      string_types[num_chars] = pst;
#if ORPHAN_PROCESSING_NEEDED
      /* Record the type entry as an orphan in case it is discarded now
         and then found again in a later phase (e.g., IL lowering). */
      add_orphaned_file_scope_il_entry((char *)pst,
                                       (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
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
                          integer_type((an_integer_kind)targ_wchar_t_int_kind);
    pst->variant.array.variant.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      wide_string_types[num_chars] = pst;
#if ORPHAN_PROCESSING_NEEDED
      /* Record the type entry as an orphan in case it is discarded now
         and then found again in a later phase (e.g., IL lowering). */
      add_orphaned_file_scope_il_entry((char *)pst,
                                       (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
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
    /* The type is deliberately not recorded as an orphan.  If it were, it
       would be found by IL lowering. */
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
    /* Deliberately not recorded as an orphan. */
  }  /* if */
  return il_unknown_type;
}  /* unknown_type */


a_type_ptr void_type(void)
/*
Make or find a type entry for a void type, and return a pointer to it.
*/
{
  if (il_void_type == NULL) {
    il_void_type = alloc_type((a_type_kind)tk_void);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)il_void_type,
                                     (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return il_void_type;
}  /* void_type */


static a_type_ptr get_based_type(a_type_ptr        base_type,
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


static void add_based_type_list_member(a_type_ptr        base_type,
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

  class_type = skip_typerefs(class_type);
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


a_type_ptr related_member_type(a_type_ptr member_type,
                               a_type_ptr class_type)
/*
member_type is intended to be the member type under a pointer-to-member type
for the class class_type.  If member_type is a function type, check that
the "this" parameter type matches class_type.  If not, create a new function
type with the right underlying class type and return it.  In all other cases,
return the original member type.
*/
{
  a_type_ptr new_member_type, old_this_type, new_this_type;
  a_type_ptr old_this_underlying_type, old_this_underlying_class;

  if (is_function_type(member_type)) {
    /* Take apart the "this" parameter type. */
    old_this_type = skip_typerefs(member_type)->variant.routine.extra_info->
                                                      implicit_this_param_type;
    /* A function type under a pointer-to-member must be a member function
       and therefore must have a "this" parameter type. */
    check_assertion(old_this_type != NULL);
    old_this_underlying_type = type_pointed_to(old_this_type);
    old_this_underlying_class = skip_typerefs(old_this_underlying_type);
    if (old_this_underlying_class != class_type) {
      /* Make a new function type with the right "this" class.  Note that
         there is no sharing of types going on here, so this may be
         wasteful if called a lot. */
      /* Build a type for the new "this" parameter.  Start with the new class
         and build up, adding the qualifiers (both under and over the
         pointer type) from the old "this" type. */
      new_this_type = make_identically_qualified_type(class_type,
                                                     old_this_underlying_type);
      new_this_type = make_pointer_type(new_this_type);    
      new_this_type = make_identically_qualified_type(new_this_type,
                                                      old_this_type);
      /* Allocate the new function type and copy into it. */
      new_member_type = alloc_type((a_type_kind)tk_routine);
      copy_type(member_type, new_member_type);
      /* Insert the new "this" parameter type. */
      new_member_type->variant.routine.extra_info->implicit_this_param_type =
                                                                 new_this_type;
      member_type = new_member_type;
    }  /* if */
  }  /* if */
  return member_type;
}  /* related_member_type */


a_type_ptr related_ptr_to_member_type(a_type_ptr member_type,
                                      a_type_ptr class_type)
/*
Make a pointer-to-member type having the indicated member type and class
type and return a pointer to it.  If the member_type is a function type,
alter the underlying "this" parameter type if necessary to be the new
class_type.
*/
{
  a_type_ptr type;

  /* Make a new member type with an altered "this" parameter type,
     or leave the member type unchanged. */
  member_type = related_member_type(member_type, class_type);
  /* Make the pointer-to-member type. */
  type = ptr_to_member_type(member_type, class_type);
  return type;
}  /* related_ptr_to_member_type */


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
                       /*class_type=*/(a_type_ptr)NULL);
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
                       /*class_type=*/(a_type_ptr)NULL);
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


static a_type_ptr copy_array_type_replacing_element_type(
                                                      a_type_ptr  old_array,
                                                      a_type_ptr  element_type)
/*
old_array is an array type that needs to be copied, but with a new element
type (one that may, e.g., differ from the existing element type in its
type qualifiers).  Make the copy, allowing for multidimensional arrays,
and return a pointer to the new array type.
*/
{
  a_type_ptr  tp, new_array = NULL, prev = NULL;

  /* Loop, in case this is a multidimensional array. */
  for (;;) {
    /* Drop typedefs; there shouldn't be any typerefs. */
    old_array = skip_typerefs(old_array);
    /* Allocate a new array type and copy the old one into it.  Note that
       some of the source correspondence information should not be preserved
       in the copy. */
    tp = alloc_type((a_type_kind)tk_array);
    copy_type(old_array, tp);
    break_source_corresp(&tp->source_corresp);
    /* Either tp becomes the top of the new array type or is added on as
       a subarray. */
    if (new_array == NULL) {
      new_array = tp;
    } else {
      prev->variant.array.element_type = tp;
    }  /* if */
    prev = tp;
    /* Advance to the element type (which may be another array type). */
    old_array = old_array->variant.array.element_type;
    if (is_array_type(old_array)) {
      /* Keep looping. */
    } else {
      /* Done.  Attach the new element type to the bottom of the chain of
         array types. */
      tp->variant.array.element_type = element_type;
      break;
    }  /* if */
  }  /* for */
  return new_array;
}  /* copy_array_type_replacing_element_type */


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
  a_type_ptr        orig_base_type, ptr;
  a_based_type_kind kind;
  a_boolean         base_type_const_qualified, base_type_volatile_qualified;
  a_boolean         is_array = FALSE;
  a_boolean         add_const, add_volatile;

  orig_base_type = base_type;
  /* According to ANSI C 3.5.3: "If the specification of an array type
     includes any type qualifiers, the element type is so-qualified,
     not the array type.", and this is interpreted recursively
     for arrays of arrays.  The type qualifiers therefore apply
     to the ultimate element type.  This can only happen with typedefs,
     as in "typedef int A[2][3]; const A a;", which makes "a" an
     array of array of const int. */
  if (is_array_type(base_type)) {
    base_type = underlying_array_element_type(base_type);
    is_array = TRUE;
  }  /* if */
  base_type_const_qualified = is_const_qualified_type(base_type);
  base_type_volatile_qualified = is_volatile_qualified_type(base_type);
  add_const = is_const && !base_type_const_qualified;
  add_volatile = is_volatile && !base_type_volatile_qualified;
  if (add_const || add_volatile) {
    /* Some qualifiers need to be added. */
    if (base_type_const_qualified || base_type_volatile_qualified) {
      /* The typeref(s) containing qualifiers, if any, are removed to get down
         to the real base type, to which the new qualifiers are added.  When
         a qualifier is removed, a flag must be set so that it will be added
         back. */
      while (base_type->kind == (a_type_kind)tk_typeref) {
        if (!base_type->variant.typeref.is_const &&
            !base_type->variant.typeref.is_volatile) {
          /* This is a typedef -- preserve it, so that the qualifier is built
             on top of it. */
          break;
        }  /* if */
        if (base_type->variant.typeref.is_const) add_const = TRUE;
        if (base_type->variant.typeref.is_volatile) add_volatile = TRUE;
        base_type = base_type->variant.typeref.type;
      }  /* while */
    }  /* if */
    /* Determine the based type kind. */
    if (add_const) {
      if (add_volatile) {
        kind = (a_based_type_kind)btk_const_volatile;
      } else {
        kind = (a_based_type_kind)btk_const;
      }  /* if */
    } else {
      kind = (a_based_type_kind)btk_volatile;
    }  /* if */
    /* See if the properly qualified version of base_type already exists.
       If so, a pointer to it is stored in the based_types for base_type. */
    ptr = get_based_type(base_type, kind, /*class_type=*/(a_type_ptr)NULL);
    if (ptr == NULL) {
      /* No allocated entry, need to allocate one. */
      ptr = alloc_type((a_type_kind)tk_typeref);
      ptr->variant.typeref.type        = base_type;
      ptr->variant.typeref.is_const    = add_const;
      ptr->variant.typeref.is_volatile = add_volatile;
      /* Remember the existence of this typeref type by putting a pointer
         to it in the based_types list. */
      add_based_type_list_member(base_type, kind, ptr);
    }  /* if */
    if (is_array) {
      /* For the strange array case, the array type entries must be
         copied in order to avoid changing the typedef type. */
      ptr = copy_array_type_replacing_element_type(orig_base_type, ptr);
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
  a_type_ptr  element_type;

  /* Remove the minimum number of typerefs that will produce an unqualified
     type, in order to save typedefs if possible. */
  if (C_mode() || !is_array_type(type)) {
    while (is_top_level_qualified_type(type)) {
      type = type->variant.typeref.type;
    }  /* while */
  } else if (type->kind == (a_type_kind)tk_typeref) {
    element_type = make_unqualified_type(underlying_array_element_type(type));
    type = copy_array_type_replacing_element_type(type, element_type);
  }  /* if */
  return type;
}  /* make_unqualified_type */


void skip_common_type_qualifiers(a_type_ptr  *type1,
                                 a_type_ptr  *type2)
/*
If both *type1 and *type2 have any of the same type qualifiers, strip off
the common qualifiers but leave any other type qualifiers in place.  Both
types may be modified, or neither, or only one of the two.  If both original
types have qualifiers, skip_typerefs is called to get down to the base
types, and then new types may be built up from them; this may result in
discarding typedefs.
*/
{
  a_boolean  type1_is_const, type1_is_volatile;
  a_boolean  type2_is_const, type2_is_volatile;
  a_boolean  is_const, is_volatile;
  a_type_ptr  tp1 = *type1, tp2 = *type2;

  if (is_qualified_type(tp1) && is_qualified_type(tp2)) {
    /* Both types have type qualifiers.  Record exactly how they are
       qualified. */
    type1_is_const = is_const_qualified_type(tp1);
    type1_is_volatile = is_volatile_qualified_type(tp1);
    type2_is_const = is_const_qualified_type(tp2);
    type2_is_volatile = is_volatile_qualified_type(tp2);
    /* Strip off the qualifiers. */
    tp1 = skip_typerefs(tp1);
    tp2 = skip_typerefs(tp2);
    if (type1_is_const == type2_is_const &&
        type1_is_volatile == type2_is_volatile) {
      /* Either both are const, both are volatile, or both are const volatile.
         Return both types with all qualifiers stripped off. */
    } else {
      /* They are differently qualified.  The qualifiers have been stripped
         off; add them back on as appropriate. */
      is_const = type1_is_const && !type2_is_const;
      is_volatile = type1_is_volatile && !type2_is_volatile;
      if (is_const || is_volatile) {
        tp1 = make_qualified_type(tp1, is_const, is_volatile);
      }  /* if */
      is_const = !type1_is_const && type2_is_const;
      is_volatile = !type1_is_volatile && type2_is_volatile;
      if (is_const || is_volatile) {
        tp2 = make_qualified_type(tp2, is_const, is_volatile);
      }  /* if */
    }  /* if */
    *type1 = tp1;
    *type2 = tp2;
  }  /* if */
}  /* skip_common_type_qualifiers */


void copy_type(a_type_ptr from,
               a_type_ptr to)
/*
Copy the type entry "from" to "to".
*/
{
  a_type_kind                   from_kind;
  a_routine_type_supplement_ptr extra_info;
  a_type_ptr                    next_ptr, tp;

  from_kind = from->kind;
  if (from_kind == (a_type_kind)tk_routine) {
    /* For a routine type, preserve the type supplement pointer for the
       copy below. */
    extra_info = to->variant.routine.extra_info;
  }  /* if */
  /* Preserve the "next" pointer in the "to" entry. */
  next_ptr = to->next;
  /* Copy the type entry. */
  *to = *from;
  to->next = next_ptr;
  to->based_types = NULL;
  if (from_kind == (a_type_kind)tk_array ||
      from_kind == (a_type_kind)tk_routine) {
    if (from_kind == (a_type_kind)tk_routine) {
      /* For a routine type, the type supplement must also be copied. */
      *extra_info = *from->variant.routine.extra_info;
      to->variant.routine.extra_info = extra_info;
      tp = skip_typerefs(to->variant.routine.return_type);
    } else {
      tp = skip_typerefs(underlying_array_element_type(to));
    }  /* if */
    if (is_incomplete_type(tp) && is_immediate_class_type(tp)) {
      /* An array type is placed on a fixup list if the underlying element
         type is an incomplete class type (allowed by extension), and a
         routine type is placed on the list if its return type is an
         incomplete class type. */
      /* Pass the NULL source position since no errors should be issued about
         this type. */
      add_to_dependent_type_fixup_list(tp,
                                       (a_dependent_type_fixup_kind)
                                                  dtfk_array_type_size,
                                       (char *)to,
                                       (a_byte_il_entry_kind)iek_type,
                                       &null_source_position);
    }  /* if */
  }  /* if */
}  /* copy_type */


void copy_routine_type_with_param_types(a_type_ptr from_type,
                                        a_type_ptr to_type)
/*
Make a copy of a routine type and its param types list.  This routine is
called in cases where a routine type and its copy may not share the same
param-types list (for example, when as the result of a user error a routine
type in a function definition is based on a typedef).
*/
{
  a_param_type_ptr  old_ptp, new_ptp, prev_new_ptp;

  copy_type(from_type, to_type);
  old_ptp = from_type->variant.routine.extra_info->param_type_list;
  prev_new_ptp = NULL;
  for (; old_ptp != NULL; old_ptp = old_ptp->next) {
    /* Pass a NULL source position to make_param_type to avoid inappropriate
       diagnostics on a type that doesn't correspond directly to a source
       construct. */
    new_ptp = make_param_type(old_ptp->type, &null_source_position);
    /* Do a struct copy from the old param type to the new. */
    *new_ptp = *old_ptp;
    /* Expressions may not be shared -- that is, they may not be pointed to
       from more than one place.  Therefore a copy must be made of the
       expression node for the default arg (if one exists). */
    if (old_ptp->default_arg_expr != NULL) {
      new_ptp->default_arg_expr = copy_expr_tree(old_ptp->default_arg_expr);
    }  /* if */
    if (prev_new_ptp == NULL) {
      to_type->variant.routine.extra_info->param_type_list = new_ptp;
    } else {
      prev_new_ptp->next = new_ptp;
    }  /* if */
    prev_new_ptp = new_ptp;
  }  /* for */
}  /* copy_routine_type_with_param_types */


a_boolean is_default_constructor(a_routine_ptr  ctor_rout)
/*
ctor_rout points to a routine entry for a constructor.   Return TRUE if it
points to a default constructor routine entry.
*/
{
  a_param_type_ptr  ptp;

  check_assertion(ctor_rout->special_kind ==
                                  (a_special_function_kind)sfk_constructor);
  ptp = ctor_rout->type->variant.routine.extra_info->param_type_list;
  /* There are no parameters or if the first (and therefore its successors,
     if any) has a default argument expression, then this is a default
     constructor. */
  return (ptp == NULL || ptp->has_default_arg);
}  /* is_default_constructor */


a_boolean is_copy_constructor(a_routine_ptr  ctor_rout,
                              a_type_ptr     class_of_which_a_member,
                              a_boolean      *const_object_okay,
                              a_boolean      *volatile_object_okay)
/*
Return TRUE if ctor_rout points to a copy constructor routine entry for
class_of_which_a_member; if it does, also set and return *const_object_okay
and/or *volatile_object_okay, depending on whether the type of the copy
constructor's first parameter is const or volatile qualified (or both).
*/
{
  a_param_type_ptr  ptp;
  a_type_ptr        tp;
  a_boolean         is_cctor = FALSE;

  check_assertion(ctor_rout->special_kind ==
                                  (a_special_function_kind)sfk_constructor);
  *const_object_okay = FALSE;
  *volatile_object_okay = FALSE;
  /* A constructor is deemed a copy constructor if (1) the type of the first
     parameter is reference-to-class or reference-to-const-class where
     "class" is the class of which it is a member function, and (2) where
     the function can be called with only one argument. */
  ptp = ctor_rout->type->variant.routine.extra_info->param_type_list;
  /* If the param type entry is non-NULL there is at least one argument.  If
     there is a second argument and it has a default expression, the function
     call need not explicitly mention the second argument. */
  if (ptp != NULL && is_reference_type(ptp->type) &&
      (ptp->next == NULL || ptp->next->has_default_arg)) {
    tp = type_pointed_to(ptp->type);
    if (skip_typerefs(tp) == class_of_which_a_member) {
      /* It is a copy constructor. */
      is_cctor = TRUE;
      /* See if the object being copied is const qualified. */
      if (tp->kind == (a_type_kind)tk_typeref) {
        if (f_is_const_qualified_type(tp, /*top_level=*/TRUE)) {
          *const_object_okay = TRUE;
        }  /* if */
        if (f_is_volatile_qualified_type(tp, /*top_level=*/TRUE)) {
          *volatile_object_okay = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_cctor;
}  /* is_copy_constructor */


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
  dip->next       = NULL;
  dip->variable   = NULL;
  dip->destructor = NULL;
  dip->follows_an_exec_statement = FALSE;
#if CHECKING
  dip->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
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


void add_to_dynamic_inits_list(a_dynamic_init_ptr dip)
/*
Add the given dynamic initialization entry to the file-scope dynamic_inits
list.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;

  /* Only the file scope has a dynamic-inits list -- in function and block
     scopes dynamic initialization is handled by statements. */
  ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
  sp = ssep->il_scope;
  if (sp->dynamic_inits == NULL) {
    sp->dynamic_inits = dip;
  } else {
    ssep->last_dynamic_init->next = dip;
  }  /* if */
  ssep->last_dynamic_init = dip;
  dip->next = NULL;
}  /* add_to_dynamic_inits_list */


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
  switch (dip->kind) {
    case dik_none:
    case dik_zero:
      break;
    case dik_expression:
    case dik_call_returning_class_via_cctor:
      new_dip->variant.expression = copy_expr_tree(dip->variant.expression);
      break;
    case dik_constructor:
      new_dip->variant.constructor.args =
                        copy_list_of_expr_trees(dip->variant.constructor.args);
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
      /* The constant pointed to is unshared and must be copied. */
      new_dip->variant.constant =
                                 copy_unshared_constant(dip->variant.constant);
      break;
#if CHECKING
    case dik_bitwise_copy:
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
#if DO_IL_LOWERING
  vp->initialization_rewritten_as_assignment = FALSE;
#endif /* DO_IL_LOWERING */
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


void remove_from_variables_list(a_variable_ptr var_ptr)
/*
Unlink the given variable from the variables list for the file scope.  This
is done so the variable can be added again at the end of the list, to keep
the variables in order of appearance of their definitions.
*/
{
  a_variable_ptr           prev_var, vp;
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  /* Get pointer to the file scope entry. */
  ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
  sp = ssep->il_scope;
  check_assertion_str(sp != NULL, "remove_from_variables_list: NULL IL scope");
  /* Find the variable on the current list that precedes var_ptr; we'll need
     it to unlink var_ptr. */
  prev_var = NULL;
  for (vp = sp->variables; vp != NULL; vp = vp->next) {
    if (vp == var_ptr) break;
    prev_var = vp;
  }  /* for */
#if CHECKING
  check_assertion_str(vp != NULL, "remove_from_variables_list: not found");
#endif /* CHECKING */
  /* Link the previous entry to the entry following this one. */
  if (prev_var == NULL) {
    sp->variables = var_ptr->next;
  } else {
    prev_var->next = var_ptr->next;
  }  /* if */
  /* If the entry being removed was the last on the list, update the
     last_variable pointer. */
  if (var_ptr == ssep->last_variable) {
    ssep->last_variable = prev_var;
  }  /* if */
}  /* remove_from_variables_list */


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
    if (ssep->kind == (a_scope_kind)sck_func_prototype) {
      /* This is an error case in which a variable is created in an old-style
         param declaration for which there was no corresponding param-id
         declaration.  Such variables should not be added to the variables
         list anyway. */
      sp = NULL;
    } else {
      /* Create the IL scope if necessary (for block scopes). */
      sp = ensure_il_scope_exists(ssep);
      check_assertion_str(sp != NULL, "add_to_variables_list: NULL IL scope");
    }  /* if */
  }  /* if */
  if (sp != NULL) {
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
      /* Variables with nonstatic storage will be allocated in the file scope
         memory region only when the scope is a function prototype scope (i.e.,
         in an error case). */
      if (ssep->kind != (a_scope_kind)sck_func_prototype &&
          in_file_scope(var_ptr)) {
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
  }  /* if */
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


a_variable_ptr make_handler_parameter(a_type_ptr  type_ptr)
/*
Create a handler parameter variable with the specified type, add it to the
parameter field of the current block scope, and return a pointer to it.
*/
{
  a_variable_ptr           vp;

  db_enter(5, "make_handler_parameter");
  /* Allocate the variable. */
  vp = alloc_variable((a_storage_class)sc_auto);
  vp->type = type_ptr;
  vp->is_handler_param = TRUE;
  /* Add it to the scope entry. */
  add_to_variables_list(vp, /*at_file_scope=*/FALSE);

  db_exit();
  return vp;
}  /* make_handler_parameter */


a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type)
/*
Make a temporary variable whose type is temp_type.  Return a pointer to it.
*/
{
  a_variable_ptr  temp_var;
  a_boolean       at_file_scope;
  a_storage_class storage_class;

  /* Typically, a temporary variable will have automatic storage class,
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
  set_default_source_corresp(fp->source_corresp);
  fp->next                 = NULL;
  fp->type                 = NULL;
  fp->offset               = 0;
  fp->offset_bit_remainder = 0;
  fp->bit_size             = 0;
  fp->is_bit_field         = FALSE;
  fp->bit_field_is_signed  = FALSE;
  fp->is_anonymous_parent_object = FALSE;
#if CHECKING
  fp->avoid_codecenter_warnings = 0;
#endif /*CHECKING */

  db_exit();
  return fp;
}  /* alloc_field */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_field_ptr next_initializable_field(a_field_ptr field)
/*
Given a pointer to a field (or NULL), return a pointer to the first field
at or after the given field that is initializable.  Unnamed bit fields,
for example, are not initializable, and are skipped by initialization
processing.  If there is no next such field, return NULL.
*/
{
  for (; field != NULL; field = field->next) {
    /* Named fields are initializable. */
    if (has_name(field)) break;
    /* Anonymous unions are also initializable in C++.  An extension allows
       anonymous parent objects in C too. */
    if (field->is_anonymous_parent_object) break;
  }  /* for */
  return field;
}  /* next_initializable_field */

#if !STANDALONE_UTILITY_PROGRAM

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
  check_assertion_str(sp != NULL, "add_to_routines_list: NULL IL scope");
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
  set_default_source_corresp(ap->source_corresp);
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
  check_assertion_str(sp != NULL, "add_to_asm_entries_list: NULL IL scope");
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
  set_default_source_corresp(lp->source_corresp);
  lp->source_corresp.is_local_to_function = TRUE;
  lp->next = NULL;
  lp->reachable_by_fall_through = TRUE;
  lp->break_label = FALSE;
  lp->continue_label = FALSE;
#if CHECKING
  lp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
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
  a_new_delete_supplement_ptr ndsp;
  a_throw_supplement_ptr      tsp;

  node->kind = kind;
  switch (kind) {
    case enk_error:
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
      ndsp->type         = NULL;
      ndsp->routine      = NULL;
      ndsp->arg          = NULL;
      ndsp->dynamic_init = NULL;
      ndsp->delete_routine = NULL;
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
      tsp->accessible_base_classes = NULL;
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
  node->result_is_not_used = FALSE;
  node->implicit_reference_indirection = FALSE;
#ifdef FIL
  node->allow_reordering = FALSE;
#endif /* ifdef FIL */
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


void set_expr_result_not_used(an_expr_node_ptr node)
/*
Mark the given node to indicate that its result is not used, i.e., that
the value of the expression is discarded.
*/
{
  node->result_is_not_used = TRUE;
  /* For some operations, subnodes get marked too.  This is only done with
     operations that have void type, because otherwise we run into problems
     if we rewrite the subnodes thinking they're top-level nodes -- it changes
     the subnode type without changing the parent node type (or the sibling
     node type, in the "?" case). */
  if (node->kind == (an_expr_node_kind)enk_operation &&
      is_void_type(node->type)) {
    an_expr_operator_kind op = node->variant.operation.kind;
    an_expr_node_ptr      operand_1 = node->variant.operation.operands;

    if (op == (an_expr_operator_kind)eok_comma) {
      /* Given a comma operation, the second operand is not used if the
         entire operation is not used. */
      set_expr_result_not_used(operand_1->next);
    } else if (op == (an_expr_operator_kind)eok_question) {
      /* Given a question mark operation, the second and third operands
         are not used if the entire operation is not used. */
      set_expr_result_not_used(operand_1->next);
      set_expr_result_not_used(operand_1->next->next);
    }  /* if */
  }  /* if */
}  /* set_expr_result_not_used */
  

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
  node->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
  node->variant.operation.operands = operands;
  if (kind == (an_expr_operator_kind)eok_comma) {
    /* The value of the first operand of a comma operator is not used. */
    set_expr_result_not_used(operands);
  }  /* if */
  /* The result_is_not_used flag of the node is preserved (it depends on
     what's pointing to this node rather than the node itself).  However,
     the subnodes (newly attached) may need to be flagged.  This comes up
     when existing expression trees are being changed, as in IL lowering. */
  if (node->result_is_not_used) {
    set_expr_result_not_used(node);
  }  /* if */
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
Allocate a copy of an expression node and return a pointer to it.
*/
{
  an_expr_node_ptr            expr_copy;
  an_expr_node_kind           kind = expr->kind;
  a_new_delete_supplement_ptr copy_new_delete;

  expr_copy = alloc_expr_node(kind);
  /* Preserve the new/delete supplement pointer if there is one. */
  if (kind == (an_expr_node_kind)enk_new_delete) {
    copy_new_delete = expr_copy->variant.new_delete;
  }  /* if */
  /* Copy the node. */
  *expr_copy = *expr;
  expr_copy->next = NULL;
  expr_copy->result_is_not_used = FALSE;
  if (kind == (an_expr_node_kind)enk_new_delete) {
    /* Copy the new/delete supplement. */
    *copy_new_delete = *expr->variant.new_delete;
    expr_copy->variant.new_delete = copy_new_delete;
  }  /* if */
  return expr_copy;
}  /* copy_node */


an_expr_node_ptr copy_list_of_expr_trees(an_expr_node_ptr expr_list)
/*
Make a copy of a list of expression trees and return a pointer to it.
*/
{
  an_expr_node_ptr expr, expr_copy, prev_expr_copy, expr_list_copy;

  expr_list_copy = prev_expr_copy = NULL;
  for (expr = expr_list; expr != NULL; expr = expr->next) {
    expr_copy = copy_expr_tree(expr);
    if (expr_list_copy == NULL) {
      expr_list_copy = expr_copy;
    } else {
      prev_expr_copy->next = expr_copy;
    }  /* if */
    prev_expr_copy = expr_copy;
  }  /* for */
  return expr_list_copy;
}  /* copy_list_of_expr_trees */


an_expr_node_ptr copy_expr_tree(an_expr_node_ptr expr)
/*
Make a copy of an expression tree and return a pointer to it.
*/
{
  an_expr_node_kind           kind = expr->kind;
  an_expr_node_ptr            expr_copy;
  a_new_delete_supplement_ptr ndsp, copy_ndsp;

  /* Copy the top node. */
  expr_copy = copy_node(expr);
  if (kind == (an_expr_node_kind)enk_operation) {
    /* Copy the operands of the operation. */
    expr_copy->variant.operation.operands =
                     copy_list_of_expr_trees(expr->variant.operation.operands);
    if (expr->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
      /* The value of the first operand of a comma operator is not used. */
      set_expr_result_not_used(expr_copy->variant.operation.operands);
    }  /* if */
  } else if (kind == (an_expr_node_kind)enk_temp_init) {
    /* Copy the dynamic init for a dynamic initialization. */
    expr_copy->variant.init.dynamic_init =
                            copy_dynamic_init(expr->variant.init.dynamic_init);
  } else if (kind == (an_expr_node_kind)enk_new_delete) {
    /* Copy the subtree and dynamic init for a new/delete operation. */
    /* Note that the new/delete supplement was copied by copy_node. */
    ndsp = expr->variant.new_delete;
    copy_ndsp = expr_copy->variant.new_delete;
    if (ndsp->arg != NULL) {
      copy_ndsp->arg = copy_list_of_expr_trees(ndsp->arg);
    }  /* if */
    if (ndsp->dynamic_init != NULL) {
      copy_ndsp->dynamic_init = copy_dynamic_init(ndsp->dynamic_init);
    }  /* if */
  }  /* if */
  return expr_copy;
}  /* copy_expr_tree */


an_expr_node_ptr copy_default_arg_expr_list(a_param_type_ptr ptp)
/*
Make an expression list containing copies of the default argument expressions
for the parameter indicated by ptp and all parameters following that.
If ptp is non-NULL, it must point to a parameter with a default argument
expression.
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
        arg_node = copy_expr_tree(ptp->default_arg_expr);
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


an_expr_node_ptr function_addr_expr(a_routine_ptr rout,
                                    a_boolean     set_address_taken_flag)
/*
Build an expression node that represents the address of the function rout
and return a pointer to it.  If set_address_taken_flag is TRUE, the
address_taken flag in the routine is set (it's not always set because
the node might be used in a way that doesn't really take the address
of the routine, e.g., to call the routine).
*/
{
  an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_routine_address);
  node->type = make_pointer_type(rout->type);
  node->variant.routine = rout;
  if (set_address_taken_flag) rout->address_taken = TRUE;
  return node;
}  /* function_addr_expr */


an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node)
/*
Add an indirection on top of the given node (or make a change that produces
the same effect), and return a pointer to the new expression.
*/
{
  if (is_variable_address_node(node)) {
    /* Address of variable becomes value of variable. */
    node->kind = (an_expr_node_kind)enk_variable;
    node->type = node->variant.variable->type;
  } else if (is_error_node(node)) {
    /* Error node -- leave alone. */
  } else {
    /* For other cases, add an indirection operator. */
    /* Drop type qualifiers from the type, because (a) they don't apply to
       rvalues, and (b) when a pointer is used for an lvalue, it has
       no top-level qualifiers.  An IL shorthand allows them to be dropped
       without a cast. */
    a_type_ptr new_type = type_pointed_to(node->type);
    new_type = make_unqualified_type(new_type);
    node->next = NULL;
    node = make_operator_node((an_expr_operator_kind)eok_indirect,
                              new_type, node);
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
  expr = var_rvalue_expr(this_param_var);
  return expr;
}  /* this_param_value_expr */


an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                             a_field_ptr      field)
/*
Make an expression for an lvalue reference to field "field" of "node" and
return a pointer to it.  Note that this does NOT add extra intermediate
selections for anonymous unions.
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
  op = (field->is_bit_field) ? (an_expr_operator_kind)eok_bit_field :
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
return a pointer to it.  Note that this does NOT add extra intermediate
selections for anonymous unions.
*/
{
  /* Make the expression node for an lvalue reference. */
  node = field_lvalue_selection_expr(node, field);
  /* Add an indirection to turn the lvalue into an rvalue.  That also
     drops any type qualifiers. */
  node = add_indirection_to_node(node);
  return node;
}  /* field_rvalue_selection_expr */

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS || DO_IL_LOWERING

void adjust_anonymous_union_field_selection(an_expr_node_ptr node,
                                            a_field_ptr      au_field)
/*
node is a field selection of some kind that selects an anonymous union
field out of an anonymous parent object (or a similar C case allowed
as an extension).  au_field is the field for the anonymous parent object.
Insert an additional selection on the first operand, e.g., change
"x.y" to "x.au_field.y".  If a multi-level anonymous union is involved,
this routine handles only one level; the caller must loop to handle the
rest.
*/
{
  an_expr_node_ptr      op1, op2, new_op1, au_field_node;
  an_expr_operator_kind op, new_op;
  a_type_ptr            new_selection_type;

  op1 = node->variant.operation.operands;
  op2 = op1->next;
  /* If the original field selection takes an lvalue as its first operand,
     the added field selection is an eok_field; if the original field
     selection takes an rvalue as its first operand, the added field
     selection is an eok_value_field.  Here are the operators:
                               in       out
       eok_field             lvalue   lvalue
       eok_value_field       rvalue   rvalue
       eok_bit_field         lvalue   lvalue
       eok_value_bit_field   rvalue   rvalue
       eok_extract_bit_field lvalue   rvalue
     Note that eok_field and eok_value_field produce as output that is
     the same as their input, which is why they are used for the added
     field selection -- whatever the first operand of the original field
     was, it's preserved by adding the right one of those two selections. */
  op = node->variant.operation.kind;
  new_selection_type = au_field->type;
  if (op == (an_expr_operator_kind)eok_value_field ||
      op == (an_expr_operator_kind)eok_value_bit_field) {
    /* These operators take an rvalue as their input, so use an
       eok_value_field for the added field selection. */
    new_op = (an_expr_operator_kind)eok_value_field;
  } else {
    /* These operators take an lvalue as their input, so use an
       eok_field for the added field selection. */
    new_op = (an_expr_operator_kind)eok_field;
    new_selection_type = make_pointer_type(new_selection_type);
  }  /* if */
  au_field_node = alloc_expr_node((an_expr_node_kind)enk_field);
  au_field_node->type = au_field->type;
  au_field_node->variant.field = au_field;
  op1->next = au_field_node;
  new_op1 = make_operator_node(new_op, new_selection_type, op1);
  /* Attach the new selection to the original selection. */
  new_op1->next = op2;
  node->variant.operation.operands = new_op1;
}  /* adjust_anonymous_union_field_selection */

#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS || DO_IL_LOWERING */

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
  for (dsp = cast_derivation_path_of(bcp);
       dsp != NULL;
       dsp = dsp->next) {
    node = make_operator_node((an_expr_operator_kind)eok_base_class_cast,
                              make_pointer_type(dsp->base_class->type), node);
  }  /* for */
  return node;
}  /* base_class_selection_expr */


a_dynamic_init_ptr alloc_dtor_dynamic_init(
                             a_dynamic_init_kind kind,
                             a_type_ptr          type,
                             a_boolean           evaluated,
                             a_boolean           in_return_by_cctor_expression,
                             a_source_position   *position)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind, and return a pointer to it.  If type is a type that
requires a destructor, put the destructor routine pointer into the dynamic
initialization entry.  *position gives the associated source position.
If evaluated is FALSE, the reference is within an unevaluated expression.
If in_return_by_cctor_expression is TRUE, the reference is within the
expression of a return statement in a routine that returns its value by
calling a copy constructor.
*/
{
  a_dynamic_init_ptr dip = alloc_dynamic_init(kind);

  if (C_dialect == C_dialect_cplusplus && is_class_struct_union_type(type)) {
    /* The type is a class.  If it has a destructor, indicate it in
       the dynamic initialization. */
    if (!in_return_by_cctor_expression) {
      dip->destructor = select_destructor(type, type, position,
                                          /*honor_virtual=*/FALSE, evaluated,
                                          /*suppress_access_check=*/FALSE);
    } else {
      /* In a cctor return expression.  Put the destructor in the entry,
         but do not do the access checking etc. at this time.  Build a fixup
         entry to remind us to do the check later, and put it on the list
         for the current expression. */
      a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
      if (cssp != NULL) {
        a_symbol_ptr dtor_sym = cssp->destructor;
        if (dtor_sym != NULL) {
          dip->destructor = dtor_sym->variant.routine.ptr;
          (void)alloc_dynamic_init_dtor_fixup(dip, position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return dip;
}  /* alloc_dtor_dynamic_init */


an_expr_node_ptr alloc_temp_init_node(a_type_ptr temp_type,
                                      a_boolean  result_is_addr)
/*
Create an enk_temp_init node and return a pointer to it.  The implied
temporary has type temp_type.  The value of the enk_temp_init is the address
(rather than the value) of the temporary if result_is_addr is TRUE.
No dynamic initialization entry is attached under the node (the caller
must do that).
*/
{
  an_expr_node_ptr         temp_init_node;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];

  temp_init_node = alloc_expr_node((an_expr_node_kind)enk_temp_init);
  temp_init_node->variant.init.result_is_addr = result_is_addr;
  if (result_is_addr) {
    /* The result is the address of the temporary, so the type is a pointer
       to the type of the temporary. */
    temp_init_node->type = make_pointer_type(temp_type);
  } else {
    /* The result is the value of the temporary, so the type is the type
       of the temporary. */
    temp_init_node->type = skip_typerefs(temp_type);
  }  /* if */
  /* Make sure the IL scope that the temporary is part of exists.  Even though
     the temporary does not exist as a variable, it's still (from a language
     point of view) part of this scope.  That's important, because it has to
     be destroyed at the right point.  (Note, however, that when a temp is
     created for a default argument in the context of a function prototype
     scope, no IL scope will be created; that's okay, since the expression
     will be copied in a context that will have an IL scope.) */
  if (ssep->kind != (a_scope_kind)sck_func_prototype) {
    (void)ensure_il_scope_exists(ssep);
  }  /* if */
  return temp_init_node;
}  /* alloc_temp_init_node */


an_expr_node_ptr create_expr_temporary(
                               a_type_ptr        temp_type,
                               a_boolean         result_is_addr,
                               a_boolean         evaluated,
                               a_boolean         in_return_by_cctor_expression,
                               a_source_position *position)
/*
Create an enk_temp_init node and return a pointer to it.  The implied
temporary has type temp_type.  A dynamic initialization entry indicating
no initialization (but indicating destruction if appropriate) is attached
under the enk_temp_init node.  The value of the enk_temp_init is the address
(rather than the value) of the temporary if result_is_addr is TRUE.
If evaluated is FALSE, the reference is within an unevaluated expression.
If in_return_by_cctor_expression is TRUE, the reference is within the
expression of a return statement in a routine that returns its value by
calling a copy constructor.  *position is the position of the reference.
Only used in C++.
*/
{
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;

  /* Allocate the dynamic initialization entry. */
  dip = alloc_dtor_dynamic_init((a_dynamic_init_kind)dik_none, temp_type,
                                evaluated, in_return_by_cctor_expression,
                                position);
  /* Make an enk_temp_init node that points at the dynamic init entry. */
  temp_init_node = alloc_temp_init_node(temp_type, result_is_addr);
  temp_init_node->variant.init.dynamic_init = dip;
  return temp_init_node;
}  /* create_expr_temporary */


void set_routine_calling_method_flag(a_type_ptr         routine_type,
                                     a_source_position  *err_pos)
/*
Set the calling-method flag in the indicated routine type; that flag is
used when the function result is returned to a temporary provided by the
caller.  Also issue a diagnostic if the return type is an abstract class.
This routine may be called more than once, since the information on the
return type can be incomplete at the original declaration of the function
and must be completed by the point of call.  Note: one cannot assume that
a routine with a body has its flag set correctly; e.g., this may not be
the case if the return type was incomplete at the point of definition.
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_type_ptr                    return_type;

  db_enter(4, "set_routine_calling_method_flag");
  if (C_dialect != C_dialect_cplusplus) {
    /* The flag cannot be set in C mode. */
  } else {
    routine_type = skip_typerefs(routine_type);
    rtsp = routine_type->variant.routine.extra_info;
    if (rtsp->value_returned_by_cctor) {
      /* Once the flag is set, it will never change. */
    } else {
      /* If the function returns a class object that has a "real" copy
         constructor, make the caller provide a temporary for the result. */
      return_type = skip_typerefs(routine_type->variant.routine.return_type);
      if (is_immediate_class_type(return_type)) {
        if (is_incomplete_type(return_type)) {
          /* The return type is an incomplete class so we can't tell whether
             special handling will be required for the return.  Enter the
             routine type on a fixup list and check again when the return
             type has been defined. */
          add_to_dependent_type_fixup_list(return_type,
                                           (a_dependent_type_fixup_kind)
                                                  dtfk_routine_calling_method,
                                           (char *)routine_type,
                                           (a_byte_il_entry_kind)iek_type,
                                           err_pos);
        } else if (!symbol_supplement_for_class(return_type)->
                                        construction_by_bitwise_copy_allowed) {
          rtsp->value_returned_by_cctor = TRUE;
          /* If the return type is an abstract class, issue an error.  Note
             that construction_by_bitwise_copy_allowed will never be TRUE
             for abstract classes.  Also note that this logic assumes that
             value_returned_by_cctor will never be set elsewhere. */
          if (return_type->variant.class_struct_union.abstract) {
            if (err_pos->seq == 0) {
              /* A null error position indicates a routine type for which
                 there is no corresponding source position -- e.g., a type
                 is being copied for some reason.  Issue no diagnostic in
                 such cases. */
            } else {
              pos_error(ec_function_returning_abstract_class, err_pos);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_routine_calling_method_flag */


an_expr_node_ptr func_call_expr(
                               an_expr_node_ptr  function_node,
                               a_type_ptr        function_type,
                               a_boolean         is_virtual,
                               a_boolean         evaluated,
                               a_boolean         in_return_by_cctor_expression,
                               a_source_position *err_pos)
/*
Make an expression for a call of the function indicated by function_node,
whose type is function_type, and which is virtual if is_virtual is TRUE or
a pointer-to-member-function call if the type of function_node is
pointer-to-member-function.  evaluated is FALSE if the function call
is within an unevaluated expression.  If in_return_by_cctor_expression
is TRUE, the reference is within the expression of a return statement in
a routine that returns its value by calling a copy constructor.  The
arguments of the call are already attached to function_node.  A
skip_typerefs need not have been done on function_type.  Return a
pointer to the call node.  *err_pos gives an error position for the case
where the function return type is invalid (i.e., incomplete); an error
node is returned for that case.
*/
{
  an_expr_operator_kind         op;
  an_expr_node_ptr              call_node;
  a_type_ptr                    return_type;
  a_routine_type_supplement_ptr rtsp;
  an_expr_node_ptr              temp_init_node = NULL;
  a_dynamic_init_ptr            dip;

  function_type = skip_typerefs(function_type);
  /* The function return type must be void or object type and not array
     type.  Half of this check is in add_to_derived_type_list.
     The check here is necessary because it is valid to declare a
     function returning a class/struct/enum type that is incomplete
     at the point of declaration of the function so long as it is completed
     by the time the function is defined or called (if it is). */
  if (!check_function_return_type(function_type, err_pos,
                                  /*is_expr_use=*/TRUE)) {
    /* There was some error in the return type, and a diagnostic was issued. */
    call_node = error_node();
    goto done;
  } /* if */
  if (function_node->kind == (an_expr_node_kind)enk_routine_address) {
    /* We know which routine is being called. */
    a_routine_ptr rp = function_node->variant.routine;
    if (evaluated && !rp->called) {
      /* It is being called -- set the flag. */
      rp->called = TRUE;
      /* Special checking is required for operator-> functions. */
      if (rp->special_kind == (a_special_function_kind)sfk_operator &&
          rp->opname_kind == (an_opname_kind)onk_arrow) {
        /* This is an operator-> function that has never before been called.
           If it is a member of a template class, be sure it has a valid
           return type.  (Note:  template classes may define operator->
           functions that return invalid types as long as they are never
           called. */
        if (symbol_supplement_for_class(rp->source_corresp.
                          class_of_which_a_member)->class_template != NULL) {
          /* If the return type is invalid, change the return type to an
             error_type and issue a diagnostic. */
          check_operator_arrow_return_type(rp, /*is_expr_use=*/TRUE, err_pos);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Any type qualifiers on the return type are dropped because rvalues
     do not have qualified types. */
  return_type = skip_typerefs(function_type->variant.routine.return_type);
  if (is_reference_type(return_type)) {
    /* If the function returns a reference type, make the result a
       pointer. */
    return_type = make_pointer_type(type_pointed_to(return_type));
  }  /* if */
  /* Determine the operator to use for the call. */
  if (is_ptr_to_member_type(function_node->type)) {
    /* Call using a pointer-to-member-function. */
    op = (an_expr_operator_kind)eok_pm_call;
  } else if (is_virtual) {
    /* Call of a virtual function. */
    op = (an_expr_operator_kind)eok_virtual_call;
  } else {
    /* Normal call. */
    op = (an_expr_operator_kind)eok_call;
  }  /* if */
  /* Make an expression for the function call. */
  call_node = make_operator_node(op, return_type, function_node);
  rtsp = function_type->variant.routine.extra_info;
  if (rtsp->value_returned_by_cctor) {
    temp_init_node = create_expr_temporary(return_type,
                                           /*result_is_addr=*/FALSE,
                                           evaluated,
                                           in_return_by_cctor_expression,
                                           err_pos);
    dip = temp_init_node->variant.init.dynamic_init;
    set_dynamic_init_kind(dip,
                      (a_dynamic_init_kind)dik_call_returning_class_via_cctor);
    dip->variant.expression = call_node;
    call_node = temp_init_node;
  }  /* if */
done:
  return call_node;
}  /* func_call_expr */


void mark_routine_referenced(a_routine_ptr  routine)
/*
Mark the indicated routine as actually referenced.  "Actually" means
as opposed to referenced in a virtual function call that may call some
other virtual function.  Note that an actual reference does not necessarily
mean that the routine is called; this reference might be taking the address
of the routine.  It does, however, force instantiation if the function
is a template function, and/or definition if the function is the right kind
of compiler-generated function (e.g., a constructor).
*/
{
  a_symbol_ptr             assoc_sym;
  a_template_instance_ptr  instance_ptr;

  /* Set the referenced flag.  This is only necessary for virtual
     functions referenced by qualified name.  For non-virtual functions,
     the normal reference-processing routines have already set the IL
     referenced flag. */
  routine->source_corresp.referenced = TRUE;
  /* If the routine is a nonstatic member function, mark the class of which
     it is a member as referenced.  This ensures that a class will not
     end up marked as unreferenced when one of its nonstatic member functions
     (which references the class at least in its "this" parameter) is
     marked referenced. */
  if (routine_type_is_nonstatic_member_function(routine->type)) {
    routine->source_corresp.class_of_which_a_member->
                                              source_corresp.referenced = TRUE;
  }  /* if */
  /* If the routine is compiler-generated and its definition has not
     yet been put out, force the definition now. */
  force_definition_of_compiler_generated_routine(routine);
  /* If the function is an instance of a function template, mark it
     as requiring an instantiation. */
  assoc_sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
  if (assoc_sym != NULL) {
    instance_ptr = assoc_sym->variant.routine.instance_ptr;
    if (instance_ptr != NULL) {
      update_instantiation_required_flag(instance_ptr, TRUE,
                                         /*defer_inline=*/FALSE);
    }  /* if */
  }  /* if */
}  /* mark_routine_referenced */


a_statement_ptr make_assignment_statement(an_expr_node_ptr dest,
                                          an_expr_node_ptr source)
/*
Create an expression statement pointing to an assignment operator that
assigns the rvalue "source" to the lvalue "dest".  Return a pointer to
the statement.  May not be used for array types.
*/
{
  a_statement_ptr  stmt;
  an_expr_node_ptr node;

  /* Make the assignment node. */
  dest->next = source;
  node = make_operator_node(which_binary_operator(tok_assign, source->type),
                            source->type, dest);
  /* Allocate the statement. */
  stmt = alloc_expr_statement(node);
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
  a_statement_ptr  stmt;
  an_expr_node_ptr node;

  /* Make the assignment node. */
  dest->next = source;
  node = make_operator_node((an_expr_operator_kind)eok_bassign,
                            type_pointed_to(dest->type), dest);
  /* Allocate the statement. */
  stmt = alloc_expr_statement(node);
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
  a_statement_ptr  stmt;
  an_expr_node_ptr node, func_addr_node;

  /* Make a node for the address of the function. */
  func_addr_node = function_addr_expr(rout, /*set_address_taken_flag=*/FALSE);
  /* Link the operands to the function address node. */
  func_addr_node->next = dest;
  dest->next = source;
  /* Make the call node. */
  node = func_call_expr(func_addr_node, rout->type,
                        (a_boolean)rout->is_virtual, /*evaluated=*/TRUE,
                        /*in_return_by_cctor_expression=*/FALSE,
                        err_pos);
  /* Allocate the statement. */
  stmt = alloc_expr_statement(node);
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


void set_block_scope_handler(a_handler_ptr  handler)
/*
Set the assoc_handler field of the current scope.
*/
{
  a_scope_ptr  sp;

  sp = ensure_il_scope_exists(&scope_stack[decl_scope_level]);
  check_assertion_str(sp != NULL, "set_block_scope_handler: NULL IL scope");
  sp->variant.assoc_handler = handler;
}  /* set_block_scope_handler */


void set_statement_kind(a_statement_ptr  sp,
                        a_statement_kind stmt_kind)
/*
Set the kind of the statement sp to stmt_kind, and set the associated variant
fields to default values.
*/
{
  a_block_ptr     bp;
  a_for_loop_ptr  flip;

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
      sp->variant.label = NULL;
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
      bp->assoc_scope      = NULL;
      bp->parent_block     = NULL;
      bp->end_of_block_reachable = TRUE;
      bp->any_initializing_decls_in_parent_block = FALSE;
#if CHECKING
      bp->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
      break;
    case stmk_init:
      sp->variant.dynamic_init = NULL;
      break;
    case stmk_asm:
      sp->variant.asm_entry = NULL;
      break;
    case stmk_try_block:
      sp->variant.try_block.statement = NULL;
      sp->variant.try_block.handlers  = NULL;
      break;
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
  sp->next                = NULL;
  sp->dependent_statement = FALSE;
  sp->has_associated_pragma = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  set_statement_kind(sp, stmt_kind);
  db_exit();
  return sp;
}  /* alloc_statement */


a_statement_ptr alloc_expr_statement(an_expr_node_ptr node)
/*
Allocate an stmk_expr statement pointing to the indicated expression
and return a pointer to it.
*/
{
  a_statement_ptr stmt = alloc_statement((a_statement_kind)stmk_expr);

  stmt->expr = node;
  set_expr_result_not_used(node);
  return stmt;
}  /* alloc_expr_statement */


void copy_statement(a_statement *from,
                    a_statement *to)
/*
Copy a statement entry from "from" to "to".
*/
{
  a_boolean has_associated_pragma = to->has_associated_pragma;

  *to = *from;
  /* Preserve the pragma flag in the destination statement. */
  to->has_associated_pragma = has_associated_pragma;
}  /* copy_statement */


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
    case pk_none:
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
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if CHECKING
    default:
      internal_error("alloc_pragma: bad pragma kind");
#endif /* CHECKING */
  }  /* switch */

  return pp;
}  /* alloc_pragma */


void add_to_pragma_list(a_pragma_ptr   pragma,
                        a_boolean      at_file_scope,
			a_type_ptr     class_type)
/*
Add pragma to the end of the pragmas list of the appropriate scope.
If class_type is not NULL, the scope of the class is used (in C++ mode).
Otherwise, either the file scope or the current scope is used, depending
on the value of at_file_scope.
*/
{
  a_scope_ptr              sp;
  a_scope_stack_entry_ptr  ssep = NULL;

  if (class_type != NULL && !C_mode()) {
    /* The pragma is bound to a member of a class.  The binding may be
       taking place in the scope of the class or may be taking place in
       some other scope.  A static data member definition may have a
       pragma bound to it at file scope and a friend declaration may have
       a pragma bound to it in the scope of some other class.  A pragma
       bound to a class member is always entered on the pragma list of
       scope of the class. */
    a_scope_depth	scope_depth;
    sp = class_type->variant.class_struct_union.extra_info->assoc_scope;
    check_assertion_str2(sp != NULL, "add_to_pragma_list:",
                         "scope for class is NULL");
    /* If the scope of the class is still on the scope stack, get a pointer
       to the scope stack entry. */
    scope_depth = sp->depth_in_scope_stack;
    if (scope_depth != NO_SCOPE_DEPTH) {
      ssep = &scope_stack[scope_depth];
    } else {
      ssep = NULL;
    }  /* if */
  } else {
    a_scope_depth	scope_depth;
    scope_depth = at_file_scope ? DEPTH_OF_FILE_SCOPE : depth_scope_stack;
    ssep = &scope_stack[scope_depth];
    sp = ensure_il_scope_exists(ssep);
  }  /* if */
  check_assertion_str(sp != NULL, "add_to_pragma_list: NULL IL scope");
  check_assertion_str(ssep == NULL ? TRUE :
                      (((a_boolean)in_file_scope(pragma)) ==
                         (ssep->il_memory_region == FILE_SCOPE_REGION_NUMBER)),
                      "add_to_pragma_list: memory region mismatch");
  if (sp->pragmas == NULL) {
    sp->pragmas = pragma;
  } else if (ssep == NULL) {
    /* No scope stack entry, find the end of the pragma list.  Note that
       the case where sp->pragmas is NULL is already tested above. */
    a_pragma_ptr	end_of_list = sp->pragmas;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = pragma;
  } else {
    ssep->last_pragma->next = pragma;
  }  /* if */
  if (ssep != NULL) ssep->last_pragma = pragma;
}  /* add_to_pragma_list */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_pragma_ptr find_assoc_pragma(char          *il_entity,
                               a_scope_ptr   curr_func_or_block_scope,
                               a_type_ptr    class_type,
                               a_pragma_ptr  prev_assoc_pragma)
/*
Return a pointer to a pragma that is bound to il_entity (an entry in the IL
whose kind is not specified because it is not needed to find the pragma).
The pragma will be on a list pointed to from a scope that is to be
determined.  prev_assoc_pragma is a (possibly NULL) pointer to another
pragma that is bound the same IL entity and has already been located.  When
prev_assoc_pragma is non-NULL, search the remainder of the list it belongs
to; otherwise, determine the scope whose pragma list is to be searched --
curr_func_or_block_scope identifies the current IL scope when it is a
function or block; class_type is non-NULL when the IL entity is a member of
a class.  A pragma must be found if prev_assoc_pragma is NULL (i.e., if a
pragma has not yet been found for the given IL entity).
*/
{
  a_pragma_ptr  assoc_pragma;
  a_scope_ptr   sp;

  if (prev_assoc_pragma) {
    /* A pragma has already been found that is associated il_entity.  Any
       additional pragmas associated with the same entity will be be among
       its successors on the same list. */
    assoc_pragma = prev_assoc_pragma->next;
  } else {
    /* Determine which scope has the list that is to be searched. */
    if (class_type != NULL) {
      check_assertion(!C_mode());
      /* The entity is a member of a class, so look on the pragma list for the
         scope associated with the class. */
      sp = class_type->variant.class_struct_union.extra_info->assoc_scope;
    } else if (curr_func_or_block_scope != NULL && !in_file_scope(il_entity)) {
      /* The entity belongs to a function or block scope and was allocated
         in the local memory region.  Check the list of the local scope. */
      sp = curr_func_or_block_scope;
    } else {
      /* The pragma must be on the file scope's pragma list. */
      sp = il_header.primary_scope;
    }  /* if */
    assoc_pragma = sp->pragmas;
  }  /* if */
  for (; assoc_pragma != NULL; assoc_pragma = assoc_pragma->next) {
    if (assoc_pragma->entity.ptr == il_entity) break;
  }  /* for */
  check_assertion_str((assoc_pragma != NULL) || (prev_assoc_pragma != NULL),
                      "find_assoc_pragma: pragma not found");
  return assoc_pragma;
}  /* find_assoc_pragma */

#if !STANDALONE_UTILITY_PROGRAM
           
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
      sp->variant.routine.ptr                 = assoc_routine;
      sp->variant.routine.parameters          = NULL;
      sp->variant.routine.constructor_inits   = NULL;
      sp->variant.routine.this_param_variable = NULL;
      sp->variant.routine.return_value_variable = NULL;
#ifdef FIL
      sp->variant.routine.function_result_var = NULL;
#endif /* ifdef FIL */
      break;
#if CHECKING
    default:
      internal_error("alloc_scope: bad scope kind");
#endif /* CHECKING */
  }  /* switch */
  sp->assoc_block          = NULL;
  sp->constants            = NULL;
  sp->types                = NULL;
  sp->variables            = NULL;
  sp->nonstatic_variables  = NULL;
  sp->labels               = NULL;
  sp->routines             = NULL;
  sp->asm_entries          = NULL;
  sp->scopes               = NULL;
  sp->dynamic_inits        = NULL;
  sp->pragmas              = NULL;
  sp->depth_in_scope_stack = NO_SCOPE_DEPTH;
#ifdef FIL
  sp->entries              = NULL;
  sp->namelist_groups      = NULL;
#endif /* ifdef FIL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sp->source_sequence_list = NULL;
  sp->src_seq_sublist_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  sp->hidden_names         = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
  sp->templates            = NULL;
#endif /* RECORD_TEMPLATES_IN_IL */

  db_exit();
  return sp;
}  /* alloc_scope */


#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
void db_source_sequence_entry(a_source_sequence_entry_ptr  ssep)
/*
Display the source-sequence entry pointed to by ssep, for debugging purposes.
*/
{
  an_il_entry_kind  kind = (an_il_entry_kind)ssep->entity.kind;

  fputs(il_entry_kind_names[(int)kind], f_debug);
  if (kind == (an_il_entry_kind)iek_src_seq_sublist) {
    fputs(" ==>\n", f_debug);
    ssep = assoc_sublist_of(ssep)->source_sequence_list;
    for (; ssep != NULL; ssep = ssep->next) {
      fputs("    ", f_debug);
      db_source_sequence_entry(ssep);
    }  /* for */
  } else {
    if (kind == (an_il_entry_kind)iek_statement) {
      char      *s;
      a_statement_ptr   sp = (a_statement_ptr)ssep->entity.ptr;

      switch (sp->kind) {
        case stmk_expr:           s = "expr";     break;
        case stmk_if:             s = "if";       break;
        case stmk_while:          s = "while";    break;
        case stmk_goto:           s = "goto";     break;
        case stmk_label:          s = "label";    break;
        case stmk_return:         s = "return";   break;
        case stmk_block:          s = "block";    break;
        case stmk_end_test_while: s = "do-while"; break;
        case stmk_for:            s = "for";      break;
        case stmk_switch:         s = "switch";   break;
      /*case stmk_init:           Missing on purpose. */
        case stmk_asm:            s = "asm";      break;
        case stmk_try_block:      s = "try";      break;
        default:  s = "*** BAD STMT KIND ***"; break;
      }  /* if */
      fprintf(f_debug, " (at %lu): %s",
             seq_number_from_stmt_source_position(sp->position), s);
      if (sp->kind == (a_statement_kind)stmk_expr) {
        if (sp->expr != NULL) {
          switch (sp->expr->kind) {
            case enk_operation:
              fprintf(f_debug, " (operator %s)",
                      db_operator_names[sp->expr->variant.operation.kind]);
              break;
            case enk_throw:
              fprintf(f_debug, " (throw)");
              break;
            case enk_new_delete:
              fprintf(f_debug, " (%s)",
                      sp->expr->variant.new_delete->is_new ? "new" : "delete");
              break;
            default:;
          }  /* switch */
        }  /* if */
      }  /* if */
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
    } else if (kind == (an_il_entry_kind)iek_comment) {
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
    } else if (kind == (an_il_entry_kind)iek_pragma) {
      a_pragma_ptr  pp = (a_pragma_ptr)ssep->entity.ptr;

      fprintf(f_debug, " (at %lu): %s", pp->position.seq,
                       pragma_ids[(int)pp->kind]);
    } else if (kind == (an_il_entry_kind)iek_switch_clause) {
      /* Nothing else to display. */
    } else if (kind == (an_il_entry_kind)iek_src_seq_end_of_construct) {
      a_src_seq_end_of_construct_ptr  sseocp;
      sseocp = (a_src_seq_end_of_construct_ptr)ssep->entity.ptr;
      fprintf(f_debug, " (at %lu): ", sseocp->source_position.seq);
      switch (sseocp->entity.kind) {
        case iek_statement:
          fputs("block statement", f_debug);
          break;
        case iek_type:
          fputc('"', f_debug);
          db_type_name((a_type_ptr)sseocp->entity.ptr);
          fputc('"', f_debug);
          break;
        default:
          fprintf(f_debug, "***BAD END-OF-CONSTRUCT KIND %s***",
                           il_entry_kind_names[(int)sseocp->entity.kind]);
          
      }  /* switch */
    } else if (kind == (an_il_entry_kind)iek_access_adjustment) {
      an_access_adjustment_ptr  aap;
      aap = (an_access_adjustment_ptr)ssep->entity.ptr;
      fputs(": \"", f_debug);
      if (aap->entity.kind == (a_byte_il_entry_kind)iek_type) {
        db_type_name((a_type_ptr)aap->entity.ptr);
      } else {
        db_name(&((a_field_ptr)aap->entity.ptr)->source_corresp);
      }  /* if */
      fputc('"', f_debug);
    } else {
      a_source_position             *pos;
      a_source_correspondence       *scp;
      a_symbol_ptr                  sym;
      a_boolean                     lparen_printed = FALSE;
      a_boolean                     autonomous = FALSE;
      a_type_ptr                    declared_type = NULL;
      a_boolean                     print_type = FALSE;
      a_src_seq_secondary_decl_ptr  sssdp = NULL;

      if (ssep->entity.ptr == NULL) {
        fputs(" <null entity ptr>", f_debug);
      } else {
        if (kind == (an_il_entry_kind)iek_src_seq_secondary_decl) {
          sssdp = (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
          scp = source_corresp_for_il_entry(
                                  sssdp->entity.ptr,
                                  (an_il_entry_kind)sssdp->entity.kind);
          check_assertion(scp != NULL);
          pos = &sssdp->decl_position;
          if (sssdp->autonomous_tag_decl) autonomous = TRUE;
        } else {
          scp = source_corresp_for_il_entry(
                                         ssep->entity.ptr,
                                         (an_il_entry_kind)ssep->entity.kind);
          check_assertion(scp != NULL);
          pos = &scp->decl_position;
          if (kind == (an_il_entry_kind)iek_type) {
            if (((a_type_ptr)ssep->entity.ptr)->autonomous_primary_tag_decl) {
              autonomous = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        sym = (a_symbol_ptr)scp->assoc_info;
        if (kind == (an_il_entry_kind)iek_variable &&
            ((a_variable_ptr)ssep->entity.ptr)->is_parameter) {
          fprintf(f_debug, " (function param");
          lparen_printed = TRUE;
        }  /* if */
        if (sym != NULL && sym->decl_seq > 0) {
          fprintf(f_debug, "%s#%lu", (lparen_printed ? ", " : " ("),
                  sym->decl_seq);
          lparen_printed = TRUE;
        }  /* if */
        if (pos->seq > 0) {
          fprintf(f_debug, "%sat %lu", (lparen_printed ? ", " : " ("),
                  pos->seq);
          lparen_printed = TRUE;
        }  /* if */
        if (autonomous) {
          fprintf(f_debug, "%sautonomous decl",
                           (lparen_printed ? ", " : " ("));
          lparen_printed = TRUE;
        }  /* if */
        fprintf(f_debug, "%s: \"", (lparen_printed ? ")" : ""));
        if (kind == (an_il_entry_kind)iek_type) {
          db_type_name((a_type_ptr)ssep->entity.ptr);
        } else {
          db_name(scp);
        }  /* if */
        fputc('"', f_debug);
        if (sssdp != NULL) {
          /* Secondary declaration. */
          declared_type = sssdp->declared_type;
          if (declared_type == NULL &&
              sssdp->entity.kind == (a_byte_il_entry_kind)iek_type) {
            /* Don't report a NULL declared type on the secondary declaration
               of a type entry -- that's what's expected. */
          } else {
            print_type = TRUE;
          }  /* if */
        } else if (kind == (an_il_entry_kind)iek_variable ||
                   kind == (an_il_entry_kind)iek_routine) {
          if (kind == (an_il_entry_kind)iek_variable) {
            declared_type = ((a_variable_ptr)ssep->entity.ptr)->declared_type;
          } else {
            declared_type = ((a_routine_ptr)ssep->entity.ptr)->declared_type;
          }  /* if */
          print_type = TRUE;
        }  /* if */
        if (print_type) {
          fprintf(f_debug, " (");
          if (declared_type == NULL) {
            fputs("type = ***NULL***", f_debug);
          } else if (has_name(declared_type)) {
            fputc('"', f_debug);
            db_type_name(declared_type);
            fputc('"', f_debug);
          } else {
            db_abbreviated_type(declared_type);
          }  /* if */
          fputc(')', f_debug);
        }  /* if */
      }  /* if */
    }  /* if */
    fputc('\n', f_debug);
  }  /* if */
}  /* db_source_sequence_entry */


void db_source_sequence_list(a_source_sequence_entry_ptr  ssep)
/*
Display the list of source-sequence entries pointed to by ssep, for debugging
purposes.
*/
{
  a_source_sequence_entry_ptr  prev = NULL;

  for (; ssep != NULL; ssep = ssep->next) {
    if (ssep->prev != prev) {
      fputs("**BAD PREV PTR:", f_debug);
    }  /* if */
    fputs("  ", f_debug);
    db_source_sequence_entry(ssep);
    prev = ssep;
  }  /* for */
}  /* db_source_sequence_list */


void db_ss_list_for_scope(a_scope_ptr  sp)
/*
Given a scope pointer, display its list of source-sequence entries, for
debugging purposes.
*/
{
  fputs("source-sequence list for ", f_debug);
  if (sp == NULL) {
    fputs("***NULL IL SCOPE***\n", f_debug);
  } else {
    (void)db_scope_kind(sp->kind);
    fprintf(f_debug, " scope %d", (int)sp->number);
    if (sp->kind == (a_scope_kind)sck_class_struct_union ||
        sp->kind == (a_scope_kind)sck_function) {
      fputs(" (", f_debug);
      if (sp->kind == (a_scope_kind)sck_class_struct_union) {
        db_type_name(sp->variant.assoc_type);
      } else {
        db_name(&sp->variant.routine.ptr->source_corresp);
      }  /* if */
      fputc(')', f_debug);
    }  /* if */
    if (sp->source_sequence_list == NULL) {
      fputs(": <empty>\n", f_debug);
    } else {
      fputs(":\n", f_debug);
      db_source_sequence_list(sp->source_sequence_list);
    }  /* if */
  }  /* if */
}  /* db_ss_list_for_scope */

void dump_ss(a_scope_ptr  sp)
/*
Interface to db_ss_list_for_scope, to allow source sequence lists to be
displayed independently of other debug output (e.g., if "-d dump_ss=3"
appears on the command-line).  This is also called from pop_scope.
*/
{
  db_enter(3, "dump_ss");
  if (debug_level >= 3) {
    /* Display source sequence lists for debug purposes. */
    if (sp != NULL && sp->source_sequence_list != NULL) {
      db_ss_list_for_scope(sp);
    }  /* if */
  }  /* if */
  db_exit();
}  /* dump_ss */
#endif /* DEBUG */


static a_source_sequence_entry_ptr alloc_source_sequence_entry(void)
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
  sssdp->autonomous_tag_decl = FALSE;

  return sssdp;
}  /* alloc_src_seq_secondary_decl */


static a_src_seq_end_of_construct_ptr alloc_src_seq_end_of_construct(void)
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


static a_src_seq_sublist_ptr alloc_src_seq_sublist(void)
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

static void add_to_src_seq_sublist_list(a_src_seq_sublist_ptr  sublist)
/*
Add sublist to the end of the source-sequence-sublist list for the current
scope.
*/
{
  a_scope_stack_entry_ptr  scope_stack_ptr;

  scope_stack_ptr = &scope_stack[depth_innermost_ss_list_scope];
  if (scope_stack_ptr->last_src_seq_sublist != NULL) {
    scope_stack_ptr->last_src_seq_sublist->next = sublist;
  } else {
    scope_stack_ptr->il_scope->src_seq_sublist_list = sublist;
  }  /* if */
  scope_stack_ptr->last_src_seq_sublist = sublist;
}  /* add_to_src_seq_sublist_list */


static a_src_seq_sublist_ptr make_sublist_header_and_parent(
                                     a_source_sequence_entry_ptr  fs_ssep,
                                     a_source_sequence_entry_ptr  *local_ssep)
/*
Allocate a pair of entries, a sublist header (in the file scope memory
region) and its "parent", a source sequence entry, which is in the function
scope memory region; the latter points to the former.  Note that they entries
are not added to the lists they belong on -- that's handled by the caller.
*/
{
  a_src_seq_sublist_ptr        sublist;
  a_source_sequence_entry_ptr  new_ssep;
  a_memory_region_number       region_to_switch_back_to;

  check_assertion(depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE);
  /* Allocate the sublist header and set it to point at file_scope_ssep. */
  sublist = alloc_src_seq_sublist();
  sublist->source_sequence_list = fs_ssep;
  sublist->last_source_sequence_entry = fs_ssep;
  /* Switch to the function scope region for the parent entry. */
  switch_to_scope_region(depth_innermost_ss_list_scope,
                         &region_to_switch_back_to);
  new_ssep = alloc_source_sequence_entry();
  switch_back_to_original_region(region_to_switch_back_to);
  new_ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_sublist;
  new_ssep->entity.ptr  = (char *)sublist;
  *local_ssep = new_ssep;
  return sublist;
}  /* make_sublist_header_and_parent */


a_source_sequence_entry_ptr find_sublist_parent(a_src_seq_sublist_ptr sublist)
/*
Find the source sequence entry that is the "parent" of sublist -- i.e., the
iek_src_seq_sublist entry on the function scope source sequence list that
points to sublist.
*/
{
  a_source_sequence_entry_ptr  ssep;

  ssep = scope_stack[depth_innermost_ss_list_scope].
                                 il_scope->source_sequence_list;
  for (;; ssep = ssep->next) {
    check_assertion(ssep != NULL);
    if (is_sublist_parent(ssep) && assoc_sublist_of(ssep) == sublist) break;
  }  /* if */
  return ssep;
}  /* find_sublist_parent */


void add_to_source_sequence_list(a_source_sequence_entry_ptr  new_ssep)
/*
Add new_ssep to the end of the source-sequence list of the appropriate scope,
which is either the file scope or a function scope; if the latter, new_ssep
will go on a sublist if it was allocated in the file-scope memory region.
*/
{
  a_scope_stack_entry_ptr      scope_stack_ptr;
  a_src_seq_sublist_ptr        sublist;
  a_scope_ptr                  sp;
  a_source_sequence_entry_ptr  func_scope_ssep = NULL;

  db_enter(4, "add_to_source_sequence_list");
  scope_stack_ptr = &scope_stack[depth_innermost_ss_list_scope];
  if (depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE &&
      in_file_scope(new_ssep)) {
    /* A filescope entity being added to a local scope. */
    func_scope_ssep = scope_stack_ptr->last_source_sequence_entry;
    if (func_scope_ssep != NULL && is_sublist_parent(func_scope_ssep)) {
      /* The end of the function scope's source sequence list already points
         to a sublist header.  Just add new_ssep to the end of the sublist. */
      sublist = assoc_sublist_of(func_scope_ssep);
      new_ssep->prev = sublist->last_source_sequence_entry;
      sublist->last_source_sequence_entry->next = new_ssep;
      sublist->last_source_sequence_entry = new_ssep;
    } else {
      /* Either the source sequence list is empty or its tail is not a
         sublist.  In either case, allocate a new sublist entry and a new
         source sequence entry to point to it and add both the ends of their
         respective lists. */
      sublist = make_sublist_header_and_parent(new_ssep, &func_scope_ssep);
      add_to_src_seq_sublist_list(sublist);                
      add_to_source_sequence_list(func_scope_ssep);
    }  /* if */
#if DEBUG
    if (debug_level >= 4) {
      db_source_sequence_entry(func_scope_ssep);
    }  /* if */
#endif /* DEBUG */
  } else {
    sp = scope_stack_ptr->il_scope;
    check_assertion_str(sp != NULL,
                        "add_to_source_sequence_list: NULL IL scope");
    if (sp->source_sequence_list == NULL) {
      new_ssep->prev = NULL;
      sp->source_sequence_list = new_ssep;
    } else {
      new_ssep->prev = scope_stack_ptr->last_source_sequence_entry;
      scope_stack_ptr->last_source_sequence_entry->next = new_ssep;
    }  /* if */
    scope_stack_ptr->last_source_sequence_entry = new_ssep;
    new_ssep->next = NULL;
#if DEBUG
    if (debug_level >= 4) {
      db_ss_list_for_scope(sp);
    }  /* if */
#endif /* DEBUG */
    if (depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
        scope_stack_ptr->ss_list_instantiation_insert_point == NULL) {
      /* This is the first source sequence entry to be entered on the source
         sequence list since the insert point for instantiations was set to
         NULL (at the point where a new declaration begins). Record the
         current entry as the insert point (i.e., the point before which the
         source sequence entries for an instantiation should be inserted). */
      scope_stack_ptr->ss_list_instantiation_insert_point = new_ssep;
    }  /* if */
  }  /* if */
  db_exit();
}  /* add_to_source_sequence_list */


void f_update_source_sequence_list(char                         *entity_ptr,
                                   an_il_entry_kind             kind,
                                   a_source_sequence_entry_ptr  old_ssep)
/*
Allocate a source sequence entry for the entity and add it to the list for
the current scope.  If old_ssep is non-NULL, it points to a source sequence
entry that has already been created and linked in for this entity.
*/
{
  a_source_sequence_entry_ptr   ssep, new_ssep;
  a_src_seq_secondary_decl_ptr  sssdp;
  a_source_correspondence       *scp;
  a_boolean                     force_alloc_in_filescope;
  a_memory_region_number        region_to_switch_back_to;

  db_enter(4, "f_update_source_sequence_list");
  check_assertion_str(!source_sequence_entries_disallowed,
                      "source sequence entries not allowed in current scope");
  if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER &&
      kind != iek_statement && kind != iek_switch_clause &&
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
      kind != iek_comment &&
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
      in_file_scope(entity_ptr)) {
    /* The entity is in the file scope, but the current memory region is
       a function-scope memory region.  We'll need to change memory regions
       before allocating a new source sequence entry. */
    force_alloc_in_filescope = TRUE;
    switch_to_file_scope_region(&region_to_switch_back_to);
  } else {
    /* Current memory region is fine. */
    force_alloc_in_filescope = FALSE;
  }  /* if */
  if (old_ssep == NULL) {
    /* There is no previously allocated source sequence entry to reuse, so
       allocate a new one.  It will be filled out later. */
    new_ssep = alloc_source_sequence_entry();
  } else {
    /* A "reusable" source sequence entry should be empty. */
    check_assertion((ss_entry_kind(old_ssep) == (an_il_entry_kind)iek_none &&
                     old_ssep->entity.ptr == NULL));
    if (in_file_scope(old_ssep) || !force_alloc_in_filescope) {
      /* Either old_ssep is already allocated in the file scope or it's
         okay as is.  We'll just reuse it. */
      new_ssep = old_ssep;
    } else {
      /* The existing entry, in a function scope source sequence list, has to
         be replaced by an file-scope entry on a sublist. */
      a_boolean                merged = FALSE;
      a_src_seq_sublist_ptr    sublist;
      a_scope_stack_entry_ptr  stack_ptr;

      /* Confirm that the current source sequence list is a function scope
         list. */
      check_assertion(depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE);
      stack_ptr = &scope_stack[depth_innermost_ss_list_scope];
      /* Allocate a new source sequence entry to replace old_ssep. */
      new_ssep = alloc_source_sequence_entry();
      if (old_ssep->next != NULL) {
        if (is_sublist_parent(old_ssep->next)) {
          /* The next entry in the function scope list after the one that's
             to be replaced is a sublist parent.  Add the new source sequence
             entry to the head of its list. */
          sublist = assoc_sublist_of(old_ssep->next);
          new_ssep->next = sublist->source_sequence_list;
          if (sublist->source_sequence_list != NULL) {
            sublist->source_sequence_list->prev = new_ssep;
          }  /* if */
          sublist->source_sequence_list = new_ssep;
          merged = TRUE;
        }  /* if */
      }  /* if */
      if (old_ssep->prev != NULL) {
        if (is_sublist_parent(old_ssep->prev)) {
          /* The entry that precedes the one that's to be replaced is a sublist
             parent.  Add the new source entry to the end of its list. */
          sublist = assoc_sublist_of(old_ssep->prev);
          new_ssep->prev = sublist->last_source_sequence_entry;
          sublist->last_source_sequence_entry->next = new_ssep;
          if (merged) {
            /* There is a sublist associated with old_ssep->next as well as
               this one associated with old_ssep->prev.  Since old_ssep is
               to be removed, the two sublists can be combined into one.
               The list for old_ssep->next has already been tacked on to the
               end of the list for old_ssep->prev. */
            a_src_seq_sublist_ptr  other_sublist =
                                             assoc_sublist_of(old_ssep->next);
            /* Set the new tail pointer. */
            sublist->last_source_sequence_entry =
                                other_sublist->last_source_sequence_entry;
            /* Clear the other list (to be neat) and remove it and its
               sublist parent. */
            other_sublist->last_source_sequence_entry = NULL;
            other_sublist->source_sequence_list = NULL;
            remove_sublist_header_and_parent(other_sublist, old_ssep->next);
          } else {
            /* Set the new tail pointer. */
            sublist->last_source_sequence_entry = new_ssep;
            merged = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (merged) {
        /* Since new_ssep has been added to a sublist, old_ssep can simply be
           removed. */
        a_src_seq_sublist_ptr  dummy = NULL;
        remove_from_source_sequence_list(old_ssep, &dummy);
#if DEBUG
        if (debug_level >= 4) {
          fputs("empty ss entry replaced and sublists merged\n", f_debug);
          db_ss_list_for_scope(stack_ptr->il_scope);
        }  /* if */
#endif /* DEBUG */
      } else {
        /* new_ssep was not merged into an existing sublist, so make one
           for it and turn old_ssep into its sublist parent. */
        sublist = alloc_src_seq_sublist();
        sublist->source_sequence_list = new_ssep;
        sublist->last_source_sequence_entry = new_ssep;
        old_ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_sublist;
        old_ssep->entity.ptr = (char *)sublist;
        /* Then new sublist entry can be added to the end of the only if
           there are no other sublists following it. */
        if (stack_ptr->last_src_seq_sublist == NULL) {
          /* No other sublists -- just add the new one. */
          add_to_src_seq_sublist_list(sublist);
        } else {
          a_src_seq_sublist_ptr  prev_sublist = NULL;
          ssep = stack_ptr->il_scope->source_sequence_list;
          for (;; ssep = ssep->next) {
            check_assertion(ssep != NULL);
            if (ssep == old_ssep) {
              /* We've found the insert point. */
              if (prev_sublist == NULL) {
                /* sublist will become the head of the list.  We don't worry
                   about checking the tail pointer because we know the list
                   isn't empty. */
                sublist->next = stack_ptr->il_scope->src_seq_sublist_list;
                stack_ptr->il_scope->src_seq_sublist_list = sublist;
              } else {
                /* sublist will be inserted after prev_sublist and before
                   prev_sublist->next.  (We know the latter is non-null from
                   when prev_sublist was set, so we don't need to worry
                   about a tail pointer. */
                sublist->next = prev_sublist->next;
                prev_sublist->next = sublist;
              }  /* if */
              break;
            } else if (is_sublist_parent(ssep)) {
              prev_sublist = assoc_sublist_of(ssep);
              if (prev_sublist->next == NULL) {
                /* We've found the end of the sublist list before finding
                   the insert point.  Therefore, simply adding to the end will
                   work. */
                prev_sublist->next = sublist;
                stack_ptr->last_src_seq_sublist = sublist;
                break;
              }  /* if */
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (force_alloc_in_filescope) {
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  /* Point the source sequence entry at the entity. */
  new_ssep->entity.kind = (a_byte_il_entry_kind)kind;
  new_ssep->entity.ptr = entity_ptr;
  /* Then point the entity back to the source sequence entry. */
  if (kind == (an_il_entry_kind)iek_src_seq_secondary_decl) {
    sssdp = (a_src_seq_secondary_decl_ptr)entity_ptr;
    kind = (an_il_entry_kind)sssdp->entity.kind;
    entity_ptr = sssdp->entity.ptr;
  }  /* if */
  if (kind == (an_il_entry_kind)iek_statement) {
    /* Statement. */
    ((a_statement_ptr)entity_ptr)->source_sequence_entry = new_ssep;
  } else if (kind == (an_il_entry_kind)iek_pragma) {
    /* Pragma. */
    ((a_pragma_ptr)entity_ptr)->source_sequence_entry = new_ssep;
  } else {
    /* See if there's a source sequence entry. */
    scp = source_corresp_for_il_entry(entity_ptr, kind);
    if (scp == NULL) {
      /* No source correspondence, so no pointer back from the IL entry to
         the source sequence entry. */
    } else {
      /* Declared entity (variable, routine, etc.). */
      if (scp->source_sequence_entry == NULL) {
        /* The entity does not yet point to a source sequence entry.  Note
           that this includes the case where the pointer has been cleared
           because a prior declaration was turned into a secondary declaration
           -- e.g., a forward reference to a function -- see mark_declared. */
        scp->source_sequence_entry = new_ssep;
      }  /* if */
    }  /* if */
  }  /* if */
  if (old_ssep == NULL) {
    add_to_source_sequence_list(new_ssep);
  } else {
#if DEBUG
    if (debug_level >= 4) {
      fputs("empty ss entry changed to ", f_debug);
      db_source_sequence_entry(new_ssep);
    }  /* if */
#endif /* DEBUG */
  } /* if */
  db_exit();
}  /* f_update_source_sequence_list */


a_src_seq_sublist_ptr sublist_header_of(a_source_sequence_entry_ptr  ssep)
/*
ssep is a pointer to a source sequence entry that was allocated in the file
scope memory region and was added to a sublist of a function scope list.
Find and return the sublist entry that is the header for its sublist.
*/
{
  a_src_seq_sublist_ptr  sublist;

  check_assertion(depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE);
  check_assertion(in_file_scope(ssep));
  /* Find the head of the sublist. */
  while (ssep->prev != NULL) ssep = ssep->prev;
  /* Find the head of the list of sublist headers and traverse that list
     until one is found that points at ssep. */
  sublist = scope_stack[depth_innermost_ss_list_scope].il_scope->
                                                        src_seq_sublist_list;
  for (;; sublist = sublist->next) {
    if (sublist->source_sequence_list == ssep) break;
    check_assertion(sublist->next != NULL);
  }  /* for */
  return sublist;
}  /* sublist_header_of */

#if 0

/*
The following two routines, insert_in_source_sequence_list_after and
insert_in_source_sequence_list_before, have not been tested.
*/

void insert_in_source_sequence_list_after(
                                 a_source_sequence_entry_ptr  position_ssep,
                                 a_source_sequence_entry_ptr  new_ssep)
/*
Add new_ssep immediately after position_ssep in the source sequence list.
The logic assumes that position_ssep and new_ssep are allocated in the same
memory region.
*/
{
  a_src_seq_sublist_ptr    sublist;
  a_scope_stack_entry_ptr  scope_stack_ptr;

  if (in_file_scope(position_ssep) == in_file_scope(new_ssep)) {
    new_ssep->prev = position_ssep;
    new_ssep->next = position_ssep->next;
    if (position_ssep->next != NULL) {
      position_ssep->next->prev = new_ssep;
    } else {
      /* End of a list or sublist.  Reset the tail pointer. */
      if (depth_innermost_ss_list_scope == DEPTH_OF_FILE_SCOPE ||
          !in_file_scope(position_ssep)) {
        /* Find the tail pointer in the scope stack entry. */
        scope_stack_ptr = &scope_stack[depth_innermost_ss_list_scope];
        check_assertion(
                scope_stack_ptr->last_source_sequence_entry == position_ssep);
        scope_stack_ptr->last_source_sequence_entry = new_ssep;
      } else {
        /* Find the tail pointer in the sublist header. */
        sublist = sublist_header_of(position_ssep);
        check_assertion(sublist->last_source_sequence_entry == position_ssep);
        sublist->last_source_sequence_entry = new_ssep;
      }  /* if */
    }  /* if */
    position_ssep->next = new_ssep;
  } else {
#if 0
    /* Beefing up this routine would make it a lot more complex -- e.g., to
       provide for inserting new sublist headers.  Do it only if there proves
       to be a need. */
#else /* if !0 */
#if CHECKING
    internal_error("insert_in_source_sequence_list_after: region mismatch");
#endif /* CHECKING */
#endif /* if 0 */
  }  /* if */
}  /* insert_in_source_sequence_list_after */


void insert_in_source_sequence_list_before(
                                 a_source_sequence_entry_ptr  position_ssep,
                                 a_source_sequence_entry_ptr  new_ssep)
/*
Add new_ssep immediately before position_ssep in the source sequence list.
The logic assumes that position_ssep and new_ssep are allocated in the same
memory region.
*/
{
  a_src_seq_sublist_ptr  sublist;
  a_scope_ptr            sp;

  if (in_file_scope(position_ssep) == in_file_scope(new_ssep)) {
    new_ssep->next = position_ssep;
    new_ssep->prev = position_ssep->prev;
    if (position_ssep->prev != NULL) {
      position_ssep->prev->next = new_ssep;
    } else {
      /* Head of a list or sublist.  Reset the start-of-list pointer. */
      if (depth_innermost_ss_list_scope == DEPTH_OF_FILE_SCOPE ||
          !in_file_scope(position_ssep)) {
        /* Find the list pointer in the IL scope entry. */
        sp = scope_stack[depth_innermost_ss_list_scope].il_scope;
        check_assertion(sp->source_sequence_list == position_ssep);
        sp->source_sequence_list = new_ssep;
      } else {
        /* Find the list pointer in the sublist header. */
        sublist = sublist_header_of(position_ssep);
        check_assertion(sublist->source_sequence_list == position_ssep);
        sublist->source_sequence_list = new_ssep;
      }  /* if */
    }  /* if */
    position_ssep->prev = new_ssep;
  } else {
#if 0
    /* Beefing up this routine would make it a lot more complex -- e.g., to
       provide for inserting new sublist headers.  Do it only if there proves
       to be a need. */
#else /* if !0 */
#if CHECKING
    internal_error("insert_in_source_sequence_list_before: region mismatch");
#endif /* CHECKING */
#endif /* if 0 */
  }  /* if */
}  /* insert_in_source_sequence_list_after */

#endif /* if 0 */

a_source_sequence_entry_ptr add_empty_source_sequence_entry(void)
/*
Create an "empty" source sequence entry (one with a null entity pointer and
an entity kind of iek_none) -- it will be allocated in the current memory
region -- and then add it to the end of the source sequence list.
*/
{
  a_source_sequence_entry_ptr  ssep;

  db_enter(4, "add_empty_source_sequence_entry");
  if (source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are not being
       generated. */
    ssep = NULL;
  } else {
    check_assertion(curr_il_region_number == FILE_SCOPE_REGION_NUMBER ||
                    scope_stack[depth_scope_stack].kind !=
                                       (a_scope_kind)sck_func_prototype);
    ssep = alloc_source_sequence_entry();
    ssep->entity.kind = (a_byte_il_entry_kind)iek_none;
    /* Note that the entity.ptr field is left NULL. */
    add_to_source_sequence_list(ssep);
  }  /* if */
  db_exit();
  return ssep;
}  /* add_empty_source_sequence_entry */


void add_end_of_construct_source_sequence_entry(char                   *ptr,
                                                a_byte_il_entry_kind   kind)
/*
Allocate two entries, an end-of-construct entry and a source sequence entry
that points to it.  The former is made to have the specified kind and point
at the specified entry.  The latter is added to the appropriate source
sequence list.
*/
{
  a_src_seq_end_of_construct_ptr  sseocp;
  a_source_sequence_entry_ptr     ssep;
  a_boolean                       force_alloc_in_filescope;
  a_memory_region_number          region_to_switch_back_to;

  if (!source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are being
       generated. */
    if (kind == (a_byte_il_entry_kind)iek_type &&
        curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
      /* Local type. */
      check_assertion(in_file_scope(ptr));
      force_alloc_in_filescope = TRUE;
      switch_to_file_scope_region(&region_to_switch_back_to);
    } else {
      force_alloc_in_filescope = FALSE;
    }  /* if */
    /* Allocate and fill in the src-seq end of construct entry. */
    sseocp = alloc_src_seq_end_of_construct();
    sseocp->source_position = pos_curr_token;
    sseocp->entity.kind = kind;
    sseocp->entity.ptr = ptr;
    /* Allocate and fill in the source sequence entry. */
    ssep = alloc_source_sequence_entry();
    ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_end_of_construct;
    ssep->entity.ptr = (char *)sseocp;
    /* Add the source sequence entry to the list. */
    add_to_source_sequence_list(ssep);
    if (force_alloc_in_filescope) {
      switch_back_to_original_region(region_to_switch_back_to);
    }  /* if */
  }  /* if */
}  /* add_end_of_construct_source_sequence_entry */


void remove_from_source_sequence_list(a_source_sequence_entry_ptr  ssep,
                                      a_src_seq_sublist_ptr        *sublist)
/*
Remove the source sequence entry pointed to by ssep from the list to which
it belongs and place it on the appropriate available list (depending on the
memory region in which it was allocated).
*/
{
  a_source_sequence_entry_ptr  *avail_list_ptr;
  a_scope_stack_entry_ptr      scope_stack_ptr;
  a_boolean                    is_on_sublist;

  db_enter(4, "remove_from_source_sequence_list");
  /* Entries allocated in the file scope memory region may be on the list of
     the file scope itself or on a side list of a function scope. */
  scope_stack_ptr = &scope_stack[depth_innermost_ss_list_scope];
  if (*sublist != NULL ||
      (in_file_scope(ssep) &&
       depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE)) {
    /* A file scope entry on a function scope list. */
    is_on_sublist = TRUE;
    /* Be sure to return the file scope entity to the available list for the
       file scope. */
    avail_list_ptr =
               &scope_stack[DEPTH_OF_FILE_SCOPE].source_sequence_avail_list;
  } else {
    /* File scope entry on a file scope list or function scope entry on a
       function scope list. */
    is_on_sublist = FALSE;
    avail_list_ptr = &scope_stack_ptr->source_sequence_avail_list;
    if (scope_stack_ptr->ss_list_instantiation_insert_point == ssep) {
      check_assertion(depth_scope_stack == DEPTH_OF_FILE_SCOPE);
      /* We are removing the source sequence entry that was marked as the
         insert point for instantiations.  Update the insert pointer
         appropriately. */
      scope_stack_ptr->ss_list_instantiation_insert_point = ssep->next;
    }  /* if */
  }  /* if */
  /* Modify the predecessor on the list (or the list pointer itself) to
     point to ssep's successor. */
  if (ssep->prev != NULL) {
    /* There is a previous entry on the list. */
    ssep->prev->next = ssep->next;
  } else {
    /* ssep is the first entry on the list. */
    if (!is_on_sublist) {
      /* It should be the first entry on the list for the entire scope. */
      check_assertion(scope_stack_ptr->il_scope->source_sequence_list == ssep);
      scope_stack_ptr->il_scope->source_sequence_list = ssep->next;
    } else {
      /* It is a file-scope entry on a sublist of a function scope list. */
      if (*sublist == NULL) *sublist = sublist_header_of(ssep);
      check_assertion((*sublist)->source_sequence_list == ssep);
      (*sublist)->source_sequence_list = ssep->next;
    }  /* if */
  }  /* if */
  /* Modify the successor on the list (or the list's tail pointer) to point
     to ssep's predecessor. */
  if (ssep->next != NULL) {
    /* There is a successor on the list. */
    ssep->next->prev = ssep->prev;
  } else {
    /* No successor.  Change the tail pointer. */
    if (!is_on_sublist) {
      /* It should be the first entry on the list for the entire scope. */
      check_assertion(scope_stack_ptr->last_source_sequence_entry == ssep);
      scope_stack_ptr->last_source_sequence_entry = ssep->prev;
    } else {
      /* It is a file-scope entry on a sublist of a function scope list. */
      if (*sublist == NULL) *sublist = sublist_header_of(ssep);
      check_assertion((*sublist)->last_source_sequence_entry == ssep);
      (*sublist)->last_source_sequence_entry = ssep->prev;
      if ((*sublist)->last_source_sequence_entry == NULL) {
        remove_sublist_header_and_parent(*sublist,
                                         find_sublist_parent(*sublist));
        *sublist = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  /* ssep is now removed from its list.  Add it to the head of the available
     list. */
  ssep->prev = NULL;
  ssep->next = *avail_list_ptr;
  *avail_list_ptr = ssep;
#if DEBUG
  if (debug_level >= 4) {
    db_ss_list_for_scope(scope_stack_ptr->il_scope);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* remove_from_source_sequence_list */


void remove_sublist_header_and_parent(a_src_seq_sublist_ptr        sublist,
                                      a_source_sequence_entry_ptr  parent)
/*
Remove a sublist header and its source sequence entry parent from their
respective linked lists.
*/
{
  a_scope_stack_entry_ptr  scope_stack_ptr;
  a_src_seq_sublist_ptr    prev_sublist, dummy = NULL;

  /* Confirm that the function scope is still on the scope stack and that
     the sublist header being removed has an empty list. */
  check_assertion(depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE);
  check_assertion(sublist->source_sequence_list == NULL);
  /* Remove the sublist parent from the function-scope source sequence list. */
  remove_from_source_sequence_list(parent, &dummy);
  /* Remove the sublist header from the linked list of sublist headers.
     Note that it is not put on an available list for reuse.  This could be
     done, but it is not likely to make much difference either way. */
  scope_stack_ptr = &scope_stack[depth_innermost_ss_list_scope];
  if (scope_stack_ptr->il_scope->src_seq_sublist_list == sublist) {
    /* sublist is the head of the list of sublist headers. */
    scope_stack_ptr->il_scope->src_seq_sublist_list = sublist->next;
    prev_sublist = NULL;
  } else {
    /* sublist is not the head.  Find its predecessor. */
    prev_sublist = scope_stack_ptr->il_scope->src_seq_sublist_list;
    for (;; prev_sublist = prev_sublist->next) {
      check_assertion(prev_sublist != NULL);
      if (prev_sublist->next == sublist) break;
    }  /* for */
    /* Link past sublist. */
    prev_sublist->next = sublist->next;
  }  /* if */
  /* Reset the tail pointer, if necessary. */
  if (scope_stack_ptr->last_src_seq_sublist == sublist) {
    scope_stack_ptr->last_src_seq_sublist = prev_sublist;
  }  /* if */
}  /* remove_sublist_header_and_parent */


a_source_sequence_entry_ptr last_matching_source_sequence_entry(char  *entity)
/*
Find the tail-most source sequence entry in the current source-sequence-list
that refers to the IL entity identified by entity and entity_kind.
*/
{
  a_source_sequence_entry_ptr  ssep, prev_ssep;
  char                         *temp;
  a_boolean                    check_sublist;

  if (source_sequence_entries_disallowed) {
    ssep = NULL;
  } else {
    /* Determine whether the source-sequence entry we are looking for will
       be on the main list of the current source-sequence-list scope or on
       a sublist. */
    check_sublist = (depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE &&
                     in_file_scope(entity));
    /* Find the last source sequence entry that was created, and loop backwards
       from there. */
    ssep = scope_stack[depth_innermost_ss_list_scope].
                                               last_source_sequence_entry;
    for (; ssep != NULL; ssep = prev_ssep) {
      prev_ssep = ssep->prev;
      if (check_sublist) {
        /* Only look at sublists -- ignore all the rest of the list. */
        if (is_sublist_parent(ssep)) {
          /* Scan from the end of the sublist. */
          ssep = (assoc_sublist_of(ssep))->last_source_sequence_entry;
          for (; ssep != NULL; ssep = ssep->prev) {
            /* Stop if ssep refers to entity either directly or through a
               secondary-decl entry. */
            temp = ssep->entity.ptr;
            if (temp == entity ||
                ((ss_entry_kind(ssep) ==
                         (an_il_entry_kind)iek_src_seq_secondary_decl) &&
                 ((a_src_seq_secondary_decl_ptr)temp)->entity.ptr == entity)) {
              goto done;
            }  /* if */
          }  /* for */
        }  /* if */
      } else {
        /* Stop if ssep refers to entity either directly or through a
           secondary-decl entry. */
        temp = ssep->entity.ptr;
        if (temp == entity ||
            ((ss_entry_kind(ssep) ==
                     (an_il_entry_kind)iek_src_seq_secondary_decl) &&
             ((a_src_seq_secondary_decl_ptr)temp)->entity.ptr == entity)) {
          goto done;
        }  /* if */
      }  /* if */
      /* Back up to the preceding entry on the main list. */
    }  /* for */
  }  /* if */
done:
  return ssep;
}  /* last_matching_source_sequence_entry */


void set_autonomous_tag_decl_flag(a_type_ptr  type,
                                  a_boolean   is_definition)
/*
Set either the autonomous_primary_tag_decl flag in the type entry or the
autonomous_tag_decl flag in the corresponding source sequence secondary decl
entry, if there is one.
*/
{
  a_source_sequence_entry_ptr   ssep;
  a_src_seq_secondary_decl_ptr  sssdp;

  type = skip_typerefs(type);
  if (is_definition) {
    /* This is a class or enum definition.  Alway set the flag in the type
       on a definition. */
    type->autonomous_primary_tag_decl = TRUE;
  } else {
    /* This is a class or enum declaration, possibly a "vacuous"
       declaration. */
    ssep = last_matching_source_sequence_entry((char *)type);
    if (ssep != NULL) {
      if (ss_entry_kind(ssep) == (an_il_entry_kind)iek_type) {
        type->autonomous_primary_tag_decl = TRUE;
      } else {
        check_assertion(ss_entry_kind(ssep) ==
                            (an_il_entry_kind)iek_src_seq_secondary_decl);
        sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
        sssdp->autonomous_tag_decl = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_autonomous_tag_decl_flag */

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


void add_to_templates_list(a_template_ptr  tp)
/*
Add the IL template entry pointed to by tp to the list for the file scope.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
  sp = ssep->il_scope;
#if CHECKING
  if (sp == NULL) internal_error("add_to_templates_list: NULL IL scope");
#endif /* CHECKING */
  if (sp->templates == NULL) {
    sp->templates = tp;
  } else {
    ssep->last_template->next = tp;
  }  /* if */
  ssep->last_template = tp;
  tp->next = NULL;
}  /* add_to_templates_list */

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
  mp->text = NULL;

  return mp;
}  /* alloc_macro */


void add_to_macros_list(a_macro_ptr  mp)
/*
Add the IL macro entry pointed to by mp to the list for the file scope.
*/
{
  if (il_header.macros == NULL) {
    il_header.macros = mp;
  } else {
    last_macro->next = mp;
  }  /* if */
  last_macro = mp;
}  /* add_to_macros_list */

#endif /* RECORD_MACROS_IN_IL */

#if DEBUG
unsigned long show_il_space_used(void)
/*
Display and return the amount of space used for various IL tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

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
  db_space_used("access adjustment", num_access_adjustments_allocated,
                an_access_adjustment);
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
  db_space_used("accessible base class", num_accessible_base_classes_allocated,
                an_accessible_base_class);
  db_space_used("switch clause",
                num_switch_clauses_allocated, a_switch_clause);
  db_space_used("handler", num_handlers_allocated, a_handler);
  db_space_used("block", num_blocks_allocated, a_block);
  db_space_used("for_loop", num_for_loops_allocated, a_for_loop);
  db_space_used("statement", num_statements_allocated, a_statement);
  db_space_used("constructor init", num_constructor_inits_allocated,
                a_constructor_init);
  db_space_used("pragma", num_pragmas_allocated, a_pragma);
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

  db_space_used_total();

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

  return grand_total;
}  /* show_il_space_used */
#endif /* DEBUG */


void il_one_time_init(void)
/*
Do one-time initialization of variables related to the IL. (Variables
that need to be reinitialized with each new translation unit are handled
in il_init.)
*/
{
#if CHECKING
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
  /* Variable in il_file.h: */
  /* Check that the table of IL-entry sizes is correctly initialized, i.e.,
     that the enumeration an_il_entry_kind and the array sizeof_il_entry
     are in sync. */
  if (sizeof_il_entry[(int)iek_last] != IEK_LAST_CHECK_SIZE) {
    internal_error("il_init: bad initialization of sizeof_il_entry");
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT */
#if NEED_IL_DISPLAY || DEBUG
  /* Variable in il.h: */
  /* Check that the table of IL entry names is correctly initialized.
     This guards against someone changing the enumeration and forgetting to
     update il_entry_kind_names. */
  if (il_entry_kind_names[(int)iek_last] == NULL ||
      strcmp(il_entry_kind_names[(int)iek_last], "last") != 0) {
    internal_error("il_init: incorrect initialization of il_entry_kind_names");
  }  /* if */
#endif /* NEED_IL_DISPLAY || DEBUG */
#if DEBUG
  /* Variable in il_def.h: */
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
#endif /* DEBUG */
  /* Variable in il_def.h: */
  /* Check that the table of pragma ids is correctly initialized.  This guards
     against someone changing the enumeration a_pragma_kind and forgetting to
     update pragma_ids. */
  if (pragma_ids[(int)pk_last] == NULL ||
      strcmp(pragma_ids[(int)pk_last], "last") != 0) {
    internal_error("il_init: incorrect initialization of pragma_ids");
  }  /* if */
#endif /* CHECKING */

  /* Set the default source correspondence variable to default values. */
  def_source_corresp.assoc_info = NULL;
  def_source_corresp.name = NULL;
  def_source_corresp.class_of_which_a_member = NULL;
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
#if DO_IL_LOWERING
  def_source_corresp.name_has_been_mangled = FALSE;
#endif /* DO_IL_LOWERING */
#if BACK_END_IS_CP_GEN_BE
  def_source_corresp.global_qualification_needed = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE */
#if RECORD_SCOPE_DEPTH_IN_IL
  def_source_corresp.scope_depth = NO_SCOPE_DEPTH;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  def_source_corresp.source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  /* Save variables from il.h and il.c that are needed for precompiled
     headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(il_header),
      pch_saved_var_array_elem(float_types),
      pch_saved_var_array_elem(il_error_type),
      pch_saved_var_array_elem(il_unknown_type),
      pch_saved_var_array_elem(il_void_type),
      pch_saved_var_array_elem(int_types),
      pch_saved_var_array_elem(signed_int_types),
      pch_saved_var_array_elem(string_types),
      pch_saved_var_array_elem(wide_string_types),
      pch_saved_var_array_elem(shareable_constants_table),
      pch_saved_var_array_elem(avail_template_args),
#if ORPHAN_PROCESSING_NEEDED
      pch_saved_var_array_elem(orphaned_file_scope_il_entries),
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
      pch_saved_var_array_elem(last_scope_orphaned_list_header),
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
      pch_saved_var_array_elem(last_macro),
#endif /* RECORD_MACROS_IN_IL */
#if DEBUG
      pch_saved_var_array_elem(num_source_files_allocated),
      pch_saved_var_array_elem(num_constants_allocated),
      pch_saved_var_array_elem(num_param_types_allocated),
      pch_saved_var_array_elem(num_routine_type_supplements_allocated),
      pch_saved_var_array_elem(num_based_type_list_members_allocated),
      pch_saved_var_array_elem(num_class_type_supplements_allocated),
      pch_saved_var_array_elem(num_access_adjustments_allocated),
      pch_saved_var_array_elem(num_class_list_entries_allocated),
      pch_saved_var_array_elem(num_routine_list_entries_allocated),
      pch_saved_var_array_elem(num_derivation_steps_allocated),
      pch_saved_var_array_elem(num_base_class_derivations_allocated),
      pch_saved_var_array_elem(num_base_classes_allocated),
      pch_saved_var_array_elem(num_template_args_allocated),
      pch_saved_var_array_elem(num_template_param_type_descrs_allocated),
      pch_saved_var_array_elem(num_types_allocated),
      pch_saved_var_array_elem(num_dynamic_inits_allocated),
      pch_saved_var_array_elem(num_variables_allocated),
      pch_saved_var_array_elem(num_fields_allocated),
      pch_saved_var_array_elem(num_routines_allocated),
      pch_saved_var_array_elem(num_exception_specifications_allocated),
      pch_saved_var_array_elem(num_exception_specification_types_allocated),
      pch_saved_var_array_elem(num_asm_entries_allocated),
      pch_saved_var_array_elem(num_labels_allocated),
      pch_saved_var_array_elem(num_expr_nodes_allocated),
      pch_saved_var_array_elem(num_new_delete_supplements_allocated),
      pch_saved_var_array_elem(num_throw_supplements_allocated),
      pch_saved_var_array_elem(num_accessible_base_classes_allocated),
      pch_saved_var_array_elem(num_switch_clauses_allocated),
      pch_saved_var_array_elem(num_blocks_allocated),
      pch_saved_var_array_elem(num_for_loops_allocated),
      pch_saved_var_array_elem(num_statements_allocated),
      pch_saved_var_array_elem(num_constructor_inits_allocated),
      pch_saved_var_array_elem(num_pragmas_allocated),
      pch_saved_var_array_elem(num_scopes_allocated),
      pch_saved_var_array_elem(num_il_entry_prefixes_allocated),
      pch_saved_var_array_elem(string_literal_text_space_allocated),
      pch_saved_var_array_elem(num_shareable_constants),
      pch_saved_var_array_elem(num_func_shareable_constants),
      pch_saved_var_array_elem(num_used_shareable_constant_buckets),
      pch_saved_var_array_elem(num_searches_for_shareable_constants),
      pch_saved_var_array_elem(num_compares_for_shareable_constants),
      pch_saved_var_array_elem(num_get_based_type_calls),
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
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* il_one_time_init */


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
#if DO_IL_LOWERING
  initial_value_for_il_lowering_flag = 0;
#endif /* DO_IL_LOWERING */

  /* Static variables in il.c: */
  /* Depending on NULL represented as zero bits here. */
  memzero((char *)int_types, sizeof(int_types));
  memzero((char *)signed_int_types, sizeof(signed_int_types));
  memzero((char *)float_types, sizeof(float_types));
  memzero((char *)string_types, sizeof(string_types));
  memzero((char *)wide_string_types, sizeof(wide_string_types));
  il_error_type = il_unknown_type = il_void_type = NULL;
  memzero((char *)shareable_constants_table,
          sizeof(shareable_constants_table));
#if RECORD_MACROS_IN_IL
  last_macro = NULL;
#endif /* RECORD_MACROS_IN_IL */

#if DEBUG
  num_source_files_allocated             = 0;
  num_constants_allocated                = 0;
  num_param_types_allocated              = 0;
  num_routine_type_supplements_allocated = 0;
  num_based_type_list_members_allocated  = 0;
  num_class_type_supplements_allocated   = 0;
  num_access_adjustments_allocated       = 0;
  num_class_list_entries_allocated       = 0;
  num_routine_list_entries_allocated     = 0;
  num_derivation_steps_allocated         = 0;
  num_base_class_derivations_allocated   = 0;
  num_base_classes_allocated             = 0;
  num_template_args_allocated            = 0;
  num_template_param_type_descrs_allocated
                                         = 0;
  num_types_allocated                    = 0;
  num_dynamic_inits_allocated            = 0;
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
  num_accessible_base_classes_allocated  = 0;
  num_switch_clauses_allocated           = 0;
  num_blocks_allocated                   = 0;
  num_for_loops_allocated                = 0;
  num_statements_allocated               = 0;
  num_constructor_inits_allocated        = 0;
  num_pragmas_allocated                  = 0;
  num_scopes_allocated                   = 0;
  num_il_entry_prefixes_allocated        = 0;
  string_literal_text_space_allocated    = 0;
  num_shareable_constants                = 0;
  num_func_shareable_constants           = 0;
  num_used_shareable_constant_buckets    = 0;
  num_searches_for_shareable_constants   = 0;
  num_compares_for_shareable_constants   = 0;
  num_get_based_type_calls               = 0;
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
#endif /* DEBUG */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  last_scope_orphaned_list_header = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  avail_template_args = NULL;
  il_reset();
}  /* il_init */

#endif /* !STANDALONE_UTILITY_PROGRAM */


void il_reset(void)
/*
Reset any variables that contain state information that becomes invalid
when the IL has been read back into memory.
*/
{
  reset_seq_cache();
}  /* il_reset */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
