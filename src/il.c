/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il.c -- Construction of intermediate language trees.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "exprutil.h"
#include "folding.h"

#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

#if ALTERNATE_IL_FILE_FORMAT
#include "il_file.h"
#endif /* ALTERNATE_IL_FILE_FORMAT */

#if !STANDALONE_UTILITY_PROGRAM
#include "func_def.h"
#include "pch.h"
#include "templates.h"
#include "class_decl.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "decl_spec.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING
#include "lower_il.h"
#if MINIMAL_INLINING
#include "inline.h"
#endif /* MINIMAL_INLINING */
#endif /* DO_IL_LOWERING */


/*
Pointers to shared types.  These are cleared by il_init.
*/
static a_type_ptr int_types[(int)ik_last];
static a_type_ptr signed_int_types[(int)ik_last];
#if MICROSOFT_EXTENSIONS_ALLOWED
static a_type_ptr microsoft_sized_int_types[(int)ik_last];
static a_type_ptr microsoft_sized_signed_int_types[(int)ik_last];
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
static a_type_ptr float_types[(int)fk_last];
#if C99_IL_EXTENSIONS_SUPPORTED
static a_type_ptr complex_types[(int)fk_last];
static a_type_ptr imaginary_types[(int)fk_last];
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#define MAX_TRACKED_STRING_TYPE_LENGTH 80
static a_type_ptr string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
static a_type_ptr wide_string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
static a_type_ptr il_error_type;
static a_type_ptr il_unknown_type;
static a_type_ptr il_void_type;
static a_type_ptr il_wchar_t_type;
static a_type_ptr il_bool_type;

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if DEBUG
#if !STANDALONE_UTILITY_PROGRAM

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

/*
Data struct used to do a fixup pass on based-type lists containing entries
that should not escape to the back end.
*/
typedef struct a_based_type_fixup *a_based_type_fixup_ptr;
typedef struct a_based_type_fixup {
  a_based_type_fixup_ptr
		next;
			/* Next in the linked list of fixup entries; NULL if
			   this is the last on the list. */
  a_type_ptr	base_type;
			/* The type whose based-type list should be scanned
			   during the based-type-list fixup. */
} a_based_type_fixup;

static a_based_type_fixup_ptr
		based_type_fixup_list;
			/* Head of a linked list of entries identifying
			   types whose based-type lists include entries that
			   must not be passed to the back end because they
			   refer to type entries that are for front-end use
			   only. */
#if DEBUG
static unsigned long
		num_based_type_fixups_allocated;

unsigned long db_show_based_type_fixups_used(unsigned long grand_total)
/*
Display memory use for based-type fixup entries.
*/
{
  unsigned long  num, size, total;

  db_space_used("based type fixups", num_based_type_fixups_allocated,
                a_based_type_fixup);
  return grand_total;
}  /* db_show_based_type_fixups_used */
#endif /* DEBUG */

/*
Flag that is set to TRUE when there are one or more implicit children of the
lifetime object that is associated with the file scope.  (Its implicit
children are entries that point to it as a parent but which it does not
point back to on its child_lifetime list; that is because implicit children
are in the function scope memory region.)
*/
static a_boolean any_function_scope_lifetime_entries;


/* Forward declarations needed because of mutual recursion: */
static a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr       dip,
                                            an_expr_copy_options_set options);
static a_constant_hash_value hash_constant(a_constant *cp);

#if DEBUG

void db_template_arg_list(a_template_arg_ptr tap)
/*
Dump a list of template arguments, enclosed by angle brackets.
*/
{
  if (tap != NULL) {
    fputs("<", f_debug);
    do {
      if (is_type_templ_arg(tap)) {
        if (tap->variant.type->source_corresp.name == NULL) {
          db_abbreviated_type(tap->variant.type);
        } else {
          db_type_name(tap->variant.type);
        }  /* if */
      } else if (is_template_templ_arg(tap)) {
        db_template_name(tap->variant.templ);
      } else if (tap->is_array_bound_of_unknown_type) {
        fprintf(f_debug, "array-bound=%lu",
                (unsigned long)tap->variant.integer_value);
      } else {
        if (tap->arg_operand != NULL) {
          fprintf(f_debug, "<arg-operand> ");
        }  /* if */
        if (tap->variant.constant == NULL) {
          fprintf(f_debug, "<NULL constant>");
        } else {
          db_constant(tap->variant.constant);
        }  /* if */
      }  /* if */
      tap = tap->next;
      if (tap != NULL) fputs(",", f_debug);
    } while (tap != NULL);
    fputs(">", f_debug);
  }  /* if */
}  /* db_template_arg_list */


void db_template_name(a_template_ptr	tp)
/*
Dump the name of a template.
*/
{
  db_name(&tp->source_corresp);
}  /* db_template */


void db_type_name(a_type_ptr  tp)
/*
Dump the name of a type.  If it's a class generated on the basis of a
template, dump the template arguments, too.
*/
{
  a_class_type_supplement_ptr ctsp;

  db_name(&tp->source_corresp);
#if NEED_NAME_MANGLING
  if (!tp->source_corresp.name_has_been_mangled) {
#endif /* NEED_NAME_MANGLING */
    if (is_immediate_class_type(tp)) {
      ctsp = tp->variant.class_struct_union.extra_info;
      if (ctsp != NULL) {
        db_template_arg_list(ctsp->template_arg_list);
        db_template_arg_list(ctsp->partial_spec_template_arg_list);
      }  /* if */
    }  /* if */
#if NEED_NAME_MANGLING
  }  /* if */
#endif /* NEED_NAME_MANGLING */
}  /* db_type_name */


void db_name(a_source_correspondence *sc)
/*
Dump the name from a source correspondence (if any).
*/
{
  char *name;

  if (sc == NULL) {
    fputs("<no source corresp>", f_debug);
  } else {
#if NEED_NAME_MANGLING
    if (sc->name_has_been_mangled) {
      name = sc->name;
    } else {
#endif /* NEED_NAME_MANGLING */
      if (sc->is_class_member) {
        db_type_name(sc->parent.class_type);
        fputs("::", f_debug);
      } else if (sc->parent.namespace_ptr != NULL) {
        db_name(&sc->parent.namespace_ptr->source_corresp);
        fputs("::", f_debug);
      }  /* if */
      name = unmangled_name_of(sc);
      if (name == NULL) name = sc->name;
#if NEED_NAME_MANGLING
    }  /* if */
#endif /* NEED_NAME_MANGLING */
    if (name != NULL) {
      fputs(name, f_debug);
    } else {
      fprintf(f_debug, "<NULL>@%lx", (unsigned long)sc);
    }  /* if */
  }  /* if */
}  /* db_name */


static void db_name_linkage(a_name_linkage_kind nlk)
/*
Dump the indicated name linkage kind.
*/
{
  fprintf(f_debug, "%s", name_linkage_kind_names[(int)nlk]);
}  /* db_name_linkage */


void db_abbreviated_type(a_type *tp)
/*
Dump a type in abbreviated form.  This is particularly required for
classes, structs, unions, which may contain fields that point to
objects of their own type.
*/
{
  if (tp == NULL) {
    fputs("<null>", f_debug);
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
      case tk_integer:
        if (is_immediate_enum_type(tp)) {
          fputs("enum ", f_debug);
          db_type_name(tp);
          /* Display the underlying integer kind if it's other than "int". */
          if (tp->variant.integer.int_kind != (an_integer_kind)ik_int) {
            fprintf(f_debug, " (%s)", int_type_name(tp));
          }  /* if */
          break;
        }  /* if */
        /*FALLTHROUGH*/
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
  int i;

  fputs("\n  ", f_debug);
  if (depth > 0) for (i = depth; i > 0; --i) fputs("  ", f_debug);
  if (C_dialect == C_dialect_cplusplus) {
    db_access_control((an_access_specifier)fp->source_corresp.access);
    (void)fputc(' ', f_debug);
  }  /* if */
  fputs("field \"", f_debug);
  db_name(&fp->source_corresp);
  fputs("\"", f_debug);
  if (fp->is_mutable) fputs(", mutable", f_debug);
  fputs(", type = ", f_debug);
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
  db_name_linkage((a_name_linkage_kind)vp->source_corresp.name_linkage);
  fprintf(f_debug, " linkage), sc_%s, type = ",
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
  db_name_linkage((a_name_linkage_kind)rp->source_corresp.name_linkage);
  fprintf(f_debug, " linkage)%s, sc_%s,\n    type = ",
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
                     (unsigned long)ctsp->virtual_function_info_offset);
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
  fprintf(f_debug, " (pointer offset = %lu",
          (unsigned long)bcp->pointer_offset);
  if (bcp->pointer_base_class != NULL) {
    fprintf(f_debug, ", in %s",
            bcp->pointer_base_class->type->source_corresp.name);
  }  /* if */
  fputs(") ]]", f_debug);
}  /* db_virtual_base_class_ptr */


static void db_virtual_base_class(a_base_class *bcp,
                                  int          depth);
static a_base_class_derivation_ptr direct_virtual_derivation_of(
                                                        a_base_class_ptr  bcp);

/* bcp is assumed to point to a base class for which direct is set to TRUE.
   If it is a virtual base class, return a pointer to the virtual derivation
   entry associated with its direct path; otherwise return a pointer to
   whatever derivation entry it points to. */
#define direct_derivation_of(bcp)                                    \
  ((bcp)->is_virtual ? direct_virtual_derivation_of(bcp) :           \
                       (bcp)->derivation)

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
    fprintf(f_debug, " (pointer offset = %lu",
            (unsigned long)bcp->pointer_offset);
    if (bcp->pointer_base_class != NULL) {
      fprintf(f_debug, ", in %s",
              bcp->pointer_base_class->type->source_corresp.name);
    }  /* if */
    fputc(')', f_debug);
  } else {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    if (complete_subobject) fputs(" (complete subobj)", f_debug);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    fprintf(f_debug, " (offset = %lu)", (unsigned long)bcp->offset);
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
  fprintf(f_debug, ", at offset %lu", (unsigned long)bcp->offset);
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
            fputs("<?>", f_debug);
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
		   tp->source_corresp.name, (unsigned long)bcp->offset);
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


static void db_using_decl(a_using_decl_ptr udp)
/*
Dump information on a using-decl entry, for debug purposes.
*/
{
  a_source_correspondence  *sc;
  char                     *str;

  if (!udp->is_using_directive) {
    /* A using-declaration. */
    sc = source_corresp_for_il_entry(udp->entity.ptr,
                                     (an_il_entry_kind)udp->entity.kind);
    check_assertion(sc != NULL);
    fputs("\n    ", f_debug);
    if (udp->is_class_member) {
      /* A class member using-declaration. */
      switch (udp->entity.kind) {
        case iek_variable:   str = "static data member";  break;
        case iek_field:      str = "field";               break;
        case iek_routine:    str = "member function";     break;
        case iek_type:       str = "member type";         break;
        case iek_constant:   str = "member constant";     break;
        case iek_template:   str = "template";            break;
        default:             str = NULL;                  break;
      }  /* switch */
    } else {
      /* A nonmember using declaration. */
      switch (udp->entity.kind) {
        case iek_variable:   str = "variable";  break;
        case iek_routine:    str = "function";     break;
        case iek_type:       str = "type";         break;
        case iek_constant:   str = "constant";     break;
        case iek_template:   str = "template";            break;
        default:             str = NULL;                  break;
      }  /* switch */
    }  /* if */
    if (str == NULL) {
      fputs("<bad entity kind>", f_debug);
    } else {
      if (udp->is_class_member) {
        db_access_control(udp->access);
        fputc(' ', f_debug);
      }  /* if */
      fprintf(f_debug, " \"%s\" = %s ", sc->name, str);
      if (!udp->is_class_member && sc->parent.namespace_ptr == NULL) {
        fputs("::", f_debug);
      }  /* if */
      db_name(sc);
      if (udp->hidden) fprintf(f_debug, ", hidden");
      if (udp->entity.kind == (a_byte_il_entry_kind)iek_routine) {
        fputs(",\n        ", f_debug);
        db_type(((a_routine_ptr)udp->entity.ptr)->type);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* db_using_decl */


void db_function_param_list(a_type_ptr  tp)
/*
If tp is a routine type, dump the function parameters, for debug purposes.
*/
{
  a_param_type_ptr              ptp;
  a_boolean	                comma_required = FALSE;

  tp = skip_typerefs(tp);
  if (tp->kind == (a_type_kind)tk_routine) {
    fputs("(", f_debug);
    for (ptp = tp->variant.routine.extra_info->param_type_list;
         ptp != NULL;
         ptp = ptp->next) {
      if (comma_required) fputs(", ", f_debug);
      if (has_name(ptp->type)) {
        db_type_name(ptp->type);
      } else {
        db_abbreviated_type(ptp->type);
      }  /* if */
      comma_required = TRUE;
    }  /* for */
    if (tp->variant.routine.extra_info->has_ellipsis) {
      if (comma_required) fputs(", ", f_debug);
      fputs("...", f_debug);
    }  /* if */
    fputs(")", f_debug);
  }  /* if */
}  /* db_function_param_list */


static void db_qualifiers(a_type_qualifier_set  qualifiers)
/*
Print the given qualifiers in human readable form.
*/
{
  if (qualifiers & TQ_CONST) fputs("const ", f_debug);
  if (qualifiers & TQ_VOLATILE) fputs("volatile ", f_debug);
  if (qualifiers & TQ_RESTRICT) fputs("restrict ", f_debug);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (qualifiers & TQ_UNALIGNED) fputs("unaligned ", f_debug);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (qualifiers & TQ_NEAR) fputs("near ", f_debug);
  if (qualifiers & TQ_FAR) fputs("far ", f_debug);
#endif /* NEAR_AND_FAR_ALLOWED */
}  /* db_qualifiers */


void db_type(a_type *tp)
/*
Dump the contents of the indicated type entry, for debug purposes.
*/
{
  a_field_ptr                   fp;
  a_param_type_ptr              ptp;
  a_class_type_supplement_ptr	ctsp;
  a_routine_type_supplement_ptr rtsp;
  a_boolean	                comma_required;

  if (tp == NULL) {
    fputs("<null pointer>", f_debug);
  } else {
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
        if (tp->variant.integer.wchar_t_type) {
          fputs("wchar_t", f_debug);
        } else if (tp->variant.integer.bool_type) {
          fputs("bool", f_debug);
        } else {
          fprintf(f_debug, "%s", int_type_name(tp));
          if (tp->variant.integer.enum_type) fputs(" enum", f_debug);
        }  /* if */
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
      case tk_imaginary:
        fprintf(f_debug, "%s", float_kind_name(tp->variant.float_kind));
        fprintf(f_debug, tp->kind == (a_type_kind)tk_complex ? " _Complex"
                                                             : " _Imaginary");
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case tk_float:
        fprintf(f_debug, "%s", float_kind_name(tp->variant.float_kind));
        break;
      case tk_pointer:
        if (tp->variant.pointer.is_reference) {
          fputs("ref to ", f_debug);
        } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (tp->variant.pointer.base_variable != NULL) {
            fputs("based(", f_debug);
            db_name(&tp->variant.pointer.base_variable->source_corresp);
            fputs(") ", f_debug);
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          fputs("ptr to ", f_debug);
        }  /* if */
        db_abbreviated_type(tp->variant.pointer.type);
        break;
      case tk_array:
        fputs("array [", f_debug);
        if (tp->variant.array.is_static) {
          fputs("static ", f_debug);
        }  /* if */
        if (tp->variant.array.is_vla) {
          if (tp->variant.array.has_assoc_vla_dimension) {
            fputs("**EXPR**", f_debug);
          } else {
            fputc('*', f_debug);
          }  /* if */
        } else if (tp->variant.array.is_variable_size_array) {
          fputs("**EXPR**", f_debug);
        } else if (tp->variant.array.is_template_dependent_size_array) {
          db_constant(tp->variant.array.variant.element_count_constant);
        } else {
          fprintf(f_debug, "%lu",
                  (unsigned long)tp->variant.array.variant.number_of_elements);
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
        fputs(" ", f_debug);
        ctsp = tp->variant.class_struct_union.extra_info;
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ctsp != NULL) {
          a_type_kind  orig_type_kind = ctsp->orig_type_kind;
          if (orig_type_kind != tp->kind) {
            fputs("[orig: ", f_debug);
            switch (orig_type_kind) {
              case tk_struct:  fputs("struct", f_debug); break;
              case tk_union:   fputs("union", f_debug); break;
              case tk_class:   fputs("class", f_debug); break;
              default:         fputs("***BAD KIND***", f_debug);
            }  /* switch */
            fputs("] ", f_debug);
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        db_type_name(tp);
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
              ctsp->anonymous_union_kind
                                      != (an_anonymous_union_kind)auk_none) {
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
            a_variable_ptr    vp = ctsp->assoc_scope->variables;
            a_routine_ptr     rp = ctsp->assoc_scope->routines;
            a_using_decl_ptr  udp = ctsp->assoc_scope->using_decls;
  
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
              fprintf(
                   f_debug,
                  "\n  member functions (highest virtual func number = %d):",
                  ctsp->highest_virtual_function_number);
              for (; rp != NULL; rp = rp->next) db_member_function(rp);
            }  /* if */
            if (udp != NULL) {
              fputs("\n  using decls:", f_debug);
              for (; udp != NULL; udp = udp->next) {
                db_using_decl(udp);
              }  /* if */
            }  /* if */
          }  /* if */
          fputc('\n', f_debug);
          if (ctsp != NULL) {
            db_all_virtual_function_override_lists(tp);
          }  /* if */
          fprintf(f_debug, "} : size = %lu, alignment = %d",
                  (unsigned long)tp->size, tp->alignment);
          if (any_virtual_base_classes) {
            fprintf(f_debug, "; w/o virtuals: size = %lu, alignment = %d",
                    (unsigned long)ctsp->size_without_virtual_base_classes,
                    ctsp->alignment_without_virtual_base_classes);
          }  /* if */
        }
        break;
      case tk_routine:
        rtsp = tp->variant.routine.extra_info;
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (rtsp->calling_convention != (a_calling_convention)cc_default) {
          fprintf(f_debug, "%s ",
                  calling_convention_names[(int)rtsp->calling_convention]);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (rtsp->routine_name_linkage != (a_name_linkage_kind)nlk_none) {
          fputs("[", f_debug);
          db_name_linkage((a_name_linkage_kind)rtsp->routine_name_linkage);
          fputs("] ", f_debug);
        }  /* if */
        fputs("function", f_debug);
        if (rtsp->assoc_routine != NULL) {
          fputs(" ", f_debug);
          db_name(&rtsp->assoc_routine->source_corresp);
        }
        fputs("(", f_debug);
        if (!rtsp->prototyped) {
          fputs(" unprototyped", f_debug);
        }  /* if */
        if (rtsp->this_class != NULL) {
          /* Display the type of *this (without qualifications. */
          fputs("this: ", f_debug);
          db_type_name(rtsp->this_class);
          comma_required = TRUE;
        } else {
          /* No comma should be displayed next since we haven't emitted the
             first parameter yet. */
          comma_required = FALSE;
        }  /* if */
        ptp = rtsp->param_type_list;
        while (ptp != NULL) {
          if (comma_required) fputs(", ", f_debug);
          if (ptp->qualifiers != TQ_NONE &&
              remove_qualifiers_from_param_types) {
            a_type_qualifier_set  qualifiers = ptp->qualifiers;
            fputs("[", f_debug);
            if (qualifiers & TQ_CONST) {
              fputs("const", f_debug);
              qualifiers &= ~TQ_CONST;
              if (qualifiers != TQ_NONE) fputs(" ", f_debug);
            }  /* if */
            if (qualifiers & TQ_VOLATILE) {
              fputs("volatile", f_debug);
              qualifiers &= ~TQ_VOLATILE;
              if (qualifiers != TQ_NONE) fputs(" ", f_debug);
            }  /* if */
            if (qualifiers & TQ_RESTRICT) {
              fputs("restrict", f_debug);
            }  /* if */
            fputs("] ", f_debug);
          }  /* if */
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
        if (rtsp->has_ellipsis) {
          if (comma_required) fputs(", ", f_debug);
          fputs("...", f_debug);
        }  /* if */
        fputs(") ", f_debug);
        db_qualifiers(rtsp->qualifiers);
        fputs("returning ", f_debug);
        db_abbreviated_type(tp->variant.routine.return_type);
        break;
      case tk_typeref:
        if (typeref_is_qualified(tp)) {
          db_qualifiers(tp->variant.typeref.qualifiers);
        } else {
          fputs("typeref ", f_debug);
          if (has_name(tp)) { 
            fputs("\"", f_debug);
            db_name(&tp->source_corresp);
            fputs("\" ", f_debug);
          }  /* if */
          if (tp->variant.typeref.is_placeholder_for_class_instantiation) {
            fputs("class-inst-PH ", f_debug);
          }  /* if */
          if (tp->variant.typeref.is_placeholder_for_namespace_type) {
            fputs("namespace-type-PH ", f_debug);
          }  /* if */
          if (tp->variant.typeref.is_placeholder_for_nested_class_def) {
            fputs("nested-class-def-PH ", f_debug);
          }  /* if */
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
                   (a_template_param_type_kind)tptk_unknown) {
          fputs(" <unknown-type>", f_debug);
        } else {
          if (tp->variant.template_param.kind ==
                     (a_template_param_type_kind)tptk_param) {
            fprintf(f_debug, "#(%lu,%lu) ",
                    (unsigned long)tp->variant.
                              template_param.extra_info->coordinates.depth,
                    (unsigned long)tp->variant.
                              template_param.extra_info->coordinates.position);
          } else {
            fputc(' ', f_debug);
          }  /* if */
          db_name(&tp->source_corresp);
        }  /* if */
        break;
      default:
        fputs("<bad type kind>", f_debug);
    }  /* switch */
  }  /* if */
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
  octl.debug_output = TRUE;

  /* If this is a template parameter, output its coordinates. */
  if (cp->kind == (a_constant_repr_kind)ck_template_param) {
    if (cp->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_param) {
      fprintf(f_debug, "template-param#(%lu,%lu) ",
       (unsigned long)cp->variant.template_param.variant.coordinates.depth,
       (unsigned long)cp->variant.template_param.variant.coordinates.position);
    }  /* if */
  }  /* if */

  /* Output the constant. */
  form_constant(cp, /*need_parens=*/FALSE, &octl);
  if (is_enum_constant(cp) && has_name(cp)) {
    /* Display the value of the enum constant, too. */
    fputs(" (= ", f_debug);
    form_integer_constant(cp, /*suppress_cast=*/TRUE, /*need_parens=*/FALSE,
                          &octl);
    fputc(')', f_debug);
  }  /* if */
}  /* db_constant */


void db_variable(a_variable_ptr var_ptr)
/*
Dump the contents of the indicated variable for debug purposes.
*/
{
  fputs("name = ", f_debug);
  db_name(&var_ptr->source_corresp);
  if (var_ptr->is_this_parameter) fputs(" (this)", f_debug);
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
      if (node->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
        fputs(" [lvalue]", f_debug);
      }  /* if */
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
      db_dynamic_initializer(node->variant.init.dynamic_init, level+2);
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
    case enk_condition:
      fputs("condition: ", f_debug);
      db_scope(node->variant.condition->scope);
      fputs(", ", f_debug);
      if (node->variant.condition->dynamic_init == NULL) {
        fputs("<null dynamic init>", f_debug);
      } else {
        a_variable_ptr  vp = node->variant.condition->dynamic_init->variable;
        if (vp == NULL) {
          fputs("<null variable>", f_debug);
        } else {
          db_name(&vp->source_corresp);
        }  /* if */
        fputs(" = ", f_debug);
        db_dynamic_initializer(node->variant.condition->dynamic_init, level+2);
      }  /* if */
      fputs("\n", f_debug);
      break;
    case enk_object_lifetime:
      fputs("object lifetime:\n", f_debug);
      db_expr_node(node->variant.object_lifetime.expr, level + 2);
      break;
    case enk_typeid:
      fputs("typeid: type = ", f_debug);
      db_abbreviated_type(node->variant.typeid_info.type);
      if (node->variant.typeid_info.expr == NULL) {
        fputc('\n', f_debug);
      } else {
        fputs(", expr =\n", f_debug);
        db_expr_node(node->variant.typeid_info.expr, level + 2);
      }  /* if */
      break;
    case enk_runtime_sizeof:
      fputs("runtime sizeof: ", f_debug);
      if (node->variant.runtime_sizeof.is_type) {
        fputs("type = ", f_debug);
        db_abbreviated_type(node->variant.runtime_sizeof.variant.type);
        fputc('\n', f_debug);
      } else {
        fprintf(f_debug, "expr%s =\n",
                node->variant.runtime_sizeof.is_lvalue ? " (lvalue)" : "");
        db_expr_node(node->variant.runtime_sizeof.variant.expr, level + 2);
      }  /* if */
      break;
    case enk_address_of_ellipsis:
      fputs("address of ellipsis\n", f_debug);
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      fputs("lowered eh: ", f_debug);
      switch (node->variant.lowered_eh.kind) {
        case leck_caught_object_address:
          fputs("caught object address\n", f_debug);
          break;
        case leck_thrown_object_address:
          fputs("thrown object address\n", f_debug);
          break;
        case leck_unreachable_cleanup_state:
          fprintf(f_debug, "(unreachable) ");
          /* FALLTHROUGH */
        case leck_cleanup_state:
#if GENERATE_EH_TABLES
          fprintf(f_debug, "cleanup state, region number = %ld\n",
                 (long)node->variant.lowered_eh.variant.cleanup_region_number);
#else /* !GENERATE_EH_TABLES */
          fputs("cleanup state\n", f_debug);
          db_dynamic_initializer(node->variant.lowered_eh.variant.cleanup_ptr,
                                 level+2);
#endif /* GENERATE_EH_TABLES */
          break;
        case leck_function_prologue:
          fputs("function prologue\n", f_debug);
          break;
        case leck_function_epilogue:
          fputs("function epilogue\n", f_debug);
          break;
        case leck_catch_epilogue:
          fputs("catch epilogue\n", f_debug);
          break;
        case leck_try_epilogue:
          fputs("try epilogue\n", f_debug);
          break;
        case leck_exception_caught:
          fputs("exception caught\n", f_debug);
          break;
        case leck_exception_started:
          fputs("exception started\n", f_debug);
          break;
        case leck_internal_try:
          fputs("internal try\n", f_debug);
          break;
        default:
          fputs("<bad lowered eh construct kind>\n", f_debug);
      }  /* switch */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      fputs("result of overriding function\n", f_debug);
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    case enk_error:
      fputs("error node\n", f_debug);
      break;
    default:
      fputs("<bad expr kind>\n", f_debug);
  }  /* switch */
}  /* db_expr_node */


void db_expression(an_expr_node_ptr node)
/*
Dump debug information on an expression node.
*/
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


static void db_destructor(a_dynamic_init_ptr  dip)
/*
Dump debug information on the destructor part of a dynamic initialization
entry.
*/
{
  a_routine_ptr  dtor = dip->destructor;

  fprintf(f_debug, "%sdtor: ",
        dip->destruction_is_for_partially_constructed_aggregate ? "EH-" : "");
  if (dtor != NULL) {
    db_name(&dtor->source_corresp);
    fputs("()", f_debug);
  } else {
    fputs("<NULL>", f_debug);
  }  /* if */
}  /* db_destructor */


static void db_constructor_initializer(a_dynamic_init_ptr  dip,
                                       int                 level)
/*
Dump debug information on a dynamic initialization entry of type
dik_constructor.
*/
{
  an_expr_node_ptr  arg;
  int               a;

  fputs("ctor: ", f_debug);
  if (dip->variant.constructor.ptr == NULL) {
    fputs("<null>", f_debug);
  } else {
    db_name(&dip->variant.constructor.ptr->source_corresp);
    db_function_param_list(dip->variant.constructor.ptr->type);
  }  /* if */
  if (dip->destructor != NULL) {
    fputs("; ", f_debug);
    db_destructor(dip);
  }  /* if */
  fputs("\n", f_debug);
  if ((arg = dip->variant.constructor.args) != NULL) {
    for (a = 0; a < level; a++) fputs(" ", f_debug);
    fputs("ctor args =\n", f_debug);
    for (; arg != NULL; arg = arg->next) {
      db_expr_node(arg, level+2);
    }  /* if */
  }  /* if */
}  /* db_constructor_initializer */


#if 0
static void db_dynamic_init_kind(a_dynamic_init_kind kind)
/*
Dump a string identifying a dynamic-init kind, for debug purposes.
*/
{
  char *s;

  switch (kind) {
    case dik_none:          s = "dik_none";			      break;
    case dik_zero:          s = "dik_zero";       		      break;
    case dik_constant:      s = "dik_constant";   		      break;
    case dik_expression:    s = "dik_expression"; 		      break;
    case dik_call_returning_class_via_cctor:
                            s = "dik_call_returning_class_via_cctor"; break;
    case dik_constructor:   s = "dik_constructor";                    break;
    case dik_nonconstant_aggregate:
                            s = "dik_nonconstant_aggregate";          break;
    case dik_bitwise_copy:  s = "dik_bitwise_copy";                   break;
    default:                s = "**BAD DYNAMIC INIT KIND";
  }  /* switch */
  fputs(s, f_debug);
}  /* db_dynamic_init_kind */
#endif /* 0 */

static void db_constant_repr_kind(a_constant_repr_kind  kind)
/*
Dump a string identifying a constant-representation kind, for debug purposes.
*/
{
  char *s;

  switch (kind) {
    case ck_error:          s = "ck_error";		break;
    case ck_integer:        s = "ck_integer";		break;
    case ck_string:         s = "ck_string";		break;
    case ck_float:          s = "ck_float";		break;
    case ck_address:        s = "ck_address";		break;
    case ck_ptr_to_member:  s = "ck_ptr_to_member";	break;
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:   s = "ck_stack_offset";	break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:   s = "ck_dynamic_init";	break;
    case ck_aggregate:      s = "ck_aggregate";		break;
    case ck_init_repeat:    s = "ck_init_repeat";	break;
    case ck_template_param: s = "ck_template_param";	break;
    case ck_designator:     s = "ck_designator";	break;
    default:                s = "**BAD CONSTANT KIND";
  }  /* switch */
  fputs(s, f_debug);
}  /* db_constant_repr_kind */


static void db_nonconstant_aggregate(a_constant_ptr  con,
                                     int             level)
/*
Dump debug information on a dynamic initialization entry of kind
dik_nonconstant_aggregate.
*/
{
  int  a;

  for (; con != NULL; con = con->next) {
    for (a = 0; a < level; a++) fputs(" ", f_debug);
    db_constant_repr_kind(con->kind);
    if (con->type != NULL) {
      fputs(" (", f_debug);
      db_abbreviated_type(con->type);
      fputs(")", f_debug);
    }  /* if */
    fputs(": ", f_debug);
    if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
      a_dynamic_init_ptr  dip = con->variant.dynamic_init;
      db_dynamic_initializer(dip, level+2);
    } else {
      if (con->kind == (a_constant_repr_kind)ck_aggregate) {
        (void)fputc('\n', f_debug);
        db_nonconstant_aggregate(con->variant.aggregate.first_constant,
                                 level + 2);
      } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
        fprintf(f_debug, "%lu repetitions of\n",
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

  if (dip->variable != NULL) {
    fputs("variable: \"", f_debug);
    db_name(&dip->variable->source_corresp);
    fputs("\", ", f_debug);
  }  /* if */
  switch (dip->kind) {
    case dik_constant:
      db_static_initializer(dip->variant.constant);
      if (dip->destructor != NULL) {
        fputs("; ", f_debug);
        db_destructor(dip);
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
        db_destructor(dip);
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
        db_destructor(dip);
      }  /* if */
      (void)fputc('\n', f_debug);
      break;
    default:
      fputs("***BAD DYNAMIC INIT KIND***\n", f_debug);
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
    partial = (char *)(var->is_partially_initialized ? " (partial)" : "");
    for (a = 0; a < level; a++) fputs(" ", f_debug);
    if (var->init_kind == (an_init_kind)initk_function_local) {
      fprintf(f_debug, "local static initialization%s\n", partial);
    } else if (var->init_kind == (an_init_kind)initk_static) {
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


void db_statement_kind(a_statement_kind  kind)
/*
Dump a statement kind, for debug purposes.
*/
{
  char *s;

  switch (kind) {
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
    case stmk_empty:           s = "empty";             break;
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
    case stmk_expr:            s = "expr";              break;
    case stmk_if:              s = "if";                break;
    case stmk_while:           s = "while";             break;
    case stmk_goto:            s = "goto";              break;
    case stmk_label:           s = "label";             break;
    case stmk_return:          s = "return";            break;
    case stmk_block:           s = "block";             break;
    case stmk_end_test_while:  s = "end-test-while";    break;
    case stmk_for:             s = "for";               break;
    case stmk_switch:          s = "switch";            break;
    case stmk_init:            s = "init";              break;
    case stmk_asm:             s = "asm";               break;
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:   s = "asm-func-body";     break;
#endif /* ASM_FUNCTION_ALLOWED */
    case stmk_try_block:       s = "try-block";         break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:   s = "microsoft-try";     break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:            s = "decl";              break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    case stmk_set_vla_size:    s = "set-vla-size";      break;
    case stmk_vla_decl:        s = "vla-decl";          break;
    case stmk_vla_dealloc:     s = "vla-dealloc";       break;
    default:                   s = "<bad stmt kind>";   break;
  }  /* switch */
  fputs(s, f_debug);
}  /* db_statement_kind */


void db_expr_summary(an_expr_node_ptr  node)
/*
Dump some summary information about an expression node.  Called by routines
dumping other structures to which the node belongs.
*/
{
  if (node != NULL) {
    if (node->kind == (an_expr_node_kind)enk_object_lifetime) {
      /* For an object lifetime expression, display the underlying
         expression. */
       node = node->variant.object_lifetime.expr;
    }  /* if */
    switch (node->kind) {
      case enk_operation:
        fprintf(f_debug, " (operator %s)",
                db_operator_names[node->variant.operation.kind]);
        break;
      case enk_throw:
        fprintf(f_debug, " (throw)");
        break;
      case enk_new_delete:
        fprintf(f_debug, " (%s)",
                node->variant.new_delete->is_new ? "new" : "delete");
        break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      case enk_lowered_eh_construct:
        fprintf(f_debug, " (lowered eh construct)");
        break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
      case enk_result_of_overriding_function:
        fprintf(f_debug, " (result of overriding function)");
        break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
      case enk_condition:
        fprintf(f_debug, " (condition)");
        break;
      default:;
    }  /* switch */
  }  /* if */
}  /* db_expr_summary */


void db_statement(a_statement_ptr  sp)
/*
Dump a statement, for debug purposes.
*/
{
  if (sp != NULL) {
    db_statement_kind(sp->kind);
    fputs("-stmt", f_debug);
    switch (sp->kind) {
      case stmk_block:
        if (sp->variant.block.extra_info->assoc_scope != NULL) {
          fputs(" [", f_debug);
          db_scope(sp->variant.block.extra_info->assoc_scope);
          fputs("]", f_debug);
        }  /* if */
        break;
      case stmk_expr:
        db_expr_summary(sp->expr);
        break;
      case stmk_label:
      case stmk_goto:
        if (sp->variant.label.ptr->source_corresp.name != NULL) {
          fputs(" \"", f_debug);
          db_name(&sp->variant.label.ptr->source_corresp);
          fputc('"', f_debug);
        } else {
          fprintf(f_debug, " <%lx>", (long)(sp->variant.label.ptr));
        }  /* if */
        break;
      default:;
    }  /* switch */
#if EXTRA_SOURCE_POSITIONS_IN_IL && FULL_SOURCE_POS_IN_IL_STATEMENT
    fprintf(f_debug, ", at %lu/%lu", sp->position.seq,
            (unsigned long)sp->position.column);
    if (sp->end_position.seq != 0) {
      fprintf(f_debug, " -- %lu/%lu", sp->end_position.seq,
              (unsigned long)sp->end_position.column);
    }  /* if */
    fputc('\n', f_debug);
#else /* !(EXTRA_SOURCE_POSITIONS_IN_IL && FULL_SOURCE_POS_IN_IL_STATEMENT) */
    fprintf(f_debug, ", at %lu\n",
            seq_number_from_stmt_source_position(sp->position));
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL && FULL_SOURCE_POS_IN_IL_STATEMENT */
  }  /* if */
}  /* db_statement */


void db_statement_list(a_statement_ptr  sp,
                       int              indent,
                       char             *str,
                       int              how_deep)
/*
Dump a list of statements, for debug purposes.  sp is the head of the
list.  indent is the number of spaces to put out for indentation.  str is a
string which is to be put out after the indentation and before the first
statement.  how_deep is how many levels of recursion to go before terminating
the dump (this one counts as the first).
*/
{
  int                  a;
  a_switch_clause_ptr  scp;
  a_handler_ptr        hp;

  if (how_deep > 0) {
    for (; sp != NULL; sp = sp->next) {
      for (a = 0; a < indent; a++) fputs(" ", f_debug);
      fputs(str, f_debug);
      db_statement(sp);
      switch (sp->kind) {
        case stmk_block:
          db_statement_list(sp->variant.block.statements, indent+2, "",
                            how_deep-1);
          break;
        case stmk_if:
          if (how_deep > 1) {
            if (sp->variant.if_stmt.then_statement == NULL) {
              for (a = 0; a < indent+2; a++) fputs(" ", f_debug);
              fprintf(f_debug, "then <null>\n");
            } else {
              db_statement_list(sp->variant.if_stmt.then_statement, indent+2,
                                "then ", how_deep-1);
            }  /* if */
            if (sp->variant.if_stmt.else_statement != NULL) {
              db_statement_list(sp->variant.if_stmt.else_statement, indent+2,
                                "else ", how_deep-1);
            }  /* if */
          }  /* if */
          break;
        case stmk_for:
          db_statement_list(sp->variant.for_loop.statement, indent+2, "",
                            how_deep-1);
          break;
        case stmk_while:
        case stmk_end_test_while:
          db_statement_list(sp->variant.loop_statement, indent+2, "",
                            how_deep-1);
          break;
        case stmk_switch:
          db_statement_list(sp->variant.switch_stmt.body_statement, indent+2,
                            "body ", how_deep-1);
          if (how_deep > 1) {
            for (scp = sp->variant.switch_stmt.clause_list;
                 scp != NULL;
                 scp = scp->next) {
              for (a = 0; a < indent+2; a++) fputs(" ", f_debug);
              fputs(scp->constant_list == NULL ? "default\n" : "case\n",
                    f_debug);
              db_statement_list(scp->statements, indent+4, "", how_deep-1);
              if (scp->implied_break_at_end) {
                a_seq_number  seq =
                    seq_number_from_stmt_source_position(scp->break_position);
                for (a = 0; a < indent+4; a++) fputs(" ", f_debug);
                fputs("[implied break", f_debug);
                if (seq != 0) {
#if EXTRA_SOURCE_POSITIONS_IN_IL && FULL_SOURCE_POS_IN_IL_STATEMENT
                  fprintf(f_debug, ", at %lu/%lu", seq,
                          (unsigned long)scp->break_position.column);
                  if (scp->break_end_position.seq != 0) {
                    fprintf(f_debug, " -- %lu/%lu",
                            scp->break_end_position.seq,
                            (unsigned long)scp->break_end_position.column);
                  }  /* if */
#else /* !(EXTRA_SOURCE_POSITIONS_IN_IL && FULL_SOURCE_POS_IN_IL_STATEMENT) */
                  fprintf(f_debug, ", at %lu", seq);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL && FULL_SOURCE_POS_IN_IL_STATEMENT */
                }  /* if */
                fputs("]\n", f_debug);
              }  /* if */
            }  /* for */
          }  /* if */
          break;
        case stmk_try_block:
          if (sp->variant.try_block != NULL) {
            db_statement_list(sp->variant.try_block->statement, indent+2, "",
                              how_deep-1);
            if (how_deep > 1) {
              for (hp = sp->variant.try_block->handlers;
                   hp != NULL;
                   hp = hp->next) {
                for (a = 0; a < indent+2; a++) fputs(" ", f_debug);
                fprintf(f_debug, "catch%s, at %lu:",
                        hp->parameter == NULL ? " (...)" : "",
                        seq_number_from_stmt_source_position(
                                                        hp->catch_position));
                if (hp->statement->kind == (a_statement_kind)stmk_block) {
                  fputs(" ", f_debug);
                  db_statement(hp->statement);
                  db_statement_list(hp->statement->variant.block.statements,
                                    indent+4, "", how_deep-1);
                } else {
                  fputs("\n", f_debug);
                  db_statement_list(hp->statement, indent+4, "", how_deep-1);
                }  /* if */
              }  /* for */
            }  /* if */
          }  /* if */
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case stmk_microsoft_try:
          db_statement_list(sp->variant.microsoft_try->guarded_statement,
                            indent+2, "", how_deep-1);
          for (a = 0; a < indent+2; a++) fputs(" ", f_debug);
          if (sp->variant.microsoft_try->except_expr != NULL) {
            fprintf(f_debug, "__except");
            db_expression(sp->variant.microsoft_try->except_expr);
          } else {
            fprintf(f_debug, "__finally");
          }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
          if (sp->variant.microsoft_try->except_or_finally_position.seq != 0) {
            fprintf(f_debug, ", at %lu/%lu",
                    sp->variant.microsoft_try->except_or_finally_position.seq,
                    (unsigned long)sp->variant.microsoft_try->
                                           except_or_finally_position.column);
          }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          fputc('\n', f_debug);
          db_statement_list(sp->variant.microsoft_try->cleanup_statement,
                            indent+2, "", how_deep-1);
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        default:;
      }  /* switch */
    }  /* for */
  }  /* if */
}  /* db_statement_list */

#endif /* DEBUG */
#endif /* !STANDALONE_UTILITY_PROGRAM */

int compare_source_positions(a_source_position	*pos1,
			     a_source_position  *pos2)
/*
Compare two source positions.

  Return +1 if pos1 is greater than pos2.
  Return  0 if pos1 is equal to pos2.
  Return -1 if pos1 is less than pos2.

*/
{
  int		result;
  a_seq_number	seq1 = pos1->seq;
  a_seq_number	seq2 = pos2->seq;
  if (seq1 != seq2) {
    result = (seq1 > seq2) ? 1 : -1;
  } else {
    /* If the sequence numbers are equal, check the column numbers. */
    a_column_number column1 = pos1->column;
    a_column_number column2 = pos2->column;
    result = (column1 == column2) ? 0 : ((column1 > column2) ? 1 : -1);
  }  /* if */
  return result;
}  /* compare_source_positions */


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


void put_str_to_temp_text_buffer(char *str)
/*
Output the indicated string to temp_text_buffer at the position indicated by
pos_in_temp_text_buffer, and update the latter.  The terminating null character
is copied but not counted in updating pos_in_temp_text_buffer.
*/
{
  sizeof_t len = strlen(str);
  sizeof_t new_size = pos_in_temp_text_buffer + len;

  ensure_temp_text_buffer_space(new_size+1);
  (void)strcpy(temp_text_buffer+pos_in_temp_text_buffer, str);
  pos_in_temp_text_buffer = new_size;
}  /* put_str_to_temp_text_buffer */


void put_ch_to_temp_text_buffer(char ch)
/*
Output the indicated character to temp_text_buffer at the position indicated
by pos_in_temp_text_buffer, and update the latter.
*/
{
  sizeof_t new_size = pos_in_temp_text_buffer + 1;

  ensure_temp_text_buffer_space(new_size);
  temp_text_buffer[pos_in_temp_text_buffer++] = ch;
}  /* put_ch_to_temp_text_buffer */


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


void switch_to_scope_region(a_scope_depth          scope_depth,
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
                                 a_boolean	   is_include_file,
				 a_boolean	   is_system_include,
                                 a_boolean         is_preinclude,
				 a_boolean	   from_system_include_dir)
/*
Create a source file entry in the intermediate language, to record the
start of a new source file (either the primary source file or an include file).
Line line_number of the new file is sequence number seq_number of the
compilation, and the new file's short and long form names are file_name
and full_name.  The parent file for this file is parent_file; if there
is no parent file (i.e., for the primary source file), parent_file == NULL.
For entries generated by #line directives, full_name == NULL and
name_as_written == NULL.  When the name is manufactured as part of automatic
instantiation, name_as_written is also NULL.  is_include_file is TRUE for
files that are explicitly or implicitly included.  is_system_include is TRUE
for files included with the #include <file.h> notation and FALSE for all other
files.  is_preinclude is TRUE for files included via the --preinclude
command-line option.  from_system_include_dir is TRUE if the file was found
in a directory marked as a system include directory.
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
  *new_file = sfp = alloc_source_file();

  sfp->file_name        = file_name;
  sfp->full_name        = full_name;
  sfp->name_as_written  = name_as_written;
  sfp->first_seq_number = seq_number;
  sfp->first_line_number= line_number;
  sfp->is_include_file = is_include_file;
  sfp->included_by_system_include = is_system_include;
  sfp->included_by_preinclude = is_preinclude;
  sfp->from_system_include_dir = from_system_include_dir;
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

#if DEBUG
static void db_indent(int indent)
/*
Indent the current line by "indent" characters.
*/
{
  fprintf(f_debug, "%*s", indent, "");
}  /* db_indent */


static void db_source_file_seq_info(a_source_file_ptr sfp,
				    int		      indent)
/*
Display the sequence number information associated with a source file.
*/
{
  for (; sfp != NULL; sfp = sfp->next) {
    db_indent(indent);
    fprintf(f_debug, "Source file seq. info for: %s\n", sfp->file_name);
    db_indent(indent);
    fprintf(f_debug, "First_seq: %lu, last_seq: %lu\n",
            (unsigned long)sfp->first_seq_number,
            (unsigned long)sfp->last_seq_number);
    db_indent(indent);
    fprintf(f_debug, "First_line_number: %lu\n",
            (unsigned long)sfp->first_line_number);
    if (sfp->first_child_file != NULL) {
      db_source_file_seq_info(sfp->first_child_file, indent+2);
    }  /* if */
  }  /* for */
}  /* db_source_file_seq_info */
#endif /* DEBUG */

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

  db_enter(5, "source_file_for_seq");
#if DEBUG
  if (debug_level >= 5) {
    db_source_file_seq_info(il_header.primary_source_file, 0);
  }  /* if */
#endif /* DEBUG */
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
    fprintf(f_debug, "File=%s, Line=%lu, sequence number=%lu\n",
                     curr_file->file_name, *line_number, seq_number);
  }  /* if */
#endif /* DEBUG */
  db_exit();
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

a_source_file_ptr eff_primary_source_file(void)
/*
Return a pointer to the source file entry for the effective primary source
file.  This is usually the same as the primary source file, but is different
when the source file begins with a #line directive; in that case, the primary
source file is the one named in that #line directive.
*/
{
  a_source_file_ptr primary_file = il_header.primary_source_file;
  a_source_file_ptr first_file_under_primary;

  if (primary_file != NULL) {
    first_file_under_primary = primary_file->first_child_file;
    while (first_file_under_primary != NULL &&
           first_file_under_primary->included_by_preinclude) {
      /* Ignore files included by --preinclude. */
      first_file_under_primary = first_file_under_primary->next;
    }  /* if */
    if (first_file_under_primary != NULL) {
      /* See if the first file entry under the primary file entry is for
         a #line directive that is the first line of the input and
         specifies a line number of 1. */
      if (first_file_under_primary->full_name == NULL &&
          first_file_under_primary->first_seq_number == 1+1 &&
          first_file_under_primary->first_line_number == 1) {
        /* Yes -- this is the effective primary source file. */
        primary_file = first_file_under_primary;
      }  /* if */
    }  /* if */
  }  /* if */
  return primary_file;
}  /* eff_primary_source_file */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean seq_is_in_include_file(a_seq_number seq_number)
/*
Return TRUE if the sequence number seq_number falls within an include file.
*/
{
  a_boolean         in_include_file;
  a_source_file_ptr proper_file;
  a_line_number     line_number;
  a_boolean         at_end_of_source;
  unsigned long     nesting_depth;

  proper_file = source_file_for_seq(seq_number, &line_number,
                                    &at_end_of_source, &nesting_depth,
                                    /*physical_line=*/FALSE);
  if (proper_file == NULL) {
    /* Sequence number is not in a file, so it's not in an include file. */
    in_include_file = FALSE;
  } else if (proper_file == eff_primary_source_file()) {
    /* Sequence number is in the (effective) primary source file, so it's not
       in an include file.  The effective primary source file can be different
       than the actual primary source file if the source begins with a
       #line directive. */
    in_include_file = FALSE;
  } else {
    /* Sequence number is in an include file. */
    in_include_file = TRUE;
  }  /* if */
  return in_include_file;
}  /* seq_is_in_include_file */

#endif /* !STANDALONE_UTILITY_PROGRAM */

unsigned long write_file_name(char      *name,
                              FILE      *f_output,
                              a_boolean process_escapes)
/*
Write out the null-terminated file name "name" to the output file f_output.
If process_escapes is TRUE, escape special characters as necessary.
Return the number of characters written.  The caller must put out
surrounding quotes if they are needed.  This routine is used to write
out the file name in a #line directive.
*/
{
  char          *p;
  unsigned long len = 0;

  for (p = name; *p != '\0'; p++) {
    char ch = *p;
    if (isprint((unsigned char)ch)) {
      if (process_escapes && (ch == '"' || ch == '\\')) {
        putc('\\', f_output);
        len++;
      }  /* if */
      putc(ch, f_output);
      len++;
    } else if (ch == '\n') {
      /* Put out newline as \n. */
      fputs("\\n", f_output);
      len += 2;
    } else {
      /* Unprintable characters: put out as \ooo. */
      fprintf(f_output, "\\%03o",
                        (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
      len += 4;
    }  /* if */
  }  /* for */
  return len;
}  /* write_file_name */

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
#if !STANDALONE_UTILITY_PROGRAM

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
    a_scope_orphaned_list_header_ptr solhp;

    solhp = alloc_scope_orphaned_list_header(routine, scope->number);
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
  check_assertion_str(!scope->variant.routine.ptr->
                                        is_trivial_default_constructor,
                      "add_scope_orphaned_il_lists: trivial default ctor");
  r_add_scope_orphaned_il_lists(scope, scope->variant.routine.ptr);
}  /* add_scope_orphaned_il_lists */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if !STANDALONE_UTILITY_PROGRAM


void add_to_namespaces_list(a_namespace_ptr  nsp)
/*
Add the given namespace entry to the namespaces list for the current scope,
which must be either the file scope or a namespace scope.
*/
{
  a_scope_stack_entry_ptr     ssep;
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;

  ssep = &scope_stack[depth_scope_stack];
  sp = ensure_il_scope_exists(ssep);
  pointers_block = assoc_pointers_block_of(ssep);
  if (sp->namespaces == NULL) {
    sp->namespaces = nsp;
  } else {
    pointers_block->last_namespace->next = nsp;
  }  /* if */
  pointers_block->last_namespace = nsp;
}  /* add_to_namespaces_list */


void add_to_using_decls_list(a_using_decl_ptr  udp)
/*
Add the given using-decl entry to the using_decls list for the current scope.
*/
{
  a_scope_stack_entry_ptr     ssep;
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;

  ssep = &scope_stack[depth_scope_stack];
  sp = ensure_il_scope_exists(ssep);
  pointers_block = assoc_pointers_block_of(ssep);
  if (sp->using_decls == NULL) {
    sp->using_decls = udp;
  } else {
    pointers_block->last_using_decl->next = udp;
  }  /* if */
  pointers_block->last_using_decl = udp;
}  /* add_to_using_decl_list */


void add_to_scopes_list(a_scope_ptr             scope_ptr,
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
break the correspondence -- i.e., set to default values those fields that
are tied to a particular source occurrence.
*/
{
  sc->assoc_info        = NULL;
  sc->name              = NULL;
  sc->is_class_member   = FALSE;
  sc->parent.class_type = NULL;
  sc->access            = (an_access_specifier)as_public;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sc->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  sc->decl_pos_info     = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  sc->per_instantiation_needed_flags = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* break_source_corresp */

#endif /* !STANDALONE_UTILITY_PROGRAM */

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
    case iek_namespace:
    case iek_template:
#if RECORD_MACROS_IN_IL
    case iek_macro:
#endif /* RECORD_MACROS_IN_IL */
#if PROTOTYPE_INSTANTIATIONS_IN_IL
    case iek_template_parameter:
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */
      scp = &((a_constant_ptr)entity_ptr)->source_corresp;
      break;
    default:
      scp = NULL;
  }  /* switch */
  return scp;
}  /* source_corresp_for_il_entry */

#if !STANDALONE_UTILITY_PROGRAM

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


a_constant_ptr alloc_error_constant(void)
/*
Allocate and return an error constant.  The constant is allocated in the
file scope and is unshared.
*/
{
  a_constant_ptr cp;

  cp = fs_constant((a_constant_repr_kind)ck_error);
  set_error_constant(cp);
  return cp;
}  /* alloc_error_constant */


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


void set_variable_address_taken(a_variable_ptr variable)
/*
Set the address_taken flag on the indicated variable.
*/
{
  variable->address_taken = TRUE;
  /* For a parameter, set param_value_has_been_changed. */
  if (variable->is_parameter || variable->is_handler_param) {
    variable->param_value_has_been_changed = TRUE;
  }  /* if */
}  /* set_variable_address_taken */


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
  if (set_address_taken_flag) {
    set_variable_address_taken(variable);
  }  /* if */
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


void set_ptr_to_member_function_constant(a_routine_ptr routine,
                                         a_constant    *con)
/*
Fill in the constant "con" as a pointer-to-member constant for the
nonstatic member function indicated by routine.
*/
{
  a_type_ptr   member_class;
  a_symbol_ptr member_sym;

  clear_constant(con, (a_constant_repr_kind)ck_ptr_to_member);
  con->variant.ptr_to_member.is_function_ptr = TRUE;
  con->variant.ptr_to_member.variant.routine = routine;
  /* Note that the class of the pointer is always the class in which
     the member was defined, not any derived class.  See [expr.unary.op]
     5.3.1p2. */
  /* Get the parent from the symbol rather than the routine in order
     to get pointers-to-members of anonymous unions right (it doesn't
     really matter for routines, but just in case). */
  member_sym = ((a_symbol_ptr)routine->source_corresp.assoc_info);
  check_assertion(member_sym != NULL && member_sym->is_class_member);
  member_class = member_sym->parent.class_type;
  con->type = ptr_to_member_type(routine->type, member_class);
}  /* set_ptr_to_member_function_constant */


void set_ptr_to_data_member_constant(a_field_ptr field,
                                     a_constant  *con)
/*
Fill in the constant "con" as a pointer-to-member constant for the
nonstatic data member indicated by field.
*/
{
  a_type_ptr   member_class;
  a_symbol_ptr member_sym;

  clear_constant(con, (a_constant_repr_kind)ck_ptr_to_member);
  con->variant.ptr_to_member.is_function_ptr = FALSE;
  con->variant.ptr_to_member.variant.field = field;
  /* Note that the class of the pointer is always the class in which
     the member was defined, not any derived class.  See [expr.unary.op]
     5.3.1p2. */
  /* Get the parent from the symbol rather than the field in order
     to get pointers-to-members of anonymous unions right. */
  member_sym = ((a_symbol_ptr)field->source_corresp.assoc_info);
  check_assertion(member_sym != NULL && member_sym->is_class_member);
  member_class = member_sym->parent.class_type;
  con->type = ptr_to_member_type(field->type, member_class);
}  /* set_ptr_to_data_member_constant */


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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  to->source_corresp.decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  to->source_corresp.per_instantiation_needed_flags = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* copy_constant */


static an_expr_node_ptr gather_initializer_expressions(a_constant_ptr con)
/*
Gather any expressions with side effects in the initializer constant con
into a single expression, and return a pointer to that expression.  If
there are no expressions with side effects in con, return NULL.  The
expressions aren't copied; they're just linked together into one tree.
*/
{
  an_expr_node_ptr expr = NULL;

  if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
    a_dynamic_init_ptr dip = con->variant.dynamic_init;
    /* Note that if designators are accepted in C++ other initialization
       kinds may show up here (dik_constructor in particular). */
    check_assertion(dip->kind == (a_dynamic_init_kind)dik_expression);
    expr = dip->variant.expression;
    if (!node_has_side_effects(expr, (a_boolean *)NULL)) expr = NULL;
  } else if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    /* Visit the members of an aggregate. */
    a_constant_ptr mcon;
    for (mcon = con->variant.aggregate.first_constant;
         mcon != NULL;
         mcon = mcon->next) {
      an_expr_node_ptr mexpr = gather_initializer_expressions(mcon);
      if (mexpr != NULL) {
        /* Add the expression to the list using a comma operator. */
        if (expr == NULL) {
          expr = mexpr;
        } else {
          expr = make_comma_node(expr, mexpr);
        }  /* if */
      }  /* if */
    }  /* for */
  } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    expr = gather_initializer_expressions(con->variant.init_repeat.constant);
  }  /* if */
  return expr;
}  /* gather_initializer_expressions */


static an_expr_node_ptr *find_expression_in_initializer(a_constant_ptr con)
/*
Find an expression in the initializer constant con, and return a pointer
to the pointer to it.  If no expression is found, change a constant to a
ck_dynamic_init with the constant as an expression under it, and return
that expression.
*/
{
  an_expr_node_ptr *expr_ptr;

  if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
    a_dynamic_init_ptr dip = con->variant.dynamic_init;
    /* Note that if designators are accepted in C++ other initialization
       kinds may show up here (dik_constructor in particular). */
    check_assertion(dip->kind == (a_dynamic_init_kind)dik_expression);
    expr_ptr = &dip->variant.expression;
  } else if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    /* Use the first member of an aggregate. */
    /* In C, {} is not allowed.  This would have to be changed if
       designated initializers were allowed in C++. */
    check_assertion(con->variant.aggregate.first_constant != NULL);
    expr_ptr = find_expression_in_initializer(
                                        con->variant.aggregate.first_constant);
  } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    expr_ptr =
             find_expression_in_initializer(con->variant.init_repeat.constant);
  } else {
    an_expr_node_ptr   expr;
    a_dynamic_init_ptr dip =
                       alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
    /* Other constants.  Change to a ck_dynamic_init. */
    expr = alloc_node_for_constant(con);
    set_constant_kind(con, (a_constant_repr_kind)ck_dynamic_init);
    con->variant.dynamic_init = dip;
    dip->variant.expression = expr;
    expr_ptr = &dip->variant.expression;
  }  /* if */
  return expr_ptr;
}  /* find_expression_in_initializer */


void combine_initializers(a_constant_ptr     first,
                          a_dynamic_init_ptr first_dip,
                          a_constant_ptr     second,
                          a_dynamic_init_ptr second_dip)
/*
Create the effect of sequentially applying two initializers to the same
variable.  If an initializer representation is available as "a_constant"
variables, they can be passed through first and second.  If either of
these is NULL, the corresponding first_dip or second_dip should
not be null and should point to a dynamic_init representing the
initializer for which (presumably) no constant has been created (yet).
The entities passed as parameters should be unshared.  The entities
pointed to by second and second_dip are modified to reflect the
combined effect.
*/
{
  an_expr_node_ptr first_expr;

  /* Get the expression for the first constant. */
  if (first != NULL) {
    first_expr = gather_initializer_expressions(first);
  } else {
    check_assertion(first_dip->kind == (a_dynamic_init_kind)dik_expression);
    first_expr = first_dip->variant.expression;
    if (!node_has_side_effects(first_expr, (a_boolean *)NULL)) {
      first_expr = NULL;
    }  /* if */
  }  /* if */
  /* If first_expr is NULL here, the first constant contains no expressions
     with side effects, so no update of the second is required. */
  if (first_expr != NULL) {
    /* Find an expression in the second constant that we can modify.
       Make one if necessary. */
    an_expr_node_ptr *modif_ptr;
    if (second != NULL) {
      modif_ptr = find_expression_in_initializer(second);
    } else {
      check_assertion(second_dip->kind == (a_dynamic_init_kind)dik_expression);
      modif_ptr = &second_dip->variant.expression;
    }  /* if */
    /* Make a comma node to join the two expressions. */
    *modif_ptr = make_comma_node(first_expr, *modif_ptr);
  }  /* if */
}  /* combine_initializers */


void combine_initializer_constants(a_constant_ptr first, a_constant_ptr second)
/*
Modify the constant pointed to by second to represent the effect of
sequentially initializing the same variable with *first followed by (the
original) *second.  In particular, *first can be discarded if it has no
side-effects.  The constants passed as parameters should be unshared.
*/
{
  combine_initializers(first, (a_dynamic_init_ptr)NULL,
                       second, (a_dynamic_init_ptr)NULL);
}  /* combine_initializer_constants */


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
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  if (ucp->expr != NULL) {
    /* If a constant in the file scope memory region has an attached
       expression in a function scope memory region, break the link to
       the expression. */
    if (in_file_scope(ucp) && !in_file_scope(ucp->expr)) ucp->expr = NULL;
  }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  return ucp;
}  /* alloc_unshared_constant */


a_constant_ptr copy_unshared_constant_full(
                                         a_constant_ptr           old_constant,
                                         an_expr_copy_options_set options)
/*
Make a copy of an unshared constant and return pointer to the copy.  options
is the set of options for the copy.  See copy_unshared_copy for a simple
interface to this routine for the usual case.
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
      new_aggr_con = copy_unshared_constant_full(old_aggr_con, options);
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
        copy_unshared_constant_full(old_constant->variant.init_repeat.constant,
                                    options);
  } else if (new_constant->kind == (a_constant_repr_kind)ck_dynamic_init) {
    /* For ck_dynamic_init constants, copy the subtree also. */
    new_constant->variant.dynamic_init =
                          copy_dynamic_init(old_constant->variant.dynamic_init,
                                            options);
  } else if (new_constant->kind == (a_constant_repr_kind)ck_address) {
    if (new_constant->variant.address.kind ==
                                          (an_address_base_kind)abk_constant) {
      a_constant_ptr old_constant_pointed_to =
                                old_constant->variant.address.variant.constant;
      if (!in_file_scope(old_constant_pointed_to)) {
        /* For an address constant pointing to a constant, the constant must be
           copied too if it's in the wrong memory region.  This comes up for
           addresses of strings; without this copy the ck_address could end
           up in the file scope memory region with the ck_string pointed to
           in the function scope memory region. */
        /* This also comes up in inlining, when we make a copy of a constant
           from one function scope memory region to another. */
        /* The copy is made unshared because if the original constant was
           unshared we want the copy to be unshared as well, and we don't
           know for sure whether the original constant is unshared. */
        new_constant->variant.address.variant.constant =
                           copy_unshared_constant_full(old_constant_pointed_to,
                                                       options);
      }  /* if */
    }  /* if */
  }  /* if */
  return new_constant;
}  /* copy_unshared_constant_full */


a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant)
/*
Simple interface to copy_unshared_constant_full for the usual case.
*/
{
  return copy_unshared_constant_full(old_constant, CE_NO_OPTIONS);
}  /* copy_unshared_constant */


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
      hash_value = (hash_value << 5) + hash_value + *cptr;
    }  /* for */
  }  /* if */
  return hash_value;
}  /* hash_name */


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
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_float:
      hash_value = type->variant.float_kind + 87;
      break;
    case tk_pointer:
      hash_value = hash_type(type->variant.pointer.type) + 107;
      break;
    case tk_array:
      hash_value = hash_type(type->variant.array.element_type) + 307;
      if (!has_unknown_specified_bound(type)) {
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
            switch (tap->kind) {
              case tak_type:
                hash_value += hash_type(tap->variant.type) + 37;
                break;
              case tak_nontype:
                hash_value += hash_constant(tap->variant.constant) + 43;
                break;
              case tak_template:
                hash_value += hash_name(&tap->variant.templ->source_corresp);
                break;
              default: unexpected_condition(); break;
            }  /* switch */
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


static a_constant_hash_value hash_constant(a_constant *cp)
/*
Return the hash value for the indicated constant, which gives the proper
bucket of the shareable_constants_table to use for the constant.
*/
{
  a_constant_hash_value hash_value;
  a_targ_size_t         length;
  a_boolean             ovflo;
  char                  *p;

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
      /* String.  Hash all the characters. */
      hash_value = 100;
      for (length = cp->variant.string.length, p = cp->variant.string.value;
           length > 0;
           length--, p++) {
        hash_value = (hash_value << 5) + hash_value + *p;
      }  /* for */
      break;
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* Use a host-dependent routine for floating-point constants. */
      hash_value = 500 + fp_hash(&cp->variant.float_value);
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      hash_value = 250 + fp_hash(&cp->variant.complex_value->real)
                       + fp_hash(&cp->variant.complex_value->imag);
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
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
        case abk_uuidof:
          hash_value = 231;
          if (cp->variant.address.variant.type != NULL) {
            hash_value += hash_type(cp->variant.address.variant.type);
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
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      hash_value =
                hash_name(&cp->variant.stack_offset.variable->source_corresp) +
                cp->variant.stack_offset.offset + 350;
      break;
#endif /* DO_IL_LOWERING && ... */
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
      case enk_temp_init:
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

  if (cp1 == cp2) {
    eq = TRUE;
    goto end_of_routine;
  } else if (cp1->kind != cp2->kind) {
    /* eq = FALSE; */
    goto end_of_routine;
  }  /* if */
  if (!strictly_identical) {
    cp1_type = skip_typerefs(cp1_type);
    cp2_type = skip_typerefs(cp2_type);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  } else if (cp1->expr != cp2->expr) {
    /* Do not attempt to share constants if they are the result of different
       expressions. */
    /* eq = FALSE; */
    goto end_of_routine;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
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
#if C99_IL_EXTENSIONS_SUPPORTED
      case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        cp1_type = skip_typerefs(cp1_type);
        if (is_floating_type(cp1_type)) {
          eq = (fp_compare(cp1_type->variant.float_kind,
                           &cp1->variant.float_value,
                           &cp2->variant.float_value,
                           &unordered) == 0 && !unordered);
        }  /* if */
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
      case ck_complex:
        cp1_type = skip_typerefs(cp1_type);
        if (is_floating_type(cp1_type)) {
          eq = (fp_compare(cp1_type->variant.float_kind,
                           &cp1->variant.complex_value->real,
                           &cp2->variant.complex_value->real,
                           &unordered) == 0 && !unordered &&
                fp_compare(cp1_type->variant.float_kind,
                           &cp1->variant.complex_value->imag,
                           &cp2->variant.complex_value->imag,
                           &unordered) == 0 && !unordered);
        }  /* if */
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
            case abk_uuidof:
              /* Microsoft __uuidof. */
              { a_type_ptr uuid_type1 = cp1->variant.address.variant.type;
                a_type_ptr uuid_type2 = cp2->variant.address.variant.type;

                if (uuid_type1 == NULL && uuid_type2 == NULL) {
                  eq = TRUE;
                } else if (uuid_type1 == NULL || uuid_type2 == NULL) {
                  /* eq = FALSE; -- already set. */
                } else {
                  /* Compare the uuid strings. */
                  eq = strcmp(uuid_type1->variant.class_struct_union.
                                                 extra_info->uuid_string,
                              uuid_type2->variant.class_struct_union.
                                                 extra_info->uuid_string) == 0;
                }  /* if */
              }
              break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
      case ck_stack_offset:
        eq = (cp1->variant.stack_offset.variable ==
              cp2->variant.stack_offset.variable &&
              cp1->variant.stack_offset.offset ==
              cp2->variant.stack_offset.offset);
        break;
#endif /* DO_IL_LOWERING && ... */
      case ck_template_param:
        /* Note that the constant types have been compared above. */
        if (cp1->variant.template_param.kind ==
                                      cp2->variant.template_param.kind) {
          switch (cp1->variant.template_param.kind) {
            case tpck_param:
              eq = (cp1->variant.template_param.variant.coordinates.position ==
                    cp2->variant.template_param.variant.coordinates.position)
                && equiv_nesting_depths(
                        cp1->variant.template_param.variant.coordinates.depth,
                        cp2->variant.template_param.variant.coordinates.depth);
              break;
            case tpck_expression:
              eq = compare_template_param_constant_expressions(
                                    cp1->variant.template_param.variant.expr,
                                    cp2->variant.template_param.variant.expr);
              break;
            case tpck_member:
              eq = (cp1->source_corresp.name ==
                    cp2->source_corresp.name &&
                    cp1->source_corresp.is_class_member ==
                    cp2->source_corresp.is_class_member &&
                    (cp1->source_corresp.is_class_member ?
                      (cp1->source_corresp.parent.class_type ==
                       cp2->source_corresp.parent.class_type) :
                      (cp1->source_corresp.parent.namespace_ptr ==
                       cp2->source_corresp.parent.namespace_ptr)));
              break;
            case tpck_unknown_function:
              check_assertion(cp1->source_corresp.assoc_info != NULL);
              check_assertion(cp2->source_corresp.assoc_info != NULL);
              eq = (cp1->source_corresp.assoc_info ==
                    cp2->source_corresp.assoc_info);
              break;
            case tpck_cast:
            case tpck_address:
              eq = compare_constants(cp1->variant.template_param.variant.
                                                                      constant,
                                     cp2->variant.template_param.variant.
                                                                      constant,
                                     strictly_identical);
              break;
            case tpck_sizeof:
            case tpck_alignof:
            case tpck_uuidof:
              eq = identical_types(cp1->variant.template_param.variant.type,
                                   cp2->variant.template_param.variant.type);
              break;
            case tpck_template_ref:
               eq = compare_constants(cp1->variant.template_param.variant.
                                                              template_ref.con,
                                      cp2->variant.template_param.variant.
                                                              template_ref.con,
                                      strictly_identical) &&
                    equiv_template_arg_lists(
                                      cp1->variant.template_param.variant.
                                                         template_ref.arg_list,
                                      cp2->variant.template_param.variant.
                                                         template_ref.arg_list,
                                      ETA_IS_NONREAL_MEMBER);
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
end_of_routine:
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
  a_boolean eq = compare_constants(cp1, cp2, /*strictly_identical=*/TRUE);

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
  a_boolean eq = compare_constants(cp1, cp2, /*strictly_identical=*/FALSE);

  return eq;
}  /* eq_constants */


a_boolean expr_tree_contains_template_param_constant(an_expr_node_ptr  node,
                                                     a_constant_ptr    cp)
/*
cp is either a pointer to a simple template parameter constant or else
NULL (indicating any template param constant will do).  If cp is NULL, return
TRUE if node is or contains any template parameter constant.  If it is not
NULL, return TRUE if node refers to that particular constant directly or
contains it among its operands (in a position that can be deduced from).
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
        found = eq_constants(cp, cp2);
      }  /* if */
    }  /* if */
  } else if (node->kind == (an_expr_node_kind)enk_operation && cp == NULL) {
    for (op = node->variant.operation.operands; op != NULL; op = op->next) {
      if (expr_tree_contains_template_param_constant(op,
                                                     (a_constant_ptr)NULL)) {
        found = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return found;
}  /* expr_tree_contains_template_param_constant */


a_boolean constant_references_non_external_entity(a_constant_ptr constant)
/*
Return TRUE if the indicated constant references a non-external entity,
e.g., a local variable.
*/
{
  a_boolean               refs_non_ext = FALSE;
  a_source_correspondence *scp = NULL;

  if (constant->kind == (a_constant_repr_kind)ck_address) {
    /* An address constant.  See if the object referenced is external. */
    /* Get a pointer to the source correspondence for the entity. */
    switch (constant->variant.address.kind) {
      case abk_routine:
        scp = &constant->variant.address.variant.routine->source_corresp;
        break;
      case abk_variable:
        scp = &constant->variant.address.variant.variable->source_corresp;
        break;
      case abk_constant:
        scp = &constant->variant.address.variant.constant->source_corresp;
        break;
      case abk_uuidof:
        if (constant->variant.address.variant.type != NULL) {
          scp = &constant->variant.address.variant.type->source_corresp;
        }  /* if */
        break;
#if CHECKING
      default:
        internal_error(
                  "constant_references_non_external_entity: bad address kind");
#endif /* CHECKING */
    }  /* switch */
    if (scp == NULL) {
      /* Nothing referenced. */
    } else if (scp->is_class_member) {
      /* The entity is a class member.  If the class is a local class,
         the entity is non-external.  Otherwise, the class will be forced
         to be external by this reference. */
      a_type_ptr class_type = scp->parent.class_type;
      if (class_type->source_corresp.is_local_to_function) {
        refs_non_ext = TRUE;
      } else {
        /* Force the class to be external. */
        set_force_external_linkage_flag(class_type);
      }  /* if */
    } else {
      /* Not a class member. */
      refs_non_ext = (scp->name_linkage == (a_name_linkage_kind)nlk_none ||
                      scp->name_linkage == (a_name_linkage_kind)nlk_internal);
    }  /* if */
  }  /* if */
  return refs_non_ext;
}  /* constant_references_non_external_entity */


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
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
    case ck_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
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
          has_nfs_ref = !in_file_scope(cp->variant.address.variant.constant);
          break;
        case abk_uuidof:
          /* The type pointed to must be in the file scope. */
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
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      /* The variable pointed to must be in the function scope. */
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_template_param:
      switch (cp->variant.template_param.kind) {
        case tpck_param:
        case tpck_member:
        case tpck_unknown_function:
        case tpck_sizeof:
        case tpck_alignof:
        case tpck_uuidof:
        case tpck_template_ref:
          break;
        case tpck_expression:
          has_nfs_ref= !in_file_scope(cp->variant.template_param.variant.expr);
          break;
        case tpck_cast:
        case tpck_address:
          has_nfs_ref =
           has_non_file_scope_ref(cp->variant.template_param.variant.constant);
          break;
        default:
          unexpected_condition_str(
                         "has_non_file_scope_ref: bad template constant kind");
      }  /* switch */
      break;
    /* Aggregates shouldn't be shared, so we don't expect them here. */
    case ck_aggregate:
    default:
      unexpected_condition_str("has_non_file_scope_ref: bad constant kind");
  }  /* switch */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  if (!has_nfs_ref && cp->expr != NULL && !in_file_scope(cp->expr)) {
    /* The constant recorded an expression that is not allocated in file
       scope. */
    has_nfs_ref = TRUE;
  }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
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
  assoc_symbol = ((a_symbol_ptr)cp->source_corresp.assoc_info);
  if (assoc_symbol != NULL) {
    /* Constant (enumeration). */
    check_assertion(assoc_symbol->kind == (a_symbol_kind)sk_constant);
    scp = assoc_symbol->variant.constant;
#if CHECKING
    if (cp->implicit_cast != scp->implicit_cast) {
      /* Someone did an implicit cast on the constant without clearing the
         source association. */
      internal_error(
           "alloc_shareable_constant: implicitly-cast const has assoc_info");
    }  /* if */
#endif /* CHECKING */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  } else if (cp->expr != NULL) {
    /* Constants that track the expression that generated them should
       not be shared. */
    scp = alloc_unshared_constant(cp);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  } else if (cp->kind == (a_constant_repr_kind)ck_template_param &&
             !prototype_instantiations_in_il) {
    /* Template param constants should not be made part of the IL tree proper.
       Those with assoc_info non-NULL were handled above.  For others, make a
       new copy every time. */
    scp = alloc_unshared_constant(cp);
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
      if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
        /* No function on the scope stack (we're probably in IL lowering).
           There's no list of shareable constants. */
        list_ptr = NULL;
      } else {
        list_ptr = &scope_stack[depth_innermost_function_scope].
                                                      shareable_constants_list;
      }  /* if */
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
      /* No identical constant exists in the table, so create one. */
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
      if (!C_mode()) {
        /* An object lifetime will have been created for this scope in
           push scope.  Now that the scope entry exists, bind the two entries
           to one another. */
        bind_object_lifetime(ssep->curr_scope_object_lifetime,
                             (an_il_entry_kind)iek_scope, (char *)sp);
      }  /* if */
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
      routine_type = skip_typerefs(routine_type);
      routine_type->variant.routine.extra_info->prototype_scope = sp;
      sp->variant.assoc_type = routine_type;
    }  /* if */
    if (sp != NULL) {
      /* Set the scope-stack-entry depth if not already set. */
      if (sp->depth_in_scope_stack == NO_SCOPE_DEPTH) {
        sp->depth_in_scope_stack = (ssep - scope_stack);
      }  /* if */
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
Add the given constant to the constants list for the file scope
or the current scope.  This is used for member constants.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;
  a_scope_pointers_block_ptr  pointers_block;

  /* Get pointer to current or file scope entry. */
  ssep = &scope_stack[at_file_scope ? DEPTH_OF_FILE_SCOPE : decl_scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  pointers_block = assoc_pointers_block_of(ssep);
  check_assertion_str(sp != NULL, "add_to_constants_list: NULL IL scope");
  if (sp->constants == NULL) {
    sp->constants = con_ptr;
  } else {
    pointers_block->last_constant->next = con_ptr;
  }  /* if */
  pointers_block->last_constant = con_ptr;
  con_ptr->next = NULL;
}  /* add_to_constants_list */


void set_integer_constant(a_constant		*cp,
                          a_host_large_integer	value,
                          an_integer_kind	kind)
/*
Set the constant entry *cp to the integer constant given by value.
Its integer kind is as given by kind.  Note that the kind is not restricted
to be a signed kind.
*/
{
  clear_constant(cp, (a_constant_repr_kind)ck_integer);
  cp->type = integer_type(kind);
  set_integer_value(&cp->variant.integer_value, (a_host_large_integer)value);
}  /* set_integer_constant */


void set_unsigned_integer_constant(a_constant			*cp,
                                   a_host_large_unsigned	value,
                                   an_integer_kind		kind)
/*
Set the constant entry *cp to the integer constant given by value.
Its integer kind is as given by kind.  Note that the kind is not restricted
to be an unsigned kind.
*/
{
  clear_constant(cp, (a_constant_repr_kind)ck_integer);
  cp->type = integer_type(kind);
  set_unsigned_integer_value(&cp->variant.integer_value,
                             (a_host_large_unsigned)value);
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
  set_integer_constant(zero_constant, (a_host_large_integer)0,
                       (an_integer_kind)ik_int);
  type_change_constant(zero_constant, desired_type,
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE,
                       /*evaluated_context=*/TRUE,
                       /*fold_constant_addr_exprs=*/TRUE,
                       /*is_reinterpret_cast=*/FALSE,
                       /*maintain_expression=*/FALSE,
                       &did_not_fold, &error_position);
}  /* make_zero_of_proper_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

void make_uuidof_constant(a_type_ptr     uuidof_type,
                          a_constant_ptr uuidof_con)
/*
Make a constant for __uuidof(uuidof_type) in *uuidof_con.  This is a
Microsoft extension.
*/
{
  a_type_ptr const_guid_type = make_qualified_type(
                                               type_of_guid,
                                               (a_type_qualifier_set)TQ_CONST);

  clear_constant(uuidof_con, (a_constant_repr_kind)ck_address);
  uuidof_con->variant.address.kind = (an_address_base_kind)abk_uuidof;
  uuidof_con->variant.address.variant.type = uuidof_type;
  uuidof_con->type = make_pointer_type(const_guid_type);
}  /* make_uuidof_constant */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_enum_constant(a_constant_ptr con)
/*
Return TRUE if the indicated constant is an enum constant.  Note that if
you want to know if the enum constant is one that appears on the constant
list of an enum type, you must also test for it having a name; there are
copies of enum constants in initializer lists, and constants formed by
casting integral constants to an enum type, which do not correspond to
any enum constant.
*/
{
  a_boolean is_enum = FALSE;

  if (con->kind == (a_constant_repr_kind)ck_integer) {
    /* The constant has an integral representation. */
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
    check_assertion(!constant->implicit_cast);
    con_type = skip_typerefs(constant->type);
    elem_type = con_type->variant.array.element_type;
    elem_type = skip_typerefs(elem_type);
    /* Check for element type that is not some variety of char. */
    is_wide_string = !is_character_type(elem_type);
  }  /* if */
  return is_wide_string;
}  /* is_wide_string_constant */

#if !STANDALONE_UTILITY_PROGRAM

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
              report_abstract_class_error(ec_abstract_class_param_type,
                                          param_type, err_pos);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_arg_transfer_method_flag */


a_param_type_ptr make_param_type(a_type_ptr         tp,
                                 a_source_position  *decl_pos)
/*
Allocate a new param type entry, setting its "type" to tp and determining
the value for its "passed_via_copy_constructor" flag, and return a pointer
to it.  *decl_position is used for issuing diagnostics.
*/
{
  a_param_type_ptr  ptp;

  ptp = alloc_param_type(tp);
  set_arg_transfer_method_flag(ptp, decl_pos);
  return ptp;
}  /* make_param_type */


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
    check_assertion_str2(bcdp != NULL,
                         "preferred_virtual_derivation_of:",
                         "no preferred derivation");
  }  /* while */
  return bcdp;
}  /* preferred_virtual_derivation_of */


#if DEBUG
static a_base_class_derivation_ptr direct_virtual_derivation_of(
                                                         a_base_class_ptr  bcp)
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
#endif /* DEBUG */


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


static a_scope_ptr get_scope_for_list(
                                 a_scope_depth               scope_level,
                                 a_source_correspondence     *scp,
                                 a_scope_pointers_block_ptr  *pointers_block)
/*
An IL entry that has been declared at the indicated scope and with the
source-correspondence pointed to by scp is to be added to (or is already on)
a list pointed to by an IL scope entry.  Return a pointer to that IL scope
entry, along with the associated pointers-block.  When scope_level is
NO_SCOPE_DEPTH, the scope is either the file scope or the scope associated
with the class or namespace of which the entry is a member.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;
  a_namespace_ptr          nsp = NULL;
  a_type_ptr               class_type = NULL;

  if (scope_level == NO_SCOPE_DEPTH) {
    /* No scope depth is specified, so either use the namespace, class, or
       file scope. */
    if (scp->is_class_member) {
      /* Compute the scope and pointers-block for the scope associated with
         the parent class. */
      class_type = scp->parent.class_type;
    } else {
      /* Compute the scope and pointers-block for the namespace scope if this
         is a namespace member or the file scope otherwise. */
      nsp = scp->parent.namespace_ptr;
      if (nsp == NULL) scope_level = DEPTH_OF_FILE_SCOPE;
    }  /* if */
  }  /* if */
  if (class_type != NULL) {
    sp = scp->parent.class_type->
                 variant.class_struct_union.extra_info->assoc_scope;
    if (sp != NULL) {
      scope_level = sp->depth_in_scope_stack;
    }  /* if */
    if (scope_level == NO_SCOPE_DEPTH) {
      /* The class has already been popped off the scope stack. */
      *pointers_block = NULL;
    } else {
      ssep = &scope_stack[scope_level];
      *pointers_block = assoc_pointers_block_of(ssep);
    }  /* if */
  } else if (nsp != NULL) {
    /* Use the IL scope from the namespace. */
    sp = nsp->variant.assoc_scope;
    *pointers_block = &symbol_supplement_for_namespace(nsp)->pointers_block;
  } else {
    /* Use the IL scope associated with scope_level. */
    ssep = &scope_stack[scope_level];
    sp = ensure_il_scope_exists(ssep);
    *pointers_block = assoc_pointers_block_of(ssep);
  }  /* if */
  return sp;
}  /* get_scope_for_list */


a_boolean may_be_added_to_types_list(a_type_ptr     type_ptr,
                                     a_scope_depth  decl_level)
/*
Return TRUE unless there is any reason why type_ptr should not be added to
the types-list for the scope associated with the indicated scope depth.
Note: the type may already be on the list; this routine is also called when
it's to be moved to another position in the list.
*/
{
  a_boolean                may_be_added = TRUE;
  a_scope_stack_entry_ptr  ssep = NULL;

  if (is_or_contains_error_type(type_ptr) ||
      ((a_symbol_ptr)type_ptr->source_corresp.assoc_info)->is_error) {
    may_be_added = FALSE;
  } else if (is_immediate_class_type(type_ptr) ||
             is_immediate_enum_type(type_ptr)) {
    if (decl_level != NO_SCOPE_DEPTH) ssep = &scope_stack[decl_level];
    if (C_mode()) {
      check_assertion(ssep != NULL);
      if (type_ptr->declared_in_function_prototype &&
          ssep->kind != (a_scope_kind)sck_func_prototype) {
        /* If the tag was declared in a prototype scope and is now being
           resolved within the function, as in
             int f(struct f p) {struct f{int a;};  ... }
           we may assume the type entry has already been entered on the types
           list. */
        may_be_added = FALSE;
      }  /* if */
    } else if (ssep != NULL) {
      if (ssep->kind == (a_scope_kind)sck_template_declaration) {
        /* Must be an error case -- e.g., a class definition within a
           template parameter declaration.  Don't try to enter the class
           in the IL. */
        may_be_added = FALSE;
      } else if (type_ptr->source_corresp.is_class_member) {
        if (ssep->kind != (a_scope_kind)sck_class_struct_union ||
            ssep->assoc_type != type_ptr->source_corresp.parent.class_type) {
          /* May be an out-of-class definition of a C++ nested class.  It's
             already on the list. */
          may_be_added = FALSE;
        }  /* if */
      } else if (ssep->in_prototype_instantiation) {
        /* Except for member types, the type entries created for a class
           template are not added to the types list. */
        may_be_added = FALSE;
      }  /* if */
    } else if (type_ptr->source_corresp.is_class_member) {
      /* Check for a nested class that is being defined after the definition
         of its parent class has been completed.   In such cases leave the
         type entry at the current place in the types list. */
      a_scope_ptr                 sp;
      a_scope_pointers_block_ptr  pointers_block;

      check_assertion(is_immediate_class_type(type_ptr));
      sp = get_scope_for_list(NO_SCOPE_DEPTH, &type_ptr->source_corresp,
                              &pointers_block);
      if (sp->depth_in_scope_stack == NO_SCOPE_DEPTH) {
        may_be_added = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return may_be_added;
}  /* may_be_added_to_types_list */


static void add_placeholder_for_namespace_type(a_type_ptr  type_ptr)
/*
Allocate a namespace-type placeholder typeref to point to type_ptr, set its
fields, and add it to the file-scope types list.
*/
{
  a_type_ptr  placeholder;

  placeholder = alloc_type((a_type_kind)tk_typeref);
  placeholder->variant.typeref.type = type_ptr;
  placeholder->variant.typeref.is_placeholder_for_namespace_type = TRUE;
  type_ptr->referenced_by_namespace_placeholder_typeref = TRUE;
  add_to_types_list(placeholder, DEPTH_OF_FILE_SCOPE);
}  /* add_placeholder_for_namespace_type */


void add_placeholder_for_class_instantiation(a_type_ptr  type_ptr)
/*
When an instantiation occurs in the midst of a class definition, the
instantiation may be dependent upon nested types from the class.  The
instantiation is put out on the file scope types list, but the types upon
which it is possibly dependent have been recorded on the class scope types
list.  To enable il-lowering to get the ordering right when it promotes the
nested types to file scope, enter a placeholder type in the class scope to
mark the declaration position of the instantiation.  This is not an issue
when the class is a local class, since a template cannot legally be defined
in terms of local classes or types that are local class members.  Also,
don't do it for class template prototypes, since the class types created for
them don't appear in the IL.  When finding the scope to which the
placeholder type should be added, we scan backwards through the scope stack
looking for a class scope for a real class (i.e., not a prototype
instantiation).  This search is unusual in that it doesn't stop at the first
instantiation scope.  This is done so that we can find the innermost class
scope, even if there are other instantiations (e.g., function
instantiations) below that on the scope stack.
*/
{
  a_scope_stack_entry_ptr        ssep;
  a_scope_depth                  scope_depth = depth_scope_stack;
  a_type_ptr                     inst_placeholder;

  db_enter(4, "add_placeholder_for_class_instantiation");
  /* This function should not be called for a partial instantiation. */
  check_assertion_str2(type_ptr->variant.class_struct_union.
                                   extra_info->assoc_scope != NULL,
                       "add_placeholder_for_class_instantiation",
                       "class is not fully instantiated");
  ssep = &scope_stack[scope_depth];
#if CHECKING
  while (ssep->kind == (a_scope_kind)sck_class_reactivation) {
    --scope_depth;
    --ssep;
  }  /* while */
  check_assertion(ssep->kind == (a_scope_kind)sck_template_instantiation);
#endif /* CHECKING */
  /* Find the nearest enclosing class scope, if any. */
  for (--scope_depth; scope_depth > DEPTH_OF_FILE_SCOPE; --scope_depth) {
    --ssep;
    if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
        !ssep->assoc_type->variant.class_struct_union.is_nonreal_class) {
      /* Found a class scope. */
      break;
    }  /* if */
  }  /* for */
  if (scope_depth > DEPTH_OF_FILE_SCOPE) {
    /* At this point, ssep should point to a real class scope. */
    if (ssep->inside_local_class) {
      /* A placeholder is not needed for an instantiation within a
         function definition. */
    } else if (type_ptr->source_corresp.is_class_member &&
               type_ptr->source_corresp.parent.class_type ==
                                                    ssep->assoc_type) {
      /* Nor is a placeholder needed within the class to which a member
         template class instance belongs. */
    } else {
      /* Allocate the placeholder type, set its fields, and add it to the
         types list of the enclosing class.  Note that this typeref has no
         name or symbol associated with it. */
      inst_placeholder = alloc_type((a_type_kind)tk_typeref);
      inst_placeholder->variant.typeref.type = type_ptr;
      set_class_membership((a_symbol_ptr)NULL,
                           &inst_placeholder->source_corresp,
                           ssep->assoc_type);
      inst_placeholder->
             variant.typeref.is_placeholder_for_class_instantiation = TRUE;
      type_ptr->variant.class_struct_union.
              referenced_by_class_instantiation_placeholder_typeref = TRUE;
      add_to_types_list(inst_placeholder, scope_depth);
    }  /* if */
  }  /* if */
  db_exit();
}  /* add_placeholder_for_class_instantiation */


void add_to_types_list(a_type_ptr     type_ptr,
                       a_scope_depth  scope_level)
/*
Add the given type to the types list for the scope corresponding to
scope_level.  When scope_level is NO_SCOPE_DEPTH, the scope is computed
rather than determined directly.
*/
{
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;

  /* Get a pointer to the current or file scope entry. */
  /* Note:  If we are currently inside the declaration list for the old-style
     parameters of a function -- e.g., in the "struct" declaration in

       int f(a) struct s {int b;} a; { }

     -- the type should be entered in the prototype scope; that makes the
     type available in the memory region of the function's parent, which is
     necessary for type-compatibility checking of parameters. A similar
     situation applies for type declarations that are part of function
     prototypes, as in

       int f(struct s {int b;} a);

     The prototype scope is hardly ever needed, and therefore it is not
     allocated by default.  It is allocated in ensure_il_scope_exists, which
     is called by get_scope_for_list. */
  sp = get_scope_for_list(scope_level, &type_ptr->source_corresp,
                          &pointers_block);
  if (sp == NULL) {
    /* May be an error case. */
  } else {
    /* Add the type to the list of types for this scope. */
    if (sp->types == NULL) {
      sp->types = type_ptr;
    } else if (pointers_block != NULL) {
      pointers_block->last_type->next = type_ptr;
    } else {
      /* The scope stack entry is no longer on the stack, so just look for
         the end of the types list and add the new type. */
      a_type_ptr tp = sp->types;
      while (tp->next != NULL) tp = tp->next;
      tp->next = type_ptr;
    }  /* if */
    type_ptr->next = NULL;
    if (pointers_block != NULL) pointers_block->last_type = type_ptr;
#if DEBUG
    if (db_flag_is_set("dump_type_lists")) {
      fprintf(f_debug, "Added to types list:  ");
      db_abbreviated_type(type_ptr);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    if (!C_mode()
#if DO_IL_LOWERING
        && !il_lowering_underway
#endif /* DO_IL_LOWERING */
                                ) {
      if (sp->kind == (a_scope_kind)sck_namespace) {
        /* We are adding a type to the types list of a namespace scope.  Add
           a placeholder type to the types list of the filescope -- it's used
           by IL lowering to get the order right when it promotes namespace
           types to the file scope. */
        add_placeholder_for_namespace_type(type_ptr);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* add_to_types_list */


void move_to_end_of_types_list(a_type_ptr     type_ptr,
                               a_scope_depth  scope_level)
/*
Move the indicated type, which is already on the types list of an IL scope,
to the end of that list.  Use scope_level to find the appropriate IL scope.
When scope_level is NO_SCOPE_DEPTH, the scope is computed rather than
determined directly.
*/
{
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;
  a_type_ptr                  tp, prev_tp;

  /* Get a pointer to the scope entry. */
  sp = get_scope_for_list(scope_level, &type_ptr->source_corresp,
                          &pointers_block);
  if (sp == NULL) {
    /* May be an error case. */
  } else {
    if (pointers_block->last_type == type_ptr) {
      /* It's already the last entry on the list. */
    } else {
      /* Scan the list until a match is found. */
      prev_tp = NULL;
      tp = sp->types;
      while (tp != type_ptr) {
        prev_tp = tp;
        tp = tp->next;
        check_assertion_str2(tp != NULL, "move_to_end_of_types_list:",
                             "cannot find type on types list");
      }  /* while */
      /* Link around the entry. */
      if (prev_tp == NULL) {
        sp->types = type_ptr->next;
      } else {
        prev_tp->next = type_ptr->next;
      }  /* if */
      /* Reenter it onto the end of the list. */
      pointers_block->last_type->next = type_ptr;
      pointers_block->last_type = type_ptr;
      type_ptr->next = NULL;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("dump_type_lists")) {
      fprintf(f_debug, "Moved to end of list: ");
      db_abbreviated_type(type_ptr);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    if (!C_mode()
#if DO_IL_LOWERING
        && !il_lowering_underway
#endif /* DO_IL_LOWERING */
                                ) {
      if (sp->kind == (a_scope_kind)sck_namespace) {
        /* Move the associated placeholder typedef (there ought to be one) to
           the end of the file-scope types list. */
        a_scope_stack_entry_ptr     ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];

        pointers_block = assoc_pointers_block_of(ssep);
        tp = pointers_block->last_type;
        if (is_assoc_namespace_type_placeholder(tp, type_ptr)) {
          /* The placeholder entry is already the last entry. */
        } else {
          /* Scan the list until a match is found. */
          prev_tp = NULL;
          for (tp = ssep->il_scope->types;; tp = tp->next) {
            check_assertion(tp != NULL);
            if (is_assoc_namespace_type_placeholder(tp, type_ptr)) {
              break;
            }  /* if */
            prev_tp = tp;
          }  /* for */
          /* Link around the entry that was found. */
          if (prev_tp == NULL) {
            ssep->il_scope->types = tp->next;
          } else {
            prev_tp->next = tp->next;
          }  /* if */
          /* Reenter it onto the end of the list. */
          pointers_block->last_type->next = tp;
          pointers_block->last_type = tp;
          tp->next = NULL;
#if DEBUG
          if (db_flag_is_set("dump_type_lists")) {
            fprintf(f_debug, "Moved to end of list: ");
            db_abbreviated_type(tp);
            fprintf(f_debug, "\n");
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* move_to_end_of_types_list */


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


#if MICROSOFT_EXTENSIONS_ALLOWED
a_type_ptr microsoft_sized_integer_type(an_integer_kind kind)
/*
Make or find a type entry for a sized integer type (__intN) of the indicated
kind, and return a pointer to it.  (This is for the case of a "__intN" type
that Microsoft Visual C++ 6.0 (and later?) treats as a distinct built-in type
as opposed to just a typedef for another integral type.)
*/
{
  a_type_ptr pit;

  if (microsoft_sized_int_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pit = microsoft_sized_int_types[kind];
  } else {
    /* The type must be created. */
    pit = alloc_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = kind;
    pit->variant.integer.microsoft_sized_int_type = TRUE;
    set_type_size(pit);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pit, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
    microsoft_sized_int_types[kind] = pit;
  }  /* if */
  return pit;
}  /* microsoft_sized_integer_type */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


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


#if MICROSOFT_EXTENSIONS_ALLOWED
a_type_ptr microsoft_sized_signed_integer_type(an_integer_kind kind)
/*
Make or find a type entry for an explicitly signed integer type of the
indicated kind, and return a pointer to it.  Keeping track of the difference
between, e.g., a plain "__int32" and a "signed __int32" is necessary because
the two may be handled differently for bit fields.  Should only be called for
the kinds corresponding to __int16, __int32 and __int64.  (This is for the
case of a "__intN" type that Microsoft Visual C++ 6.0 (and later?) treats as a
distinct built-in type as opposed to just a typedef for another integral
type.)
*/
{
  a_type_ptr pit;

  if (microsoft_sized_signed_int_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pit = microsoft_sized_signed_int_types[kind];
  } else {
    /* The type must be created. */
#if CHECKING
    if (kind != targ_int16_int_kind &&
        kind != targ_int32_int_kind &&
        kind != targ_int64_int_kind) {
      internal_error("microsoft_sized_signed_integer_type: bad int kind");
    }  /* if */
#endif /* CHECKING */
    pit = alloc_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = kind;
    pit->variant.integer.explicitly_signed = TRUE;
    pit->variant.integer.microsoft_sized_int_type = TRUE;
    set_type_size(pit);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pit, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
    microsoft_sized_signed_int_types[kind] = pit;
  }  /* if */
  return pit;
}  /* microsoft_sized_signed_integer_type */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


a_type_ptr wchar_t_type(void)
/*
Make or find a type entry for a wchar_t type and return a pointer to it.
This is only used when wchar_t is a distinct type.
*/
{
  a_type_ptr pit;

  if (il_wchar_t_type != NULL) {
    /* The type has previously been created, and can be reused. */
    pit = il_wchar_t_type;
  } else {
    /* The type must be created. */
    il_wchar_t_type = pit = alloc_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = targ_wchar_t_int_kind;
    pit->variant.integer.wchar_t_type = TRUE;
    set_type_size(pit);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pit, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pit;
}  /* wchar_t_type */

#if C99_IL_EXTENSIONS_SUPPORTED

a_boolean bool_type_used(void)
/*
Return TRUE if the bool type has been used.
*/
{
  return il_bool_type != NULL;
}  /* bool_type_used */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

a_type_ptr bool_type(void)
/*
Make or find a type entry for a bool type and return a pointer to it.
*/
{
  a_type_ptr pit;

  if (il_bool_type != NULL) {
    /* The type has previously been created, and can be reused. */
    pit = il_bool_type;
  } else {
    /* The type must be created. */
    il_bool_type = pit = alloc_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = targ_bool_int_kind;
    pit->variant.integer.bool_type = TRUE;
    set_type_size(pit);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pit, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pit;
}  /* bool_type */


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

#if C99_IL_EXTENSIONS_SUPPORTED

a_boolean complex_type_used(a_float_kind kind)
/*
Return TRUE if the complex type of the indicated kind was used so far.
*/
{
  return complex_types[kind] != NULL;
}  /* complex_type_used */


a_type_ptr complex_type(a_float_kind kind)
/*
Make or find a type entry for a complex type of the indicated kind, and
return a pointer to it.
*/
{
  a_type_ptr pft;

  if (complex_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pft = complex_types[kind];
  } else {
    /* The type must be created. */
    complex_types[kind] = pft = alloc_type((a_type_kind)tk_complex);
    pft->variant.float_kind = kind;
    set_type_size(pft);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pft, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pft;
}  /* complex_type */


a_boolean imaginary_type_used(a_float_kind kind)
/*
Return TRUE if the imaginary type of the indicated kind was used so far.
*/
{
  return imaginary_types[kind] != NULL;
}  /* imaginary_type_used */


a_type_ptr imaginary_type(a_float_kind kind)
/*
Make or find a type entry for a imaginary type of the indicated kind, and
return a pointer to it.
*/
{
  a_type_ptr pft;

  if (imaginary_types[kind] != NULL) {
    /* The type has previously been created, and can be reused. */
    pft = imaginary_types[kind];
  } else {
    /* The type must be created. */
    imaginary_types[kind] = pft = alloc_type((a_type_kind)tk_imaginary);
    pft->variant.float_kind = kind;
    set_type_size(pft);
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pft, (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pft;
}  /* imaginary_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

a_type_ptr string_type(a_targ_size_t num_chars)
/*
Make or find an entry for the type of a string literal of length num_chars
characters, and return a pointer to it.
*/
{
  a_type_ptr pst, elem_type;

  if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH &&
      string_types[num_chars] != NULL) {
    /* The type has previously been created, and can be reused. */
    pst = string_types[num_chars];
  } else {
    /* The type must be created. */
    pst = alloc_type((a_type_kind)tk_array);
    elem_type = integer_type(plain_char_int_kind);
    if (string_literals_are_const) {
      /* The element type is CONST char. */
      elem_type = make_qualified_type(elem_type, TQ_CONST);
    }  /* if */
    pst->variant.array.element_type = elem_type;
    pst->variant.array.variant.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      string_types[num_chars] = pst;
    }  /* if */
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pst,
                                     (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pst;
}  /* string_type */


a_type_ptr wide_string_type(a_targ_size_t num_chars)
/*
Make or find an entry for the type of a wide string literal of length
num_chars characters, and return a pointer to it.
*/
{
  a_type_ptr pst;

  if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH &&
      wide_string_types[num_chars] != NULL) {
    /* The type has previously been created, and can be reused. */
    pst = wide_string_types[num_chars];
  } else {
    /* The type must be created. */
    a_type_ptr	elem_type;
    pst = alloc_type((a_type_kind)tk_array);
    if (wchar_t_is_keyword) {
      elem_type = wchar_t_type();
    } else {
      elem_type = integer_type((an_integer_kind)targ_wchar_t_int_kind);
    }  /* if */
    if (string_literals_are_const) {
      /* The element type is CONST wchar_t. */
      elem_type = make_qualified_type(elem_type, TQ_CONST);
    }  /* if */
    pst->variant.array.element_type = elem_type;
    pst->variant.array.variant.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      wide_string_types[num_chars] = pst;
    }  /* if */
#if ORPHAN_PROCESSING_NEEDED
    /* Record the type entry as an orphan in case it is discarded now
       and then found again in a later phase (e.g., IL lowering). */
    add_orphaned_file_scope_il_entry((char *)pst,
                                     (an_il_entry_kind)iek_type);
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
  return pst;
}  /* wide_string_type */


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


void do_based_type_fixup(void)
/*
Go though the based-type fixup list.  For each type specified, go through
the associated based-type list and remove any entries marked as front-end
only.
*/
{
  a_based_type_fixup_ptr        btfp = based_type_fixup_list;
  a_type_ptr                    tp;
  a_based_type_list_member_ptr  btlmp, prev_btlmp, next_btlmp;

  /* Traverse the based-type fixup list. */
  for (; btfp != NULL; btfp = btfp->next) {
    /* For each type on the list, traverse the associated based-type list
       and look for based-type entries marked front-end-only. */
    tp = btfp->base_type;
    prev_btlmp = NULL;
    for (btlmp = tp->based_types; btlmp != NULL; btlmp = next_btlmp) {
      next_btlmp = btlmp->next;
      if (btlmp->front_end_only) {
        /* Match -- link around the entry and continue looping. */
        if (prev_btlmp == NULL) {
          tp->based_types = btlmp->next;
        } else {
          prev_btlmp->next = btlmp->next;
        }  /* if */
        /* Leave prev_btlmp set as is. */
      } else {
        prev_btlmp = btlmp;
      }  /* if */
    }  /* for */
  }  /* for */
  based_type_fixup_list = NULL;
}  /* do_based_type_fixup */


static void add_to_based_type_fixup_list(a_type_ptr  base_type)
/*
Look for an entry on the based-type fixup list that points to base_type.
If none is found, create one and add it to the list.
*/
{
  a_based_type_fixup_ptr  btfp, prev_btfp;

  if (!prototype_instantiations_in_il &&
      is_template_dependent_type(base_type)) {
    /* Don't bother adding a fixup entry for base types that shouldn't escape
       the front end anyway.  In other words, if T is a template param type,
       we don't need a fixup entry for T A<T>::* but we do need one for
       int A<T>::* (in which examples the base-types are T and int,
       respectively), because the base_type T is not going to be in
       the IL, but the base type int is. */
  } else {
    prev_btfp = NULL;
    for (btfp = based_type_fixup_list; btfp != NULL; btfp = btfp->next) {
      if (btfp->base_type == base_type) {
        /* The specified base type is already represented on the fixup list.
           Unless it's already there, move the entry to the head of the
           list (as an optimization for subsequent traversals of the list). */
        if (prev_btfp != NULL) {
          /* Remove the entry and re-add it at the front. */
          prev_btfp->next = btfp->next;
          btfp->next = based_type_fixup_list;
          based_type_fixup_list = btfp;
        }  /* if */
        goto done;
      }  /* if */
      prev_btfp = btfp;
    }  /* for */
    /* Falling through means a match was not found. */
    btfp = (a_based_type_fixup_ptr)alloc_fe(sizeof(a_based_type_fixup));
#if DEBUG
    num_based_type_fixups_allocated++;
#endif /* DEBUG */
    btfp->base_type = base_type;
    /* Add the entry to the start of the list. */
    btfp->next = based_type_fixup_list;
    based_type_fixup_list = btfp;
  }  /* if */
done:;
}  /* add_to_based_type_fixup_list */


#if !NEAR_AND_FAR_ALLOWED
/*ARGSUSED*/  /* <-- Because expl_mem_attr_implicit is only used when near
                     and far may be recognized. */
#endif /* !NEAR_AND_FAR_ALLOWED */
static a_type_ptr get_based_type(a_type_ptr            base_type,
                                 a_based_type_kind     kind,
                                 a_type_qualifier_set  qualifiers,
                                 a_boolean             expl_mem_attr_implicit,
                                 a_type_ptr            class_type)
/*
Search the based_types list of base_type to see if it contains a based type
of the kind indicated by "kind".  If the kind is "btk_qualified", the
"qualifiers" parameter must match the "qualifiers" field of the based type,
and expl_mem_attr_implicit must match the
explicit_memory_attribute_made_implicit flag of the based type (that's used
for memory attributes like near/far).  If the kind is
"btk_ptr_to_member", the specified "class_type" must also match
"class_of_which_a_member" of that based type.  Return a pointer to the
type if such an entry exists, or NULL if no such entry exists.  The
based_types list is used to hold pointers to types based on the base
type, so that only one copy of pointer-to that type, reference-to that
type, etc., is allocated.  As a simple optimization to based type lookup,
move the desired based type, if found, to the front of the list.  This
will tend to keep frequently-asked-for based types at the front of the
list.
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
      ptr = btlmp->based_type;
      if (kind == (a_based_type_kind)btk_ptr_to_member &&
          ptr->variant.ptr_to_member.class_of_which_a_member != class_type) {
        /* Pointer-to-member parent class does not match class type -- keep
           looking. */
        ptr = NULL;
      } else if (kind == (a_based_type_kind)btk_qualified &&
                 (ptr->variant.typeref.qualifiers != qualifiers
#if NEAR_AND_FAR_ALLOWED
                  || (a_boolean)ptr->variant.typeref.
                                     explicit_memory_attribute_made_implicit
                                                  != expl_mem_attr_implicit
#endif /* NEAR_AND_FAR_ALLOWED */
                                                                           )) {
        /* Qualifiers do not match -- keep looking. */
        ptr = NULL;
      } else {
        /* This is a match. */
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
file scope memory region.  This routine does not check to see if there
is already an entry of the indicated kind on the list.
*/
{
  a_based_type_list_member_ptr btlmp;

  btlmp = alloc_based_type_list_member(kind);
  btlmp->based_type = based_type;
  /* Add the entry to the front of the existing based_types list. */
  btlmp->next = base_type->based_types;
  base_type->based_types = btlmp;
  if (!prototype_instantiations_in_il &&
      btlmp->kind == (a_based_type_kind)btk_ptr_to_member) {
    a_type_ptr tp = based_type->variant.ptr_to_member.class_of_which_a_member;
    if (!is_class_struct_union_type(tp) ||
        tp->variant.class_struct_union.is_nonreal_class) {
      /* Either the class specified in this ptr-to-member type is a template
         param type or a nonreal instantiation -- update the fixup list so
         this type doesn't leak into the back end.  Pointers-to-members
         are odd, because the base type is the member type, so a
         template-dependent pointer to member type can be attached to
         a base type that is not template-dependent.  The base type must
         stay in the IL, and the based type entry must not, so we
         arrange to remove the based type entry later in a fixup pass. */
      btlmp->front_end_only = TRUE;
      add_to_based_type_fixup_list(base_type);
    }  /* if */
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  /* Don't set keep-in-il or needed flags on the based type if it is a
     front-end-only type. */
  if (!btlmp->front_end_only) {
    /* If the "needed" or "keep_in_il" flag has already been set on this
       type, clear it and set it again to ensure that the entry on the based
       types list is visited if required.  This is done by calling
       remark_as_needed/mark_to_keep_in_il for the primary entry, rather
       than for the based types list member, because the marking process
       ignores some based type entries and we don't want to duplicate the
       logic for that here. */
    remark_as_needed((char *)base_type, iek_type);
    if (il_entry_prefix_of(base_type).keep_in_il) {
      il_entry_prefix_of(base_type).keep_in_il = FALSE;
      mark_to_keep_in_il((char *)base_type, iek_type);
    }  /* if */
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* add_based_type_list_member */


a_type_ptr check_ptr_to_member_function_type(a_type_ptr  member_type,
                                             a_type_ptr  class_type)
/*
This routine is called when forming a pointer-to-member type.  If
member_type is a function type, be sure it is represented as a nonstatic
member function of class_type -- that is, set the implicit-this parameter
type if necessary.  Also verify that the name linkage on the member type
is not "C".
*/
{
  a_routine_type_supplement_ptr  rtsp;
  a_type_ptr                     tp;

  check_assertion(class_type != NULL && member_type != NULL);
  tp = skip_typerefs(member_type);
  if (is_function_type(tp)) {
    rtsp = tp->variant.routine.extra_info;
    if (rtsp->this_class == NULL ||
        rtsp->routine_name_linkage == (a_name_linkage_kind)nlk_external) {
      /* Before updating it, copy the routine type if it might be shared. */
      if (tp->variant.routine.return_type != NULL) {
        /* If the return type is not set, the type is still under construction
           and hence certainly not shared. */
        member_type =
           copy_routine_type_with_param_types(tp, /*copy_default_args=*/TRUE);
        rtsp = member_type->variant.routine.extra_info;
      }  /* if */
      if (rtsp->this_class == NULL) {
        rtsp->this_class = class_type;
      }  /* if */
      if (rtsp->routine_name_linkage == (a_name_linkage_kind)nlk_external) {
        /* Change from C linkage to C++ linkage: */
        rtsp->routine_name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
      }  /* if */
    }  /* if */
  }  /* if */
  return member_type;
}  /* check_ptr_to_member_function_type */


a_type_ptr ptr_to_member_type(a_type_ptr  member_type,
                              a_type_ptr  class_type)
/*
Allocate and return a pointer-to-member type, initializing its fields based
on the specified member and class types.  Attempt to find and reuse an
existing type entry.
*/
{
  a_type_ptr                     tp;

  class_type = skip_typerefs(class_type);
  if (is_template_param_type(class_type)) {
    /* The class type is a template parameter.  Substitute the template
       parameter's proxy class. */
    class_type = proxy_class_for_template_param(class_type);
  }  /* if */
  if (member_type != NULL && is_function_type(member_type)) {
    /* This is a pointer-to-member-function type.  Be sure the implicit this
       parameter is set.  If not, create it based on class_type. */
    member_type = check_ptr_to_member_function_type(member_type, class_type);
  }  /* if */
  /* Check if this is an incomplete type being formed. */
  if (member_type != NULL) {
    /* See if a pointer-to-member type such as the one being requested has
       already been allocated.  If one was allocated, a pointer to it is
       stored in the based_types list of the member type, and the pointer
       can be reused. */
    tp = get_based_type(member_type, (a_based_type_kind)btk_ptr_to_member,
                        (a_type_qualifier_set)TQ_NONE,
                        /*expl_mem_attr_implicit=*/FALSE,
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
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode) {
        /* A pointer-to-member declaration involving a given class type
           locks in the inheritance kind (i.e., the pointer-to-member
           representation). */
        a_class_type_supplement_ptr ctsp =
                             class_type->variant.class_struct_union.extra_info;

        /* Force instantiation of template class. */
        instantiate_template_class(class_type);
        if (ctsp->inheritance_kind == (an_inheritance_kind)ihk_none) {
          if (!is_incomplete_type(class_type)) {
            check_inheritance_kind(class_type, default_inheritance_kind,
                                   &error_position);
          }  /* if */
          ctsp->inheritance_kind = default_inheritance_kind;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
  if (is_function_type(member_type)) {
    a_routine_type_supplement_ptr  old_rtsp =
                       skip_typerefs(member_type)->variant.routine.extra_info;
    a_type_ptr                     old_this_class = old_rtsp->this_class;

    /* A function type under a pointer-to-member must be a member function
       and therefore must have a "this" parameter type. */
    check_assertion(old_this_class != NULL);
    if (old_this_class != class_type) {
      /* Make a new function type with the right "this" class.  Note that
         there is no sharing of types going on here, so this may be
         wasteful if called a lot. */
      a_type_ptr                     new_member_type;
      a_routine_type_supplement_ptr  new_rtsp;

      new_member_type =
               copy_routine_type_with_param_types(member_type,
                                                  /*copy_default_args=*/FALSE);
      new_rtsp = skip_typerefs(new_member_type)->variant.routine.extra_info;
      new_rtsp->this_class = class_type;
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


a_type_ptr make_pointer_type(a_type_ptr pointed_to_type)
/*
Allocate a pointer type record and initialize it.  Attempt to find and reuse
an existing entry if possible.
*/
{
  register a_type_ptr ptr;

  /* See if a pointer type for the type pointed to has already been allocated.
     If one was allocated, a pointer to it is stored in the based_types list
     for the base type, and the pointer type can be reused. */
  ptr = get_based_type(pointed_to_type, (a_based_type_kind)btk_pointer,
                       (a_type_qualifier_set)TQ_NONE,
                       /*expl_mem_attr_implicit=*/FALSE,
                       /*class_type=*/(a_type_ptr)NULL);
  if (ptr == NULL) {
    /* No allocated entry, need to allocate one. */
    ptr = alloc_type((a_type_kind)tk_pointer);
    ptr->variant.pointer.type = pointed_to_type;
    set_type_size(ptr);
    /* Remember the existence of this pointer type by putting a pointer
       to it in the based_types list. */
    add_based_type_list_member(pointed_to_type, (a_based_type_kind)btk_pointer,
                               ptr);
  }  /* if */

  return ptr;
}  /* make_pointer_type */


#if MICROSOFT_EXTENSIONS_ALLOWED
a_type_ptr make_based_pointer_type(a_type_ptr     pointed_to_type,
				   a_variable_ptr variable)
/*
Allocate a pointer type for a based pointer, and initialize it.
*/
{
  register a_type_ptr ptr;

  /* No allocated entry, need to allocate one. */
  ptr = alloc_type((a_type_kind)tk_pointer);
  ptr->variant.pointer.type = pointed_to_type;
  ptr->variant.pointer.base_variable = variable;
  set_type_size(ptr);
  return ptr;
}  /* make_pointer_type */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


a_type_ptr make_reference_type(a_type_ptr pointed_to_type)
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
  ptr = get_based_type(pointed_to_type, (a_based_type_kind)btk_reference,
                       (a_type_qualifier_set)TQ_NONE,
                       /*expl_mem_attr_implicit=*/FALSE,
                       /*class_type=*/(a_type_ptr)NULL);
  if (ptr == NULL) {
    /* No allocated entry, need to allocate one. */
    ptr = alloc_type((a_type_kind)tk_pointer);
    ptr->variant.pointer.type = pointed_to_type;
    ptr->variant.pointer.is_reference = TRUE;
    set_type_size(ptr);
    /* Remember the existence of this reference type by putting a pointer
       to it in the based_types list. */
    add_based_type_list_member(pointed_to_type,
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


a_type_ptr make_qualified_type(a_type_ptr            base_type,
                               a_type_qualifier_set  qualifiers)
/*
Make a version of the type base_type with the additional type qualifiers
indicated by the set of flags in "qualifiers".  Attempt to find and reuse
an existing entry if possible.  The new qualifiers are added only if they
are not already present.
*/
{
  a_type_ptr            orig_base_type, ptr;
  a_boolean             is_array = FALSE, expl_mem_attr_implicit = FALSE;
  a_type_qualifier_set  base_type_qualifiers;
  a_type_qualifier_set  qualifiers_to_add;

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
  /* Applying qualifiers to a function type is not allowed in C++.  The
     check, if needed, should have been done by the caller. */
#if NEAR_AND_FAR_ALLOWED
  check_assertion(!is_function_type(base_type) || qualifiers == TQ_NONE ||
                  (near_and_far_enabled() &&
                   (qualifiers & ~(TQ_NEAR | TQ_FAR)) == TQ_NONE) ||
                  C_mode());
#else /* !NEAR_AND_FAR_ALLOWED */
  check_assertion(!is_function_type(base_type) || qualifiers == TQ_NONE ||
                  C_mode());
#endif /* NEAR_AND_FAR_ALLOWED */
  base_type_qualifiers = get_type_qualifiers(base_type);
  qualifiers_to_add = qualifiers & ~base_type_qualifiers;
  if (qualifiers_to_add != TQ_NONE) {
#if NEAR_AND_FAR_ALLOWED
    if (qualifiers_to_add & (TQ_NEAR | TQ_FAR)) {
       /* Don't add explicit qualifiers for memory attributes that are
         implied anyway.  Note that even if only one qualifier is being
         added and it's implied, we add a typeref (containing no qualifiers)
         so we can put on the explicit_memory_attribute_made_implicit flag. */
      a_type_qualifier_set implied_qualifier =
                                     is_far_type(base_type) ? TQ_FAR : TQ_NEAR;
      if (qualifiers_to_add & implied_qualifier) {
        /* A qualifier being added is implied.  Don't add it, but record
           that it was explicit. */
        qualifiers_to_add &= ~implied_qualifier;
        expl_mem_attr_implicit = TRUE;
      }  /* if */
    }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
    /* Some qualifiers need to be added. */
    if (base_type_qualifiers != TQ_NONE) {
      /* The typeref(s) containing qualifiers, if any, are removed to get down
         to the real base type, to which the new qualifiers are added.  When
         a qualifier is removed, a flag must be set so that it will be added
         back. */
      while (base_type->kind == (a_type_kind)tk_typeref) {
        if (typeref_is_typedef(base_type)) {
          /* This is a typedef -- preserve it, so that the qualifier is built
             on top of it. */
          break;
        }  /* if */
        qualifiers_to_add |= base_type->variant.typeref.qualifiers;
        base_type = base_type->variant.typeref.type;
      }  /* while */
    }  /* if */
    /* See if the properly qualified version of base_type already exists.
       If so, a pointer to it is stored in the based_types for base_type. */
    ptr = get_based_type(base_type, (a_based_type_kind)btk_qualified,
                         qualifiers_to_add,
                         expl_mem_attr_implicit,
                         /*class_type=*/(a_type_ptr)NULL);
    if (ptr == NULL) {
      /* No allocated entry, need to allocate one. */
      ptr = alloc_type((a_type_kind)tk_typeref);
      ptr->variant.typeref.type = base_type;
      ptr->variant.typeref.qualifiers = qualifiers_to_add;
#if NEAR_AND_FAR_ALLOWED
      ptr->variant.typeref.explicit_memory_attribute_made_implicit =
                                                        expl_mem_attr_implicit;
#endif /* NEAR_AND_FAR_ALLOWED */
      /* Remember the existence of this typeref type by putting a pointer
         to it in the based_types list. */
      add_based_type_list_member(base_type, (a_based_type_kind)btk_qualified,
                                 ptr);
    }  /* if */
    if (is_array) {
      /* For the strange array case, the array type entries must be
         copied in case they are shared. */
      ptr = copy_array_type_replacing_element_type(orig_base_type, ptr);
      /* Save a pointer to the original type on the based types list for
         the new type created by the copy. */
      add_based_type_list_member(ptr,
                                 (a_based_type_kind)btk_unqualified_array_type,
                                 orig_base_type);
    }  /* if */
  } else {
    /* No qualifiers to add, so return the original type. */
    ptr = orig_base_type;
  }  /* if */

  return ptr;
}  /* make_qualified_type */


a_type_ptr make_unqualified_type(a_type_ptr type)
/*
Return a type that is the unqualified version of the type given by type.
This differs from skip_typerefs in that it preserves typedefs where possible.
Note that this is not the routine to use to drop qualifiers when changing
to an rvalue type, except possibly for C-mode-only code; see rvalue_type
instead.
*/
{
  a_type_ptr  element_type;

  if (is_array_type(type)) {
    /* There can never be type qualifiers on top of an array type.  If there
       are qualifiers, they are attached to the element type. */
    if (C_mode()) {
      /* In C array-of-const-int (for example) is not considered a qualified
         type -- is_qualified_type will not return TRUE for it, so nothing
         more needs to be done to make it unqualified. */
    } else {
      /* In C++ array-of-const-int *is* a qualified type.  Remove the
         qualifiers from the element type and create another array type. */
      element_type = underlying_array_element_type(type);
      if (element_type == NULL) {
        /* Array-of-NULL is a possible temporary state during construction of
           a derived type. */
      } else {
        element_type = make_unqualified_type(element_type);
        type = copy_array_type_replacing_element_type(type, element_type);
      }  /* if */
    }  /* if */
  } else {
    /* Non-array case.  Remove the minimum number of typerefs that will
       produce an unqualified type.  It's done in a loop in order to save
       typedefs if possible. */
    while (is_top_level_qualified_type(type)) {
      type = type->variant.typeref.type;
    }  /* while */
  }  /* if */
  return type;
}  /* make_unqualified_type */


a_type_ptr rvalue_type(a_type_ptr type)
/*
type is the type of an lvalue.  Return the type that the associated rvalue
would have.  That is, drop type qualifiers as appropriate.  Array-to-pointer
and function-to-pointer decay are not considered.
*/
{
  /* In C++, class rvalues can have cv-qualified type, so the cv-qualifiers
     are kept.  In all other cases, they are removed. */
  if (C_mode() ||
#if DO_IL_LOWERING
      /* Force C semantics for things created by IL lowering. */
      il_lowering_underway ||
#endif /* DO_IL_LOWERING */
      !is_class_struct_union_type(type)) {
    type = make_unqualified_type(type);
  }  /* if */
  return type;
}  /* rvalue_type */


a_type_ptr return_type_of(a_type_ptr routine_type)
/*
Return the type that is the return type of the given function type.  If
the function returns a reference, the type is the type of the lvalue returned.
Otherwise, it is the type of the rvalue returned.
*/
{
  a_type_ptr return_type;

  routine_type = skip_typerefs(routine_type);
  return_type = routine_type->variant.routine.return_type;
  if (is_reference_type(return_type)) {
    /* The function returns a reference type.  Drop the reference to
       get to the underlying lvalue type. */
    return_type = type_pointed_to(return_type);
  } else {
    /* The conversion function returns a non-reference type, i.e.,
       an rvalue.  Drop cv-qualifiers as appropriate. */
    return_type = rvalue_type(return_type);
  }  /* if */
  return return_type;
}  /* return_type_of */


a_type_ptr il_return_type_of(a_type_ptr routine_type)
/*
Return the type that is the return type of the given function type, as
it would appear as the type on a call of the function in the IL.
*/
{
  a_type_ptr return_type;

  routine_type = skip_typerefs(routine_type);
  return_type = routine_type->variant.routine.return_type;
  if (is_reference_type(return_type)) {
    /* If the function returns a reference type, make the result a
       pointer. */
    return_type = make_pointer_type(type_pointed_to(return_type));
  } else {
    /* The function returns a non-reference type, so the result is an
       rvalue and cv-qualifiers should be dropped appropriately. */
    return_type = rvalue_type(return_type);
  }  /* if */
  return return_type;
}  /* il_return_type_of */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_vla_dimension_ptr find_vla_dimension(a_type_ptr array_type)
/*
Find the VLA (variable-length array) dimension entry associated with the
given array type, and return a pointer to it.  innermost_function_scope
must be set to the scope for the current function.
*/
{
  a_vla_dimension_ptr vlap;

  check_assertion_str(innermost_function_scope != NULL,
                      "find_vla_dimension: innermost_function_scope is NULL");
  for (vlap = innermost_function_scope->vla_dimensions;
       ;
       vlap = vlap->next) {
    check_assertion_str(vlap != NULL, "find_vla_dimension: not found");
    if (vlap->type == array_type) break;
  }  /* for */
  return vlap;
}  /* find_vla_dimension */

#if !STANDALONE_UTILITY_PROGRAM

a_type_ptr make_field_selection_type(a_field_ptr           field,
                                     a_type_qualifier_set  qualifiers)
/*
Return a type based on the type of the specified field, with the indicated
qualifiers added.  However, if the field was declared mutable, "const" in
the qualifier set is ignored.
*/
{
  /* The selected field has all the type qualifiers of both the field
     and the selecting pointer -- except that const is removed if the
     field was declared to be mutable. */
  if (field->is_mutable) qualifiers &= ~TQ_CONST;
  return make_qualified_type(field->type, qualifiers);
}  /* make_field_selection_type */


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
  a_type_qualifier_set  type1_qualifiers;
  a_type_qualifier_set  type2_qualifiers;
  a_type_qualifier_set  qualifiers;
  a_type_ptr  tp1 = *type1, tp2 = *type2;

  type1_qualifiers = get_type_qualifiers(tp1);
  type2_qualifiers = get_type_qualifiers(tp2);
  if (type1_qualifiers != TQ_NONE && type2_qualifiers != TQ_NONE) {
    /* Strip off the qualifiers. */
    tp1 = skip_typerefs(tp1);
    tp2 = skip_typerefs(tp2);
    if (type1_qualifiers ^ type2_qualifiers) {
      /* They are differently qualified.  The qualifiers have been stripped
         off; add them back on as appropriate. */
      qualifiers = type1_qualifiers & ~type2_qualifiers;
      if (qualifiers != TQ_NONE) {
        tp1 = make_qualified_type(tp1, qualifiers);
      }  /* if */
      qualifiers = type2_qualifiers & ~type1_qualifiers;
      if (qualifiers != TQ_NONE) {
        tp2 = make_qualified_type(tp2, qualifiers);
      }  /* if */
    } else {
      /* Either both are const, both are volatile, or both are const volatile.
         Return both types with all qualifiers stripped off. */
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
  a_dependent_type_fixup_kind   dtf_kind;

  from_kind = from->kind;
  /* If one type is a routine, both must be. */
  check_assertion((from_kind == (a_type_kind)tk_routine) ==
                  (to->kind == (a_type_kind)tk_routine));
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  to->source_corresp.decl_pos_info = NULL;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (from_kind == (a_type_kind)tk_array ||
      from_kind == (a_type_kind)tk_routine) {
    if (from_kind == (a_type_kind)tk_routine) {
      /* For a routine type, the type supplement must also be copied. */
      *extra_info = *from->variant.routine.extra_info;
      to->variant.routine.extra_info = extra_info;
      tp = skip_typerefs(to->variant.routine.return_type);
      dtf_kind = (a_dependent_type_fixup_kind)dtfk_routine_calling_method;
    } else {
      tp = f_skip_typerefs(underlying_array_element_type(to));
      dtf_kind = (a_dependent_type_fixup_kind)dtfk_array_type_size;
    }  /* if */
    if (is_incomplete_type(tp) && is_immediate_class_type(tp)) {
      /* An array type is placed on a fixup list if the underlying element
         type is an incomplete class type (allowed by extension), and a
         routine type is placed on the list if its return type is an
         incomplete class type. */
      /* Pass the NULL source position since no errors should be issued about
         this type. */
      add_to_dependent_type_fixup_list(tp, dtf_kind, (char *)to,
                                       (a_byte_il_entry_kind)iek_type,
                                       &null_source_position);
    }  /* if */
  }  /* if */
}  /* copy_type */


static a_param_type_ptr copy_param_type_list(
                                       a_param_type_ptr  ptp,
                                       a_boolean         copy_default_args)
/*
Copy the param-type list pointed to by ptp and return a pointer to the new
list.  If copy_default_args is TRUE, copy any default argument expressions
into the new param types.  If it is FALSE, the default_arg_expr field
in the new param type will be NULL.
*/
{
  a_param_type_ptr  new_list = NULL, new_ptp, prev_new_ptp = NULL;

  for (; ptp != NULL; ptp = ptp->next) {
    /* Pass a NULL source position to make_param_type to avoid inappropriate
       diagnostics on a type that doesn't correspond directly to a source
       construct. */
    new_ptp = make_param_type(ptp->type, &null_source_position);
    /* Do a struct copy from the old param type to the new. */
    *new_ptp = *ptp;
    if (copy_default_args) {
      /* Expressions may not be shared -- that is, they may not be pointed to
         from more than one place.  Therefore a copy must be made of the
         expression node for the default arg (if one exists). */
      if (ptp->default_arg_expr != NULL) {
        new_ptp->default_arg_expr =
                         duplicate_default_arg_expr(ptp->default_arg_expr);
      }  /* if */
    } else {
      new_ptp->has_default_arg = FALSE;
      new_ptp->default_arg_expr = NULL;
    }  /* if */
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
    /* Note: the name associated with the original param type entry is
       preserved in the copy. */
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
    if (new_list == NULL) {
      new_list = new_ptp;
    } else {
      prev_new_ptp->next = new_ptp;
    }  /* if */
    prev_new_ptp = new_ptp;
  }  /* for */
  return new_list;
}  /* copy_param_type_list */

#if GENERATE_SOURCE_SEQUENCE_LISTS

void copy_routine_type_default_args(a_type_ptr  from_type,
                                    a_type_ptr  to_type)
/*
from_type and to_type are routine types with identical param-type lists --
except that the param-type entries of from_type may have default arguments
that are missing from to_type.  Copy the default argument expressions from
from_type to to_type.  This should be called only in C++ mode.
*/
{
  a_param_type_ptr  from_ptp, to_ptp;

  db_enter(5, "copy_routine_type_default_args");
  check_assertion(!C_mode());
  from_ptp = skip_typerefs(from_type)->
                    variant.routine.extra_info->param_type_list;
  to_ptp = skip_typerefs(to_type)->
                    variant.routine.extra_info->param_type_list;
  for (;; from_ptp = from_ptp->next, to_ptp = to_ptp->next) {
    check_assertion((from_ptp == NULL) == (to_ptp == NULL));
    if (from_ptp == NULL) break;
    if (from_ptp->has_default_arg) {
      to_ptp->has_default_arg = TRUE;
      if (from_ptp->default_arg_expr != NULL) {
        check_assertion(to_ptp->default_arg_expr == NULL || total_errors != 0);
        to_ptp->default_arg_expr =
                        duplicate_default_arg_expr(from_ptp->default_arg_expr);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* copy_routine_type_default_args */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

a_type_ptr copy_routine_type_with_param_types(a_type_ptr  from_type,
                                              a_boolean   copy_default_args)
/*
Make a copy of a routine type and its param types list.  This routine is
called in cases where a routine type and its copy may not share the same
param-types list (for example, when as the result of a user error a routine
type in a function definition is based on a typedef).  If copy_default_args
is TRUE, the copies of the default argument expressions are made; otherwise,
they are not made and the has_default_arg flag is set to FALSE in the
param-type entry of the new type.
*/
{
  a_type_qualifier_set	qualifiers;
  a_type_ptr		to_type;

  /* Save any qualifiers above the routine type.  These will be
     put on top of the newly created type later. */
  qualifiers = get_type_qualifiers(from_type);
  from_type = skip_typerefs(from_type);
  to_type = alloc_type((a_type_kind)tk_routine);
  copy_type(from_type, to_type);
  to_type->variant.routine.extra_info->param_type_list =
            copy_param_type_list(from_type->variant.routine.extra_info->
                                                            param_type_list,
                                 copy_default_args);
  if (qualifiers != TQ_NONE) {
    /* If the original type had qualifiers above the routine type, add
       them to the newly created type now. */
    to_type = make_qualified_type(to_type, qualifiers);
  }  /* if */
  return to_type;
}  /* copy_routine_type_with_param_types */

#if GENERATE_SOURCE_SEQUENCE_LISTS

a_type_ptr routine_type_without_default_args(a_type_ptr  orig_type)
/*
If the specified routine type has default arguments, return a copy of it
with the default arguments removed.  Otherwise, return the original type
unchanged.
*/
{
  a_param_type_ptr  ptp, orig_param_type_list;
  a_type_ptr        tp = orig_type;

  /* Traverse the param-types, stopping as soon as a parameter type with
     a default argument is encountered. */
  orig_param_type_list = skip_typerefs(orig_type)->variant.routine.
                                              extra_info->param_type_list;
  for (ptp = orig_param_type_list; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg) {
      /* A copy of the original type is required, because the default arg
         expression has to be stripped off. */
      tp = alloc_type((a_type_kind)tk_routine);
      copy_type(orig_type, tp);
      tp->variant.routine.extra_info->param_type_list =
                          copy_param_type_list(orig_param_type_list,
                                               /*copy_default_args=*/FALSE);
      break;
    }  /* if */
  }  /* for */
  return tp;
}  /* routine_type_without_default_args */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

a_template_arg_ptr copy_template_arg_list(a_template_arg_ptr orig_list)
/*
Create a copy of the template argument list specified by orig_list
and return a pointer to the new list.
*/
{
  a_template_arg_ptr	new_list = NULL;
  a_template_arg_ptr	tail = NULL;
  a_template_arg_ptr	tap;

  for (tap = orig_list; tap != NULL; tap = tap->next) {
    a_template_arg_ptr	new_tap;
    new_tap = alloc_template_arg(tap->kind);
    *new_tap = *tap;
    new_tap->next = NULL;
    if (new_list == NULL) new_list = new_tap;
    if (tail != NULL) tail->next = new_tap;
    tail = new_tap;
  }  /* for */
  return new_list;
}  /* copy_template_arg_list */


a_boolean is_default_constructor(a_routine_ptr  ctor_rout,
                                 a_boolean      is_declarative_context)
/*
ctor_rout points to a routine entry for a constructor.   Return TRUE if it
points to a default constructor routine entry.  is_declarative_context is
TRUE if this is a constructor declaration rather than a constructor reference.
*/
{
  a_param_type_ptr  ptp;
  a_boolean         is_def_ctor = FALSE;

  check_assertion(ctor_rout->special_kind ==
                                  (a_special_function_kind)sfk_constructor);
  ptp = skip_typerefs(ctor_rout->type)->
                                  variant.routine.extra_info->param_type_list;
  /* There are no parameters or if the first (and therefore its successors,
     if any) has a default argument expression, then this is a default
     constructor. */
  if (ptp == NULL) {
    is_def_ctor = TRUE;
  } else if (ptp->has_default_arg) {
    is_def_ctor = TRUE;
    if (!is_declarative_context) {
      /* If is_declarative_context is TRUE, the check for a default argument
         on the first parameter is more relaxed, but when it is FALSE, only
         return TRUE if the default argument expression has already been
         generated.  This is to handle cases like this:
           class X { X(X* = new X); };
         where it is important *not* to find X(X*) when generating IL for the
         default argument expression (lest the compiler go into a loop).  In
         other words, the above will produce the same error as this:
           class X { X(X*); };
           X::X(X* = new X) { ... }
         It's not only the first param-type entry that has to be checked,
         since there's also this to deal with:
           class X { X(int = 0, X* = new X); };

         A default argument with a NULL default_arg_expr is accepted if it
         has an unevaluated template value, because we know this value can
         be produced when the call is generated.
      */
      for (; ptp != NULL;ptp = ptp->next) {
        if (ptp->default_arg_expr == NULL &&
            !ptp->has_unevaluated_template_default) {
          is_def_ctor = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return is_def_ctor;
}  /* is_default_constructor */


a_boolean is_copy_constructor_type(
                                 a_type_ptr            routine_type,
                                 a_type_ptr            class_of_which_a_member,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             is_declarative_context)
/*
Return TRUE if routine_type (a constructor's function type) is the type of a
copy constructor for class class_of_which_a_member; if it is, also set and
return *qualifiers to indicate the type qualifiers on the copy constructor's
first parameter -- this will show what restrictions are placed on the object
being copied.  is_declarative_context is TRUE if this is a constructor
declaration rather than a constructor reference.
*/
{
  a_param_type_ptr  ptp;
  a_type_ptr        tp;
  a_boolean         is_cctor = FALSE;

  /* A constructor is deemed a copy constructor if (1) the type of the first
     parameter is reference-to-class or reference-to-qualified-class where
     "class" is the class of which it is a member function, and
     (2) the function can be called with only one argument. */
  ptp = skip_typerefs(routine_type)->
                                  variant.routine.extra_info->param_type_list;
  /* If the param type entry is non-NULL there is at least one argument.  If
     there is a second argument and it has a default expression, the function
     call need not explicitly mention the second argument. */
  /* An ellipsis is also allowed by virtue of the fact that it is not checked
     for. */
  if (ptp != NULL && is_reference_type(ptp->type) &&
      (ptp->next == NULL || ptp->next->has_default_arg)) {
    tp = type_pointed_to(ptp->type);
    if (skip_typerefs(tp) == class_of_which_a_member) {
      /* It is probably a copy constructor. */
      is_cctor = TRUE;
      if (!is_declarative_context) {
        /* If this is a call context, be sure the default argument has already
           been scanned.  Here's a case where this makes a difference:
             struct X {
               static X xx;
               X(const X&, int i = (throw xx, 1)) { }
             };
           In even more obscure cases, there may be more than one parameter
           to examine.  Note: the loop start with the second parameter, if
           there is one.
           A default argument with a NULL default_arg_expr is accepted if it
           has an unevaluated template value, because we know this value can
           be produced when the call is generated. */
        for (ptp = ptp->next; ptp != NULL; ptp = ptp->next) {
          if (ptp->default_arg_expr == NULL &&
              !ptp->has_unevaluated_template_default) {
            is_cctor = FALSE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      if (is_cctor) {
        /* See if the object being copied is qualified. */
        *qualifiers = get_top_level_type_qualifiers(tp);
      }  /* if */
    }  /* if */
  }  /* if */
  return is_cctor;
}  /* is_copy_constructor_type */


a_boolean is_copy_constructor(a_routine_ptr         ctor_rout,
                              a_type_ptr            class_of_which_a_member,
                              a_type_qualifier_set  *qualifiers,
                              a_boolean             is_declarative_context)
/*
Return TRUE if ctor_rout points to a copy constructor routine entry for
class_of_which_a_member; if it does, also set and return *qualifiers to
indicate the type qualifiers on the copy constructor's first parameter --
this will show what restrictions are placed on the object being copied.
is_declarative_context is TRUE if this is a constructor declaration rather
than a constructor reference.
*/
{
  a_boolean is_cctor;

  check_assertion(ctor_rout->special_kind ==
                                  (a_special_function_kind)sfk_constructor);
  is_cctor = is_copy_constructor_type(ctor_rout->type, class_of_which_a_member,
                                      qualifiers, is_declarative_context);
  return is_cctor;
}  /* is_copy_constructor */


static void instantiate_il_entity(a_source_correspondence *scp)
/*
Call set_instance_required on the IL entity with the indicated source
correspondence.  That will cause it to be instantiated if it is a template
function or static data member (or, in some configurations, an inline
function).
*/
{
  a_symbol_ptr sym = (a_symbol_ptr)(scp->assoc_info);

  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_static_data_member ||
        sym->kind == (a_symbol_kind)sk_member_function ||
        sym->kind == (a_symbol_kind)sk_routine) {
      set_instance_required(sym, TRUE, /*defer_inline=*/FALSE);
    }  /* if */
  }  /* if */
}  /* instantiate_il_entity */


static void do_instantiations_for_copied_default_arg_expr(
                                                         an_expr_node_ptr expr)
/*
expr is an expression that is a copy of a default argument expression,
i.e., it is a real use of the default argument expression.  Instantiate
any template entities referenced in the top level of the expression,
because they were not instantiated during the original expression
scan.
*/
{
  if (expr->kind == (an_expr_node_kind)enk_routine_address) {
    instantiate_il_entity(&expr->variant.routine->source_corresp);
  } else if (expr->kind == (an_expr_node_kind)enk_variable ||
             expr->kind == (an_expr_node_kind)enk_variable_address) {
    instantiate_il_entity(&expr->variant.variable->source_corresp);
  } else if (expr->kind == (an_expr_node_kind)enk_constant) {
    a_constant_ptr con = expr->variant.constant;
    if (con->kind == (a_constant_repr_kind)ck_address) {
      if (con->variant.address.kind == (an_address_base_kind)abk_routine) {
        instantiate_il_entity(
                        &con->variant.address.variant.routine->source_corresp);
      } else if (con->variant.address.kind ==
                                          (an_address_base_kind)abk_variable) {
        instantiate_il_entity(
                       &con->variant.address.variant.variable->source_corresp);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* do_instantiations_for_copied_default_arg_expr */


void add_to_dynamic_inits_list(a_dynamic_init_ptr dip)
/*
Add the given dynamic initialization entry to the file-scope dynamic_inits
list.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp;

  /* Only the file scope has a dynamic-inits list -- dynamic init entries
     generated for namespace scopes go on the file scope list, and in function
     and block scopes dynamic initialization is handled by statements. */
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


static a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr       dip,
                                            an_expr_copy_options_set options)
/*
Make a copy of a dynamic initialization entry and return a pointer to the copy.
This is not a general-purpose routine -- it is meant to be called from
copy_expr_tree for the kinds of dynamic initializations done under an
expression node.  options is a set of options for the copy.
*/
{
  a_dynamic_init_ptr new_dip;

  new_dip = alloc_dynamic_init(dip->kind);
  *new_dip = *dip;
  if (options & CE_INSIDE_CONDITIONAL_EXPRESSION) {
    new_dip->inside_conditional_expression = TRUE;
  }  /* if */
#if MINIMAL_INLINING
  if (options & CE_DOING_INLINING_OF_FUNCTION_CALL) {
    /* Look for variables that get remapped while copying the expressions
       in a function being inlined. */
    if (dip->variable != NULL) {
      new_dip->variable = remap_var_for_inlining(dip->variable);
    }  /* if */
  }  /* if */
#endif /* MINIMAL_INLINING */
  switch (dip->kind) {
    case dik_none:
    case dik_zero:
      break;
    case dik_expression:
    case dik_call_returning_class_via_cctor:
      new_dip->variant.expression = copy_expr_tree(dip->variant.expression,
                                                   options);
      break;
    case dik_constructor:
      if (options & CE_COPYING_EVALUATED_DEFAULT_ARG_EXPR) {
        /* Instantiate referenced routines in a default argument expression. */
        /* Watch out for prototype instantiations. */
        if (dip->variant.constructor.ptr != NULL) {
          instantiate_il_entity(&dip->variant.constructor.ptr->source_corresp);
        }  /* if */
      }  /* if */
      new_dip->variant.constructor.args =
                         copy_list_of_expr_trees(dip->variant.constructor.args,
                                                 options);
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
      /* The constant pointed to is unshared and must be copied. */
      new_dip->variant.constant =
                   copy_unshared_constant_full(dip->variant.constant, options);
      break;
#if CHECKING
    case dik_bitwise_copy:
      /* These kinds are not expected under expression nodes. */
    default:
      internal_error("copy_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  check_assertion_str(dip->init_expr_lifetime == NULL,
                      "copy_dynamic_init: init_expr_lifetime is non-NULL");
  if (dip->lifetime != NULL) {
    /* This dynamic init is on a destruction list, so the copy must be
       put on a destruction list in the current context. */
    an_object_lifetime_kind kind = dip->lifetime->kind;
    a_boolean               static_lifetime =
                                          is_static_object_lifetime_kind(kind);
    new_dip->lifetime = NULL;
    new_dip->next_in_destruction_list = NULL;
    record_end_of_lifetime_destruction(new_dip, static_lifetime,
                                       /*block_lifetime=*/FALSE);
#if DO_IL_LOWERING
    if (options & CE_UNLINK_SOURCE_DESTRUCTIONS) {
      /* Unlink the source dynamic initialization from its lifetime.  This
         is requested by the caller when the source expression will not
         remain in the IL tree. */
      remove_from_destruction_list(dip);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#if DO_IL_LOWERING
  if (options & CE_TRANSFER_DESTR_ENTITY_DESCR) {
    /* If IL lowering has already attached a destructible entity description to
       the dynamic initialization, it goes with the copy and not the original.
       (If the pointer in the original were not cleared, the destructible
       entity description would be freed twice.) */
    dip->destructible_entity_descr = NULL;
  } else {
    /* The destructible entity description stays with the original entry. */
    new_dip->destructible_entity_descr = NULL;
  }  /* if */
#endif /* DO_IL_LOWERING */
  return new_dip;
}  /* copy_dynamic_init */


a_local_static_variable_init_ptr make_local_static_variable_init(
                                                  a_variable_ptr     var,
                                                  a_scope_ptr        var_scope,
                                                  an_init_kind       init_kind,
                                                  a_constant_ptr     con,
                                                  a_dynamic_init_ptr dip)
/*
Allocate a_local_static_variable_init entry for the indicated variable (which
is a member of the scope var_scope), set its kind to init_kind, and use
con or dip as appropriate to set one of the variant fields.  Add the
entry to the list for the scope var_scope.  Set the initialization kind
for the variable to initk_function_local.  Return a pointer to the entry.
If var_scope is NULL, use the current scope in the scope stack.
*/
{
  a_local_static_variable_init_ptr lsvip;

  db_enter(5, "make_local_static_variable_init");
  if (var_scope == NULL) {
    var_scope = scope_stack[decl_scope_level].il_scope;
    check_assertion(var_scope != NULL);
  }  /* if */
  check_assertion(var_scope->kind == (a_scope_kind)sck_function ||
                  var_scope->kind == (a_scope_kind)sck_block);
  check_assertion(curr_il_region_number != FILE_SCOPE_REGION_NUMBER);
  lsvip = alloc_local_static_variable_init();
  lsvip->next = var_scope->local_static_variable_inits;
  var_scope->local_static_variable_inits = lsvip;
  lsvip->variable = var;
  var->init_kind = (an_init_kind)initk_function_local;
  lsvip->init_kind = init_kind;
  switch (init_kind) {
    case initk_static:
      lsvip->initializer.constant = con;
      break;
    case initk_dynamic:
      lsvip->initializer.dynamic = dip;
      break;
#if CHECKING
    default:
      internal_error("make_local_static_variable_init: bad init kind");
#endif /* CHECKING */
  }  /* switch */
  db_exit();
  return lsvip;
}  /* make_local_static_variable_init */


static a_scope_ptr find_scope_of_variable(a_variable_ptr variable,
                                          a_scope_ptr    scope)
/*
If the indicated variable is declared in the indicated scope or one of
its sub-scopes, return the appropriate scope.  Otherwise, return NULL.
*/
{
  a_scope_ptr    result_scope = NULL, subscope;
  a_variable_ptr test_var;

  /* Check static variables. */
  for (test_var = scope->variables;
       test_var != NULL;
       test_var = test_var->next) {
    if (test_var == variable) {
      result_scope = scope;
      goto end_of_routine;
    }  /* if */
  }  /* for */
  /* Check auto variables. */
  for (test_var = scope->nonstatic_variables;
       test_var != NULL;
       test_var = test_var->next) {
    if (test_var == variable) {
      result_scope = scope;
      goto end_of_routine;
    }  /* if */
  }  /* for */
  /* Check sub-scopes. */
  for (subscope = scope->scopes;
       subscope != NULL;
       subscope = subscope->next) {
    result_scope = find_scope_of_variable(variable, subscope);
    if (result_scope != NULL) goto end_of_routine;
  }  /* for */
end_of_routine:
  return result_scope;
}  /* find_scope_of_variable */
      

static a_scope_ptr scope_of_local_variable(a_variable_ptr variable)
/*
Determine the scope of the indicated local variable and return it.
innermost_function_scope must be set to indicate the function containing
the variable or the variable's scope must be on the scope stack.
*/
{
  a_scope_ptr scope = NULL;

  if (innermost_function_scope != NULL) {
    scope = find_scope_of_variable(variable, innermost_function_scope);
  }  /* if */
  if (scope == NULL && depth_scope_stack != NO_SCOPE_DEPTH) {
    /* While we are still in the front end proper, the subscopes pointer in
       innermost_function_scope is not set yet.  Use the scope stack to
       find the subscopes. */
    a_scope_stack_entry_ptr ssep;
    for (ssep = &scope_stack[depth_scope_stack];
         ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      if (ssep->il_scope != NULL) {
        scope = find_scope_of_variable(variable, ssep->il_scope);
        if (scope != NULL) break;
      }  /* if */
    }  /* for */
  }  /* if */
  check_assertion_str(scope != NULL,
                      "scope_of_local_variable: scope not found");
  return scope;
}  /* scope_of_local_variable */


#endif /* !STANDALONE_UTILITY_PROGRAM */

a_local_static_variable_init_ptr find_local_static_variable_init(
                                                      a_variable_ptr  var,
                                                      a_scope_ptr     scope)
/*
Return a pointer to the local static variable init entry that appears on the
linked list for the specified scope and points to the specified variable.
(It is an internal error for none to be found.)
*/
{
  a_local_static_variable_init_ptr  lsvip;

  check_assertion(scope != NULL &&
                  (scope->kind == (a_scope_kind)sck_function ||
                   scope->kind == (a_scope_kind)sck_block));
  for (lsvip = scope->local_static_variable_inits;
       lsvip != NULL;
       lsvip = lsvip->next) {
    if (lsvip->variable == var) {
      /* Found it. */
      break;
    }  /* if */
  }  /* for */
  check_assertion_str2(lsvip != NULL, "find_local_static_variable_init:",
                       "none found for specified variable and scope");
  return lsvip;
}  /* find_local_static_variable_init */


void get_variable_initializer(a_variable_ptr     variable,
                              a_scope_ptr        var_scope,
                              an_init_kind       *init_kind,
                              an_initializer_ptr *initializer)
/*
Fetch the initialization kind and initializer pointer for the indicated
variable (which is a member of the scope var_scope), and return them
in *init_kind and *initializer.  This is useful for local static variables,
where the initialization information may be provided remotely in
a local-static-variable-init entry to sidestep memory region problems
(the variable is in the file scope memory region, but the initialization
is in the function scope).  var_scope can be NULL if it is not known;
it will be determined by this routine.  We must be in a context where
innermost_function_scope is set to indicate the current function,
or the variable's scope must be on the scope stack.
*/
{
  *init_kind = variable->init_kind;
  *initializer = &variable->initializer;
  if (*init_kind == (an_init_kind)initk_function_local) {
    /* This is a local static variable whose initialization is described
       by an entry of type a_local_static_variable_init. */
    a_local_static_variable_init_ptr lsvip;

#if !STANDALONE_UTILITY_PROGRAM
    if (var_scope == NULL) {
      /* Determine the scope of the variable. */
      var_scope = scope_of_local_variable(variable);
    }  /* if */
#endif /* !STANDALONE_UTILITY_PROGRAM */
    lsvip = find_local_static_variable_init(variable, var_scope);
    *init_kind = lsvip->init_kind;
    *initializer = &lsvip->initializer;
  }  /* if */
}  /* get_variable_initializer */

#if !STANDALONE_UTILITY_PROGRAM

a_vla_dimension_ptr make_vla_dimension(a_type_ptr        array_type,
                                       an_expr_node_ptr  expr_node)
/*
Allocate a_vla_dimension entry for the indicated array_type and set its
type and dimension_expr fields to the values passed in as parameters.
Add the entry to the list for the current scope.  Return a pointer to
the entry.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             il_scope;
  a_vla_dimension_ptr     vdp, end_of_list;

  db_enter(5, "make_vla_dimension");
  check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH &&
                  scope_stack[decl_scope_level].kind !=
                                    (a_scope_kind)sck_func_prototype);
  check_assertion(array_type != NULL &&
                  array_type->kind == (a_type_kind)tk_array);
  /* VLA in block or function scope.  Allocate the vla_dimension in
     the current IL memory region and add it to the vla_dimensions
     list in the IL scope for the function. */
  ssep = &scope_stack[depth_innermost_function_scope];
  il_scope = ensure_il_scope_exists(ssep);
  check_assertion_str(il_scope != NULL,
                      "make_vla_dimension:  NULL IL scope");
  /* Allocate and initialize the vla_dimension. */
  vdp = alloc_vla_dimension();
  vdp->type = array_type;
  vdp->dimension_expr = expr_node;
  array_type->variant.array.has_assoc_vla_dimension = TRUE;
  /* Add the vla_dimension to the end of the list. */
  if (il_scope->vla_dimensions == NULL) {
    il_scope->vla_dimensions = vdp;
  } else {
    end_of_list = il_scope->vla_dimensions;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = vdp;
  }  /* if */
  db_exit();
  return vdp;
}  /* make_vla_dimension */


void remove_from_variables_list(a_variable_ptr var_ptr,
                                a_scope_depth  scope_depth)
/*
Unlink the given variable from the variables list for the scope indicated by
scope_depth.  This is done so the variable can be added again at the end of
the list, to keep the variables in order of appearance of their definitions.
*/
{
  a_variable_ptr              prev_var, vp;
  a_scope_stack_entry_ptr     ssep;
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;

  /* Get pointer to the file scope entry. */
  ssep = &scope_stack[scope_depth];
  check_assertion_str(!var_ptr->source_corresp.is_class_member,
                      "remove_from_variables_list: class member not expected");
  sp = ssep->il_scope;
  pointers_block = assoc_pointers_block_of(ssep);
  check_assertion_str(sp != NULL, "remove_from_variables_list: NULL IL scope");
  /* Find the variable on the current list that precedes var_ptr; we'll need
     it to unlink var_ptr. */
  prev_var = NULL;
  for (vp = sp->variables; vp != NULL; vp = vp->next) {
    if (vp == var_ptr) break;
    prev_var = vp;
  }  /* for */
  check_assertion_str(vp != NULL, "remove_from_variables_list: not found");
  /* Link the previous entry to the entry following this one. */
  if (prev_var == NULL) {
    sp->variables = var_ptr->next;
  } else {
    prev_var->next = var_ptr->next;
  }  /* if */
  /* If the entry being removed was the last on the list, update the
     last_variable pointer. */
  if (var_ptr == pointers_block->last_variable) {
    pointers_block->last_variable = prev_var;
  }  /* if */
}  /* remove_from_variables_list */


void add_to_variables_list(a_variable_ptr var_ptr,
                           a_scope_depth  scope_depth)
/*
Add the given variable to the variables list for the scope at the indicated
scope depth.
*/
{
  a_scope_stack_entry_ptr     ssep;
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;
  a_boolean                   at_file_or_namespace_scope;

  ssep = &scope_stack[scope_depth];
  at_file_or_namespace_scope = (scope_depth == DEPTH_OF_FILE_SCOPE ||
                               scope_depth == depth_innermost_namespace_scope);
  /* Get pointer to current or file scope entry. */
  if (at_file_or_namespace_scope) {
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
  } else if (ssep->kind == (a_scope_kind)sck_func_prototype) {
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
  if (sp != NULL) {
    /* Variables requiring static allocation go on one list, those for stack
       and register allocation on another. */
    if (at_file_or_namespace_scope ||
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
      pointers_block = assoc_pointers_block_of(ssep);
      if (sp->variables == NULL) {
        sp->variables = var_ptr;
      } else {
        pointers_block->last_variable->next = var_ptr;
      }  /* if */
      pointers_block->last_variable = var_ptr;
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


a_variable_ptr make_variable(a_type_ptr      type_ptr,
                             a_storage_class storage_class,
                             a_scope_depth   scope_depth)
/*
Allocate an entry for a variable with type type_ptr and storage class
storage_class, and return a pointer to it.  Unless scope_depth equals
NO_SCOPE_DEPTH, add the variable to the indicated scope.
*/
{
  a_variable_ptr          vp;

  /* Allocate the variable at the file scope if requested.  Variables
     receiving static storage, even if they are not to go on the file scope
     variables list, should also be allocated in the file scope memory
     region. */
  vp = alloc_variable(storage_class);
  vp->type = type_ptr;
  if (scope_depth != NO_SCOPE_DEPTH) {
    add_to_variables_list(vp, scope_depth);
  }  /* if */
  return vp;
}  /* make_variable */


a_variable_ptr make_handler_parameter(a_type_ptr  type_ptr)
/*
Create a handler parameter variable with the specified type, add it to the
parameter field of the current block scope, and return a pointer to it.
*/
{
  a_variable_ptr           vp;

  db_enter(5, "make_handler_parameter");
  /* Allocate the variable. */
  vp = make_variable(type_ptr, (a_storage_class)sc_auto, decl_scope_level);
  vp->is_handler_param = TRUE;

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
  /* make_variable/alloc_variable uses the appropriate memory region,
     based on storage class.*/
  temp_var = make_variable(temp_type, storage_class,
                           at_file_scope ? depth_innermost_namespace_scope :
                                           decl_scope_level);
  /* Name linkage stays nlk_none. */
  return temp_var;
}  /* alloc_temporary_variable */

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

void remove_from_routines_list(a_routine_ptr rout_ptr,
                               a_scope_depth scope_depth)
/*
Unlink the given routine from the routines list for the scope indicated by
scope_depth.  This is done so the routine can be added again at the end of
the list, to keep the routines in order of appearance of their definitions.
*/
{
  a_routine_ptr               prev_routine = NULL, rp;
  a_scope_stack_entry_ptr     ssep;
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;

  /* Get pointer to the file scope entry. */
  ssep = &scope_stack[scope_depth];
  sp = ssep->il_scope;
  check_assertion_str(!rout_ptr->source_corresp.is_class_member,
                      "remove_from_routines_list: class member not expected");
  check_assertion_str(sp != NULL, "remove_from_routines_list: NULL IL scope");
  pointers_block = assoc_pointers_block_of(ssep);
  /* Find the routine on the current list in order to find the previous entry
     so we can unlink. */
  for (rp = sp->routines; rp != NULL; prev_routine = rp, rp = rp->next) {
    if (rp == rout_ptr) break;
  }  /* for */
  check_assertion_str(rp != NULL,
                      "remove_from_routines_list: routine not found on list");
  /* Link the previous entry to the entry following this one. */
  if (prev_routine == NULL) {
    sp->routines = rout_ptr->next;
  } else {
    prev_routine->next = rout_ptr->next;
  }  /* if */
  /* If the entry being removed was the last on the list, update the
     last_routine pointer. */
  if (rout_ptr == pointers_block->last_routine) {
    pointers_block->last_routine = prev_routine;
  }  /* if */
}  /* remove_from_routines_list */


void add_to_routines_list(a_routine_ptr  rout_ptr,
                          a_scope_depth  scope_level)
/*
Add the given routine to the routines list for the scope corresponding to
scope_level.  When scope_level is NO_SCOPE_DEPTH, the scope is computed
rather than determined directly.
*/
{
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block;

  sp = get_scope_for_list(scope_level, &rout_ptr->source_corresp,
                          &pointers_block);
  check_assertion_str(sp != NULL, "add_to_routines_list: NULL IL scope");
  /* Add the routine to the list of routines for this scope. */
  if (sp->routines == NULL) {
    sp->routines = rout_ptr;
  } else if (pointers_block != NULL) {
    pointers_block->last_routine->next = rout_ptr;
  } else {
    /* The scope stack entry is no longer on the stack, so just look for
       the end of the routines list and add the new routine. */
    a_routine_ptr rp = sp->routines;
    while (rp->next != NULL) rp = rp->next;
    rp->next = rout_ptr;
  }  /* if */
  rout_ptr->next = NULL;
  if (pointers_block != NULL) pointers_block->last_routine = rout_ptr;
}  /* add_to_routines_list */


void add_to_asm_entries_list(an_asm_entry_ptr  asm_entry_ptr)
/*
Add the given routine to the asm entries list for the current scope.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;
  a_scope_pointers_block_ptr  pointers_block;

  ssep = &scope_stack[decl_scope_level];
  /* Create the IL scope if necessary (for block scopes). */
  sp = ensure_il_scope_exists(ssep);
  check_assertion_str(sp != NULL, "add_to_asm_entries_list: NULL IL scope");
  pointers_block = assoc_pointers_block_of(ssep);
  if (sp->asm_entries == NULL) {
    sp->asm_entries = asm_entry_ptr;
  } else {
    pointers_block->last_asm_entry->next = asm_entry_ptr;
  }  /* if */
  pointers_block->last_asm_entry = asm_entry_ptr;
  asm_entry_ptr->next = NULL;
}  /* add_to_asm_entries_list */


void add_to_labels_list(a_label_ptr label_ptr)
/*
Add the given label to the labels list for the function (not current) scope.
*/
{
  a_scope_stack_entry_ptr ssep;

  /* Get pointer to the current function scope entry. */
#if CHECKING
  if (innermost_function_scope == NULL) {
    internal_error("add_to_labels_list: not inside function");
  }  /* if */
#endif /* CHECKING */
  if (innermost_function_scope->depth_in_scope_stack != NO_SCOPE_DEPTH) {
    /* The function scope is on the scope stack. */
    ssep = &scope_stack[innermost_function_scope->depth_in_scope_stack];
  } else {
    /* The function scope is not on the scope stack, as in generated routines
       in IL lowering. */
    ssep = NULL;
  }  /* if */
  if (innermost_function_scope->labels == NULL) {
    innermost_function_scope->labels = label_ptr;
  } else {
    if (ssep != NULL) ssep->last_label->next = label_ptr;
  }  /* if */
  if (ssep != NULL) ssep->last_label = label_ptr;
  label_ptr->next = NULL;
}  /* add_to_labels_list */


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
     node type, in the "?" case).  The transformation done in lower_temp_init
     is the difficult case. */
  /* If you change this, see also check_result_not_used_flag. */
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
  } else if (node->kind == (an_expr_node_kind)enk_object_lifetime) {
    set_expr_result_not_used(node->variant.object_lifetime.expr);
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


an_expr_node_ptr make_comma_node(an_expr_node_ptr expr1,
                                 an_expr_node_ptr expr2)
/*
Make a comma expression node with the indicated two expressions as its
operands, and return a pointer to it.
*/
{
  an_expr_node_ptr comma_node;

  expr1->next = expr2;
  expr2->next = NULL;
  comma_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                  expr2->type, expr1);
  return comma_node;
}  /* make_comma_node */


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
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (constant->expr != NULL &&
          !is_routine_address_node(constant->expr)) {
        /* Use the constant form if there's extra information in it on the
           expression that generated the constant. */
      } else
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      /* Do not insert code here. */
      {
        /* The constant is the address of a function; use an address-of-
           function expression node. */
        node = alloc_expr_node((an_expr_node_kind)enk_routine_address);
        node->variant.routine = constant->variant.address.variant.routine;
        goto have_node;
      }  /* if */
    } else if (abkind == (an_address_base_kind)abk_variable) {
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (constant->expr != NULL &&
          !is_variable_address_node(constant->expr)) {
        /* Use the constant form if there's extra information in it on the
           expression that generated the constant. */
      } else
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      /* Do not insert code here. */
      {
        /* The constant is the address of a variable; use an address-of-
           variable expression node. */
        node = alloc_expr_node((an_expr_node_kind)enk_variable_address);
        node->variant.variable = constant->variant.address.variant.variable;
        goto have_node;
      }  /* if */
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


an_expr_node_ptr alloc_node_for_allocated_constant(a_constant *constant)
/*
Allocate an expression node for a constant, and return a pointer to the
expression node.  The constant in this case is an already-allocated
IL constant.
*/
{
  an_expr_node_ptr expr;

  expr = alloc_expr_node((an_expr_node_kind)enk_constant);
  expr->variant.constant = constant;
  expr->type = constant->type;
  return expr;
}  /* alloc_node_for_allocated_constant */


an_expr_node_ptr node_for_integer_constant(long            value,
                                           an_integer_kind kind)
/*
Make a node for an integer constant with value "value" and kind "kind",
and return a pointer to it.
*/
{
  an_expr_node_ptr node;
  a_constant       constant;

  set_integer_constant(&constant, (a_host_large_integer)value, kind);
  node = alloc_node_for_constant(&constant);

  return node;
}  /* node_for_integer_constant */


an_expr_node_ptr node_for_host_large_integer(a_host_large_integer	value,
                                             an_integer_kind		kind)
/*
Make a node for a host large integer with value "value" and kind "kind",
and return a pointer to it.
*/
{
  an_expr_node_ptr node;
  a_constant       constant;

  set_integer_constant(&constant, value, kind);
  node = alloc_node_for_constant(&constant);

  return node;
}  /* node_for_host_large_integer */


a_boolean is_bad_type_for_template_arg_operand(a_type_ptr type)
/*
Return TRUE if type is a bad type for an operand of an expression in
a template argument, i.e., it is not integral or enum.
*/
{
  a_boolean is_bad_type;

  if (is_integral_or_enum_type(type)) {
    is_bad_type = FALSE;
  } else if (is_error_type(type)) {
    is_bad_type = FALSE;
  } else {
    is_bad_type = TRUE;
  }  /* if */
  return is_bad_type;
}  /* is_bad_type_for_template_arg_operand */


static a_type_ptr type_of_copied_template_expr(an_expr_node_ptr expr,
                                               a_constant       *constant,
                                               a_constant_ptr   alloc_con)
/*
expr/constant/alloc_con represent a copied template parameter expression
in the form used in copy_template_param_expr and its subroutines.  Return
the type of the expression, stripped of typerefs.
*/
{
  a_type_ptr type;

  if (expr != NULL) {
    type = expr->type;
  } else if (alloc_con != NULL) {
    type = alloc_con->type;
  } else {
    check_assertion(constant != NULL);
    type = constant->type;
  }  /* if */
  type = skip_typerefs(type);
  return type;
}  /* type_of_copied_template_expr */


static void cast_copied_template_param_expr(an_expr_node_ptr  *expr,
                                            a_constant        *constant,
                                            a_constant_ptr    *alloc_con,
                                            a_type_ptr        new_type,
                                            a_source_position *source_pos)
/*
expr/constant/alloc_con represent a copied template parameter expression
in the form used in copy_template_param_expr and its subroutines.  Cast
the expression to the type new_type, if necessary.  *expr, *alloc_con, and
the constant pointed to by constant are updated as needed.  *source_pos
gives the source position for errors.
*/
{
  a_type_ptr curr_type;
  a_boolean  did_not_fold;

  curr_type = type_of_copied_template_expr(*expr, constant, *alloc_con);
  if (!identical_types(curr_type, new_type)) {
    /* Casting is required. */
    if (*expr != NULL) {
      /* There is already an expression, so add a cast to it. */
      *expr = make_operator_node((an_expr_operator_kind)eok_cast, new_type,
                                 *expr);
      (*expr)->variant.operation.compiler_generated = TRUE;
    } else {
      if (*alloc_con != NULL) {
        /* Drop back from an allocated to an unallocated copy of the
           constant. */
        copy_constant(*alloc_con, constant);
        *alloc_con = NULL;
      }  /* if */
      /* Cast the constant to the new type. */
      type_change_constant(constant, new_type,
                           /*is_implicit_cast=*/TRUE,
                           /*constant_context=*/TRUE,
                           /*evaluated_context=*/TRUE,
                           /*fold_constant_addr_exprs=*/TRUE,
                           /*is_reinterpret_cast=*/FALSE,
                           /*maintain_expression=*/FALSE,
                           &did_not_fold,
                           source_pos);
      check_assertion(!did_not_fold);
    }  /* if */
  }  /* if */
}  /* cast_copied_template_param_expr */


static void do_conversions_on_operands_of_copied_template_expr(
                                         an_expr_operator_kind op,
                                         an_expr_node_ptr      *operand_1,
                                         a_constant            *constant_1,
                                         a_constant_ptr        *alloc_con_1,
                                         a_boolean             op_2_present,
                                         an_expr_node_ptr      *operand_2,
                                         a_constant            *constant_2,
                                         a_constant_ptr        *alloc_con_2,
                                         a_boolean             op_3_present,
                                         an_expr_node_ptr      *operand_3,
                                         a_constant            *constant_3,
                                         a_constant_ptr        *alloc_con_3,
                                         a_source_position     *source_pos,
                                         a_type_ptr            *operation_type,
                                         a_boolean             *copy_error)
/*
Subroutine for copy_template_param_expr.  An operation involving template
parameter constants is being processed.  Its operator is op, and its
operands are *operand_1, *operand_2, and *operand_3.  op_2_present
and op_3_present indicate the presence of operands after the first.
If a given operand is available only as a constant, *operand_n is NULL
and the corresponding parameters alloc_con_n and constant_n point to
an allocated or unallocated copy of the constant.  *operation_type
indicates the operation type; it is updated on return.  If there is
some error (e.g., an operand type is not integral), *copy_error is set
to TRUE.  *source_pos gives the source position for errors.
*/
{
  a_type_ptr type_1, type_2, type_3;
  a_type_ptr result_type = *operation_type, promoted_type_2;
  a_boolean  do_promotion, do_usual_arith_conversions;

  type_1 = type_of_copied_template_expr(*operand_1, constant_1, *alloc_con_1);
  if (op_2_present) {
    type_2 = type_of_copied_template_expr(*operand_2, constant_2,
                                          *alloc_con_2);
    if (op_3_present) {
      type_3 = type_of_copied_template_expr(*operand_3, constant_3,
                                            *alloc_con_3);
    }  /* if */
  }  /* if */
  if (is_bad_type_for_template_arg_operand(type_1) ||
      (op_2_present && is_bad_type_for_template_arg_operand(type_2)) ||
      (op_3_present && is_bad_type_for_template_arg_operand(type_3))) {
    /* At least one of the operands has an invalid type, e.g., a pointer
       type.  Deduction fails. */
    *copy_error = TRUE;
  } else if (!op_2_present) {
    /* One-operand operation. */
    /* Determine whether the integral promotions should be done for this
       operation. */
    switch (op) {
      case eok_inegate:
      case eok_unary_plus:
      case eok_complement:
        do_promotion = TRUE;
        break;
      case eok_not:
        do_promotion = FALSE;
        break;
      default:
        unexpected_condition_str2(
                         "do_conversions_on_operands_of_copied_template_expr:",
                         "bad unary operator");
    }  /* switch */
    if (do_promotion) {
      result_type = type_after_integral_promotion(type_1);
      cast_copied_template_param_expr(operand_1, constant_1, alloc_con_1,
                                      result_type, source_pos);
    }  /* if */
  } else if (!op_3_present) {
    /* Two-operand operation. */
    do_usual_arith_conversions = do_promotion = FALSE;
    switch (op) {
      case eok_imultiply:
      case eok_idivide:
      case eok_iadd:
      case eok_isubtract:
      case eok_igt:
      case eok_ilt:
      case eok_ige:
      case eok_ile:
      case eok_ieq:
      case eok_ine:
      case eok_and:
      case eok_or:
      case eok_xor:
        do_usual_arith_conversions = TRUE;
        break;
      case eok_shiftl:
      case eok_shiftr:
        do_promotion = TRUE;
        break;
      case eok_land:
      case eok_lor:
        break;
      default:
        unexpected_condition_str2(
                         "do_conversions_on_operands_of_copied_template_expr:",
                         "bad binary operator");
    }  /* switch */
    if (do_usual_arith_conversions) {
      result_type = usual_arithmetic_conversions(type_1, type_2);
      cast_copied_template_param_expr(operand_1, constant_1, alloc_con_1,
                                      result_type, source_pos);
      cast_copied_template_param_expr(operand_2, constant_2, alloc_con_2,
                                      result_type, source_pos);
    } else if (do_promotion) {
      result_type = type_after_integral_promotion(type_1);
      cast_copied_template_param_expr(operand_1, constant_1, alloc_con_1,
                                      result_type, source_pos);
      promoted_type_2 = type_after_integral_promotion(type_2);
      cast_copied_template_param_expr(operand_2, constant_2, alloc_con_2,
                                      promoted_type_2, source_pos);
    }  /* if */
  } else {
    /* Three-operand operation, i.e., "?" */
    check_assertion(op == (an_expr_operator_kind)eok_question);
    /* If the operands have the same type, use that type.  Otherwise, do
       the usual arithmetic conversions. */
    if (!types_are_compatible(type_2, type_3)) {
      result_type = usual_arithmetic_conversions(type_2, type_3);
      cast_copied_template_param_expr(operand_2, constant_2, alloc_con_2,
                                      result_type, source_pos);
      cast_copied_template_param_expr(operand_3, constant_3, alloc_con_3,
                                      result_type, source_pos);
    }  /* if */
  }  /* if */
  *operation_type = result_type;        
}  /* do_conversions_on_operands_of_copied_template_expr */


/* Forward declaration needed because of mutual recursion. */
static a_constant_ptr copy_template_param_con(
                                  a_constant_ptr           con,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_nesting_depth depth,
                                  a_type_ptr               guide_type,
                                  a_source_position        *source_pos,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_constant_ptr           constant);


static an_expr_node_ptr alloc_copied_template_param_expr(
                                                    an_expr_node_ptr expr,
                                                    a_constant       *constant,
                                                    a_constant_ptr   alloc_con)
/*
Return a pointer to an expression for a copied template parameter expression.
If expr is non-NULL, the expression already exists and the value of expr
is returned.  Otherwise, an expression for a constant must be created
and a pointer to that returned.  If alloc_con is non-NULL, it is a pointer
to an already-allocated constant; otherwise, constant points to the
(unallocated) constant value.
*/
{
  if (expr == NULL) {
    if (alloc_con != NULL) {
      expr = alloc_node_for_allocated_constant(alloc_con);
    } else {
      expr = alloc_node_for_constant(constant);
    }  /* if */
  }  /* if */
  return expr;
}  /* alloc_copied_template_param_expr */


static an_expr_node_ptr copy_template_param_expr(
                                  an_expr_node_ptr         expr,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_nesting_depth depth,
                                  a_source_position        *source_pos,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_constant_ptr           constant,
                                  a_constant_ptr           *alloc_con)
/*
Copy the expression expr, and return a pointer to the copy.  The expression
is a constant expression that is part of a template argument expression.
In the process of copying, replace any template parameters at depth "depth"
with the corresponding values from the template argument list
template_arg_list.  source_pos provides the source position for any
calls of copy_type_with_substitution.  If there is an error in the
copying (specifically, if there is an error in doing substitution
on a type), set *copy_error to TRUE.  If the expression after
substitution is a constant, set *alloc_con to the address of the
constant and return NULL.  If there no allocated copy of the constant,
set *alloc_con to NULL, set *constant to the constant value, and
return NULL.  options is a set of name lookup options.
*/
{
  an_expr_node_ptr expr_copy = expr;

  *alloc_con = NULL;
  switch (expr->kind) {
    case enk_constant:
      /* The expression is just a constant.  Do substitution and return
         the constant after substitution. */
      *alloc_con = copy_template_param_con(expr->variant.constant,
                                           template_arg_list,
                                           depth,
                                           (a_type_ptr)NULL,
                                           source_pos,
                                           options,
                                           copy_error,
                                           constant);
      expr_copy = NULL;
      break;
    case enk_operation:
      { an_expr_node_ptr operand_1 = expr->variant.operation.operands;
        an_expr_node_ptr operand_2 = operand_1->next;
        an_expr_node_ptr operand_3 = NULL;
        an_expr_operator_kind
                         op = expr->variant.operation.kind;
        an_expr_node_ptr new_operand_1, new_operand_2 = NULL;
        an_expr_node_ptr new_operand_3 = NULL;
        a_constant       constant_1, constant_2, constant_3;
        a_constant_ptr   alloc_con_1, alloc_con_2 = NULL, alloc_con_3 = NULL;
        a_boolean        folded_to_constant = FALSE;
        a_type_ptr       operation_type = expr->type;

        /* Do substitution on the operands. */
        new_operand_1 = copy_template_param_expr(operand_1,
                                                 template_arg_list,
                                                 depth,
                                                 source_pos,
                                                 options,
                                                 copy_error,
                                                 &constant_1,
                                                 &alloc_con_1);
        if (operand_2 != NULL) {
          new_operand_2 = copy_template_param_expr(operand_2,
                                                   template_arg_list,
                                                   depth,
                                                   source_pos,
                                                   options,
                                                   copy_error,
                                                   &constant_2,
                                                   &alloc_con_2);
          operand_3 = operand_2->next;
          if (operand_3 != NULL) {
            new_operand_3 = copy_template_param_expr(operand_3,
                                                     template_arg_list,
                                                     depth,
                                                     source_pos,
                                                     options,
                                                     copy_error,
                                                     &constant_3,
                                                     &alloc_con_3);
          }  /* if */
        }  /* if */
        /* Do the usual arithmetic conversion or the like on the operands
           after substitution. */
        do_conversions_on_operands_of_copied_template_expr(
                  op,
                  &new_operand_1, &constant_1, &alloc_con_1,
                  operand_2 != NULL, &new_operand_2, &constant_2, &alloc_con_2,
                  operand_3 != NULL, &new_operand_3, &constant_3, &alloc_con_3,
                  source_pos, &operation_type,
                  copy_error);
        if (new_operand_1 == NULL &&
            new_operand_2 == NULL &&
            new_operand_3 == NULL) {
          /* All the operands are constant. */
          if (alloc_con_1 != NULL) copy_constant(alloc_con_1, &constant_1);
          if (alloc_con_2 != NULL) copy_constant(alloc_con_2, &constant_2);
          if (alloc_con_3 != NULL) copy_constant(alloc_con_3, &constant_3);
          /* Do not fold if any of the constants is still a
             template parameter constant. */
          if (constant_1.kind != (a_constant_repr_kind)ck_template_param &&
              (operand_2 == NULL ||
               constant_2.kind != (a_constant_repr_kind)ck_template_param) &&
              (operand_3 == NULL ||
               constant_3.kind != (a_constant_repr_kind)ck_template_param)) {
            /* All the operands are constants and not template parameter
               constants.  Fold the operation. */
            a_boolean did_not_fold, template_constant;

            expr_copy = NULL;
            folded_to_constant = TRUE;
            if (operand_2 != NULL) {
              if (operand_3 != NULL) {
                /* Three-operand operation, "?". */
                check_assertion(op == (an_expr_operator_kind)eok_question);
                if (is_false_constant(&constant_1)) {
                  /* Operand 1 is false, so the result is operand 3. */
                  *alloc_con = alloc_con_3;
                  if (alloc_con_3 == NULL) *constant = constant_3;
                } else {
                  /* Operand 1 is true, so the result is operand 2. */
                  *alloc_con = alloc_con_2;
                  if (alloc_con_2 == NULL) *constant = constant_2;
                }  /* if */
              } else {
                /* Two-operand operation. */
                binary_operation(op, &constant_1, &constant_2,
                                 operation_type, constant,
                                 /*constant_context=*/TRUE,
                                 /*evaluated_context=*/TRUE,
                                 &did_not_fold,
                                 &template_constant,
                                 source_pos);
                check_assertion(!did_not_fold);
                *alloc_con = NULL;
              }  /* if */
            } else {
              /* One-operand operation. */
              unary_operation(op, &constant_1, operation_type, constant,
                              /*constant_context=*/TRUE,
                              /*evaluated_context=*/TRUE,
                              &did_not_fold,
                              &template_constant,
                              source_pos);
              check_assertion(!did_not_fold);
              *alloc_con = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
        if (!folded_to_constant) {
          /* Some operand is non-constant or a template parameter, so
             an expression is needed. */
          /* Allocate the node for each operand if it has not been
             allocated yet. */
          new_operand_1 = alloc_copied_template_param_expr(
                                      new_operand_1, &constant_1, alloc_con_1);
          if (operand_2 != NULL) {
            new_operand_2 = alloc_copied_template_param_expr(
                                      new_operand_2, &constant_2, alloc_con_2);
            if (operand_3 != NULL) {
              new_operand_3 = alloc_copied_template_param_expr(
                                      new_operand_3, &constant_3, alloc_con_3);
            }  /* if */
          }  /* if */
          /* Link the operand expressions together and create a new
             expression node. */
          new_operand_1->next = new_operand_2;
          if (new_operand_2 != NULL) {
            new_operand_2->next = new_operand_3;
          }  /* if */
          expr_copy = make_operator_node(op, operation_type, new_operand_1);
        }  /* if */
      }
      break;
    default:
      unexpected_condition_str("copy_template_param_expr: bad kind");
  }  /* if */
  return expr_copy;
}  /* copy_template_param_expr */


static a_constant_ptr copy_template_param_unknown_entity_con(
                                  a_constant_ptr           con,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_nesting_depth depth,
                                  a_type_ptr               guide_type,
                                  a_boolean                is_address,
                                  a_boolean                is_template_ref,
                                  a_template_arg_ptr       ref_arg_list,
                                  a_source_position        *source_pos,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_constant_ptr           constant)
/*
Copy the ck_template_param/tpck_member or .../tpck_unknown_function
constant "con", replacing any occurrences of template parameters at depth
"depth" with the corresponding values from the template argument list
template_arg_list, and return a pointer to the copy after
substitution.  If there is no allocated instance of the constant, set
*constant to the constant value and return NULL.  If guide_type is
non-NULL, it is a "guide" type for the constant, either the type of
the template parameter or the destination type of a cast above this
constant.  is_address is TRUE if the constant produced should be for
the address of the member rather than the value (e.g., there is a
tpck_address constant over this constant).  is_template_ref is TRUE if
the constant produced should be for a function template followed by
explicit arguments (i.e., there is a tpck_template_ref over this
constant); ref_arg_list provides the explicit argument list in that
case.  source_pos provides the source position for any calls of
copy_type_with_substitution.  If there is an error in the copying
(specifically, if there is an error in doing substitution on a type),
set *copy_error to TRUE.  options is a set of name lookup options.
*/
{
  a_constant_ptr con_copy;
  a_symbol_ptr   sym, orig_sym;
  a_type_ptr     parent_type;
  a_boolean      err = FALSE, type_check_needed = (guide_type != NULL);
  a_boolean      unhandled_template_args = is_template_ref;

  check_assertion(con->kind == (a_constant_repr_kind)ck_template_param &&
                  (con->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_member ||
                   con->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_unknown_function));
  con_copy = con;
  if (!con->source_corresp.is_class_member) {
    check_assertion(con->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_unknown_function);
    /* For a non-member unknown function, the original symbol (probably
       an overload set) was saved when this constant was created. */
    sym = con->variant.template_param.variant.unknown_function.symbol;
    check_assertion(sym != NULL);
  } else {
    /* Member constant (normal case). */
    /* This occurs for member constants specified in forms such as A<T>::x.
       Do substitution on the parent type and then look up the name in the
       updated class to see what the member is. */
    orig_sym = (a_symbol_ptr)con->source_corresp.assoc_info;
    check_assertion(orig_sym != NULL && con->source_corresp.is_class_member);
    parent_type = con->source_corresp.parent.class_type;
    sym = copy_parent_type_with_substitution(orig_sym, parent_type,
                                             template_arg_list, depth,
                                             source_pos,
                                             /*is_type=*/FALSE,
                                             options,
                                             copy_error);
    if (sym == orig_sym) {
      /* A reference like "X::operator T" will not be substituted by the call
         above because the parent type is not altered.  Check for an unknown
         conversion function that must be processed. */
      a_type_ptr	conv_type;
      conv_type = type_if_unknown_conversion_function_symbol(orig_sym);
      if (conv_type != NULL) {
        /* Substitute the any template parameters in the conversion type. */
        conv_type = copy_type_with_substitution(conv_type, template_arg_list,
                                                depth, source_pos,
                                                options, copy_error);
        /* Look for a conversion function that converts to the new type. */
        sym = look_up_conversion_function(parent_type, conv_type, source_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    /* The substituted parent class has no member of the specified name. */
    err = TRUE;
  } else if (sym->kind == (a_symbol_kind)sk_constant) {
    con_copy = sym->variant.constant;
  } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    a_variable_ptr var = sym->variant.static_data_member.variable;
    if (!is_address) {
      /* The value of a static data member is acceptable as a result if
         the data member can be used as a constant. */
      con_copy = var_constant_value(var);
      if (con_copy == NULL) err = TRUE;
    } else {
      /* Address of a static data member. */
      set_variable_address_constant(var, constant,
                                    /*set_address_taken_flag=*/TRUE);
      con_copy = NULL;
    }  /* if */
  } else if (sym->kind == (a_symbol_kind)sk_field && is_address) {
    /* The address of a nonstatic data member: pointer to member. */
    a_field_ptr field = sym->variant.field.ptr;
    set_ptr_to_data_member_constant(field, constant);
    con_copy = NULL;
  } else if ((sym->kind == (a_symbol_kind)sk_member_function ||
              sym->kind == (a_symbol_kind)sk_function_template ||
              sym->kind == (a_symbol_kind)sk_overloaded_function) &&
             guide_type != NULL && is_address) {
    /* A member function is acceptable as a pointer or pointer-to-member.
       Choose a function from the overload set based on the guide type. */
    choose_function_and_make_address_constant(sym,
                                              is_template_ref, ref_arg_list,
                                              guide_type, constant, &err);
    unhandled_template_args = FALSE;
    con_copy = NULL;
    type_check_needed = FALSE;
  } else {
    err = TRUE;
  }  /* if */
  if (!err && type_check_needed) {
    /* The type of the entity must match the guide type.  If it doesn't,
       deduction fails.  Some kinds of conversions are allowed. */
    a_constant_ptr source_con = (con_copy != NULL) ? con_copy : constant;
    a_type_ptr     source_type = source_con->type;
    if (!identical_types(guide_type, source_type)) {
      a_std_conv_descr std_conv;
      if (!impl_conversion_possible(source_type,
                                    /*source_is_constant=*/TRUE,
                                    /*source_is_string_literal=*/FALSE,
                                    source_con,
                                    guide_type,
                                    /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                    /*suppress_extensions=*/TRUE,
                                    ec_no_error,
                                    &std_conv) ||
          !conversion_allowed_for_nontype_template_argument(&std_conv)) {
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (unhandled_template_args) err = TRUE;
  if (err) {
    /* The constant was specified as something like A<T>::B, but the
       substituted "A<T>" does not contain a B, or the B found is not
       a constant. */
    *copy_error = TRUE;
    con_copy = alloc_error_constant();
  }  /* if */
  return con_copy;
}  /* copy_template_param_unknown_entity_con */


static a_constant_ptr copy_template_param_con(
                                  a_constant_ptr           con,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_nesting_depth depth,
                                  a_type_ptr               guide_type,
                                  a_source_position        *source_pos,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_constant_ptr           constant)
/*
Copy a ck_template_param constant, replacing any occurrences of
template parameters at depth "depth" with the corresponding values from
the template argument list template_arg_list, and return a pointer to
the copy after substitution.  If there is no allocated instance of the
constant, set *constant to the constant value and return NULL.
If guide_type is non-NULL, it is a "guide" type for the constant,
either the type of the template parameter or the destination type
of a cast above this constant.  source_pos provides the source position
for any calls of copy_type_with_substitution.  If there is an error in
the copying (specifically, if there is an error in doing substitution
on a type), set *copy_error to TRUE.  options is a set of name lookup
options.
*/
{
  a_constant_ptr con_copy, other_con;
  a_type_ptr     new_type;
  a_boolean      did_not_fold;

  con_copy = con;
  if (con->kind == (a_constant_repr_kind)ck_template_param) {
    switch (con->variant.template_param.kind) {
      case tpck_param:
        /* The template param constant represents a simple non-type template
           parameter.  Replace it if it has the right depth. */
        if (con->variant.template_param.variant.coordinates.depth == depth) {
          a_template_arg_ptr tap = get_template_arg_by_list_pos(
                                            (a_template_param_ptr)NULL,
                                            &template_arg_list,
                                            con->variant.template_param.
                                                 variant.coordinates.position);
          check_assertion(is_nontype_templ_arg(tap) &&
                          !tap->is_array_bound_of_unknown_type);
          if (tap->variant.constant != NULL) {
            /* Only use the template argument value if one was specified. */
            con_copy = tap->variant.constant;
          }  /* if */
        }  /* if */
        break;
      case tpck_member:
        /* A member constant, e.g., for a case like A<T>::x. */
        con_copy = copy_template_param_unknown_entity_con(
                                                  con, template_arg_list,
                                                  depth, guide_type,
                                                  /*is_address=*/FALSE,
                                                  /*is_template_ref=*/FALSE,
                                                  (a_template_arg_ptr)NULL,
                                                  source_pos, options,
                                                  copy_error, constant);
        break;
      case tpck_unknown_function:
        /* An unknown function. */
        con_copy = copy_template_param_unknown_entity_con(
                                                  con, template_arg_list,
                                                  depth, guide_type,
                                                  /*is_address=*/TRUE,
                                                  /*is_template_ref=*/FALSE,
                                                  (a_template_arg_ptr)NULL,
                                                  source_pos, options,
                                                  copy_error, constant);
        break;
      case tpck_cast:
        /* The template param constant represents a cast of a constant to
           a template parameter type. */
        new_type = copy_type_with_substitution(con->type,
                                               template_arg_list,
                                               depth,
                                               source_pos,
                                               options,
                                               copy_error);
        other_con = copy_template_param_con(
                                 con->variant.template_param.variant.constant,
                                 template_arg_list,
                                 depth,
                                 new_type,
                                 source_pos,
                                 options,
                                 copy_error,
                                 constant);
        if (new_type == con->type &&
            other_con == con->variant.template_param.variant.constant) {
          /* No change in the type or constant. */
        } else if (is_incomplete_type(new_type) ||
                   is_class_struct_union_type(new_type) ||
                   is_function_type(new_type) ||
                   is_array_type(new_type)) {
          /* You can't cast to an incomplete type, a class type, a
             function type, or an array type. */
          *copy_error = TRUE;
        } else {
          if (other_con != NULL) *constant = *other_con;
          /* Do the cast again with the type and constant after
             substitution. */
          type_change_constant(constant, new_type,
                               /*is_implicit_cast=*/FALSE,
                               /*constant_context=*/TRUE,
                               /*evaluated_context=*/TRUE,
                               /*fold_constant_addr_exprs=*/TRUE,
                               /*is_reinterpret_cast=*/FALSE,
                               /*maintain_expression=*/FALSE,
                               &did_not_fold,
                               source_pos);
          check_assertion(!did_not_fold);
          con_copy = NULL;
        }  /* if */
        break;
      case tpck_address:
        /* The template param constant represents the address of a member.
           Process the underlying tpck_member constant as an address, and
           use the result of that in place of both the tpck_address and
           the tpck_member. */
        con_copy = copy_template_param_unknown_entity_con(
                                 con->variant.template_param.variant.constant,
                                 template_arg_list,
                                 depth, guide_type,
                                 /*is_address=*/TRUE,
                                 /*is_template_ref=*/FALSE,
                                 (a_template_arg_ptr)NULL,
                                 source_pos, options,
                                 copy_error, constant);
        break;
      case tpck_sizeof:
      case tpck_alignof:
      case tpck_uuidof:
        /* The template param represents sizeof(T), __ALIGNOF__(T), or
           __uuidof(T), where T is a type containing a template parameter.
           Determine the type of T after substitution. */
        new_type = copy_type_with_substitution(con->variant.template_param.
                                                                  variant.type,
                                               template_arg_list,
                                               depth,
                                               source_pos,
                                               options,
                                               copy_error);
        if (new_type == con->variant.template_param.variant.type) {
          /* No change in the type. */
        } else {
          if (is_or_contains_template_param(new_type)) {
            /* Still a template parameter type, so still need a
               tpck_sizeof/alignof/uuidof constant. */
            *constant = *con;
            constant->variant.template_param.variant.type = new_type;
            con_copy = NULL;
          } else {
            /* No longer a template parameter type, so the sizeof/alignof
               or uuidof is known. */
            new_type = skip_typerefs(new_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (con->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_uuidof) {
              /* __uuidof. */
              make_uuidof_constant(new_type, constant);
            } else 
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            /* Do not insert code here. */
            {
              /* sizeof/alignof. */
              a_boolean is_sizeof = (con->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_sizeof);
              set_unsigned_integer_constant(
                                  constant, is_sizeof ?
                                    (a_host_large_unsigned)new_type->size :
                                    (a_host_large_unsigned)new_type->alignment,
                                  targ_size_t_int_kind);
            }  /* if */
            con_copy = NULL;
          }  /* if */
        }  /* if */
        break;
      case tpck_template_ref:
        /* The template param constant represents a function template with
           a list of explicit template arguments.  Process the underlying
           tpck_unknown_function constant with the explicit arguments. */
        { a_constant_ptr templ_con =
                          con->variant.template_param.variant.template_ref.con;
          a_template_arg_ptr arg_list = con->variant.template_param.variant.
                                                         template_ref.arg_list;
          /* Do substitution on the template argument list. */
          arg_list = copy_template_arg_list_with_substitution(
                                                    arg_list,
                                                    (a_template_param_ptr)NULL,
                                                    template_arg_list,
                                                    depth,
                                                    source_pos,
                                                    options,
                                                    copy_error);
          /* Apply the template argument list to the template. */
          con_copy = copy_template_param_unknown_entity_con(
                                 templ_con,
                                 template_arg_list,
                                 depth, guide_type,
                                 /*is_address=*/TRUE,
                                 /*is_template_ref=*/TRUE,
                                 arg_list,
                                 source_pos, options,
                                 copy_error, constant);
        }
        break;
      case tpck_expression:
        /* The template param represents an expression that involves
           template parameters.  Substitute the values of the template
           arguments and fold any constant operations that result. */
        { an_expr_node_ptr expr = con->variant.template_param.variant.expr;
          an_expr_node_ptr expr_copy =
                                    copy_template_param_expr(expr,
                                                             template_arg_list,
                                                             depth,
                                                             source_pos,
                                                             options,
                                                             copy_error,
                                                             constant,
                                                             &con_copy);
          if (expr_copy == NULL) {
            /* The expression folds to a constant. */
            /* con_copy and constant are already set correctly. */
          } else if (expr != expr_copy) {
            /* The expression remains an expression, different than the
               original one.  Make a tpck_expression constant for it. */
            *constant = *con;
            constant->variant.template_param.variant.expr = expr_copy;
            con_copy = NULL;
          }  /* if */
        }
        break;
      default:
        unexpected_condition_str("copy_template_param_con: unexpected kind");
    }  /* switch */
  }  /* if */
  return con_copy;
}  /* copy_template_param_con */


a_constant_ptr copy_template_param_con_with_substitution(
                                  a_constant_ptr           con,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_nesting_depth depth,
                                  a_type_ptr               template_param_type,
                                  a_source_position        *source_pos,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error)
/*
Copy a ck_template_param constant, and return a pointer to the
copy.  In the process of copying, replace any template parameters at
depth "depth" with the corresponding values from the template argument list
template_arg_list.  template_param_type, if non-NULL, is the type of the
template parameter for which con is the actual argument.  source_pos
provides the source position for any calls of copy_type_with_substitution.
If there is an error in the copying (specifically, if there is an error
in doing substitution on a type), set *copy_error to TRUE.  The copy of
the constant is always placed in the file scope memory region.  options
is a set of name lookup options.
*/
{
  a_constant_ptr con_copy;
  a_constant     constant;

  a_memory_region_number region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  con_copy = copy_template_param_con(con,
                                     template_arg_list,
                                     depth,
                                     template_param_type,
                                     source_pos,
                                     options,
                                     copy_error,
                                     &constant);
  if (con_copy == NULL) {
    con_copy = alloc_shareable_constant(&constant);
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  return con_copy;
}  /* copy_template_param_con_with_substitution */


a_boolean is_operator_returning_bool(an_expr_operator_kind op)
/*
Return TRUE iff the indicated operator returns a bool (C++) or int (C)
result.  These are the operators for which an implicit "!= 0" need not
be added on a tested condition in the IL.
*/
{
  a_boolean returns_bool;

  switch (op) {
    case eok_land: case eok_lor: case eok_not:
    case eok_ieq: case eok_feq: case eok_peq:
    case eok_ine: case eok_fne: case eok_pne:
    case eok_igt: case eok_fgt: case eok_pgt:
    case eok_ilt: case eok_flt: case eok_plt:
    case eok_ige: case eok_fge: case eok_pge:
    case eok_ile: case eok_fle: case eok_ple:
    case eok_pmne: case eok_pmeq:
    case eok_bool_cast:
      returns_bool = TRUE;
      break;
    default:
      returns_bool = FALSE;
      break;
  }  /* switch */
  return returns_bool;
}  /* is_operator_returning_bool */


an_expr_node_ptr add_cast(an_expr_node_ptr node,
                          a_type_ptr       new_type)
/*
Add a cast to new_type to the node and return the cast node.
new_type should not have any top-level type qualifiers.
*/
{
  return make_operator_node((an_expr_operator_kind)eok_cast, new_type, node);
}  /* add_cast */


an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                       a_type_ptr       new_type)
/*
Add a cast to new_type to the node and return the cast node.  If the
type of the node is already new_type return the original node.
new_type should not have any top-level type qualifiers.
*/
{
  if (!il_identical_types(node->type, new_type)) {
    node = add_cast(node, new_type);
  }  /* if */
  return node;
}  /* add_cast_if_necessary */


an_expr_node_ptr copy_node(an_expr_node_ptr expr)
/*
Allocate a copy of an expression node and return a pointer to it.
*/
{
  an_expr_node_ptr              expr_copy;
  an_expr_node_kind             kind = expr->kind;
  a_new_delete_supplement_ptr   copy_new_delete;
  a_throw_supplement_ptr        copy_throw_info;
  a_condition_supplement_ptr    copy_condition;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  an_eh_prologue_supplement_ptr copy_prologue_info = NULL;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */

  expr_copy = alloc_expr_node(kind);
  /* Preserve the supplement pointer if there is one. */
  if (kind == (an_expr_node_kind)enk_new_delete) {
    copy_new_delete = expr_copy->variant.new_delete;
  } else if (kind == (an_expr_node_kind)enk_throw) {
    copy_throw_info = expr_copy->variant.throw_info;
  } else if (kind == (an_expr_node_kind)enk_condition) {
    copy_condition = expr_copy->variant.condition;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  } else if (kind == (an_expr_node_kind)enk_lowered_eh_construct &&
             expr->variant.lowered_eh.kind ==
                         (a_lowered_eh_construct_kind)leck_function_prologue) {
    copy_prologue_info = expr_copy->variant.lowered_eh.variant.prologue_info;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
  /* Copy the node. */
  *expr_copy = *expr;
  expr_copy->next = NULL;
  expr_copy->result_is_not_used = FALSE;
  if (kind == (an_expr_node_kind)enk_new_delete) {
    /* Copy the new/delete supplement. */
    *copy_new_delete = *expr->variant.new_delete;
    expr_copy->variant.new_delete = copy_new_delete;
  } else if (kind == (an_expr_node_kind)enk_throw) {
    /* Copy the throw supplement. */
    if (expr->variant.throw_info != NULL) {
      *copy_throw_info = *expr->variant.throw_info;
    } else {
      /* Rethrow; discard the throw supplement on the copy. */
      copy_throw_info = NULL;
    }  /* if */
    expr_copy->variant.throw_info = copy_throw_info;
  } else if (kind == (an_expr_node_kind)enk_condition) {
    /* Copy the condition supplement. */
    *copy_condition = *expr->variant.condition;
    expr_copy->variant.condition = copy_condition;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  } else if (copy_prologue_info != NULL) {
    /* Copy the EH prologue supplement. */
    *copy_prologue_info = *expr_copy->variant.lowered_eh.variant.prologue_info;
    expr_copy->variant.lowered_eh.variant.prologue_info = copy_prologue_info;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
  return expr_copy;
}  /* copy_node */


an_expr_node_ptr copy_list_of_expr_trees(an_expr_node_ptr         expr_list,
                                         an_expr_copy_options_set options)
/*
Make a copy of a list of expression trees and return a pointer to it.
options is a set of options for the copy.
*/
{
  an_expr_node_ptr expr, expr_copy, prev_expr_copy, expr_list_copy;

  expr_list_copy = prev_expr_copy = NULL;
  for (expr = expr_list; expr != NULL; expr = expr->next) {
    expr_copy = copy_expr_tree(expr, options);
    if (expr_list_copy == NULL) {
      expr_list_copy = expr_copy;
    } else {
      prev_expr_copy->next = expr_copy;
    }  /* if */
    prev_expr_copy = expr_copy;
  }  /* for */
  return expr_list_copy;
}  /* copy_list_of_expr_trees */


an_expr_node_ptr copy_expr_tree(an_expr_node_ptr         expr,
                                an_expr_copy_options_set options)
/*
Make a copy of an expression tree and return a pointer to it.  options is
a set of options for the copy.
*/
{
  an_expr_node_ptr            expr_copy;
  a_new_delete_supplement_ptr ndsp, copy_ndsp;

  /* Copy the top node. */
  expr_copy = copy_node(expr);
  switch (expr->kind) {
    case enk_error:
    case enk_constant:
    case enk_variable:
    case enk_variable_address:
    case enk_field:
    case enk_routine_address:
    case enk_address_of_ellipsis:
      /* Nothing more to copy. */
      break;
    case enk_operation:
      /* Copy the operands of the operation. */
#if MINIMAL_INLINING
      /* Some short-circuited operations can be simplified while they are
         copied if the first operand value is constant. */
      if ((options & CE_DOING_INLINING_OF_FUNCTION_CALL) &&
          copy_and_simplify_short_circuited_operation(expr_copy)) break;
#endif /* MINIMAL_INLINING */
      expr_copy->variant.operation.operands =
                      copy_list_of_expr_trees(expr->variant.operation.operands,
                                              options);
      if (expr->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
        /* The value of the first operand of a comma operator is not used. */
        set_expr_result_not_used(expr_copy->variant.operation.operands);
      }  /* if */
      break;
    case enk_temp_init:
      /* Copy the dynamic init for a dynamic initialization. */
      expr_copy->variant.init.dynamic_init =
                             copy_dynamic_init(expr->variant.init.dynamic_init,
                                               options);
      /* If the dynamic initialization is attached to a static object lifetime,
         make the temporary static too. */
      { an_object_lifetime_ptr lifetime =
                                expr_copy->variant.init.dynamic_init->lifetime;
        if (lifetime != NULL) {
          if (is_static_object_lifetime_kind(lifetime->kind)) {
            expr_copy->variant.init.static_temp = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case enk_new_delete:
      /* Copy the subtree and dynamic init for a new/delete operation. */
      /* Note that the new/delete supplement was copied by copy_node. */
      ndsp = expr->variant.new_delete;
      copy_ndsp = expr_copy->variant.new_delete;
      if (ndsp->arg != NULL) {
        copy_ndsp->arg = copy_list_of_expr_trees(ndsp->arg, options);
      }  /* if */
      if (ndsp->dynamic_init != NULL) {
        copy_ndsp->dynamic_init = copy_dynamic_init(ndsp->dynamic_init,
                                                    options);
      }  /* if */
      if (ndsp->freeing_of_storage_on_exception != NULL) {
        copy_ndsp->freeing_of_storage_on_exception =
                       copy_dynamic_init(ndsp->freeing_of_storage_on_exception,
                                         options);
      }  /* if */
      break;
    case enk_throw:
      if (expr->variant.throw_info != NULL) {
        /* Copy the dynamic init for a throw. */
        expr_copy->variant.throw_info->dynamic_init =
                      copy_dynamic_init(expr->variant.throw_info->dynamic_init,
                                        options);
      }  /* if */
      break;
    case enk_condition:
      /* Copy the dynamic init and the expression. */
      expr_copy->variant.condition->dynamic_init =
                      copy_dynamic_init(expr->variant.condition->dynamic_init,
                                        options);
      expr_copy->variant.condition->expr =
                      copy_expr_tree(expr->variant.condition->expr, options);
      break;
    case enk_object_lifetime:
      /* For an object lifetime, create a new object lifetime for the copy. */
#if MINIMAL_INLINING
      { a_boolean need_to_pop_function_lifetime = FALSE;
        if (options & CE_DOING_INLINING_OF_FUNCTION_CALL) {
          /* When doing inlining, we might be expanding a function that has
             object lifetimes into a function that has none.  If so, we need
             to add an object lifetime to the current function. */
          if (!in_file_scope(expr_copy)) {
            check_assertion(innermost_function_scope != NULL);
            if (innermost_function_scope->lifetime == NULL) {
              /* Add an object lifetime for the function. */
              push_object_lifetime(iek_scope,
                                   (char *)innermost_function_scope,
                                   (an_object_lifetime_kind)olk_block);
              need_to_pop_function_lifetime = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
#endif /* MINIMAL_INLINING */
        check_assertion_str(curr_object_lifetime != NULL,
                            "copy_expr_tree: curr_object_lifetime is NULL");
        check_assertion_str(expr->variant.object_lifetime.ptr != NULL,
                      "copy_expr_tree: enk_object_lifetime has NULL lifetime");
        if (curr_object_lifetime->kind ==
                                 (an_object_lifetime_kind)olk_expr_temporary) {
          check_assertion(expr->variant.object_lifetime.ptr->kind ==
                                 (an_object_lifetime_kind)olk_expr_temporary);
          /* We're already inside an expr temporary lifetime and we would
             be pushing another.  Ignore this inner lifetime.  This can come
             up with inlining. */
          expr_copy = copy_expr_tree(expr->variant.object_lifetime.expr,
                                     options);
        } else {
          push_object_lifetime(iek_none, (char *)NULL,
                               expr->variant.object_lifetime.ptr->kind);
          expr_copy->variant.object_lifetime.expr =
                             copy_expr_tree(expr->variant.object_lifetime.expr,
                                            options);
          expr_copy->variant.object_lifetime.ptr = NULL;
          bind_object_lifetime(curr_object_lifetime, iek_expr_node,
                               (char *)expr_copy);
          (void)pop_object_lifetime();
          if (expr_copy->variant.object_lifetime.ptr == NULL) {
            /* The copied lifetime was useless and was deleted.  This can
               happen when the source lifetime contains no destructions, which
               comes up when placement delete is lowered.  Eliminate the
               enk_object_lifetime node in the copy. */
            expr_copy = expr_copy->variant.object_lifetime.expr;
          }  /* if */
        }  /* if */
#if MINIMAL_INLINING
        if (need_to_pop_function_lifetime) (void)pop_object_lifetime();
      }
#endif /* MINIMAL_INLINING */
      break;
    case enk_typeid:
      /* If the expr field is non-NULL, copy it. */
      if (expr->variant.typeid_info.expr != NULL) {
        expr_copy->variant.typeid_info.expr =
                       copy_expr_tree(expr->variant.typeid_info.expr, options);
      }  /* if */
      break;
    case enk_runtime_sizeof:
      /* If there is an expression, copy it. */
      if (!expr->variant.runtime_sizeof.is_type) {
        expr_copy->variant.runtime_sizeof.variant.expr =
            copy_expr_tree(expr->variant.runtime_sizeof.variant.expr, options);
      }  /* if */
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    case enk_lowered_eh_construct:
      /* Nothing to copy. */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Nothing to copy. */
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    default:
      unexpected_condition_str("copy_expr_tree: bad expr kind");
  }  /* if */
#if MINIMAL_INLINING
  if (options & CE_DOING_INLINING_OF_FUNCTION_CALL) {
    /* When doing inlining, look for parameters that should be remapped. */
    adjust_copied_expression_for_inlining(expr_copy);
  }  /* if */
#endif /* MINIMAL_INLINING */
  if (options & CE_COPYING_EVALUATED_DEFAULT_ARG_EXPR) {
    do_instantiations_for_copied_default_arg_expr(expr_copy);
  }  /* if */
  return expr_copy;
}  /* copy_expr_tree */


an_expr_node_ptr add_object_lifetime_to_expr(an_expr_node_ptr       expr,
                                             an_object_lifetime_ptr lifetime)
/*
Put an enk_object_lifetime node on top of the indicated expression and
return a pointer to the new expression.
*/
{
  an_expr_node_ptr orig_expr = expr;

  expr = alloc_expr_node((an_expr_node_kind)enk_object_lifetime);
  expr->variant.object_lifetime.expr = orig_expr;
  /* expr->variant.object_lifetime.ptr is set by the bind call. */
  expr->type = orig_expr->type;
  bind_object_lifetime(lifetime, iek_expr_node, (char *)expr);
  return expr;
}  /* add_object_lifetime_to_expr */


an_expr_node_ptr duplicate_default_arg_expr(an_expr_node_ptr expr)
/*
Copy a default argument expression and return a pointer to the copy.
This routine is used to copy such expressions when function types
pointing to default argument expressions are copied, but not to copy
them when they are added implicitly to calls.  The difference between
the two is in the handling of object lifetimes.  The copy is made in
the file scope even if the current IL memory region is a function
scope memory region.
*/
{
  an_object_lifetime_ptr lifetime = NULL,
                         saved_curr_object_lifetime;
  a_memory_region_number region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  if (expr->kind == (an_expr_node_kind)enk_object_lifetime) {
    /* Make a copy of the object lifetime and push it onto the object
       lifetime stack.  Dynamic inits copied will be put into the new
       lifetime. */
    saved_curr_object_lifetime = curr_object_lifetime;
    /* Clear the object lifetime stack before pushing this lifetime. */
    curr_object_lifetime = il_header.primary_scope->lifetime;
    push_object_lifetime(iek_none, (char *)NULL,
                         expr->variant.object_lifetime.ptr->kind);
    lifetime = curr_object_lifetime;
    expr = expr->variant.object_lifetime.expr;
  }  /* if */
  /* Copy the expression. */
  expr = copy_expr_tree(expr, CE_NO_OPTIONS);
  if (lifetime != NULL) {
    /* Put an enk_object_lifetime node on the copy. */
    expr = add_object_lifetime_to_expr(expr, lifetime);
    curr_object_lifetime = saved_curr_object_lifetime;
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  return expr;
}  /* duplicate_default_arg_expr */


an_expr_node_ptr copy_default_arg_expr(
				a_routine_ptr	 rout,
                                a_param_type_ptr ptp,
                                a_boolean        inside_conditional_expression,
                                a_boolean        potentially_evaluated)
/*
Copy the default argument expression from ptp, which is a parameter of
rout, and return a pointer to the copy.  This routine is used to copy
such expressions when they are added implicitly to calls, but not to
copy them when function types pointing to default argument expressions
are copied.  The difference between the two is in the handling of
object lifetimes.  In addition, a flag is set to identify this as a
"generated" default argument expression.
inside_conditional_expression is TRUE if the default argument
expression copy will be inside a conditional part of an expression.
potentially_evaluated is TRUE if the expression is potentially evaluated.
*/
{
  an_expr_copy_options_set options = CE_NO_OPTIONS;
  an_expr_node_ptr	   expr;

  if (ptp->has_unevaluated_template_default) {
    /* This is a parameter of a function template, or a member function of a
       template class, and the default value has not yet been instantiated.
       Instantiate it now. */
    if (rout == NULL) {
      check_assertion_str(total_errors != 0,
                          "copy_default_arg_expr: rout NULL, no error");
      /* Avoid an error recovery problem. */
    } else {
      a_symbol_ptr rout_sym;
      rout_sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
      instantiate_default_argument(rout_sym, ptp);
    }  /* if */
  }  /* if */
  /* Watch out for cases where a default argument is followed by
     a non-default argument.  An error will have been issued at
     the point of declaration of the function, but the problem
     could not be corrected there because the function type may
     have come from a typedef (i.e., it might be shared). */
  if (ptp->default_arg_expr == NULL) {
    expr = error_node();
  } else {
    expr = ptp->default_arg_expr;
    if (expr->kind == (an_expr_node_kind)enk_object_lifetime) {
      /* The top node is an enk_object_lifetime.  The lifetime is not copied.
         Instead, copies of dynamic inits associated with that lifetime will be
         bound into the curr_object_lifetime. */
      expr = expr->variant.object_lifetime.expr;
    }  /* if */
    if (potentially_evaluated) {
      options = CE_COPYING_EVALUATED_DEFAULT_ARG_EXPR;      
      if (inside_conditional_expression) {
        /* The copy will be inside a conditional part of an expression. */
        options |= CE_INSIDE_CONDITIONAL_EXPRESSION;
      }  /* if */
    }  /* if */
    expr = copy_expr_tree(expr, options);
    expr->generated_default_arg = TRUE;
  }  /* if */
  return expr;
}  /* copy_default_arg_expr */


an_expr_node_ptr copy_default_arg_expr_list(
				a_routine_ptr	 rout,
                                a_param_type_ptr ptp,
                                a_boolean        inside_conditional_expression,
                                a_boolean        potentially_evaluated)
/*
Make an expression list containing copies of the default argument
expressions for the parameter indicated by ptp, which is a parameter
of rout, and all parameters following that.  If ptp is non-NULL, it
must point to a parameter with a default argument expression.
inside_conditional_expression is TRUE if the default argument
expression copies will be inside a conditional part of an expression.
potentially_evaluated is TRUE if the expression is potentially evaluated.
*/
{
  an_expr_node_ptr first_node = NULL, last_node = NULL, arg_node;

  if (ptp != NULL) {
#if CHECKING
    if (ptp->default_arg_expr == NULL &&
        !ptp->has_unevaluated_template_default) {
      internal_error("copy_default_arg_expr_list: param has no default arg");
    }  /* if */
#endif /* CHECKING */
    /* Copy the default argument expressions. */
    do {
      arg_node = copy_default_arg_expr(rout, ptp,
                                       inside_conditional_expression,
                                       potentially_evaluated);
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
return a pointer to it.  Note that this routine does not do anything special
for variables with reference type.
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
return a pointer to it.  Note that this routine does not do anything special
for variables with reference type.
*/
{
  an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_variable);
  /* Drop any type qualifiers on the variable type as appropriate. */
  node->type = rvalue_type(var->type);
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
  if (is_error_node(node)) {
    /* Error node -- leave alone. */
  } else {
    a_boolean  optimized_case = FALSE;
    a_type_ptr new_type;
    if (is_variable_address_node(node)) {
      /* A variable address node.  Change to the value of the variable. */
      optimized_case = TRUE;
      node->kind = (an_expr_node_kind)enk_variable;
    } else if (node->kind == (an_expr_node_kind)enk_temp_init &&
               node->variant.init.result_is_addr) {
      /* enk_temp_init node.  Change from "address of temporary" to "value
         of temporary". */
      optimized_case = TRUE;
      node->variant.init.result_is_addr = FALSE;
    } else if (is_operation_node(node)) {
      /* An operation node. */
      an_expr_operator_kind op = node->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_padd ||
          op == (an_expr_operator_kind)eok_padd_subsc) {
        /* A pointer addition; change to a subscripting operation. */
        optimized_case = TRUE;
        node->variant.operation.kind = (an_expr_operator_kind)eok_subscript;
      } else if (op == (an_expr_operator_kind)eok_bit_field) {
        /* The value is the "address" of a bit-field.  Therefore, the
           indirect version is an extract of the bit-field. */
        optimized_case = TRUE;
        node->variant.operation.kind =
                                  (an_expr_operator_kind)eok_extract_bit_field;
      }  /* if */
    }  /* if */
    if (is_template_param_type(node->type)) {
      new_type = type_of_unknown_templ_param_nontype;
    } else {
      /* The new type for the node is the type pointed to. */
      new_type = type_pointed_to(node->type);
      /* Drop type qualifiers as appropriate on rvalues.  Note that no cast
         is needed to drop the qualifiers: an IL shorthand applies in this
         case. */
      if (is_qualified_type(new_type)) {
        new_type = rvalue_type(new_type);
      }  /* if */
    }  /* if */
    if (optimized_case) {
      /* For the optimized cases, just set the node type. */
      node->type = new_type;
    } else {
      /* Not an optimized case.  Add an indirection. */
      node->next = NULL;
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
  expr = var_rvalue_expr(this_param_var);
  return expr;
}  /* this_param_value_expr */


an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                             a_field_ptr      field)
/*
Make an expression for an lvalue reference to field "field" of "node" and
return a pointer to it.  Note that this does NOT add extra intermediate
selections for anonymous unions (either standard or nonstandard).  Within
the front end proper, use fe_field_lvalue_selection_expr instead.
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      field_node;
  a_type_ptr            selection_type, pointed_to_type;
  a_type_qualifier_set  qualifiers;

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
  pointed_to_type = type_pointed_to(node->type);
  qualifiers = get_type_qualifiers(pointed_to_type);
  selection_type = make_field_selection_type(field, qualifiers);
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
selections for anonymous unions (either standard or nonstandard).  Within
the front end proper, use fe_field_rvalue_selection_expr instead.
*/
{
  /* Make the expression node for an lvalue reference. */
  node = field_lvalue_selection_expr(node, field);
  /* Add an indirection to turn the lvalue into an rvalue.  That also
     drops any type qualifiers, as appropriate. */
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
     Note that eok_field and eok_value_field produce an output that is
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
    /* The following is not needed at present, because rvalues in C do not
       have cv-qualifiers, and this routine is called only in C mode or
       for C++ code that is being lowered to C.  But for completeness ... */
    new_selection_type = type_plus_qualifiers_from_second_type(
                                               new_selection_type, node->type);
  } else {
    /* These operators take an lvalue as their input, so use an
       eok_field for the added field selection. */
    a_type_ptr tp = type_pointed_to(op1->type);

    new_op = (an_expr_operator_kind)eok_field;
    /* Carry through any cv-qualifiers on the left operand. */
    new_selection_type =
               type_plus_qualifiers_from_second_type(new_selection_type, tp);
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
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS

void adjust_nonstandard_anonymous_object_field_references(
                                                    an_expr_node_ptr node,
                                                    a_symbol_ptr     field_sym,
                                                    a_boolean        std_also)
/*
node points to an expression for a field selection of the field field_sym.
field_sym is a member of some kind of anonymous parent object.  If it's
a member of a nonstandard anonymous parent (rather than a standard
C++ anonymous union), insert the elided field selections.  If std_also
is TRUE, also do the insertions for standard anonymous unions.
The insertions, if any, are done in place; the expression address does
not change.  This routine can be called only when IL entries still point
back to the associated symbols.
*/
{
  a_symbol_ptr anon_parent_sym = field_sym;
  a_field_ptr  field;

  /* Loop for multiple levels of anonymous parent objects. */
  for (;;) {
    check_assertion(anon_parent_sym->kind == (a_symbol_kind)sk_field);
    field = anon_parent_sym->variant.field.ptr;
    anon_parent_sym = anon_parent_sym->variant.field.anonymous_parent_object;
    /* Stop if there's no anonymous parent, meaning we've handled all
       the levels of anonymous parents. */
    if (anon_parent_sym == NULL) break;
    /* Stop if we've worked up to a top-level (variable) anonymous union. */
    if (anon_parent_sym->kind == (a_symbol_kind)sk_variable) break;
    check_assertion(anon_parent_sym->kind == (a_symbol_kind)sk_field);
    /* In C++, skip a standard anonymous union, because those don't get
       handled here.  But keep looping because there might be more
       nonstandard cases further out. */
    /* If the std_also flag is set, process standard anonymous unions too. */
    if (!std_also && !C_mode()) {
      /* C++.  See if this is an anonymous union case. */
      a_type_ptr field_class = field->source_corresp.parent.class_type;
      a_class_type_supplement_ptr
                 ctsp = field_class->variant.class_struct_union.extra_info;
      /* Skip this level if the field is from a standard anonymous union. */
      if (ctsp->anonymous_union_kind == (an_anonymous_union_kind)auk_field) {
        continue;
      }  /* if */
    }  /* if */
    /* Rewrite the field selection to add an implied selection. */
    adjust_anonymous_union_field_selection(node,
                                           anon_parent_sym->variant.field.ptr);
    /* Loop to see if the rewritten first operand still refers to an
       anonymous union field (because there are several nested anonymous
       unions), and if so, rewrite it. */
    node = node->variant.operation.operands;
  }  /* for */
}  /* adjust_nonstandard_anonymous_object_field_references */

#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

an_expr_node_ptr fe_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                a_field_ptr      field)
/*
Make an expression for an lvalue reference to field "field" of "node" and
return a pointer to it.  This is intended for use in the front end
proper.  It inserts extra field selections for nonstandard anonymous
unions, but not for standard anonymous unions (those are added by
IL lowering).
*/
{
  node = field_lvalue_selection_expr(node, field);
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  { a_symbol_ptr field_sym = (a_symbol_ptr)field->source_corresp.assoc_info;
    adjust_nonstandard_anonymous_object_field_references(node, field_sym,
                                                         /*std_also=*/FALSE);
  }
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  return node;
}  /* fe_field_lvalue_selection_expr */


an_expr_node_ptr fe_field_rvalue_selection_expr(an_expr_node_ptr node,
                                                a_field_ptr      field)
/*
Make an expression for an rvalue reference to field "field" of "node" and
return a pointer to it.  This is intended for use in the front end
proper.  It inserts extra field selections for nonstandard anonymous
unions, but not for standard anonymous unions (those are added by
IL lowering).
*/
{
  node = fe_field_lvalue_selection_expr(node, field);
  /* Add an indirection to turn the lvalue into an rvalue.  That also
     drops any type qualifiers, as appropriate. */
  node = add_indirection_to_node(node);
  return node;
}  /* fe_field_rvalue_selection_expr */


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
              report_abstract_class_error(ec_function_returning_abstract_class,
                                          return_type, err_pos);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_routine_calling_method_flag */


void mark_routine_referenced_full(a_routine_ptr routine,
                                  a_boolean     instantiate)
/*
Mark the indicated routine as actually referenced.  "Actually" means
as opposed to referenced in a virtual function call that may call some
other virtual function.  Note that an actual reference does not necessarily
mean that the routine is called; this reference might be taking the address
of the routine.  It does, however, force instantiation if the function
is a template function, and/or definition if the function is the right kind
of compiler-generated function (e.g., a constructor).  Instantiation is
forced only if instantiate is TRUE.
*/
{
  a_symbol_ptr             assoc_sym;

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
    routine->source_corresp.parent.class_type->
                                   source_corresp.referenced = TRUE;
  }  /* if */
  /* If the routine is compiler-generated and its definition has not
     yet been put out, force the definition now. */
  force_definition_of_compiler_generated_routine(routine);
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* In Microsoft mode, friend functions defined in class templates are
     only analyzed if they are used.  A non-NULL routine_fixup pointer
     indicates that the definition has not yet been processed.  This special
     treatment is also extended to Microsoft mode specializations that are
     defined within a class. */
  if (routine->routine_fixup != NULL) {
    microsoft_friend_function_fixup(routine->routine_fixup);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* If the function is an instance of a function template, mark it
     as requiring an instantiation.  This is also done for extern inline
     functions when inline functions are instantiated using a
     mechanism like the template instantiation mechanism. */
  assoc_sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
  if (instantiate && assoc_sym != NULL) {
    set_instance_required(assoc_sym, TRUE, /*defer_inline=*/FALSE);
  }  /* if */
}  /* mark_routine_referenced_full */


void mark_routine_referenced(a_routine_ptr routine)
/*
Interface to mark_routine_referenced_full for the simple case.
*/
{
  mark_routine_referenced_full(routine, /*instantiate=*/TRUE);
}  /* mark_routine_referenced */


void set_routine_defined(a_routine_ptr rout)
/*
Set the "defined" flag for a routine.  This can kick off some processing
related to "needed" flags.
*/
{
  if (!rout->defined) {
    rout->defined = TRUE;
#if MAINTAIN_NEEDED_FLAGS
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "Setting defined on rout ");
      db_name(&rout->source_corresp);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* If the definition_needed flag was set previously, set it again so
       the function body will be scanned.  The fact that rout->defined was
       FALSE prevented the scanning of the body.  This also sets the
       keep_definition_in_il flag if the routine is needed; clear it to
       make sure the subtree is walked if it was set already (if it
       wasn't set, clearing it does nothing). */
    if (rout->definition_needed) rout->keep_definition_in_il = FALSE;
    remark_routine_definition_needed(rout);
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
}  /* set_routine_defined */


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
  a_boolean       has_associated_pragma = to->has_associated_pragma;
  a_statement_ptr to_next = to->next;

  *to = *from;
  /* Preserve the pragma flag in the destination statement. */
  to->has_associated_pragma = has_associated_pragma;
  /* Preserve the next pointer of the destination statement. */
  to->next = to_next;
  /* If the statement is a label, bind the a_label to the copy. */
  if (to->kind == (a_statement_kind)stmk_label) {
    to->variant.label.ptr->variant.exec_stmt = to;
  } else if (to->kind == (a_statement_kind)stmk_block) {
    /* If the statement is a block with an associated scope, change the
       back-pointer from the scope to point to the copy. */
    a_scope_ptr scope = to->variant.block.extra_info->assoc_scope;
    if (scope != NULL) scope->assoc_block = to;
  }  /* if */
}  /* copy_statement */


void change_statement_into_block(a_statement_ptr statement,
                                 a_statement_ptr *orig_statement)
/*
Turn a statement into a block by allocating a new statement, copying
the statement to it, and changing the original statement into a block
containing the copied statement.  *orig_statement is set to point to the
original statement in its new location.
*/
{
  a_statement_ptr stmt_copy;

  /* Make a copy of the original statement. */
  *orig_statement = stmt_copy = alloc_statement(statement->kind);
  copy_statement(statement, stmt_copy);
  /* Turn the statement into a block statement. */
  set_statement_kind(statement, (a_statement_kind)stmk_block);
  statement->variant.block.statements = stmt_copy;
  clear_stmt_source_position(statement->position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  clear_stmt_source_position(statement->end_position);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* change_statement_into_block */


void add_to_pragma_list(a_pragma_ptr             pragma,
                        a_scope_depth            scope_depth,
			a_source_correspondence  *scp)
/*
Add the indicated pragma entry to the end of the pragmas list of the
appropriate scope.  scope_depth may be specified, in which case the
corresponding IL scope is used.  Otherwise, *scp will point to the source
correspondence of the entity (a class or namespace member) to which the
pragma is bound, and the pragma will be entered in the IL scope associated
with the class or namespace.
*/
{
  a_scope_ptr                 sp;
  a_scope_pointers_block_ptr  pointers_block = NULL;

  if (scope_depth == NO_SCOPE_DEPTH) {
    check_assertion_str(scp != NULL,
                        "add_to_pragma_list: NULL source corresp ptr");

    /* The pragma is bound to a member of a class or namespace.  The binding
       may be taking place in the scope of the class or may be taking place
       in some other scope.  A static data member definition may have a
       pragma bound to it at file scope and a friend declaration may have a
       pragma bound to it in the scope of some other class.  A pragma bound
       to a class member is always entered on the pragma list of the scope
       of the class.  */
    if (scp->is_class_member) {
      sp = scp->parent.class_type->
                  variant.class_struct_union.extra_info->assoc_scope;
      scope_depth = sp->depth_in_scope_stack;
      if (scope_depth != NO_SCOPE_DEPTH) {
        pointers_block = &scope_stack[scope_depth].pointers_block;
      } else {
        /* The scope stack entry is no longer available. */
        pointers_block = NULL;
      }  /* if */
    } else {
      a_namespace_ptr  nsp = scp->parent.namespace_ptr;
      check_assertion_str(nsp, "add_to_pragma_list: NULL namespace ptr");
      check_assertion_str(!nsp->is_namespace_alias,
                          "add_to_pragma_list: namespace alias not expected");
      sp = nsp->variant.assoc_scope;
      pointers_block = &symbol_supplement_for_namespace(nsp)->pointers_block;
    }  /* if */
    /* If the scope of the class or pragma is still on the scope stack, get
       a pointer to the scope stack entry. */
  } else {
    a_scope_stack_entry_ptr  ssep = &scope_stack[scope_depth];
    sp = ensure_il_scope_exists(ssep);
    check_assertion_str(sp != NULL, "add_to_pragma_list: NULL IL scope");
    pointers_block = assoc_pointers_block_of(ssep);
  }  /* if */
  if (sp->pragmas == NULL) {
    sp->pragmas = pragma;
  } else if (pointers_block == NULL) {
    /* No scope stack entry, find the end of the pragma list.  Note that
       the case where sp->pragmas is NULL is already tested above. */
    a_pragma_ptr	end_of_list = sp->pragmas;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = pragma;
  } else {
    pointers_block->last_pragma->next = pragma;
  }  /* if */
  if (pointers_block != NULL) pointers_block->last_pragma = pragma;
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
    /* A pragma has already been found that is associated with *il_entity.
       Any additional pragmas associated with the same entity will be among
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

static void add_to_destructions_list(a_dynamic_init_ptr      dip,
                                     an_object_lifetime_ptr  olp)
/*
Add the indicated dynamic init entry to the destructions list of the
indicated object lifetime entry.
*/
{
  check_assertion_str2(in_file_scope(olp) == in_file_scope(dip),
                       "add_to_destructions_list: object lifetime",
                       "and dynamic init in different memory regions");
  check_assertion_str2(dip->lifetime == NULL,
                       "add_to_destructions_list:",
                       "entry is already on a destructions list");
  /* Add the dynamic init entry to the front of the destructions list for
     the lifetime.  (It's on the front because the last entry constructed
     will be the first entry destructed.) */
  dip->next_in_destruction_list = olp->destructions;
  olp->destructions = dip;
  /* Update the lifetime pointer in the dynamic init entry. */
  dip->lifetime = olp;
}  /* add_to_destructions_list */


void add_to_end_of_destructions_list(a_dynamic_init_ptr      dip,
                                     an_object_lifetime_ptr  olp)
/*
Add the indicated dynamic init entry to the end of the destructions list
of the indicated object lifetime entry.
*/
{
  a_dynamic_init_ptr     last_dip;
  an_object_lifetime_ptr colp;

  check_assertion_str2(in_file_scope(olp) == in_file_scope(dip),
                       "add_to_end_of_destructions_list: object lifetime",
                       "and dynamic init in different memory regions");
  check_assertion_str2(dip->lifetime == NULL,
                       "add_to_end_of_destructions_list:",
                       "entry is already on a destructions list");
  last_dip = olp->destructions;
  if (last_dip == NULL) {
    /* The list is empty, so the entry goes at the front. */
    olp->destructions = dip;
  } else {
    /* Find the end of the list and insert there. */
    for (; last_dip->next_in_destruction_list != NULL;
         last_dip = last_dip->next_in_destruction_list) {}
    last_dip->next_in_destruction_list = dip;
  }  /* if */
  dip->next_in_destruction_list = NULL;
  /* Update the lifetime pointer in the dynamic init entry. */
  dip->lifetime = olp;
  /* Any child lifetime whose parent_destruction_sublist indicates that
     it is at the beginning of the lifetime should include this destruction. */
  for (colp = olp->child_lifetime; colp != NULL; colp = colp->next) {
    if (colp->parent_destruction_sublist == NULL) {
      colp->parent_destruction_sublist = dip;
    }  /* if */
  }  /* for */
}  /* add_to_end_of_destructions_list */


void record_end_of_lifetime_destruction(a_dynamic_init_ptr  dip,
                                        a_boolean           static_lifetime,
                                        a_boolean           block_lifetime)
/*
If the dynamic init entry pointed to by dip has a destructor associated with
it, add the entry to the destructors list for the appropriate object
lifetime.  If static_lifetime is TRUE, the object in question has static
storage duration -- it persists till the end of program execution (i.e., till
final object clean up).  If block_lifetime is TRUE, use the innermost
olk_block or olk_block_after_label object lifetime (i.e., skip the current
object lifetime if it is an expr-temporary lifetime).
*/
{
  an_object_lifetime_ptr  olp;

  db_enter(4, "record_end_of_lifetime_destruction");
  if (dip->destructor != NULL) {
    /* This is a destructible entity. */
    if (static_lifetime) {
      /* Note that we do NOT use depth_innermost_function_scope, as it would
         not be set correctly during IL lowering. */
      if (innermost_function_scope != NULL) {
        /* The object is a local static variable.  Use the list on the
           scope entry for the function. */
        a_scope_ptr sp = innermost_function_scope;
        olp = sp->variant.routine.lifetime_of_local_static_vars;
        if (olp == NULL) {
          olp = alloc_object_lifetime(
                             (an_object_lifetime_kind)olk_function_static);
          bind_object_lifetime(olp, (an_il_entry_kind)iek_scope, (char *)sp);
        }  /* if */
      } else {
        /* The object is a static object outside a function context -- it
           belongs to the lifetime of the file scope itself. */
        olp = scope_stack[DEPTH_OF_FILE_SCOPE].curr_scope_object_lifetime;
      }  /* if */
    } else {
      an_object_lifetime_ptr temp_olp = NULL;
      /* Not a static lifetime. */
      if (block_lifetime) {
        /* Want the innermost block lifetime. */
        /* Skip an expr-temporary lifetime, if any. */
        olp = innermost_block_object_lifetime(curr_object_lifetime);
      } else {
        /* The default case is to use whatever is on top of the object lifetime
           stack. */
        olp = curr_object_lifetime;
      }  /* if */
      temp_olp = dip->init_expr_lifetime;
      if (temp_olp == NULL) temp_olp = curr_object_lifetime;
      if (temp_olp != olp) {
        /* This entity is initialized during a nested object lifetime.
           If the nested lifetime has any destructible temporaries, they
           will be destroyed after this entity has been constructed.
           Adjust the parent pointer from the nested lifetime so that it
           includes this entity. */
        /* Find the lifetime immediately under the olp lifetime and
           adjust its parent_destruction_sublist. */
        /* Note that some of the entries on the list may relate to freeing
           of storage allocated by a "new" (is_freeing_of_storage_on_exception
           is TRUE) rather than being temporaries in the strict sense. */
        while (temp_olp->parent_lifetime != olp) {
          temp_olp = temp_olp->parent_lifetime;
        }  /* while */
        if (temp_olp->destructions != NULL) {
          temp_olp->parent_destruction_sublist = dip;
          dip->overlaps_temps_in_inner_lifetime = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Now that we've determined the appropriate object lifetime, add the
       dynamic init entry to its destructions list. */
    add_to_destructions_list(dip, olp);
#if DEBUG
  if (debug_level >= 4) {
    db_pending_destructions(dip, (an_object_lifetime_ptr)NULL);
  }  /* if */
#endif /* DEBUG */
  }  /* if */
  db_exit();
}  /* record_end_of_lifetime_destruction */


void move_destruction_to_curr_object_lifetime(a_dynamic_init_ptr  dip)
/*
Move a dynamic init entry representing a destruction (or a list thereof) to
the destructions list associated with the current object lifetime.  This
is routine is called to promote the destruction list from an object
lifetime that is being discarded.
*/
{
  if (dip->next_in_destruction_list != NULL) {
    /* This dynamic init entry is part of a list.  Preserve the original
       order by processing the next entry first. */
    move_destruction_to_curr_object_lifetime(dip->next_in_destruction_list);
    dip->next_in_destruction_list = NULL;
  }  /* if */
  /* Disassociate this destruction from the old lifetime. */
  dip->lifetime = NULL;
  record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                     /*block_lifetime=*/FALSE);
}  /* move_destruction_to_curr_object_lifetime */

#if DEBUG

void db_destruction(a_dynamic_init_ptr  dip)
/*
Dump debug information on a dynamic init entry insofar as it represents a
destruction.
*/
{
  if (dip->variable != NULL) {
    fputs("variable: \"", f_debug);
    db_name(&dip->variable->source_corresp);
    fputs("\", ", f_debug);
  }  /* if */
  db_destructor(dip);
}  /* db_destruction */


void db_object_lifetime_name(an_object_lifetime_ptr olp)
/*
Dump the "name" of an object lifetime (really, some identifying information
about it).
*/
{
  char            *str;

  switch (olp->kind) {
    case olk_block_after_label: str = "block_after_label"; break;
    case olk_global_static:     str = "global_static";     break;
    case olk_block:             str = "block";             break;
    case olk_function_static:   str = "function_static";   break;
    case olk_expr_temporary:    str = "expr_temporary";    break;
    case olk_try_block:         str = "try_block";         break;
    default:                    str = "***BAD LIFETIME KIND***"; break;
  }  /* switch */
  fprintf(f_debug, "%s [", str);
  if (olp->kind == (an_object_lifetime_kind)olk_block_after_label) {
    if (olp->entity.kind == (a_byte_il_entry_kind)iek_statement) {
      a_statement_ptr  sp = (a_statement_ptr)olp->entity.ptr;
      if (sp->kind == (a_statement_kind)stmk_label) {
        fputc('"', f_debug);
        db_name(&sp->variant.label.ptr->source_corresp);
        fputs("\" ", f_debug);
      } else {
        db_statement_kind((a_statement_kind)sp->kind);
        fputs("-stmt", f_debug);
      }  /* if */
    } else if (olp->entity.kind == (a_byte_il_entry_kind)iek_switch_clause) {
      a_constant_ptr  cp;
      cp = ((a_switch_clause_ptr)olp->entity.ptr)->constant_list;
      if (cp != NULL) {
        fputs("case ", f_debug);
        db_constant(cp);
        fputc(' ', f_debug);
      } else {
        fputs("default ", f_debug);
      }  /* if */
    }  /* if */
    fputs("==> ", f_debug);
    do {
      olp = olp->parent_lifetime;
    } while (olp != NULL &&
             olp->kind == (an_object_lifetime_kind)olk_block_after_label);
  }  /* if */
  if (olp == NULL) {
    fputs("<null>", f_debug);
  } else if (olp->entity.kind == (a_byte_il_entry_kind)iek_scope) {
    db_scope((a_scope_ptr)olp->entity.ptr);
  } else if (olp->entity.kind == (a_byte_il_entry_kind)iek_expr_node) {
    fprintf(f_debug, "expr-node@%lx:", (unsigned long)(olp->entity.ptr));
    db_expr_summary((an_expr_node_ptr)(olp->entity.ptr));
  } else if (olp->entity.kind == (a_byte_il_entry_kind)iek_none) {
    fputs("<unbound>", f_debug);
  } else {
    fprintf(f_debug, "%s@%lx", il_entry_kind_names[(int)olp->entity.kind],
                               (unsigned long)(olp->entity.ptr));
  }  /* if */
  fputc(']', f_debug);
}  /* db_object_lifetime_name */
    

void db_object_lifetime(an_object_lifetime_ptr  olp)
/*
Dump debug information about an object lifetime entry.
*/
{
  a_dynamic_init_ptr  dip;

  if (olp == NULL) {
    fputs("null object lifetime\n", f_debug);
  } else {
    db_object_lifetime_name(olp);
    /* Dump the "name" of the parent. */
    if (olp->parent_lifetime != NULL) {
      fprintf(f_debug, "\n  parent_lifetime = ");
      db_object_lifetime_name(olp->parent_lifetime);
    }  /* if */
    /* Dump the "name" of each of the children. */
    if (olp->child_lifetime != NULL) {
      an_object_lifetime_ptr  temp = olp->child_lifetime->next;
      fprintf(f_debug, "\n  child_lifetime = ");
      db_object_lifetime_name(olp->child_lifetime);
      for (; temp != NULL; temp = temp->next) {
        fputs("\n                   ", f_debug);
        db_object_lifetime_name(temp);
      }  /* for */
    }  /* if */
    /* Dump the "name" of the next entry in the sibling list. */
    if (olp->next != NULL) {
      fprintf(f_debug, "\n  next = ");
      db_object_lifetime_name(olp->next);
    }  /* if */
    /* Dump the destructions list. */
    dip = olp->destructions;
    if (dip != NULL) {
      fprintf(f_debug, "\n  destructions = ");
      db_destruction(dip);
      dip = dip->next_in_destruction_list;
      for (; dip != NULL; dip = dip->next_in_destruction_list) {
        fputs("\n                 ", f_debug);
        db_destruction(dip);
      }  /* for */
    }  /* if */
    fputc('\n', f_debug);
  }  /* if */
}  /* db_object_lifetime */


void db_object_lifetime_stack(void)
/*
Dump information about the object lifetime stack (i.e., start with
curr_object_lifetime and dump the name of each entry found by following
the parent_lifetime pointer.
*/
{
  an_object_lifetime_ptr  olp = curr_object_lifetime;

  fprintf(f_debug,
          "object_lifetime_stack:%s\n", olp == NULL ? " <empty>" : "");
  for (; olp != NULL; olp = olp->parent_lifetime) {
    fputs("  ", f_debug);
    db_object_lifetime_name(olp);
    fputc('\n', f_debug);
  }  /* for */
}  /* db_object_lifetime_stack */


void db_pending_destructions(a_dynamic_init_ptr      dip,
                             an_object_lifetime_ptr  stop_at)
/*
Dump all the destructions that are active on the object lifetime stack,
stopping when the object lifetime indicated by stop_at is reached.
*/
{
  an_object_lifetime_ptr  olp;

  if (dip != NULL && dip->lifetime != stop_at) {
    olp = dip->lifetime;
    fputs("pending destructions:\n", f_debug);
    for (; olp != NULL && olp != stop_at; olp = olp->parent_lifetime) {
      fputs("  --for lifetime associated with ", f_debug);
      /* Dump the "name" of each object lifetime. */
      db_object_lifetime_name(olp);
      fputc(':', f_debug);
      /* Dump the destructions associated with it. */
      if (dip == NULL) {
        fputs(" <none>", f_debug);
      } else {
        for (; dip != NULL; dip = dip->next_in_destruction_list) {
          fputs("\n      ", f_debug);
          db_destruction(dip);
        }  /* for */
      }  /* if */
      fputc('\n', f_debug);
      /* Note: on a parent destruction list, only those following
         parent_destruction_sublist are active from the point of view of the
         current stack.  Set dip to start at the right entry. */
      dip = olp->parent_destruction_sublist;
    }  /* for */
  }  /* if */
}  /* db_pending_destructions */


static void db_object_lifetime_with_indentation(an_object_lifetime_ptr  olp,
                                                char                    *str)
/*
Display an object lifetime in a special format, for use when dump_lifetimes
has been enabled at the command line.  The display line includes the current
sequence number, indentation corresponding to the depth of object lifetime
stack, a string supplied by the caller, and the object lifetime "name".
*/
{
  an_object_lifetime_ptr  parent = olp->parent_lifetime;

  fprintf(f_debug, "OL-%.4d..", (int)pos_curr_token.seq);
  if (olp->kind == (an_object_lifetime_kind)olk_block_after_label) {
    while (parent->kind == (an_object_lifetime_kind)olk_block_after_label) {
      parent = parent->parent_lifetime;
    }  /* while */
    parent = parent->parent_lifetime;
  }  /* if */
  for (; parent != NULL; parent = parent->parent_lifetime) {
    if (parent->kind != (an_object_lifetime_kind)olk_block_after_label) {
      fputs("..", f_debug);
    }  /* if */
  }  /* for */
  if (str != NULL) fputs(str, f_debug);
  db_object_lifetime_name(olp);
  fputc('\n', f_debug);
}  /* db_object_lifetime_with_indentation */


void db_object_lifetime_tree(an_object_lifetime_ptr olp)
/*
Dump information about the indicated object lifetime and the lifetimes and
destructions under it.
*/
{
  db_object_lifetime(olp);
  if (olp != NULL) {
    an_object_lifetime_ptr  temp = olp->child_lifetime;
    for (; temp != NULL; temp = temp->next) {
      db_object_lifetime_tree(temp);
    }  /* for */
  }  /* if */
}  /* db_object_lifetime_tree */

#endif /* DEBUG */

void free_object_lifetime(an_object_lifetime_ptr  olp)
/*
Return an object lifetime to the appropriate available list.
*/
{
  an_object_lifetime_ptr  *avail_list_ptr;
  a_scope_depth           scope_depth;

  db_enter(5, "free_object_lifetime");
  if (in_file_scope(olp)) {
    /* Return the entry to the file scope's available list. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else {
    /* Use the current function scope. */
    scope_depth = depth_innermost_function_scope;
  }  /* if */
  /* When IL lowering generates routines, there is no scope stack entry,
     and therefore no available list can be maintained. */
  if (scope_depth != NO_SCOPE_DEPTH) {
    /* Copy the address of the available list. */
    avail_list_ptr = &scope_stack[scope_depth].object_lifetime_avail_list;
    /* Link it onto the front of the available list. */
    olp->next = *avail_list_ptr;
    *avail_list_ptr = olp;
  }  /* if */
  db_exit();
}  /* free_object_lifetime */


static an_object_lifetime_ptr *addr_of_lifetime_ptr(
                                         an_il_entry_kind         entity_kind,
                                         char                     *entity_ptr,
                                         an_object_lifetime_kind  kind)
/*
Given an IL entry kind and a pointer to the entry, return the address of the
field of that entry that points to an object lifetime.  Since scope entries
have two such pointers, the object lifetime kind is also passed in to help
determine which address to return.
*/
{
  an_object_lifetime_ptr *lifetime_addr;

  switch (entity_kind) {
    case iek_scope:
      /* There are three lifetime pointers in a scope entry.  Use kind to
         select the right one. */
      if (kind == (an_object_lifetime_kind)olk_function_static) {
        lifetime_addr = &((a_scope_ptr)entity_ptr)->
                              variant.routine.lifetime_of_local_static_vars;
      } else {
        lifetime_addr = &((a_scope_ptr)entity_ptr)->lifetime;
      }  /* if */
      break;
    case iek_expr_node:
      check_assertion(((an_expr_node_ptr)entity_ptr)->kind ==
                                  (an_expr_node_kind)enk_object_lifetime);
      lifetime_addr = &((an_expr_node_ptr)entity_ptr)->
                                              variant.object_lifetime.ptr;
      break;      
    case iek_block:
      lifetime_addr = &((a_block_ptr)entity_ptr)->lifetime;
      break;
    case iek_try_supplement:
      lifetime_addr = &((a_try_supplement_ptr)entity_ptr)->lifetime;
      break;
    case iek_dynamic_init:
      lifetime_addr = &((a_dynamic_init_ptr)entity_ptr)->init_expr_lifetime;
      break;
    case iek_local_static_variable_init:
      lifetime_addr =
                     &((a_local_static_variable_init_ptr)entity_ptr)->lifetime;
      break;
#if CHECKING
    default:
      internal_error("addr_of_lifetime_ptr: bad il entry kind");
#endif /* CHECKING */
  }  /* switch */
  return lifetime_addr;
}  /* addr_of_lifetime_ptr */


void bind_object_lifetime(an_object_lifetime_ptr  olp,
                          an_il_entry_kind        entity_kind,
                          char                    *entity_ptr)
/*
Set the object lifetime entry pointed to by olp to point to the IL entry
represented by entity_kind and entity_ptr, and set the IL entry to point
back to it.  This function should not be called for olk_block_after_label
lifetimes, since those are never bound.
*/
{
  an_object_lifetime_ptr   *lifetime_addr;

#if CHECKING
  char *str = "bind_object_lifetime:";

  check_assertion_str2(entity_ptr != NULL, str, "NULL entity");
  check_assertion_str2(olp->entity.ptr == NULL, str, "lifetime already bound");
  /* Be sure the object lifetime kind is consistent with the kind of
     entity with which the object lifetime is being bound. */
  switch (olp->kind) {
    case olk_function_static:
      check_assertion_str2((entity_kind == (an_il_entry_kind)iek_scope) &&
                           (((a_scope_ptr)entity_ptr)->kind ==
                                         (a_scope_kind)sck_function),
                           str,
                           "bad entity or scope kind for olk_function_static");
      break;
    case olk_global_static:
      check_assertion_str2((entity_kind == (an_il_entry_kind)iek_scope) &&
                           (((a_scope_ptr)entity_ptr)->kind ==
                                         (a_scope_kind)sck_file),
                           str,
                           "bad entity or scope kind for olk_global_static");
      break;
    case olk_block:
      switch (entity_kind) {
        case iek_scope:
          check_assertion_str2((((a_scope_ptr)entity_ptr)->kind ==
                                           (a_scope_kind)sck_function) ||
                               (((a_scope_ptr)entity_ptr)->kind ==
                                         (a_scope_kind)sck_block) ||
                               (((a_scope_ptr)entity_ptr)->kind ==
                                         (a_scope_kind)sck_condition),
                               str, "bad scope kind for olk_block");
          break;
        case iek_local_static_variable_init:
        case iek_block:
          /* Okay. */
          break;
        default:
          unexpected_condition_str2(str, "bad entity kind for olk_block");
      }  /* switch */
      break;
    case olk_block_after_label:
      switch (entity_kind) {
        case iek_statement:
        case iek_switch_clause:
          /* Okay. */
          break;
        default:
          unexpected_condition_str2(
                           str, "bad entity kind for olk_block_after_label");
      }  /* switch */
      break;
    case olk_expr_temporary:
      switch (entity_kind) {
        case iek_block:
        case iek_expr_node:
        case iek_dynamic_init:
          /* Okay. */
          break;
        default:
          unexpected_condition_str2(str,
                                    "bad entity kind for olk_expr_temporary");
      }  /* switch */
      break;
    case olk_try_block:
      switch (entity_kind) {
        case iek_try_supplement:
        case iek_block:
          /* Okay. */
          break;
        default:
          unexpected_condition_str2(str, "bad entity kind for olk_try_block");
      }  /* switch */
      break;
    default:
      unexpected_condition_str2(str, "bad object lifetime kind");
  }  /* if */
#endif /* CHECKING */
  /* Point the object lifetime at the IL entry. */
  olp->entity.kind = (a_byte_il_entry_kind)entity_kind;
  olp->entity.ptr = entity_ptr;
  if (olp->kind != (an_object_lifetime_kind)olk_block_after_label) {
    /* Get the address of the appropriate field of the IL entry and point
       back to the object lifetime entry. */
    lifetime_addr = addr_of_lifetime_ptr(entity_kind, entity_ptr, olp->kind);
    check_assertion(*lifetime_addr == NULL);
    *lifetime_addr = olp;
  }  /* if */
#if DEBUG
  if (db_flag_is_set("dump_lifetimes")) {
    db_object_lifetime_with_indentation(olp, "Binding: ");
  }  /* if */
#endif /* DEBUG */
}  /* bind_object_lifetime */


void unbind_object_lifetime(an_object_lifetime_ptr  olp)
/*
Undo the binding between an object lifetime entry and the IL entry to which
it points.
*/
{
  an_object_lifetime_ptr  *lifetime_addr;

  if (olp->kind != (an_object_lifetime_kind)olk_block_after_label) {
    /* Get the address of the appropriate field of the IL entry so that the
       lifetime pointer can be cleared. */
    lifetime_addr = addr_of_lifetime_ptr((an_il_entry_kind)olp->entity.kind,
                                         olp->entity.ptr, olp->kind);
    *lifetime_addr = NULL;
  }  /* if */
  /* Clear the fields in the object lifetime, too. */
  olp->entity.kind = (a_byte_il_entry_kind)iek_none;
  olp->entity.ptr = NULL;
}  /* unbind_object_lifetime */

      
void push_object_lifetime(an_il_entry_kind         entity_kind,
                          char                     *entity_ptr,
                          an_object_lifetime_kind  kind)
/*
Create a new object lifetime entry of the specified kind and push it onto the
object lifetime stack by setting its parent pointer and then changing
curr_object_lifetime to point to it.  Also, set its sibling pointer, and, if
entity_ptr is non-NULL, bind it to the IL entity with which it is associated.
(When entity_ptr is NULL, the binding takes place later, when we are sure the
entry is needed.)
*/
{
  an_object_lifetime_ptr   olp, parent;

  db_enter(3, "push_object_lifetime");
  check_assertion_str(kind != (an_object_lifetime_kind)olk_function_static,
                      "push_object_lifetime: olk_function_static not allowed");
  olp = alloc_object_lifetime(kind);
  if (kind == (an_object_lifetime_kind)olk_global_static) {
    /* No parent pointer. */
  } else {
    check_assertion_str2(curr_object_lifetime->kind !=
                                  (an_object_lifetime_kind)olk_expr_temporary,
                         "push_object_lifetime:",
                         "pushing on top of olk_expr_temporary not allowed");
    /* Link the new entry into the object lifetime tree. */
    parent = curr_object_lifetime;
    olp->parent_lifetime = parent;
    if (entity_kind == (an_il_entry_kind)iek_scope && entity_ptr != NULL &&
        ((a_scope_ptr)entity_ptr)->kind == (a_scope_kind)sck_function) {
      /* This is an object lifetime for a function scope; its parent pointer
         is the file scope lifetime entry, but it's an "implicit" child of the
         latter -- because of a memory region incompatibility, olp doesn't
         appear explicitly on the child_lifetime list of its parent . */
      check_assertion(scope_stack[DEPTH_OF_FILE_SCOPE].il_scope ==
                             (a_scope_ptr)parent->entity.ptr);
      /* Don't add the current entry to the parent's list of children, and
         don't update the sibling pointer. */
    } else {
#if CHECKING
      if (in_file_scope(olp) != in_file_scope(parent)) {
        if (in_file_scope(parent)) {
          unexpected_condition_str2("push_object_lifetime: parent is in",
                                    "file scope memory, new olp is not");
        } else {
          unexpected_condition_str2("push_object_lifetime: new olp is in",
                                    "file scope memory, parent is not");
        }  /* if */
      }  /* if */
#endif /* CHECKING */
      /* If the parent already has a list of children, add the new entry to
         the front of the list. */
      olp->next = parent->child_lifetime;
      parent->child_lifetime = olp;
      if (kind == (an_object_lifetime_kind)olk_block_after_label) {
        parent->has_block_after_label_child_lifetime = TRUE;
      }  /* if */
      /* Record the current position in the dynamic inits list of the
         parent. */
      olp->parent_destruction_sublist = parent->destructions;
    }  /* if */
  }  /* if */
  /* Bind the object lifetime and the entity with which it is associated. */
  if (entity_ptr != NULL) {
    bind_object_lifetime(olp, entity_kind, entity_ptr);
#if DEBUG
  } else if (db_flag_is_set("dump_lifetimes")) {
    if (kind != (an_object_lifetime_kind)olk_expr_temporary ||
        long_lifetime_temps) {
      db_object_lifetime_with_indentation(olp, "Adding: ");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* Now set the new entry to be the current object lifetime. */
  curr_object_lifetime = olp;
#if DEBUG
  if (debug_level >= 3) db_object_lifetime_stack();
#endif /* DEBUG */
  db_exit();
}  /* push_object_lifetime */


static a_boolean any_destruction_has_temp_lifetime(an_object_lifetime_ptr olp)
/*
Do through the destructions for lifetime *olp and return TRUE if any is
marked as having "temporary lifetime" -- i.e., a lifetime (long or short)
that is not governed by the lifetime of another entity to which it is
bound (e.g., to variable of reference type).
*/
{
  a_boolean           found = FALSE;
  a_dynamic_init_ptr  dip = olp->destructions;

  for (; dip != NULL; dip = dip->next_in_destruction_list) {
    if (dip->has_temporary_lifetime) {
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* any_destruction_has_temp_lifetime */


static a_boolean has_child_with_temporary_lifetime(an_object_lifetime_ptr olp)
/*
Go through the children of olp and return TRUE if any has a destruction
marked as having "temporary lifetime".
*/
{
  a_boolean               found = FALSE;
  an_object_lifetime_ptr  child = olp->child_lifetime;

  for (; child != NULL; child = child->next) {
    if (any_destruction_has_temp_lifetime(child)) {
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* has_child_with_temporary_lifetime */


a_boolean is_useless_object_lifetime(an_object_lifetime_ptr  olp)
/*
Return TRUE if the object lifetime entry pointed to by olp is "useless" --
that is, there is no justification for its remaining in the IL.  For
the most part, it is useless if it has no dynamic initializations associated
with it.  Entries associated with scopes must also have no child entries.
*/
{
  a_boolean    is_useless = FALSE;
  a_boolean    do_child_check;

  if (olp->destructions != NULL) {
    /* is_useless = FALSE. */
  } else {
    switch (olp->kind) {
      case olk_global_static:
        /* The file scope object lifetime is preserved if it has any "implicit
           children" -- i.e., any function scope object lifetimes that are not
           useless; the latter point to the file scope as parent_lifetime.
           Explicit children also make the lifetime useful. */
        if (olp->child_lifetime == NULL &&
            !any_function_scope_lifetime_entries) is_useless = TRUE;
        break;
      case olk_block:
        do_child_check = TRUE;
        if (olp->entity.kind == (a_byte_il_entry_kind)iek_scope) {
          a_scope_ptr  sp = (a_scope_ptr)olp->entity.ptr;
          if (sp->kind == (a_scope_kind)sck_block &&
              sp->variant.assoc_handler != NULL) {
            /* The lifetime associated with a catch clause is retained in
               the IL even if it has no destructions and no children. */
            do_child_check = FALSE;
#if DO_IL_LOWERING
#if NEW_CAN_BE_FOLDED_INTO_CTOR
          } else if (exceptions_enabled &&
                     sp->kind == (a_scope_kind)sck_function &&
                     sp->variant.routine.ptr->special_kind ==
                                    (a_special_function_kind)sfk_constructor &&
                     sp->variant.routine.ptr->
                              source_corresp.parent.class_type->
                                variant.class_struct_union.extra_info->
                                  assoc_operator_new_routine != NULL) {
            /* When exceptions are enabled, a constructor with the allocation
               folded in needs an object lifetime so an entry for the
               deletion of the storage can be added. */
            do_child_check = FALSE;
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#endif /* DO_IL_LOWERING */
          } else if (olp->child_lifetime == NULL) {
            /* No children, no destructions. */
            is_useless = TRUE;
            do_child_check = FALSE;
          } else if (sp->kind == (a_scope_kind)sck_function) {
            /* A function lifetime with children is retained in the IL even
               if it has no destructions. */
            do_child_check = FALSE;
          }  /* if */
        } else if (exceptions_enabled &&
                   olp->entity.kind ==
                        (a_byte_il_entry_kind)iek_local_static_variable_init) {
          /* A lifetime that surrounds the initialization of a local static
             variable is kept when exceptions are enabled, because it
             delimits the region within which the initialization must be
             undone and set up to restart if an exception is thrown during
             the initialization. */
          do_child_check = FALSE;
        }  /* if */
        if (do_child_check) {
          /* A block lifetime bound to a scope but not covered by the
             preceding tests, or else a block lifetime bound to a block
             statement (the cfront dependent statement case), or else an
             unbound object lifetime. */
          if (olp->has_block_after_label_child_lifetime) {
            /* A lifetime is kept in the IL even if it has no destructions of
               its own if it has a block-after-label child lifetime. */
          } else if (has_child_with_temporary_lifetime(olp)) {
            /* A block lifetime is kept in the IL if it has a child lifetime
               that represents an expression temporary.  This is to keep
               such a temporary from "floating up" the lifetime tree because
               its parent was deemed useless. */
          } else {
            is_useless = TRUE;
          }  /* if */
        }  /* if */
        break;
      case olk_block_after_label:
        if (has_child_with_temporary_lifetime(olp)) {
          /* Do not remove a block-after-label lifetime if it has any children
             that have temporary lifetimes. */
        } else if (long_lifetime_temps &&
                   any_destruction_has_temp_lifetime(olp->parent_lifetime)) {
          /* If temps have long lifetimes, the label is the point at which
             those temps have to be destroyed.  Preserve the current lifetime
             in that case, too. */
        } else {
          is_useless = TRUE;
        }  /* if */
        break;
      case olk_expr_temporary:
        is_useless = TRUE;
        break;
      case olk_try_block:
        /* The lifetime associated with a try block is retained in the IL
           even if it has no destructions and no children. */
        break;
#if CHECKING
      case olk_function_static:
        /* Should not have been created unless there were destructions. */
      default:
        unexpected_condition_str2("is_useless_object_lifetime:",
                                  "bad object lifetime kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return is_useless;
}  /* is_useless_object_lifetime */


void remove_from_destruction_list(a_dynamic_init_ptr  dip)
/*
If the specified dynamic init entry is associated with an object lifetime,
unlink it from the latter's destructions list, and clear the pointer in
the dynamic init entry to the object lifetime.
*/
{
  an_object_lifetime_ptr  olp = dip->lifetime, colp;
  a_dynamic_init_ptr      prev;

  if (olp != NULL) {
    /* There is an associated object lifetime. */
    if (olp->destructions == dip) {
      /* dip is the head of the destructions list. */
      olp->destructions = dip->next_in_destruction_list;
      if (olp->destructions == NULL) {
        /* The destructions list is now empty.  If there is an initialization
           in an enclosing lifetime whose initialization overlapped the
           lifetime of the inner lifetime temporaries, it no longer does.
           Clear the flag that indicates that. */
        a_dynamic_init_ptr outer_dip = olp->parent_destruction_sublist;
        if (outer_dip != NULL) {
          if (outer_dip->overlaps_temps_in_inner_lifetime &&
              outer_dip->init_expr_lifetime == olp) {
            outer_dip->overlaps_temps_in_inner_lifetime = FALSE;
            olp->parent_destruction_sublist =
                                           outer_dip->next_in_destruction_list;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* It's not the head; link around it once the previous entry in the
         list has been located. */
      prev = olp->destructions;
      for (;;) {
        check_assertion_str2(prev != NULL, "remove_from_destruction_list:",
                             "dynamic init not on list of assoc lifetime");
        if (prev->next_in_destruction_list == dip) {
          prev->next_in_destruction_list = dip->next_in_destruction_list;
          break;
        }  /* if */
        prev = prev->next_in_destruction_list;
      }  /* for */
    }  /* if */
    /* If any child lifetime has its parent_destruction_sublist pointing
       to the removed dynamic init, change the pointer to the next entry
       on the destruction list. */
    for (colp = olp->child_lifetime; colp != NULL; colp = colp->next) {
      if (colp->parent_destruction_sublist == dip) {
        colp->parent_destruction_sublist = dip->next_in_destruction_list;
      }  /* if */
    }  /* for */
    dip->next_in_destruction_list = NULL;
    /* Clear the lifetime pointer in the dynamic init entry.  Note: it should
       be set if and only if it is on the list of the entry pointed to. */
    dip->lifetime = NULL;
  }  /* if */
}  /* remove_from_destruction_list */


void mark_object_lifetime_as_useless(an_object_lifetime_ptr  olp)
/*
Mark the indicated object lifetime entry as being useless, i.e., as one that
can be discarded when popped off the object lifetime stack.  This is needed
when errors occur, to avoid complaints at the time of popping that an object
lifetime containing destructions has not been bound to anything.  The object
lifetime is marked as useless by removing all the destructions on its
list.
*/
{
#if CHECKING
  if ((olp->entity.kind == (a_byte_il_entry_kind)iek_scope &&
       (((a_scope_ptr)olp->entity.ptr)->kind != (a_scope_kind)sck_block ||
        ((a_scope_ptr)olp->entity.ptr)->variant.assoc_handler != NULL)) ||
      olp->entity.kind == (a_byte_il_entry_kind)iek_try_supplement) {
    /* Cannot be made useless. */
    internal_error("mark_object_lifetime_as_useless: bad entity kind");
  }  /* if */
#endif /* CHECKING */
  /* Clear the destructions pointer by removing the entries and resetting
     their own pointers properly. */
  while (olp->destructions != NULL) {
    check_assertion(olp->destructions->lifetime == olp);
    remove_from_destruction_list(olp->destructions);
  }  /* while */
}  /* mark_object_lifetime_as_useless */


a_boolean pop_object_lifetime(void)
/*
Pop an object lifetime off the object lifetimes stack.  Check whether it
needs to be kept in the IL tree.  If not, unlink it from the IL and
return it to the appropriate available list.  Return TRUE if the object
lifetime is retained in the IL tree.
*/
{
  a_boolean               is_implicit_child = FALSE;
  an_object_lifetime_ptr  olp, parent;
  a_boolean               is_retained_in_il;

  db_enter(3, "pop_object_lifetime");
#if DEBUG
  if (debug_level >= 3) {
    fputs("curr_object_lifetime = ", f_debug);
    db_object_lifetime(curr_object_lifetime);
  }  /* if */
#endif /* DEBUG */
  olp = curr_object_lifetime;
  /* Pop the lifetime entry -- that is, update curr_object_lifetime to
     point to its parent entry. */
  curr_object_lifetime = olp->parent_lifetime;
  /* Do additional processing connected with whether the entry remains in
     the IL or should be removed. */
  if (olp->kind == (an_object_lifetime_kind)olk_block &&
      olp->entity.kind == (a_byte_il_entry_kind)iek_scope &&
      ((a_scope_ptr)olp->entity.ptr)->kind == (a_scope_kind)sck_function) {
    /* This is an object lifetime for a function scope; its parent pointer
       is the file scope lifetime entry, but it's an "implicit" child of the
       latter -- because of a memory region incompatibility, olp doesn't
       appear explicitly on the child_lifetime list of its parent . */
    is_implicit_child = TRUE;
  }  /* if */
  /* Determine whether the entry needs to be kept in the IL at all.  If not,
     modify all related pointers and then return it to an available list. */
  if (is_useless_object_lifetime(olp)) {
#if DEBUG
    if (db_flag_is_set("dump_lifetimes")) {
      if (olp->kind != (an_object_lifetime_kind)olk_expr_temporary ||
          long_lifetime_temps) {
        db_object_lifetime_with_indentation(olp, "Discarding: ");
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    /* Unlink the object lifetime entry from its parent, children, and
       siblings. */
    parent = olp->parent_lifetime;
    /* Unless *olp is an "implicit child", the lifetime entry that's no
       longer needed should be the first entry on the parent's child list. */
    if (parent == NULL || is_implicit_child) {
      /* We must be disposing of the object lifetime entry of a file or
         function scope, so there must not be any children. */
      check_assertion(olp->child_lifetime == NULL);
    } else {
      /* Remove the current object lifetime from the parent's child-lifetime
         list.  Promote its own children, if appropriate. */
      an_object_lifetime_ptr  child, end_of_child_list, *olp_loc;

      /* Determine the position of the current object lifetime in its
         parent's object-lifetime list. */
      olp_loc = &parent->child_lifetime;
      if (parent->child_lifetime != olp) {
        /* It's not the first in the list, so find the point at which to
           link around it and at which to insert its children, if required. */
        an_object_lifetime_ptr  prev = parent->child_lifetime;
        while (prev->next != olp) {
          prev = prev->next;
          check_assertion(prev != NULL);
        }  /* while */
        olp_loc = &prev->next;
      }  /* if */
      /* Loop through all the children of olp and move them up to the parent's
         child list -- i.e., promote the children to siblings. */
      end_of_child_list = NULL;
      for (child = olp->child_lifetime; child != NULL; child = child->next) {
        child->parent_lifetime = parent;
        child->parent_destruction_sublist = olp->parent_destruction_sublist;
        end_of_child_list = child;
      }  /* for */
      /* If there is a child list, promote it to parent. */
      if (olp->child_lifetime != NULL) {
        end_of_child_list->next = olp->next;
        *olp_loc = olp->child_lifetime;
      } else {
        *olp_loc = olp->next;
      }  /* if */
      if (olp->kind == (an_object_lifetime_kind)olk_block_after_label) {
        /* We have removed a block-after-label child from the parent's list
           of children.  Reset the flag, unless another block-after-label
           has been promoted in its place. */
        if (!olp->has_block_after_label_child_lifetime) {
          parent->has_block_after_label_child_lifetime = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* *olp's former parent and children, if any, should no longer have
       pointers back to it.  Now (to be safe) remove its own pointers. */
    olp->parent_lifetime = NULL;
    olp->child_lifetime = NULL;
    olp->next = NULL;
    if (olp->entity.ptr == NULL) {
      /* It's not been bound to any IL entry. */
    } else {
      /* Unbind from the IL entry with which it is associated. */
      unbind_object_lifetime(olp);
    }  /* if */
    is_retained_in_il = FALSE;
    /* Return the entry to its available list. */
    free_object_lifetime(olp);
  } else {
    /* Be sure an object lifetime that is being left in the IL has been
       bound to some other IL entity. */
    check_assertion_str(olp->entity.ptr != NULL ||
                        olp->kind ==
                              (an_object_lifetime_kind)olk_block_after_label,
                        "pop_object_lifetime: useful lifetime is unbound");
    is_retained_in_il = TRUE;
    if (is_implicit_child) {
      /* This is an object lifetime for a function scope that will remain
         in the IL.  Set the global variable to assure that the file scope
         lifetime entry will be preserved. */
      any_function_scope_lifetime_entries = TRUE;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("dump_lifetimes")) {
      db_object_lifetime_with_indentation(olp, "Keeping: ");
    }  /* if */
#endif /* DEBUG */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) db_object_lifetime_stack();
#endif /* DEBUG */
  db_exit()
  return is_retained_in_il;
}  /* pop_object_lifetime */


an_object_lifetime_ptr innermost_block_object_lifetime(
                                               an_object_lifetime_ptr  olp)
/*
Starting with the object lifetime entry pointed to by olp, advance though
its parents and return the first entry that has a kind of olk_block or
olk_block_after_label.
*/
{
  while (olp->kind != (an_object_lifetime_kind)olk_block &&
         olp->kind != (an_object_lifetime_kind)olk_block_after_label) {
    olp = olp->parent_lifetime;
    check_assertion_str(olp != NULL,
                        "innermost_block_object_lifetime: not found");
  }  /* while */
  return olp;
}  /* innermost_block_object_lifetime */

#if DEBUG

void db_scope(a_scope_ptr sp)
/*
Write out a scope entry for debugging purposes.
*/
{
  if (sp == NULL) {
    (void)fputs("<null scope>", f_debug);
  } else {
    (void)db_scope_kind(sp->kind);
    (void)fprintf(f_debug, " scope %d", (int)sp->number);
    if (sp->kind == (a_scope_kind)sck_class_struct_union ||
        sp->kind == (a_scope_kind)sck_function ||
        sp->kind == (a_scope_kind)sck_namespace) {
      (void)fputs(" (", f_debug);
      if (sp->kind == (a_scope_kind)sck_class_struct_union) {
        db_type_name(sp->variant.assoc_type);
      } else if (sp->kind == (a_scope_kind)sck_namespace) {
        db_name(&sp->variant.assoc_namespace->source_corresp);
      } else {
        db_name(&sp->variant.routine.ptr->source_corresp);
      }  /* if */
      (void)fputc(')', f_debug);
    }  /* if */
  }  /* if */
}  /* db_scope */


static void db_type_list(a_type_ptr type_list,
                         int        indent,
                         a_boolean  do_subscopes)
/*
Write out a type list, for debugging purposes.  indent is the indentation
level.  If do_subscopes is TRUE, write out the type lists for subscopes
as well.
*/
{
  a_type_ptr type;
  int        n;

  for (type = type_list; type != NULL; type = type->next) {
    for (n = 0; n < indent; n++) fputc(' ', f_debug);
    db_abbreviated_type(type);
    (void)fprintf(f_debug, "\n");
    if (do_subscopes && is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      if (ctsp != NULL && ctsp->assoc_scope != NULL) {
        db_type_list(ctsp->assoc_scope->types, indent+2, do_subscopes);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* db_type_list */


void db_scope_type_list(a_scope_ptr scope,
                        int         indent,
                        a_boolean   do_subscopes)
/*
Write out the type list for a scope, for debugging purposes.  indent is the
indentation level.  If do_subscopes is TRUE, write out the type lists for
subscopes as well.
*/
{
  int n;

  for (n = 0; n < indent; n++) fputc(' ', f_debug);
  (void)fprintf(f_debug, "Type list for ");
  db_scope(scope);
  (void)fprintf(f_debug, ":\n");
  db_type_list(scope->types, indent+2, do_subscopes);
}  /* db_scope_type_list */


void db_type_lists(a_scope_ptr scope,
                   int         indent)
/*
Dump the type lists for the indicate scope and its subscopes, for debug
purposes.  indent indicates the indentation level.
*/
{
  a_namespace_ptr nsp;
  a_scope_ptr     bscope;

  db_scope_type_list(scope, indent, /*do_subscopes=*/TRUE);
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      db_type_lists(nsp->variant.assoc_scope, indent+2);
    }  /* if */
  }  /* for */
  for (bscope = scope->scopes; bscope != NULL; bscope = bscope->next) {
    db_type_lists(bscope, indent+2);
  }  /* for */
}  /* db_type_lists */

#endif /* DEBUG */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#if !STANDALONE_UTILITY_PROGRAM

void add_to_templates_list(a_template_ptr  tp,
                           a_scope_depth   scope_depth)
/*
Add the IL template entry pointed to by tp to the indicated scope.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;
  a_scope_pointers_block_ptr  pointers_block;

  ssep = &scope_stack[scope_depth];
#if PROTOTYPE_INSTANTIATIONS_IN_IL
  if (ssep->in_prototype_instantiation && !prototype_instantiations_in_il) {
    /* We are not going to record the indicated scope (because it belongs to
       a prototype instantiation), but the given template must go on a list
       somewhere because assoc_template fields may point to it.
       Add it to the file scope (arbitrary). */
    ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
  }  /* if */
#endif  /* PROTOTYPE_INSTANTIATIONS_IN_IL */
  sp = ensure_il_scope_exists(ssep);
  check_assertion_str(sp != NULL, "add_to_templates_list: NULL IL scope");
  pointers_block = assoc_pointers_block_of(ssep);
  if (sp->templates == NULL) {
    sp->templates = tp;
  } else {
    pointers_block->last_template->next = tp;
  }  /* if */
  pointers_block->last_template = tp;
  tp->next = NULL;
}  /* add_to_templates_list */

#if RECORD_MACROS_IN_IL

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
 
void clear_function_body(a_routine_ptr  rp)
/*
rp points to a routine whose definition is being eliminated.  Reset the entry
to an undefined state and free the associated memory region.
*/
{
  a_memory_region_number  n = rp->assoc_scope;

  /* Reset the routine entry to undefined state.
     (Note that the corresponding symbol remains marked as "defined" so that
      duplicate definitions can be caught.) */
  rp->defined = FALSE;
  rp->defined_in_friend_decl = FALSE;
  rp->assoc_scope = NULL_region_number;
  (skip_typerefs(rp->type))->variant.routine.extra_info->assoc_routine = NULL;
  if (rp->storage_class == (a_storage_class)sc_unspecified) {
    rp->storage_class = (a_storage_class)sc_extern;
  }  /* if */
#if DO_IL_LOWERING && MINIMAL_INLINING
  /* If IL lowering was done and these flags are set, clear them to avoid
     problems later. */
  rp->inlinable = FALSE;
  rp->need_out_of_line_copy = FALSE;
#endif /* DO_IL_LOWERING && MINIMAL_INLINING */
  /* Free the memory region. */
  free_memory_region(n);
}  /* clear_function_body */

#if MAINTAIN_NEEDED_FLAGS

static void eliminate_references_from_befriended_entities(
                                                     a_type_ptr  class_type)
/*
class_type is a class whose definition is being eliminated or that is being
removed from the IL altogether.  In either case, if it has any friend
declarations (classes or functions), those entities will have pointers back
to class_type.  Those back-pointers should be cleared.  In the process, the
friend_classes and friend_routines pointers in class_type will also be
cleared.
*/
{
  a_class_type_supplement_ptr  ctsp, friend_ctsp;
  a_type_ptr                   friend_class;
  a_routine_ptr                friend_rout;
  a_class_list_entry_ptr       clep, prev_clep, next_clep;

  db_enter(4, "eliminate_references_from_befriended_entities");
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* Go through the befriended class. */
  while (ctsp->friend_classes != NULL) {
    friend_class = ctsp->friend_classes->class_type;
    friend_ctsp = friend_class->variant.class_struct_union.extra_info;
    if (friend_ctsp == NULL) {
      /* This class must have been removed from the IL already (the extra_info
         pointer is cleared when that happens). */
      check_assertion(!il_entry_prefix_of(friend_class).keep_in_il);
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_elim")) {
        fputs("  Befriended ", f_debug);
        db_abbreviated_type(friend_class);
        fputs(" is already eliminated", f_debug);
        fputc('\n', f_debug);
      }  /* if */
#endif /* DEBUG */
    } else {
      /* Go through the list of classes that have specified friend_class
         as a friend, find the entry that matches class_type, and link
         around it. */
      prev_clep = NULL;
      clep = friend_ctsp->befriending_classes;
      for (; clep != NULL; clep = next_clep) {
        next_clep = clep->next;
        if (clep->class_type == class_type) {
#if DEBUG
          if (debug_level >= 4 || db_flag_is_set("dump_elim")) {
            fputs("  ", f_debug);
            db_type_name(friend_class);
            fputs(" no longer befriended by ", f_debug);
            db_type_name(class_type);
            fputc('\n', f_debug);
          }  /* if */
#endif /* DEBUG */
          /* A match -- link around it. */
          if (prev_clep == NULL) {
            friend_ctsp->befriending_classes = next_clep;
          } else {
            prev_clep->next = next_clep;
          }  /* if */
          /* Break out of the inner loop and continue the outer loop,
             moving to the next class declared as a friend of class_type. */
          break;
        }  /* if */
        /* No match -- keep looping. */
        prev_clep = clep;
      }  /* for */
#if CHECKING
      if (clep == NULL) {
#if DEBUG
        fprintf(f_debug, "class type: ");
        db_abbreviated_type(class_type);
        fprintf(f_debug, "\nfriend class: ");
        db_abbreviated_type(friend_class);
        fprintf(f_debug, "\n");
#endif /* DEBUG */
        unexpected_condition_str2(
               "eliminate_references_from_befriended_entities",
               "class not found among befriending_classes of friend class");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
    /* Check the next friend class. */
    ctsp->friend_classes = ctsp->friend_classes->next;
  }  /* while */
  /* Now go through the befriended routines. */
  while (ctsp->friend_routines != NULL) {
    friend_rout = ctsp->friend_routines->routine;
    /* Go through the list of classes that have specified friend_rout as a
       friend, find the entry that matches class_type, and link around it. */
    prev_clep = NULL;
    clep = friend_rout->befriending_classes;
    for (; clep != NULL; clep = next_clep) {
      next_clep = clep->next;
      if (clep->class_type == class_type) {
        /* A match -- link around it. */
#if DEBUG
          if (debug_level >= 4 || db_flag_is_set("dump_elim")) {
            fputs("  Routine ", f_debug);
            db_name(&friend_rout->source_corresp);
            fputs(" no longer befriended by ", f_debug);
            db_type_name(class_type);
            fputc('\n', f_debug);
          }  /* if */
#endif /* DEBUG */
        if (prev_clep == NULL) {
          friend_rout->befriending_classes = next_clep;
        } else {
          prev_clep->next = next_clep;
        }  /* if */
        /* Break out of the inner loop and continue the outer loop, moving
           to the next routine declared as a friend of class_type. */
        break;
      }  /* if */
      /* No match -- keep looping. */
      prev_clep = clep;
    }  /* for */
#if CHECKING
    if (clep == NULL) {
#if DEBUG
      fprintf(f_debug, "class type: ");
      db_abbreviated_type(class_type);
      fprintf(f_debug, "\nfriend rout: ");
      db_name(&friend_rout->source_corresp);
      fprintf(f_debug, "\n");
#endif /* DEBUG */
        unexpected_condition_str2(
               "eliminate_references_from_befriended_entities",
               "class not found among befriending_classes of friend routine");
    }  /* if */
#endif /* CHECKING */
    /* Check the next friend function. */
    ctsp->friend_routines = ctsp->friend_routines->next;
  }  /* while */
  db_exit();
}  /* eliminate_references_from_befriended_entities */


static void unlink_from_child_lifetime_list(an_object_lifetime_ptr  olp)
/*
Unlink the object lifetime entry pointed to by olp from the child-lifetime
list of its parent.
*/
{
  an_object_lifetime_ptr  parent, child, prev_child;

  parent = olp->parent_lifetime;
  check_assertion(parent != NULL);
  prev_child = NULL;
  child = parent->child_lifetime;
  while (child != olp) {
    check_assertion(child != NULL);
    prev_child = child;
    child = child->next;
  }  /* while */
  if (prev_child == NULL) {
    parent->child_lifetime = olp->next;
  } else {
    prev_child->next = olp->next;
  }  /* if */
}  /* unlink_from_child_lifetime_list */


void eliminate_default_arg_object_lifetimes(a_type_ptr  rout_type)
/*
A routine type is being eliminated from the IL.  Be sure that any object
lifetimes created for its default arguments have been removed, too.
*/
{
  a_param_type_ptr        ptp;
  an_expr_node_ptr        def_arg_expr;
  an_object_lifetime_ptr  olp;

  if (rout_type != NULL) {
    rout_type = skip_typerefs(rout_type);
    for (ptp = rout_type->variant.routine.extra_info->param_type_list;
         ptp != NULL;
         ptp = ptp->next) {
      def_arg_expr = ptp->default_arg_expr;
      if (def_arg_expr != NULL &&
          def_arg_expr->kind == (an_expr_node_kind)enk_object_lifetime) {
        olp = def_arg_expr->variant.object_lifetime.ptr;
        check_assertion(olp != NULL);
        unlink_from_child_lifetime_list(olp);
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
          fputs("Unlinking default arg object lifetime\n", f_debug);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* eliminate_default_arg_object_lifetimes */


static void eliminate_member_function_default_arg_object_lifetimes(
                                                   a_type_ptr  class_type)
/*
Either class_type itself or its definition is being eliminated.  Be sure
that any object lifetimes created for default arguments of its member
functions have been removed from the IL.
*/
{
  a_scope_ptr             sp;
  a_routine_ptr           rp;

  /* Get the scope associated with this class. */
  sp = class_type->variant.class_struct_union.extra_info->assoc_scope;
  if (sp != NULL) {
    /* Traverse its member function list. */
    for (rp = sp->routines; rp != NULL; rp = rp->next) {
      eliminate_default_arg_object_lifetimes(rp->type);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (rp->declared_type != rp->type) {
        eliminate_default_arg_object_lifetimes(rp->declared_type);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Check the default arguments on the declared-type recorded in
       secondary-decl source sequence entries, too. */
    { a_source_sequence_entry_ptr   ssep;
      a_src_seq_secondary_decl_ptr  sssdp;
      an_il_entry_kind              kind;

      /* Traverse the section of the source-sequence list associated with
         the body of the class. */
      ssep = class_type->source_corresp.source_sequence_entry;
      if (ssep != NULL) {
        ssep = ssep->next;
        for (; ssep != NULL; ssep = ssep->next) {
          check_assertion(ssep != NULL);
          kind = (an_il_entry_kind)ssep->entity.kind;
          if (kind == (an_il_entry_kind)iek_src_seq_end_of_construct) {
            if (ss_entry_ptr(ssep, a_src_seq_end_of_construct_ptr)->
                                           entity.ptr == (char *)class_type) {
              /* Last of the source sequence entries belonging to the class
                 body. */
              break;
            }  /* if */
          } else if (kind == (an_il_entry_kind)iek_src_seq_secondary_decl) {
            sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
            if ((an_il_entry_kind)sssdp->entity.kind ==
                                               (an_il_entry_kind)iek_routine &&
                sssdp->declared_type != NULL) {
              eliminate_default_arg_object_lifetimes(sssdp->declared_type);
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
}  /* eliminate_member_function_default_arg_object_lifetimes */


static void turn_class_definition_into_declaration(a_type_ptr  class_type)
/*
class_type identifies a class whose definition is not needed.  Turn the IL
entry into one representing a nondefining declaration.
*/
{
  db_enter(4, "turn_class_definition_into_declaration");
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
    fputs("Removing definition of ", f_debug);
    db_abbreviated_type(class_type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
#if CHECKING
  if (class_definition_needed_flag_is_set(class_type)) {
#if DEBUG
    fprintf(f_debug, "Class type: ");
    db_abbreviated_type(class_type);
    fprintf(f_debug, "\n");
#endif /* DEBUG */
    internal_error("turn_class_definition_into_declaration: class def needed");
  }  /* if */
#endif /* CHECKING */
  if (!C_mode()) {
    /* In C++ mode fix up the class-type-supplement and data structures
       pointed to from it. */
    a_class_type_supplement_ptr  ctsp;
    a_class_type_supplement      old_supp;

    /* If the definition of class_type included friend declarations, the
       befriended classes and routines have pointers back to class_type.
       Those pointers have to be removed. */
    eliminate_references_from_befriended_entities(class_type);
    /* Remove any object lifetimes that may be associated with default
       arguments of its member functions. */
    eliminate_member_function_default_arg_object_lifetimes(class_type);
    /* Clear the pointers in the class_type_supplement, including the
       assoc_scope pointer; however, the template arg list and the list of
       befriending classes should be preserved.
       partial_spec_template_arg_list does not need to be saved because it
       is only present for fully instantiated partial specializations. */
    ctsp = class_type->variant.class_struct_union.extra_info;
    /* Save the old supplement's contents in order to restore individual
       fields later. */
    old_supp = *ctsp;
    clear_class_type_supplement(ctsp);
    ctsp->template_arg_list = old_supp.template_arg_list;
    ctsp->befriending_classes = old_supp.befriending_classes;
#if MICROSOFT_EXTENSIONS_ALLOWED
    ctsp->orig_type_kind = old_supp.orig_type_kind;
    ctsp->uuid_string = old_supp.uuid_string;
#if DO_IL_LOWERING
    ctsp->uuid_variable = old_supp.uuid_variable;
#endif /* DO_IL_LOWERING */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Clear flags that can only be TRUE for classes with definitions. */
    class_type->variant.class_struct_union.any_const_member = FALSE;
    class_type->variant.class_struct_union.any_virtual_base_classes = FALSE;
    class_type->variant.class_struct_union.abstract = FALSE;
    class_type->variant.class_struct_union.any_virtual_functions = FALSE;
    class_type->variant.class_struct_union.any_pure_virtual_functions = FALSE;
    class_type->variant.class_struct_union.
               any_virtual_functions_including_in_base_classes = FALSE;
  }  /* if */
  /* Reset size and alignment to default values, as though this class had
     never been defined. */
  class_type->size = 0;
  class_type->alignment = 1;
  /* Similarly, the field list pointer is cleared. */
  class_type->variant.class_struct_union.field_list = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  eliminate_class_body_source_sequence_entries(class_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  class_type->variant.class_struct_union.
                       nested_class_defined_outside_of_parent = FALSE;
  db_exit();
}  /* turn_class_definition_into_declaration */


static void eliminate_unneeded_class_definitions(a_type_ptr  class_type)
/*
This routine is part of the processing that prunes the IL based on how the
keep_in_il and keep_definition_in_il flags are set on various entries.
class_type specifies a class which is to be kept in the IL but whose
definition (if there is one) may not need to be retained.  If the definition
is not needed, transform this entry to represent a non-defining declaration
of the class.
*/
{
  db_enter(4, "eliminate_unneeded_class_definitions");
  if (!C_mode()) {
    a_class_type_supplement_ptr  ctsp;

    ctsp = class_type->variant.class_struct_union.extra_info;
    if (ctsp->assoc_scope != NULL) {
      /* This is a C++ class for which a definition has been provided.  Apply
         this check on each of its nested classes.  Note that it may turn out
         that the nested class definition is eliminated even though the
         containing class definition is retained. */
      a_type_ptr  tp = ctsp->assoc_scope->types;
      for (; tp != NULL; tp = tp->next) {
        if (is_immediate_class_type(tp)) {
          eliminate_unneeded_class_definitions(tp);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Now do the transformation of the class itself, if appropriate.  Note
     that we check the class size rather than the assoc_scope, since in C
     mode there is no assoc_scope even when the class has a definition. */
  if (!class_type->variant.class_struct_union.keep_definition_in_il &&
      class_type->size > 0) {
    turn_class_definition_into_declaration(class_type);
  }  /* if */
  db_exit();
}  /* eliminate_unneeded_class_definitions */


void eliminate_bodies_of_unneeded_functions(void)
/*
Go through all the memory regions looking for those associated with routines
that are not needed.  Eliminate the body -- the IL scope entry and everything
dependent on it.  The routine entry itself is dealt with later.
*/
{
  a_memory_region_number  n;
  a_scope_ptr             sp;

  db_enter(3, "eliminate_bodies_of_unneeded_functions");
  /* Loop through the memory regions.  Skip the front end and file scope
     memory regions. */
  for (n = FILE_SCOPE_REGION_NUMBER + 1;
       n <= highest_used_region_number;
       ++n) {
    if (mem_region_table[n] == NULL) {
      /* This memory has already been freed. */
    } else {
      sp = il_header.region_scope_entry[n];
      check_assertion(sp->kind == (a_scope_kind)sck_function);
      if (!sp->variant.routine.ptr->keep_definition_in_il) {
        /* An unneeded routine definition. */
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
          fprintf(f_debug, "Removing function body for ");
          db_name(&sp->variant.routine.ptr->source_corresp);
          fputc('\n', f_debug);
        }  /* if */
#endif /* DEBUG */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        eliminate_function_body_source_sequence_entries(sp);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (vla_enabled) {
          /* Any vla-dimension entries associated with this routine will
             point to file-scope types that are flagged as having an
             associated vla-dimension.  Clear those flags. */
          a_vla_dimension_ptr  vdp;

          for (vdp = sp->vla_dimensions; vdp != NULL; vdp = vdp->next) {
            vdp->type->variant.array.has_assoc_vla_dimension = FALSE;
          }  /* for */
        }  /* if */
        /* Reset the routine entry to undefined state. */
        clear_function_body(sp->variant.routine.ptr);
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* eliminate_bodies_of_unneeded_functions */

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

static void eliminate_unneeded_scope_orphaned_list_entries(void)
/*
Remove scope-orphaned-list headers that are associated with routines whose
bodies have been eliminated.
*/
{
  a_scope_orphaned_list_header_ptr  solhp, prev_solhp, next_solhp;
  a_routine_ptr    rp;
  a_variable_ptr   vp, prev_vp, next_vp;
  a_type_ptr       tp, prev_tp, next_tp;

  prev_solhp = NULL;
  for (solhp = il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = next_solhp) {
    next_solhp = solhp->next;
    rp = solhp->assoc_routine;
    if (rp->defined) {
      /* The "defined" flag has not been reset to FALSE so the body of this
         routine has not been eliminated. */
      prev_solhp = solhp;
    } else {
      /* This one has.  First traverse the variables list.  If any
         variables on the orphaned list are marked "keep_in_il", the
         orphaned-list header itself has to be kept, too. */
      prev_vp = NULL;
      for (vp = solhp->orphaned_variables; vp != NULL; vp = next_vp) {
        next_vp = vp->next;
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
          fprintf(f_debug, "%semoving orphaned variable ",
                  il_entry_prefix_of(vp).keep_in_il ? "Not r" : "R");
          db_name(&vp->source_corresp);
          fputc('\n', f_debug);
        }  /* if */
#endif /* DEBUG */
        if (!il_entry_prefix_of(vp).keep_in_il) {
          /* Remove it from the variables list by linking around it. */
          if (prev_vp == NULL) {
            solhp->orphaned_variables = vp->next;
          } else {
            prev_vp->next = vp->next;
          }  /* if */
          vp->next = NULL;
        } else {
          prev_vp = vp;
        }  /* if */
      }  /* for */
      /* Traverse the types list. */
      prev_tp = NULL;
      for (tp = solhp->orphaned_types; tp != NULL; tp = next_tp) {
        next_tp = tp->next;
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
          fprintf(f_debug, "%semoving orphaned type ",
                  il_entry_prefix_of(tp).keep_in_il ? "Not r" : "R");
          db_abbreviated_type(tp);
          fputc('\n', f_debug);
        }  /* if */
#endif /* DEBUG */
        if (!il_entry_prefix_of(tp).keep_in_il) {
          /* Remove it from the types list by linking around it. */
          if (prev_tp == NULL) {
            solhp->orphaned_types = tp->next;
          } else {
            prev_tp->next = tp->next;
          }  /* if */
          tp->next = NULL;
        } else {
          prev_tp = tp;
        }  /* if */
      }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (solhp->orphaned_src_seq_sublists != NULL) {
        /* Whether or not the header itself remains in the IL, the
           source sequence information associated with the function body
           is unneeded.  First go through all the source sequence entries
           and clear out pointers from IL entries back to them. */
        a_source_sequence_entry_ptr  ssep;
        a_src_seq_sublist_ptr        sublist;
        a_source_correspondence      *scp;

        for (sublist = solhp->orphaned_src_seq_sublists;
             sublist != NULL;
             sublist = sublist->next) {
          for (ssep = sublist->source_sequence_list;
               ssep != NULL;
               ssep = ssep->next) {
            scp = source_corresp_for_il_entry(ssep->entity.ptr,
                                              (an_il_entry_kind)ssep->
                                                            entity.kind);
            if (scp != NULL) {
              check_assertion(scp->source_sequence_entry == ssep ||
                              scp->source_sequence_entry == NULL);
              scp->source_sequence_entry = NULL;
            }  /* if */
          }  /* for */
        }  /* for */
        /* Now throw away the list. */
        solhp->orphaned_src_seq_sublists = NULL;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Only retain scope-orphaned-list headers for which non-NULL lists
         remain. */
      if (solhp->orphaned_variables != NULL ||
          solhp->orphaned_types != NULL) {
        prev_solhp = solhp;
        /* The scope-orphaned-list header is being retained in the IL, and
           it points to the routine, so be sure the routine entry is kept,
           too. */
        if (!il_entry_prefix_of(rp).keep_in_il) {
          mark_to_keep_in_il((char *)rp, (an_il_entry_kind)iek_routine);
        }  /* if */
      } else {
        /* Unlink it. */
        if (prev_solhp == NULL) {
          il_header.scope_orphaned_list_headers = next_solhp;
        } else {
          prev_solhp->next = next_solhp;
        }  /* if */
        solhp->next = NULL;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* eliminate_unneeded_scope_orphaned_list_entries */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

void eliminate_unneeded_il_entries(a_scope_ptr scope)
/*
Remove selected IL entries from the IL tree.  scope is the file scope or
a namespace scope; this processing is not done for function or block scopes.
Removal is based on how the keep_in_il flag is set for variables, routines,
and types.  Hidden-name entries and source sequence entries are also
eliminated, if appropriate.
*/
{
  a_namespace_ptr  nsp;
  a_variable_ptr   vp, prev_vp, next_vp;
  a_type_ptr       tp, prev_tp, next_tp;
  a_routine_ptr    rp, prev_rp, next_rp;
  a_boolean        in_scope_stack = (scope->depth_in_scope_stack !=
                                     NO_SCOPE_DEPTH);

  db_enter(3, "eliminate_unneeded_il_entries");
  /* In C++ process the entities on lists belonging to namespaces defined
     within the current scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Nested namespace scope. */
      eliminate_unneeded_il_entries(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Go through the list of variables that were declared in the current
     scope, removing any for which the keep_in_il flag is FALSE. */
  prev_vp = NULL;
  for (vp = scope->variables; vp != NULL; vp = next_vp) {
    next_vp = vp->next;
#if DEBUG
    if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
      fprintf(f_debug, "%semoving variable ",
              il_entry_prefix_of(vp).keep_in_il ? "Not r" : "R");
      db_name(&vp->source_corresp);
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    if (!il_entry_prefix_of(vp).keep_in_il) {
      /* Remove it from the variables list by linking around it. */
      if (prev_vp == NULL) {
        scope->variables = vp->next;
      } else {
        prev_vp->next = vp->next;
      }  /* if */
      vp->next = NULL;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
      /* If the instantiation_required flag was set, clear it now. */
      if (vp->is_template_static_data_member && !vp->is_specialized) {
        a_symbol_ptr             sym;
        a_template_instance_ptr  tip;

        sym = (a_symbol_ptr)vp->source_corresp.assoc_info;
        if (sym != NULL) {
          tip = sym->variant.static_data_member.instance_ptr;
          check_assertion(tip != NULL);
          tip->instantiation_required = FALSE;
        }  /* if */
      }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
    } else {
      prev_vp = vp;
    }  /* if */
  }  /* for */
  if (in_scope_stack) {
    /* The scope is on the scope stack, so update the "last" pointer. */
    assoc_pointers_block_of(&scope_stack[scope->depth_in_scope_stack])->
                                                       last_variable = prev_vp;
  }  /* if */
  prev_tp = NULL;
  for (tp = scope->types; tp != NULL; tp = next_tp) {
    next_tp = tp->next;
#if DEBUG
    if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
      fprintf(f_debug, "%semoving ",
              il_entry_prefix_of(tp).keep_in_il ? "Not r" : "R");
      if (has_name(tp)) {
        db_type_name(tp);
      } else {
        db_abbreviated_type(tp);
      }  /* if */
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    if (!il_entry_prefix_of(tp).keep_in_il) {
      /* Remove it from the types list by linking around it. */
      if (prev_tp == NULL) {
        scope->types = tp->next;
      } else {
        prev_tp->next = tp->next;
      }  /* if */
      tp->next = NULL;
      if (is_immediate_class_type(tp)) {
        if (!C_mode()) {
          /* If the definition of tp included friend declarations, the
             befriended classes and routines have pointers back to tp. Those
             pointers have to be removed. */
          eliminate_references_from_befriended_entities(tp);
          /* Remove any object lifetimes that may be associated with default
             arguments of its member functions. */
          eliminate_member_function_default_arg_object_lifetimes(tp);
        }  /* if */
        /* This is a class type that has been removed from the IL (because
           it's not really needed anywhere), but just in case there's a
           reference to it somewhere that causes it to be written, clear its
           pointers so they can't be walked. */
        tp->variant.class_struct_union.field_list = NULL;
        tp->variant.class_struct_union.extra_info = NULL;
      }  /* if */
    } else {
      if (is_immediate_class_type(tp)) {
        eliminate_unneeded_class_definitions(tp);
      }  /* if */
      prev_tp = tp;
    }  /* if */
  }  /* for */
  if (in_scope_stack) {
    /* The scope is on the scope stack, so update the "last" pointer. */
    assoc_pointers_block_of(&scope_stack[scope->depth_in_scope_stack])->
                                                           last_type = prev_tp;
  }  /* if */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  if (scope->kind == (a_scope_kind)sck_file) {
    eliminate_unneeded_scope_orphaned_list_entries();
  }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  prev_rp = NULL;
  for (rp = scope->routines; rp != NULL; rp = next_rp) {
    next_rp = rp->next;
#if DEBUG
    if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
      fprintf(f_debug, "%semoving routine ",
              il_entry_prefix_of(rp).keep_in_il ? "Not r" : "R");
      db_name(&rp->source_corresp);
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    if (!il_entry_prefix_of(rp).keep_in_il) {
      if (!C_mode()) {
        /* Remove any object lifetimes that may be associated with its default
           arguments. */
        eliminate_default_arg_object_lifetimes(rp->type);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (rp->declared_type != rp->type) {
          eliminate_default_arg_object_lifetimes(rp->declared_type);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
      /* Remove it from the routines list by linking around it. */
      if (prev_rp == NULL) {
        scope->routines = rp->next;
      } else {
        prev_rp->next = rp->next;
      }  /* if */
      rp->next = NULL;
      /* If the instantiation_required flag was set, clear it now.  Or,
         if the function is extern inline and we are instantiating extern
         inline functions using a mechanism like the template instantiation
         mechanism, clear the inline instance required flag. */
      if ((rp->is_template_function && !rp->is_specialized) ||
          (instantiate_extern_inline && rp->is_inline)) {
        a_symbol_ptr             sym;
        a_boolean                okay_to_clear_flag = TRUE;

        sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
        if (sym != NULL) {
          if (rp->is_virtual) {
#if DO_IL_LOWERING
            a_type_ptr                   class_type = sym->parent.class_type;
            a_class_type_supplement_ptr  ctsp;
            a_variable_ptr               vtbl_var, typeinfo_var;

            ctsp = class_type->variant.class_struct_union.extra_info;
            /* Note: the class-type supplement will be NULL if the class
               body has been eliminated. */
            if (ctsp != NULL &&
                ((vtbl_var = ctsp->virtual_function_table_var) == NULL ||
                  !il_entry_prefix_of(vtbl_var).keep_in_il)
#if ABI_CHANGES_FOR_RTTI
                                                            &&
                ((typeinfo_var = class_type->typeinfo_var) == NULL ||
                  !il_entry_prefix_of(typeinfo_var).keep_in_il)
#endif /* ABI_CHANGES_FOR_RTTI */
                                                               ) {
              /* Either there is no virtual function table or it's been
                 eliminated from the IL: it's okay to clear the flag, since
                 an otherwise unreferenced virtual function would be needed
                 only if the virtual function table is defined in this
                 translation unit.  If the typeinfo variable is kept,
                 keep the virtual function so the virtual function table
                 will be kept so that the typeinfo variable will be kept. */
            } else
#endif /* DO_IL_LOWERING */
            /* Do not insert code here. */
            {
              /* Virtual function may be needed for defining the virtual
                 function table. */
              okay_to_clear_flag = FALSE;
            }  /* if */
          }  /* if */
          if (okay_to_clear_flag) {
            set_instance_required(sym, FALSE, /*defer_inline=*/FALSE);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      prev_rp = rp;
    }  /* if */
  }  /* for */
  if (in_scope_stack) {
    /* The scope is on the scope stack, so update the "last" pointer. */
    assoc_pointers_block_of(&scope_stack[scope->depth_in_scope_stack])->
                                                        last_routine = prev_rp;
  }  /* if */
  if (il_header.main_routine != NULL &&
      !il_entry_prefix_of(il_header.main_routine).keep_in_il) {
    /* The routine entry itself will already have been removed from the IL,
       so remove this reference to it. */
    il_header.main_routine = NULL;
  }  /* if */
#if RECORD_HIDDEN_NAMES_IN_IL
  /* Hidden name table entries need not be kept in the IL if they refer
     to entities that do not need to be kept. */
  {
  a_hidden_name_ptr        hnp, prev_hnp = NULL, next_hnp;

  for (hnp = scope->hidden_names; hnp != NULL; hnp = next_hnp) {
    next_hnp = hnp->next;
#if DEBUG
    if (debug_level >= 3 || db_flag_is_set("dump_elim")) {
      fprintf(f_debug, "%semoving hidden name entry for ",
              il_entry_prefix_of(hnp->entity.ptr).keep_in_il ? "Not r" : "R");
      if (hnp->entity.kind == (a_byte_il_entry_kind)iek_type) {
        db_abbreviated_type((a_type_ptr)hnp->entity.ptr);
      } else {
        db_name(source_corresp_for_il_entry(hnp->entity.ptr,
                                      (an_il_entry_kind)hnp->entity.kind));
      }  /* if */
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    if (!il_entry_prefix_of(hnp->entity.ptr).keep_in_il) {
      if (prev_hnp == NULL) {
        scope->hidden_names = next_hnp;
      } else {
        prev_hnp->next = next_hnp;
      }  /* if */
      hnp->next = NULL;
    } else {
      prev_hnp = hnp;
    }  /* if */
  }  /* for */
  }
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (scope->kind == (a_scope_kind)sck_file) {
    eliminate_unneeded_source_sequence_entries(scope);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Relink the il_header.nontag_types_used_in_exception_or_rtti list to
     eliminate removed entities. */
  prev_tp = NULL;
  for (tp = il_header.nontag_types_used_in_exception_or_rtti;
       tp != NULL;
       tp = next_tp) {
    next_tp = tp->next;
    if (!il_entry_prefix_of(tp).keep_in_il) {
      /* Remove the entry from the list by linking around it. */
      if (prev_tp == NULL) {
        il_header.nontag_types_used_in_exception_or_rtti = tp->next;
      } else {
        prev_tp->next = tp->next;
      }  /* if */
      tp->next = NULL;
    } else {
      /* Keep the entry. */
      prev_tp = tp;
    }  /* if */
  }  /* for */
  db_exit();
}  /* eliminate_unneeded_il_entries */

#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if ONE_INSTANTIATION_PER_OBJECT

unsigned long assign_instantiation_needed_bit_number(void)
/*
Increment the number of external template entities and return the bit number
to be used for the next instantiation needed bit number.
*/
{
  il_header.number_of_external_nonclass_template_entities++;
  /* When generating one instantiation per object, assign a "bit number"
     to each external nonclass entity.  Actually, two bits: the second
     is used for class definition_needed bits.  Bit numbers 1 and 2 are
     reserved for the needed and definition_needed flags for entities
     in the compilation that are not instantiations. */
  return (il_header.number_of_external_nonclass_template_entities*2) + 1;
}  /* assign_instantiation_needed_bit_number */


void clear_instantiation_needed_flags_scan_state(
                           an_instantiation_needed_flags_scan_state_ptr infssp,
                           a_source_correspondence                      *scp)
/*
Initialize an_instantiation_needed_flags_scan_state to begin scanning for
per-instantiation "needed" flags in the set associated with scp.
*/
{
  infssp->curr_segment = scp->per_instantiation_needed_flags;
  infssp->first_bit_this_segment = 1;
  infssp->byte_number = 0;
  infssp->bit_number = -1;
}  /* clear_instantiation_needed_flags_scan_state */


unsigned long next_set_instantiation_needed_flag(
                           an_instantiation_needed_flags_scan_state_ptr infssp)

/*
Return the bit number of the next set per-instantiation "needed" flag in
a bit vector.  infssp indicates the current state of the scan, and is updated
on return.  Return 0 if there is no next set bit.  Note that
"definition needed" bits are returned as well; the caller must ignore them
if appropriate.
*/
{
  unsigned long next_set_bit;
  a_per_instantiation_needed_flags_entry_ptr
                curr_segment = infssp->curr_segment;
  unsigned long first_bit_this_segment = infssp->first_bit_this_segment;
  int           byte_number = infssp->byte_number;
  int           bit_number = infssp->bit_number;

  for (;
       curr_segment != NULL;
       curr_segment = curr_segment->next,
         first_bit_this_segment +=
                          BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY*CHAR_BIT) {
    a_byte byte = curr_segment->bytes[byte_number];
    /* Clear the bits that have already been looked at, in a copy of the
       current byte. */
    if (bit_number >= 0) {
      /* Current byte has been started.  bit_number indicates the
         bit last examined. */
      if (bit_number == CHAR_BIT-1) {
        /* Nothing left in this byte. */
        byte = 0;
      } else {
        /* Mask off the bits that have been looked at already. */
        byte &= (~(a_byte)0) << (bit_number+1);
      }  /* if */
    }  /* if */
    /* Advance over bytes with no bits set.  The first time around, this
       tests the byte produced above, which has the bits that have already
       been examined turned off. */
    while (byte == 0) {
      bit_number = -1;
      if (byte_number == BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY-1) {
        byte_number = 0;
        goto next_segment;
      }  /* if */
      byte_number++;
      byte = curr_segment->bytes[byte_number];
    }  /* while */
    /* Here, we have a byte with some bits set.  Find the first of
       those. */
    for (;;) {
      bit_number++;
      if (byte & (((a_byte)1) << bit_number)) {
        /* Found the first set bit. */
        next_set_bit = first_bit_this_segment + byte_number*CHAR_BIT +
                       bit_number;
        goto end_of_routine;
      }  /* if */
    }  /* if */
next_segment:;
  }  /* for */
  /* Fell off end of list. */
  next_set_bit = 0;
end_of_routine:
  infssp->curr_segment = curr_segment;
  infssp->first_bit_this_segment = first_bit_this_segment;
  infssp->byte_number = byte_number;
  infssp->bit_number = bit_number;
  return next_set_bit;
}  /* next_set_instantiation_needed_flag */


a_boolean instantiation_needed_flag_is_set(a_source_correspondence *scp,
                                           int                     bit_offset)
/*
Fetch and return the value of the per-instantiation "needed" flag
associated with source correspondence entry *scp and with the bit
number given by global variable needed_flag_bit_number plus bit_offset.
*/
{
  a_boolean     flag_value;
  a_per_instantiation_needed_flags_entry_ptr
                ptr;
  unsigned long first_bit_this_segment;
  unsigned long byte_number;
  unsigned long bit_number = needed_flag_bit_number + bit_offset;
#define BITS_PER_ENTRY (BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY*CHAR_BIT)

  /* Loop through the list of entries to find the one containing the
     bit we want to test. */
  for (ptr = scp->per_instantiation_needed_flags,
         first_bit_this_segment = 1;
       ptr != NULL &&
         (first_bit_this_segment + BITS_PER_ENTRY) <= bit_number;
       ptr = ptr->next,
         first_bit_this_segment += BITS_PER_ENTRY) {
  }  /* for */
  if (ptr == NULL) {
    /* Ran off the end of the list.  All bits out here are presumed
       to be zero. */
    flag_value = FALSE;
  } else {
    bit_number -= first_bit_this_segment;
    byte_number = bit_number / CHAR_BIT;
    bit_number  = bit_number % CHAR_BIT;
    flag_value = (ptr->bytes[byte_number] >> bit_number) & 1;
  }  /* if */
  return flag_value;
#undef BITS_PER_ENTRY
}  /* instantiation_needed_flag_is_set */

#if !STANDALONE_UTILITY_PROGRAM

void set_instantiation_needed_flag(a_source_correspondence *scp,
                                   int                     bit_offset,
                                   int                     new_value)
/*
Set to new_value the per-instantiation "needed" flag associated with source
correspondence entry *scp and with the bit number given by global variable
needed_flag_bit_number plus bit_offset.
*/
{
  a_per_instantiation_needed_flags_entry_ptr
                ptr, prev_ptr;
  unsigned long first_bit_this_segment;
  unsigned long byte_number;
  unsigned long bit_number = needed_flag_bit_number + bit_offset;
  a_byte        bit;
#define BITS_PER_ENTRY (BYTES_PER_INSTANTIATION_NEEDED_FLAG_ENTRY*CHAR_BIT)

  /* Loop through the list of entries to find the one containing the
     bit we want to test. */
  for (prev_ptr = NULL,
         ptr = scp->per_instantiation_needed_flags,
         first_bit_this_segment = 1;
       ;
       prev_ptr = ptr,
         ptr = ptr->next,
         first_bit_this_segment += BITS_PER_ENTRY) {
    if (ptr == NULL) {
      /* Ran off the end of the list, so allocate another entry. */
      ptr = alloc_per_instantiation_needed_flags_entry(in_file_scope(scp));
      if (prev_ptr == NULL) {
        scp->per_instantiation_needed_flags = ptr;
      } else {
        prev_ptr->next = ptr;
      }  /* if */
    }  /* if */
    if ((first_bit_this_segment + BITS_PER_ENTRY) > bit_number) {
      /* This segment contains the bit we want. */
      break;
    }  /* if */
  }  /* for */
  bit_number -= first_bit_this_segment;
  byte_number = bit_number / CHAR_BIT;
  bit_number  = bit_number % CHAR_BIT;
  bit = (unsigned char)1 << bit_number;
  if (new_value != 0) {
    /* Set the bit. */
    ptr->bytes[byte_number] |= bit;
  } else {
    /* Clear the bit. */
    ptr->bytes[byte_number] &= ~bit;
  }  /* if */
#undef BITS_PER_ENTRY
  if (needed_flag_bit_number != 1 && new_value != 0 &&
      scp->name_linkage == (a_name_linkage_kind)nlk_internal) {
    /* This is a static entity that is needed from an instantiation.
       Mark the entity so that it will be made external so it can be accessed
       from the instantiation file. */
    scp->static_used_by_instantiation = TRUE;
  }  /* if */
}  /* set_instantiation_needed_flag */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if !STANDALONE_UTILITY_PROGRAM

a_namespace_ptr f_skip_namespace_aliases(a_namespace_ptr nsp)
/*
nsp points to either a namespace or a namespace alias.  If nsp is
a namespace, the pointer to that namespace is returned.  If nsp
is a namespace alias, a pointer to the real namespace is returned.
*/
{
  while (nsp->is_namespace_alias) {
    nsp = nsp->variant.assoc_namespace;
    check_assertion_str2(nsp != NULL, "f_skip_namespace_aliases:",
                         "NULL namespace pointer");
  }  /* while */
  return nsp;
}  /* f_skip_namespace_aliases */


a_boolean is_member_of_unnamed_namespace(a_source_correspondence  *scp)
/*
Return TRUE if the entity with the specified source-correspondence is a
direct or indirect member of an unnamed namespace.
*/
{
  a_boolean        found = FALSE;
  a_namespace_ptr  nsp;

  if (scp->is_class_member) {
    scp = &scp->parent.class_type->source_corresp;
    found = is_member_of_unnamed_namespace(scp);
  } else if ((nsp = scp->parent.namespace_ptr) != NULL) {
    if (unmangled_name_of(&nsp->source_corresp) == NULL) {
      found = TRUE;
    } else {
      found = is_member_of_unnamed_namespace(&nsp->source_corresp);
    }  /* if */
  }  /* if */
  return found;
}  /* is_member_of_unnamed_namespace */


#if DEBUG
unsigned long show_il_space_used(void)
/*
Display and return the amount of space used for various IL tables.
*/
{
  unsigned long grand_total = 0;

  grand_total = show_il_alloc_space_used(grand_total);

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


a_type_ptr init_predeclared_class(a_type_kind          kind,
                                  char                 *name)
/*
Create a type entry for a predeclared class of the specified kind.  It is
given the name indicated and a symbol is created, but the symbol is not
entered into the symbol table.
*/
{
  a_type_ptr  predeclared_type;

  predeclared_type = alloc_type(kind);
  /* Default name-linkage for classes is C++ external linkage. */
  predeclared_type->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_cplusplus_external;
  make_symbol_for_predeclared_type(predeclared_type, name);
  return predeclared_type;
}  /* init_predeclared_class */


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
    internal_error("il_one_time_init: bad initialization of sizeof_il_entry");
  }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT */
#if NEED_IL_DISPLAY || DEBUG
  /* Variable in il_def.h: */
  /* Check that the table of IL entry names is correctly initialized.
     This guards against someone changing the enumeration and forgetting to
     update il_entry_kind_names. */
  if (il_entry_kind_names[(int)iek_last] == NULL ||
      strcmp(il_entry_kind_names[(int)iek_last], "last") != 0) {
    internal_error(
          "il_one_time_init: incorrect initialization of il_entry_kind_names");
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
       "il_one_time_init: incorrect initialization of db_storage_class_names");
  }  /* if */
  /* Check that the table of special function kind names is correctly
     initialized. */
  if (db_special_function_kinds[(int)sfk_last] == NULL ||
      strcmp(db_special_function_kinds[(int)sfk_last], "last") != 0) {
    internal_error(
    "il_one_time_init: incorrect initialization of db_special_function_kinds");
  }  /* if */
  /* Check that the table of operator names is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to update
     db_operator_names. */
  if (db_operator_names[(int)eok_last] == NULL ||
      strcmp(db_operator_names[(int)eok_last], "last") != 0) {
    internal_error(
            "il_one_time_init: incorrect initialization of db_operator_names");
  }  /* if */
#endif /* DEBUG */
  /* Variable in il_def.h: */
  /* Check that the table of linkage kind names is correctly initialized. */
  if (name_linkage_kind_names[(int)nlk_last] == NULL ||
      strcmp(name_linkage_kind_names[(int)nlk_last], "last") != 0) {
    internal_error(
      "il_one_time_init: incorrect initialization of name_linkage_kind_names");
  }  /* if */
#if DECL_MODIFIERS_IN_USE
  /* Variable in il_def.h: */
  /* Check that the table of decl modifier names is correctly initialized. */
  if (decl_modifier_names[(int)dmt_last] == NULL ||
      strcmp(decl_modifier_names[(int)dmt_last], "last") != 0) {
    internal_error(
          "il_one_time_init: incorrect initialization of decl_modifier_names");
  }  /* if */
#endif /* DECL_MODIFIERS_IN_USE */
  /* Variable in il_def.h: */
  /* Check that the table of pragma ids is correctly initialized.  This guards
     against someone changing the enumeration a_pragma_kind and forgetting to
     update pragma_ids. */
  if (pragma_ids[(int)pk_last] == NULL ||
      strcmp(pragma_ids[(int)pk_last], "last") != 0) {
    internal_error(
                   "il_one_time_init: incorrect initialization of pragma_ids");
  }  /* if */
  /* Variable in il_def.h: */
  /* Check that unsigned_int_kind_of is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to update
     the initialization. */
  if (unsigned_int_kind_of[(int)ik_last] != 111) {
    internal_error(
         "il_one_time_init: incorrect initialization of unsigned_int_kind_of");
  }  /* if */
#endif /* CHECKING */

  /* Save variables from il.h and il.c that are needed for precompiled
     headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(type_of_type_info),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(type_of_guid),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_array_saved_var_array_elem(float_types),
      pch_saved_var_array_elem(il_error_type),
      pch_saved_var_array_elem(il_unknown_type),
      pch_saved_var_array_elem(il_void_type),
      pch_saved_var_array_elem(il_wchar_t_type),
      pch_saved_var_array_elem(il_bool_type),
      pch_array_saved_var_array_elem(int_types),
      pch_array_saved_var_array_elem(signed_int_types),
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_array_saved_var_array_elem(microsoft_sized_int_types),
      pch_array_saved_var_array_elem(microsoft_sized_signed_int_types),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
      pch_array_saved_var_array_elem(complex_types),
      pch_array_saved_var_array_elem(imaginary_types),
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      pch_array_saved_var_array_elem(string_types),
      pch_array_saved_var_array_elem(wide_string_types),
      pch_array_saved_var_array_elem(shareable_constants_table),
      pch_saved_var_array_elem(curr_object_lifetime),
      pch_saved_var_array_elem(any_function_scope_lifetime_entries),
      pch_saved_var_array_elem(based_type_fixup_list),
#if ORPHAN_PROCESSING_NEEDED
      pch_array_saved_var_array_elem(orphaned_file_scope_il_entries),
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
      pch_saved_var_array_elem(last_scope_orphaned_list_header),
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
      pch_saved_var_array_elem(last_macro),
#endif /* RECORD_MACROS_IN_IL */
      pch_saved_var_array_elem(curr_fp_contract_state),
      pch_saved_var_array_elem(curr_fenv_access_state),
      pch_saved_var_array_elem(curr_cx_limited_range_state),
#if DEBUG
      pch_saved_var_array_elem(num_compares_for_shareable_constants),
      pch_saved_var_array_elem(num_func_shareable_constants),
      pch_saved_var_array_elem(num_get_based_type_calls),
      pch_saved_var_array_elem(num_shareable_constants),
      pch_saved_var_array_elem(num_used_shareable_constant_buckets),
      pch_saved_var_array_elem(num_based_type_fixups_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  il_alloc_one_time_init();
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
  curr_object_lifetime = NULL;
  type_of_type_info = NULL;
  /* remove_unneeded_entities is the value, settable from the command line,
     to which okay_to_eliminate_unneeded_il_entries should be initialized
     with each new translation unit. */
  okay_to_eliminate_unneeded_il_entries = remove_unneeded_entities;

  /* Static variables in il.c: */
  /* Depending on NULL represented as zero bits here. */
  memzero((char *)int_types, sizeof(int_types));
  memzero((char *)signed_int_types, sizeof(signed_int_types));
#if MICROSOFT_EXTENSIONS_ALLOWED
  memzero((char *)microsoft_sized_int_types,
          sizeof(microsoft_sized_int_types));
  memzero((char *)microsoft_sized_signed_int_types,
          sizeof(microsoft_sized_signed_int_types));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  memzero((char *)float_types, sizeof(float_types));
  memzero((char *)string_types, sizeof(string_types));
  memzero((char *)wide_string_types, sizeof(wide_string_types));
  il_wchar_t_type = NULL;
  il_bool_type = NULL;
  il_error_type = il_unknown_type = il_void_type = NULL;
  memzero((char *)shareable_constants_table,
          sizeof(shareable_constants_table));
#if RECORD_MACROS_IN_IL
  last_macro = NULL;
#endif /* RECORD_MACROS_IN_IL */
  based_type_fixup_list = NULL;
  curr_fp_contract_state = (a_stdc_pragma_value)stdc_pv_default;
  curr_fenv_access_state = (a_stdc_pragma_value)stdc_pv_default;
  curr_cx_limited_range_state = (a_stdc_pragma_value)stdc_pv_default;
#if DEBUG
  num_shareable_constants                = 0;
  num_func_shareable_constants           = 0;
  num_used_shareable_constant_buckets    = 0;
  num_searches_for_shareable_constants   = 0;
  num_compares_for_shareable_constants   = 0;
  num_get_based_type_calls               = 0;
  num_based_type_fixups_allocated        = 0;
#endif /* DEBUG */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  last_scope_orphaned_list_header = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  any_function_scope_lifetime_entries = FALSE;
  il_reset();
  il_alloc_init();
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
* Copyright 1988-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
