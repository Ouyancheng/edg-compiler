/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
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
#include "symbol_tbl.h"
#include "error.h"
#include "types.h"
#include "cmd_line.h"
#include "float_pt.h"

#if ALTERNATE_IL_FILE_FORMAT
#include "il_file.h"
#endif /* ALTERNATE_IL_FILE_FORMAT */

/*
Pointers to shared types.  These are cleared by il_init.
*/
static a_type_ptr int_types[(int)ik_last];
static a_type_ptr float_types[(int)fk_last];
#define MAX_TRACKED_STRING_TYPE_LENGTH 80
static a_type_ptr string_types[MAX_TRACKED_STRING_TYPE_LENGTH+1];
static a_type_ptr il_signed_int_type;
static a_type_ptr il_error_type;
static a_type_ptr il_void_type;
static a_type_ptr il_no_type;

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_constants_allocated,
		num_param_types_allocated,
		num_routine_type_supplements_allocated,
                num_access_adjustments_allocated;
                num_class_list_entries_allocated;
		num_class_type_supplements_allocated,
                num_derivation_steps_allocated,
                num_base_classes_allocated,
		num_types_allocated,
		num_dynamic_inits_allocated,
		num_variables_allocated,
		num_fields_allocated,
		num_routines_allocated,
		num_labels_allocated,
		num_expr_nodes_allocated,
		num_switch_clauses_allocated,
		num_blocks_allocated,
		num_statements_allocated,
		num_scopes_allocated,
		string_literal_text_space_allocated;
#if ALTERNATE_IL_FILE_FORMAT
static unsigned long
		num_il_entry_numbers_allocated;
#endif /* ALTERNATE_IL_FILE_FORMAT */
/*
Counts of uses of get_based_type.
*/
static unsigned long
		num_based_type_arrays_allocated,
		num_get_based_type_calls,
		num_costly_get_based_type_calls;
#endif /* DEBUG */

/* Initial setting for il_walk_flag.  Can be (arbitrarily) either 0 or 1. */
#define INITIAL_IL_WALK_FLAG_SETTING 0

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
static a_constant_ptr
		func_shareable_constants_list;
			/* List of shared constants for the current function.
			   These are constants that refer to something local
			   to the function, and therefore cannot be shared at 
			   the file scope.  The only meaningful case is
			   a constant indicating the address of a local
			   variable. */

#if DEBUG
static unsigned long
		num_shareable_constants,
		num_func_shareable_constants,
		num_used_shareable_constant_buckets,
		num_searches_for_shareable_constants,
		num_compares_for_shareable_constants;
#endif /* DEBUG */


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
  return (p);
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
  return (p);
}  /* db_float_type_name */


static void db_name(a_source_correspondence *sc)
/*
Dump the name from a source correspondence (if any).
*/
{
  if (sc->name != NULL) {
    fputs(sc->name, f_debug);
  } else {
    fputs("(null)", f_debug);
  }  /* if */
}  /* db_name */


void db_abbreviated_type(a_type *tp)
/*
Dump a type in abbreviated form.  This is particularly required for
classes, structs, unions, which may contain fields that point to
objects of their own type.
*/
{
  if (tp == NULL) {
    fputs("null type pointer", f_debug);
  } else {
    switch (tp->kind) {
      case tk_class:
        fputs("class", f_debug);
        goto print_name;
      case tk_struct:
        fputs("struct", f_debug);
        goto print_name;
      case tk_union:
        fputs("union", f_debug);
print_name:
        if (tp->source_corresp.name != NULL) {
          fprintf(f_debug, " \"%s\"", tp->source_corresp.name);
        }  /* if */
        break;
      default:
        db_type(tp);
        break;
    }  /* switch */
  }  /* if */
}  /* db_abbreviated_type */


static void db_access_control(an_access_specifier as)
/*
Dump an access control specifier.
*/
{
  switch (as) {
    case as_public:	  fputs("public", f_debug);	  break;
    case as_protected:	  fputs("protected", f_debug);	  break;
    case as_private:	  fputs("private", f_debug);	  break;
    case as_inaccessible: fputs("inaccessible", f_debug); break;
  }  /* switch */
}  /* db_access_control */


void db_field(a_field *fp)
/*
Dump a field entry, for debug purposes.
*/
{
  fputc(' ', f_debug);
  if (C_dialect == C_dialect_cplusplus) {
    fputc(' ', f_debug);
    db_access_control(fp->source_corresp.access);
  }  /* if */
  fputs(" field \"", f_debug);
  db_name(&fp->source_corresp);
  fputs("\", type = ", f_debug);
  db_abbreviated_type(fp->type);
  fprintf(f_debug, ", bit offset %lu", fp->bit_offset);
  if (fp->bit_size > 0) {
    fprintf(f_debug, ", bit size %d", fp->bit_size);
  }  /* if */
  fputc('\n', f_debug);
}  /* db_field */


void db_static_data_member(a_variable_ptr vp)
/*
Dump a static data member (a variable entry), for debug purposes.
*/
{
  fputs("  ", f_debug);
  db_access_control(vp->source_corresp.access);
  fputs(" static data member \"", f_debug);
  db_name(&vp->source_corresp);
  fputs("\", type = ", f_debug);
  db_abbreviated_type(vp->type);
  fputc('\n', f_debug);
}  /* db_static_data_member */


void db_member_function(a_routine_ptr rp)
/*
Dump a member function (a routine entry), for debug purposes.
*/
{
  fputs("  ", f_debug);
  db_access_control(rp->source_corresp.access);
  if (!routine_type_is_nonstatic_member_function(rp->type)) {
    fputs(" static", f_debug);
  }  /* if */
  fputs(" member function \"", f_debug);
  db_name(&rp->source_corresp);
  fputs("\",\n    type = ", f_debug);
  db_abbreviated_type(rp->type);
  fputc('\n', f_debug);
}  /* db_member_function */


static void db_base_class_field(a_field *fp,
				a_type  *tp,
                                int     depth)
/*
Dump field *fp derived from base class *tp, for debug purposes.
*/
{
  int i;

  fputs("\n    ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  db_access_control(fp->source_corresp.access);
  fprintf(f_debug, " field %s::", tp->source_corresp.name);
  db_name(&fp->source_corresp);
  fputs(", type = ", f_debug);
  db_abbreviated_type(fp->type);
  fprintf(f_debug, ", bit offset %lu", fp->bit_offset);
  if (fp->bit_size > 0) {
    fprintf(f_debug, ", bit size %d", fp->bit_size);
  }  /* if */
}  /* db_base_class_field */


static void db_direct_base_class(a_base_class *bcp,
                                 int          depth)
/*
Dump a direct base class entry, for debug purposes.
*/
{
  a_type     *tp = bcp->class;
  a_field    *fp;
  int        i;

  fputs("\n  ", f_debug);
  for (i = depth; i > 0; --i) fputs("  ", f_debug);
  fputs("[[ ", f_debug);
  if (bcp->virtual) {
    fputs("virtual ", f_debug);
  }  /* if */
  db_access_control(bcp->access);
  fprintf(f_debug, " base class %s (%soffset = %lu)",
		   tp->source_corresp.name, bcp->virtual ? "pointer " : "",
                   bcp->offset);
  if (!bcp->virtual) {
    bcp = tp->variant.class_struct_union.extra_info->base_classes;
    while (bcp != NULL) {
      if (bcp->direct) db_direct_base_class(bcp, depth+1);
      bcp = bcp->next;
    }  /* while */
    fp = tp->variant.class_struct_union.field_list;
    while (fp != NULL) {
      db_base_class_field(fp, tp, depth);
      fp = fp->next;
    }  /* while */
  }  /* if */
  fputs(" ]]", f_debug);
}  /* db_base_class */


static void db_virtual_base_class(a_base_class *bcp)
/*
Dump a virtual base class entry, for debug purposes.
*/
{
  a_type       *tp = bcp->class;
  a_field      *fp;
  
  fprintf(f_debug, "  [( virtual base class %s (offset = %lu)",
		   tp->source_corresp.name, bcp->offset);
  bcp = tp->variant.class_struct_union.extra_info->base_classes;
  while (bcp != NULL) {
    if (bcp->direct) db_direct_base_class(bcp, /*nesting_depth=*/1);
    bcp = bcp->next;
  }  /* while */
  fp = tp->variant.class_struct_union.field_list;
  while (fp != NULL) {
    db_base_class_field(fp, tp, /*nesting_depth=*/0);
    fp = fp->next;
  }  /* while */
  fputs(" )]\n", f_debug);
}  /* db_virtual_base_class */


static void db_access_adjustment(an_access_adjustment_ptr aap)
/*
*/
{
  a_source_correspondence  *sc;
  a_type_ptr               class;
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
  }  /* switch */
  fputs("    ", f_debug);
  db_access_control(aap->access);
  fputs(" \"", f_debug);
  db_name(sc);
  fprintf(f_debug, "\" = %s ", str);
  class = ((a_symbol_ptr)(sc->assoc_info))->class_of_which_a_member;
  db_name(&class->source_corresp);
  fputs("::", f_debug);
  db_name(sc);
  fputc('\n', f_debug);
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
    case tk_none:
      fputs("<no type>", f_debug);
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
      fputs("ptr(", f_debug);
      goto pointer_or_reference;
    case tk_reference:
      fputs("ref(", f_debug);
pointer_or_reference:
      /* Dump classes/structs/unions specially to avoid recursive loops
         when then contain pointers to themselves. */
      db_abbreviated_type(tp->variant.pointer_type_pointed_to);
      fputc(')', f_debug);
      break;
    case tk_array:
      fputc('(', f_debug);
      db_type(tp->variant.array.element_type);
      fprintf(f_debug, ")[%lu]", tp->variant.array.number_of_elements);
      break;
    case tk_struct:
      fputs("struct {", f_debug);
      goto class_struct_union;
    case tk_union:
      fputs("union {", f_debug);
      goto class_struct_union;
    case tk_class:
      fputs("class {", f_debug);
class_struct_union:
      {
        a_base_class_ptr  bcp = NULL;
        a_boolean         any_virtual_base_classes = FALSE;

        ctsp = tp->variant.class_struct_union.extra_info;
        if (ctsp != NULL) bcp = ctsp->base_classes;
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct) db_direct_base_class(bcp, 0);
          if (bcp->virtual) any_virtual_base_classes = TRUE;
        } /* for */
        fputc('\n', f_debug);
        fp = tp->variant.class_struct_union.field_list;
        for (; fp != NULL; fp = fp->next) db_field(fp);
        if (ctsp != NULL) {
          a_variable_ptr           vp = ctsp->assoc_scope->variables;
          a_routine_ptr            rp = ctsp->assoc_scope->routines;
          an_access_adjustment_ptr aap = ctsp->access_adjustments;

          if (any_virtual_base_classes) {
            fputs("  collected virtual base classes:\n", f_debug);
            for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
              if (bcp->virtual) db_virtual_base_class(bcp);
            }  /* for */
          }  /* if */
          if (vp != NULL) {
            fputs("  static data members:\n", f_debug);
            for (; vp != NULL; vp = vp->next) db_static_data_member(vp);
          }  /* if */
          if (rp != NULL) {
            fputs("  member functions:\n", f_debug);
            for (; rp != NULL; rp = rp->next) db_member_function(rp);
          }  /* if */
          if (aap != NULL) {
            fputs("  access adjustments:\n", f_debug);
            for (; aap != NULL; aap = aap->next) db_access_adjustment(aap);
          }  /* if */
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
      fputs("routine ", f_debug);
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
        if (ptp != NULL) fputs("; ", f_debug);
      }  /* if */
      comma_required = FALSE;
      while (ptp != NULL) {
	if (comma_required) fputs(", ", f_debug);
        db_type(ptp->type);
	comma_required = TRUE;
        ptp = ptp->next;
      }  /* while */
      if (tp->variant.routine.extra_info->has_ellipsis) {
	if (comma_required) fputs(", ", f_debug);
        fputs("...", f_debug);
      }  /* if */
      fputs(") returning ", f_debug);
      db_type(tp->variant.routine.return_type);
      break;
    case tk_typeref:
      fputs("typeref ", f_debug);
      if (tp->variant.typeref.is_const) fputs("const ", f_debug);
      if (tp->variant.typeref.is_volatile) fputs("volatile ", f_debug);
      db_type(tp->variant.typeref.type);
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
    fputc('(', f_debug);
    db_type(con_type);
    fputc(')', f_debug);
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
          fputc(c, f_debug);
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
    case ck_aggregate:
      fputc('{', f_debug);
      cp2 = cp->variant.aggregate.first_constant;
      while (cp2 != NULL) {
        db_constant(cp2);
        cp2 = cp2->next;
        if (cp2 != NULL) fputc(',', f_debug);
      }  /* while */
      fputc('}', f_debug);
      break;
    default:
      fputs("<bad constant>", f_debug);
  }  /* switch */
}  /* db_constant */


void db_variable(a_variable_ptr var_ptr)
/*
Dump the contents of the indicated variable, for debug purposes.
*/
{
  fputs("name = ", f_debug);
  db_name(&var_ptr->source_corresp);
  fputs(", type = ", f_debug);
  db_type(var_ptr->type);
}  /* db_variable */


static void db_expr_node(an_expr_node_ptr node,
		         int              level)
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
      db_type(node->type);
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
        fputs("constant: value=", f_debug);
      } else {
	fprintf(f_debug, "constant (%s): value=",
		const_ptr->source_corresp.name);
      }  /* if */
      db_constant(const_ptr);
      fputs("\n", f_debug);
      break;
    case enk_variable_address:
      fputs("address of - ", f_debug);
      db_variable(node->variant.variable);
      fputs("\n", f_debug);
      break;
    case enk_variable:
      /* For now. */
      fputs("variable - ", f_debug);
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
    case enk_error:
      fputs("error node\n", f_debug);
      break;
#if CHECKING
    default:
      break;
#endif /* CHECKING */
  }  /* switch */
}  /* db_expr_node */


void db_expression(an_expr_node_ptr node)
{
  fputs("*** start of expression ***\n", f_debug);
  db_expr_node(node, 0);
  fputs("*** end of expression ***\n", f_debug);
}  /* db_expression */
#endif /* DEBUG */


#if ALTERNATE_IL_FILE_FORMAT
/*
For the alternate IL file format, each entry's allocation must be preceded
by an entry number, which is initialized to zero here.
*/
#if DEBUG
#define do_alloc(ptr, region_number, size)                            \
{ ptr = alloc_in_region((region_number),                              \
                         (sizeof_t)((size)+sizeof(an_il_entry_number))); \
  *(an_il_entry_number *)ptr = 0;                                     \
  num_il_entry_numbers_allocated++;                                   \
  ptr += sizeof(an_il_entry_number);                                  \
}  /* do_alloc */
#else /* !DEBUG */
#define do_alloc(ptr, region_number, size)                            \
{ ptr = alloc_in_region((region_number),                              \
                         (sizeof_t)((size)+sizeof(an_il_entry_number))); \
  *(an_il_entry_number *)ptr = 0;                                     \
  ptr += sizeof(an_il_entry_number);                                  \
}  /* do_alloc */
#endif /* DEBUG */
#else /* !ALTERNATE_IL_FILE_FORMAT */
/*
For the usual IL file format, or when no IL file is written, no extra space
is required.
*/
#define do_alloc(ptr, region_number, size)                            \
  ptr = alloc_in_region((region_number), (size))
#endif /* ALTERNATE_IL_FILE_FORMAT */


char *alloc_il(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the file scope memory region.
*/
{
  char *ptr;
  do_alloc(ptr, FILE_SCOPE_REGION_NUMBER, size);
  return (ptr);
}  /* alloc_il */


char *alloc_cil(sizeof_t size)
/*
Allocate and return "size" bytes of storage in the current il memory region.
*/
{
  char *ptr;
  do_alloc(ptr, curr_il_region_number, size);
  return (ptr);
}  /* alloc_cil */


void switch_il_region(a_memory_region_number region_number)
/*
Change the current il memory region to "region_number".
*/
{
  curr_il_region_number = region_number;
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Switching to il region %d.\n", curr_il_region_number);
  }  /* if */
#endif /* DEBUG */
}  /* switch_il_region */


void new_il_region(void)
/*
Start a new memory region for the il because of a new function.
Establish this new region as the current il region.
*/
{
  switch_il_region(new_memory_region());
  /* Clear the list of function-local shared constants, since anything on
     the list at this point is from a previous function. */
  func_shareable_constants_list = NULL;
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


static a_source_file_ptr source_file_for_seq(a_seq_number  seq_number,
                                             a_line_number *line_number,
                                             a_boolean     *at_end_of_source)
/*
Find the source file entry within which the sequence number seq_number falls,
and return a pointer to it.  Return NULL if the sequence number falls
outside of any file.  If the sequence number falls within a file, also
return *line_number set to the line number in the file.  Return
*at_end_of_source TRUE if the line number is the special number indicating
the end-of-file line (one more than the last line in the primary input file).
*/
{
  register a_source_file_ptr curr_file, child_file;
  unsigned long              lines_in_children;

  *at_end_of_source = FALSE;
  *line_number = 0;
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
  a_source_file_ptr proper_file;

  db_enter(5, "conv_seq_to_file_and_line");

  /* Find out which file the sequence number is in. */
  proper_file = source_file_for_seq(seq_number, line_number, at_end_of_source);
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


a_boolean seq_is_in_include_file(a_seq_number seq_number)
/*
Return TRUE if the sequence number seq_number falls within an include file.
*/
{
  a_boolean         in_include_file;
  a_source_file_ptr proper_file;
  a_line_number     line_number;
  a_boolean         at_end_of_source;

  proper_file = source_file_for_seq(seq_number, &line_number,
                                    &at_end_of_source);
  in_include_file = (proper_file != NULL &&
                     proper_file != il_header.primary_source_file);
  return in_include_file;
}  /* seq_is_in_include_file */


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


void set_default_source_corresp(a_source_correspondence *sc)
/*
Set the given source correspondence struct to default values.
*/
{
  sc->assoc_info           = NULL;
  sc->name                 = NULL;
  sc->decl_position.seq    = 0;
  sc->decl_position.column = SP_COL_UNKNOWN;
  /* access is set to "public" because "no access restriction" is the default
     for everything except class members.  For the latter the field must be
     set manually. */
  sc->access               = (an_access_specifier)as_public;
  /* referenced is set TRUE because so far this is an entity not associated
     with one in the source program.  All unassociated entities are assumed
     to be referenced (otherwise, they wouldn't be created).  This does away
     with the difficult job of setting the referenced flag in a lot of
     different places for unassociated entities. set_source_corresp resets
     the flag to FALSE for associated entities, for which the flag is then
     set to TRUE (for an actual reference) by mark_referenced. */
  sc->referenced           = TRUE;
  sc->il_walk_flag         = INITIAL_IL_WALK_FLAG_SETTING;
  sc->scope_depth          = IL_NO_SCOPE;
}  /* set_default_source_corresp */


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
    case ck_aggregate:
      cp->variant.aggregate.first_constant = NULL;
      cp->variant.aggregate.last_constant  = NULL;
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

  db_exit();

  return (cp);
}  /* alloc_constant */


a_constant_ptr fs_constant(a_constant_repr_kind kind)
/*
Same as alloc_constant, but allocates a constant in the file scope memory
region.  This is useful for constants that are going to be reused, and
therefore must be in that memory region so they will always be accessible.
The constant is NOT put onto the file-scope constants list at this time,
but the caller must put it there (now or eventually).  This allows
shareable constants to use the "next" field for linkage in the shareable
constants table.  At the end of the compilation, they are put on the
file-scope constants list.
*/
{
  a_constant_ptr         cp;
  a_memory_region_number region_to_switch_back_to = NULL_region_number;

  if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
    region_to_switch_back_to = curr_il_region_number;
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
  }  /* if */
  cp = alloc_constant(kind);
  if (region_to_switch_back_to != NULL_region_number) {
    switch_il_region(region_to_switch_back_to);
  }  /* if */
  return (cp);
}  /* fs_constant */


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
  set_default_source_corresp(&ucp->source_corresp);
  return (ucp);
}  /* alloc_unshared_constant */


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
  return (hash_value);
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
#if CHECKING
      default:
        internal_error("eq_constants: bad constant kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return (eq);
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
  if (!in_file_scope((char *)cp->type)) {
    /* The constant's type is in a function scope (probably because of
       an implicit cast). */
    has_nfs_ref = TRUE;
  } else {
    switch (cp->kind) {
      case ck_error:
      case ck_integer:
      case ck_float:
        /* No references. */
        break;
      case ck_string:
        has_nfs_ref = !in_file_scope(cp->variant.string.value);
        break;
      case ck_address:
        switch (cp->variant.address.kind) {
          case abk_routine:
            has_nfs_ref =
                   !in_file_scope((char *)cp->variant.address.variant.routine);
            break;
          case abk_variable:
            has_nfs_ref =
                  !in_file_scope((char *)cp->variant.address.variant.variable);
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
#if CHECKING
      case ck_aggregate:
        /* Aggregates shouldn't be shared, so we don't expect them here. */
      default:
        internal_error("has_non_file_scope_ref: bad constant kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return (has_nfs_ref);
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
#if CHECKING
    if (cp->implicit_cast) {
      /* Someone did an implicit cast on the constant without clearing the
         source association. */
      internal_error(
           "alloc_shareable_constant: implicitly-cast const has assoc_info");
    }  /* if */
#endif /* CHECKING */
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
      list_ptr = &func_shareable_constants_list;
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
  return (scp);
}  /* alloc_shareable_constant */


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


void add_shareable_constants_to_constants_list(void)
/*
Add the constants in the shareable constants table to the file-scope
constants list.  This is called at the end of compilation, when the
shareable constants table is no longer needed (the table is cleared).
Note that the information in the table is not needed to produce the
debug space summary for the shareable constants table -- there are
separate variables that are set already.
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
      add_to_constants_list(scp);
    }  /* for */
    shareable_constants_table[hash_value] = NULL;
  }  /* for */
}  /* add_shareable_constants_to_constants_list */


void set_integer_constant(a_constant *cp,
                          long       value)
/*
Set the constant entry *cp to the integer constant given by value.
*/
{
  db_enter(5, "set_integer_constant");
  clear_constant(cp, (a_constant_repr_kind)ck_integer);
  cp->type = integer_type((an_integer_kind)ik_int);
  cp->variant.integer_value = value;
  db_exit();
}  /* set_integer_constant */


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
  return (alloc_il(size));
}  /* alloc_text_of_string_literal */


a_param_type_ptr alloc_param_type(a_boolean at_file_scope)
/*
Allocate a new parameter type entry and return a pointer to it.  Set
its fields to default values.  Allocate it at the file scope if
at_file_scope == TRUE.
*/
{
  a_param_type_ptr ptp;

  db_enter(5, "alloc_param_type");

  if (at_file_scope) {
    ptp = (a_param_type_ptr)alloc_il(sizeof(a_param_type));
  } else {
    ptp = (a_param_type_ptr)alloc_cil(sizeof(a_param_type));
  }  /* if */
#if DEBUG
  num_param_types_allocated++;
#endif /* DEBUG */
  ptp->next = NULL;
  ptp->type = NULL;
  ptp->il_walk_flag = INITIAL_IL_WALK_FLAG_SETTING;

  db_exit();
  return (ptp);
}  /* alloc_param_type */


a_derivation_step_ptr alloc_derivation_step(void)
/*
Allocate and initialize a derivation step entry and return a pointer to it.
*/
{
  a_derivation_step_ptr  dsp;

  db_enter(5, "alloc_derivation_step");

  dsp = (a_derivation_step_ptr)alloc_fe(sizeof(a_derivation_step));
#if DEBUG
  num_derivation_steps_allocated++;
#endif /* DEBUG */
  dsp->next       = NULL;
  dsp->base_class = NULL;

  db_exit();
  return dsp;
}  /* alloc_derivation_step */


a_base_class_ptr alloc_base_class(void)
/*
Allocate a base class entry, initialize its fields, and return a pointer
to it.
*/
{
  a_base_class_ptr bcp;

  bcp = (a_base_class_ptr)alloc_cil(sizeof(a_base_class));

#if DEBUG
  num_base_classes_allocated++;
#endif
  bcp->next           = NULL;
  bcp->class          = NULL;
  bcp->virtual        = FALSE;
  bcp->direct	      = FALSE;
  bcp->ambiguous      = FALSE;
  bcp->is_accessible  = FALSE;
  bcp->access         = (an_access_specifier)as_public;
  bcp->offset         = 0;
  bcp->pointer_offset = 0;
  bcp->derivation     = NULL;

  return bcp;
}  /* alloc_base_class */


an_access_adjustment_ptr alloc_access_adjustment(an_access_adjustment_kind kind)
/*
Allocate an access-adjustment entry, initialize its fields, and return a
pointer to it.
*/
{
  an_access_adjustment_ptr aap;

  aap = (an_access_adjustment_ptr)alloc_cil(sizeof(an_access_adjustment));
#if DEBUG
  num_access_adjustments_allocated++;
#endif /* DEBUG */
  aap->next    = NULL;
  aap->access  = (an_access_specifier)as_public;
  aap->kind    = NULL;
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

  clep = (a_class_list_entry_ptr)alloc_cil(sizeof(a_class_list_entry));
#if DEBUG
  num_class_list_entries_allocated++;
#endif /* DEBUG */
  clep->next  = NULL;
  clep->class = NULL;

  return clep;
}  /* alloc_list_entry_for_class */


static a_class_type_supplement_ptr alloc_class_type_supplement(void)
/*
Allocate a class-type-supplement entry, initialize its fields, and return
a pointer to it.
*/
{
  a_class_type_supplement_ptr ctsp;

  ctsp = (a_class_type_supplement_ptr)alloc_cil(
			      sizeof(a_class_type_supplement));
#if DEBUG
  num_class_type_supplements_allocated++;
#endif /* DEBUG */
  ctsp->base_classes                           = NULL;
  ctsp->size_without_virtual_base_classes      = 0;
  ctsp->alignment_without_virtual_base_classes = 1;
  ctsp->access_adjustments                     = NULL;
  ctsp->befriending_classes                    = NULL;
  ctsp->assoc_scope                            = NULL;
  return ctsp;
}  /* alloc_class_type_supplement */


a_type_ptr alloc_type(a_type_kind kind)
/*
Allocate a new type entry and return a pointer to it.  Set general fields,
set kind to the indicated value, and set the associated variant fields
to default values.
*/
{
  a_type_ptr                    pte;
  a_routine_type_supplement_ptr rtsp;

  db_enter(5, "alloc_type");
  pte = (a_type_ptr)alloc_cil(sizeof(a_type));
#if DEBUG
  num_types_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(&(pte->source_corresp));
  pte->next = NULL;
  pte->based_type_array = NULL;
  pte->size = 0;
  pte->alignment = 1;
  pte->kind = kind;
  switch (kind) {
    case tk_error:
    case tk_unknown:
    case tk_none:
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
    case tk_reference:
      pte->variant.pointer_type_pointed_to = NULL;
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
      /* The class type supplement is only allocated in C++ mode. */
      pte->variant.class_struct_union.extra_info       = 
                                           (C_dialect == C_dialect_cplusplus) ?
                                                alloc_class_type_supplement() :
                                                NULL;
      break;
    case tk_routine:
      pte->variant.routine.return_type = NULL;
      pte->variant.routine.extra_info = rtsp =
           (a_routine_type_supplement_ptr)alloc_cil(
                                           sizeof(a_routine_type_supplement));
#if DEBUG
      num_routine_type_supplements_allocated++;
#endif /* DEBUG */
      rtsp->param_type_list          = NULL;
      rtsp->implicit_this_param_type = NULL;
      rtsp->prototype_scope          = NULL;
      rtsp->assoc_routine            = NULL;
      rtsp->prototyped               = FALSE;
      rtsp->has_ellipsis             = FALSE;
      rtsp->lint_argsused_flag       = FALSE;
      rtsp->lint_varargs_count       = NOT_LINT_VARARGS;
      rtsp->arg_pragma               = (an_arg_pragma_kind)apk_none;
      break;
    case tk_typeref:
      pte->variant.typeref.type        = NULL;
      pte->variant.typeref.is_const    = FALSE;
      pte->variant.typeref.is_volatile = FALSE;
      break;
#if CHECKING
    default:
      internal_error("alloc_type: bad type kind");
#endif /* CHECKING */
  }  /* switch */
  db_exit();
  return (pte);
}  /* alloc_type */


static a_scope_ptr ensure_il_scope_exists(a_scope_stack_entry_ptr ssep)
/*
Make sure that the scope stack entry pointed to by ssep points to an IL
scope.  For block scopes, create the scope now if necessary.
*/
{
  a_scope_ptr sp = ssep->il_scope;

  if (sp == NULL) {
    /* There is no IL scope. */
    if (ssep->kind == (a_scope_kind)sck_block) {
      /* Create the IL scope in a block scope. */
      ssep->il_scope = sp = alloc_scope(ssep->number, (a_scope_kind)sck_block);
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


void add_to_types_list(a_type_ptr type_ptr,
                       a_boolean  at_file_scope,
                       a_boolean  in_old_style_param_decl_list)
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

  /* Get a pointer to the current or file scope entry. */
  ssep = &scope_stack[at_file_scope ? DEPTH_OF_FILE_SCOPE : decl_scope_level];
  /* Create the IL scope if necessary (for block scopes or class scopes). */
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
    }  /* if */
#endif /* CHECKING */
    /* In C++, there can be type qualifiers above the tk_routine entry. */
    routine_type = skip_typerefs(routine_type);
#if CHECKING
    if (routine_type->kind != (a_type_kind)tk_routine) {
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
       called because this is not a scope for a statement block. */
    sp = alloc_scope(ssep->number, (a_scope_kind)sck_func_prototype);
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


a_type_ptr fs_type(a_type_kind kind)
/*
Same as alloc_type, but allocates a type in the file scope memory region.
This is useful for types that are going to be reused, and therefore must
be in that memory region so they will always be accessible.  The type is
put onto the file-scope types list.
*/
{
  a_type_ptr             pte;
  a_memory_region_number region_to_switch_back_to = NULL_region_number;

  if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
    region_to_switch_back_to = curr_il_region_number;
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
  }  /* if */
  pte = alloc_type(kind);
  add_to_types_list(pte, /*at_file_scope=*/TRUE,
                    /*in_old_style_param_decl_list=*/FALSE);
  if (region_to_switch_back_to != NULL_region_number) {
    switch_il_region(region_to_switch_back_to);
  }  /* if */
  return (pte);
}  /* fs_type */


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
    int_types[kind] = pit = fs_type((a_type_kind)tk_integer);
    pit->variant.integer.int_kind = kind;
    set_type_size(pit);
  }  /* if */
  return (pit);
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
    il_signed_int_type = fs_type((a_type_kind)tk_integer);
    il_signed_int_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    il_signed_int_type->variant.integer.explicitly_signed = TRUE;
    set_type_size(il_signed_int_type);
  }  /* if */
  return (il_signed_int_type);
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
    float_types[kind] = pft = fs_type((a_type_kind)tk_float);
    pft->variant.float_kind = kind;
    set_type_size(pft);
  }  /* if */
  return (pft);
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
    pst = fs_type((a_type_kind)tk_array);
    pst->variant.array.element_type = integer_type(plain_char_int_kind);
    pst->variant.array.number_of_elements = num_chars;
    set_type_size(pst);
    if (num_chars <= MAX_TRACKED_STRING_TYPE_LENGTH) {
      string_types[num_chars] = pst;
    }  /* if */
  }  /* if */
  return (pst);
}  /* string_type */


a_type_ptr error_type(void)
/*
Make or find a type entry for an error type, and return a pointer to it.
*/
{
  if (il_error_type == NULL) {
    il_error_type = fs_type((a_type_kind)tk_error);
    set_type_size(il_error_type);
  }  /* if */
  return (il_error_type);
}  /* error_type */


a_type_ptr void_type(void)
/*
Make or find a type entry for an void type, and return a pointer to it.
*/
{
  if (il_void_type == NULL) {
    il_void_type = fs_type((a_type_kind)tk_void);
  }  /* if */
  return (il_void_type);
}  /* void_type */


a_type_ptr no_type(void)
/*
Make or find a type entry for a tk_none type, and return a pointer to it.
*/
{
  if (il_no_type == NULL) {
    il_no_type = fs_type((a_type_kind)tk_none);
  }  /* if */
  return (il_no_type);
}  /* no_type */


a_type_ptr *get_based_type(a_type_ptr                     base_type,
                           a_based_type_array_element_num element_num)
/*
Fetch and return a pointer to an element of the based_type_array pointed
to by the type entry identified by base_type.  element_num indicates the
element number (e.g., bta_pointer), and therefore the particular based
type that is sought.  Allocate the based_type_array if needed (it is not
allocated until it is needed).  The based_type_array is used to hold
pointers to types based on the base type, so that only one copy of
pointer-to that type, reference-to that type, etc., is allocated.
Note that the value returned is not the actual type wanted, but a pointer
to the proper element of the array; the caller must check to see if the
element is NULL or non-NULL, and allocate and record the appropriate based
type if necessary.
*/
{
  a_based_type_array_ptr btap;
  a_type_ptr             *element_ptr;

#if DEBUG
  num_get_based_type_calls++;
#endif /* DEBUG */
  btap = base_type->based_type_array;
  if (btap == NULL) {
    /* No based type array yet; allocate it. */
    btap = (a_based_type_array_ptr)alloc_il(sizeof(a_based_type_array));
    /* Depending on NULL represented as zero bits here. */
    memzero((char *)btap, sizeof(a_based_type_array));
    base_type->based_type_array = btap;
#if DEBUG
    num_based_type_arrays_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Develop and return a pointer to the right element. */
  element_ptr = &btap[(int)element_num];
#if DEBUG
  /* Count cases where the allocation will have to be done. */
  if (*element_ptr == NULL) num_costly_get_based_type_calls++;
#endif /* DEBUG */
  return element_ptr;
}  /* get_based_type */


a_type_ptr make_pointer_type(a_type_ptr type_pointed_to)
/*
Allocate a pointer type record and initialize it.  Attempt to find and reuse
an existing entry if possible.
*/
{
  register a_type_ptr ptr;
  register a_type_ptr *btaep;

  /* See if a pointer type for the type pointed to has already been allocated.
     If one was allocated, a pointer to it is stored in the based_type_array
     for the base type, and the pointer type can be reused. */
  btaep = get_based_type(type_pointed_to, bta_pointer);
  ptr = *btaep;
  if (ptr == NULL) {
    /* No allocated entry, need to allocate one.  If the entry is a pointer
       to a file-scope type, make sure it gets allocated in the file-scope
       memory region. */
    if (in_file_scope((char *)type_pointed_to)) {
      ptr = fs_type((a_type_kind)tk_pointer);
    } else {
      ptr = alloc_type((a_type_kind)tk_pointer);
    }  /* if */
    ptr->variant.pointer_type_pointed_to = type_pointed_to;
    set_type_size(ptr);
    /* Remember the existence of this pointer type by putting a pointer
       to it in the based_type_array. */
    *btaep = ptr;
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
  register a_type_ptr *btaep;

  /* See if a reference type for the type pointed to has already been
     allocated.  If one was allocated, a pointer to it is stored in the
     based_type_array for the base type, and the reference type can be
     reused. */
  btaep = get_based_type(type_pointed_to, bta_reference);
  ptr = *btaep;
  if (ptr == NULL) {
    /* No allocated entry, need to allocate one.  If the entry is a reference
       to a file-scope type, make sure it gets allocated in the file-scope
       memory region. */
    if (in_file_scope((char *)type_pointed_to)) {
      ptr = fs_type((a_type_kind)tk_reference);
    } else {
      ptr = alloc_type((a_type_kind)tk_reference);
    }  /* if */
    ptr->variant.pointer_type_pointed_to = type_pointed_to;
    set_type_size(ptr);
    /* Remember the existence of this reference type by putting a pointer
       to it in the based_type_array. */
    *btaep = ptr;
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
  register a_type_ptr            ptr;
  register a_type_ptr            *btaep;
  a_based_type_array_element_num element_num;

  /* Add only qualifiers not present already in the type. */
  is_const = is_const && !is_const_qualified_type(base_type);
  is_volatile = is_volatile && !is_volatile_qualified_type(base_type);
  if (is_const || is_volatile) {
    /* Type qualifiers are added by adding a typeref entry which includes
       the type qualifiers.  The original type is not modified. */
    /* See if a typeref type for the type pointed to has already been
       allocated.  If one was allocated, a pointer to it is stored in the
       based_type_array for the base type, and the typeref type can be
       reused. */
    /* Determine the proper element number in the based type array. */
    if (is_const) {
      if (is_volatile) {
        element_num = bta_const_volatile;
      } else {
        element_num = bta_const;
      }  /* if */
    } else {
      element_num = bta_volatile;
    }  /* if */
    btaep = get_based_type(base_type, element_num);
    ptr = *btaep;
    if (ptr == NULL) {
      /* No allocated entry, need to allocate one.  If the entry is a typeref
         to a file-scope type, make sure it gets allocated in the file-scope
         memory region. */
      if (in_file_scope((char *)base_type)) {
        ptr = fs_type((a_type_kind)tk_typeref);
      } else {
        ptr = alloc_type((a_type_kind)tk_typeref);
      }  /* if */
      ptr->variant.typeref.type        = base_type;
      ptr->variant.typeref.is_const    = is_const;
      ptr->variant.typeref.is_volatile = is_volatile;
      /* Remember the existence of this typeref type by putting a pointer
         to it in the based_type_array. */
      *btaep = ptr;
    }  /* if */
  } else {
    /* No qualifiers to add, so return the original type. */
    ptr = base_type;
  }  /* if */

  return ptr;
}  /* make_qualified_type */


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


void copy_type(a_type_ptr from,
               a_type_ptr to)
/*
Copy the type entry "from" to "to".
*/
{
  a_type_kind                   from_kind;
  a_routine_type_supplement_ptr extra_info;
  a_type_ptr                    next_ptr;

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
  to->based_type_array = NULL;
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


a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind,
                                      a_variable_ptr      variable)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind and its variable to variable, and return a pointer to it.
*/
{
  a_dynamic_init_ptr dip;

  db_enter(5, "alloc_dynamic_init");

  dip = (a_dynamic_init_ptr)alloc_cil(sizeof(a_dynamic_init));
#if DEBUG
  num_dynamic_inits_allocated++;
#endif /* DEBUG */
  dip->next     = NULL;
  dip->variable = variable;
  dip->kind     = kind;
  switch (kind) {
    case dik_constant:
      dip->variant.constant = NULL;
      break;
    case dik_expression:
      dip->variant.expression = NULL;
      break;
    case dik_statement:
      dip->variant.statement.constructor = NULL;
      dip->variant.statement.destructor  = NULL;
      break;
#if CHECKING
    default:
      internal_error("alloc_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  db_exit();
  return dip;
}  /* alloc_dynamic_init */


a_variable_ptr alloc_variable(void)
/*
Allocate a variable entry, clear it to default values, and return a pointer
to it.
*/
{
  a_variable_ptr vp;

  db_enter(5, "alloc_variable");

  vp = (a_variable_ptr)alloc_cil(sizeof(a_variable));
#if DEBUG
  num_variables_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(&(vp->source_corresp));
  vp->next                        = NULL;
  vp->type                        = NULL;
  vp->storage_class               = (a_storage_class)sc_unspecified;
  vp->address_taken               = FALSE;
  vp->is_parameter                = FALSE;
  vp->init_kind                   = (an_init_kind)initk_none;
#ifdef FIL
  vp->by_address                  = FALSE;
  vp->base_var                    = NULL;
  vp->association_offset          = 0;
  vp->function_result_var_function= NULL;
#endif /* ifdef FIL */

  db_exit();
  return (vp);
}  /* alloc_variable */


void add_to_variables_list(a_variable_ptr var_ptr,
                           a_boolean      at_file_scope)
/*
Add the given variable to the variables list for the current scope, or
for the file scope if at_file_scope is TRUE.
*/
{
  a_scope_stack_entry_ptr
		 ssep;
  a_scope_ptr    sp;

  /* Get pointer to current or file scope entry. */
  ssep = &scope_stack[at_file_scope ? DEPTH_OF_FILE_SCOPE : decl_scope_level];
  /* Create the IL scope if necessary (for block scopes or class scopes). */
  sp = ensure_il_scope_exists(ssep);
  if (sp->variables == NULL) {
    sp->variables = var_ptr;
  } else {
    ssep->last_variable->next = var_ptr;
  }  /* if */
  ssep->last_variable = var_ptr;
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


a_field_ptr alloc_field(void)
/*
Allocate a field entry, clear it to default values, and return a pointer
to it.
*/
{
  a_field_ptr fp;

  db_enter(5, "alloc_field");

  fp = (a_field_ptr)alloc_cil(sizeof(a_field));
#if DEBUG
  num_fields_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(&(fp->source_corresp));
  fp->next       = NULL;
  fp->type       = NULL;
  fp->bit_offset = 0;
  fp->bit_size   = 0;

  db_exit();
  return (fp);
}  /* alloc_field */


a_routine_ptr alloc_routine(void)
/*
Allocate a routine entry, clear it to default values, and return a pointer
to it.
*/
{
  a_routine_ptr rp;

  db_enter(5, "alloc_routine");

  /* Even though at the moment routines always end up at the file scope level,
     we do not call alloc_il here.  It wouldn't hurt, but it's not 
     necessary, since the caller handles the switch of memory regions.
     And, this approach makes it possible to allocate routines differently
     if the rules change in the future. */
  rp = (a_routine_ptr)alloc_cil(sizeof(a_routine));
#if DEBUG
  num_routines_allocated++;
#endif /* DEBUG */
  set_default_source_corresp(&(rp->source_corresp));
  rp->next                = NULL;
  rp->type                = NULL;
  rp->assoc_scope         = NULL_region_number;
  rp->storage_class       = (a_storage_class)sc_unspecified;
  rp->special_kind        = (a_special_function_kind)sfk_none;
  rp->is_inline           = FALSE;
  rp->is_virtual          = FALSE;
  rp->befriending_classes = NULL;
#ifdef FIL
  rp->is_fortran_entry    = FALSE;
  rp->local_routine_scope = NULL;
  rp->intrinsic_func_code = (an_intrinsic_function_code)ifc_none;
#endif /* ifdef FIL */

  db_exit();
  return (rp);
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
  a_scope_stack_entry_ptr
		 ssep;
  a_scope_ptr    sp;

  /* Get pointer to current or file scope entry. */
  ssep = &scope_stack[at_file_scope ? DEPTH_OF_FILE_SCOPE : decl_scope_level];
  /* Create the IL scope if necessary (for block scopes or class scopes). */
  sp = ensure_il_scope_exists(ssep);
  if (sp->routines == NULL) {
    sp->routines = rout_ptr;
  } else {
    ssep->last_routine->next = rout_ptr;
  }  /* if */
  ssep->last_routine = rout_ptr;
  rout_ptr->next = NULL;
}  /* add_to_routines_list */


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
  lp->next = NULL;
  lp->variant.exec_stmt = NULL;
#ifdef FIL
  lp->kind = (a_label_kind)lk_executable;
  lp->used_in_assign = FALSE;
#endif /* ifdef FIL */

  db_exit();
  return (lp);
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
  switch(kind) {
    case enk_error:
      /* No variant fields. */
      break;
    case enk_operation:
      node->variant.operation.kind = (an_expr_operator_kind)eok_last;
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
  return (ptr);
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

  return (node);
}  /* make_operator_node */


an_expr_node_ptr error_node(void)
/*
Make and return an expression node indicating an error.
*/
{
  register an_expr_node_ptr node;

  node = alloc_expr_node((an_expr_node_kind)enk_error);
  node->type = error_type();

  return (node);
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

  return (node);
}  /* alloc_node_for_constant */


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
  scp->next           = NULL;
  scp->constant_list  = NULL;
  scp->statements     = NULL;
  return (scp);
}  /* alloc_switch_clause */


a_statement_ptr alloc_statement(a_statement_kind stmt_kind)
/*
Allocate a statement entry, clear it to default values, and return a pointer
to it.  The statement kind is set as indicated.
*/
{
  a_statement_ptr sp;
  a_block_ptr     bp;

  db_enter(5, "alloc_statement");

  sp = (a_statement_ptr)alloc_cil(sizeof(a_statement));
#if DEBUG
  num_statements_allocated++;
#endif /* DEBUG */
  sp->seq_number       = pos_curr_token.seq;
  sp->next             = NULL;
  sp->kind             = stmt_kind;
  sp->expr             = NULL;
  switch(stmt_kind) {
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
      break;
    case stmk_init:
      sp->variant.dynamic_init = NULL;
      break;
    case stmk_asm:
      sp->variant.asm_string = NULL;
      break;
#if CHECKING
    default:
      internal_error("alloc_statement: bad kind");
#endif /* CHECKING */
  }  /* switch */
  db_exit();
  return (sp);
}  /* alloc_statement */


a_scope_ptr alloc_scope(a_scope_number number,
                        a_scope_kind   kind)
/*
Allocate a scope entry, and return a pointer to it.  Set fixed fields to
default values.  number indicates the unique number for the scope, and
kind indicates the scope kind (e.g., function, block).
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
      sp->variant.routine.ptr                 = NULL;
      sp->variant.routine.parameters          = NULL;
      sp->variant.routine.this_param_variable = NULL;
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
  sp->labels              = NULL;
  sp->routines            = NULL;
  sp->scopes              = NULL;
  sp->dynamic_inits       = NULL;
#ifdef FIL
  sp->entries             = NULL;
  sp->namelist_groups     = NULL;
#endif /* ifdef FIL */

  db_exit();
  return (sp);
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
  write_one("class type supplement", num_class_type_supplements_allocated,
                                     a_class_type_supplement);
  write_one("access adjustment", num_access_adjustments_allocated,
                                 an_access_adjustment);
  write_one("class list entry", num_class_list_entries_allocated,
                                a_class_list_entry);
  write_one("derivation steps", num_derivation_steps_allocated,
                                a_derivation_step);
  write_one("base class", num_base_classes_allocated, a_base_class);
  write_one("type", num_types_allocated, a_type);
  write_one("dynamic init", num_dynamic_inits_allocated, a_dynamic_init);
  write_one("variable", num_variables_allocated, a_variable);
  write_one("field", num_fields_allocated, a_field);
  write_one("routine", num_routines_allocated, a_routine);
  write_one("label", num_labels_allocated, a_label);
  write_one("expr node", num_expr_nodes_allocated, an_expr_node);
  write_one("switch clause", num_switch_clauses_allocated, a_switch_clause);
  write_one("block", num_blocks_allocated, a_block);
  write_one("statement", num_statements_allocated, a_statement);
  write_one("scope", num_scopes_allocated, a_scope);
#if ALTERNATE_IL_FILE_FORMAT
  write_one("IL entry numbers", num_il_entry_numbers_allocated,
            an_il_entry_number);
#endif /* ALTERNATE_IL_FILE_FORMAT */

  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total", "", "", grand_total);

  fputc('\n', f_debug);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "get_based_type calls", "", "",
                                          num_get_based_type_calls);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "... that allocate a type", "", "",
                                          num_costly_get_based_type_calls);
  write_one("based type array", num_based_type_arrays_allocated,
            a_based_type_array);
  
  
  fputc('\n', f_debug);
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

  return (grand_total);
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
  /* Variable in il.h: */
  curr_il_region_number = NULL_region_number;

  /* Static variables in il.c: */
#if CHECKING && DEBUG
  /* Check that the table of storage class names is correctly initialized.
     This guards against someone changing the enumeration and forgetting to
     update db_storage_class_names. */

  if (db_storage_class_names[(int)sc_last] == NULL ||
      strcmp(db_storage_class_names[(int)sc_last], "last") != 0) {
    internal_error(
              "il_init: incorrect initialization of db_storage_class_names");
  }  /* if */
#endif /* CHECKING && DEBUG */
  /* Depending on NULL represented as zero bits here. */
  memzero((char *)int_types, sizeof(int_types));
  memzero((char *)float_types, sizeof(float_types));
  memzero((char *)string_types, sizeof(string_types));
  il_signed_int_type = il_error_type = il_void_type = NULL;
  memzero((char *)shareable_constants_table,
          sizeof(shareable_constants_table));
#if DEBUG
  num_constants_allocated                = 0;
  num_param_types_allocated              = 0;
  num_routine_type_supplements_allocated = 0;
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
  num_labels_allocated                   = 0;
  num_expr_nodes_allocated               = 0;
  num_switch_clauses_allocated           = 0;
  num_blocks_allocated                   = 0;
  num_statements_allocated               = 0;
  num_scopes_allocated                   = 0;
  string_literal_text_space_allocated    = 0;
  num_shareable_constants                = 0;
  num_func_shareable_constants           = 0;
  num_used_shareable_constant_buckets    = 0;
  num_searches_for_shareable_constants   = 0;
  num_compares_for_shareable_constants   = 0;
  num_based_type_arrays_allocated        = 0;
  num_get_based_type_calls               = 0;
  num_costly_get_based_type_calls        = 0;
#if ALTERNATE_IL_FILE_FORMAT
  num_il_entry_numbers_allocated         = 0;
#endif /* ALTERNATE_IL_FILE_FORMAT */
#endif /* DEBUG */
}  /* il_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
