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

symbol_tbl.c - Symbol table management routines.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Although symbol_tbl.c is not really a "declaration processing file",
   it turns out that most of the header files it needs are in decl_hdrs.h. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */
/* exprutil.h is needed to get an_expr_stack_entry for the scope stack. */
#include "exprutil.h"
/* statement.h is needed because of wrapup_control_flow_processing call. */
#include "statements.h"

/* The multiplier used in the hash algorithm that generates an index
   in the hash table from an identifier name string.  Do not change
   without investigating the hash table performance that results.
   Prime values are likely to work better than non-prime values. */
#define HASH_FACTOR 73

/*
Dummy symbol headers used for compiler-generated error symbols and for
unnamed class symbols.
*/
static a_symbol_header_ptr
		error_symbol_header,
		unnamed_class_symbol_header,
		anonymous_parent_object_symbol_header,
		unnamed_field_symbol_header;

/*
An empty symbol used to initialize newly allocated symbols.
*/
static a_symbol cleared_symbol;

/*
Set the shared fields of a symbol to default values, set the kind, and
initialize its variant fields.
*/
#define clear_symbol(sym, kind)                                    \
  { *(sym) = cleared_symbol; set_symbol_kind((sym), (kind)); }

#if DEBUG
/*
Information used to gather performance statistics on the symbol table:
*/
static unsigned long
		num_symbols_allocated,
		num_symbol_headers_allocated,
		num_symbol_headers_in_hash_table,
		num_conversion_headers_allocated,
		symbol_name_string_space,
		num_class_symbol_supplements_allocated,
		num_template_symbol_supplements_allocated,
		num_template_params_allocated,
		num_param_ids_allocated,
		num_dependent_type_fixups_allocated,
		num_template_instances_allocated,
		num_symbol_list_entries_allocated,
		num_extern_symbol_descrs_allocated,
		num_extern_type_fixups_allocated,
		num_projection_descrs_allocated,
		num_used_symbol_buckets,
		num_searches_for_symbols,
		num_compares_for_symbols,
		num_fast_id_lookups,
		num_slow_id_lookups,
		num_access_error_descrs_allocated;
#endif /* DEBUG */

/*
Variables and constants related to the scope_stack:
*/
static sizeof_t	size_scope_stack = 0;
			/* Allocated size of scope_stack in elements.
			   Not per-file. */
#define SCOPE_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to scope_stack each
			   time it is reallocated; also the initial
			   allocation. */

static a_scope_depth
		num_classes_on_scope_stack;
			/* Current count of sck_class_struct_union and
			   sck_class_reactivation entries in scope_stack.
			   When non-zero, we are inside a class or
			   reactivation of the scope of a class, and name
			   lookup is more complicated. */
static a_scope_depth
		depth_of_innermost_scope_that_affects_access_control;
			/* If there are scopes on the scope stack that
			   affect C++ access control, this is the depth of
			   the innermost one.  Otherwise, NO_SCOPE_DEPTH.
			   Heads a list linked by
			   next_scope_that_affects_access_control. */

/*
Array used to hold an identifier for external name or destructor name
generation.
*/
static char     *ident_buffer = NULL;
			/* Buffer itself.  Dynamic allocated; current size is
			   given by size_ident_buffer.  Allocated in general
			   storage.  Not per-file. */
static sizeof_t size_ident_buffer = 0;
			/* Current allocated size of ident_buffer. */
#define IDENT_BUFFER_INCREMENTAL_ALLOCATION 300
			/* Incremental allocation for ident_buffer.  Should
			   be bigger than most identifiers. */

static a_param_id_ptr
		avail_param_ids;
			/* List of parameter id entries freed and available
			   for reuse. */

static a_dependent_type_fixup_ptr
		avail_dependent_type_fixups;
			/* List of dependent type fixup entries freed and
			   available for reuse. */

static an_access_error_descr_ptr
		avail_access_error_descrs;
			/* List of access error description  entries (allocated
                           in front end storage) freed and available for
                           reuse. */


void form_symbol_name(a_symbol_ptr                          sym,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output the (possibly qualified) name of the indicated symbol.  The output
is done according to the output control block octl.
*/
{
  char                    *entry;
  an_il_entry_kind        kind;
  a_source_correspondence *scp;

  /* See whether the symbol has an associated IL entry and whether the IL
     entry has a source correspondence field -- but use it only if its
     class-of-which-a-member matches that of the symbol (they can differ
     for symbols promoted from anonymous unions, in which case preference
     is given to the symbol). */
  entry = il_entry_for_symbol(sym, &kind);
  if (entry != NULL &&
      (scp = source_corresp_for_il_entry(entry, kind)) != NULL &&
      scp->class_of_which_a_member == sym->class_of_which_a_member) {
    /* Use the IL entry to generate the name. */
    form_name(scp, kind, octl);
  } else {
    /* No source correspondence entry, or else it has a different class
       parent; use the symbol name directly. */
    if (il_header.source_language == sl_Cplusplus) {
      a_type_ptr class_type = sym->class_of_which_a_member;
      /* Put out the class qualifier on a class member. */
      if (class_type != NULL) {
        form_class_qualifier(class_type, octl);
      }  /* if */
    }  /* if */
    octl->output_str(sym->header->identifier);
  }  /* if */
}  /* form_symbol_name */


#if DEBUG
#define DEBUG_LINE_LENGTH ((unsigned int)79)
/* Macros used within db_symbol, referencing local variables defined
   in that routine. */
/* put_separator appends the separator to the current line, along with a
   blank if the line still has room for sting_len additional characters;
   otherwise, it puts a new-line character and indents the next line.
   Variable col is updated in both cases. */
#define put_separator(separator, string_len)			\
{ col += strlen(separator) + 1;					\
  if (col + (string_len) > DEBUG_LINE_LENGTH) {				\
    fprintf(f_debug, "%s\n%*s", (separator), indentation, "");	\
    col = indentation;						\
  } else {							\
    fprintf(f_debug, "%s ", (separator));			\
  }  /* if */							\
}  /* put_separator */


/* put_string puts out a comma separator and then writes out str.  col is
   updated. */
#define put_string(str)						\
{ put_separator(",", strlen(str));				\
  fputs((str), f_debug);					\
  col += strlen((str));						\
}  /* put_string */


/* Determines whether the current line has a certain amount of room left. */
#define space_left(size)  (DEBUG_LINE_LENGTH - (size) + 1 >= col)


/*
Current output buffer pointer for put_str_into_db_symbol_buffer. */
static char *db_symbol_buffer_pointer;

static void put_str_into_db_symbol_buffer(char *str)
/*
Output a string into the db_symbol buffer.  Used once
set_up_for_output_to_buffer has been called to set the buffer address.
*/
#if 0
/* There is no overflow check on this. */
#endif  /* 0 */
{
  /* Copy the string including the terminating null. */
  while ((*db_symbol_buffer_pointer++ = *str++) != '\0') {}
  /* Back up onto the null character so it will be rewritten if something
     else is added to the output. */
  db_symbol_buffer_pointer--;
}  /* put_str_into_db_symbol_buffer */


/*
Output control block used to interface to the il_to_str routines.
*/
static an_il_to_str_output_control_block octl;


static void set_up_for_output_to_buffer(char *buffer)
/*
Set octl so that it can be passed into the il_to_str routines to tell them
to output to the indicated buffer.
*/
{
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_into_db_symbol_buffer;
  octl.gen_pcc_code = (C_dialect == C_dialect_pcc);
  db_symbol_buffer_pointer = buffer;
}  /* set_up_for_output_to_buffer */


static char *str_access(char                *buffer,
                        an_access_specifier access)
/*
Construct a string in buffer that represents an access specifier -- called
from db_symbol.
*/
{
  char  *s;
  switch (access) {
    case as_public:       s = "public";       break;
    case as_protected:    s = "protected";    break;
    case as_private:      s = "private";      break;
    case as_inaccessible: s = "inaccessible"; break;
  }  /* switch */
  (void)sprintf(buffer, "%s", s);
  return buffer;
}  /* str_access */


/* Display an access specifier. */
#define put_access(access)                                      \
{									\
  (void)str_access(buffer, (an_access_specifier)(access)); put_string(buffer);\
}


static void str_type(char        buffer[],
                     a_type_ptr  tp)
/*
Construct a string in buffer that represents a type.
*/
{
  set_up_for_output_to_buffer(buffer);
  form_type(tp, &octl);
}  /* str_type */


static char *str_qualified_name(char         buffer[],
                                a_symbol_ptr sym)
/*
Construct a string in buffer that represents a qualified name -- called
from db_symbol.
*/
{
  set_up_for_output_to_buffer(buffer);
  form_symbol_name(sym, &octl);
  return buffer;
}  /* str_qualified_name */


static char *str_path(char               buffer[],
                      a_derivation_step  *path,
                      char               *initial_string,
                      char               *separator)
/*
Construct a string in buffer that represents a derivation path -- called
from db_symbol.
*/
{
  a_derivation_step  *dsp;
  a_base_class       *bcp;
  a_type             *tp;
  char               *sep = initial_string;

  buffer[0] = '\0';
  for (dsp = path; dsp != NULL; dsp = dsp->next) {
    bcp = dsp->base_class;
    (void)sprintf(&buffer[strlen(buffer)],
                  "%s%s",
                  sep,
                  (bcp == NULL) ?
                      "<null bcp>" :
                      ((tp = bcp->type) == NULL) ?
                          "<null tp>" : tp->source_corresp.name);
    sep = separator;
  }  /* for */
  return buffer;
}  /* str_path */


static char *str_name_linkage(char                     *buffer,
                              a_source_correspondence  *source_corresp)
/*
Construct a string in buffer that represents a name linkage kind -- called
from db_symbol.
*/
{
  char *str;

  switch (source_corresp->name_linkage) {
    case nlk_none:                str = "no"; break;
    case nlk_internal:            str = "int'l"; break;
    case nlk_external:            str = "ext'l"; break;
    case nlk_cplusplus_external:  str = "C++"; break;
    default:                      str = "<BAD KIND>"; break;
  }  /* switch */
  (void)sprintf(&buffer[0], "%s linkage", str);
  return buffer;
}  /* str_name_linkage */


void db_symbol(a_symbol_ptr	sym,
	       char		*string,
	       int		indentation)
/*
Write out information on a symbol, for debugging purposes.  sym points to
the symbol; string is an optional identifying string ("" or NULL if omitted);
and indentation is the indentation desired.
*/
{
  char				*str, buffer[1000];
  int				col = indentation;
  a_type_ptr			type = NULL, temp_type;
  a_variable_ptr		var = NULL;
  a_routine_ptr                 rp;
  a_boolean                     suppress_newline = FALSE;
  a_symbol_ptr                  apo_sym = NULL;

  if (string != NULL && strlen(string) > 0) {
    fputs(string, f_debug);
    col += strlen(string);
  }  /* if */

  str = symbol_kind_names[(int)sym->kind];
  if (col + strlen(str) + 2 > DEBUG_LINE_LENGTH) {
    fprintf(f_debug, "\n%*s", indentation, "");
    col = indentation;
  }  /* if */
  fprintf(f_debug, "<%s>", str);
  col += strlen(str) + 2;

  str = str_qualified_name(buffer, sym);
  put_separator("", strlen(str) + 2);
  fprintf(f_debug, "\"%s\"", str);
  col += strlen(str) + 2;

  if (sym->kind == (a_symbol_kind)sk_projection) {
    a_symbol_ptr fsym = sym->variant.projection.extra_info->fundamental_symbol;
    if (fsym != NULL) str = str_qualified_name(buffer, fsym);
    put_separator("", strlen(str) + 6);
    fprintf(f_debug, "(= \"%s\")", str);
    col += strlen(str) + 6;
  }  /* if */

  if (sym->decl_seq > 0) {
    (void)sprintf(buffer, "#%lu", sym->decl_seq);
    put_separator("", strlen(buffer));
    fputs(buffer, f_debug);
    col += strlen(buffer);
  }  /* if */

  (void)sprintf(buffer, "(%lu/%d)", sym->decl_position.seq,
		sym->decl_position.column);
  put_separator("", strlen(buffer));
  fputs(buffer, f_debug);
  col += strlen(buffer);

  (void)sprintf(buffer, "scope %d", sym->decl_scope);
  put_string(buffer);

  if (sym->referenced) put_string("ref'd");
  if (sym->defined) put_string("def'd");
  switch (sym->kind) {
    case sk_undefined:
    case sk_extern_variable:
    case sk_extern_routine:
    case sk_parameter:
      break;
    case sk_keyword:
      fprintf(f_debug, "\"%s\"",
                       token_names[(int)sym->variant.keyword.token]);
      break;
    case sk_macro:

      break;
    case sk_constant:
      fprintf(f_debug, ",\n%*s", indentation, "");
      db_constant(sym->variant.constant);
      break;
    case sk_type:
    case sk_enum_tag:
      type = sym->variant.type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      type = sym->variant.class_struct_union.type;
      if (type == NULL) break;
      /* The result of skip_typerefs() is copied to a temporary variable to
         work around a problem with Borland C++. */
      temp_type = skip_typerefs(type);
      (void)str_name_linkage(buffer, &(temp_type->source_corresp));
      put_string(buffer);
      {
        a_class_symbol_supplement_ptr  cssp;
        cssp = sym->variant.class_struct_union.extra_info;
        if (cssp->constructor_required) put_string("ctor req'd");
        if (cssp->destructor_required) put_string("dtor req'd");
        if (cssp->constructor != NULL) put_string("has ctor");
        if (cssp->has_default_constructor) {
          put_string("has default-ctor");
        }  /* if */
        if (cssp->has_copy_constructor_for_const_object) {
          put_string("has const-copy-ctor");
        } else if (cssp->has_copy_constructor) {
          put_string("has copy-ctor");
        }  /* if */
        if (cssp->destructor != NULL) put_string("has dtor");
        if (cssp->construction_by_bitwise_copy_allowed) {
          put_string("ctor bitwise copy okay");
        }  /* if */
        if (cssp->assignment_by_bitwise_copy_allowed) {
          put_string("op= bitwise copy okay");
        }  /* if */
        if (cssp->class_template != NULL) {
          if (debug_level >= 4) put_string("has class template ptr");
        }  /* if */
        if (cssp->is_nonreal_class) {
          put_string("nonreal");
        }  /* if */
        if (cssp->is_prototype_instantiation) {
          put_string("prototype instantiation");
        }  /* if */
        if (cssp->is_specific_template_def) {
          put_string("specific template def");
        }  /* if */
        if (cssp->any_nonreal_base_classes) {
          put_string("has nonreal base class");
        }  /* if */
        if (cssp->member_decl_scope != NO_SCOPE_NUMBER) {
          sprintf(buffer, "member_decl_scope %0d\n", cssp->member_decl_scope);
        }  /* if */
        if (cssp->template_param_for_proxy_class != NULL) {
          if (debug_level >= 4) put_string("has ptr for proxy");
        }  /* if */
      }
      if (temp_type->declared_in_function_prototype) {
        put_string("in func prototype");
      }  /* if */
      break;
    case sk_field:
      if (sym->variant.field.ptr == NULL) {
        put_string("<null>");
      } else {
        a_field_ptr  fp = sym->variant.field.ptr;
        if (C_dialect == C_dialect_cplusplus) {
          put_access(fp->source_corresp.access);
        }  /* if */
        (void)sprintf(buffer, "offset");
        apo_sym = sym->variant.field.anonymous_parent_object;
        if (apo_sym != NULL) {
          (void)sprintf(&buffer[strlen(buffer)],
                        " (relative to anon parent obj)");
        }  /* if */
        (void)sprintf(&buffer[strlen(buffer)], " = %lu",
                      (unsigned long)fp->offset);
        if (fp->is_bit_field) {
          (void)sprintf(&buffer[strlen(buffer)], "+%d",
                        (int)fp->offset_bit_remainder);
          put_string(buffer);
          (void)sprintf(buffer, "size = %d bit%s", (int)fp->bit_size,
                        fp->bit_size == 1 ? "" : "s");
        }  /* if */
        put_string(buffer);
        if (fp->is_anonymous_parent_object) {
          put_string("is anon parent object");
        }  /* if */
        type = sym->variant.field.ptr->type;
      }  /* if */
      break;
    case sk_label:
      break;
    case sk_static_data_member:
      var = sym->variant.static_data_member.variable;
      goto do_variable;
    case sk_variable:
      var = sym->variant.variable.ptr;
do_variable:
      if (var == NULL) {
        put_string("<null>");
      } else {
        if (sym->kind == (a_symbol_kind)sk_static_data_member) {
          put_access(var->source_corresp.access);
        }  /* if */
        (void)sprintf(buffer, "sc_%s",
	              db_storage_class_names[(int)var->storage_class]);
        put_string(buffer);
        (void)str_name_linkage(buffer, &(var->source_corresp));
        put_string(buffer);
        if (sym->kind == (a_symbol_kind)sk_static_data_member) {
          if (var->is_template_static_data_member) put_string("is instance");
        } else {
          if (sym->variant.variable.value_has_been_set) put_string("set");
          if (sym->variant.variable.used) put_string("used");
          if (var->is_parameter || var->is_handler_param) {
            put_string(var->is_parameter ? "is param" : "is handler param");
            if (var->param_value_has_been_changed) put_string("changed");
            if (var->param_used_more_than_once) put_string("multiply used");
          }  /* if */
        }  /* if */
        if (var->is_anonymous_parent_object) {
          put_string("is anon parent object");
        }  /* if */
        type = var->type;
      }  /* if */
      break;
    case sk_member_function:
    case sk_routine:
      rp = sym->variant.routine.ptr;
      if (rp == NULL) {
        put_string("<null>");
      } else {
        if (sym->kind == (a_symbol_kind)sk_member_function) {
          put_access(rp->source_corresp.access);
          if (rp->is_virtual) {
            (void)sprintf(buffer, "virtual (%d)", rp->virtual_function_number);
            put_string(buffer);
          }  /* if */
          if (rp->special_kind != (a_special_function_kind)sfk_none) {
            put_string(db_special_function_kinds[rp->special_kind]);
          }  /* if */
        }  /* if */
        if (rp->is_inline) put_string("inline");
        if (rp->compiler_generated) put_string("compiler generated");
        (void)sprintf(buffer, "sc_%s",
                      db_storage_class_names[(int)rp->storage_class]);
        put_string(buffer);
        (void)str_name_linkage(buffer, &(rp->source_corresp));
        put_string(buffer);
        if (rp->is_template_function) put_string("is instance");
        type = rp->type;
        if (C_dialect == C_dialect_cplusplus) {
          an_exception_specification_ptr       esp;
          an_exception_specification_type_ptr  estp;

          esp = (skip_typerefs(type))->variant.routine.extra_info->
                                                      exception_specification;
          if (esp == NULL) {
            if (exceptions_enabled) put_string("throws any");
          } else if (esp->exception_specification_type_list == NULL) {
            put_string("throws none");
          } else {
            estp = esp->exception_specification_type_list;
            (void)sprintf(buffer, "throws (");
            str_type(&buffer[strlen(buffer)], estp->type);
            for (estp = estp->next; estp != NULL; estp = estp->next) {
              put_string(buffer);
              buffer[0] = 0;
              str_type(buffer, estp->type);
            }  /* for */
            (void)sprintf(&buffer[strlen(buffer)], ")");
            put_string(buffer);
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case sk_projection:
      put_access(sym->variant.projection.access);
      if (sym->variant.projection.ambiguous) put_string("ambig");
      if (sym->variant.projection.access_adjustment_made) {
        put_string("access decl");
      }  /* if */
      if (sym->variant.projection.intervening_access_adjustment) {
        put_string("intervening access decl");
      }  /* if */
      { a_projection_descr_ptr pdp = sym->variant.projection.extra_info;
        if (pdp->fundamental_base_class != NULL &&
            pdp->fundamental_base_class->derivation != NULL) {
          a_derivation_step_ptr  step;
          step = pdp->fundamental_base_class->derivation->path;
          if (pdp->fundamental_base_class->is_virtual) {
            /* For virtual base classes show just the last "hop". */
            while (step->next != NULL) step = step->next;
          }  /* if */
          put_string(str_path(buffer, step, "path = ==>", "==>"));
        }  /* if */
      }
      break;
    case sk_overloaded_function:
      if (sym->variant.overloaded_function.mixed_static_nonstatic) {
        put_string ("mixed static/nonstatic");
      }  /* if */
      if (sym->variant.overloaded_function.symbols == NULL) {
        put_string("func symbols = <null>");
      } else {
        a_symbol_ptr  rtn_sym = sym->variant.overloaded_function.symbols;
        put_string("func symbols =\n");
        for (; rtn_sym != NULL; rtn_sym = rtn_sym->next) {
          fprintf(f_debug, "%*s", indentation, "");
          db_symbol(rtn_sym, "", indentation + 2);
        }  /* for */
        col = 0;
        suppress_newline = TRUE;
      }  /* if */
      break;
    case sk_class_template:
    case sk_function_template:
      {
        a_template_symbol_supplement_ptr  tssp;
        a_template_param_ptr              tplep;
        a_symbol_ptr                      inst_sym, mft_sym;

        tssp = sym->variant.template_info;
        if (tssp->token_cache.first_token != NULL) {
          put_string("template body cached");
        }  /* if */
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          if (tssp->variant.function.cannot_be_called) {
            put_string("cannot be called");
          }  /* if */
        } else if (sym->kind == (a_symbol_kind)sk_class_template) {
          switch (tssp->variant.class_template.type_kind) {
            case tk_class:  put_string("class");           break;
            case tk_struct: put_string("struct");          break;
            case tk_union:  put_string("union");           break;
            case tk_error:  put_string("no type kind");    break;
            default:        put_string("<BAD TYPE KIND>"); break;
          }  /* switch */
        }  /* if */
        /* Output information from the template symbol supplement. */
        put_string("template parameters =\n");
        for (tplep = tssp->parameters; tplep != NULL; tplep = tplep->next) {
          fprintf(f_debug, "%*s", indentation + 2, "");
          db_symbol(tplep->param_symbol, "", indentation + 4);
          switch (tplep->param_symbol->kind) {
            case sk_type:
              fprintf(f_debug, "%*sparameter type: ", indentation + 4, "");
              /* Display the proxy class type if one exists. */
              if (tplep->variant.param_type != NULL) {
                a_type_ptr type = tplep->variant.param_type;
                db_type(type);
                if (type->variant.template_param.descr != NULL) {
                  a_type_ptr  class_type;
                  class_type = type->variant.template_param.descr->class_type;
                  if (class_type != NULL) {
                    fprintf(f_debug, "\n%*sproxy class: ",
                            indentation + 6, "");
                    db_type(class_type);
                  }  /* if */
                }  /* if */
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              break;
            case sk_constant:
              fprintf(f_debug, "%*sparameter constant: ", indentation + 4, "");
              if (tplep->variant.param_constant.ptr != NULL) {
                db_constant(tplep->variant.param_constant.ptr);
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              if (tplep->variant.param_constant.has_default_arg) {
		if (!tplep->
			variant.param_constant.type_involves_template_param &&
                    !tplep->
                     variant.param_constant.constant_involves_template_param) {
		  put_string("= ");
		  db_constant(tplep->variant.param_constant.
							default_arg.constant);
		} else {
		  put_string("= <token cache>");
		}  /* if */
	      }  /* if */
              break;
            default:
              fprintf(f_debug, "<BAD TEMPLATE PARAM SYMBOL KIND>");
          }  /* if */
          fprintf(f_debug, "\n");
          col = 0;
        }  /* for */
        if (sym->kind == (a_symbol_kind)sk_class_template) {
          inst_sym = tssp->variant.class_template.instantiations;
          while (inst_sym != NULL) {
            fprintf(f_debug, "%*sinstantiation:\n", indentation, "");
            fprintf(f_debug, "%*s", indentation + 2, "");
            db_symbol(inst_sym, "", indentation + 4);
            inst_sym = inst_sym->next;
          }  /* while */
          mft_sym = tssp->variant.class_template.member_function_templates;
          while (mft_sym != NULL) {
            fprintf(f_debug, "%*smember function template:\n",
                    indentation, "");
            fprintf(f_debug, "%*s", indentation + 2, "");
            db_symbol(mft_sym, "", indentation + 4);
            mft_sym = mft_sym->next;
          }  /* while */
        } else if (sym->kind == (a_symbol_kind)sk_function_template) {
          a_routine_ptr            routine = tssp->variant.function.routine;
          a_template_instance_ptr  tip;

          fprintf(f_debug, "%*sroutine type: ", indentation, "");
          if (routine != NULL) {
            db_type(tssp->variant.function.routine->type);
          } else {
            fprintf(f_debug, "(routine ptr is NULL)");
          }  /* if */
          fprintf(f_debug, "\n");
          if (tssp->variant.function.class_declared_in != NULL) {
            fprintf(f_debug, "%*sclass_declared_in: ", indentation, "");
            db_type(tssp->variant.function.class_declared_in);
            fprintf(f_debug, "\n");
          }  /* if */
          tip = tssp->variant.function.instantiations;
          while (tip != NULL) {
            fprintf(f_debug, "%*sinstantiation", indentation, "");
            if (tip->instantiation_required || tip->specific_decl ||
                tip->specific_def) {
              char* comma = "";
              fputs(" (", f_debug);
              if (tip->instantiation_required) {
                fputs("instantiation req'd", f_debug);
                comma = ", ";
              }  /* if */
              if (tip->specific_decl) {
                fprintf(f_debug, "%sspecific decl", comma);
                comma = ", ";
              }  /* if */
              if (tip->specific_def) {
                fprintf(f_debug, "%sspecific def", comma);
              }  /* if */
              fputc(')', f_debug);
            }  /* if */
            fputs(":\n", f_debug);
            fprintf(f_debug, "%*s", indentation + 2, "");
            db_symbol(tip->instance_sym, "", indentation + 4);
            tip = tip->next;
          }  /* while */
        }  /* if */
        col = 0;
        suppress_newline = TRUE;
      }
      break;
#if CHECKING
    default:
      put_string("UNEXPECTED SYMBOL KIND");
      break;
#endif /* CHECKING */
  }  /* switch */
  if (type != NULL) {
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
    if (type->use_cfront_transitional_nested_type_name_mangling) {
      put_string("semivisible");
    }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
    if (!space_left(20) ||
        (!space_left(30) && is_array_type(type)) || is_function_type(type) ||
        ((is_pointer_type(type) || is_reference_type(type)) &&
	 ((is_array_type(type->variant.pointer.type) && !space_left(35)) ||
	  is_function_type(type->variant.pointer.type))) ||
        (!space_left(55) && is_template_class_type(type))) {
      fprintf(f_debug, ",\n%*stype = ", indentation, "");
    } else {
      fputs(", type = ", f_debug);
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
        sym->kind == (a_symbol_kind)sk_union_tag) {
      db_type(type);
    } else {
      db_abbreviated_type(type);
    }  /* if */
    suppress_newline = FALSE;
  }  /* if */
  if (apo_sym != NULL) {
    if (!suppress_newline) (void)fputc('\n', f_debug);
    fprintf(f_debug, "%*s", indentation, "");
    (void)sprintf(buffer, "- anon parent object [%lu]: ",
                  (unsigned long)apo_sym);
    db_symbol(apo_sym, buffer, indentation + 2);
    suppress_newline = TRUE;
  }  /* if */
  /* Recursive calls to db_symbol can create unwanted newlines in the
     output.  Don't output a newline if the last thing we did was
     a call to db_symbol. */
  if (!suppress_newline) (void)fputc('\n', f_debug);
  if (var != NULL) {
    db_initializer(var, indentation);
  }  /* if */
}  /* db_symbol */


int db_scope_kind(a_scope_kind sck)
/*
Put out a scope kind name (for debugging).
*/
{
  char	*s;

  switch (sck) {
    case sck_file:                   s = "file";                     break;
    case sck_func_prototype:         s = "function prototype";       break;
    case sck_block:                  s = "block";                    break;
    case sck_class_struct_union:     s = "class/struct/union";       break;
    case sck_class_reactivation:     s = "class reactivation";       break;
    case sck_function:               s = "function";                 break;
    case sck_template_declaration:   s = "template declaration";     break;
    case sck_template_instantiation: s = "template instantiation";   break;
    case sck_pragma:		     s = "pragma";		     break;
    case sck_function_access:	     s = "function access";	     break;
    default:                         s = "***UNKNOWN SCOPE KIND***"; break;
  }  /* switch */
  fputs(s, f_debug);
  return strlen(s);
}  /* db_scope_kind */


void db_scope_stack(void)
/*
Dump the entire scope stack (for debugging).
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_boolean                done = FALSE;
  int                      len;

  do {
    fprintf(f_debug, "%s%3d ",
            (ssep == &scope_stack[decl_scope_level]) ? "**" : "  ",
            ssep->number);
    len = db_scope_kind(ssep->kind);
    fprintf(f_debug, "%-*s", 25-len, "");
    switch (ssep->kind) {
      case sck_function:
        if (ssep->il_scope == NULL) {
          fprintf(f_debug, "null IL scope");
        } else {
          db_name(&ssep->il_scope->variant.routine.ptr->source_corresp);
        }  /* if */
        break;
      case sck_file:
        done = TRUE;
      case sck_block:
        if (ssep->il_scope == NULL) {
          fprintf(f_debug, "null IL scope");
        }  /* if */
        break;
      case sck_class_struct_union:
      case sck_class_reactivation:
        db_abbreviated_type(ssep->assoc_type);
        break;
      case sck_template_instantiation:
        if (ssep->template_sym == NULL) {
          fputs("<null template symbol>", f_debug);
        } else {
          char* s;
          switch (ssep->template_sym->kind) {
            case sk_class_template:      s = "<class-template>";    break;
            case sk_function_template:   s = "<function-template>"; break;
            case sk_static_data_member:  s = "<static-data-member-template>";
                                                                    break;
            default:                     s = "<BAD SYMBOL KIND>";   break;
          }  /* switch */
          fprintf(f_debug, "%s %s", s, ssep->template_sym->header->identifier);
        }  /* if */
        break;
      case sck_template_declaration:
      case sck_func_prototype:
      default:;
    }  /* switch */
    fputs("\n", f_debug);
    --ssep;
  } while (!done);
}  /* db_scope_stack */

#endif /* DEBUG */


a_scope_depth scope_depth_of(a_symbol_ptr  sym,
                             a_boolean     *is_local_to_function)
/*
Given a symbol with a decl_scope (which is a scope number), search the
scope stack for the scope stack entry that corresponds to it, and return
the depth.  Also return TRUE in *is_local_to_function if the declaration
is in within a function body.
*/
{
  a_scope_depth  scope_depth;

  if (sym->decl_scope == FILE_SCOPE_NUMBER) {
    /* Leave the is_local_to_function flag FALSE. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else if (sym->decl_scope == NO_SCOPE_NUMBER) {
    /* Leave the is_local_to_function flag FALSE. */
    /* Some entities (e.g., macros) have no decl_scope number. */
    scope_depth = NO_SCOPE_DEPTH;
  } else if (sym->decl_scope == scope_stack[decl_scope_level].number) {
    /* The normal case is when the current decl_scope_level corresponds to
       what's in the symbol.  Use the global variables. */
    if (depth_innermost_function_scope != NO_SCOPE_NUMBER ||
        inside_local_class) {
      *is_local_to_function = TRUE;
    }  /* if */
    scope_depth = decl_scope_level;
  } else {
    /* In certain unusual cases (e.g., when an entity is first seen in a
       friend declaration) it is necessary to compute the scope depth by
       running through the scope stack. */
    for (scope_depth = depth_scope_stack; ; --scope_depth) {
      if (scope_depth < DEPTH_OF_FILE_SCOPE) {
        scope_depth = NO_SCOPE_DEPTH;
        break;
      } else if (scope_stack[scope_depth].number == sym->decl_scope) {
        /* This is the scope stack entry corresponding to the declaration
           scope number, where relevant characteristics of the scope are
           recorded. */
        if (scope_stack[scope_depth].depth_innermost_function_scope !=
                                                           NO_SCOPE_NUMBER ||
            scope_stack[scope_depth].inside_local_class) {
          *is_local_to_function = TRUE;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return scope_depth;
}  /* scope_depth_of */


void set_source_corresp(a_source_correspondence *sc,
                        a_symbol_ptr            sp)
/*
Set the given source correspondence to point to the given symbol.  The
scope for the symbol must still be active.
*/
{
  a_boolean  is_local_to_function = FALSE;

  sc->assoc_info = (char *)sp;
  /* Note that the identifier name was allocated in the intermediate language
     memory area (see find_symbol); it can therefore be used without
     copying. */
  sc->name = sp->header->identifier;
  if (sc->decl_position.seq != 0) {
    /* The decl-position is already set, so this must be a resetting of
       the source correspondence.  Let the caller decide whether and how the
       current value should be overwritten. */
  } else {
    /* Set the source position. */
    sc->decl_position = sp->decl_position;
  }  /* if */
  /* Clear the referenced flag.  It was set to TRUE in
     set_default_source_corresp, so that unassociated entities will
     all have the referenced flag set.  Here it's cleared, now that we
     know this is an entity with some corresponding entity in the
     source program.  The flag will later be set to TRUE again if
     there is an actual reference. */
  sc->referenced = FALSE;
#if RECORD_SCOPE_DEPTH_IN_IL
  /* Record the scope depth of the declaration of this entity in the source
     correspondence. */
  sc->scope_depth = scope_depth_of(sp, &is_local_to_function);
#else /* RECORD_SCOPE_DEPTH_IN_IL */
  (void)scope_depth_of(sp, &is_local_to_function);
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Set the is_local_to_function flag. */
  sc->is_local_to_function = is_local_to_function;
}  /* set_source_corresp */


#if !RECORD_SCOPE_DEPTH_IN_IL
/*ARGSUSED*/ /* <-- depth is only used when local entities are promoted. */
#endif /* !RECORD_SCOPE_DEPTH_IN_IL */
static void set_source_corresp_with_scope_depth(a_source_correspondence *sc,
			                        a_symbol_ptr            sp,
						a_scope_depth		depth)
/*
Set the source correspondence to point to a given symbol that for which
the scope is not still active.  This routine works by temporarily
changing the scope of the symbol to NO_SCOPE_NUMBER and calling
set_source_corresp.  The scope number is the set to its original value
and the scope depth is set to the value passed by the caller.
*/
{
  a_scope_depth		saved_scope_number;

  saved_scope_number = sp->decl_scope;
  sp->decl_scope = NO_SCOPE_NUMBER;
  set_source_corresp(sc, sp);
  sp->decl_scope = saved_scope_number;
#if RECORD_SCOPE_DEPTH_IN_IL
  sc->scope_depth = depth;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
}  /* set_source_corresp_with_scope_depth */


a_boolean is_special_function_symbol(a_symbol_ptr             sym,
                                     a_special_function_kind  kind)
/*
If sym is a routine symbol of some sort, return TRUE if the special function
kind recorded in its routine entry is "kind" and FALSE if it is not.  If sym
is not a routine symbol, return FALSE.
*/
{
  a_boolean  match;

  switch (sym->kind) {
    case sk_routine:
    case sk_member_function:
      match = (sym->variant.routine.ptr->special_kind == kind);
      break;
    case sk_overloaded_function:
      /* All entries on a list of overloaded functions should have the same
         special function kind, so looking at the first on the list is
         sufficient. */
      sym = sym->variant.overloaded_function.symbols;
      match = is_special_function_symbol(sym, kind);
      break;
    case sk_function_template:
      match = (sym->variant.template_info->
                        variant.function.routine->special_kind == kind);
      break;
    default:
      match = FALSE;
  }  /* switch */
  return match;
}  /* is_special_function_symbol */


static a_symbol_header_ptr alloc_symbol_header(void)
/*
Allocate a new symbol header, and return a pointer to it.
*/
{
  register a_symbol_header_ptr ptr;

  db_enter(5, "alloc_symbol_header");

  ptr = (a_symbol_header_ptr)alloc_fe(sizeof(a_symbol_header));
#if DEBUG
  num_symbol_headers_allocated++;
#endif /* DEBUG */
  ptr->next              = NULL;
  ptr->symbol            = NULL;
  ptr->inactive_symbols  = NULL;
  ptr->identifier        = NULL;
  ptr->identifier_length = 0;
  ptr->any_nested_types_on_inactive_list = FALSE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  ptr->has_cfront_transitional_nested_type_mangled_name = FALSE;
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */

  db_exit();

  return ptr;
}  /* alloc_symbol_header */


static a_conversion_header_ptr alloc_conversion_header(void)
/*
Allocate a new conversion header and return a pointer to it.
*/
{
  register a_conversion_header_ptr ptr;

  db_enter(5, "alloc_conversion_header");
  ptr = (a_conversion_header_ptr)alloc_fe(sizeof(a_conversion_header));
#if DEBUG
  num_conversion_headers_allocated++;
#endif /* DEBUG */
  ptr->next          = NULL;
  ptr->symbol_header = NULL;
  ptr->type          = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_conversion_header */


a_symbol_list_entry_ptr alloc_symbol_list_entry(void)
/*
Allocate a new conversion list entry and return a pointer to it.
*/
{
  register a_symbol_list_entry_ptr ptr;

  db_enter(5, "alloc_symbol_list_entry");
  ptr = (a_symbol_list_entry_ptr)alloc_fe(sizeof(a_symbol_list_entry));
#if DEBUG
  num_symbol_list_entries_allocated++;
#endif /* DEBUG */
  ptr->next    = NULL;
  ptr->symbol  = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_symbol_list_entry */


a_symbol_ptr find_symbol(char             *identifier,
			 sizeof_t         length,
			 a_symbol_locator *location)
/*
Look up a symbol in the table.  If the symbol header is not there, create one
and set the symbol locator to point to the header.  Note that the source
position in the locator is not changed; usually, it will have been set
by get_token when an identifier is scanned, but sometimes the caller may
have to set it directly.
*/
{
  register unsigned            hash_value = 0;
  register char                *ptr;
  register sizeof_t            a;
  register a_symbol_header_ptr hdr_ptr;
  a_symbol_header_ptr	       prev_hdr_ptr;
  a_symbol_ptr                 sym_ptr    = NULL;
  int                          bucket_number;

  db_enter(4, "find_symbol");
#if DEBUG
  num_searches_for_symbols++;
#endif /* DEBUG */

  /* Hash the symbol's identifier.  This involves taking the identifier's
     first, last, and middle 3 characters.  Of course, if the identifier has
     fewer than 5 characters, take the entire identifier. */
  if (length > 5) {
    ptr = identifier + (length >> 1) - 1;
    hash_value = (int)*identifier;
    hash_value = (hash_value * HASH_FACTOR) + (int)*(identifier + length - 1);
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr;
  } else {
    ptr = identifier;
    for (a = 0; a < length; a++) {
      hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    }  /* for */
  }  /* if */

  /* Look in the symbol bucket saving the position in case this symbol needs
     to be added. */
  bucket_number = hash_value % SYMBOL_TABLE_SIZE;
  if ((hdr_ptr = symbol_table[bucket_number]) != NULL) {
    prev_hdr_ptr = NULL;
    do {
#if DEBUG
      num_compares_for_symbols++;
#endif /* DEBUG */
      if (length == hdr_ptr->identifier_length) {
        if (memcmp(identifier, hdr_ptr->identifier, size_t_arg(length)) == 0) {
	  /* Have a match. */
	  sym_ptr = hdr_ptr->symbol;
          /* Relink the symbol header at the front of the list of headers,
             so that frequently-used headers will be found quickly. */
          if (prev_hdr_ptr != NULL) {
            prev_hdr_ptr->next = hdr_ptr->next;
            hdr_ptr->next = symbol_table[bucket_number];
            symbol_table[bucket_number] = hdr_ptr;
          }  /* if */
	  goto symbol_found;
        }  /* if */
      }  /* if */
      prev_hdr_ptr = hdr_ptr;
    } while ((hdr_ptr = hdr_ptr->next) != NULL);
  }  /* if */

  /* Exiting this loop indicates that the symbol does not exist in the table;
     allocate a symbol header for it. */
  hdr_ptr = alloc_symbol_header();
#if DEBUG
  num_symbol_headers_in_hash_table++;
#endif /* DEBUG */

  /* Link the new header onto the front of the appropriate bucket of the symbol
     table. */
#if DEBUG
  if (symbol_table[bucket_number] == NULL) num_used_symbol_buckets++;
#endif /* DEBUG */
  hdr_ptr->next = symbol_table[bucket_number];
  symbol_table[bucket_number] = hdr_ptr;

  /* Allocate the identifier string.  It is allocated in the intermediate
     language memory region because it must be passed to the back end. */
  hdr_ptr->identifier = alloc_il((sizeof_t)(length + 1));
#if DEBUG
  symbol_name_string_space += length+1;
#endif /* DEBUG */
  (void)memcpy(hdr_ptr->identifier, identifier, size_t_arg(length));

  /* Terminate the string and set the length. */
  hdr_ptr->identifier[length] = '\0';
  hdr_ptr->identifier_length = length;

  /* There is no symbol. */
  sym_ptr = NULL;

symbol_found:
  location->symbol_header = hdr_ptr;

  db_exit();

  return sym_ptr;
}  /* find_symbol */


void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                             a_symbol_locator *location)
/*
Create in *location a locator for the symbol pointed to by sym_ptr.
*/
{
  clear_locator(location, &sym_ptr->decl_position);
  location->symbol_header = sym_ptr->header;
  location->specific_symbol = sym_ptr;
  location->is_error = sym_ptr->is_error;
}  /* make_locator_for_symbol */


void make_specific_symbol_error_locator(a_symbol_locator *locator)
/*
Make a specific symbol error locator in *locator.  This identifies a
specific symbol which is an error symbol.
*/
{
  a_symbol_header_ptr  hdr_ptr = locator->symbol_header;

  clear_locator(locator, &error_position);
  locator->symbol_header = hdr_ptr;
  locator->is_error = TRUE;
  locator->do_not_clear_specific_symbol = TRUE;
  locator->specific_symbol = enter_symbol((a_symbol_kind)sk_undefined,
                                          locator,
                                          DEPTH_OF_FILE_SCOPE,
                                          /*suppress_error=*/TRUE);
}  /* make_specific_symbol_error_locator */


a_template_symbol_supplement_ptr alloc_template_symbol_supplement(
                                                          a_symbol_kind  kind)
/*
Allocate a new template symbol supplement entry, initialize its fields
appropriately (based on the kind of symbol with which it will be associated),
and return a pointer to it.
*/
{
  a_template_symbol_supplement_ptr  tssp;

  db_enter(5, "alloc_template_symbol_supplement");
  /* Allocate a template symbol supplement. */
  tssp = (a_template_symbol_supplement_ptr)
                   alloc_fe(sizeof(a_template_symbol_supplement));
#if DEBUG
  num_template_symbol_supplements_allocated++;
#endif /* DEBUG */
  /* Initialize its fields. */
  tssp->parameters = NULL;
  tssp->declaration_scope = NO_SCOPE_NUMBER;
  tssp->pending_instantiations = 0;
  tssp->pragmas_bound_to_template = NULL;
  tssp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  clear_token_cache(&tssp->token_cache, /*reusable=*/TRUE);
  tssp->befriending_classes = NULL;
  switch (kind) {
    case sk_class_template:
      tssp->variant.class_template.instantiations = NULL;
      tssp->variant.class_template.type_kind = (a_type_kind)tk_error;
      tssp->variant.class_template.member_function_templates = NULL;
      tssp->variant.class_template.prototype_instantiation = NULL;
      tssp->variant.class_template.prototype_instantiation_complete = FALSE;
#if CHECKING 
      tssp->variant.class_template.dummy = FALSE;
#endif /* CHECKING */
      break;
    case sk_function_template:
    case sk_member_function:
      tssp->variant.function.instantiations = NULL;
      tssp->variant.function.routine = NULL;
      clear_func_info(&tssp->variant.function.func_info);
      tssp->variant.function.def_arg_expr_list = NULL;
      clear_token_cache(&tssp->variant.function.decl_token_cache,
                        /*reusable=*/TRUE);
      tssp->variant.function.cannot_be_called = FALSE;
      tssp->variant.function.unused_instantiations = 0;
      tssp->variant.function.class_declared_in = NULL;
#if CHECKING
      tssp->variant.function.dummy = FALSE;
#endif /* CHECKING */
      break;
    case sk_static_data_member:
      tssp->variant.static_data_member.definitions = NULL;
      break;
#if CHECKING
    default:
      internal_error("alloc_template_symbol_supplement: bad symbol kind");
#endif /* CHECKING */
  }  /* switch */

  db_exit();
  return tssp;
}  /* alloc_template_symbol_supplement */


void set_symbol_kind(register a_symbol_ptr sym_ptr,
		     a_symbol_kind         sym_kind)
/*
Set the symbol's kind and initialize the associated variant fields to a safe
state.
*/
{
  db_enter(5, "set_symbol_kind");

  sym_ptr->kind = sym_kind;
  switch (sym_kind) {
    case sk_undefined:
      /* No variant fields to set. */
      break;
    case sk_keyword:
      sym_ptr->variant.keyword.token = tok_error;
      sym_ptr->variant.keyword.unimplemented_diagnostic_issued = FALSE;
      break;
    case sk_macro:
      sym_ptr->variant.macro_def = NULL;
      break;
    case sk_constant:
      sym_ptr->variant.constant = NULL;
      break;
    case sk_type:
    case sk_enum_tag:
      sym_ptr->variant.type = NULL;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      sym_ptr->variant.class_struct_union.type = NULL;
      { a_class_symbol_supplement  *cssp;
        cssp = (a_class_symbol_supplement_ptr)alloc_fe(
                                            sizeof(a_class_symbol_supplement));
#if DEBUG
        num_class_symbol_supplements_allocated++;
#endif /* DEBUG */
        sym_ptr->variant.class_struct_union.extra_info = cssp;
        cssp->symbols = NULL;
        cssp->constructor = NULL;
        cssp->destructor = NULL;
        cssp->assignment_operator = NULL;
        cssp->conversion_list = NULL;
        cssp->routine_fixup_list = NULL;
        cssp->class_template = NULL;
        cssp->member_decl_scope = NO_SCOPE_NUMBER;
        cssp->template_param_for_proxy_class = NULL;
        cssp->corresp_prototype_sym = NULL;
        cssp->prototype_token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
        cssp->dependent_type_fixup_list = NULL;
        cssp->constructor_required = FALSE;
        cssp->destructor_required = FALSE;
        cssp->has_default_constructor = FALSE;
        cssp->has_copy_constructor = FALSE;
        cssp->has_copy_constructor_for_const_object = FALSE;
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
        cssp->construction_by_bitwise_copy_allowed = FALSE;
        cssp->target_of_conversion_function = FALSE;
        cssp->any_ref_member = FALSE;
        cssp->any_nested_classes = FALSE;
        /* The is_class_aggregate flag is initialized to TRUE when we are not
           in C++ mode. */
        cssp->is_class_aggregate = (C_dialect != C_dialect_cplusplus);
        cssp->has_operator_new = FALSE;
        cssp->has_operator_delete = FALSE;
        cssp->is_nonreal_class = FALSE;
        cssp->is_prototype_instantiation = FALSE;
        cssp->is_specific_template_def = FALSE;
        cssp->any_nonstatic_data_members = FALSE;
        cssp->any_nonreal_base_classes = FALSE;
      }
      break;
    case sk_variable:
      sym_ptr->variant.variable.ptr = NULL;
      sym_ptr->variant.variable.value_has_been_set = FALSE;
      sym_ptr->variant.variable.used = FALSE;
      break;
    case sk_static_data_member:
      sym_ptr->variant.static_data_member.variable = NULL;
      sym_ptr->variant.static_data_member.instance_ptr = NULL;
      break;
    case sk_field:
      sym_ptr->variant.field.ptr = NULL;
      sym_ptr->variant.field.anonymous_parent_object = NULL;
      break;
    case sk_routine:
    case sk_member_function:
      sym_ptr->variant.routine.ptr = NULL;
      sym_ptr->variant.routine.instance_ptr = NULL;
      break;
    case sk_label:
      sym_ptr->variant.label.ptr = NULL;
      sym_ptr->variant.label.assoc_control_flow_descr = NULL;
      break;
    case sk_extern_variable:
    case sk_extern_routine:
      { an_extern_symbol_descr_ptr esdp;
        esdp = (an_extern_symbol_descr_ptr)alloc_fe(
                                               sizeof(an_extern_symbol_descr));
#if DEBUG
        num_extern_symbol_descrs_allocated++;
#endif /* DEBUG */
        sym_ptr->variant.extern_symbol_descr = esdp;
        esdp->type = NULL;
        esdp->variant.variable = NULL;  /* Clears routine too. */
      }
      break;
    case sk_projection:
      { a_projection_descr *pdp;
        pdp = (a_projection_descr_ptr)alloc_fe(sizeof(a_projection_descr));
#if DEBUG
        num_projection_descrs_allocated++;
#endif /* DEBUG */
        pdp->fundamental_symbol     = NULL;
        pdp->fundamental_base_class = NULL;
        sym_ptr->variant.projection.extra_info= pdp;
        sym_ptr->variant.projection.access    = (an_access_specifier)as_public;
        sym_ptr->variant.projection.ambiguous = FALSE;
        sym_ptr->variant.projection.access_adjustment_made = FALSE;
        sym_ptr->variant.projection.intervening_access_adjustment = FALSE;
      }
      break;
    case sk_overloaded_function:
      sym_ptr->variant.overloaded_function.symbols = NULL;
      sym_ptr->variant.overloaded_function.mixed_static_nonstatic = FALSE;
      break;
    case sk_parameter:
      sym_ptr->variant.param_id = NULL;
      break;
    case sk_class_template:
    case sk_function_template:
      sym_ptr->variant.template_info =
                             alloc_template_symbol_supplement(sym_ptr->kind);
      break;
#if CHECKING
    default:
      internal_error("set_symbol_kind: bad symbol kind");
#endif /* CHECKING */
  }  /* switch */

  db_exit();
}  /* set_symbol_kind */


static a_symbol_ptr alloc_symbol(a_symbol_kind       kind,
                                 a_symbol_header_ptr hdr_ptr,
                                 a_source_position   *position)
/*
Allocate a symbol and initialize the fields to a safe state.  Set the
kind to kind, the header to hdr_ptr, and the decl_position to *position.
hdr_ptr == NULL indicates that an error symbol should be constructed.
*/
{
  register a_symbol_ptr sym_ptr;

  db_enter(5, "alloc_symbol");

  sym_ptr = (a_symbol_ptr)alloc_fe(sizeof(a_symbol));
#if DEBUG
  num_symbols_allocated++;
#endif /* DEBUG */
  /* Set the shared fields to default values, set the kind, and initialize
     its variant fields. */
  clear_symbol(sym_ptr, kind);
  /* Set the header. */
  if (hdr_ptr == NULL) {
    /* Use the error symbol header.  Allocate it if necessary. */
    if (error_symbol_header == NULL) {
      error_symbol_header = alloc_symbol_header();
      error_symbol_header->identifier = "<error>";
      error_symbol_header->identifier_length = 7;
    }  /* if */
    hdr_ptr = error_symbol_header;
  }  /* if */
  sym_ptr->header = hdr_ptr;
  /* Set the declaration source position. */
  sym_ptr->decl_position = *position;
  db_exit();
  return sym_ptr;
}  /* alloc_symbol */


static void unlink_symbol_from_symbol_table(a_symbol_ptr sym_ptr)
/*
Remove a symbol from the symbol table, i.e., unlink it from its header's
list.
*/
{
  register a_symbol_ptr        ptr, prev_ptr;
  register a_symbol_header_ptr hdr_ptr;

  db_enter(4, "unlink_symbol_from_symbol_table");
#if DEBUG
  if (db_active && 
      (debug_level >= 3 ||
       db_flag_is_set("dump_symbols"))) {
    db_symbol(sym_ptr, "unlinking: ", 2);
  }  /* if */
#endif /* CHECKING */
  if (sym_ptr->is_error) {
    /* Error symbols are never added to a symbol list and cannot be removed. */
  } else {
    hdr_ptr = sym_ptr->header;
    prev_ptr = hdr_ptr->symbol;
    if (sym_ptr == prev_ptr) {
      /* The symbol is the first on the header list. */
      hdr_ptr->symbol = sym_ptr->next;
    } else {
      /* The symbol is not the first on the list.  Find the previous symbol
         on the list. */
      for (; (ptr = prev_ptr->next) != sym_ptr; prev_ptr = ptr) {
#if CHECKING
        if (ptr == NULL) {
#if DEBUG
          if (debug_level > 0) {
            fprintf(f_debug, "Symbol name = %s\n", hdr_ptr->identifier);
          }  /* if */
#endif /* DEBUG */
          internal_error(
                  "unlink_symbol_from_symbol_table: cannot find symbol entry");
        }  /* if */
#endif /* CHECKING */
      }  /* for */
      /* Found the entry; unlink it. */
      prev_ptr->next = sym_ptr->next;
    }  /* if */
  }  /* if */
  sym_ptr->next = NULL;
  db_exit();
}  /* unlink_symbol_from_symbol_table */


static void remove_symbol_from_scope_list(a_symbol_ptr sym_ptr)
/*
Remove the given symbol from the list of symbols for its scope.
*/
{
  register a_symbol_ptr   ptr, prev_ptr;
  a_scope_stack_entry_ptr ssep;

  if (sym_ptr->is_error) {
    /* Error symbols are not on the scope list and cannot be removed. */
  } else if (sym_ptr->decl_scope == NO_SCOPE_NUMBER) {
    /* Symbols removed by a command-line -U option can be outside of any
       scope. */
  } else {
    /* Find the proper entry in the scope stack (it will almost always be
       the topmost entry). */
#if CHECKING
    if (depth_scope_stack < 0) {
      internal_error("remove_symbol_from_scope_list: empty scope stack");
    }  /* if */
#endif /* CHECKING */
    for (ssep = &scope_stack[depth_scope_stack];
         sym_ptr->decl_scope != ssep->number;
         ssep--) {
#if CHECKING
      if (ssep == &scope_stack[0]) {
#if DEBUG
        if (debug_level > 0) {
          fprintf(f_debug, "Symbol name = %s\n", sym_ptr->header->identifier);
        }  /* if */
#endif /* DEBUG */
        internal_error("remove_symbol_from_scope_list: bad scope");
      }  /* if */
#endif /* CHECKING */
    }  /* for */
    /* Usually (i.e., when this routine is called from pop_scope), the
       symbol will be the first on the list.  If not, we have to find the
       previous symbol. */
    if (sym_ptr == ssep->symbols) {
      ssep->symbols = sym_ptr->next_in_scope;
      prev_ptr = NULL;
    } else {
      for (prev_ptr = ssep->symbols;
           (ptr = prev_ptr->next_in_scope) != sym_ptr;
           prev_ptr = ptr) {
#if CHECKING
        if (ptr == NULL) {
#if DEBUG
          if (debug_level > 0) {
            fprintf(f_debug, "Symbol name = %s\n",
                             sym_ptr->header->identifier);
          }  /* if */
#endif /* DEBUG */
          internal_error(
                       "remove_symbol_from_scope_list: could not find symbol");
        }  /* if */
#endif /* CHECKING */
      }  /* for */
      prev_ptr->next_in_scope = sym_ptr->next_in_scope;
    }  /* if */
    /* If the removed entry is the last entry on the list, update the
       last-pointer. */
    if (sym_ptr == ssep->last_symbol) ssep->last_symbol = prev_ptr;
  }  /* if */
  sym_ptr->next_in_scope = NULL;
}  /* remove_symbol_from_scope_list */


void remove_symbol(a_symbol_ptr sym_ptr)
/*
Unlink a symbol from the symbol table and remove it from the list of symbols
for the scope it's in.
*/
{
  db_enter(4, "remove_symbol");
  /* Remove the symbol from the symbol table. */
  unlink_symbol_from_symbol_table(sym_ptr);
  /* Remove the symbol from the list of symbols declared in its scope. */
  remove_symbol_from_scope_list(sym_ptr);
  db_exit();
}  /* remove_symbol */


void remove_anonymous_union_member_from_inactive_symbols_list
                                                       (a_symbol_ptr sym_ptr)
/*
Remove the indicated symbol from its header's inactive symbol list.
This is used in removing symbols inside unnamed unions before re-entering
them up one level.
*/
{
  a_symbol_header_ptr hdr_ptr = sym_ptr->header;
  a_symbol_ptr        prev_sym;

  db_enter(4, "remove_anonymous_union_member_from_inactive_symbol_list");
  
  if (sym_ptr == hdr_ptr->inactive_symbols) {
    /* The symbol is the first one on the list. */
    hdr_ptr->inactive_symbols = sym_ptr->next;
  } else {
    /* Find the previous entry on the list. */
    for (prev_sym = hdr_ptr->inactive_symbols;
         prev_sym->next != sym_ptr;
         prev_sym = prev_sym->next) {
         check_assertion_str(prev_sym->next != NULL,
                             "remove_anonymous_union...: symbol_not_found");
    }  /* for */
    prev_sym->next = sym_ptr->next;
  }  /* if */
  sym_ptr->next = NULL;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  if (cfront_2_1_mode) {
    /* If the is a type symbol that has previously been designated as the
       symbol receiving special transitional nested type name mangling, then
       reset that flag now that it has been promoted to another scope.  The
       flag will be restored, if appropriate, when this symbol is popped
       from the scope in which it will be reentered. */
    if (sym_ptr->header->has_cfront_transitional_nested_type_mangled_name) {
      if (is_type_symbol(sym_ptr)) {
        a_type_ptr	tp = type_symbol_type(sym_ptr);
        if (tp->use_cfront_transitional_nested_type_name_mangling) {
          tp->use_cfront_transitional_nested_type_name_mangling = FALSE;
          sym_ptr->header->has_cfront_transitional_nested_type_mangled_name
                                                                      = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  db_exit();
}  /* remove_anonymous_union_member_from_inactive_symbols_list */

#if RECORD_HIDDEN_NAMES_IN_IL

void record_defeatable_name_hiding(a_symbol_ptr  hidden_sym,
                                   a_boolean     tag_hidden_by_nontag,
                                   a_scope_ptr   sp)
/*
hidden_sym is a symbol for an entity that is hidden by another declaration
of the same name -- but the hiding can be "defeated" by using an
elaborated type specifier or global qualification (preceding "::") when
referring to the hidden name.  Create the hidden-name entity to represent
this case and add it to the list for the current scope.
*/
{
  a_hidden_name_ptr        hnp;
  a_scope_stack_entry_ptr  ssep;
  char                     *entity;
  an_il_entry_kind         kind;

  if (depth_template_declaration_scope == NO_SCOPE_DEPTH) {
    switch (hidden_sym->kind) {
      case sk_label:
      case sk_keyword:
      case sk_macro:
      case sk_undefined:
      case sk_extern_variable:
      case sk_extern_routine:
        /* Not in the same name space. */
        break;
      case sk_projection:
        /* Ignore projection symbols. */
        break;
      case sk_overloaded_function:
        /* Enter members of an overload set separately. */
        for (hidden_sym = hidden_sym->variant.overloaded_function.symbols;
             hidden_sym != NULL;
             hidden_sym = hidden_sym->next) {
          record_defeatable_name_hiding(hidden_sym, tag_hidden_by_nontag, sp);
        }  /* for */
        break;
      case sk_class_template:
      case sk_function_template:
        /* Template support is not yet provided. */
        break;
      case sk_routine:
      case sk_member_function:
      case sk_static_data_member:
        if ((hidden_sym->kind == (a_symbol_kind)sk_static_data_member &&
             hidden_sym->variant.static_data_member.instance_ptr != NULL) ||
            hidden_sym->variant.routine.instance_ptr != NULL) {
          /* Template support is not yet provided. */
          break;
        }  /* if */
      default:
        /* The normal case.  First find the entity associated with the
           symbol. */
        entity = il_entry_for_symbol(hidden_sym, &kind);
        if (entity != NULL) {
          if (sp == NULL) {
            /* Get pointer to current scope entry. */
            ssep = &scope_stack[decl_scope_level];
            /* Create the IL scope if necessary (for block scopes). */
            sp = ensure_il_scope_exists(ssep);
            check_assertion_str(sp != NULL,
                               "record_defeatable_name_hiding: NULL IL scope");
          }  /* if */
          /* If there is already a hidden name entry for this entity in this
             scope, reuse it. */
          for (hnp = sp->hidden_names; hnp != NULL; hnp = hnp->next) {
            if (hnp->entity.ptr == entity) break;
          }  /* for */
          if (hnp == NULL) {
            /* No existing entry.  Allocate a new one. */
            hnp = alloc_hidden_name();
            hnp->entity.ptr = entity;
            hnp->entity.kind = (a_byte_il_entry_kind)kind;
            /* Add it to the start of the hiden_names list for the current
               scope. */
            hnp->next = sp->hidden_names;
            sp->hidden_names = hnp;
          }  /* if */
          /* Set the appropriate flag. */
          if (tag_hidden_by_nontag) {
            check_assertion(kind == (an_il_entry_kind)iek_type);
            hnp->elaborated_type_specifier_needed = TRUE;
          } else {
            check_assertion(in_file_scope(entity));
            hnp->global_qualification_needed = TRUE;
          }  /* if */
        }  /* if */
    }  /* switch */
  }  /* if */
}  /* record_defeatable_name_hiding */

#endif /* RECORD_HIDDEN_NAMES_IN_IL */

a_boolean symbols_may_coexist_in_curr_scope(a_symbol_ptr  old_sym,
                                            a_symbol_ptr  new_sym,
                                            a_symbol_ptr  *insert_sym,
					    a_boolean	  suppress_error)
/*
old_sym is a symbol that is already in the symbol table.  new_sym is
a newly created symbol that is about to be added or a symbol for which
a projection symbol will be created and added.  Return TRUE if the
old and new symbols can coexist in the same scope.  For example, in C++
a tag symbol and a nontype symbol may coexist on the symbol list for a
scope.  The tag symbol should follow the other in the list, so if the
new symbol is a tag symbol, it must be inserted after the old.
insert_sym points provides the location at which the new symbol
should be entered.  This is used to make sure that a nontype symbol
will be found instead of a type symbol when both exist.  *insert_sym
is set only if insert_sym is not NULL.

In pcc mode and in cfront compatibility mode local variables of a
function are allowed to hide function parameters.  A warning is
issued for this case, except if suppress_error is TRUE.  In modes where
this is not allowed, an error will be issued by the caller.
*/
{
  a_boolean  err = TRUE;

  if (!C_mode() && is_tag_symbol(fundamental_symbol_of(new_sym))) {
    /* New symbol is a tag symbol. */
    a_symbol_ptr fund_old_sym = fundamental_symbol_of(old_sym);
    if (!is_type_symbol(fund_old_sym) &&
        !is_class_template_symbol(fund_old_sym)) {
      /* The old symbol is a non-type name.  Be sure the new symbol
         inserted into the list after the old one. */
      err = FALSE;
      if (insert_sym != NULL) *insert_sym = old_sym;
    }  /* if */
  } else if (!C_mode() && is_tag_symbol(fundamental_symbol_of(old_sym))) {
    /* The old symbol is a tag symbol. */
    a_symbol_ptr fund_new_sym = fundamental_symbol_of(new_sym);
    if (!is_type_symbol(fund_new_sym) &&
        !is_class_template_symbol(fund_new_sym)) {
      /* The new one is not a type symbol or a class template name.  It
         will be placed at the front of the list automatically. */
      err = FALSE;
#if RECORD_HIDDEN_NAMES_IN_IL
      /* The current declaration hides a tag declaration in the current
         scope. */
      record_defeatable_name_hiding(old_sym, /*tag_hidden_by_nontag=*/TRUE,
                                    (a_scope_ptr)NULL);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    }  /* if */
  } else if ((cfront_2_1_mode || C_dialect == C_dialect_pcc) &&
             old_sym->kind == (a_symbol_kind)sk_variable &&
             old_sym->variant.variable.ptr->is_parameter &&
	     (new_sym->kind != (a_symbol_kind)sk_variable ||
              (new_sym->variant.variable.ptr == NULL ||
               !new_sym->variant.variable.ptr->is_parameter))) {
    /* The old symbol is a parameter and the new symbol not a
       parameter -- allowed in cfront and pcc modes.  Note that we
       test the variable pointer for being NULL before dereferencing it
       above and we also pass the identifier string to the warning routine
       rather than using the standard symbol name fill-in.  This is done
       because the variable pointer may not have been filled in at the
       time the symbol is entered. */
    err = FALSE;
    if (!suppress_error) {
      pos_st_warning(ec_decl_hides_function_parameter, &new_sym->decl_position,
		     new_sym->header->identifier);
    }  /* if */
  }  /* if */
  return !err;
}  /* symbols_may_coexist_in_curr_scope */


static a_boolean is_redeclared_template_param(a_symbol_ptr   sym)
/*
Look through the template parameters associated with the innermost
instantiation scope for a symbol whose header matches the header of sym.
Return TRUE if a match is found.
*/
{
  a_template_param_ptr	tpp;
  a_boolean		result = FALSE;

  tpp = scope_stack[depth_innermost_instantiation_scope].template_param_list;
  check_assertion(tpp != NULL);
  while (tpp != NULL && !result) {
    a_symbol_ptr  param_symbol = tpp->param_symbol;
    if (param_symbol->header == sym->header) {
      result = TRUE;
    }  /* if */
    tpp = tpp->next;
  }  /* while */
  return result;
}  /* is_redeclared_template_param */


static void link_symbol_into_symbol_table(a_symbol_ptr  sym_ptr,
                                          a_scope_depth scope_depth,
                                          a_boolean     suppress_error)
/*
Add the given symbol to the symbol table, i.e., link it into its header's
list.  Also check to see if there is a previous definition of the symbol
in the same scope, and issue an error in that case.  The error is suppressed
if suppress_error is TRUE.  scope_depth indicates the level in the scope
stack at which the symbol is being entered, which is needed to determine
the proper insert location.
*/
{
  register a_symbol_ptr        old_sym_ptr;
  register a_scope_number      scope_number;
  register a_name_space_kind   sym_name_space_kind;
  a_symbol_header_ptr          hdr_ptr = sym_ptr->header;
  a_symbol_ptr                 insert_after;
  a_scope_depth                curr_depth;
  a_boolean		       redeclared_template_param;

  if (sym_ptr->is_error) {
    /* Error symbols are never added to the symbol table. */
  } else {
#if CHECKING
    if (hdr_ptr == NULL || hdr_ptr == error_symbol_header) {
      internal_error("link_symbol_into_symbol_table: NULL or error header");
    }  /* if */
#endif /* CHECKING */
    insert_after = NULL;
    if (scope_depth == NO_SCOPE_DEPTH) {
      /* The symbol is being entered outside of any scope; this happens
         for keywords and command-line -D options, for example.  No error
         check is done. */
    } else {
      /* If the symbol is not being entered in the innermost scope, skip
         past any symbols on the active list from the scopes inside the
         entry scope.  That's necessary so that the new symbol can be added
         at the right place in the active list, which is ordered from
         innermost to outermost scope. */
      old_sym_ptr = hdr_ptr->symbol;
      for (curr_depth = depth_scope_stack; ; curr_depth--) {
        scope_number = scope_stack[curr_depth].number;
        if (curr_depth == scope_depth) break;
        /* Ignore any symbols from the scope skipped over. */
        while (old_sym_ptr != NULL &&
               old_sym_ptr->decl_scope == scope_number) {
          insert_after = old_sym_ptr;
          old_sym_ptr = old_sym_ptr->next;
        }  /* while */
      }  /* for */
      sym_name_space_kind = name_space_for_symbol_kind[(int)sym_ptr->kind];
      /* See if this name a redeclaration of a template parameter name. */
      redeclared_template_param = (depth_innermost_instantiation_scope !=
				   NO_SCOPE_DEPTH) &&
				  sym_name_space_kind == nsk_other &&
				  is_redeclared_template_param(sym_ptr);
      if (!suppress_error && redeclared_template_param &&
          scope_stack[scope_depth].template_param_decl_scope) {
        /* A template parameter name has been reused in the first scope
	   associated with the instantiation that affects the declarative
           level.  Note that we pass the identifier string to the error
           routine rather than using the standard symbol name fill-in. 
           This is done because the variable pointer may not have been
           filled in at the time the symbol is entered. */
        pos_st_error(ec_redeclaration_of_template_param_name,
                     &(sym_ptr->decl_position), sym_ptr->header->identifier);
      } else {
        a_boolean	set_insert_after = TRUE;
        if (redeclared_template_param) {
          /* A template parameter name has been reused in an inner scope
             of a template class or function.  Issue a warning that the
             template parameter will be hidden. */
          pos_st_warning(ec_decl_hides_template_parameter,
                         &(sym_ptr->decl_position),
                         sym_ptr->header->identifier);
        }  /* if */
        /* See if there is already a definition of this identifier in the same
           scope and name space.  (For name spaces, see C standard, 3.1.2.3.)
           Because of the code above and because the active list is ordered,
           if there are any symbols in the same scope they will be at the
           front of the list.  Note that this test must be done even if
           suppress_error is TRUE, because tag names must still be entered
           behind existing non-tag names in C++.  suppress_error is only TRUE
           when there has already been an error issued, so in practical terms
           the extra check costs nothing. */
        for (; old_sym_ptr != NULL && old_sym_ptr->decl_scope == scope_number;
             old_sym_ptr = old_sym_ptr->next) {
          if (name_space_for_symbol_kind[(int)old_sym_ptr->kind] ==
                                                         sym_name_space_kind) {
            /* Two declarations in the same name space in the same scope:
               in most cases, this is an error, but in C++, one is allowed to
               define a tag name and a non-type name in the same scope (see ARM
               3.2, 3.1c, and 7.1.3).  In cfront and pcc modes a variable is
               allowed to hide a function parameter. */
            if (!symbols_may_coexist_in_curr_scope
                            (old_sym_ptr, sym_ptr,
                             set_insert_after ? &insert_after :
                                                (a_symbol_ptr*)NULL,
                             suppress_error)) {
              /* Error, this identifier has already been declared. */
              if (!suppress_error) {
                /* Note that we pass the identifier string to the error routine
                   rather than using the standard symbol name fill-in. 
                   This is done because the variable pointer may not have been
                   filled in at the time the symbol is entered. */
                pos_st_error((is_type_symbol(sym_ptr) &&
                              is_type_symbol(old_sym_ptr) &&
                              C_dialect == C_dialect_cplusplus) ?
                                             ec_bad_type_name_redeclaration :
                                             ec_id_already_declared,
                             &(sym_ptr->decl_position),
                             sym_ptr->header->identifier);
              }  /* if */
              /* Only break out of the loop if an error occurred.  Otherwise
                 check with other symbols to make sure that this symbol
                 can coexist with an already hidden symbol. */
              break;
            }  /* if */
            /* Go ahead and enter the symbol anyway.  Both symbols will be
               in the symbol table.  The insert position should only be set
               by the first symbols_may_coexist_in_curr_scope call.
               Subsequent calls are only for error detection purposes and
               should not affect the position of the new symbol in the symbol
               table. */
            set_insert_after = FALSE;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (insert_after == NULL) {
      /* Link the symbol onto the front of the list in the symbol header.
         This is the normal case. */
      sym_ptr->next = hdr_ptr->symbol;
      hdr_ptr->symbol = sym_ptr;
    } else {
      /* Link the symbol behind insert_after. */
      sym_ptr->next = insert_after->next;
      insert_after->next = sym_ptr;
    }  /* if */
  }  /* if */
}  /* link_symbol_into_symbol_table */


static a_boolean member_name_conflicts_with_class_name(a_type_ptr   class_type,
                                                       a_symbol_ptr member_sym)
/*
If the member specified by member_sym has the same name as the class of
which it is a member (class_type), issue an error -- except for nonstatic
data members in a class with no constructors (ARM 9.2).  Constructors
are another special case, but since they are not actually entered into
the symbol table, this routine is not called for them.
*/
{
  a_symbol_ptr class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  a_boolean    err = FALSE;

  if (class_sym->header == member_sym->header) {
    /* Member has the same name as the class to which it belongs. */
    /* If no constructor already exists we permit a field with the same
       name its class, as long as it's not an anonymous union field being
       promoted to a containing class with the same name. */
    if (member_sym->kind == (a_symbol_kind)sk_field &&
        class_sym->variant.
                    class_struct_union.extra_info->constructor == NULL &&
        (member_sym->variant.field.ptr == NULL ||
         member_sym->class_of_which_a_member ==
                                member_sym->variant.field.ptr->
                                    source_corresp.class_of_which_a_member)) {
      /* No error.  Either the field has not yet been bound to the symbol
         (never true for anonymous union symbol promotions) or the parent
         classes correspond (also untrue for anonymous union promotions). */
    } else {
      /* Error: an identifier that is not a constructor and that has the
         same name as a class is being defined within the class. */
      pos_error(is_function_symbol(member_sym) ?
                     ec_class_and_member_function_name_conflict :
                     ec_class_and_member_name_conflict,
                &member_sym->decl_position);
      err = TRUE;
    }  /* if */
  }  /* if */
  return err;
}  /* member_name_conflicts_with_class_name */


static void add_symbol_to_scope_list(a_symbol_ptr  sym_ptr,
                                     a_scope_depth scope_depth,
                                     a_boolean     *err)
/*
Add the given symbol to the list of symbols for the scope at scope_depth
in the scope stack.  *err is set to TRUE if there is an error; it is not
changed if there is no error.
*/
{
  a_scope_stack_entry_ptr ssep;

  if (scope_depth == NO_SCOPE_DEPTH) {
    /* The symbol is being entered outside of any scope (e.g., a macro
       defined by a command-line -D option). */
    sym_ptr->decl_scope = NO_SCOPE_NUMBER;
  } else {
#if CHECKING
    if (scope_depth < 0 || scope_depth > depth_scope_stack) {
      internal_error("add_symbol_to_scope_list: bad scope depth");
    }  /* if */
#endif /* CHECKING */
    ssep = &scope_stack[scope_depth];
    /* Put the proper scope number into the symbol entry. */
    sym_ptr->decl_scope = ssep->number;
    if (sym_ptr->is_error) {
      /* Error symbols are not added to the scope list. */
    } else {
      /* Add the symbol to the end of the symbols list for the scope. */
      if (ssep->symbols == NULL) {
        ssep->symbols = sym_ptr;
      } else {
        ssep->last_symbol->next_in_scope = sym_ptr;
      }  /* if */
      ssep->last_symbol = sym_ptr;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++, it's an error for something with the same name as a class to
         be defined within the class unless it's a constructor (the symbol
         for which is not added to the scope list) or a nonstatic data
         member that is not an anonymous union member (ARM 9.2). */
      if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
          member_name_conflicts_with_class_name(ssep->assoc_type, sym_ptr)) {
        *err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  sym_ptr->next_in_scope = NULL;
}  /* add_symbol_to_scope_list */


a_symbol_ptr enter_symbol(a_symbol_kind    sym_kind,
			  a_symbol_locator *location,
			  a_scope_depth    scope_depth,
                          a_boolean        suppress_error)
/*
Enter a new symbol table entry into the symbol table.  This routine assumes
that "find_symbol" has already been called to correctly set the header that
the symbol will be linked from (this is summarized in *location).  scope_depth
indicates the level of the scope stack at which the symbol should be entered.
Generate an error if the symbol is already defined in that scope and name
space unless suppress_error is TRUE.  If *location is an error locator,
an error symbol is created and entered.  Note that any specific symbol
indicated in the locator is supposed to be ignored.
*/
{
  register a_symbol_ptr sym_ptr;

  db_enter(4, "enter_symbol");

  /* Allocate and initialize the symbol. */
  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->is_error = location->is_error;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(sym_ptr, scope_depth, &suppress_error);
  /* Add the symbol to the symbol table.  This must be done after the symbol
     is added to the scope list, because that sets the scope number, which
     is needed to check for redeclaration. */
  link_symbol_into_symbol_table(sym_ptr, scope_depth, suppress_error);
  db_exit();
  return sym_ptr;
}  /* enter_symbol */


void reenter_symbol(a_symbol_ptr     symbol_to_reenter,
		    a_scope_depth    scope_depth,
                    a_boolean        suppress_error)
/*
Re-enter a symbol table entry into the symbol table.  symbol_to_reenter
points to a symbol that was previously in the symbol table and was
removed.  scope_depth indicates the level of the scope stack at which the
symbol should be re-entered.  Generate an error if the symbol is already
defined in that scope and name space unless suppress_error is TRUE.
*/
{
  db_enter(4, "reenter_symbol");

  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(symbol_to_reenter, scope_depth, &suppress_error);
  /* Add the symbol to the symbol table.  This must be done after the symbol
     is added to the scope list, because that sets the scope number, which
     is needed to check for redeclaration. */
  link_symbol_into_symbol_table(symbol_to_reenter, scope_depth,
                                suppress_error);
  db_exit();
}  /* reenter_symbol */


void reactivate_prototype_scope_symbols(a_symbol_ptr  prototype_scope_symbols)
/*
Some symbols for types were created in a function prototype scope.
Reactivate those symbols (which were removed at the end of the function
prototype scope) now that we are in the body of the function.
*/
{
  a_symbol_ptr curr_symbol, next_symbol;

  /* Re-enter each symbol in the symbol table. */
  for (curr_symbol = prototype_scope_symbols;
       curr_symbol != NULL;
       curr_symbol = next_symbol) {
    next_symbol = curr_symbol->next_in_scope;
    reenter_symbol(curr_symbol, depth_scope_stack, /*suppress_error=*/TRUE);
    curr_symbol->reentered_from_prototype_scope = TRUE;
  }  /* for */
}  /* reactivate_prototype_scope_symbols */


void relink_unnamed_class_symbol(a_symbol_ptr      sym,
                                 a_symbol_locator  *locator)
/*
A name is belatedly specified for a class, and so the tag symbol originally
created for it most be modified to bear the new name.  Give the symbol
the new name and relink it into the symbol table under the new header.
*/
{
  db_enter(4, "relink_unnamed_class_symbol");
#if CHECKING
  /* The symbol should not have been linked onto the symbol list for its
     header. */
  if (sym->header != unnamed_class_symbol_header) {
    internal_error("relink_unnamed_class_symbol: unexpected symbol header");
  }  /* if */
  /* The declaration scope should not be changed. */
  if (scope_stack[decl_scope_level].number != sym->decl_scope) {
    internal_error("relink_unnamed_class_symbol: bad scope");
  }  /* if */
#endif /* CHECKING */
  /* Replace the special symbol header for unnamed class symbols with the
     header associated with its new name. */
  sym->header = locator->symbol_header;
  /* Add the symbol to the symbol table. */
  reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
  db_exit();
}  /* relink_unnamed_class_symbol */


a_symbol_ptr add_symbol_to_overload_list(a_symbol_ptr  new_sym,
                                         a_symbol_ptr  other_sym)
/*
new_sym is a newly created function (or function template) symbol that
shares a name with other_sym, which is either an overloaded function symbol
or another function or function template symbol.  If necessary, create an
overloaded function symbol and add other_sym to its list.  Add new_sym to
the new or existing list of overloaded functions, and return a pointer to
the overloaded function symbol.
*/
{
  a_symbol_ptr        overload_sym, prev_sym_ptr;
  a_symbol_header_ptr hdr_ptr;
  a_scope_stack_entry *ssep;
  
  if (other_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    overload_sym = other_sym;
    other_sym = overload_sym->variant.overloaded_function.symbols;
  } else {
    /* The existing symbol is not an sk_overloaded_function symbol
       (i.e., it's a simple function symbol of some kind). */
    /* Create an sk_overloaded_function symbol and attach the old
       function symbol to it. */
    hdr_ptr = other_sym->header;
    overload_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                   hdr_ptr, &(other_sym->decl_position));
    overload_sym->decl_scope = other_sym->decl_scope;
    overload_sym->class_of_which_a_member =
                                       other_sym->class_of_which_a_member;
    /* Put overload_sym into the primary list in place of other_sym. */
    /* Find the symbol preceding other_sym on its list. */
    prev_sym_ptr = hdr_ptr->symbol;
    if (prev_sym_ptr == other_sym) {
      /* The entry is the first on the header list. */
      hdr_ptr->symbol = overload_sym;
    } else {
      while (prev_sym_ptr->next != other_sym) {
        prev_sym_ptr = prev_sym_ptr->next;
      }  /* while */
      prev_sym_ptr->next = overload_sym;
    }  /* if */
    overload_sym->next = other_sym->next;
    other_sym->next = NULL;
    /* Also put overload_sym into the scope list in place of other_sym. */
    ssep = &scope_stack[decl_scope_level];
    /* If the scope stack entry for the overloaded function is not that of
       the current scope (e.g., when a friend declaration refers to a function
       at file scope), find the correct one. */
    while (ssep->number != other_sym->decl_scope) {
#if CHECKING
      if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) {
        internal_error("enter_overloaded_symbol: scope stack overrun error");
      }  /* if */
#endif /* CHECKING */
      --ssep;
    }  /* if */
    prev_sym_ptr = ssep->symbols;
    if (prev_sym_ptr == other_sym) {
       /* The entry is the first on the scope's symbol list. */
       ssep->symbols = overload_sym;
    } else {
       while (prev_sym_ptr->next_in_scope != other_sym) {
         prev_sym_ptr = prev_sym_ptr->next_in_scope;
       }  /* while */
       prev_sym_ptr->next_in_scope = overload_sym;
    }  /* if */
    overload_sym->next_in_scope = other_sym->next_in_scope;
    other_sym->next_in_scope = NULL;
    if (ssep->last_symbol == other_sym) ssep->last_symbol = overload_sym;
    /* Attach the old symbol under the overloaded symbol. */
    overload_sym->variant.overloaded_function.symbols = other_sym;
  }  /* if */
  /* Attach the new symbol to the front of the list under the overloaded
     symbol. */
  new_sym->next = overload_sym->variant.overloaded_function.symbols;
  overload_sym->variant.overloaded_function.symbols = new_sym;
  /* Return a pointer to the sk_overloaded_function symbol. */
  return overload_sym;
}  /* add_symbol_to_overload_list */


a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                     a_symbol_locator *location,
                                     a_symbol_ptr     other_sym,
                                     a_symbol_ptr     *overload_sym)
/*
Enter a new symbol that is an overloading of the existing symbol other_sym.
other_sym may be either a simple function or an sk_overloaded_function.
The kind of symbol is sym_kind (some function kind).  *location gives
a locator for the new symbol.  Return a pointer to the new symbol.
*/
{
  a_symbol_ptr        sym_ptr;

  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->decl_scope = other_sym->decl_scope;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  /* Add the symbol to the overloaded function list. */
  *overload_sym = add_symbol_to_overload_list(sym_ptr, other_sym);
  /* Return a pointer to the newly created symbol as well. */
  return sym_ptr;
}  /* enter_overloaded_symbol */


a_symbol_ptr make_projection_symbol(a_symbol_ptr      progenitor_sym,
                                    a_type_ptr        class_ptr,
                                    a_base_class_ptr  fundamental_bcp,
                                    a_derivation_step *path,
                                    a_boolean         ambiguous)
/*
Create a new projection symbol entry and return a pointer to it.  The symbol
is a projection of progenitor_sym into the current scope.  The symbol is not
added to the scope symbols list and is not linked into the symbol table.
fundamental_bcp is a pointer to the base class of class_ptr in which the
fundamental symbol resides; if it is NULL, it must be computed, using *path
(which describes the derivation of *class_ptr from the class of which
progenitor_sym is a member) if ambiguous is TRUE.
*/
{
  register a_symbol_ptr        sym;
  a_projection_descr_ptr       pdp, progenitor_pdp = NULL;
  a_base_class_ptr             bcp;

  db_enter(4, "make_projection_symbol");

  /* Allocate and initialize the symbol. */
  sym = alloc_symbol((a_symbol_kind)sk_projection, progenitor_sym->header,
                     &progenitor_sym->decl_position);
  sym->class_of_which_a_member = class_ptr;
  sym->decl_scope = class_ptr->variant.class_struct_union.extra_info->
                                                        assoc_scope->number;
  sym->variant.projection.ambiguous = ambiguous;
  pdp = sym->variant.projection.extra_info;
  if (progenitor_sym->kind == (a_symbol_kind)sk_projection) {
    /* The "progenitor" of this new projection symbol is itself a projection
       symbol. */
    progenitor_pdp = progenitor_sym->variant.projection.extra_info;
    pdp->fundamental_symbol = progenitor_pdp->fundamental_symbol;
    /* Set the flag indicating whether there are any intervening access
       declarations in the inheritance path. */
    if (progenitor_sym->variant.projection.access_adjustment_made ||
        progenitor_sym->variant.projection.intervening_access_adjustment) {
      sym->variant.projection.intervening_access_adjustment = TRUE;
    }  /* if */
  } else {
    pdp->fundamental_symbol = progenitor_sym;
  }  /* if */
  if (fundamental_bcp != NULL) {
    /* The caller has supplied the fundamental base class. */
    pdp->fundamental_base_class = fundamental_bcp;
  } else {
    /* To set the fundamental_base_class pointer in the projection descriptor
       for sym, we have to look through the base symbols for the current class.
       The base class with which the fundamental symbol is associated is the
       one we want. */
    bcp = class_ptr->variant.class_struct_union.extra_info->base_classes;
    if (!ambiguous) {
      /* There is no ambiguity in the use of this name, so a simple type match
         is enough to identify the base class of the fundamental symbol. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->type == pdp->fundamental_symbol->class_of_which_a_member) {
          pdp->fundamental_base_class = bcp;
          break;
        }  /* if */
      }  /* for */
    } else {
      /* When there is an ambiguity, we must check the paths as well as the
         type. */
      a_base_class_ptr  ref_bcp = path->base_class;

      if (ref_bcp->derived_class != class_ptr) {
        ref_bcp = corresponding_base_class(ref_bcp, class_ptr,
                                           (a_base_class_ptr)NULL);
      }  /* if */
      check_assertion(ref_bcp->direct || ref_bcp->is_virtual);
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->type == pdp->fundamental_symbol->class_of_which_a_member) {
          /* ref_bcp is the root of a path to the base class where the
             inherited name was found.  Be sure ref_bcp is also on the path to
             bcp before deciding the bcp is the base class of the fundamental
             symbol.  Note since the inheritance is ambiguous there may be
             several base classes that match the type in question, and there
             may be more than one for which ref_bcp is on the path. */
          if (!bcp->ambiguous || ref_bcp == bcp ||
              is_on_any_derivation_of(bcp, ref_bcp)) {
            pdp->fundamental_base_class = bcp;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#if CHECKING
    if (pdp->fundamental_base_class == NULL) {
      internal_error("make_projection_symbol: no fundamental base class");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  db_exit();
  return sym;
}  /* make_projection_symbol */


static a_symbol_ptr make_parameter_symbol(a_symbol_locator  *locator)
/*
Create but do not yet enter an sk_parameter symbol.  This routine is called
for old style parameter declaration.
*/
{
  a_symbol_ptr  sym;

  sym = alloc_symbol((a_symbol_kind)sk_parameter, locator->symbol_header,
                     &locator->source_position);
  /* Set the locator to point to the symbol entered. */
  locator->specific_symbol = sym;
  locator->is_qualified_name = FALSE;

  return sym;
}  /* make_parameter_symbol */



a_symbol_ptr make_template_class_symbol(a_symbol_ptr       ct_symbol,
                                        a_source_position *pos)
/*
Create a symbol for an instance of a class template.  Link the symbol to
the class template symbol but do not enter it into the symbol table.
ct_symbol is the symbol of the class template.
*/
{
  a_symbol_ptr  sym;
  a_symbol_kind kind;

  /* Determine kind of symbol to be entered.  It can be either a
     class_or_struct or a union depending on the type of the class
     template. */
  switch (ct_symbol->variant.template_info->variant.class_template.type_kind) {
    case tk_class:
    case tk_struct:  kind = (a_symbol_kind)sk_class_or_struct_tag;  break;
    case tk_union:   kind = (a_symbol_kind)sk_union_tag;            break;
#if CHECKING
    default:
      internal_error("make_template_class_symbol: bad type kind");
#endif /* CHECKING */
  }  /* switch */
  /* Create the symbol.  Use the current source position as the declaration
     position. */
  sym = alloc_symbol(kind, ct_symbol->header, pos);
  /* Set the pointer that points back to the original class template symbol. */
  sym->variant.class_struct_union.extra_info->class_template = ct_symbol;
  /* Make the declaration scope the same as the class template's. */
  sym->decl_scope = ct_symbol->decl_scope;

  return sym;
}  /* make_template_class_symbol */


a_symbol_ptr make_template_function_symbol(a_symbol_ptr       templ_sym,
                                           a_source_position  *pos)
/*
Create a symbol for a template function.  Do not enter it into the symbol
table, since it is accessed from the associated function instantiation entry.
*/
{
  a_symbol_ptr  sym;

  sym = alloc_symbol((a_symbol_kind)sk_routine, templ_sym->header, pos);
  /* Template functions will be in the same scope as the template (which
     should always be the file scope. */
  sym->decl_scope = templ_sym->decl_scope;;

  return sym;
}  /* make_template_function_symbol */


a_symbol_ptr get_member_function_template_symbol(a_symbol_ptr  rout_sym)
/*
rout_sym is a symbol representing a member function of a prototype
instantiation of a template class.  If there is already a function template
symbol associated with rout_sym, return it.  Otherwise, allocate and
initialize a function instantiation entry and a function template symbol
and attach them to rout_sym, and return the function template symbol.
*/
{
  a_symbol_ptr             template_sym;
  a_template_instance_ptr  tip;

#if CHECKING
  if (!(symbol_supplement_for_class(rout_sym->class_of_which_a_member))->
                                                            is_nonreal_class) {
    internal_error("make_member_function_template_symbol: real class member");
  }  /* if */
#endif /* CHECKING */
  tip = rout_sym->variant.routine.instance_ptr;
  if (tip != NULL) {
    template_sym = tip->template_sym;
  } else {
    /* Note that the function template symbol is not entered in the symbol
       table, since it need only be accessed only through the corresponding
       member function symbol rout_sym. */
    template_sym = alloc_symbol((a_symbol_kind)sk_function_template,
                                rout_sym->header, &rout_sym->decl_position);
    template_sym->class_of_which_a_member = rout_sym->class_of_which_a_member;
    template_sym->variant.template_info->variant.function.routine =
                                            rout_sym->variant.routine.ptr;
    /* Create the associated function instantiation entry, but do not link it
       onto the instantiation list for the template. */
    tip = alloc_template_instance();
    tip->template_sym = template_sym;
    /* Make the function instantiation entry and its associated symbol
       point at each other. */
    tip->instance_sym = rout_sym;
    rout_sym->variant.routine.instance_ptr = tip;
  }  /* if */
  return template_sym;
}  /* get_member_function_template_symbol */


a_boolean current_class_symbol_if_class_template(a_symbol_ptr *sym)
/*
If the symbol is a class template that is currently being instantiated,
or if a specific definition of the class is being defined, the symbol of
the instantiation (or the specific definition) is returned in *sym,
otherwise the original symbol is left unchanged.  If the symbol returned
is not a class template symbol (either because the symbol passed by the
caller was not a class template or because we succeeded in finding an
instantiation or definition) we return TRUE.  If the symbol is a class
template with no current instantiation or definition, we return FALSE.
*/
{
  a_scope_depth  depth;
  a_boolean      found = TRUE;
  a_boolean      is_instantiation_scope;
  a_symbol_ptr   instance_sym;

  if ((*sym)->kind == (a_symbol_kind)sk_class_template) {
    found = FALSE;
    /* We can skip the lookup if there are no class scopes (including
       reactivation scopes) or instantiation scopes on the stack. */
    if ((num_classes_on_scope_stack > 0) ||
        (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH)) {
      /* Loop through the scope stack looking at the instantiation scopes
         and the class declaration and reactivation scopes.  Stop after
         finding the first instantiation scope.  Check each of these
         scopes to see if the associated type is a template class associated
         with the class template symbol. */
      for (depth = depth_scope_stack; depth >= 0; --depth) {
        a_scope_stack_entry_ptr ssep = &scope_stack[depth];
        is_instantiation_scope =
                        ssep->kind == (a_scope_kind)sck_template_instantiation;
        if (is_instantiation_scope ||
            ssep->kind == (a_scope_kind)sck_class_struct_union ||
            ssep->kind == (a_scope_kind)sck_class_reactivation) {
          /* Get the instance symbol pointed to by the type from the scope
             stack entry. */
          if (is_instantiation_scope) {
              /* Don't look beyond the innermost instantiation scope that
                 is not a nested instantiation. */
            if (!ssep->nested_instantiation) break;
          } else {
            check_assertion_str(ssep->assoc_type != NULL,
				"ccsict: assoc_type is NULL");
            instance_sym = (a_symbol_ptr)(ssep->assoc_type->
                                                    source_corresp.assoc_info);
            /* A class/struct/union scope or reactivation scope. */
            if (instance_sym->variant.class_struct_union.
                            extra_info->class_template == *sym) {
              found = TRUE;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      if (found) *sym = instance_sym;
    }  /* if */
  }  /* if */
  return found;
}  /* current_class_symbol_if_class_template */


a_symbol_ptr make_unnamed_class_symbol(a_symbol_kind      sym_kind,
                                       a_source_position  *pos)
/*
Create a symbol for a tagless class, struct, or union symbol.  Do not enter
it into the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(4, "make_unnamed_class_symbol");
  /* Use the unnamed class symbol header.  Allocate it if necessary. */
  if (unnamed_class_symbol_header == NULL) {
    unnamed_class_symbol_header = alloc_symbol_header();
    unnamed_class_symbol_header->identifier = "<unnamed>";
    unnamed_class_symbol_header->identifier_length = 9;
  }  /* if */
  sym = alloc_symbol(sym_kind, unnamed_class_symbol_header, pos);
  sym->decl_scope = scope_stack[decl_scope_level].number;
  db_exit();
  return sym;
}  /* make_unnamed_class_symbol */


a_boolean is_unnamed_class_symbol(a_symbol_ptr  sym)
/*
Return TRUE if sym represents an unnamed class type.
*/
{
  return (sym->header == unnamed_class_symbol_header);
}  /* if */


a_symbol_ptr unnamed_field_symbol(void)
/*
Return a pointer to "the" unnamed field symbol, which exists only for the
sake of identifying a given field entry as representing an unnamed field.
*/
{
  static a_symbol             sym;

  if (unnamed_field_symbol_header == NULL) {
    /* Set the shared fields to default values, set the kind, and initialize
       its variant fields. */
    clear_symbol(&sym, (a_symbol_kind)sk_field);
    /* Set the header. */
    unnamed_field_symbol_header = alloc_symbol_header();
    unnamed_field_symbol_header->identifier = "<unnamed>";
    unnamed_field_symbol_header->identifier_length = 9;
    sym.header = unnamed_field_symbol_header;
  }  /* if */
  return &sym;
}  /* unnamed_field_symbol */


a_symbol_ptr make_anonymous_parent_object_symbol(a_symbol_kind      kind,
                                                 a_source_position  *pos,
                                                 a_scope_number     decl_scope)
/*
Return a symbol for a field or variable that serves as the "anonymous
parent object" for an anonymous union.  Do not enter it in the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(4, "make_anonymous_parent_object_symbol");
  /* Use the unnamed class symbol header.  Allocate it if necessary. */
  if (anonymous_parent_object_symbol_header == NULL) {
    anonymous_parent_object_symbol_header = alloc_symbol_header();
    anonymous_parent_object_symbol_header->identifier = "<unnamed>";
    anonymous_parent_object_symbol_header->identifier_length = 9;
  }  /* if */
  sym = alloc_symbol(kind, anonymous_parent_object_symbol_header, pos);
  sym->decl_scope = decl_scope;
  db_exit();
  return sym;
}  /* make_anonymous_parent_object_symbol */


a_symbol_ptr full_enter_symbol(char          *identifier,
		               sizeof_t      length,
			       a_symbol_kind sym_kind,
			       a_scope_depth scope_depth)
/*
Enter a new symbol into the symbol table.  This is like enter_symbol, but
for those cases where a symbol locator is not available (because the symbol
is being entered regardless of any previous definition), like for keywords
entered during initialization.
*/
{
  a_symbol_locator      location;
  register a_symbol_ptr sym_ptr;

  db_enter(4, "full_enter_symbol");
  clear_locator(&location, &null_source_position);
  (void)find_symbol(identifier, length, &location);
  sym_ptr = enter_symbol(sym_kind, &location, scope_depth,
                         /*suppress_error=*/FALSE);
  db_exit();
  return sym_ptr;
}  /* full_enter_symbol */


static void expand_ident_buffer(sizeof_t size_needed)
/*
Expand the ident_buffer by reallocating it, so that its total size is at
least size_needed.  Called by ensure_ident_buffer_space.
*/
{
  sizeof_t new_size;

  db_enter(4, "expand_ident_buffer");
  new_size = size_ident_buffer + IDENT_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  ident_buffer = realloc_general(ident_buffer, size_ident_buffer, new_size);
  size_ident_buffer = new_size;
  db_exit();
}  /* expand_ident_buffer */


/*
Ensure that ident_buffer has at least size_needed bytes in it.
If not, expand ident_buffer by reallocating it.
*/
#define ensure_ident_buffer_space(size_needed)                        \
{ if (size_ident_buffer < size_needed) {                              \
    expand_ident_buffer((sizeof_t)(size_needed));                     \
  }  /* if */                                                         \
}  /* ensure_ident_buffer_space */


a_symbol_ptr find_external_symbol(a_symbol_locator     *location,
                                  a_name_linkage_kind  linkage,
                                  a_type_ptr           rout_type,
                                  a_symbol_locator     *ext_location)
/*
Determine the external name that should be associated with the identifier
specified by *location, and return a locator for it in *ext_location.  Also
return a pointer to any existing sk_extern_variable or sk_extern_routine
found, or NULL if there is no such entry.  The handling for C and C++ names
is different in two respects.  First, in C++ we have to deal with the
possible overloading of function names.  Second, in C++ we assume the
linker can handle whatever names the compiler will generate on the basis of
user name and type, and that those names will be generated in such a way as
to be unique, whereas in C we we allow for differences in external names
due to truncation.  Input arguments are the name linkage and rout_type;
the latter will be NULL for variables.
*/
{
  a_symbol_header_ptr hdr_ptr;
  a_symbol_ptr        sym_ptr;

  db_enter(4, "find_external_symbol");
  /* Start with the external locator the same as the normal locator.  This
     is usually correct. */
  *ext_location = *location;
  if (is_error_locator(*ext_location)) {
    /* This is a compiler-generated error symbol (probably generated because
       an identifier was missing). */
    sym_ptr = NULL;
  } else {
    hdr_ptr = ext_location->symbol_header;
    if (linkage != (a_name_linkage_kind)nlk_external) {
      /* Either static or C++ external name linkage.  We can use the name and
         locator as passed in. */
      sym_ptr = hdr_ptr->symbol;
    } else {
      /* The identifier is external, with "ordinary C" linkage. */
#if !TARG_CASE_SENSITIVE_EXTERNAL_NAMES
      /* External names are case-insensitive, so make a case-neutral copy
         of the identifier, upcasing each letter. */
      { char     *src = hdr_ptr->identifier, *dest;
        sizeof_t count;
        char     ch;
#if TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0
        /* There is an upper limit on significance in external names. */
        char     new_ident[TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME];

        /* Copy the name, upcasing each letter.  Stop at the end of the name
           (without copying the final null) or when enough characters have been
           copied to meet the maximum number of significant characters. */
        dest = new_ident;
        for (count = 0;
             *src != '\0' && count < TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME;
             count++) {
          ch = *src++;
          if (islower((unsigned char)ch)) ch = toupper(ch);
          *dest++ = ch;
        }  /* for */
#else /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME == 0 */
        /* Use a dynamically-allocated array, since there is no limit to
           the significance of external names. */
        char *new_ident;
        ensure_ident_buffer_space(hdr_ptr->identifier_length);
        dest = new_ident = ident_buffer;
   
        /* Copy the name, upcasing each letter.  Stop at the end of the name
           (without copying the final null). */
        for (count = 0; *src != '\0'; count++) {
          ch = *src++;
          if (islower((unsigned char)ch)) ch = toupper(ch);
          *dest++ = ch;
        }  /* for */
#endif /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0 */
        /* Create a header/locator for the upcased/truncated name. */
        sym_ptr = find_symbol(new_ident, count, ext_location);
      }
#else /* TARG_CASE_SENSITIVE_EXTERNAL_NAMES */
#if TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0
      /* See if the symbol's name is longer than the maximum number of
         significant characters in an external name. */
      if (hdr_ptr->identifier_length > TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME) {
        /* The name is overlong; the external version must be truncated.
           Create the header/locator by looking up the truncated name. */
        sym_ptr = find_symbol(hdr_ptr->identifier,
                              TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME,
                              ext_location);
      } else {
#endif /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0 */
        /* The name is not overlong; the external name will be the same as the 
           source name, and therefore the locator for the new symbol is the
           same as that for the current symbol. */
        sym_ptr = hdr_ptr->symbol;
#if TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0
      }  /* if */
#endif /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0 */
#endif /* !TARG_CASE_SENSITIVE_EXTERNAL_NAMES */
    }  /* if */
    /* See if there is already an external symbol with this name. */
    for (; sym_ptr != NULL; sym_ptr = sym_ptr->next) {
      if (sym_ptr->kind == (a_symbol_kind)sk_extern_variable) {
        break;
      } else if (sym_ptr->kind == (a_symbol_kind)sk_extern_routine) {
        /* A type compatibility check may also be required for routines. */
        if (rout_type == NULL || C_dialect != C_dialect_cplusplus) {
          /* A name match is enough. */
          break;
        } else if (is_error_type(sym_ptr->variant.extern_symbol_descr->type)) {
          /* Assume this is not a match.  Keep looking. */
        } else {
          /* In C++ the function's type signature is effectively part of the
             name.  Therefore we check for parameter type compatibility (the
             return type is not decisive, since functions with the same
             param types and different return types are not allowed).  Since
             names with extern "C" linkage are not mangled, one would think
             that param type checking would not be required in that case.
             However, two functions with extern "C" linkage and different
             param types are treated not as incompatible declarations of a
             routine but as an instance of illegal overloading of a routine
             name.  The error is issued later. */
          /* Note that error types are not considered compatible with
             anything here, and that's deliberate to avoid a false clash
             on something like
               int f(int)   { return 0; }
               int f(undef) { return 0; }
          */
          if (param_types_are_compatible(
                                 rout_type,
                                 sym_ptr->variant.extern_symbol_descr->type,
                                 TCF_NO_FLAGS)) {
            /* Param types are compatible, so we have a match.  */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      /* No match found yet, so keep looking.  If none if found, a NULL
         sym_ptr is returned to the caller. */
    }  /* for */
  }  /* if */
  /* Make the ext_location source position the same as the original source
     position. */
  ext_location->source_position = location->source_position;
  db_exit();
  return sym_ptr;
}  /* find_external_symbol */


void tildize_locator(a_symbol_locator *locator)
/*
Change the name indicated by the given locator so that it has a "~" on the
front.  This is used in C++ for destructor names.
*/
{
  sizeof_t ident_length = locator->symbol_header->identifier_length;
  a_source_position position;

  /* Copy the identifier name into a dynamically-allocated buffer and put
     a tilde at the front.  The final null is not copied. */
  ensure_ident_buffer_space(ident_length);
  (void)memcpy(ident_buffer+1, locator->symbol_header->identifier,
               size_t_arg(ident_length));
  ident_buffer[0] = '~';
  ident_length++;
  position = locator->source_position;
  clear_locator(locator, &position);
  locator->is_destructor_name = TRUE;
  (void)find_symbol(ident_buffer, ident_length, locator);
}  /* tildize_locator */


a_boolean destructor_name_matches_class_name(a_symbol_ptr class_sym)
/*
The locator points to a symbol header associated with a destructor
(e.g., "~A") and class_sym points to a symbol associated with a
class (e.g., "A").  This routine compares the class name portion of the
destructor name with the name of the class symbol.  Returns TRUE if the
names match and FALSE if they do not match.
*/
{
  char		*destructor_name;
  char		*class_name;
  a_boolean	result = FALSE;

  check_assertion(!is_error_locator(locator_for_curr_id));
  check_assertion(class_sym != NULL);
  check_assertion(locator_for_curr_id.is_destructor_name);
  /* Get a pointer to the second character of the identifier associated
     with the destructor.  This skips over the tilde. */
  destructor_name = &locator_for_curr_id.symbol_header->identifier[1];
  /* Get a pointer to the first character of the class name. */
  class_name = class_sym->header->identifier;
  /* Compare the strings -- return TRUE if they are the same. */
  result = (strcmp(destructor_name, class_name) == 0);
  return result;
}  /* destructor_name_matches_class_name */


void change_class_locator_into_constructor_locator(a_symbol_locator  *locator,
                                                   a_source_position *pos)
/*
Change a locator for a class name into the locator for the constructor for
the class.  The original locator must be for a specific symbol.  pos_curr_token
is used as the source position in the new locator.  This routine is only
used in C++ mode.
*/
{
  a_symbol_ptr                  class_symbol = locator->specific_symbol;
  a_class_symbol_supplement_ptr extra_info;
  a_symbol_header_ptr           hdr_ptr;

#if CHECKING
  if (class_symbol == NULL) {
    internal_error(
        "change_class_locator_into_constructor_locator: NULL specific symbol");
  }  /* if */
  if (class_symbol->kind != (a_symbol_kind)sk_class_or_struct_tag &&
      class_symbol->kind != (a_symbol_kind)sk_union_tag) {
    internal_error(
       "change_class_locator_into_constructor_locator: locator not for class");
  }  /* if */
#endif /* CHECKING */
  extra_info = class_symbol->variant.class_struct_union.extra_info;
  if (extra_info->constructor != NULL) {
    /* A constructor exists already, so get the header pointer from it. */
    hdr_ptr = extra_info->constructor->header;
  } else {
    /* The class has no constructor yet, so create a new header. */
    hdr_ptr = alloc_symbol_header();
    /* The name string can be shared with the class header. */
    hdr_ptr->identifier = locator->symbol_header->identifier;
    hdr_ptr->identifier_length = locator->symbol_header->identifier_length;
  }  /* if */
  clear_locator(locator, pos);
  locator->symbol_header = hdr_ptr;
}  /* change_class_locator_into_constructor_locator */


void make_opname_locator(an_opname_kind     opname,
                         a_symbol_locator   *locator,
                         a_source_position  *pos)
/*
Make a locator for the operator name associated with opname.  This is
used for C++ constructs like "operator+".  Use pos as the source position.
*/
{
  a_symbol_header_ptr *table_entry = &opname_symbol_table[opname];
  a_symbol_header_ptr hdr_ptr;
  char                *opstr, *str;
  a_boolean           blank_needed;
  sizeof_t            opname_length;
#define OPERATOR_LEN 8 /* Length of "operator" */

  clear_locator(locator, pos);
  hdr_ptr = *table_entry;
  if (hdr_ptr == NULL) {
    /* First use of this opname.  Allocate the header. */
    *table_entry = hdr_ptr = alloc_symbol_header();
    /* Give the header the name "operatorX" where "X" is the string for the
       operator. */
    opstr = opname_names[(int)opname];
    /* For "new" and "delete", a blank is needed between the "operator"
       and the opname. */
    blank_needed = (isalpha((unsigned char)opstr[1]) != FALSE);
    opname_length = OPERATOR_LEN + strlen(opstr) + blank_needed;
    hdr_ptr->identifier_length = opname_length;
    hdr_ptr->identifier = str = alloc_il((sizeof_t)(opname_length + 1));
    (void)memcpy(str, "operator", OPERATOR_LEN);
    if (blank_needed) str[OPERATOR_LEN] = ' ';
    (void)strcpy(str+OPERATOR_LEN+blank_needed, opstr);
#if DEBUG
    symbol_name_string_space += opname_length+1;
#endif /* DEBUG */
  }  /* if */
  locator->symbol_header = hdr_ptr;
  locator->is_operator_name = TRUE;
  locator->variant.opname = opname;
#undef OPERATOR_LEN
}  /* make_opname_locator */


void make_type_conversion_locator(a_type_ptr         type,
                                  a_symbol_locator   *locator,
                                  a_source_position  *pos)
/*
Create a locator to represent a type conversion function.  The destination
type "type" is recorded in the locator.  The symbol header is looked up
in the conversion header list; if there is none, a new one is created.
*/
{
  a_conversion_header_ptr  conv_hdr, prev_conv_hdr;
  a_symbol_header_ptr      sym_hdr;
  char                     *type_name;
  sizeof_t                 type_name_length;
#define OPERATOR_LEN 9 /* Length of "operator " */

  if (is_error_type(type)) {
    set_to_error_locator(*locator);
  } else {
    clear_locator(locator, pos);
    /* Search the conversion header list for an entry of the required type.
       If one is found, it is moved to the front of the list. */
    prev_conv_hdr = NULL;
    conv_hdr = conversion_header_list;
    for (; conv_hdr != NULL; conv_hdr = conv_hdr->next) {
      if (types_are_compatible(type, conv_hdr->type)) {
	/* Found it.  Move it to the front of the list. */
	if (prev_conv_hdr != NULL) {
          prev_conv_hdr->next = conv_hdr->next;
          conv_hdr->next = conversion_header_list;
          conversion_header_list = conv_hdr;
	}  /* if */
	break;
      }  /* if */
      prev_conv_hdr = conv_hdr;
    }  /* if */
    /* conv_hdr is NULL if no entry already exists on the list for the
       specified type. */
    if (conv_hdr == NULL) {
      /* Create a new conversion header entry and add it to the front of
         the list. */
      conv_hdr = alloc_conversion_header();
      conv_hdr->next = conversion_header_list;
      conversion_header_list = conv_hdr;
      /* Set the type and symbol header. */
      conv_hdr->type = type;
      conv_hdr->symbol_header = sym_hdr = alloc_symbol_header();
      /* Conversion symbols have the name "operator <type-name>". */
      type_name = format_type_string(type, &type_name_length);
      sym_hdr->identifier_length = (sizeof_t)OPERATOR_LEN + type_name_length;
      sym_hdr->identifier = alloc_il(sym_hdr->identifier_length + 1);
      (void)memcpy(sym_hdr->identifier, "operator ", OPERATOR_LEN);
      (void)strcpy((sym_hdr->identifier + OPERATOR_LEN), type_name);
#if DEBUG
      symbol_name_string_space += sym_hdr->identifier_length;
#endif /* DEBUG */
    }  /* if */
    locator->symbol_header = conv_hdr->symbol_header;
  }  /* if */
  locator->is_conversion_name = TRUE;
  locator->variant.conversion_result_type = type;
#undef OPERATOR_LEN
}  /* make_type_conversion_locator */


a_symbol_ptr extract_default_operator_new_sym(a_symbol_ptr sym)
/*
Given the symbol for an operator new() (which may be overloaded),
find the default new() and return a pointer to its symbol, or NULL if
it is not found.  The symbol might be for a class-specific operator
new(), and therefore might be a projection symbol.
*/
{
  a_boolean        is_overloaded;
  a_param_type_ptr ptp;

  reduce_projection_symbol_to_fundamental_symbol(sym);
  is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
  if (is_overloaded) sym = sym->variant.overloaded_function.symbols;
  for (; sym != NULL; sym = is_overloaded ? sym->next : NULL) {
    /* Ignore function templates. */
    if (is_function_symbol(sym)) {
      /* Look for a symbol for a function with just one parameter.
         Default arguments are not allowed and need not be checked for. */
      ptp = skip_typerefs(sym->variant.routine.ptr->type)->
                                  variant.routine.extra_info->param_type_list;
      if (ptp != NULL && ptp->next == NULL) break;
    }  /* if */
  }  /* for */
  return sym;
}  /* extract_default_operator_new_sym */


void make_global_operator_new_or_delete_symbol(an_opname_kind  opname)
/*
Create a symbol and routine entry for ::operator new or ::operator delete.
These are entered into the symbol table as part of initialization, so
the locator has a default value (as used with keywords).  The routine
entry is marked as compiler generated; if a user declaration appears later,
the compiler-generated flag should be cleared.
*/
{
  a_symbol_locator               locator;
  a_symbol_ptr                   sym = NULL, ext_sym;
  a_type_ptr                     tp, rout_type, old_type;
  a_routine_type_supplement_ptr  extra_info;
  an_id_linkage_kind             linkage;
  a_func_info_block              func_info;

  db_enter(5, "make_global_operator_new_or_delete_symbol");
#if CHECKING
  if (opname != (an_opname_kind)onk_new &&
      opname != (an_opname_kind)onk_delete) {
    internal_error("global_operator_new_or_delete_symbol: bad opname kind");
  }  /* if */
#endif /* CHECKING */
  /* Create a locator for the symbol that is to be created. This will also
     create the symbol header. */
  make_opname_locator(opname, &locator, &null_source_position);
  /* Create a routine type. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  extra_info = rout_type->variant.routine.extra_info;
  /* Return type for operator delete is void; return type for operator new
     is void*. */
  tp = void_type();
  if (opname == (an_opname_kind)onk_new) tp = make_pointer_type(tp);
  rout_type->variant.routine.return_type = tp;
  /* Both new and delete take one parameter -- the size for the former and
     void* for the latter. */
  if (opname == (an_opname_kind)onk_new) {
    tp = integer_type(targ_size_t_int_kind);
  } else {
    tp = make_pointer_type(void_type());
  }  /* if */
  extra_info->param_type_list = alloc_param_type(tp);
  extra_info->prototyped = TRUE;
  set_routine_calling_method_flag(rout_type, &null_source_position);
  clear_func_info(&func_info);
  /* Create the symbol and routine entry.  Note that the routine entry
     is given a storage class of sc_extern since there is no definition
     in the current translation unit. */
  decl_var_or_routine(&locator, (a_storage_class)sc_extern, rout_type,
                      &func_info, (a_source_sequence_entry_ptr)NULL,
                      SRK_DECLARATION, DM_NONE, &sym, &linkage, &old_type,
                      &ext_sym);
  sym->variant.routine.ptr->compiler_generated = TRUE;
  db_exit();
}  /* make_global_operator_new_or_delete_symbol */


a_routine_ptr select_default_constructor(a_type_ptr        class_type,
                                         a_source_position *err_pos,
					 a_type_ptr        object_class_type,
                                         a_boolean         evaluated)
/*
Find and return a pointer to a routine representing a default constructor for
the class indicated by class_type.  (A default constructor is a constructor
that requires no arguments.)  If no acceptable constructor is found, issue
a diagnostic and return NULL.  If more than one acceptable constructor is
found, issue a (different) diagnostic and return NULL.  This routine is
only used in C++ mode.  object_class_type points to the type of the object
being created;  class_type may be a base class of object_class_type.
This is needed for protected member access checking.  If evaluated is
FALSE, the reference is within an unevaluated expression.
*/
{
  a_symbol_ptr  sym, ctor_sym = NULL;
  a_boolean     is_overloaded_function, ambiguous = FALSE;
  a_routine_ptr ctor_routine;

  /* This routine is similar to select_overloaded_function. */
  class_type = skip_typerefs(class_type);
  sym = (symbol_supplement_for_class(class_type))->constructor;
#if CHECKING
  if (sym == NULL) {
    internal_error("select_default_constructor: NULL constructor");
  }  /* if */
#endif /* CHECKING */
  /* If sym is an overloaded function symbol we need to go through the whole
     list. */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    is_overloaded_function = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  } else {
    is_overloaded_function = FALSE;
  }  /* if */
  /* Examine each constructor for this class to find a default constructor.
     There may be more than one.  For instance, there may be a constructor
     with no arguments and one with one argument with a default value. */
  for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
    if (is_default_constructor(sym->variant.routine.ptr)) {
      /* sym is a default constructor. */
      if (ctor_sym != NULL) {
        /* A default constructor had already been found, so there's
           more than one.  We have an ambiguous reference. */
        ambiguous = TRUE;
        break;
      } else {
        /* We've found one.  Record it, but keep looking.  If there's an
           ambiguity we need to report it. */
        ctor_sym = sym;
      }  /* if */
    }  /* if */
  }  /* for */
  ctor_routine = NULL;
  if (ctor_sym == NULL) {
    /* No default constructor. */
    pos_ty_error(ec_no_default_constructor, err_pos, class_type);
  } else if (ambiguous) {
    /* More than one default constructor. */
    pos_ty_error(ec_ambiguous_default_constructor, err_pos, class_type);
  } else {
    /* Exactly one default constructor. */
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(ctor_sym, err_pos,
                                             object_class_type,
                                             /*honor_virtual=*/FALSE,
                                             evaluated,
                                             /*suppress_access_check=*/FALSE);
    ctor_routine = ctor_sym->variant.routine.ptr;
  }  /* if */
  return ctor_routine;
}  /* select_default_constructor */


a_routine_ptr select_destructor(a_type_ptr        class_type,
				a_type_ptr        object_class_type,
                                a_source_position *position,
                                a_boolean         honor_virtual,
                                a_boolean         evaluated,
                                a_boolean         suppress_access_check)
/*
If the indicated class has a destructor, check that it is accessible, mark
it as referenced, and return a pointer to the routine entry.  Otherwise,
return NULL.  object_class_type points to the type of the object being
destroyed; class_type may be a base class of object_class_type.  This is
needed for protected member access checking.  If honor_virtual is TRUE, and
if the destructor is virtual, consider this reference a virtual function
call.  If evaluated is FALSE, the reference is within an unevaluated
expression.  *position is the source position of the reference.  If
suppress_access_check is TRUE, no access checking is done.
*/
{
  a_symbol_ptr  dtor_sym;
  a_routine_ptr dtor_routine = NULL;
  a_class_symbol_supplement_ptr
                cssp = symbol_supplement_for_class(class_type);

  if (cssp != NULL) {
    dtor_sym = cssp->destructor;
    if (dtor_sym != NULL) {
      /* Check that the destructor is accessible and mark it referenced. */
      reference_to_implicitly_invoked_function(dtor_sym, position,
                                               object_class_type,
                                               honor_virtual, evaluated,
                                               suppress_access_check);
      dtor_routine = dtor_sym->variant.routine.ptr;
    }  /* if */
  }  /* if */
  return dtor_routine;
}  /* select_destructor */


a_symbol_ptr find_copy_constructor(a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             *ambiguous,
                                   a_boolean             *class_bitwise_copy)
/*
Find and return a pointer to a symbol representing a copy constructor for
the class indicated by class_type.  If const_object_required is TRUE, return
a copy constructor that accepts a first parameter whose type is const
qualified.  Similarly for volatile_object_required.  Otherwise, return what's
found.  If no acceptable copy constructor is found, return NULL.  If more
than one acceptable copy constructor is found, set *ambiguous to TRUE
and return NULL.  If a bitwise copy is allowed, return NULL and
*class_bitwise_copy TRUE.  This routine is only used in C++ mode.
*/
{
  a_symbol_ptr                   sym, cctor_sym = NULL;
  a_boolean                      is_overloaded_function;
  a_type_qualifier_set           qualifiers;
  a_boolean                      sym_matches_exactly;
  a_boolean                      cctor_sym_matches_exactly = FALSE;
  a_class_symbol_supplement_ptr  cssp;

  /* This routine is similar to select_overloaded_function. */
  *ambiguous = FALSE;
  *class_bitwise_copy = FALSE;
  class_type = skip_typerefs(class_type);
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->construction_by_bitwise_copy_allowed) {
    /* A bitwise copy is allowed. */
    cctor_sym = NULL;
    *class_bitwise_copy = TRUE;
  } else {
    sym = cssp->constructor;
#if CHECKING
    if (sym == NULL) {
      internal_error("find_copy_constructor: NULL constructor");
    }  /* if */
#endif /* CHECKING */
    /* If sym is an overloaded function symbol we need to go through the whole
       list. */
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_overloaded_function = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    } else {
      is_overloaded_function = FALSE;
    }  /* if */
    /* Examine each constructor for this class to find a copy constructor.
       There may be more than one.  For instance, there may be a copy
       constructor that can copy a const object and another that cannot. */
    for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
      if (is_copy_constructor(sym->variant.routine.ptr,
                              sym->class_of_which_a_member, &qualifiers)) {
        if ((required_qualifiers & qualifiers) != required_qualifiers) {
          /* A copy constructor was found that cannot copy the sort of object
             that we need to be able to copy. Keep looking for a suitable copy
             constructor. */
        } else {
          /* sym represents a suitable copy constructor. */
          sym_matches_exactly = (required_qualifiers == qualifiers);
          if (cctor_sym != NULL) {
            /* A suitable copy constructor had already been found, so there's
               more than one.  We may have an ambiguous reference.  We give
               preference to a perfect match over the case in which the
               required and provided qualifiers do not match up exactly. */
            if (!cctor_sym_matches_exactly && sym_matches_exactly) {
              /* cctor_sym was not a perfect match but sym is, so sym is
                 preferred.  Fall through to override the previous settings of
                 cctor_sym and related variables. */
            } else {
              if (cctor_sym_matches_exactly == sym_matches_exactly) {
                /* Both constructors match up exactly with the const and
                   volatile requirements or neither does.  In either case
                   there's no reason to prefer one over the other. */
                *ambiguous = TRUE;
              } else {
                /* cctor_sym is an exact match and sym is not.  Ignore sym. */
              }  /* if */
              /* Skip to the end of the loop. */
              continue;
            }  /* if */
          }  /* if */
          /* We've found one.  Record it, but keep looking.  If there's an
             ambiguity we need to report it. */
          cctor_sym = sym;
          cctor_sym_matches_exactly = sym_matches_exactly;
          *ambiguous = FALSE;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Return NULL if the copy constructor is ambiguous. */
  if (*ambiguous) cctor_sym = NULL;
  return cctor_sym;
}  /* find_copy_constructor */


a_routine_ptr select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  qualifiers_required,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             evaluated,
                                  a_boolean             suppress_access_check)
/*
Find and return a pointer to a routine representing a copy constructor for
the class indicated by class_type.  If qualifiers_required is non-zero,
return a copy constructor that accepts a first parameter whose type is
compatibly qualified.  Otherwise, return what's found.  If no acceptable
copy constructor is found, issue a diagnostic and return NULL.  If more than
one acceptable copy constructor is found, issue a (different) diagnostic and
return NULL.  object_class_type points to the type of the object being
copied; class_type may be a base class of object_class_type.  This is needed
for protected member access checking.  If a bitwise copy is allowed, return
NULL and *class_bitwise_copy TRUE.  If evaluated is FALSE, the reference is
within an unevaluated expression.  This routine is only used in C++ mode.
If suppress_access_check is TRUE, no access checking is done.
*/
{
  a_symbol_ptr  cctor_sym;
  a_routine_ptr cctor_routine;
  a_boolean     ambiguous;

  cctor_sym = find_copy_constructor(class_type, qualifiers_required,
                                    &ambiguous, class_bitwise_copy);
  cctor_routine = NULL;
  if (*class_bitwise_copy) {
    /* A bitwise copy is allowed. */
  } else if (cctor_sym == NULL) {
    if (!ambiguous) {
      /* No applicable copy constructor. */
      if (qualifiers_required == TQ_CONST) {
        /* The common case:  missing const copy constructor. */
        pos_ty_error(ec_missing_const_copy_constructor, err_pos, class_type);
      } else {
        /* Unusual case: volatile or const-volatile expected. */
        pos_ty_error(ec_no_suitable_copy_constructor, err_pos, class_type);
      }  /* if */
    } else {
      /* More than one applicable copy constructor. */
      pos_ty_error(ec_ambiguous_copy_constructor, err_pos, class_type);
    }  /* if */
  } else {
    /* Exactly one copy constructor is best. */
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(cctor_sym, err_pos,
                                             object_class_type,
                                             /*honor_virtual=*/FALSE,
                                             evaluated, suppress_access_check);
    cctor_routine = cctor_sym->variant.routine.ptr;
  }  /* if */
  return cctor_routine;
}  /* select_copy_constructor */


char *il_entry_for_symbol(a_symbol_ptr      sym,
                          an_il_entry_kind  *kind)
/*
Return a pointer to the IL entry to which the specified symbol refers.  Also
return the kind of IL entry that is found.  If the symbol is not associated
with an IL entry, return NULL, and leave kind set to iek_none.
*/
{
  char  *entry_ptr = NULL;

  *kind = iek_none;
  switch (sym->kind) {
    case sk_macro:
#if RECORD_MACROS_IN_IL
      entry_ptr = (char *)sym->variant.macro_def->macro;
      if (entry_ptr != NULL) *kind = iek_macro;
#endif /* RECORD_MACROS_IN_IL */
      break;
    case sk_constant:
      entry_ptr = (char *)sym->variant.constant;
      *kind = iek_constant;
      break;
    case sk_type:
    case sk_enum_tag:
      entry_ptr = (char *)sym->variant.type;
      *kind = iek_type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      entry_ptr = (char *)sym->variant.class_struct_union.type;
      *kind = iek_type;
      break;
    case sk_variable:
      entry_ptr = (char *)sym->variant.variable.ptr;
      *kind = iek_variable;
      break;
    case sk_static_data_member:
      entry_ptr = (char *)sym->variant.static_data_member.variable;
      *kind = iek_variable;
      break;
    case sk_field:
      entry_ptr = (char *)sym->variant.field.ptr;
      *kind = iek_field;
      break;
    case sk_routine:
    case sk_member_function:
      entry_ptr = (char *)sym->variant.routine.ptr;
      *kind = iek_routine;
      break;
    case sk_label:
      entry_ptr = (char *)sym->variant.label.ptr;
      *kind = iek_label;
      break;
    default:;
      /* Other cases ignored. */
  }  /* switch */
#if CHECKING
  if (entry_ptr == NULL && *kind != iek_none) {
    internal_error("il_entry_for_symbol: NULL assoc IL entry ptr");
  }  /* if */
#endif /* CHECKING */
  return entry_ptr;
}  /* il_entry_for_symbol */


a_source_correspondence *source_corresp_entry_for_symbol(a_symbol_ptr sym_ptr)
/*
Return a pointer to the source correspondence entry in the IL entry
for the given symbol.  Return NULL if there isn't one (if that's
allowed for that kind of symbol).
*/
{
  char              *entity_ptr;
  an_il_entry_kind  entity_kind;

  entity_ptr = il_entry_for_symbol(sym_ptr, &entity_kind);
  return (entity_ptr == NULL ?
           NULL : source_corresp_for_il_entry(entity_ptr, entity_kind));
}  /* source_corresp_entry_for_symbol */


an_access_specifier access_for_symbol(a_symbol_ptr sym_ptr)
/*
Return the access specified in the symbol pointed to by sym_ptr.  This is
a low-level routine that just gets the access from the symbol, with
a special case for projection symbols and overloaded function symbols.
It cannot be used for checking access (see have_access_to_symbol).
*/
{
  an_access_specifier access;

  if (fundamental_symbol_of(sym_ptr)->kind ==
                                       (a_symbol_kind)sk_overloaded_function) {
    /* Overloaded function.  Cannot tell what the access is; leave it
       to be checked later.  This may not be necessary because overloaded
       functions shouldn't get into the main portion of the access checking
       code. */
    access = (an_access_specifier)as_public;
  } else if (sym_ptr->kind == (a_symbol_kind)sk_projection) {
    /* Projection symbol. */
    access = sym_ptr->variant.projection.access;
  } else {
    /* Normal symbol (not projection or overloaded function). */
    access = source_corresp_entry_for_symbol(sym_ptr)->access;
  }  /* if */
  return access;
}  /* access_for_symbol */


an_access_specifier compute_access(an_access_specifier sym_access,
                                   an_access_specifier deriv_access)
/*
Compute the access for a symbol projected from a base class, where sym_access
is its accessibility in the base class and deriv_access describes the class
derivation from the base class.

If the derivation access for the class is "public", the access of public
and protected members stays as it is; if it is "protected", public
members become protected and protected members are unaffected; if it is
"private", public and protected symbols become private.  Members private
to the base class become inaccessible to the derived class in every case.
(ARM 11.2).  The following table summarizes the transformations:

                 derivation:
                   inaccessible  private       protected     public
  symbol:        ------------------------------------------------------
    public       | inaccessible  private       protected     public
                 |
    protected    | inaccessible  private       protected     protected
                 |
    private      | inaccessible  inaccessible  inaccessible  inaccessible
                 |
    inaccessible | inaccessible  inaccessible  inaccessible  inaccessible

An "inaccessible" derivation is not a single derivation step; it is
a combination of several steps whose net effect is complete loss of
accessibility.
*/
{
  if (deriv_access == (an_access_specifier)as_inaccessible ||
      !is_more_accessible(sym_access, as_private)) {
    sym_access = (an_access_specifier)as_inaccessible;
  } else if (deriv_access == (an_access_specifier)as_private) {
    sym_access = (an_access_specifier)as_private;
  } else if (sym_access != deriv_access) {
    sym_access = (an_access_specifier)as_protected;
  }  /* if */
  /* Return the (possibly altered) symbol access specifier. */
  return sym_access;
}  /* compute_access */


an_access_specifier access_to_end_of_path(
                                        an_access_specifier         sym_access,
                                        a_derivation_step_ptr       path,
                                        a_base_class_derivation_ptr bcdp)
/*
Compute the accessibility (public, protected, private, inaccessible) to an
entity with access sym_access from the end of the derivation path pointed
to by "path".  This path is part of the path for the base class derivation
bcdp.
*/
{
  a_base_class_ptr            bcp;
  a_base_class_derivation_ptr pref_bcdp;

  if (path != NULL) {
    /* Use a recursive call to compute the access over all the steps
       after the first one. */
    if (path->next != NULL) {  /* Test is for speed. */
      sym_access = access_to_end_of_path(sym_access, path->next, bcdp);
    }  /* if */
    /* Now modify the access to account for the first step. */
    /* Virtual base classes get special handling, but not when they appear as
       the last step of derivations. */
    bcp = path->base_class;
    if (path->next == NULL) {
      /* This is the last step in the derivation for a base class.
         Add the effect of this step into the accumulated access.
         Use the derivation access specified in the header for the base
         class derivation (important for virtual base classes). */
      sym_access = compute_access(sym_access, bcdp->access);
    } else if (is_virtual_but_not_simple_direct_base_class(bcp)) {
      /* The first step is a virtual step which is more than a direct base
         class with a single derivation.  Compute the access over the
         preferred derivation (which has the best access). */
      pref_bcdp = preferred_virtual_derivation_of(bcp);
      sym_access = access_to_end_of_path(sym_access, pref_bcdp->path,
                                         pref_bcdp);
    } else {
      /* The first step is a nonvirtual step, or it's a simple virtual
         step.  Add the effect of this step into the accumulated access. */
      sym_access = compute_access(sym_access, bcp->derivation->access);
    }  /* if */
  }  /* if */
  return sym_access;
}  /* access_to_end_of_path */
  

static a_boolean on_befriending_list(a_class_list_entry_ptr befriending_list,
                                     a_type_ptr             class_type)
/*
Return TRUE if the class type indicated by class_type in on the list
of befriending classes given by befriending_list.
*/
{
  a_boolean on_list = FALSE;

  for (; befriending_list != NULL; befriending_list = befriending_list->next) {
    if (befriending_list->class_type == class_type) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* on_befriending_list */


static a_boolean have_member_access_from_class_scope(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep)
/*
We have member access privilege to the class indicated by the scope stack
entry pointed to by ssep.  Return TRUE if that fact means that we have
member access privilege to class_type.
*/
{
  a_boolean  have_member_privilege = FALSE;
  a_type_ptr scope_class = ssep->assoc_type;

  if (scope_class == class_type) {
    /* We are inside class_type. */
    have_member_privilege = TRUE;
  } else if (on_befriending_list(scope_class->variant.
                                            class_struct_union.extra_info->
                                                           befriending_classes,
                                 class_type)) {
    /* We are inside a class that is a friend of class_type. */
    have_member_privilege = TRUE;
  }  /* if */
  return have_member_privilege;
}  /* have_member_access_from_class_scope */


a_boolean have_member_access_privilege(a_type_ptr class_type)
/*
Return TRUE if we currently have member access privilege to the class
indicated by class_type.
*/
{
  a_boolean               have_member_privilege = FALSE;
  a_scope_stack_entry_ptr ssep;
  a_routine_ptr           scope_routine;
  a_scope_depth           scope_depth, scope_depth_to_skip = NO_SCOPE_DEPTH;

  /* This routine looks a lot like have_protected_member_access_privilege. */
  /* Consider each scope on the scope stack that affects access control.
     They are linked together on a list. */
  for (scope_depth = depth_of_innermost_scope_that_affects_access_control;
       scope_depth != NO_SCOPE_DEPTH;
       scope_depth = ssep->next_scope_that_affects_access_control) {
    ssep = &scope_stack[scope_depth];
    if (ssep->kind == (a_scope_kind)sck_function) {
      /* A function.  See if class_type is on its befriending list. */
      scope_routine = ssep->il_scope->variant.routine.ptr;
      if (on_befriending_list(scope_routine->befriending_classes,
                              class_type)) {
        /* We are inside a function that is a friend of class_type. */
        have_member_privilege = TRUE;
        break;
      }  /* if */
      /* The ARM says "Member functions of a nested class have no special
         access to members of an enclosing class".  It's not clear that's
         right or precise enough, but in strict mode, we'll do exactly that.
         For a member function in a nested class, set up to skip all
         the reactivations for the classes within which it is nested. */
      if (strict_ansi_mode &&
          (ssep-1)->kind == (a_scope_kind)sck_class_reactivation &&
          (ssep-2)->kind == (a_scope_kind)sck_class_reactivation) {
        scope_depth_to_skip = scope_depth - 2;
      }  /* if */
    } else if (ssep->kind == (a_scope_kind)sck_function_access) {
      /* A function access scope.  See if class_type is on its
         befriending list. */
      scope_routine = ssep->assoc_routine;
      if (on_befriending_list(scope_routine->befriending_classes,
                              class_type)) {
        /* We are inside a function that is a friend of class_type. */
        have_member_privilege = TRUE;
        break;
      }  /* if */
    } else {
      check_assertion_str(ssep->kind == (a_scope_kind)sck_class_struct_union ||
                          ssep->kind == (a_scope_kind)sck_class_reactivation,
                          "have_member_access_privilege: bad stack entry");
      /* A class or class reactivation.  Check for access granted by being
         a member of the class. */
      if (scope_depth == scope_depth_to_skip) {
        /* This is a reactivation that should be skipped (see comment above).
           If the entry below this one is also a reactivation, keep skipping.
           Otherwise, stop skipping. */
        if ((ssep-1)->kind == (a_scope_kind)sck_class_reactivation) {
          scope_depth_to_skip--;
        } else {
          scope_depth_to_skip = NO_SCOPE_DEPTH;
        }  /* if */
      } else if (have_member_access_from_class_scope(class_type, ssep)) {
        /* We are inside a class that gives us member access. */
        have_member_privilege = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return have_member_privilege;
}  /* have_member_access_privilege */


static a_boolean have_protected_access_from_derived_class(
                                                      a_type_ptr class_type,
                                                      a_type_ptr derived_class)
/*
Return TRUE if a protected member of class_type can be accessed from
derived_class.  We know that we have member access to derived_class but we
do not know if class_type is a base class of derived_class or if the derivation
between the two will allow access to a protected member.  This routine
is used is determining access to protected members and in the presence
of protected derivations.
*/
{
  a_boolean                   accessible = FALSE;
  a_base_class_ptr            bcp;
  a_base_class_derivation_ptr preferred_derivation;

  /* See if class_type is a base class of derived_class. */
  bcp = find_base_class_of(derived_class, class_type);
  if (bcp != NULL) {
    /* Yes.  See if the derivation steps are such that a protected member
       of the base class can be accessed in the derived class. */
    preferred_derivation = preferred_derivation_of(bcp);
    if (access_to_end_of_path((an_access_specifier)as_protected,
                              preferred_derivation->path,
                              preferred_derivation) !=
                                        (an_access_specifier)as_inaccessible) {
      accessible = TRUE;
    }  /* if */
  }  /* if */
  return accessible;
}  /* have_protected_access_from_derived_class */
  

static a_boolean have_protected_access_from_befriending_list(
                                       a_class_list_entry_ptr befriending_list,
                                       a_type_ptr             class_type)
/*
Return TRUE if protected members of class_type are accessible from
any class on the list of befriending classes given by befriending_list.
We know that we have member access to the classes on befriending_list but we
do not know if class_type is a base class of any of those classes or if the
derivations between the class_type and one of those classes will allow access
to a protected member.  This routine is used in determining access to
protected members and in the presence of protected derivations.
*/
{
  a_boolean accessible = FALSE;

  for (; befriending_list != NULL; befriending_list = befriending_list->next) {
    if (have_protected_access_from_derived_class(class_type,
                                               befriending_list->class_type)) {
      accessible = TRUE;
      break;
    }  /* if */
  }  /* for */
  return accessible;
}  /* have_protected_access_from_befriending_list */


static a_boolean have_protected_access_from_class_scope(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep)
/*
We have member access privilege to the class indicated by the scope stack
entry pointed to by ssep.  Return TRUE if that fact means that we have
access to protected members of class_type by virtue of having member
access privilege to a derived class of class_type.  This is needed in
determining accessibility to protected members and in the presence of
protected derivations.
*/
{
  a_boolean  have_protected_access = FALSE;
  a_type_ptr scope_class = ssep->assoc_type;

  if (have_protected_access_from_derived_class(class_type, scope_class)) {
    /* We are in a class that is an appropriate derived class of class_type,
       so we have access to protected members of class_type. */
    have_protected_access = TRUE;
  } else if (have_protected_access_from_befriending_list(scope_class->
                    variant.class_struct_union.extra_info->befriending_classes,
                                                                 class_type)) {
    /* We are in a class that is a friend of an appropriate derived class
       of class_type, so we have access to protected members of class_type. */
    have_protected_access = TRUE;
  }  /* if */
  return have_protected_access;
}  /* have_protected_access_from_class_scope */


a_boolean have_protected_member_access_privilege(a_type_ptr class_type)
/*
Return TRUE if we have access to protected members of class_type by
virtue of having member access privilege to a derived class of class_type.
This is needed in determining accessibility to protected members and
in the presence of protected derivations.  See p. 214 of "The C++
Programming Language", 2nd Edition.
*/
{
  a_boolean               have_protected_access = FALSE;
  a_scope_stack_entry_ptr ssep;
  a_routine_ptr           scope_routine;
  a_scope_depth           scope_depth, scope_depth_to_skip = NO_SCOPE_DEPTH;

  /* This routine looks a lot like have_member_access_privilege. */
  /* Consider each scope on the scope stack that affects access control.
     They are linked together on a list. */
  for (scope_depth = depth_of_innermost_scope_that_affects_access_control;
       scope_depth != NO_SCOPE_DEPTH;
       scope_depth = ssep->next_scope_that_affects_access_control) {
    ssep = &scope_stack[scope_depth];
    if (ssep->kind == (a_scope_kind)sck_function) {
      /* A function.  See if class_type is on its befriending list. */
      scope_routine = ssep->il_scope->variant.routine.ptr;
      if (have_protected_access_from_befriending_list(
                                            scope_routine->befriending_classes,
                                            class_type)) {
        /* We are inside a function that is a friend of class_type. */
        have_protected_access = TRUE;
        break;
      }  /* if */
      /* The ARM says "Member functions of a nested class have no special
         access to members of an enclosing class".  It's not clear that's
         right or precise enough, but in strict mode, we'll do exactly that.
         For a member function in a nested class, set up to skip all
         the reactivations for the classes within which it is nested. */
      if (strict_ansi_mode &&
          (ssep-1)->kind == (a_scope_kind)sck_class_reactivation &&
          (ssep-2)->kind == (a_scope_kind)sck_class_reactivation) {
        scope_depth_to_skip = scope_depth - 2;
      }  /* if */
    } else if (ssep->kind == (a_scope_kind)sck_function_access) {
      /* A function access scope.  See if class_type is on its
         befriending list. */
      scope_routine = ssep->assoc_routine;
      if (on_befriending_list(scope_routine->befriending_classes,
                              class_type)) {
        /* We are inside a function that is a friend of class_type. */
        have_protected_access = TRUE;
        break;
      }  /* if */
    } else {
      check_assertion_str(ssep->kind == (a_scope_kind)sck_class_struct_union ||
                          ssep->kind == (a_scope_kind)sck_class_reactivation,
                    "have_protected_member_access_privilege: bad stack entry");
      /* A class or class reactivation.  Check for access granted by being
         a member of the class. */
      if (scope_depth == scope_depth_to_skip) {
        /* This is a reactivation that should be skipped (see comment above).
           If the entry below this one is also a reactivation, keep skipping.
           Otherwise, stop skipping. */
        if ((ssep-1)->kind == (a_scope_kind)sck_class_reactivation) {
          scope_depth_to_skip--;
        } else {
          scope_depth_to_skip = NO_SCOPE_DEPTH;
        }  /* if */
      } else if (have_protected_access_from_class_scope(class_type, ssep)) {
        /* We are inside a class that gives us member access. */
        have_protected_access = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return have_protected_access;
}  /* have_protected_member_access_privilege */


/*
Entry used to keep track of stacking in processing virtual steps on
a derivation path in have_access_across_path.  These are allocated as
auto variables and chained together.
*/
typedef struct a_virtual_step_stack_entry *a_virtual_step_stack_entry_ptr;
typedef struct a_virtual_step_stack_entry {
  a_virtual_step_stack_entry_ptr
		next;	/* Next entry on the list. */
  a_derivation_step_ptr
		virtual_step;
			/* Derivation step for a virtual base class, being
			   expanded. */
  a_base_class_derivation_ptr
		derivation;
			/* The base class derivation of whose path virtual_step
			   is a step. */
} a_virtual_step_stack_entry;


static an_access_specifier access_to_end_of_virtual_step_stack(
                             an_access_specifier            access,
                             a_virtual_step_stack_entry_ptr virtual_step_stack)
/*
Determine and return the amount of access available to an entity with
access "access" across the concatenation of the derivation paths indicated
by the stack of entries in virtual_step_stack.
*/
{
  if (virtual_step_stack != NULL) {
    /* Do a recursive call to do all the path segments for the stack entries
       following the first one. */
    access = access_to_end_of_virtual_step_stack(access,
                                                 virtual_step_stack->next);
    /* Add in the access for the segment represented by the first stack
       entry. */
    access = access_to_end_of_path(access,
                                   virtual_step_stack->virtual_step->next,
                                   virtual_step_stack->derivation);
  }  /* if */
  return access;
}  /* access_to_end_of_virtual_step_stack */


static a_boolean have_access_across_path(
                             a_symbol_ptr                   fund_sym,
                             a_type_ptr                     viewpoint_class,
                             a_derivation_step_ptr          path,
                             a_base_class_derivation_ptr    bcdp,
                             a_symbol_ptr                   proj_sym,
                             a_virtual_step_stack_entry_ptr virtual_step_stack)
/*
Return TRUE if the symbol fund_sym is accessible at the current location
in the source program when viewed from the class viewpoint_class.
path is the derivation path from viewpoint_class to fund_sym;
it is NULL if fund_sym is in viewpoint_class.  If non-NULL, it is part of
the path of the base class derivation bcdp.  proj_sym is the projection
symbol from which we started this access check, or an updated one picked
up during the recursive descent through the derivation; it is ignored if
path == NULL, but otherwise it must be a projection symbol (although
its fundamental symbol might not be fund_sym, for example in the overloaded
function case).  virtual_step_stack is a pointer to a linked list that
describes a stack of virtual steps being expanded by invocations of
this routine above this one.
*/
{
  a_boolean             have_access = FALSE, base_class_accessible;
  a_boolean             need_to_compute_access;
  an_access_specifier   access, base_class_deriv;
  a_symbol_ptr          step_proj_sym;
  a_boolean             have_member_access, determined_member_access;
  a_boolean             have_protected_member_access;
  a_boolean             determined_protected_member_access;
  a_base_class_ptr      bcp;
  a_virtual_step_stack_entry
                        vsse;
  a_boolean             virtual_step;
  a_derivation_step_ptr path_next;

  /* Determine the effective access to the fundamental symbol from the
     viewpoint class. */
  if (path == NULL) {
    /* No derivation path, so the access is the access for the symbol. */
    access = access_for_symbol(fund_sym);
  } else {
    /* Determine the effective access in the viewpoint class. */
#if CHECKING
    if (proj_sym == NULL) {
      internal_error("have_access_across_path: proj_sym is NULL");
    }  /* if */
    if (proj_sym->kind != (a_symbol_kind)sk_projection) {
      internal_error("have_access_across_path: proj_sym not projection");
    }  /* if */
#endif /* CHECKING */
    need_to_compute_access = TRUE;
    if (proj_sym->class_of_which_a_member == viewpoint_class) {
      /* The step we are looking at is the first one, so the effective
         access is available from the projection symbol. */
      access = proj_sym->variant.projection.access;
      need_to_compute_access = FALSE;
    } else if (proj_sym->variant.projection.intervening_access_adjustment) {
      /* There is an access adjustment somewhere on the derivation path, so
         we must find the projection symbol that applies at this step of
         the path in case it is an access adjustment. */
      for (step_proj_sym = proj_sym->header->inactive_symbols;
           step_proj_sym != NULL;
           step_proj_sym = step_proj_sym->next) {
        if (step_proj_sym->class_of_which_a_member == viewpoint_class &&
            step_proj_sym->kind == (a_symbol_kind)sk_projection &&
            step_proj_sym->variant.projection.extra_info->fundamental_symbol ==
                                                                    fund_sym) {
          goto have_proj_sym;
        }  /* if */
      }  /* for */
      /* Not found on the inactive list, so check the active list. */
      for (step_proj_sym = proj_sym->header->symbol;
           step_proj_sym != NULL;
           step_proj_sym = step_proj_sym->next) {
        if (step_proj_sym->class_of_which_a_member == viewpoint_class &&
            step_proj_sym->kind == (a_symbol_kind)sk_projection &&
            step_proj_sym->variant.projection.extra_info->fundamental_symbol ==
                                                                    fund_sym) {
          goto have_proj_sym;
        }  /* if */
      }  /* for */
#if CHECKING
      internal_error("have_access_across_path: proj sym not found");
#endif /* CHECKING */
have_proj_sym:
      /* Replace the projection symbol we have by the new one.  Note that
         it will get passed down in the recursive call below, which is
         good, because once we get past the access adjustments we can use
         the faster technique. */
      proj_sym = step_proj_sym;
      access = proj_sym->variant.projection.access;
      need_to_compute_access = FALSE;
    }  /* if */
    if (!need_to_compute_access) {
      /* If the symbol is for an overloaded function, we can use the access
         computed only if the projection symbol is an access adjustment.
         Otherwise, the individual functions in the overload sets can have
         distinct access settings, and the projection symbol cannot
         indicate all of them. */
      if (proj_sym->variant.projection.extra_info->fundamental_symbol->kind ==
                                       (a_symbol_kind)sk_overloaded_function) {
        if (!proj_sym->variant.projection.access_adjustment_made) {
          need_to_compute_access = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (need_to_compute_access) {
      /* The access must be determined by looking at the derivation steps.
         This is probably a little faster than looking for the projection
         symbol. */
      access = access_for_symbol(fund_sym);
      /* Adjust the access for any path sequences indicated in the virtual
         step stack. */
      if (virtual_step_stack != NULL) {
        access = access_to_end_of_virtual_step_stack(access,
                                                     virtual_step_stack);
      }  /* if */
      /* Adjust the access for the path remaining after the steps indicated in
         the stack. */
      access = access_to_end_of_path(access, path, bcdp);
    }  /* if */
  }  /* if */
  /* We now have the effective access to the member in the viewpoint class,
     statically determined.  See if we have access. */
  /* The expensive determinations are only done if needed. */
  determined_member_access = determined_protected_member_access = FALSE;
  if (access == (an_access_specifier)as_public) {
    /* The member is public, so it is accessible. */
    have_access = TRUE;
  } else if (access != (an_access_specifier)as_inaccessible &&
             (determined_member_access = TRUE,
              have_member_access =
                              have_member_access_privilege(viewpoint_class))) {
    /* The member is not inaccessible (i.e., there is some access to it),
       and we have member access privilege to the class, so we have access
       to the member. */
    have_access = TRUE;
  } else if (access == (an_access_specifier)as_protected &&
             (determined_protected_member_access = TRUE,
              have_protected_member_access =
                    have_protected_member_access_privilege(viewpoint_class))) {
    /* The member is protected, and we have member access to a derived
       class of the viewpoint class, so we have access to the member.
       Note that this is more generous than the access allowed by ARM 11.5;
       additional checking in the expression routines is needed to enforce
       that restriction. */
    have_access = TRUE;
  } else {
    /* We do not have access to the member in this class, but perhaps we
       have access to it in a base class.  This would be because of some
       member access to a base class that does not figure into the
       general-case access determined above.  We walk down the path
       to the fundamental base class, continuing as long as the base
       class at each step is accessible from the original class, and we
       check for special access at each step. */
    if (path == NULL) {
      /* We're already in the class of the fundamental symbol, so we do
         not have access. */
      /* have_access = FALSE;  -- already set. */
    } else {
      /* Find the base class that's first on the path.  It most cases, that's
         trivial, but for virtual base classes we have to go to the virtual
         base class itself and run down its derivation (or derivations,
         as there may be several).  The final step on the derivation for
         a virtual base class is not treated specially -- it's just a simple
         step to that base class. */
      bcp = path->base_class;
      virtual_step = FALSE;
      if (bcp->is_virtual && path->next != NULL) {
        /* Virtual step.  We have to examine the various derivations for
           the virtual base class.  Add an entry to the stack of virtual step
           entries being processed.  This stack is used when the other end of
           the virtual base class derivation is reached, to know where to
           continue on the derivation path following this virtual step. */
        vsse.next = virtual_step_stack;
        vsse.virtual_step = path;
        vsse.derivation = bcdp;
        virtual_step_stack = &vsse;
        virtual_step = TRUE;
        /* The loop will go through all the derivations of the virtual
           base class.  Start with the first.  It doesn't seem necessary to
           start with the preferred derivation, since we've already failed
           to obtain access in the usual way over the preferred derivation.
           Any access we get now is going to be unusual in some way. */
        bcdp = bcp->derivation;
        path = bcdp->path;
      }  /* if */
      /* Loop through the derivation paths to be considered.  There is more
         than one path only in the virtual step case. */
      for (;;) {
        /* Determine whether or not the base class is accessible.  A base class
           is accessible if its public members are accessible from the derived
           class.  This is like the macro is_accessible_imm_base_class, but
           optimized to use whatever we've already determined about member
           access to the viewpoint class. */
        /* Get the derivation access for this derivation step.  If this step
           is the last on a derivation, get the access from the base class
           derivation entry (important for virtual base classes). */
        path_next = path->next;
        if (path_next == NULL) {
          base_class_deriv = bcdp->access;
        } else {
          base_class_deriv = path->base_class->derivation->access;
        }  /* if */
        base_class_accessible = FALSE;
        if (base_class_deriv == (an_access_specifier)as_public) {
          /* The base class is public, so it is accessible. */
          base_class_accessible = TRUE;
        } else {
          /* See if we have member access privilege to the viewpoint class. */
          if (!determined_member_access) {
            determined_member_access = TRUE;
            have_member_access = have_member_access_privilege(viewpoint_class);
          }  /* if */
          if (have_member_access) {
            /* We have member access to the viewpoint class, so the base class
               is accessible regardless of the type of derivation. */
            base_class_accessible = TRUE;
          } else {
            /* See if special protected member access privilege applies.  This
               is only meaningful when the base class derivation is
               protected. */
            if (base_class_deriv == (an_access_specifier)as_protected) {
              if (!determined_protected_member_access) {
                determined_protected_member_access = TRUE;
                have_protected_member_access =
                       have_protected_member_access_privilege(viewpoint_class);
              }  /* if */
              if (have_protected_member_access) {
                base_class_accessible = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        if (base_class_accessible) {
          /* The base class is accessible, so we want to do a recursive call
             to check accessibility at the next step.  Determine the path
             for the next step.  Usually, it's just path->next, already in
             path_next. */
          a_virtual_step_stack_entry_ptr local_virtual_step_stack =
                                                            virtual_step_stack;
          a_base_class_derivation_ptr    local_bcdp = bcdp;
          while (path_next == NULL && virtual_step_stack != NULL) {
            /* At the end of the derivation path for a virtual step, continue
               with the step following the virtual step (up one level in
               the stack). */
            path_next = virtual_step_stack->virtual_step->next;
            local_bcdp = virtual_step_stack->derivation;
            local_virtual_step_stack = virtual_step_stack->next;
          }  /* while */
          /* Do a recursive call to see if the member is accessible in the
             base class. */
          if (have_access_across_path(fund_sym, bcp->type, path_next,
                                      local_bcdp, proj_sym,
                                      local_virtual_step_stack)) {
            /* Yes, it is. */
            have_access = TRUE;
          }  /* if */
        }  /* if */
        /* Loop only for the virtual step case. */
        if (!virtual_step) break;
        bcdp = bcdp->next;
        /* Stop after the last derivation for the virtual step case. */
        /* Note that we do not have to take the stack entry off the stack
           or restore bcdp et al., since we have not affected the caller's
           variables.  If there were more processing to be done in this
           routine, that might be a good thing to do. */
        if (bcdp == NULL) break;
        /* Loop for another derivation. */
        path = bcdp->path;
      }  /* for */
    }  /* if */
  }  /* if */
  return have_access;
}  /* have_access_across_path */


static a_boolean have_access_across_derivations(a_symbol_ptr symbol,
                                                a_symbol_ptr view_sym)
/*
Return TRUE if the symbol "symbol" is accessible at the current location
in the source program when viewed from the class of which view_sym is a
member.  view_sym is either the same as the fundamental symbol of "symbol",
or is an overloaded function symbol containing that fundamental symbol,
or is a projection symbol for one of those.
*/
{
  a_boolean                   have_access = FALSE;
  a_base_class_ptr            bcp;
  a_base_class_derivation_ptr derivations, preferred_derivation, bcdp;
  a_derivation_step_ptr       preferred_path;
  a_type_ptr                  viewpoint_class =
                                             view_sym->class_of_which_a_member;

  symbol = fundamental_symbol_of(symbol);
  if (view_sym->kind == (a_symbol_kind)sk_projection) {
    /* The view symbol is a projection symbol. */
    bcp = view_sym->variant.projection.extra_info->fundamental_base_class;
    derivations = bcp->derivation;
    preferred_derivation = preferred_derivation_of(bcp);
    preferred_path = preferred_derivation->path;
  } else {
    /* The view symbol is not a projection symbol, so the view class is the
       same as the class of the viewed symbol. */
    derivations = preferred_derivation = NULL;
    preferred_path = NULL;
  }  /* if */
  /* Check the preferred derivation (the one that gives the most access
     statically).  preferred_derivation and preferred_path are NULL if the
     view class is the same class as the class of the viewed symbol. */
  if (have_access_across_path(symbol, viewpoint_class,
                              preferred_path, preferred_derivation,
                              view_sym,
                              (a_virtual_step_stack_entry_ptr)NULL)) {
    /* The preferred derivation gives access. */
    have_access = TRUE;
  } else {
    /* The preferred derivation does not give access.  Check all the other
       derivations, if any.  Only virtual base classes can have more than
       one derivation. */
    for (bcdp = derivations; bcdp != NULL; bcdp = bcdp->next) {
      if (!bcdp->preferred) {
        if (have_access_across_path(symbol, viewpoint_class, bcdp->path, bcdp,
                                    view_sym,
                                    (a_virtual_step_stack_entry_ptr)NULL)) {
          /* This derivation gives access. */
          have_access = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return have_access;
}  /* have_access_across_derivations */


a_boolean have_access_to_symbol(a_symbol_ptr symbol)
/*
Return TRUE if the indicated symbol is accessible from the current location
in the source program.
*/
{
  a_boolean have_access = have_access_across_derivations(symbol, symbol);
  return have_access;
}  /* have_access_to_symbol */


static void issue_access_error(a_symbol_ptr       sym,
                               a_source_position  *err_pos)
/*
Issue the appropriate error on the inaccessibility of sym.
*/
{
  an_error_code  	error_code = ec_no_access_to_name;
  an_error_severity	error_severity = es_discretionary_error;
  a_routine_ptr  rp;

  if (is_function_symbol(sym)) {
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      rp = sym->variant.overloaded_function.symbols->variant.routine.ptr;
    } else {
      rp = sym->variant.routine.ptr;
    }  /* if */
    if (rp->special_kind == (a_special_function_kind)sfk_constructor ||
        rp->special_kind == (a_special_function_kind)sfk_destructor ||
        rp->special_kind == (a_special_function_kind)sfk_conversion ||
        (rp->special_kind == (a_special_function_kind)sfk_operator &&
         rp->opname_kind == (an_opname_kind)onk_assign)) {
      error_code = ec_inaccessible_special_function;
    }  /* if */
  } else if (is_type_symbol(sym)) {
    if (any_cfront_mode()) {
      /* In cfront mode access errors on types are only warnings.  cfront
         doesn't check access to types at all. */
      error_severity = es_warning;
      error_code = ec_no_access_to_type_cfront_mode;
    }  /* if */
  }  /* if */
  pos_sy_diagnostic(error_severity, error_code, err_pos, sym);
}  /* issue_access_error */


static an_access_error_descr_ptr alloc_access_error_descr(void)
/*
Allocate an access error description entry.  Reuse a freed entry if possible.
*/
{
  an_access_error_descr_ptr aedp;

  if (avail_access_error_descrs != NULL) {
    /* Reuse a freed entry. */
    aedp = avail_access_error_descrs;
    avail_access_error_descrs = avail_access_error_descrs->next;
  } else {
    /* Allocate a new entry. */
    aedp = (an_access_error_descr_ptr)alloc_fe(sizeof(an_access_error_descr));
#if DEBUG
    num_access_error_descrs_allocated++;
#endif /* DEBUG */
  }  /* if */
  aedp->next = NULL;
  aedp->sym = NULL;
  aedp->position = pos_curr_token;
  aedp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  return aedp;
}  /* alloc_access_error_descr */
	

static void free_access_error_descr(an_access_error_descr_ptr aedp)
/*
Free the access error description entry pointed to by aedp.
Put the freed entry on the available list to be reused.
*/
{
  aedp->next = avail_access_error_descrs;
  avail_access_error_descrs = aedp;
}  /* free_access_error_descr */


void member_check_ambiguity_and_verify_access
			(a_symbol_locator		*locator)
/*
Verify that the indicated member symbol is not ambiguous and that we have
access to it.  In case of an ambiguity, the locator is set to an error
locator.  Note that no access checking is done on overloaded function symbols.

If an error is detected, the scope stack is consulted to see if access
errors should be deferred and rechecked later.  If so, an access
error descriptor is added to the list on the scope stack.  Otherwise,
the error is issued immediately.
*/
{
  a_symbol_ptr   sym = locator->specific_symbol;

  /* This routine looks like overload_check_ambiguity_and_verify_access. */
  /* Issue an error if the symbol is ambiguous.  Only a symbol projected
     into a derived class by inheritance can be ambiguous.  Ambiguity checking
     must precede access control (ARM, 10.1.1). */
  if (sym->kind == (a_symbol_kind)sk_projection &&
      sym->variant.projection.ambiguous) {
    pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
    set_to_error_locator(*locator);
  } else if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Suppress access checking during prototype instantiations.  Access
       checking cannot be done for a template, only for instances. */
  } else if (fundamental_symbol_of(sym)->kind !=
	                            (a_symbol_kind)sk_overloaded_function &&
             !have_access_to_symbol(sym)) {
    a_boolean			defer_access_checks = FALSE;
    a_scope_stack_entry_ptr	ssep;

    if (curr_deferred_access_scope != NO_SCOPE_DEPTH) {
      ssep = &scope_stack[curr_deferred_access_scope];
      defer_access_checks = ssep->defer_access_checks;
    }  /* if */
    /* The symbol is not accessible. */
    if (!defer_access_checks) {
      issue_access_error(fundamental_symbol_of(sym),
                         &locator->source_position);
      locator->access_control_error_reported = TRUE;
    } else {
      an_access_error_descr_ptr	aedp;
      aedp = alloc_access_error_descr();
      aedp->sym = sym;
      aedp->position = locator->source_position;
      aedp->token_sequence_number = curr_token_sequence_number;
      if (ssep->deferred_access_checks == NULL) {
        ssep->deferred_access_checks = aedp;
      }  /* if */
      if (ssep->last_deferred_access_check != NULL) {
        ssep->last_deferred_access_check->next = aedp;
      }  /* if */
      ssep->last_deferred_access_check = aedp;
    }  /* if */
  }  /* if */
}  /* member_check_ambiguity_verify_access */


void perform_deferred_access_checks(void)
/*
Go through the list of deferred access checks and repeat the test.  If
the symbol is still not accessible, the entry may either stay on the
list (if the defer_access_checks flag is still set) or an error may be
issued.  The ability to retain failed checks on the list is needed because
the deferred access checks need to be done in two phases.  First,
member access is checked during declarator processing when the class
reactivation scope has been pushed.  Later, after the entire
function declaration has been processed, we need to check for friend
access.
*/
{
  a_scope_stack_entry_ptr	ssep;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  if (ssep->deferred_access_checks != NULL) {
    an_access_error_descr_ptr	aedp = ssep->deferred_access_checks;
    an_access_error_descr_ptr	new_head = NULL;
    an_access_error_descr_ptr	new_tail = NULL;
    an_access_error_descr_ptr	next_aedp;
    a_boolean			remove = TRUE;
    if (aedp != NULL) {
      for (; aedp != NULL; aedp = next_aedp) {
        next_aedp = aedp->next;
        aedp->next = NULL;
        if (!have_access_to_symbol(aedp->sym)) {
          /* The access check still failed. */
          if (ssep->defer_access_checks) {
            /* Keep the entry on the list. */
            remove = FALSE;
          } else {
            issue_access_error(fundamental_symbol_of(aedp->sym),
                               &aedp->position);
          }  /* if */
        }  /* if */
        if (remove) {
          free_access_error_descr(aedp);
        } else {
          /* If we are keeping the entry, add it to the new list. */
          if (new_head == NULL) new_head = aedp;
          if (new_tail != NULL) new_tail->next = aedp;
          new_tail = aedp;
        }  /* if */
      }  /* for */
      ssep->deferred_access_checks = new_head;
      ssep->last_deferred_access_check = new_tail;
    }  /* if */
  }  /* if */
}  /* perform_deferred_access_checks */


void perform_deferred_access_checks_for_function(a_routine_ptr rp)
/*
Push a function access scope and retry any failed access checks
that were encountered while the function declaration was being 
scanned.  This causes the accessibility to be reevaluated taking
into account possible friendship relationships.  rp points to the
routine entry of the function that was declared.
*/
{
  a_scope_stack_entry_ptr	ssep;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  /* This routine is always called last, so we can reset this flag now. */
  ssep->defer_access_checks = FALSE;
  if (ssep->deferred_access_checks != NULL) {
    if (ssep->deferred_access_checks != NULL) {
      a_type_ptr	cowam_type;
      cowam_type = rp->source_corresp.class_of_which_a_member;
      if (cowam_type != NULL) push_class_reactivation_scope(cowam_type);
      (void)push_scope((a_scope_kind)sck_function_access, NO_SCOPE_NUMBER,
                       (a_type_ptr)NULL, rp);
      perform_deferred_access_checks();
      pop_scope();
      if (cowam_type != NULL) pop_class_reactivation_scope();
    }  /* if */
  }  /* if */
}  /* perform_deferred_access_checks_for_function */


void f_discard_deferred_access_checks(void)
/*
Free any deferred access checks that may have been created and clear
the list pointers.
*/
{
  a_scope_stack_entry_ptr	ssep;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  if (ssep->deferred_access_checks != NULL) {
    an_access_error_descr_ptr	aedp = ssep->deferred_access_checks;
    an_access_error_descr_ptr	next_aedp;
    for (; aedp != NULL; aedp = next_aedp) {
      next_aedp = aedp->next;
      free_access_error_descr(aedp);
    }  /* for */
    ssep->deferred_access_checks = NULL;
    ssep->last_deferred_access_check = NULL;
  }  /* if */
}  /* f_discard_deferred_access_checks */


void discard_declarator_access_errors(void)
/*
Discard any deferred access checks that were recorded while scanning the
declarator name.  The current token must be the coalesced declarator
identifier at which point curr_token_sequence is the number of the first
token that comprised the generalized identifier.  All tokens after
the start of the declarator and before the next token are assumed to
be part of the declarator name.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_token_sequence_number	next_tok_seq_number;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  if (ssep->deferred_access_checks != NULL) {
    an_access_error_descr_ptr	aedp = ssep->deferred_access_checks;
    an_access_error_descr_ptr	new_head = NULL;
    an_access_error_descr_ptr	new_tail = NULL;
    an_access_error_descr_ptr	next_aedp;
    /* Get the sequence number associated with the next token. */
    (void)next_token_with_seq_number(&next_tok_seq_number);
    for (; aedp != NULL; aedp = next_aedp) {
      next_aedp = aedp->next;
      aedp->next = NULL;
      if (aedp->token_sequence_number >= curr_token_sequence_number &&
          aedp->token_sequence_number < next_tok_seq_number) {
        free_access_error_descr(aedp);
      } else {
        /* If we are keeping the entry, add it to the new list. */
        if (new_head == NULL) new_head = aedp;
        if (new_tail != NULL) new_tail->next = aedp;
        new_tail = aedp;
      }  /* if */
    }  /* for */
    ssep->deferred_access_checks = new_head;
    ssep->last_deferred_access_check = new_tail;
  }  /* if */
}  /* discard_declarator_access_errors */


void overload_check_ambiguity_and_verify_access(
                                            a_symbol_locator *locator,
                                            a_symbol_ptr     overloaded_symbol)
/*
Verify that the function symbol indicated in the locator (a specific
function from an overload set) is not ambiguous and that we have
access to it when it is viewed from the vantage point of overloaded_symbol;
issue an error if appropriate.  In case of an ambiguity,
the locator is set to an error locator.  overloaded_symbol is either
the sk_overloaded_function symbol containing the locator symbol, or
a projection symbol pointing to that sk_overloaded_function symbol.
*/
{
  /* This routine looks like member_check_ambiguity_verify_access. */
  if (overloaded_symbol->class_of_which_a_member == NULL) {
    /* Non-class-members cannot be ambiguous and are always accessible. */
  } else {
    /* Issue an error if the symbol is ambiguous.  Only a symbol projected
       into a derived class by inheritance can be ambiguous.  Ambiguity
       checking must precede access control (ARM, 10.1.1). */
    if (overloaded_symbol->kind == (a_symbol_kind)sk_projection &&
        overloaded_symbol->variant.projection.ambiguous) {
      pos_sy_error(ec_ambiguous_name, &locator->source_position,
                   overloaded_symbol);
      set_to_error_locator(*locator);
    } else {
      /* See if we have access to the symbol. */
      if (!have_access_across_derivations(locator->specific_symbol,
                                          overloaded_symbol)) {
        /* The symbol is not accessible. */
        issue_access_error(fundamental_symbol_of(locator->specific_symbol),
                           &locator->source_position);
        locator->access_control_error_reported = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* overload_check_ambiguity_and_verify_access */


a_boolean uniform_access_of_overloaded_function(a_symbol_ptr  sym)
/*
Given overloaded function symbol sym, return TRUE if all the functions have
the same access, FALSE otherwise.
*/
{
  an_access_specifier  access;
  a_boolean            all_have_same_access = TRUE;

#if CHECKING
  if (sym->kind != (a_symbol_kind)sk_overloaded_function) {
    internal_error("max_access_of_overloaded_functions: bad symbol kind");
  }  /* if */
#endif /* CHECKING */
  sym = sym->variant.overloaded_function.symbols;
  access = access_for_symbol(sym);
  while ((sym = sym->next) != NULL) {
    if (access != access_for_symbol(sym)) {
      all_have_same_access = FALSE;
      break;
    }  /* if */
  }  /* while */
  return all_have_same_access;
}  /* uniform_access_of_overloaded_function */


static an_access_specifier max_access_of_overloaded_function(a_symbol_ptr  sym)
/*
Given overloaded function symbol sym, return in *max_access the access control
value of the most accessible of the functions.
*/
{
  an_access_specifier  access, max_access;

#if CHECKING
  if (sym->kind != (a_symbol_kind)sk_overloaded_function) {
    internal_error("max_access_of_overloaded_functions: bad symbol kind");
  }  /* if */
#endif /* CHECKING */
  sym = sym->variant.overloaded_function.symbols;
  max_access = access_for_symbol(sym);
  while ((sym = sym->next) != NULL) {
    access = access_for_symbol(sym);
    if (is_more_accessible(access, max_access)) max_access = access;
  }  /* while */
  return max_access;
}  /* max_access_of_overloaded_function */


void f_check_protected_member_access(a_symbol_ptr      sym_param,
				     a_source_position *err_pos,
                                     a_type_ptr        access_class)
/*
This routine implements the access control check mandated by ARM 11.5, which
requires that a protected member be accessed only through a pointer or
object of a type to which we have member access (or a derived type thereof).
locator is a locator for the member symbol being referenced.  access_class
is the class of the pointer or object through which the member is being
accessed.  access_class is NULL if we don't know the object type (which
will cause an error).  access_class may also be an error type (which will
cause no error).  *err_pos is the source position for an error.  See the
macro check_protected_member_access for a convenient way to invoke this
function.
*/
{
  a_boolean                   have_access;
  a_symbol_ptr		      sym = fundamental_symbol_of(sym_param);
  a_type_ptr                  base_class, class_type;
  a_base_class_ptr            bcp;
  a_derivation_step_ptr       dsp;
  a_base_class_derivation_ptr bcdp;

  if (access_class == NULL) {
    /* Class is unknown; error. */
    have_access = FALSE;
  } else if (is_error_type(access_class)) {
    /* Class is an error type; no error. */
    have_access = TRUE;
  } else {
    /* Get the class of the symbol being referenced. */
    base_class = sym->class_of_which_a_member;
    /* Try to find a class class_type such that
         (1)  class_type is on the derivation list between base_class
              and access_class.  That is,
                base_class is a base class of
                    ^
                    |
                class_type, which is a base class of
                    ^
                    |
                access_class.
              class_type can be the same as either of the other
              classes.  In fact, all three can be the same.
              It is already known that base_class is a base class of
              access_class or is the same class.
         (2)  We have member access to class_type.
    */
    if (have_member_access_privilege(access_class)) {
      /* We have member access to the access_class, so we've found our
         class_type. */
      class_type = access_class;
      have_access = TRUE;
    } else if (access_class == base_class) {
      /* The endpoints are the same class, so there is no class that meets
         the requirement (we tested the only possible class above). */
      have_access = FALSE;
    } else {
      /* The endpoints are not the same, so examine the classes on the
         derivation(s) between them. */
      bcp = find_base_class_of(access_class, base_class);
#if CHECKING
      if (bcp == NULL) {
        internal_error(
                      "f_check_protected_member_access: base class not found");
      }  /* if */
#endif /* CHECKING */
      /* For each derivation (virtual base classes can have more than one): */
      for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
        /* For each step on the derivation path, check to see if we have
           member access to the class. */
        for (dsp = bcdp->path; dsp != NULL; dsp = dsp->next) {
          class_type = dsp->base_class->type;
          if (have_member_access_privilege(class_type)) {
            /* Found a class_type that satisfies our requirements, so we
               have access. */
            have_access = TRUE;
            goto have_accessibility;
          }  /* if */
        }  /* for */
      }  /* for */
      /* We did not find a suitable class, so we do not have access. */
      have_access = FALSE;
    }  /* if */
  }  /* if */
have_accessibility:
  if (!have_access) {
    pos_syty_error(ec_protected_access_problem, err_pos, sym, access_class);
  }  /* if */
}  /* f_check_protected_member_access */


a_boolean is_accessible_base_class(a_base_class_ptr bcp)
/*
Return TRUE if the base class indicated by bcp is accessible from the
current point in the program, relative to the class of which it is a
base class.  bcp need not be an immediate base class of its derived
class.
*/
{
  a_boolean                   accessible = TRUE;
  a_derivation_step_ptr       dsp;
  a_base_class_ptr            base_class;
  a_type_ptr                  curr_type;

  curr_type = bcp->derived_class;
  if (bcp->is_virtual) {
    /* Use a special subroutine for a virtual base class. */
    accessible = is_accessible_virtual_base_class(bcp, curr_type);
  } else {
    /* Non-virtual base class. */
    for (dsp = bcp->derivation->path; dsp != NULL; dsp = dsp->next) {
      base_class = dsp->base_class;
      if (!is_accessible_imm_base_class(base_class, curr_type)) {
        accessible = FALSE;
        break;
      }  /* if */
      curr_type = base_class->type;
    }  /* for */
  }  /* if */
  return accessible;
}  /* is_accessible_base_class */


a_boolean is_accessible_virtual_base_class(a_base_class_ptr bcp,
                                           a_type_ptr       viewpoint_class)
/*
Return TRUE if the base class bcp (a virtual base class) is accessible from
the current point in the program, relative to viewpoint_class.
*/
{
  a_boolean                   accessible = FALSE, last_step;
  a_base_class_derivation_ptr bcdp, step_bcdp;
  a_derivation_step_ptr       dsp;
  a_base_class_ptr            base_class;
  a_type_ptr                  curr_type;

  check_assertion(bcp->is_virtual);
  /* A virtual base class can have multiple derivations.  Loop through
     each derivation in turn. */
  for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
    curr_type = viewpoint_class;
    /* Look through the path of the derivation. */
    for (dsp = bcdp->path; dsp != NULL; dsp = dsp->next) {
      base_class = dsp->base_class;
      /* See if the base class at this step is accessible. */
      /* Virtual steps cause recursive calls, but treat the last step
         as a direct base class and as the specific derivation of the base
         class even if it is virtual. */
      last_step = (dsp->next == NULL);
      step_bcdp = last_step ? bcdp : base_class->derivation;
      if ((!last_step &&
           is_virtual_but_not_simple_direct_base_class(base_class)) ?
          /* Non-simple virtual base class, not last step. */
          is_accessible_virtual_base_class(base_class, curr_type) :
          /* Simple direct base class, or last step on derivation. */
          is_accessible_direct_base_class_derivation(step_bcdp, curr_type)) {
        /* Base class is accessible, so keep going on the path for this
           derivation. */
      } else {
        /* Base class is not accessible, so go on to the next derivation. */
        goto next_derivation;
      }  /* if */
      curr_type = base_class->type;
    }  /* for */
    /* We've found a derivation that gives access, so we can stop now. */
    accessible = TRUE;
    break;
next_derivation:;
  }  /* for */
  return accessible;
}  /* is_accessible_virtual_base_class */


/* Declaration needed because of mutual recursion: */
static a_symbol_ptr find_progenitor_symbol(a_type_ptr            class_ptr,
                                           a_symbol_locator      *locator,
                                           a_boolean             must_be_tag,
                                           a_derivation_step_ptr *path,
                                           an_access_specifier   *access,
                                           a_boolean             *ambiguous);


static a_symbol_ptr symbol_projected_from_base_class(
                                          a_base_class_ptr      base_class,
                                          a_symbol_locator      *locator,
                                          a_boolean             must_be_tag,
                                          a_derivation_step_ptr *path,
                                          an_access_specifier   *access,
                                          a_boolean             *ambiguous)
/*
Given a pointer to a base class (the "current class") and a locator, determine
whether the name specified in the locator is either defined in the class or
has a progenitor in a class from which the current class is derived.  If either
is found, update *path (the derivation path, starting from the current base
class) and the access specification *access.  base_class is a direct base
class of its derived class.
*/
{
  a_symbol_ptr     sym, tag_sym;
  a_scope_ptr      scope;

  db_enter(4, "symbol_projected_from_base_class");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for \"%s\" in base class \"%s\"\n",
                     locator->symbol_header->identifier,
                     base_class->type->source_corresp.name);
  }  /* if */
#endif /* DEBUG */
  /* First look in the scope of the base class itself. */
  scope = base_class->type->variant.class_struct_union.extra_info->assoc_scope;
  if (scope == NULL) {
    /* This is probably a nonreal class encountered during a prototype
       instantiation.  Ignore it. */
    sym = NULL;
  } else {
    /* We need search only the inactive symbols list, since a class cannot be
       declared as a base class unless it has been fully defined (at which
       point each of its member symbols is moved off the active list and onto
       the inactive list).  class_qualified_id_lookup is not called for two
       reasons:  to avoid unnecessary overhead and to prevent extra projection
       symbols from being created. */
    sym = inactive_symbol_list_from_locator(*locator);
    tag_sym = NULL;
    for (; sym != NULL; sym = sym->next) {
      if (sym->decl_scope == scope->number) {
        if (is_tag_symbol(fundamental_symbol_of(sym))) {
          if (must_be_tag) {
            /* Tag symbol is required and that's what we have. */
            break;
          } else {
            /* Tag and nontag symbols can coexist in the same scope, and the
               latter are preferred, so keep looking -- but remember the tag
               symbol in case no other is found. */
            tag_sym = sym;
          }  /* if */
        } else {
          if (must_be_tag) {
            /* A tag symbol is required but this isn't one.  Keep looking. */
          } else {
            /* Found a match. */
#if CHECKING
            /* In C++ mode fields are always in the nsk_other name space. */
            if (name_space_for_symbol_kind[(int)sym->kind] != nsk_other) {
              internal_error(
               "symbol_projected_from_base_class: unexpected name space kind");
            }  /* if */
#endif /* CHECKING */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (sym == NULL) {
      sym = tag_sym;
    } else if (is_destructor_symbol(sym)) {
      /* A destructor cannot be inherited (ARM 12.4) so don't make a projection
         symbol for it. */
      sym = NULL;
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    /* Found in the base class itself. */
#if DEBUG
    if (debug_level >= 4) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      *access = max_access_of_overloaded_function(sym);
    } else if (sym->kind == (a_symbol_kind)sk_projection) {
      *ambiguous = sym->variant.projection.ambiguous;
      *access = sym->variant.projection.access;
    } else {
      *access = access_for_symbol(sym);
    }  /* if */
  } else {
    /* Not found in the base class, so examine its own base classes, if any. */
    sym = find_progenitor_symbol(base_class->type, locator, must_be_tag,
                                 path, access, ambiguous);
  }  /* if */
  if (sym != NULL) {
    /* Some symbol was found.  Determine its derivation and access
       specification. */
    /* The path is not augmented if it starts with a virtual base class,
       unless it is only a single step.  This is consistent with the way
       derivations are constructed for base classes: the steps between the
       most derived class and an intermediate virtual base class are elided. */
    if (*path == NULL || (*path)->next == NULL ||
        !(*path)->base_class->is_virtual) {
      *path = make_derivation_step(base_class, *path);
    }  /* if */
    *access = compute_access(*access,
                             preferred_derivation_of(base_class)->access);
  }  /* if */
  db_exit();
  return sym;
}  /* symbol_projected_from_base_class */


static a_derivation_step_ptr path_to_fundamental_symbol_base_class
                                              (a_symbol_ptr      sym,
                                               a_base_class_ptr  disambiguator)
/*
sym is a projection symbol.  Disambiguator is a base class of the current
most derived class that is intermediate between the base class we are looking
for and the derived class.  What we're looking for is the base class in the
derived class that corresponds to the base class associated with sym's
fundamental symbol.  Return the preferred derivation of that base class.
*/
{
  a_type_ptr             tp;
  a_base_class_ptr       bcp;
  a_derivation_step_ptr  path = NULL;

  /* Note that corresponding_base_class is not called, since it is hard to
     compute a disambiguator that is a immediately derived from the base
     class we're looking for. */
  tp = sym->variant.projection.extra_info->fundamental_base_class->type;
  bcp = base_classes_of(disambiguator->derived_class);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->type == tp) {
      /* A base class with the right type. */
      if (!bcp->ambiguous || is_on_any_derivation_of(bcp, disambiguator)) {
        /* Either unambiguous or disambiguated. */
        path = preferred_derivation_of(bcp)->path;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
#if CHECKING
  if (path == NULL) {
    /* We shouldn't fail to find a corresponding base class. */
    internal_error("path_to_fundamental_symbol_base_class: not found");
  } else if (bcp->ambiguous) {
    /* Check to be sure the disambiguation was correct -- look at the rest of
       the base classes to see if any would also qualify. */
    for (bcp = bcp->next; bcp != NULL; bcp = bcp->next) {
      if (bcp->type == tp && is_on_any_derivation_of(bcp, disambiguator)) {
        internal_error(
                    "path_to_fundamental_symbol_base_class: ambiguous deriv");
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* CHECKING */
  return path;
}  /* path_to_fundamental_symbol_base_class */


static a_boolean projections_are_equivalent(a_symbol_ptr           sym1,
                                            a_derivation_step_ptr  path1,
                                            a_symbol_ptr           sym2,
                                            a_derivation_step_ptr  path2)
/*
Given two symbols sym1 and sym2 projected into the same class from two
different base classes, with derivations path1 and path2, respectively,
return TRUE if their respective fundamental symbols are the same (not
only the same members of the same class but with equivalent derivations).
*/
{
  a_boolean              equiv = FALSE;
  a_symbol_ptr           fundamental_sym1;
  a_type_ptr             rout_type;
  a_derivation_step_ptr  tail1, tail2;

  db_enter(4, "projections_are_equivalent");
  fundamental_sym1 = fundamental_symbol_of(sym1);
  if (fundamental_sym1 == fundamental_symbol_of(sym2)) {
    /* Fundamental symbols are the same.  Set equiv to TRUE if they
       represent the same function, object, type, or enumerator (ARM 10.1.1).
       In other words, if they are independent of a class object, they are
       equivalent (any path to a static data member, for example, gets to
       the same object) or if they are dependent on the same class object
       (e.g., if a field belongs to a virtual base class). */
    switch (fundamental_sym1->kind) {
      case sk_field:
        /* Nonstatic data member.  Equivalence must be determined by comparing
           the paths to the subclass object. */
        break;
      case sk_overloaded_function:
        /* Overloaded function.  If there are any nonstatic member functions,
           we must compare the paths. */
        if (fundamental_sym1->
                      variant.overloaded_function.mixed_static_nonstatic) {
          /* One or more is a nonstatic member function. */
          break;
        } else {
          /* Either all are static or all are nonstatic.  Check the first in
             the list. */
          rout_type = fundamental_sym1->variant.overloaded_function.symbols->
                                                    variant.routine.ptr->type;
          goto check_rout_type;
        }
      case sk_member_function:
        /* See if the member function is static.  Otherwise the paths must be
           compared. */
        rout_type = fundamental_sym1->variant.routine.ptr->type;
check_rout_type:
        if (!routine_type_is_nonstatic_member_function(rout_type)) {
          /* There is only one instance of a static member function. */
          equiv = TRUE;
        }  /* if */
        break;
      default:
        /* Static data member, member constant, or member type. */
        equiv = TRUE;
    }  /* switch */
    if (!equiv) {
      /* The fundamental symbols are the same but may not represent the same
         object (nonstatic data member) or routine (nonstatic member function).
         They will be considered the same only if the belong to the same
         base class subobject.  For example,
                   A{i}
                   |
                   V
                 /   \
                B     C
                 \   /
                   D
         If "i" were declared in A, it is inherited by D along two paths which
         are equivalent insofar as the lead to one and the same base class
         subobject.  However, in this case,
                A{i}  A{i}
                |     |
                B     C
                 \   /
                   D
         A::i is inherited by D along paths that lead to different base class
         subobjects named "A".  Thus the determination of equivalence requires
         determining whether the paths lead to the same of different base
         class subobjects. */
      /* If sym1 or sym2 is a projection symbol, use a path that goes all the
         way to the corresponding fundamental symbol instead of a path to the
         projection. */
      if (sym1->kind == (a_symbol_kind)sk_projection) {
        /* Find the path to the base class to which sym1 belongs. */
        path1 = path_to_fundamental_symbol_base_class(sym1, path1->base_class);
      }  /* if */
      if (sym2->kind == (a_symbol_kind)sk_projection) {
        /* Find the path to the base class to which sym1 belongs. */
        path2 = path_to_fundamental_symbol_base_class(sym2, path2->base_class);
      }  /* if */
      /* Find the end of each path. */
      for (tail1 = path1; tail1->next != NULL; tail1 = tail1->next) {}
      for (tail2 = path2; tail2->next != NULL; tail2 = tail2->next) {}
      if (tail1->base_class->type == tail2->base_class->type) {
        if (tail1->base_class->is_virtual) {
          /* If the ends of the paths refer to the same virtual base class,
             then the members belong to the same subobject. */
          if (tail2->base_class->is_virtual) equiv = TRUE;
        } else if (!tail2->base_class->is_virtual) {
          /* If they refer to the same nonvirtual base class, the paths must
             coincide for it to be the same subobject. */
          if (congruent_paths(path1, path2)) equiv = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return equiv;
}  /* projections_are_equivalent */


static a_boolean check_for_dominance(a_symbol_ptr          sym1,
                                     a_symbol_ptr          sym2,
                                     a_derivation_step_ptr path_to_sym2,
                                     a_type_ptr            class_type)
/*
This routine returns TRUE if sym2 is on a path dominated by sym1.

Dominance is discussed (rather imprecisely) in ARM 10.1.1.  Briefly, if the
declaration of a name in a virtual base class is hidden/overridden by a
redeclaration along one of the paths from the virtual base class, an ambiguity
between the initial declaration and the redeclaration is resolved in favor of
the latter.  Consider, for example,
    class A {public: int i; };
    class B : virtual public A {public: int i; };
    class C : virtual public A {};
    class D : public B, public C {};
which graphically looks like this:
          A{i}
         /   \
        B{i}  C
         \   /
           D
Within the scope of D, where one derivation path for i leads to B::i and the
other leads to A::i, there is in fact no ambiguity, since the declaration of
i in B dominates all other paths from A.  Thus an unqualified reference to i
within the scope of D unambiguously refers to B::i (though of course a
qualified reference either to A::i or to C::i will pick up A::i).
*/
{
  a_boolean              dominated = FALSE;
  a_derivation_step_ptr  step;
  a_base_class_ptr       bcp, next_bcp, dominated_bcp = NULL;

  /* If sym1, the candidate dominating symbol, is a projection symbol, find
     its fundamental symbol. */
  reduce_projection_symbol_to_fundamental_symbol(sym1);
  /* Loop through the base classes of the class of which sym1 is a member. */
  for (bcp = base_classes_of(sym1->class_of_which_a_member);
       bcp != NULL;
       bcp = next_bcp) {
    next_bcp = bcp->next;
    /* We are interested only in virtual base classes. */
    if (bcp->is_virtual) {
      /* Translate the virtual base class into a base class of the common
         derived type. */
      bcp = corresponding_base_class(bcp, class_type, (a_base_class_ptr)NULL);
      if (dominated_bcp == NULL) {
        /* Do the same for the base class associated with the candidate for
           dominated declaration.  dominated_bcp is the base class in which
           sym2 was declared. */
        for (step = path_to_sym2; step->next != NULL; step = step->next);
        dominated_bcp = corresponding_base_class(step->base_class, class_type,
                                                 (a_base_class_ptr)NULL);
        if (sym2->kind == (a_symbol_kind)sk_projection) {
          /* Follow out to the fundamental symbol if sym2 is a projection.
             dominated_bcp will be the base class in which the fundamental
             symbol for sym2 was declared. */
          a_base_class_ptr  temp_bcp = sym2->variant.projection.extra_info->
                                                       fundamental_base_class;
          dominated_bcp = corresponding_base_class(temp_bcp, class_type,
                                                   dominated_bcp);
        }  /* if */
      }  /* if */
      /* If they are the same base class or if bcp is on any possible
         derivation of dominated_bcp, return TRUE.  Here's an example:
                       X
                      /|\
                     A B C
                      \|/
                       Y
                      /|\
                     D E F
                      \|/
                       Z
          A declaration of D::i dominates declarations of X::i, A::i, B::i,
          C::i, and Y::i, because (1) D is on one of the derivations of each
          of X, A, B, C, and Y, and (2) there is another derivation of each.
          The second may be assumed, since the search up the derivation graph
          stops when a name match is found; if the path through D were the
          only path to X, A, etc., X::i, A::i, etc., would not be found. */
      if (dominated_bcp == bcp ||
          is_on_any_derivation_of(dominated_bcp, bcp)) {
        dominated = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return dominated;
}  /* check_for_dominance */       


static a_symbol_ptr find_progenitor_symbol(a_type_ptr            class_ptr,
                                           a_symbol_locator      *locator,
                                           a_boolean             must_be_tag,
                                           a_derivation_step_ptr *path,
                                           an_access_specifier   *access,
                                           a_boolean             *ambiguous)
/*
Given a pointer to a class (or struct or union) type and a locator, find
in the classes from which the current class is derived a symbol that
would serve as progenitor of the name specified in the locator.  Return the
progenitor symbol or NULL is none is found.  Set *ambiguous to TRUE if
there is more than one progenitor.  *path and *access (and *ambiguous as well
under certain circumstances) may be set by subroutines and are just passed
through back to the caller.
*/
{
  a_symbol_ptr                 sym = NULL, other_sym;
  a_derivation_step_ptr        other_path;
  an_access_specifier          other_access;
  a_base_class_ptr             bcp;

  db_enter(4, "find_progenitor_symbol");
  bcp = class_ptr->variant.class_struct_union.extra_info->base_classes;
  /* Loop through the base classes. */
  for (; bcp != NULL; bcp = bcp->next) {
    /* For the most part, we are only interested in the direct base classes
       (either virtual or nonvirtual).  However, a virtual base class may be
       marked as "direct" even though the path of greatest access is that of
       an indirect derivation; such cases are treated as indirect base
       classes. */
    if (preferred_derivation_is_direct(bcp)) {
      if (sym == NULL) {
        /* Look for a projection from this base class (or from any class from
           which it is derived). */
        sym = symbol_projected_from_base_class(bcp, locator, must_be_tag,
                                               path, access, ambiguous);
      } else {
        /* One projection has already been found; look for another. */
        other_path = NULL;
        other_sym = symbol_projected_from_base_class(bcp, locator,
                                                     must_be_tag, &other_path,
                                                     &other_access, ambiguous);
        if (other_sym != NULL) {
          /* A second projection has been found.  Determine whether this is an
             actual ambiguity. */
          if (projections_are_equivalent(sym, *path, other_sym, other_path)) {
            /* No ambiguity (presumably because sym and other_sym are the
               same member of a virtually derived class; choose between the
               two projections based on access. */
            if (is_more_accessible(other_access, *access)) {
              /* Use other_sym in favor of sym. */
              sym = other_sym;
              free_derivation_step(*path);
              *path = other_path;
              *access = other_access;
            } else {
              /* Use sym in favor of other_sym. */
              free_derivation_step(other_path);
            }  /* if */
          } else {
            /* This is an ambiguous reference, unless one of the instances of
               the name dominates the path to the other. */
            if (check_for_dominance(sym, other_sym, other_path, class_ptr)) {
              /* sym dominates other_sym, resolving a potential ambiguity. */
              free_derivation_step(other_path);
            } else if (check_for_dominance(other_sym, sym, *path, class_ptr)) {
              /* other_sym dominates sym. */
              sym = other_sym;
              free_derivation_step(*path);
              *path = other_path;
              *access = other_access;
            } else {
              /* No resolution to the ambiguity, so set *ambiguous TRUE. */
              *ambiguous = TRUE;
              /* if one of the symbols represents a type name, return that
                 symbol.  (This makes a difference in declaration processing,
                 whereas in executable expression processing only the
                 ambiguity is of interest.)  Otherwise just return the first
                 symbol seen. */
              if (is_type_symbol(fundamental_symbol_of(other_sym))) {
                sym = other_sym;
                free_derivation_step(*path);
                *path = other_path;
                *access = other_access;
              } else {
                free_derivation_step(other_path);
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
  return sym;
}  /* find_progenitor_symbol */


a_boolean find_projected_symbol(a_type_ptr        class_ptr,
                                a_symbol_locator  *locator,
                                a_boolean         must_be_tag,
                                a_boolean         must_be_type_name,
                                a_boolean         add_to_active_list,
                                a_symbol_ptr      insert_sym,
                                a_symbol_ptr      *projected_symbol)
/*
Given class_ptr, which identifies a class (or struct or union) type, search
its base classes for a symbol that projects the name specified in *locator
into the class.  If such a symbol is found, create a projection symbol for
it (marked "ambiguous" if there is more than one possible progenitor) and
return it to the caller; otherwise, return NULL.  The new symbol is added
to the symbol table in one of two ways, depending on how add_to_active_list
is set: if the flag is FALSE, the new symbol is added to the beginning of
the locator's inactive list; if it is TRUE, it is inserted in the locator's
active list (which is order dependent) immediately following insert_sym
(or, if insert_sym is NULL, at the beginning of the list), and in addition
it is added to the end of the scope entry symbol list for the class.
*/
{
  a_derivation_step_ptr        path = NULL;
  a_symbol_ptr                 progenitor_sym, new_sym = NULL;
  an_access_specifier          access;
  a_boolean                    ambiguous = FALSE, found;
  a_scope_stack_entry_ptr      ssep;
  a_symbol_ptr                 class_sym;

  db_enter(4, "find_projected_symbol");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for projection of \"%s\" into class \"%s\"\n",
                     locator->symbol_header->identifier,
                     class_ptr->source_corresp.name);
  }  /* if */
#endif /* DEBUG */
  class_sym = (a_symbol_ptr)class_ptr->source_corresp.assoc_info;
  if (locator->symbol_header == class_sym->header &&
      class_sym->
        variant.class_struct_union.extra_info->class_template == NULL) {
    /* A name X cannot be inherited into class X, since the "name slot" for
       is already taken (sort of) by the constructor. */
    progenitor_sym = NULL;
  } else if (locator->is_operator_name &&
             locator->variant.opname == (an_opname_kind)onk_assign) {
    /* Assignment operators are not inherited (13.4.3). */
    progenitor_sym = NULL;
  } else {
    progenitor_sym = find_progenitor_symbol(class_ptr, locator, must_be_tag,
                                            &path, &access, &ambiguous);
  }  /* if */
  if (progenitor_sym == NULL) {
    /* Indicate that no symbol was found and return a NULL pointer. */
    found = FALSE;
  } else {
    /* A symbol was found. */
    found = TRUE;
    if (must_be_type_name &&
        !is_type_symbol(fundamental_symbol_of(progenitor_sym))) {
      /* The symbol found is not a type name symbol, so do not create a
         projection for it. */
    } else {
      /* Create a new symbol based on the symbol returned. */
      new_sym = make_projection_symbol(progenitor_sym, class_ptr,
                                       (a_base_class_ptr)NULL, path,
                                       ambiguous);
      new_sym->variant.projection.access = access;
      free_derivation_step(path);
      /* Add the symbol to the symbol table. */
      if (add_to_active_list) {
        /* Insert the symbol into the active list. */
        if (insert_sym == NULL) {
          /* Insert at head of list. */
          new_sym->next = locator->symbol_header->symbol;
          locator->symbol_header->symbol = new_sym;
        } else {
          /* Insert following insert_sym. */
          new_sym->next = insert_sym->next;
          insert_sym->next = new_sym;
        }  /* if */
        /* Add the symbol to the scope symbol list, so that it will be moved
           to the inactive list when the scope is popped. */
        for (ssep = &scope_stack[depth_scope_stack];
             new_sym->decl_scope != ssep->number;
             ssep--) {
#if CHECKING
          if (ssep == &scope_stack[0]) {
#if DEBUG
            if (debug_level > 0) {
              fprintf(f_debug, "symbol name = %s\n",
                                new_sym->header->identifier);
            }  /* if */
#endif /* DEBUG */
            internal_error("find_projected_symbol: bad scope");
          }  /* if */
#endif /* CHECKING */
        }  /* for */
        if (ssep->symbols != NULL) {
          ssep->last_symbol->next_in_scope = new_sym;
        } else {
          ssep->symbols = new_sym;
        }  /* if */
        ssep->last_symbol = new_sym;
      } else {
        /* Add it to the inactive list.  It can go at the beginning. */
        new_sym->next = locator->symbol_header->inactive_symbols;
        locator->symbol_header->inactive_symbols = new_sym;
      }  /* if */
#if DEBUG
      if (debug_level >= 4) db_symbol(new_sym, "symbol created: ", 2);
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  *projected_symbol = new_sym;

  db_exit();
  return found;
}  /* find_projected_symbol */


static a_symbol_ptr find_nested_type_symbol(a_symbol_locator *locator)
/*
Find a "semivisible" symbol for a member type that is no longer in scope
(nested class, typedef name, or enumeration).  Such symbols are not found
by the normal lookup procedure but are visible according to the "nested
class anachronism" (ARM 18.3.5) which, it turns out, applies in cfront to
typedefs and enums as well.  They are visible as though they had been
entered in the file, function or block scope that is the containing
nonclass scope (i.e., the declaration scope of the parent class).  Look
for a qualifying symbol on the inactive list for the specified symbol
locator.  In the case of an ambiguity, return NULL.
*/
{
  a_symbol_ptr            sym, nested_type_sym = NULL;
  a_type_ptr              tp;
  a_scope_stack_entry_ptr ssep;
  a_scope_number          effective_scope, effective_scope_of_nested_type;

  db_enter(4, "find_nested_type_symbol");
  if (allow_anachronisms &&
      locator->symbol_header->any_nested_types_on_inactive_list) {
    sym = inactive_symbol_list_from_locator(*locator);
    effective_scope_of_nested_type = NO_SCOPE_NUMBER;
    for (; sym != NULL; sym = sym->next) {
      if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
        /* Found an type symbol, but is it nested? */
        tp = sym->class_of_which_a_member;
        if (tp != NULL) {
          /* It is a nested member type.  Pop out to the outermost parent
             class to get the nonclass scope number. */
          while (tp->source_corresp.class_of_which_a_member != NULL) {
            tp = tp->source_corresp.class_of_which_a_member;
          }  /* while */
          /* The effective scope is the innermost file, function, or block
             scope in which parent class is declared. */
          effective_scope =
                    ((a_symbol_ptr)tp->source_corresp.assoc_info)->decl_scope;
          if (effective_scope == effective_scope_of_nested_type) {
            /* We have an ambiguity.  We could issue a warning or error, but
               we choose to recognize the nested class anachronism only when
               it is "legally" used -- we don't want a message that says,
               "You're doing something nonstandard and what's more you aren't
               even doing it correctly."  Especially since the user may not
               have been intending to do any such thing.  However, this may
               introduce some differences with Cfront (2.1), which is wedded
               to the nested class anachronism is surprising ways. */
            nested_type_sym = NULL;
            break;
          }  /* if */
          /* If the effective scope is still active on the scope stack sym is
             a match.  Look through the scope stack for scope number. */
          ssep = &scope_stack[depth_scope_stack];
          for (;;) {
            if (nested_type_sym == NULL) {
              if (ssep->number == effective_scope) {
                /* sym's effective scope is still active, so sym is a match. */
                nested_type_sym = sym;
                effective_scope_of_nested_type = effective_scope;
                /* Break out of the inner loop, but keep looking at symbols
                   in case there's an ambiguity. */
                break;
              } else {
                /* Continue through the scope stack till the scope is found,
                   if it's still active. */
              }  /* if */
            } else {
              /* We already have a symbol but ambiguity has been ruled out.
                 If the current symbol also has an effective scope that's
                 that's still active, keep the symbol whose effective scope
                 is closer to the top of the scope stack. */
              if (ssep->number == effective_scope) {
                /* sym's effective scope is still active and is higher. */
                nested_type_sym = sym;
                effective_scope_of_nested_type = effective_scope;
                break;
              } else if (ssep->number == effective_scope_of_nested_type) {
                /* Other symbol's effective scope is higher.  Break out of
                   the inner loop but keep looking at symbols. */
                break;
              }  /* if */
            }  /* if */
            /* End the loop when we reach the bottom of the scope stack. */
            if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) break;
            ssep--;
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
  return nested_type_sym;
}  /* find_nested_type_symbol */


a_symbol_ptr curr_scope_id_lookup(a_symbol_locator         *locator,
                                  an_id_lookup_options_set options)
/*
Lookup, in the current scope, the identifier indicated by *locator and
return a pointer to the symbol found, or NULL if the symbol is not found.
options contains bits indicating special restrictions, i.e., the symbol
must a tag.  Projection symbols are not considered in the lookup.
*/
{
  a_symbol_ptr    sym;
  a_scope_number  scope_number;
  a_boolean	  must_be_tag = (options & IDL_MUST_BE_TAG);

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym)                                       \
   ((!must_be_tag || is_tag_symbol(sym)) &&				\
    sym->kind != (a_symbol_kind)sk_projection)

#if CHECKING
  if ((options & ~IDL_MUST_BE_TAG) != 0) {
    internal_error("curr_scope_id_lookup: invalid option");
  }  /* if */
#endif /* CHECKING */
  sym = locator->specific_symbol;
  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else if (sym != NULL) {
    check_assertion(is_acceptable_symbol(sym));
    /* The locator is for a specific symbol, so return the symbol for it. */
  } else {
    /* Look for a symbol in the current scope for which the kind matches that
       of the scope level specified by the caller. */
    scope_number = scope_stack[decl_scope_level].number;
    sym = symbol_list_from_locator(*locator);
    for (; sym != NULL; sym = sym->next) {
     if (sym->decl_scope == scope_number && is_acceptable_symbol(sym)) {
        /* Found it. */
        break;
      }  /* if */
    }  /* for */
    locator->specific_symbol = sym;
  }  /* if */
  return sym;
#undef is_acceptable_symbol
}  /* curr_scope_id_lookup */


#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
static int compare_source_positions(a_source_position	*pos1,
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


static a_symbol_ptr check_for_cfront_name_lookup_bug
					(a_type_ptr                class_type,
					 a_symbol_ptr	           sym,
                                         a_symbol_locator          *locator,
					 an_id_lookup_options_set  options)
/*
Cfront 2.1 has a bug that causes a global identifier to be found when
a member of a class or one of its base classes should actually be found.
The following code illustrates an instance in which the bug occurs:

struct B   {
	void func(const char*);	// Needs to be here
};

struct D : public B {
public:
	D();
	void Init(const char* );
};

struct func {
	func( const char* msg);
};

D::D(){}

void D::Init(const char* t)
{
	new func(t);
}

For the bad lookup to occur:

1. A member in a base class must have the same name as an identifier
   at the global scope.  Any member kind is OK -- it can be a function,
   static data member, or nonstatic data member.  Member type names don't
   apply because a nested type will be promoted to the global scope by
   cfront which disallows a later declaration of a type with the same name
   at the global scope.

2. The declaration of the global scope name must occur between the declaration
   of the derived class and the declaration of either an out-of-line
   constructor or destructor.  The global scope name must be a type name.

3. No other member function definition -- even one for an unrelated class
   may appear between the destructor and the offending reference.
   This has the effect that the bad lookup applies to only one class at
   any given point in time.

The global variable last_ctor_or_dtor_sym is set by function_declaration
when the body of a constructor or destructor that is defined outside of
the class definition is processed.  This field is cleared when any other
member function is defined.
*/
{
  a_derivation_step_ptr	path = NULL;
  an_access_specifier   access;
  a_boolean		ambiguous;
  a_symbol_ptr		new_sym = sym;

  /* Before this routine is called we will have already verified that the
     lookup terminated in the class reactivation scope for the same class
     as indicated by last_ctor_or_dtor_sym.  This means that we are
     in a member function definition (or static data member definition) of
     a class for which a constructor or destructor was just defined. */
  if (sym == NULL || sym->kind != (a_symbol_kind)sk_projection) {
    /* If sym is NULL it means that a symbol was found but it was not a
       type when a tentative type lookup was being done.  If the symbol
       is not a projection symbol, then the name was found in the
       derived class.  For the bug to occur, the name must be defined
       in the base class even if the name was redefined in the derived
       class.  Look for the name in a base class. */
    new_sym = find_progenitor_symbol(class_type, locator,
                                     /*must_be_tag=*/FALSE, &path, &access,
                                     &ambiguous);
  }  /* if */
  if (new_sym != NULL) {
    a_symbol_ptr  fund_sym = fundamental_symbol_of(new_sym);
    if (!(is_type_symbol(fund_sym) &&
          type_symbol_type(fund_sym)->
                       use_cfront_transitional_nested_type_name_mangling)) {
      /* Names that are in essence promoted to file scope are not
         considered. */
      a_symbol_ptr file_scope_sym;
      /* new_sym must now be a projection symbol or progenitor symbol.  Look
         for a symbol with the same name at file scope. */
      check_assertion(class_type != fund_sym->class_of_which_a_member);
      file_scope_sym = file_scope_id_lookup(locator, options);
      if (file_scope_sym != NULL && is_type_symbol(file_scope_sym)) {
        /* A file scope symbol was found.  For the incorrect lookup to be
           done the file scope symbol must have been declared after the
           derived class but before the most recent constructor or
           destructor body. */
        a_symbol_ptr	class_sym;
        class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
        if ((compare_source_positions(&file_scope_sym->decl_position,
                                      &class_sym->decl_position) > 0) &&
            (compare_source_positions(&file_scope_sym->decl_position,
                                      &last_ctor_or_dtor_sym->
                                                       decl_position) < 0)) {
          sym = file_scope_sym;
          pos_sy2_warning(ec_cfront_name_lookup_bug,
                          &locator->source_position,
                          sym, new_sym);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return (sym);
}  /* check_for_cfront_name_lookup_bug */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */


static void create_proxy_class(a_type_ptr   templ_param_type)
/*
Creates the proxy class pointed to by a template parameter type description
record.  This consists of allocating and initializing the class and assigning
a scope number.  The class type is created the first time that a template
parameter is used in a context in which a class qualified name lookup
needs to be done using the template parameter as the class type.
*/
{
  a_type_ptr				type;
  a_template_param_type_descr_ptr	tptdp;
  a_symbol_ptr				sym;
  a_symbol_ptr				templ_param_sym;
  a_class_symbol_supplement_ptr		cssp;
  tptdp = templ_param_type->variant.template_param.descr;
  if (tptdp == NULL) {
    /* Allocate a template parameter type description entry. */
    tptdp = alloc_template_param_type_descr();
    templ_param_type->variant.template_param.descr = tptdp;
  }  /* if */
  /* Get the symbol pointer associated with the template parameter. */
  templ_param_sym = (a_symbol_ptr)templ_param_type->source_corresp.assoc_info;
  /* Create a symbol for the class.  The symbol will have the same name
     as the template parameter symbol.  mark_declared is not called
     because this symbol is not visible to the user. */
  sym = alloc_symbol((a_symbol_kind)sk_class_or_struct_tag,
                     templ_param_sym->header, &templ_param_sym->decl_position);
  /* The class will be considered to be at file scope.  If this is changed
     to be some other scope then set_source_corresp_with_scope_depth may
     need to be called because set_source_corresp requires that the
     decl_scope of the symbol still be an active scope. */
  sym->decl_scope = FILE_SCOPE_NUMBER;
  /* Create the type for the class. */
  type = alloc_type((a_type_kind)tk_class);
  set_source_corresp(&(type->source_corresp), sym);
  sym->variant.class_struct_union.type = type;
  type->source_corresp.class_of_which_a_member =
                  sym->class_of_which_a_member = 
                     templ_param_type->source_corresp.class_of_which_a_member;
  tptdp->class_type = type;
  /* Set the scope number. */
  cssp = symbol_supplement_for_class(type);
  cssp->member_decl_scope = next_scope_number++;
  cssp->template_param_for_proxy_class = templ_param_type;
}  /* create_proxy_class */


static a_symbol_ptr add_member_to_proxy_or_nonreal_class
					(a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator)
/*
This routine is called by class_qualified_id_lookup when the name
being looked up is not found in the proxy class associated with a
template parameter type or in a class that is a nonreal instantiation.
We don't know anything about the name that is being looked up except
whether or not it is a type (based on the is_type parameter).  If
is_type is TRUE we create a member of class_type that is a
tk_template_param.  If is_type is FALSE we create a member of
class_type that is a ck_template_param.
*/
{
  a_symbol_kind			kind;
  a_class_symbol_supplement_ptr	cssp;
  a_scope_depth			depth = NO_SCOPE_DEPTH;
  a_symbol_ptr			sym;
  a_boolean			is_type;

  db_enter(4, "add_member_to_proxy_or_nonreal_class");
  /* If we are doing a  "must be class", "must be tag" or "tentative type"
     lookup then we create the symbol as a type; otherwise we create it
     as a constant. */
  is_type = options & IDL_MUST_BE_CLASS || options & IDL_MUST_BE_TAG ||
            options & IDL_DO_NOT_MAKE_PROJECTION_IF_NOT_TYPE_NAME;
  /* Create a symbol for the member.  mark_declared is not called
     because this symbol is not visible to the user. */
  kind = (a_symbol_kind)(is_type ? sk_type : sk_constant);
  sym = alloc_symbol(kind, locator->symbol_header, &locator->source_position);
  /* Get the scope number from the symbol supplement.  The scope depth
     will be the scope depth of the class plus one. */
  cssp = symbol_supplement_for_class(class_type);
  sym->decl_scope = cssp->member_decl_scope;
#if RECORD_SCOPE_DEPTH_IN_IL
  depth = class_type->source_corresp.scope_depth;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Create the type or constant. */
  if (is_type) {
    a_type_ptr	type = alloc_type((a_type_kind)tk_template_param);
    type->variant.template_param.kind =
                                    (a_template_param_type_kind)tptk_member;
    sym->variant.type = type;
    set_source_corresp_with_scope_depth(&type->source_corresp, sym, depth);
    type->source_corresp.class_of_which_a_member = class_type;
  } else {
    /* Create a ck_template_param constant.  We don't know the type of the
       constant so we allocate a tk_template_param to use as the type. */
    a_constant_ptr  constant;
    constant = fs_constant((a_constant_repr_kind)ck_template_param);
    constant->variant.template_param.kind =
                                  (a_template_param_constant_kind)tpck_member;
    sym->variant.constant = constant;
    constant->type = alloc_type((a_type_kind)tk_template_param);
    constant->type->variant.template_param.kind = 
                   (a_template_param_type_kind)tptk_type_of_member_constant;
    set_source_corresp_with_scope_depth(&constant->source_corresp, sym, depth);
    constant->source_corresp.class_of_which_a_member = class_type;
  }  /* if */
  /* Add the symbol to the inactive list. */
  sym->next = sym->header->inactive_symbols;
  sym->header->inactive_symbols = sym;
  sym->class_of_which_a_member = class_type;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Adding: ");
    db_symbol(sym, "", 0);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* add_member_to_proxy_or_nonreal_class */


a_symbol_ptr normal_id_lookup(a_symbol_locator         *locator,
                              an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator and return a pointer to
the symbol found, or NULL if the symbol is not found.  options contains
bits indicating special restrictions, i.e., the symbol must be a class
name or tag.  The symbol is looked up in the normal name space (variables,
functions, classes, types, etc.).  If the symbol found is a projection
symbol, the projection symbol pointer is recorded in the locator and the
fundamental symbol pointer is returned.  This routine is used in both
C and C++.
*/
{
  a_symbol_ptr            sym, inactive_sym, inactive_symbol_list;
  a_symbol_ptr            active_symbol_list, active_sym, prev_active_sym;
  a_symbol_ptr            insert_sym, tag_symbol;
  a_scope_stack_entry_ptr ssep;
  a_boolean               must_be_class = (options & IDL_MUST_BE_CLASS);
  a_boolean               must_be_tag   = (options & IDL_MUST_BE_TAG);
  a_boolean		  skip_curr_function_scope =
                                      (options & IDL_SKIP_CURR_FUNCTION_SCOPE);
  a_boolean		  first_scope;
  a_boolean               must_be_type_name;
  a_boolean               look_for_projected_symbol = FALSE;
  a_boolean               add_to_active_list;
  a_name_space_kind       required_name_space_kind =
                            (C_dialect != C_dialect_cplusplus && must_be_tag) ?
                                                           nsk_tag : nsk_other;
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  a_boolean		  projection_symbol_found = FALSE;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  a_boolean		  skip_first_class_reactivation_scope = FALSE;
  a_boolean		  any_nonreal_bases = FALSE;
  a_type_ptr		  class_with_nonreal_base;

/* Local macro that tests whether or not a symbol is acceptable. */
/* is_class_or_class_proxy_symbol checks for a symbol that is a class,
   class template, or template type parameter. */
#define is_acceptable_symbol(sym)                                       \
  ((!must_be_class ||							\
    is_class_or_class_proxy_symbol(fundamental_symbol_of(sym))) &&  \
   (!must_be_tag   ||						   \
    is_tag_or_tag_proxy_symbol(fundamental_symbol_of(sym))))
/* Local macro that tests whether or not a symbol on the active list
   is acceptable.  See if the symbol is in the proper name space. */
#define is_acceptable_active_symbol(sym)                              \
  (name_space_for_symbol_kind[(int)sym->kind] == required_name_space_kind && \
   is_acceptable_symbol(sym))

/* Local macro that skips over any symbols on the active list that are
   associated with the current scope. */
#define skip_symbols_from_this_scope()					\
  {									\
    while (active_sym != NULL &&					\
           active_sym->decl_scope == ssep->number) {			\
      active_sym = active_sym->next;					\
    }  /* while */							\
  }

  db_enter(4, "normal_id_lookup");

  sym = locator->specific_symbol;
  if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
  } else if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else {
    /* We must search for the symbol. */
    /* We have two search algorithms: the first is the C algorithm, which
       is fast; it just searches the active list.  The second is the C++
       algorithm, which is slower; it considers each scope on the scope
       stack in turn, and looks for a symbol in that scope. */
    active_symbol_list = symbol_list_from_locator(*locator);
    inactive_symbol_list = inactive_symbol_list_from_locator(*locator);
    /* If there are no classes, no class reactivations, and no instantiations
       on the scope stack, or if the symbol cannot be a member symbol, the
       fast algorithm can be used.  This is always the case in C. The
       more complicated (and slower) algorithm must be used to look up
       the name in the symbol (1) if there are symbols on the inactive
       list for the name in question -- i.e., symbols that will not be
       found with the fast algorithm; (2) if the present scope stack
       is such that currently visible symbols might be on an inactive
       list; and (3) if there are scopes on the scope stack for which
       active symbols must be excluded from the lookup (such as pragma
       and template instantiation scopes).  Only symbols for members
       of classes that go out of scope appear on the inactive list.
       Such symbols become visible in only two ways -- they belong to
       a base class of a class that is currently in scope or they
       belong to a class that has been reactivated (e.g., for the
       definition of a member or friend function or the initialization
       of a static data member).  Note that the slow algorithm is not
       required for member symbols on the active list because they are
       found properly on the search of the active list in the fast
       algorithm.  We don't need to check skip_curr_function_scope
       when deciding whether to use the fast or slow algorithm because
       there will always be a class reactivation scope on the stack
       which will force the slow lookup.  */
    ssep = &scope_stack[depth_scope_stack];
#if CHECKING
    /* IDL_SKIP_CURR_FUNCTION_SCOPE must only be used when the top scope
       entry is for a function. */
    if (skip_curr_function_scope) {
      if (ssep->kind != (a_scope_kind)sck_function) {
        internal_error("normal_id_lookup: skip_curr_function_scope error");
      }  /* if */
    }  /* if */
#endif /* CHECKING */
    if (C_dialect != C_dialect_cplusplus ||
        ((inactive_symbol_list == NULL ||
          !ssep->inactive_symbols_may_be_visible) &&
         !ssep->slow_lookup_required)) {
      /* Fast algorithm: just search the active symbol list. */
#if DEBUG
      num_fast_id_lookups++;
#endif /* DEBUG */
      for (sym = active_symbol_list; sym != NULL; sym = sym->next) {
        /* See if the symbol is acceptable (e.g., it's a class if it
           must be one). */
        if (is_acceptable_active_symbol(sym)) break;
      }  /* for */
    } else {
      /* There are inactive symbols and they may be visible, so the more
         complicated search is required. */
      a_boolean	check_for_nonreal_bases;
#if DEBUG
      num_slow_id_lookups++;
#endif /* DEBUG */
      /* Work out from the innermost scope on the stack, and look at each
         scope.  If the scope is a class reactivation or a template
         instantiation, look on the inactive list for a symbol from that
         scope.  Otherwise, search part of the active list to look for an
         active symbol.  We maintain a pointer to the point in the active
         list up to which we've searched.  This works because the symbols
         on the active list are in order according to the scope they're in,
         from innermost scope to outermost. */
      prev_active_sym = NULL;
      active_sym = active_symbol_list;
      /* If any instantiation scopes are active we will need to check for
         the presence of nonreal base classes. */
      check_for_nonreal_bases =
                       depth_innermost_instantiation_scope != NO_SCOPE_DEPTH;
      /* Since there is a class or class reactivation on the stack, we know the
         stack has at least two entries (the file scope and the class or
         class reactivation). */
      for (first_scope = TRUE;; first_scope = FALSE) {
        if (ssep->kind == (a_scope_kind)sck_class_reactivation ||
	    ssep->kind == (a_scope_kind)sck_template_instantiation) {
          if (cfront_2_1_mode &&
              ssep->kind == (a_scope_kind)sck_class_reactivation &&
              skip_first_class_reactivation_scope) {
            /* This is used to skip class reactivation scopes when
               processing friend declarations in cfront compatibility
               mode.  Cfront ignores the innermost class reactivation
               scope when processing friend functions. */
            skip_first_class_reactivation_scope = FALSE;
            goto next_scope;
          }  /* if */
          /* Look on the inactive list for a symbol from this reactivated
             scope. */
          tag_symbol = NULL;
          for (inactive_sym = inactive_symbol_list;
               inactive_sym != NULL;
               inactive_sym = inactive_sym->next) {
            if (inactive_sym->decl_scope == ssep->number) {
              if (is_acceptable_symbol(inactive_sym)) {
                /* Found a symbol. */
                /* If this is a template parameter symbol that should not
		   be visible then continue looking for another symbol. */
	        if (inactive_sym->template_param_not_visible) continue;
                /* If the symbol is a tag symbol and we're not required to find
                   a tag symbol, there's the possibility that there is a
                   non-type symbol in the same scope later in the list (because
                   the inactive list is not ordered in any way).  Save the
                   tag symbol and keep looking.  If nothing else turns up,
                   use the tag symbol. */
                if (is_tag_symbol(inactive_sym) && !must_be_tag) {
                  tag_symbol = inactive_sym;
                } else {
                  /* Take the symbol. */
                  sym = inactive_sym;
                  goto end_lookup;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* for */
          /* We reached the end of the list.  If there is a tag symbol saved
             within the loop, use it. */
          if (tag_symbol != NULL) {
            sym = tag_symbol;
            goto end_lookup;
          }  /* if */
	  if (ssep->kind == (a_scope_kind)sck_class_reactivation) {
            /* There is no inactive symbol that is in this class. */
            /* Look for a symbol projected (inherited) into this class. */
            look_for_projected_symbol = TRUE;
            add_to_active_list = FALSE;
            insert_sym = NULL;
          } else {
            /* Not a class scope, so do not look for projected symbol. */
            look_for_projected_symbol = FALSE;
          }  /* if */
	} else if (ssep->kind == (a_scope_kind)sck_pragma) {
	  /* We have found a pragma scope -- ignore symbols in this scope. */
          skip_symbols_from_this_scope();
	  goto next_scope;
        } else {
          /* Not a class reactivation or a template instantiation,
             i.e., normal scope.  Search through any symbols on the front
             of the active list that are from the associated scope, and see
             if any one is the symbol desired. */
          for (;active_sym != NULL && active_sym->decl_scope == ssep->number;
               prev_active_sym = active_sym, active_sym = active_sym->next) {
            if (first_scope && skip_curr_function_scope) {
              /* IDL_SKIP_CURR_FUNCTION_SCOPE is being used.  Don't accept
                 symbols from the first scope entry.  This is used when
		 looking up names from the initializer list of a
		 constructor declaration.  The constructor parameters
		 must not be visible during this lookup. */
            } else if (is_acceptable_active_symbol(active_sym)) {
              /* Found a symbol. */
              sym = active_sym;
              goto end_lookup;
            }  /* if */
          }  /* for */
          if (ssep->kind == (a_scope_kind)sck_class_struct_union) {
            /* For class scopes, look for a symbol projected (inherited)
               into the class scope. */
            look_for_projected_symbol = TRUE;
            add_to_active_list = TRUE;
            insert_sym = prev_active_sym;
          } else {
            /* Not a class scope, so do not look for projected symbol. */
            look_for_projected_symbol = FALSE;
          }  /* if */
        }  /* if */
        /* For class and class reactivation scopes, when the symbol is not
           found, see if there is a projection of some symbol into the
           scope. */
        if (look_for_projected_symbol) {
          must_be_type_name = 
                    (options & IDL_DO_NOT_MAKE_PROJECTION_IF_NOT_TYPE_NAME);
          if (find_projected_symbol(ssep->assoc_type, locator, must_be_tag,
                                    must_be_type_name, add_to_active_list,
                                    insert_sym, &sym)) {
            if (sym == NULL) {
              /* A symbol was found in a base class, but it was not returned
                 (presumably because must_be_type_name was not satisfied).
                 Don't continue looking. */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
	      projection_symbol_found = TRUE;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
              goto end_lookup;
            } else {
              /* A projection symbol was created.  It must still satisfy the
                 constraints for this lookup. */
              if (is_acceptable_symbol(sym)) goto end_lookup;
              sym = NULL;
            }  /* if */
          } else {
            if (check_for_nonreal_bases && !any_nonreal_bases) {
              /* No projection symbol was found.  If the class has any
                 nonreal base classes record this information for possible
                 later use. */
              class_with_nonreal_base = ssep->assoc_type;
              any_nonreal_bases = symbol_supplement_for_class(ssep->assoc_type)
                                                    ->any_nonreal_base_classes;
            }  /* if */
          }  /* if */
        }  /* if */
next_scope:
        /* End the loop when we reach the bottom of the scope stack. */
        if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) break;
        if (cfront_2_1_mode && ssep->kind == (a_scope_kind)sck_function) {
          /* In cfront compatibility mode friend functions defined within
             a class ignore the innermost class reactivation scope.
             If this is a friend function, set a flag that will cause
             the innermost class reactivation scope to be ignored.  This
             is a cfront 2.1 problem that appears to have been fixed in
             cfront 3.0. */
          if (ssep->assoc_routine->source_corresp.class_of_which_a_member ==
                                                                       NULL) {
            skip_first_class_reactivation_scope = TRUE;
          }  /* if */
        }  /* if */
        /* If this scope is for a template instantiation or a pragma, ignore
           the scopes between the file scope and the current instantiation
           scope.  The only processing done on these scopes is to remove their
           symbols from the active list so that when we reach the file
           scope we are at the right point in the active list.  Within
           an instantiation scope only file scope symbols, template
           parameters, and symbols defined within the instantiation
           should be visible.  If this is a nested instantiation (i.e.,
           an instantiation of something defined within another template)
	   then continue looking for names until the first nonnested
	   instantiation is encountered. */
        if (ssep->kind == (a_scope_kind)sck_template_instantiation &&
	    !ssep->nested_instantiation) {
          a_scope_number  file_scope_number;
          ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
          file_scope_number = ssep->number;
          /* Scan the active list until we find a file scope symbol.  We don't
             need to worry about prev_active_sym because it is not used
             for the file scope and there can be no scopes beyond the file
             scope. */
          while (active_sym != NULL &&
                 active_sym->decl_scope != file_scope_number) {
            active_sym = active_sym->next;
          }  /* while */
        } else {
          ssep--;
        }  /* if */
      }  /* for */
    }  /* if */
    if (sym == NULL) {
      /* See if the nested class anachronism (ARM 18.3.5) yields a symbol.
         Note that if there is an ambiguity, NULL is returned.  Note also
         that we look for a semivisible nested class only if no other symbol
         is found.  This means a nested class that is semivisible at function
         scope will not hide a name at file scope; this is different from how
         cfront 2.1 works, but it means that programs that are legal by the
         ARM do not fail to compile or otherwise behave differently because
         the anachronism was invoked. */
      if (allow_anachronisms) sym = find_nested_type_symbol(locator);
      if (sym != NULL) {
        if (is_acceptable_symbol(sym)) {
          locator->is_semivisible_nested_type = TRUE;
        } else {
          sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
    if (sym == NULL && any_nonreal_bases) {
      /* If no symbol was found and one of the classes searched has
         a nonreal base class then consider the symbol to be a member
         of the class with the nonreal base class.  This will occur when
         a base class depends on a template parameter (such as A<T>)
         or when the base class is a template parameter (such as T).
         In these cases it is impossible to know, at the time that
         prototype instantiation is done, which names will be in the
         classes used in the real instantiations.  Any name is accepted
         as a member of the class. */
      sym = add_member_to_proxy_or_nonreal_class(class_with_nonreal_base,
						 options, locator);
    }  /* if */
end_lookup:
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    if (cfront_2_1_mode &&
        (sym != NULL || projection_symbol_found)) {
      /* Special case to emulate a cfront 2.1 bug.  See the comments
         in check_for_cfront_name_lookup_bug for more information.  The
         special case code is only executed when the symbol found is from
	 a class reactivation scope for the same class of a constructor
	 or destructor that was just defined -- so this should not have
	 a significant performance impact.

         We have either found a symbol (sym != NULL) or a projection symbol
         was found that was not a type symbol in a tentative type lookup
         (projection_symbol_found == TRUE).  Call the special routine to
         see if a file scope name exists that satisfies the required
         criteria. */
      if (ssep->kind == (a_scope_kind)sck_class_reactivation) {
        a_type_ptr      class_type = ssep->assoc_type;
	if (last_ctor_or_dtor_sym != NULL &&
            class_type == last_ctor_or_dtor_sym->class_of_which_a_member) {
	  sym = check_for_cfront_name_lookup_bug(class_type, sym, locator,
						 options);
          /* This looks like we can find an alternate symbol when emulating
	     the cfront bug and then discard it because it is not the
	     correct kind of symbol.  In practice this will never happen
	     because the only symbols that can be rejected are types
	     (either classes or tags) and because of the transitional model
	     of nested type handling, will always be defined before a nested
	     class of the same name can be used. */
          if (sym != NULL && !is_acceptable_symbol(sym)) {
	    sym = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
#if DEBUG
  if (debug_level >= 4) {
    if (sym != NULL) {
      fprintf(f_debug, "normal_id_lookup: found %s\n",
                       sym->header->identifier);
    } else {
      fprintf(f_debug, "normal_id_lookup: not found\n");
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
#undef is_acceptable_symbol
#undef is_acceptable_active_symbol
}  /* normal_id_lookup */


a_symbol_ptr curr_tag_symbol(a_symbol_locator  *locator,
                             a_symbol_kind     tag_kind)
/*
The current token is an identifier.  If it is a tag of the indicated kind
do ambiguity and access control checking and return a pointer to the tag
symbol.  Otherwise, return NULL.
*/
{
  a_symbol_ptr assoc_symbol;

  /* Look up the current token.  Note that a qualified name is not allowed. */ 
  assoc_symbol = normal_id_lookup(locator, IDL_MUST_BE_TAG);
  if (assoc_symbol != NULL &&
      assoc_symbol->kind == (a_symbol_kind)sk_class_template &&
      depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    /* If the symbol found is a class template symbol and we are inside an
       instantiation of the class, use the template class symbol associated
       with the current instantiation. */
    (void)current_class_symbol_if_class_template(&assoc_symbol);
  }  /* if */
  if (assoc_symbol != NULL) {
    if (assoc_symbol->is_template_param) {
      a_type_ptr   	tp;
      a_symbol_ptr	new_sym;
      /* We are within a template instantiation, so the name may map to a
         template parameter.  For example,
            class A { };
            template <class T> class B { class T x; };
            B<A> b;
         The symbol is for a type template parameter (nontypes will not be
         found by an IDL_MUST_BE_TAG lookup).  During prototype instantiation
         the template parameter symbol is simply returned to the caller.
         During a real instantiation the symbol associated with the type
         pointed to by the template parameter is returned. */
      check_assertion_str(assoc_symbol->kind == (a_symbol_kind)sk_type,
                          "curr_tag_symbol: bad symbol kind");
      tp = assoc_symbol->variant.type;
      new_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
      if (tp->kind == (a_type_kind)tk_template_param) {
        /* A template parameter encountered during prototype instantiation.
           Simply return the original symbol. */
      } else if (new_sym != NULL && new_sym->kind == tag_kind) {
        /* Use the template argument to which the template parameter points. */
        assoc_symbol = new_sym;
      } else {
        /* The template argument is the wrong kind of tag. */
        pos_stty_error(ec_tag_kind_incompatible_with_template_parameter,
                       &error_position,
                       name_of_symbol_kind(tag_kind), tp);
        set_to_error_locator(*locator);
        assoc_symbol = NULL;
      }  /* if */
    }  /* if */
    if (assoc_symbol == NULL) {
      /* A NULL symbol resulted from an error above. */
    } else if (assoc_symbol->kind == (a_symbol_kind)sk_type) {
      /* This must be a symbol for a template parameter, and we must be in
         the midst of a prototype instantiation.  Return the symbol that
         was found. */
    } else if (assoc_symbol->kind != tag_kind) {
      /* A tag, but the wrong kind of tag (e.g., struct when union is
         required). */
      assoc_symbol = NULL;
    } else {
      if (locator->is_semivisible_nested_type) {
        /* The symbol in the locator is a nested class that is not visible
           according to the ARM lookup rules but is returned in support of the
           nested class anachronism (ARM 18.3.5).  Issue an anachronism
           diagnostic. */
        sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                       locator->specific_symbol);
      }  /* if */
      /* Do ambiguity and access control checking on the member. */
      check_ambiguity_and_verify_access(locator);
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_tag_symbol */


static void determine_projected_symbol_insert_location(
                                          a_symbol_locator *locator,
                                          a_type_ptr       class_type,
                                          a_boolean        *add_to_active_list,
                                          a_symbol_ptr     *insert_sym)
/*
Determine the insert location (add_to_active_list and insert_sym) required
by find_projected_symbol to insert a projection symbol for the locator
*locator into the class indicated by class_type.
*/
{
  a_symbol_ptr            prev_active_sym, active_sym;
  a_scope_stack_entry_ptr ssep;

  /* Find out whether or not the class is active, and if so, where in
     the active list its entries begin. */
  *add_to_active_list = FALSE;
  *insert_sym = NULL;
  prev_active_sym = NULL;
  active_sym = symbol_list_from_locator(*locator);
  for (ssep = &scope_stack[depth_scope_stack];
       ssep != &scope_stack[DEPTH_OF_FILE_SCOPE];
       ssep--) {
    /* If we've found the class, exit the loop. */
    if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
        ssep->assoc_type == class_type) {
      *add_to_active_list = TRUE;
      *insert_sym = prev_active_sym;
      break;
    }  /* if */
    /* If the stack entry may have associated entries on the active list,
       move past them. */
    if (ssep->kind != (a_scope_kind)sck_class_reactivation) {
      for (;active_sym != NULL && active_sym->decl_scope == ssep->number;
           prev_active_sym = active_sym, active_sym = active_sym->next) {
      }  /* for */
    }  /* if */
  }  /* for */
}  /* determine_projected_symbol_insert_location */


a_symbol_ptr class_qualified_id_lookup(a_symbol_locator         *locator,
                                       a_type_ptr               class_type,
                                       an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the class indicated by
class_type, and return a pointer to the symbol found, or NULL if
the symbol is not found.  options indicates a set of special options,
as a bit set.  For example, if IDL_MUST_BE_CLASS is TRUE, the symbol found
must be a class name.  If the symbol found is a projection symbol, the
projection symbol pointer is recorded in the locator and the fundamental
symbol pointer is returned.  This routine is used in both C and C++ mode.
*/
{
  a_symbol_ptr sym, tag_symbol, class_symbol;
  a_boolean    must_be_class = (options & IDL_MUST_BE_CLASS);
  a_boolean    must_be_tag = (options & IDL_MUST_BE_TAG);
  a_class_symbol_supplement_ptr
               cssp;
  a_symbol_ptr insert_sym;
  a_boolean    add_to_active_list;
  a_boolean    is_proxy_or_nonreal_class_lookup = FALSE;

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym)                                     \
  ((sym)->class_of_which_a_member == class_type &&                    \
   (!must_be_class || is_class_or_class_proxy_symbol(sym)) &&	      \
   (!must_be_tag || is_tag_or_tag_proxy_symbol(sym)))

  db_enter(4, "class_qualified_id_lookup");
  /* Remove any typedef on the class type. */
  class_type = skip_typerefs(class_type);
  if (class_type->kind == (a_type_kind)tk_template_param) {
    /* We are looking up a name in a template parameter that is being used
       as a class (e.g., T::X, where T is a template parameter).  Each
       template parameter that is used as a class has a "proxy class"
       created for it that contains a list of member names that have
       been looked up in the class.  Any name that is looked up in the
       proxy class will be found -- if it doesn't already exist, a symbol
       entry will be created for it. */
    a_template_param_type_descr_ptr	tptdp;
    tptdp = class_type->variant.template_param.descr;
    if (tptdp == NULL || tptdp->class_type == NULL) {
      create_proxy_class(class_type);
      tptdp = class_type->variant.template_param.descr;
    }  /* if */
    /* Use the proxy class in place of the template parameter type. */
    class_type = tptdp->class_type;
    is_proxy_or_nonreal_class_lookup = TRUE;
  } else {
    /* Determine whether we are looking up a name in a nonreal class
       that is not the prototype instantiation.  Nonreal lookups are
       handled like proxy class lookups; any name looked up is found.
       If the symbol does not exist one will be created.  The
       assoc_scope check is used to exclude the prototype
       instantiation from being considered nonreal for lookup
       purposes. */
    cssp = symbol_supplement_for_class(class_type);
    if (cssp->is_nonreal_class && class_type->variant.class_struct_union.
                                             extra_info->assoc_scope == NULL) {
      is_proxy_or_nonreal_class_lookup = TRUE;
    }  /* if */
  }  /* if */
  if ((sym = locator->specific_symbol) != NULL) {
    /* There is an existing specific symbol. */
  } else {
    /* Search for a symbol in the right scope. */
    /* First, search the list of inactive symbols.  These are class
       members for classes that are no longer active.  Or, in C,
       fields of structs/unions. */
    tag_symbol = NULL;
    for (sym = inactive_symbol_list_from_locator(*locator);
         sym != NULL;
         sym = sym->next) {
      if (is_acceptable_symbol(sym)) {
        /* Found an acceptable symbol. */
        /* If the symbol is a tag symbol, there's the possibility that
           there is a non-type symbol in the same scope later in the list
           (because the inactive list is not ordered in any way).  Save the
           tag symbol and keep looking.  If nothing else turns up,
           use the tag symbol. */
        if (!is_tag_symbol(sym)) goto end_lookup;
        tag_symbol = sym;
      }  /* if */
    }  /* for */
    /* We reached the end of the list.  If there is a tag symbol saved
       within the loop, use it. */
    if (tag_symbol != NULL) {
      sym = tag_symbol;
      goto end_lookup;
    }  /* if */
    if (is_proxy_or_nonreal_class_lookup &&
        !(options & IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) {
      /* When looking up a name in a proxy or nonreal class, the name is
         always found.  If we did not find the name in the search
         above then we must create a symbol now. */
      sym = add_member_to_proxy_or_nonreal_class(class_type, options, locator);
      goto end_lookup;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      /* The name was not found on the inactive symbols list.  Try the
         active symbols list.  This would come up when a qualified name
         is used when the qualification is not really necessary, i.e.,
         when we're inside the class mentioned in the qualifier. */
      for (sym = symbol_list_from_locator(*locator);
           sym != NULL;
           sym = sym->next) {
        if (is_acceptable_symbol(sym)) {
          /* Found an acceptable symbol. */
          goto end_lookup;
        }  /* if */
      }  /* for */
      /* Look to see if the name is the name of a constructor or destructor
         for the class.  The symbols for those are not entered in the
         normal symbol table; they're pointed to from the class symbol
         supplement. */
      class_symbol = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
      cssp = class_symbol->variant.class_struct_union.extra_info;
      if (locator->symbol_header == class_symbol->header) {
        /* Looking up the class name within itself.  Return the constructor if
           there is one. */
        sym = cssp->constructor;
        if (sym != NULL) {
          /* There is a constructor.  Change the locator symbol header to
             the header for the constructor rather than the header for the
             class.  They have the same name, but different headers. */
          locator->symbol_header = sym->header;
          goto end_lookup;
        }  /* if */
      } else if (cssp->destructor != NULL &&
                 locator->symbol_header == cssp->destructor->header) {
        /* This is the destructor. */
        sym = cssp->destructor;
        goto end_lookup;
      }  /* if */
      /* The name was not found.  Try looking for a member symbol that can
         be projected into the class. */
      determine_projected_symbol_insert_location(locator,
                                                 class_type,
                                                 &add_to_active_list,
                                                 &insert_sym);
      (void)find_projected_symbol(class_type, locator, /*must_be_tag=*/FALSE,
                                  /*must_be_type_name=*/FALSE,
                                  add_to_active_list, insert_sym, &sym);
    }  /* if */
end_lookup:
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "class_qualified_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
#undef is_acceptable_symbol
}  /* class_qualified_id_lookup */


a_symbol_ptr file_scope_id_lookup(a_symbol_locator         *locator,
                                  an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the file scope, and
return a pointer to the symbol found, or NULL if the symbol is not found.
options indicates a set of special options, as a bit set.  For example,
if IDL_MUST_BE_CLASS is TRUE, the symbol found must be a class name.
Only symbols in the nsk_other name space are considered.  This routine is
used for the unary "::" qualifier and may only be used in C++ mode.
*/
{
  a_symbol_ptr   sym;
  a_boolean      must_be_class = (options & IDL_MUST_BE_CLASS);
  a_boolean      must_be_tag = (options & IDL_MUST_BE_TAG);

/* Local macro that tests whether or not a symbol is acceptable. */
/* The name space test is needed when searching the file scope, so
   sk_extern_variable and sk_extern_routine are not found.
   is_class_or_class_proxy_symbol checks for a symbol that is a class,
   class template, or template type parameter. */
#define is_acceptable_symbol(sym)                                     \
  ((sym)->decl_scope == FILE_SCOPE_NUMBER &&                          \
   name_space_for_symbol_kind[sym->kind] == nsk_other &&              \
   (!must_be_class || is_class_or_class_proxy_symbol(sym)) && 	      \
   (!must_be_tag || is_tag_symbol(sym)))

  db_enter(4, "file_scope_id_lookup");
  if ((sym = locator->specific_symbol) != NULL) {
    /* There is an existing specific symbol. */
  } else {
    /* Search for a symbol in the file scope. */
    for (sym = symbol_list_from_locator(*locator);
         sym != NULL;
         sym = sym->next) {
      if (is_acceptable_symbol(sym)) break;
    }  /* for */
    locator->specific_symbol = sym;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "file_scope_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
#undef is_acceptable_symbol
}  /* file_scope_id_lookup */


a_symbol_ptr opname_member_function_symbol(an_opname_kind kind,
                                           a_type_ptr     class_type)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind in class class_type, or NULL if there is no such
operator.  The symbol may be a projection symbol (that's desirable, because
a projection symbol is needed to check for ambiguity and access).
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;
  a_symbol_locator    locator;

  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Yes.  Look for one in the desired class. */
    clear_locator(&locator, &pos_curr_token);
    locator.symbol_header = symhdr;
    locator.is_operator_name = TRUE;
    locator.variant.opname = kind;
    if (class_qualified_id_lookup(&locator, class_type,
                                  (IDL_NO_OPTIONS |
                                   IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) != NULL) {
      /* Get the projection symbol if any. */
      sym = locator.specific_symbol;
    }  /* if */
  }  /* if */
  return sym;
}  /* opname_member_function_symbol */


a_symbol_ptr opname_function_symbol(an_opname_kind kind)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind, or NULL if there is no such operator.
Only non-member functions will be found.  Function templates *will*
be found.
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;

  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Yes.  Look for one that's visible and a non-member function. */
    for (sym = symhdr->symbol; sym != NULL; sym = sym->next) {
      if (sym->class_of_which_a_member == NULL &&
          (is_function_symbol(sym) ||
           sym->kind == (a_symbol_kind)sk_function_template)) {
        /* A non-member function or function template. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return sym;
}  /* opname_function_symbol */



static void update_template_param_symbols(a_template_param_ptr  tpp,
                                          a_template_arg_ptr    arg_list)
/*
Update the symbol entries for template formal parameters to reflect the
values to be used for a given instantiation.  This routine is called by
push_scope to update the parameters for a new instantiation and is called
by pop_scope in the case of a recursive instantiation to recreate the
values needed for the previous call.
*/
{
  a_template_arg_ptr    tap = arg_list;

  db_enter(4, "update_template_param_symbols");
  /* Loop through the parameters and arguments.  There may be fewer
     template arguments than parameters when push_scope is done while
     scanning a the template argument list of a template class reference. */
  while (tpp != NULL) {
    register a_symbol_ptr  param_symbol = tpp->param_symbol;
    if (tap != NULL) {
      /* A template argument exists for this parameter. */
      if (tap->is_type) {
        param_symbol->variant.type = tap->variant.type;
      } else {
        param_symbol->variant.constant = tap->variant.constant;
      }  /* if */
      param_symbol->template_param_not_visible = FALSE;
      tap = tap->next;
    } else {
      /* No template parameter exists for this parameter.  Set the "not
         visible field in the symbol. */
      param_symbol->template_param_not_visible = TRUE;
    }  /* if */
    tpp = tpp->next;
  }  /* while */
  db_exit();
}  /* update_template_param_symbols */


static void restore_default_template_params(a_template_param_ptr  tpp)
/*
Update the symbol entries for template formal parameters to their
"resting values".  These are the initial values supplied when the template
declaration is scanned and are used as placeholders between instantiations.
*/
{
  db_enter(4, "restore_default_template_params");
  /* Loop through the parameters and and set them to either the original
     template type or the original template constant (as specified by the
     param_type or param_constant field). */
  while (tpp != NULL) {
    register a_symbol_ptr  param_symbol = tpp->param_symbol;
    if (param_symbol->kind == (a_symbol_kind)sk_type) {
      param_symbol->variant.type = tpp->variant.param_type;
    } else {
      param_symbol->variant.constant = tpp->variant.param_constant.ptr;
    }  /* if */
    param_symbol->template_param_not_visible = FALSE;
    tpp = tpp->next;
  }  /* while */
 db_exit();
}  /* restore_default_template_params */


/*
Return TRUE if the scope stack entry kind given by kind is for something
that has an effect on access control (a class, class reactivation, or
function).  Access control only exists in C++.
*/
#define is_scope_kind_that_affects_access_control(kind)               \
   ((kind) == (a_scope_kind)sck_class_struct_union ||                 \
    (kind) == (a_scope_kind)sck_class_reactivation ||                 \
    (kind) == (a_scope_kind)sck_function ||			      \
    (kind) == (a_scope_kind)sck_function_access)


/*
Return TRUE if the scope stack entry kind is for something that should
affect the current declarative level.  In C, the current declarative
level is the same as depth_scope_stack except when struct/union field
scopes are active; when they are, it indicates the first non-struct-or-union
scope.  In C++, struct/union/class scopes are real scopes; however,
class reactivations and template instantiations are not real scopes.
*/
#define is_scope_kind_that_affects_declarative_level(kind)		\
   ((C_dialect != C_dialect_cplusplus) ?				\
       /* C -- struct/union classes are not real scopes. */		\
        ((kind) != (a_scope_kind)sck_class_struct_union) :		\
        /* C++ -- class reactivations are not real scopes. */		\
        ((kind) != (a_scope_kind)sck_class_reactivation &&		\
         (kind) != (a_scope_kind)sck_template_instantiation))


static a_scope_ptr push_scope_full(a_scope_kind         kind,
				   a_scope_number       scope_number_to_reuse,
				   a_type_ptr           assoc_type,
				   a_routine_ptr        assoc_routine,
				   a_symbol_ptr         instance_sym,
				   a_symbol_ptr         template_sym,
				   a_template_arg_ptr   template_arg_list,
				   a_boolean            nested_instantiation)
/*
Begin a new name scope by pushing an entry on the scope stack.  kind indicates
the kind of scope (file, function, block, function prototype, etc.).  Returns
a pointer to the IL scope allocated (or NULL if no IL scope is allocated,
as happens, for example, with function prototype scopes).  For function
scopes, scope_number_to_reuse is the scope number to be used (it was chosen
when the function prototype was scanned, or is NO_SCOPE_NUMBER if it hasn't
been chosen yet); for class reactivation scopes, scope_number_to_reuse is
the class scope number; for the other cases, a new scope number is generated.
assoc_type points to an associated type for the cases where that's
meaningful (function prototype, class, class reactivation, and template
instantiation (for class templates only) scopes); it must be NULL in other
cases.  assoc_routine points to a routine for the function scope case and
for function access scopes; it must be NULL in other cases.  instance_symbol,
template_symbol, and template_arg_list are non-NULL only when a template
instantiation scope is being pushed; they represent, respectively, the symbol
for the class or function being instantiated or the static data member being
defined; the symbol identifying the template on which the instantiation or
definition is based; and the template argument list the produces the
specific version of the template.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp = NULL;
  a_boolean		  reactivate_template_params = FALSE;

  db_enter(3, "push_scope_full");
  if (depth_scope_stack+1 == (int)size_scope_stack) {
    /* The stack is full; expand it by reallocating. */
    sizeof_t new_size = size_scope_stack + SCOPE_STACK_INCREMENTAL_ALLOCATION;
    scope_stack = (a_scope_stack_entry_ptr)realloc_general(
                      (char *)scope_stack,
                      (sizeof_t)(size_scope_stack*sizeof(a_scope_stack_entry)),
                      (sizeof_t)(new_size*sizeof(a_scope_stack_entry)));
    size_scope_stack = new_size;
  }  /* if */
  /* Push the stack, initialize the new scope entry. */
  ssep = &scope_stack[++depth_scope_stack];
  /* Determine the scope number. */
  if ((scope_number_to_reuse != NO_SCOPE_NUMBER &&
       (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_func_prototype)) ||
      kind == (a_scope_kind)sck_class_reactivation ||
      kind == (a_scope_kind)sck_template_instantiation) {
    /* For function scopes, reuse the scope used for the parameters
       in the function declarator. */
    /* For class reactivations, re-establish the class scope and for template
       instantiations re-establish the template declaration scope. */
    ssep->number       = scope_number_to_reuse;
  } else {
    /* Assign a new scope number for other kinds of scopes. */
    ssep->number       = next_scope_number++;
  }  /* if */
  /* Save the current IL memory region for restoration by pop_scope.  That's
     important if we have temporarily switched into the file scope memory
     region. */
  ssep->prev_il_memory_region = curr_il_region_number;
  /* Allocate the IL scope entry if one is needed. */
  switch (kind) {
    case sck_file:
    case sck_function:
      /* Start a new memory region for the file scope or a function scope.
         This ensures that the intermediate language is divided into 
         manageable pieces.  This call also allocates the top-level
         scope entry for the region. */
      sp = new_il_region(kind, ssep->number, assoc_routine);
      sp->depth_in_scope_stack = depth_scope_stack;
      ssep->il_memory_region = curr_il_region_number;
      break;
    case sck_template_instantiation:
      /* Template instantiations should always take place in the file scope
         memory region. */
    case sck_func_prototype:
      /* Use the file scope memory region for a function prototype scope,
         since param types and types declared within it are pointed to from
         the function type, which is also in file scope memory.  (Parameter
         variables are not created until the function scope is pushed.)
         However, the IL scope is not allocated until it is needed -- it
         usually isn't. */
      sp = NULL;
      if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
        switch_il_region(FILE_SCOPE_REGION_NUMBER);
      }  /* if */
      ssep->il_memory_region = FILE_SCOPE_REGION_NUMBER;
      break;
    case sck_class_struct_union:
      /* Class/struct/union definitions require the file-scope memory region,
         since the entities created to represent the members are pointed to
         from the type entry. */
      if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
        switch_il_region(FILE_SCOPE_REGION_NUMBER);
      }  /* if */
      ssep->il_memory_region = FILE_SCOPE_REGION_NUMBER;
      /* Save a copy of the scope number in the class symbol supplement. */
      symbol_supplement_for_class(assoc_type)->member_decl_scope =
                                                              ssep->number;
      /* Only in C++ mode do classes have an associated scope. */
      if (C_mode()) {
        sp = NULL;
      } else {
        sp = alloc_scope(kind, ssep->number, (a_routine_ptr)NULL);
        sp->depth_in_scope_stack = depth_scope_stack;
      }  /* if */
      break;
    default:
      /* For scopes for which a new memory region is not begun, the associated
         memory region is the same as for the enclosing scope (there must be an
         enclosing scope, because the scope we're opening here is not the file
         scope). */
      ssep->il_memory_region = (ssep-1)->il_memory_region;
      /* For block scopes the IL scope is not allocated until it is needed,
         because usually it will not be needed. For class reactivations in
         C++, no scope is ever allocated. */
      sp = NULL;
  }  /* switch */
  /* Fill in the fields of the scope entry. */
  ssep->kind                     = kind;
  ssep->current_access           = (an_access_specifier)as_public;
  ssep->inactive_symbols_may_be_visible = FALSE;
  ssep->inside_local_class       = inside_local_class;
  ssep->template_param_decl_scope= FALSE;
  ssep->is_loop_scope            = FALSE;
  ssep->slow_lookup_required     = FALSE;
  ssep->return_value_optimization_possible = FALSE;
  ssep->in_prototype_instantiation = FALSE;
  ssep->defer_access_checks      = FALSE;
  ssep->symbols                  = NULL;
  ssep->last_symbol              = NULL;
  ssep->il_scope                 = sp;
  ssep->assoc_type               = assoc_type;
  ssep->assoc_routine            = assoc_routine;
  ssep->extern_type_fixup_list   = NULL;
  ssep->shareable_constants_list = NULL;
  ssep->last_routine_fixup       = NULL;
  ssep->last_parameter           = NULL;
  ssep->last_constant            = NULL;
  ssep->last_type                = NULL;
  ssep->last_variable            = NULL;
  ssep->last_nonstatic_variable  = NULL;
  ssep->last_label               = NULL;
  ssep->last_routine             = NULL;
  ssep->last_asm_entry           = NULL;
  ssep->first_scope              = NULL;
  ssep->last_scope               = NULL;
  ssep->last_dynamic_init        = NULL;
  ssep->last_pragma              = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  ssep->last_source_sequence_entry = NULL;
  ssep->source_sequence_avail_list = NULL;
  ssep->last_src_seq_sublist     = NULL;
  ssep->depth_innermost_ss_list_scope = depth_innermost_ss_list_scope;
  ssep->source_sequence_entries_disallowed =
                                       source_sequence_entries_disallowed;
  ssep->ss_list_instantiation_insert_point
                                 = NULL;
  ssep->saved_last_ss_entry      = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  ssep->last_hidden_name         = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
  ssep->last_template            = NULL;
#endif /* RECORD_TEMPLATES_IN_IL */
  ssep->depth_template_declaration_scope = depth_template_declaration_scope;
  ssep->depth_innermost_instantiation_scope =
                                       depth_innermost_instantiation_scope;
  ssep->instance_sym             = instance_sym;
  ssep->template_sym             = template_sym;
  ssep->template_arg_list        = template_arg_list;
  ssep->nested_instantiation     = nested_instantiation;
  ssep->source_position          = pos_curr_token;
  ssep->depth_innermost_function_scope = depth_innermost_function_scope;
  ssep->template_param_list      = NULL;
  ssep->last_label_decl_seq      = 0;
  ssep->pending_pragmas          = NULL;
  ssep->curr_construct_pragmas	 = NULL;
  ssep->next_scope_that_affects_access_control =
                          depth_of_innermost_scope_that_affects_access_control;
  ssep->deferred_access_checks   = NULL;
  ssep->last_deferred_access_check
                                 = NULL;
  ssep->saved_curr_deferred_access_scope
				 = curr_deferred_access_scope;
  ssep->saved_expr_stack         = expr_stack;  /* See also the setting of
                                                   expr_stack to NULL below. */
  ssep->curr_scope_object_lifetime = NULL;
  ssep->object_lifetime_avail_list = NULL;
  ssep->saved_curr_object_lifetime = curr_object_lifetime;
  /* Put the associated type (if any) into the IL scope (if any). */
  /* Note that the corresponding routine case was handled by the
     new_il_region call. */
  if (assoc_type != NULL && sp != NULL) sp->variant.assoc_type = assoc_type;
  /* Maintain the current declarative level.  It is the same as 
     depth_scope_stack except when struct/union field scopes are
     active; when they are, it indicates the first non-struct-or-union
     scope.  In C++, struct/union/class scopes are real scopes; however,
     class reactivations are not real scopes. */
  if (is_scope_kind_that_affects_declarative_level(kind)) {
    if (decl_scope_level < depth_innermost_instantiation_scope) {
      if (scope_stack[depth_innermost_instantiation_scope].
		template_sym->kind != (a_symbol_kind)sk_static_data_member) {
        /* Template parameters are considered part of the next scope that
           affects the declarative level -- except for static data member
           instantiations for which no such scope exists. */
        reactivate_template_params = TRUE;
      }  /* if */
    }  /* if */
    decl_scope_level = depth_scope_stack;
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Check for class reactivations, classes with base classes, and
       template instantiations.  When these are found name lookup is more
       involved.  If the new scope is neither, we can just use the state
       from the previous scope. */
    if (kind == (a_scope_kind)sck_class_reactivation ||
        kind == (a_scope_kind)sck_template_instantiation ||
        (kind == (a_scope_kind)sck_class_struct_union &&
         base_classes_of(assoc_type) != NULL)) {
      ssep->inactive_symbols_may_be_visible = TRUE;
    } else if (kind != (a_scope_kind)sck_file) {
      ssep->inactive_symbols_may_be_visible =
                                  (ssep-1)->inactive_symbols_may_be_visible;
    }  /* if */
    /* Pragma and instantiation scopes require that the slow lookup
       algorithm be used because they require that certain symbols on the
       active list not be considered. */
    if (kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_template_instantiation) {
      ssep->slow_lookup_required = TRUE;
    } else if (kind != (a_scope_kind)sck_file) {
      ssep->slow_lookup_required = (ssep-1)->slow_lookup_required;
    }
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      /* Keep track of the number of classes and class reactivations. */
      num_classes_on_scope_stack++;
      /* If we're entering a class and we're already inside a function,
         the class is a local class. */
      /* Note that this is done before depth_innermost_function_scope is
         cleared below. */
      if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        inside_local_class = ssep->inside_local_class = TRUE;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_template_instantiation) {
      a_template_symbol_supplement_ptr  tssp;

      tssp = template_supplement_for_symbol(template_sym);
      check_assertion(tssp != NULL);
      /* Save the depth of the innermost instantiation scope. */
      depth_innermost_instantiation_scope = depth_scope_stack;
      /* Update the symbols of the template parameters to represent the
         values of the actual arguments by simply changing each to point to
         the type or constant specified by the corresponding template argument.
         The old values do not need to be saved because they can be easily
         recreated by pop_scope. */
      update_template_param_symbols(tssp->parameters, template_arg_list);
      ssep->template_param_list = tssp->parameters;
      /* The current stack state is suspended when an template instantiation
         is done.  It will be restored in pop_scope. */
      inside_local_class = ssep->inside_local_class = FALSE;
      depth_innermost_function_scope =
              ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
      innermost_function_scope = NULL;
      if (template_sym->kind == (a_symbol_kind)sk_static_data_member) {
        /* Static data members don't have their own scope so the
           template parameters are added at the instantiation scope. */
        reactivate_template_params = TRUE;
      } else if (is_class_template_symbol(template_sym)) {
        if (instance_sym != NULL && is_template_class_symbol(instance_sym)) {
          ssep->in_prototype_instantiation =
                    instance_sym->variant.class_struct_union.extra_info->
                                                is_prototype_instantiation;
        }  /* if */
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      {
      /* Instantiations may be triggered almost anywhere, but the source
         sequence list for an instantiation has to be inserted at file scope.
         The current insert point is maintained in the scope stack entry
         for the file scope -- it is where the source sequence entries for
         the current instantiation should appear.  Make the changes required
         for this to happen. */
      a_scope_stack_entry_ptr      fs_ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
      a_source_sequence_entry_ptr  insert_point, new_last_ss_entry;

      /* The insert point, if any, is in the scope stack entry for the file
         scope. */
      insert_point = fs_ssep->ss_list_instantiation_insert_point;
      if (insert_point == NULL) {
        /* A NULL insert point means the source sequence entries for the
           instantiation may be added to the end of the file scope's source
           sequence list. */
      } else {
        /* A non-NULL insert point is the point *before which* the source
           sequence entries for the instantiation should be added.  Simply
           clip off the segment of source sequence entries, so that
           point->prev becomes the new end-of-list entry; the segment will
           be restored in pop_scope. */
        /* Save the head of the list segment that is to be clipped off. */
        ssep->ss_list_instantiation_insert_point = insert_point;
        /* Note the new end-of-list entry. */
        new_last_ss_entry = insert_point->prev;
        insert_point->prev = NULL;
        /* Save the old end-of-list entry (it will be restored in pop_scope)
           and replace it with the new one. */
        ssep->saved_last_ss_entry = fs_ssep->last_source_sequence_entry;
        fs_ssep->last_source_sequence_entry = new_last_ss_entry;
        if (new_last_ss_entry != NULL) {
          new_last_ss_entry->next = NULL;
        } else {
          fs_ssep->il_scope->source_sequence_list = NULL;
        }  /* if */
        /* Clear the old insert point (it will be restored in pop_scope). */
        fs_ssep->ss_list_instantiation_insert_point = NULL;
      }  /* if */
      }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (kind != (a_scope_kind)sck_file) {
      ssep->in_prototype_instantiation = (ssep-1)->in_prototype_instantiation;
    }  /* if */
    if (reactivate_template_params) {
      /* We want to ensure that the first declarative scope following
         an instantiation scope does not allow the redeclaration of a
         template parameter name.  This is done by saving a pointer to
         the template parameter list in the scope stack entry of the
         template instantiation scope and setting the templ_param_decl_scope
         flag in the scope for which this test must be done.  For template
         classes and template functions the scope is the next scope
         that affects the declarative level.  For static data members
         there is no such scope, but we set this flag in the instantiation
         scope for consistency. */
      ssep->template_param_decl_scope = TRUE;
    }  /* if */
  }  /* if */
  /* Maintain the depth of the innermost function scope. */
  if (kind == (a_scope_kind)sck_function) {
    depth_innermost_function_scope =
            ssep->depth_innermost_function_scope = depth_scope_stack;
    innermost_function_scope = sp;
  } else if (C_dialect == C_dialect_cplusplus &&
             kind == (a_scope_kind)sck_class_struct_union) {
    /* When we enter a class scope, the containing function scope (if any)
       becomes invisible in some respects.  (In particular, some expression
       processing routines need to know whether a function scope is the
       immediate context for processing.)  So clear out the variable and
       restore it in pop_scope. */
    depth_innermost_function_scope =
            ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
    innermost_function_scope = NULL;
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Maintain the depth of the innermost stack entry that affects access
       control. */
    if (is_scope_kind_that_affects_access_control(kind)) {
      depth_of_innermost_scope_that_affects_access_control = depth_scope_stack;
    } else if (kind == (a_scope_kind)sck_template_instantiation) {
      /* A template instantiation makes the things outside it invisible
         out to the file scope, and the file scope doesn't affect access
         control. */
      depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
    }  /* if */
    /* Determine whether this scope affects whether access checks can
       be deferred. */
    if (kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_class_struct_union) {
      /* A scope that introduces a new level at which deferred access
         checks may be recorded. */
      curr_deferred_access_scope = depth_scope_stack;
    } else if (kind == (a_scope_kind)sck_template_declaration ||
               kind == (a_scope_kind)sck_func_prototype ||
               kind == (a_scope_kind)sck_function_access ||
               kind == (a_scope_kind)sck_class_reactivation) {
      /* The current deferred access scope is left unchanged. */
    } else {
      /* For all other scopes, access checks cannot be deferred. */
      curr_deferred_access_scope = NO_SCOPE_DEPTH;
    }  /* if */
    /* Maintain the depth of a template declaration scope, if any. */
    if (kind == (a_scope_kind)sck_template_declaration) {
      ssep->depth_template_declaration_scope =
        depth_template_declaration_scope = depth_scope_stack;
    } else if (kind == (a_scope_kind)sck_template_instantiation) {
      /* A template instantiation.  The things outside the instantiation
         become invisible. */
      ssep->depth_template_declaration_scope =
        depth_template_declaration_scope = NO_SCOPE_DEPTH;
    }  /* if */
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_pragma) {
      /* When beginning a nested context, clear the expression stack. */
      expr_stack = NULL;
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Maintain the depth of the scope of the current source sequence list. */
  if (kind == (a_scope_kind)sck_function) {
    ssep->depth_innermost_ss_list_scope =
      depth_innermost_ss_list_scope = depth_scope_stack;
  } else if (kind == (a_scope_kind)sck_template_instantiation ||
             kind == (a_scope_kind)sck_file) {
    ssep->depth_innermost_ss_list_scope =
      depth_innermost_ss_list_scope = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* The creation of source sequence entries is suppressed in certain
     contexts. */
  if (kind == (a_scope_kind)sck_template_declaration ||
      kind == (a_scope_kind)sck_pragma) {
    ssep->source_sequence_entries_disallowed =
      source_sequence_entries_disallowed = TRUE;
  } else if (kind == (a_scope_kind)sck_template_instantiation) {
    if (ssep->in_prototype_instantiation || instance_sym == NULL) {
      /* Under no circumstances should source sequence entries be generated
         during a prototype instantiation.  Also, if instance_sym is NULL we
         are pushing the scope for the declaration (but not the body) of a
         template function -- no source sequence entries would be involved. */
      source_sequence_entries_disallowed = TRUE;
    } else if (assoc_type != NULL) {
      /* We are pushing the scope for a class template instantiation. */
      source_sequence_entries_disallowed =
                  !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS;
    } else {
      /* We are pushing the scope for a function template instantiation
         or for the definition of a template static data member. */
      source_sequence_entries_disallowed =
                  !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS;
    }  /* if */
    ssep->source_sequence_entries_disallowed =
                                     source_sequence_entries_disallowed;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (!C_mode()) {
    /* Do management related to the object lifetime stack. */
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_func_prototype) {
      /* These scopes do not nest properly from the point of view of object
         lifetimes, so break the object lifetime stack and then restore it
         in pop_scope. */
      curr_object_lifetime =
                   scope_stack[DEPTH_OF_FILE_SCOPE].curr_scope_object_lifetime;
    }  /* if */
    if (kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_block) {
      /* This is the sort of scope for which a new object lifetime is
         pushed. */
      push_object_lifetime((an_il_entry_kind)iek_scope, (char *)sp,
                           (an_object_lifetime_kind)(
                                (kind == (a_scope_kind)sck_file) ?
                                   olk_global_static : olk_block));
      ssep->curr_scope_object_lifetime = curr_object_lifetime;
    }  /* if */
  }  /* if */      
#if DEBUG
  if (debug_level >= 3) {
    db_scope_stack();
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sp;
}  /* push_scope_full */

a_scope_ptr push_scope(a_scope_kind         kind,
		       a_scope_number       scope_number_to_reuse,
		       a_type_ptr           assoc_type,
		       a_routine_ptr        assoc_routine)
/*
Interface to push_scope_full that is used for scopes other than template
instantiation scopes.
*/
{
  a_scope_ptr scope;
  scope = push_scope_full(kind, scope_number_to_reuse, assoc_type,
			  assoc_routine, (a_symbol_ptr)NULL,
			  (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL,
			  /*nested_instantiation=*/FALSE);
  return scope;
}  /* push_scope */


a_scope_ptr push_template_instantiation_scope
                           (a_scope_number       scope_number_to_reuse,
			    a_type_ptr           assoc_type,
			    a_routine_ptr        assoc_routine,
			    a_symbol_ptr         instance_sym,
			    a_symbol_ptr         template_sym,
			    a_template_arg_ptr   template_arg_list,
			    a_boolean            nested_instantiation)
/*
Interface to push_scope_full that is used for template instantiation
scopes.
*/
{
  a_scope_ptr scope;
  scope = push_scope_full((a_scope_kind)sck_template_instantiation,
			  scope_number_to_reuse,
			  assoc_type, assoc_routine, instance_sym,
			  template_sym, template_arg_list,
			  nested_instantiation);
  return scope;
}  /* push_template_instantiation_scope */


#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY

a_boolean check_for_file_scope_type_with_same_name(a_symbol_ptr sym_to_find)
/*
This routine is used to implement the "transitional model" for nested types.
Given a symbol this routine looks for a file scope type (class, struct, union,
typedef, or enum) with the same name.  Returns TRUE if one is found, FALSE
otherwise.
*/
{
  a_symbol_ptr   sym;
  a_boolean      found = FALSE;

  sym = sym_to_find->header->symbol;
  while (sym != NULL) {
    if (sym->decl_scope == FILE_SCOPE_NUMBER) {
      /* Look for class, struct, union, enum, or typedef. */
      if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
        found = TRUE;
        break;
      }  /* if */
    }  /* if */
    sym = sym->next;
  }  /* while */
  return found;
}  /* check_for_file_scope_type_with_same_name */


a_symbol_ptr find_cfront_transitional_nested_type_symbol
                                             (a_symbol_ptr sym_to_find)
/*
Given a symbol looks through the inactive list for a type symbol
of the same name whose type has the transitional name mangling flag set.
This is used for error generation of the transitional model for nested
type support.
*/
{
  a_symbol_ptr   sym;

  sym = sym_to_find->header->inactive_symbols;
  while (sym != NULL) {
    /* Look for class, struct, union, enum, or typedef. */
    if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
      a_type_ptr  sym_type = type_symbol_type(sym);
      if (sym_type->use_cfront_transitional_nested_type_name_mangling) {
        break;
      }  /* if */
    }  /* if */
    sym = sym->next;
  }  /* while */
#if CHECKING
  if (sym == NULL) {
    internal_error
      ("find_cfront_transitional...: no semivisible symbol found");
  }  /* if */
#endif /* CHECKING */
  return sym;
}  /* find_cfront_transitional_nested_type_symbol */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */



static void check_referenced_member_functions(a_symbol_ptr  class_sym)
/*
Issue an error for member functions that have been referenced but are
internally linked and undefined.  Only report instances in which the IL
entry is marked "referenced", since symbols for virtual functions may be
marked as referenced without the associated routine having actually been
called.
*/
{
  a_symbol_ptr   sym, rout_sym;
  a_routine_ptr  rp;
  a_boolean      is_overloaded;

  if (C_dialect == C_dialect_cplusplus) {
    sym = class_sym->variant.class_struct_union.extra_info->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        /* Member function. */
        rout_sym = sym;
        is_overloaded = FALSE;
      } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* Overloaded member function. */
        rout_sym = sym->variant.overloaded_function.symbols;
        is_overloaded = TRUE;
      } else {
        /* Not a member function -- keep looping. */
        continue;
      }  /* if */
      for (; rout_sym != NULL;
             rout_sym = (is_overloaded ? rout_sym->next : NULL)) {
        rp = rout_sym->variant.routine.ptr;
        if (rp->source_corresp.referenced) {
          /* Referenced. */
          if (rp->storage_class == (a_storage_class)sc_static &&
              rp->assoc_scope == NULL_region_number) {
            /* An undefined routine with internal linkage that has been
               referenced -- issue an error. */
            pos_sy_error(ec_never_defined, &rout_sym->decl_position, rout_sym);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
}  /* check_referenced_member_functions */


static void report_unreferenced(a_symbol_ptr  	  sym,
                                an_error_code	  error_code,
			        an_error_severity normal_severity)
/*
Issue a warning for an unreferenced entity.  However, demote the warning to
a remark if the entity is a file-scope entity declared in an include file.
*/
{
  if (normal_severity == es_remark ||
      (depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
       seq_is_in_include_file(sym->decl_position.seq))) {
    pos_sy_remark(error_code, &sym->decl_position, sym);
  } else {
    pos_sy_warning(error_code, &sym->decl_position, sym);
  }  /* if */
}  /* report_unreferenced */


/*
Return TRUE if a type is a completable incomplete type, i.e., it's incomplete
but it's not void.
*/
#define is_completable_type(tp)                                       \
  (is_incomplete_type(tp) && !is_void_type(tp))


/*
Return TRUE if a type is an array type whose elements have a complete
type.  This rules out arrays of incomplete struct/union types (an extension).
*/
#define is_array_with_complete_element_type(tp)                       \
  (is_array_type(tp) && !is_incomplete_type(array_element_type(tp)))


static void end_of_scope_symbol_check(a_symbol_ptr  sym,
                                      a_routine_ptr curr_routine)
/*
The symbol sym is about to be removed from the symbol table at the end of
a scope.  Do any checking or processing required (e.g., issue a warning
message if the symbol is unreferenced).  If the scope that is ending is
for a function, curr_routine points to the routine entry; otherwise, it is
NULL.
*/
{
  a_storage_class storage_class;
  a_type_ptr      var_type, type_ptr;
  a_variable_ptr  var_ptr;
  a_routine_ptr   rout_ptr;
#if CHECKING
  a_source_correspondence  *scp = NULL;
#endif /* CHECKING */

  switch (sym->kind) {
    case sk_variable:
      /* Variable or parameter. */
      var_ptr = sym->variant.variable.ptr;
      storage_class = var_ptr->storage_class;
      if (storage_class == (a_storage_class)sc_unspecified) {
        /* Note that if this test succeeds (i.e., the variable has
           storage class sc_unspecified), we do not do the test for
           referenced.  That's because an external variable can be assumed
           to be referenced from another compilation unit.  The referenced
           flag in the IL entry is set slightly later, in the
           sk_extern_variable processing. */
      } else if (storage_class == (a_storage_class)sc_extern) {
        /* No warning for unused "extern" variables; this is a long-standing
           C convention. */
      } else if (C_dialect == C_dialect_cplusplus &&
                 depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
                 is_const_qualified_type(var_ptr->type) &&
                 seq_is_in_include_file(sym->decl_position.seq)) {
        /* Since a const variable defined in a header is the C++ idiom
           corresponding to #define, issue no diagnostic on not using it. */
      } else if (var_ptr->is_parameter) {
        if (!sym->referenced) {
          /* An unreferenced parameter.  Warn unless a lint-style "argsused"
             comment appeared.  Also do not warn for parameters of "main". */
#if CHECKING
          if (curr_routine == NULL) {
            internal_error(
               "end_of_scope_symbol_check: parameter with no assoc routine");
          }  /* if */
#endif /* CHECKING */
          /* In C++ a routine type can have type qualifiers above it. */
          if (skip_typerefs(curr_routine->type)->variant.routine.
                                              extra_info->lint_argsused_flag) {
            /* The "argsused" flag was specified, so no warning is issued. */
          } else if (curr_routine == il_header.main_routine) {
            /* No warning for arguments of the main program, since
               they're dictated by the environment. */
#if ASM_FUNCTION_ALLOWED
          } else if (curr_routine->storage_class == (a_storage_class)sc_asm) {
            /* Parameters of "asm" functions are not referenced in the 
               usual way, so do not issue warnings. */
#endif /* ASM_FUNCTION_ALLOWED */
          } else {
            /* Unreferenced parameter. */
            report_unreferenced(sym, ec_declared_but_not_referenced,
                                es_remark);
          }  /* if */
        } else if (var_ptr->param_value_has_been_changed &&
                   !sym->variant.variable.used) {
          report_unreferenced(sym, ec_set_but_not_used, es_warning);
        }  /* if */
      } else if (var_ptr->is_handler_param) {
        /* A handler parameter. */
        if (!sym->referenced) {
          /* Unreferenced handler parameter. */
          report_unreferenced(sym, ec_declared_but_not_referenced,
                              es_remark);
        } else if (var_ptr->param_value_has_been_changed &&
                   !sym->variant.variable.used) {
          report_unreferenced(sym, ec_set_but_not_used, es_warning);
        }  /* if */
      } else if (!sym->referenced ||
                 (sym->variant.variable.value_has_been_set &&
                  !sym->variant.variable.used)) {
        /* An unreferenced or unused variable or an unused parameter. */
        a_boolean           suppress_warning;
        an_error_code       error_code;
        an_error_severity   severity;
        an_init_kind        init_kind;
        an_initializer_ptr  ip;

        /* Check for a dynamic initialization that has side effects (such as
           a constructor call).  If such an initialization exists, issue a
           remark rather than a warning. */
        get_variable_initializer(var_ptr,
                                 scope_stack[depth_scope_stack].il_scope,
                                 &init_kind, &ip);
        if (init_kind == (an_init_kind)initk_dynamic &&
            (dynamic_init_has_side_effects(ip->dynamic, &suppress_warning) ||
             suppress_warning)) {
          severity = es_remark;
        } else {
          severity = es_warning;
        }  /* if */
        /* Issue different warnings depending on whether the variable was
           completely unreferenced or was set but not used. */
        if (!sym->referenced) {
          error_code = ec_declared_but_not_referenced;
        } else {
          check_assertion(sym->variant.variable.value_has_been_set);
          error_code = ec_set_but_not_used;
        }  /* if */
        report_unreferenced(sym, error_code, severity);
      }  /* if */
#if CHECKING
      scp = &var_ptr->source_corresp;
#endif /* CHECKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Do fixup on the source sequence entry for a tentative definition. */
      if (C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
          sym->defined) {
        a_source_sequence_entry_ptr   ssep;
        a_src_seq_secondary_decl_ptr  sssdp;

        ssep = var_ptr->source_corresp.source_sequence_entry;
        if (ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
          /* This source sequence entry must represent a tentative definition
             of the variable (the first, if there were more than one) -- turn
             it into a primary declaration. */
          sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
#if CHECKING
          check_assertion(sssdp->decl_position.seq == scp->decl_position.seq &&
                          sssdp->decl_position.column ==
                                                  scp->decl_position.column);
#endif /* CHECKING */
          ssep->entity.kind = (a_byte_il_entry_kind)iek_variable;
          ssep->entity.ptr = (char *)var_ptr;
          check_assertion(var_ptr->declared_type == NULL);
          var_ptr->declared_type = sssdp->declared_type;
        }  /* if */
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      break;
    case sk_overloaded_function:
      /* For each function or function template symbol on the overload list
         do the check. */
      for (sym = sym->variant.overloaded_function.symbols;
           sym != NULL;
           sym = sym->next) {
        end_of_scope_symbol_check(sym, curr_routine);
      }  /* for */
      break;
#if CHECKING
    case sk_member_function:
      rout_ptr = sym->variant.routine.ptr;
      scp = &rout_ptr->source_corresp;
      break;
#endif /* CHECKING */
    case sk_routine:
      /* Function. */
      rout_ptr = sym->variant.routine.ptr;
      storage_class = rout_ptr->storage_class;
      if (storage_class == (a_storage_class)sc_unspecified) {
        /* Regard functions with "unspecified" storage class to be referenced
           somewhere, even if not in the current translation unit. */
        rout_ptr->source_corresp.referenced = TRUE;
        sym->referenced = TRUE;
      } else if (rout_ptr->source_corresp.referenced) {
        /* Referenced function.  We check the IL referenced flag because
           a reference in, say, a sizeof operation doesn't count. */
        if (storage_class == (a_storage_class)sc_static &&
            depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
            rout_ptr->assoc_scope == NULL_region_number) {
          /* A non-external routine that is referenced was never given
             a body (3.7, constraints).  This is checked only at the file
             scope because there can be symbols with linkage defined in
             inner scopes, but only the file scope declaration can have
             a body. */
          if (C_dialect == C_dialect_pcc) {
            /* In pcc mode, just change the routine to extern. */
            rout_ptr->storage_class = (a_storage_class)sc_extern;
            rout_ptr->source_corresp.name_linkage =
                                         (a_name_linkage_kind)nlk_external;
          } else {
            pos_sy_error(ec_never_defined,
                         &sym->decl_position, sym);
          }  /* if */
        }  /* if */
      } else if (!sym->referenced) {
        /* Unreferenced function. */
        if (storage_class == (a_storage_class)sc_extern) {
          /* No warning on unused "extern" routines; this is a
             long-standing C tradition. */
        } else if (rout_ptr->is_inline && sym->defined &&
                   seq_is_in_include_file(sym->decl_position.seq)) {
          /* No diagnostic on inline non-member functions defined in a header
             file. */
#if ASM_FUNCTION_ALLOWED
        } else if (storage_class == (a_storage_class)sc_asm) {
          /* "asm" functions don't generate any code unless referenced,
             and may appear in header files, so no warning is generated. */
#endif /* ASM_FUNCTION_ALLOWED */
        } else {
          /* An unreferenced routine. */
          report_unreferenced(sym, ec_declared_but_not_referenced,
			      es_warning);
        }  /* if */
      }  /* if */
#if CHECKING
      scp = &rout_ptr->source_corresp;
#endif /* CHECKING */
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      /* Check for referenced but undefined non-extern functions. */
      check_referenced_member_functions(sym);
      /* Fall through for further processing. */
    case sk_enum_tag:
      /* Struct, union, or enum tag. */
      type_ptr = type_symbol_type(sym);
      if (is_incomplete_type(type_ptr)) {
        /* A tag that was never completed.  This is not an error.
           It's not even a warning, because people really do this
           intentionally.  Declaring something of this type would
           be an error; declaring something a pointer to this type
           would be allowed. */
        if (scope_stack[depth_scope_stack].kind ==
                               (a_scope_kind)sck_template_instantiation) {
          /* Type was declared in a prototype instantiation.  It should not
             be added to a types list. */
        } else if (type_ptr->source_corresp.class_of_which_a_member != NULL &&
                   (scope_stack[depth_scope_stack].kind !=
                      (a_scope_kind)sck_class_struct_union ||
                    scope_stack[depth_scope_stack].assoc_type !=
                      type_ptr->source_corresp.class_of_which_a_member)) {
          /* This must be the symbol for an anonymous union member type that
             has been promoted out of the scope of the anonymous union.
             The type is already on a list. */
        } else {
          /* Add it now to the current scope's type list.  It was not added
             previously because no actual definition appeared.   Don't
             do this for non-real template class instantiations.  Don't
             do this for tags reentered from the prototype scope because they
             will have been added when the prototype scope was popped. */
          if ((sym->kind == (a_symbol_kind)sk_enum_tag ||
               is_real_class_symbol(sym)) &&
              !sym->reentered_from_prototype_scope) {
            add_to_types_list(type_ptr, depth_scope_stack);
          }  /* if */
        }  /* if */
      }  /* if */
#if CHECKING
      scp = &type_ptr->source_corresp;
#endif /* CHECKING */
      break;
    case sk_label:
      /* Label. */
      if (sym->variant.label.ptr->variant.exec_stmt == NULL) {
        /* A label that was used but never defined. */
        pos_sy_error(ec_never_defined, &sym->decl_position, sym);
      } else if (!sym->referenced) {
        /* An unreferenced label. */
        report_unreferenced(sym, ec_declared_but_not_referenced,
			    es_warning);
      }  /* if */
      break;
    case sk_extern_variable:
      /* Symbol for a variable with linkage. */
      var_ptr = sym->variant.extern_symbol_descr->variant.variable;
      storage_class = var_ptr->storage_class;
      var_type = skip_typerefs(var_ptr->type);
      /* Look for variables that have retained an incomplete type
         that isn't just plain "void". */
      if (is_completable_type(var_type)) {
        if ((storage_class == (a_storage_class)sc_unspecified ||
             (C_dialect != C_dialect_cplusplus &&
              storage_class == (a_storage_class)sc_static)) &&
            is_array_with_complete_element_type(var_type)) {
          /* A file-scope incomplete array with no storage class, for example
             "int a[];" at file scope; or else (in C mode) a file-scope
             incomplete array with static storage class.  Such an array is
             defined by the standard (3.7.2 semantics) to be equivalent
             to "int a[] = {0};"; change the size to 1 here.  However, if
             the extern_variable entry has more complete type information
             (i.e., an exact dimension), use that.  Note that no explicit
             check for scope depth is required since sk_extern_variable
             symbols are generated for file-scope variables only. */
          /* The test for complete element type disallows arrays of
             incomplete struct/unions (which are an extension). */
          if (!is_incomplete_type(sym->variant.extern_symbol_descr->type)){
            /* The external symbol entry has a dimension for the array.
               Use it.  This would happen for
                 int a[];
                 main () {extern int a[5];}
            */
            var_ptr->type = sym->variant.extern_symbol_descr->type;
          } else {
            /* There is no additional information; the array has an
               unknown size. */
            if (C_dialect != C_dialect_pcc ||
                storage_class == (a_storage_class)sc_static) {
              /* Change the array size to 1. */
              a_type_ptr array_type = alloc_type((a_type_kind)tk_array);
              copy_type(var_type, array_type);
              check_assertion(
                          !array_type->variant.array.is_variable_size_array);
              array_type->variant.array.variant.number_of_elements = 1;
              set_type_size(array_type);
              var_ptr->type = array_type;
              /* No need to call check_linked_entity_type here.  We
                 know elem[] and elem[1] are compatible. */
            } else {
              /* pcc mode.  Leave the size as zero but change the
                 storage class to extern. */
              var_ptr->storage_class = (a_storage_class)sc_extern;
            }  /* if */
          }  /* if */
        } else if (storage_class != (a_storage_class)sc_extern) {
          /* A file-scope variable that defines storage and has
             an incomplete type, as in "struct incomplete v;".  Issue an
             error.  Note that arrays like this, except for static arrays,
             were handled above. */
          pos_st_error(ec_var_retained_incomp_type, &sym->decl_position,
                       sym->header->identifier);
        }  /* if */
      }  /* if */
      /* If the storage class remains sc_unspecified, set the
         referenced flag now to indicate possible references from other
         compilation units. */
      if (var_ptr->storage_class == (a_storage_class)sc_unspecified) {
        var_ptr->source_corresp.referenced = TRUE;
      }  /* if */
      break;
#if CHECKING
    case sk_static_data_member:
      scp = &sym->variant.static_data_member.variable->source_corresp;
      break;
    case sk_constant:
      scp = &sym->variant.constant->source_corresp;
      break;
    case sk_field:
      if (sym->variant.field.anonymous_parent_object == NULL) {
        scp = &sym->variant.field.ptr->source_corresp;
      } else {
        a_symbol_ptr  apo_sym;

        for (apo_sym = sym->variant.field.anonymous_parent_object;
             apo_sym->kind == (a_symbol_kind)sk_field;
             apo_sym = apo_sym->variant.field.anonymous_parent_object) {
          if (apo_sym->variant.field.anonymous_parent_object == NULL) {
            scp = &apo_sym->variant.field.ptr->source_corresp;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      break;
    case sk_type:
      scp = &sym->variant.type->source_corresp;
      break;
#endif /* CHECKING */
    case sk_class_template:
      {
      a_template_symbol_supplement_ptr  tssp;
      a_symbol_ptr                      template_class_sym;
      tssp = sym->variant.template_info;
      template_class_sym = tssp->variant.class_template.instantiations;
      for (; template_class_sym != NULL;
             template_class_sym = template_class_sym->next) {

        if (template_class_sym->
                    variant.class_struct_union.extra_info->is_nonreal_class) {
          /* Skip the recursive check for prototype instantiation of a class
             template. */
        } else {
          end_of_scope_symbol_check(template_class_sym, curr_routine);
        }  /* if */
      }  /* for */
      }
      break;
    case sk_function_template:
      {
      a_template_instance_ptr  tip;

      tip = sym->variant.template_info->variant.function.instantiations;
      for (; tip != NULL; tip = tip->next) {
        if (tip->specific_decl) {
          /* A user declaration was provided, so the associated symbol should
             be on the overload list -- ignore it here. */
        } else {
          end_of_scope_symbol_check(tip->instance_sym, curr_routine);
        }  /* if */
      }  /* for */
      }
    default:
      /* No processing for other kinds. */
      break;
  }  /* switch */
#if CHECKING
  if (scp != NULL &&
      sym->class_of_which_a_member != scp->class_of_which_a_member &&
      scp->class_of_which_a_member->variant.class_struct_union.extra_info->
                   anonymous_union_kind == (an_anonymous_union_kind)auk_none) {
    internal_error("end_of_scope_symbol_check: bad class_of_which_a_member");
  }  /* if */
#endif /* if */
}  /* end_of_scope_symbol_check */


void pop_scope(void)
/*
End a name scope by popping an entry off the scope stack.
*/
{
  a_scope_stack_entry_ptr  ssep, parent_ssep;
  a_symbol_ptr             sym;
  a_routine_ptr            curr_routine = NULL;
  a_memory_region_number   old_memory_region_number, new_memory_region_number;
  a_scope_kind             kind;
  an_extern_type_fixup_ptr etfp;
  a_scope_depth            scope_depth;
  a_boolean                old_region_still_needed;
  a_boolean		   do_semivisible_type_processing = TRUE;
  a_boolean                is_prototype_instantiation = FALSE;
  a_scope_ptr              il_scope;

  db_enter(3, "pop_scope");
  ssep = &scope_stack[depth_scope_stack];
  kind = ssep->kind;
  if (kind == (a_scope_kind)sck_function) {
    /* If the scope is for a routine, get a pointer to the routine. */
    curr_routine = ssep->il_scope->variant.routine.ptr;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (ssep->symbols != NULL || debug_level >= 4) {
      fprintf(f_debug, "pop_scope: number = %d, depth = %d",
              ssep->number, depth_scope_stack);
      if (curr_routine != NULL) {
        (void)fputs(", curr_routine = \"", f_debug);
        db_name(&curr_routine->source_corresp);
        (void)fputc('"', f_debug);
      } else if ((kind == (a_scope_kind)sck_class_struct_union ||
                  kind == (a_scope_kind)sck_class_reactivation) &&
                 ssep->assoc_type != NULL) {
        (void)fputs(", class = \"", f_debug);
        db_name(&ssep->assoc_type->source_corresp);
        (void)fputc('"', f_debug);
      } else {
        fputs(", kind = ", f_debug);
        (void)db_scope_kind(kind);
      }  /* if */
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Determine whether types defined in this scope should be handled
     as semivisible types.  Template classes and classes nested within
     template classes do not have this processing done. */
  {
    /* Loop back through the scope stack until we find a scope that is
       not a class_struct_union scope or until we find a class_struct_union
       scope that is a template class_struct_union. */
    a_scope_depth sd = depth_scope_stack;
    for (; sd > DEPTH_OF_FILE_SCOPE; sd--) {
      a_scope_kind skind = scope_stack[sd].kind;
      if (skind != (a_scope_kind)sck_class_struct_union) break;
      if (is_template_class_type(scope_stack[sd].assoc_type)) {
        do_semivisible_type_processing = FALSE;
        break;
      }  /* if */
    }  /* for */
  }
  /* Check for prototype instantiation of a class template. */
  if (kind == (a_scope_kind)sck_class_struct_union &&
      (symbol_supplement_for_class(ssep->assoc_type))->is_nonreal_class) {
    is_prototype_instantiation = TRUE;
  }  /* if */
  /* Remove the symbols declared in this scope from the symbol table.
     Check for unreferenced symbols, and issue warnings for those. */
  for (sym = ssep->symbols; sym != NULL; sym = sym->next_in_scope) {
    if (kind == (a_scope_kind)sck_func_prototype && !is_tag_symbol(sym)) {
      /* Don't check on symbols entered in the scope of a function prototype.
         They will be reentered in the scope of the function and should be
         checked when the function scope is popped.  Tag symbols are
	 checked because tags associated with incomplete types need to
	 be put on the types list of the prototype scope. */
    } else if (is_prototype_instantiation) {
      /* Don't check on symbols entered in the scope of a class template
         prototype instantiation -- the information may not be complete. */
    } else {
      end_of_scope_symbol_check(sym, curr_routine);
    }  /* if */
    /* Remove the symbol from the symbol table.  (Note that symbols are not
       removed from the scope list.  This is because they must sometimes
       remain accessible and the scope list, saved away in some other data
       structure, is a convenient way to get at them again.) */
    unlink_symbol_from_symbol_table(sym);
    /* Put struct/union/class members and template parameters on the
       inactive list of the proper symbol header. */
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_template_declaration) {
      sym->next = sym->header->inactive_symbols;
      sym->header->inactive_symbols = sym;
      /* Check for nested class/struct/unions on the inactive list.  If
         there are any, set the flag in the symbol header.  This is
         used to support the nonnested class anachronism.  We do not
         apply the anachronism to template classes. */
      if (kind == (a_scope_kind)sck_class_struct_union && allow_anachronisms) {
        if ((is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) &&
            do_semivisible_type_processing) {
          sym->header->any_nested_types_on_inactive_list = TRUE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
          /* Cfront 2.1 implements a special "transitional model" for nested
             types.  Under this model cfront promotes nested types to the file
             scope unless a file scope type of the same name is already
             defined.  Subsequent definition of additional nested types with
             the same name is an error.  This code, which simulates the
             cfront behavior, sets a flag for the first nested type with
             a given name and issues errors on subsequent definitions. */
          {
            if (cfront_2_1_mode) {
              /* Only do this if the name is not a type name at file
                 scope. */
              if (!check_for_file_scope_type_with_same_name(sym)) {
                if (!sym->header->
                    has_cfront_transitional_nested_type_mangled_name) {
                  a_type_ptr   sym_type;
                  sym_type = type_symbol_type(sym);
                  sym->header->
                    has_cfront_transitional_nested_type_mangled_name = TRUE;
                  sym_type->
                    use_cfront_transitional_nested_type_name_mangling = TRUE;
                } else {
                  a_symbol_ptr other_sym;
                  other_sym = find_cfront_transitional_nested_type_symbol(sym);
                  pos_sy2_error(ec_cfront_multiple_nested_types,
                                &sym->decl_position, sym, other_sym);
                }  /* if */
              }  /* if */
            }  /* if */
          }
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
        }  /* if */
      }  /* if */
    }  /* if */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
    /* Look for file scope symbols that have the "cfront transitional" nested
       type flag set.  This indicates that a file scope symbol with the same
       name was defined after the nested class was seen.  This is an error
       in cfront compatibility mode. */
    if (kind == (a_scope_kind)sck_file && cfront_2_1_mode) {
      if (sym->header->has_cfront_transitional_nested_type_mangled_name) {
        if (is_type_symbol(sym)) {
          a_type_ptr	sym_type = type_symbol_type(sym);
          if (sym_type->use_cfront_transitional_nested_type_name_mangling) {
            /* The symbol being popped is already designated as the
               transitional nested type, don't issue an error.  This can
               occur when the type is promoted out of an anonymous union and
               reentered on the active list at file scope. */
          } else {
            a_symbol_ptr other_sym;
            other_sym = find_cfront_transitional_nested_type_symbol(sym);
            pos_sy2_error(ec_cfront_global_defined_after_nested_type,
                          &sym->decl_position, sym, other_sym);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  }  /* for */
  il_scope = ssep->il_scope;
  if (C_dialect == C_dialect_cplusplus) {
    if (kind == (a_scope_kind)sck_function ||
        (kind == (a_scope_kind)sck_block && il_scope != NULL)) {
      /* If there are any local classes, check for compiler-generated
         virtual destructors for which bodies should be put out. */
      generate_required_virtual_destructor_bodies(il_scope->types);
    }  /* if */
  }  /* if */
  if (ssep->curr_construct_pragmas != NULL && total_errors != 0) {
    /* There should be no items remaining on the list.  If any errors
       occurred, the list items may be a result of the errors.  Discard
       the items on the list.  If no errors have been issued, an internal
       error will be issued below. */
    free_pending_pragma_list(ssep->curr_construct_pragmas);
    ssep->curr_construct_pragmas = NULL;
  }  /* if */
  /* There should be no entries left on the curr_construct_pragmas list when
     the scope stack is popped. */
  check_assertion_str2(ssep->curr_construct_pragmas == NULL,
		       "pop_scope:", "curr_construct_pragmas != NULL");
  if (ssep->pending_pragmas != NULL) {
    /* Issue diagnostics on any pragmas that are still on the pending list. */
    end_of_scope_pragma_processing(ssep->pending_pragmas);
  }  /* if */
  if (!C_mode()) {
    /* Do management related to the object lifetime stack.  Don't pop the
       file scope object lifetime yet, though, because we need it in IL
       lowering; see below */
    if (kind == (a_scope_kind)sck_block ||
        kind == (a_scope_kind)sck_function) {
      /* For a function or block scope, pop the current object lifetime, which
         ought to be the one created when this scope was pushed. */
      check_assertion_str2(curr_object_lifetime ==
                                          ssep->curr_scope_object_lifetime,
                           "pop_scope: unexpected curr_object_lifetime",
                           "for function or block scope");
      (void)pop_object_lifetime();
      if (kind == (a_scope_kind)sck_function) {
        if (!il_scope->variant.routine.ptr->compiler_generated) {
          /* Flow control wrapup for statement processing is done here because
             part of what needs to be done is dependent on popping the object
             lifetime of the function scope. */
          wrapup_control_flow_processing(il_scope);
        }  /* if */
        /* Functions are always processed in the context of the file scope
           lifetime; restore the lifetime stack as it was when the function
           scope was pushed. */
        curr_object_lifetime = ssep->saved_curr_object_lifetime;
      }  /* if */
    } else if (kind == (a_scope_kind)sck_pragma ||
               kind == (a_scope_kind)sck_func_prototype ||
               kind == (a_scope_kind)sck_template_instantiation) {
      check_assertion_str2(curr_object_lifetime ==
                                   scope_stack[DEPTH_OF_FILE_SCOPE].
                                                   curr_scope_object_lifetime,
                           "pop_scope: curr_object_lifetime is not that of",
                           "file scope");
      curr_object_lifetime = ssep->saved_curr_object_lifetime;
    }  /* if */
  }  /* if */      
  check_assertion_str2(ssep->defer_access_checks == FALSE &&
                       ssep->deferred_access_checks == NULL,
                       "pop_scope:", "deferred access checks still on list");
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
  if (db_active) {
    /* Display source sequence lists for debug purposes. */
    if (il_scope != NULL && il_scope->source_sequence_list != NULL) {
      dump_ss(il_scope);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ssep->first_scope != NULL) {
    /* Transfer the list of scopes nested within the current scope
       to the IL scope entry if there is one, or otherwise add it to
       the local scopes list for the parent scope.  We are doing this
       to avoid allocating IL scopes for empty block scopes and empty
       non-top-level function prototype scopes. */
    if (il_scope == NULL) {
      parent_ssep = ssep-1;
      /* Generate the scope entry if ssep is a top-level function prototype
         scope, i.e., one that is not nested within another function prototype
         scope. */
      if (kind == (a_scope_kind)sck_func_prototype &&
          parent_ssep->kind != (a_scope_kind)sck_func_prototype) {
        il_scope = ensure_il_scope_exists(ssep);
      }  /* if */
    }  /* if */
    if (il_scope != NULL) {
      /* There is an allocated IL scope entry. */
      il_scope->scopes = ssep->first_scope;
    } else {
      /* Add the list of scopes to the list for the parent scope. */
      if (parent_ssep->first_scope == NULL) {
        parent_ssep->first_scope = ssep->first_scope;
      } else {
        parent_ssep->last_scope->next = ssep->first_scope;
      }  /* if */
      parent_ssep->last_scope = ssep->last_scope;
    }  /* if */
  }  /* if */
  /* Determine and remember the current (old) memory region, to see
     if it changes when returning to the outer scope. */
  old_memory_region_number = ssep->il_memory_region;
  /* If the old memory region number does not appear anywhere in the
     remaining stack, the region is no longer needed by the front end. */
  old_region_still_needed = FALSE;
  for (scope_depth = depth_scope_stack-1; scope_depth >= 0; scope_depth--) {
    if (scope_stack[scope_depth].il_memory_region == old_memory_region_number){
      old_region_still_needed = TRUE;
      break;
    }  /* if */
  }  /* for */
  if (!old_region_still_needed) {
    /* The old memory region is no longer needed. */
#if DO_IL_LOWERING
    /* Do IL lowering (change the C++ IL into C IL). */
    lower_il_memory_region(old_memory_region_number);
#endif /* DO_IL_LOWERING */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
    if (kind == (a_scope_kind)sck_function) {
      /* If a function or block scope has local types or static variables,
         make a special entry to record those orphan lists on the il_header
         scope_orphaned_list_headers list so they can be found when processing
         the file scope memory region.  Note that processing for block scopes
         is done at the end of the function scope to give IL lowering a chance
         to add variables and types in block scopes. */
      add_scope_orphaned_il_lists(il_scope);
    }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
    /* Clear out the shareable constants table for the file scope or a
       function scope. */
    if (old_memory_region_number == FILE_SCOPE_REGION_NUMBER) {
      empty_shareable_constants_table();
    } else {
      empty_func_shareable_constants_table();
    }  /* if */
  }  /* if */
  if (!C_mode()) {
    /* Pop the file scope object lifetime.  This must be done after IL
       lowering. */
    if (kind == (a_scope_kind)sck_file) {
      check_assertion(curr_object_lifetime ==
                      ssep->curr_scope_object_lifetime);
      (void)pop_object_lifetime();
    }  /* if */
#if DO_IL_LOWERING
    if (!old_region_still_needed && il_lowering_needed()) {
      /* If we're not supposed to pass object lifetime information to the back
         end, unlink all object lifetimes from the IL tree.  This has to
         be done after the file scope object lifetime has been popped. */
      clean_up_all_object_lifetimes(il_scope);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
  /* The IL scope, if any, is no longer on the stack.  This must occur
     after IL lowering and before done_with_memory_region. */
  if (il_scope != NULL) {
    il_scope->depth_in_scope_stack = NO_SCOPE_DEPTH;
  }  /* if */
  if (!old_region_still_needed) {
    done_with_memory_region(old_memory_region_number);
  }  /* if */
  /* For any entities on the extern_type_fixup_list, restore the type of the
     variable or routine to what it was earlier.  This is used for cases like
       int a[];
       main () {
         extern int a[5];
         ... Type of "a" is now "int [5]".
       }
       ... Type of "a" must be restored to "int []" at the end of "main".
     Note that the entries are just thrown away.  There are expected to be
     very few of them.
  */
  for (etfp = ssep->extern_type_fixup_list; etfp != NULL; etfp = etfp->next) {
    if (etfp->is_routine) {
      etfp->variant.routine->type  = etfp->type;
    } else {
      etfp->variant.variable->type = etfp->type;
    }  /* if */
  }  /* for */
  /* For template instantiation scopes, restore the template parameters
     to their previous state.  Normally this just involves setting the
     parameters to point to the "resting" values assigned when the
     template declaration is scanned.  In the event of a recursive
     instantiation, however, this requires restoring the values from the
     previous instantiation. */
  if (kind == (a_scope_kind)sck_template_instantiation) {
    a_scope_depth                     prev_depth;
    a_template_symbol_supplement_ptr  tssp;

    tssp = template_supplement_for_symbol(ssep->template_sym);
    check_assertion(tssp != NULL);
    prev_depth = NO_SCOPE_DEPTH;
    /* Loop through the scope stack looking for a previous instantiation
       scope that uses the same template parameter list as the one being
       popped.  This is necessary because template parameter lists are
       shared between a class and the member functions defined inside the
       class. */
    for (scope_depth = depth_scope_stack - 1;
         scope_depth >= 0;
         scope_depth--) {
      if (scope_stack[scope_depth].kind ==
          (a_scope_kind)sck_template_instantiation &&
          scope_stack[scope_depth].template_param_list == tssp->parameters) {
          prev_depth = scope_depth;
        break;
      }  /* if */
    }  /* for */
    if (prev_depth == NO_SCOPE_DEPTH) {
      /* Restore the default values of the parameters. */
      restore_default_template_params(tssp->parameters);
    } else {
      /* Restore the parameter values from the previous instantiation. */
      update_template_param_symbols(tssp->parameters,
                                    scope_stack[prev_depth].template_arg_list);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    {
    /* Restore the integrity of the file-scope source sequence list -- it was
       temporarily changed in push_scope to allow the source sequence entries
       for the template instantiation to "float up" to the right spot in the
       list. */
    a_scope_stack_entry_ptr      fs_ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
    a_source_sequence_entry_ptr  saved_insert_point;

    saved_insert_point = ssep->ss_list_instantiation_insert_point;
    fs_ssep->ss_list_instantiation_insert_point = saved_insert_point;
    if (saved_insert_point != NULL) {
      if (fs_ssep->last_source_sequence_entry != NULL) {
        fs_ssep->last_source_sequence_entry->next = saved_insert_point;
        saved_insert_point->prev = fs_ssep->last_source_sequence_entry;
      } else {
        fs_ssep->il_scope->source_sequence_list = saved_insert_point;
      }  /* if */
      fs_ssep->last_source_sequence_entry = ssep->saved_last_ss_entry;
    }  /* if */
    }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  /* Determine the memory region to restore for the outer scope. */
  new_memory_region_number = ssep->prev_il_memory_region;
  /* Pop the stack. */
  if (--depth_scope_stack >= 0) {
    /* The stack is not empty, so do anything necessary to activate the
       new top entry. */
    /* If the new memory region is not the same as the old, activate it. */
    if (new_memory_region_number != old_memory_region_number) {
      switch_il_region(new_memory_region_number);
    }  /* if */
    /* Restore state variables. */
    inside_local_class = scope_stack[depth_scope_stack].inside_local_class;
    depth_innermost_function_scope = scope_stack[depth_scope_stack].
                                            depth_innermost_function_scope;
    innermost_function_scope =
                   (depth_innermost_function_scope != NO_SCOPE_DEPTH) ?
                         scope_stack[depth_innermost_function_scope].il_scope :
                         NULL;
    depth_template_declaration_scope =
             scope_stack[depth_scope_stack].depth_template_declaration_scope;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    depth_innermost_ss_list_scope =
             scope_stack[depth_scope_stack].depth_innermost_ss_list_scope;
    source_sequence_entries_disallowed =
             scope_stack[depth_scope_stack].source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Keep track of the number of current classes and class reactivations.
       (If either count is non-zero name lookup is more involved.) */
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      num_classes_on_scope_stack--;
    }  /* if */
    /* Maintain the depth of the innermost template instantiation scope. */
    depth_innermost_instantiation_scope =
                                 ssep->depth_innermost_instantiation_scope;
    /* Maintain the depth of the innermost stack entry that affects access
       control. */
    depth_of_innermost_scope_that_affects_access_control =
                                  ssep->next_scope_that_affects_access_control;
    curr_deferred_access_scope = ssep->saved_curr_deferred_access_scope;
    expr_stack = ssep->saved_expr_stack;
  }  /* if */
  /* Maintain the current declarative level.  It is the same as 
     depth_scope_stack except when struct/union field scopes are
     active; when they are, it indicates the first non-struct-or-union
     scope.  Be careful, you can have a prototype scope inside a
     struct declaration or vice-versa.  In C++, struct/union/class scopes
     are real scopes, but class reactivation scopes are not "real" scopes. */
  for (decl_scope_level = depth_scope_stack;
       decl_scope_level >= DEPTH_OF_FILE_SCOPE;
       decl_scope_level--) {
    a_scope_kind skind = scope_stack[decl_scope_level].kind;
    if (is_scope_kind_that_affects_declarative_level(skind)) break;
  }  /* for */
  db_exit();
}  /* pop_scope */


void push_class_reactivation_scope(a_type_ptr class_type)
/*
Push one or more scopes that will reactivate the indicated class type.
This is used, for example, when scanning member functions.  This routine
is called only in C++.
*/
{
  a_symbol_ptr class_symbol;
  a_scope_ptr  il_scope;

  /* Get the symbol associated with the class. */
  class_symbol = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
#if CHECKING
  if (class_symbol == NULL) {
    internal_error(
              "push_class_reactivation_scope: class type has NULL assoc_info");
  }  /* if */
#endif /* CHECKING */
  if (class_symbol->class_of_which_a_member != NULL) {
    /* Nested class.  Push the containing class(es) first. */
    push_class_reactivation_scope(class_symbol->class_of_which_a_member);
  }  /* if */
  /* Find the IL scope to get the scope number. */
  il_scope = class_type->variant.class_struct_union.extra_info->assoc_scope;
#if CHECKING
  if (il_scope == NULL) {
    internal_error("push_class_reactivation_scope: NULL assoc_scope");
  }  /* if */
#endif /* CHECKING */
  /* Push an entry for the scope. */
  (void)push_scope((a_scope_kind)sck_class_reactivation, il_scope->number,
                   class_type, (a_routine_ptr)NULL);
}  /* push_class_reactivation_scope */


void pop_class_reactivation_scope(void)
/*
Pop one or more scopes pushed by push_class_reactivation_scope.  This routine
is called only in C++.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_symbol_ptr            class_symbol;

  ssep = &scope_stack[depth_scope_stack];
#if CHECKING
  if (ssep->kind != (a_scope_kind)sck_class_reactivation) {
    internal_error(
                 "pop_class_reactivation_scope: entry not class reactivation");
  }  /* if */
#endif /* CHECKING */
  /* Get the symbol associated with the class. */
  class_symbol = (a_symbol_ptr)(ssep->assoc_type->source_corresp.assoc_info);
#if CHECKING
  if (class_symbol == NULL) {
    internal_error(
               "pop_class_reactivation_scope: assoc type has NULL assoc_info");
  }  /* if */
#endif /* CHECKING */
  /* Pop the reactivation scope. */
  pop_scope();
  if (class_symbol->class_of_which_a_member != NULL) {
    /* Nested class.  Pop the containing class(es) too. */
    pop_class_reactivation_scope();
  }  /* if */
}  /* pop_class_reactivation_scope */


an_extern_type_fixup_ptr alloc_etype_fixup(void)
/*
Allocate an_extern_type_fixup entry and return a pointer to it.  A list
of such entries is used to record variables and routines whose types must
be restored to their outer-scope values at the end of a scope.  The entry
allocated is added to the front of the list of fixup entries for the
current scope (the front of the list is good, because the fixup list
should act like a stack if the same entity has several fixups).
*/
{
  register an_extern_type_fixup_ptr ptr;

  db_enter(5, "alloc_etype_fixup");

  ptr = (an_extern_type_fixup_ptr)alloc_fe(sizeof(an_extern_type_fixup));
#if DEBUG
  num_extern_type_fixups_allocated++;
#endif /* DEBUG */
  ptr->next            = scope_stack[depth_scope_stack].extern_type_fixup_list;
  ptr->type            = NULL;
  ptr->is_routine      = FALSE;
  ptr->variant.variable= NULL;
  scope_stack[depth_scope_stack].extern_type_fixup_list = ptr;

  db_exit();
  return ptr;
}  /* alloc_etype_fixup */


static a_param_id_ptr alloc_param_id(void)
/*
Allocate a parameter id block, set its fields to default values, and
return a pointer to it.  The locator field of the entry is set to
locator_for_curr_id.
*/
{
  register a_param_id_ptr pip;

  db_enter(5, "alloc_param_id");
  if (avail_param_ids != NULL) {
    /* Reuse a previously freed entry. */
    pip = avail_param_ids;
    avail_param_ids = avail_param_ids->next;
  } else {
    /* Allocate a new entry. */
    pip = (a_param_id_ptr)alloc_fe(sizeof(a_param_id));
#if DEBUG
    num_param_ids_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Set the entry's fields to default values. */
  pip->next = NULL;
  pip->symbol = NULL;
  pip->type = NULL;
  pip->type_pos = null_source_position;
  pip->storage_class = (a_storage_class)sc_unspecified;
  pip->implicitly_declared = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pip->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return(pip);
}  /* alloc_param_id */


void free_param_id(a_param_id_ptr *ppip)
/*
Free the parameter id block pointed to by *ppip, set *ppip to NULL.
*/
{
  db_enter(5, "free_param_id");
  (*ppip)->next = avail_param_ids;
  avail_param_ids = *ppip;
  *ppip = NULL;
  db_exit();
}  /* free_param_id */


void free_param_id_list(a_param_id_ptr *pidlist)
/*
Free the list of parameter id blocks pointed to by *pidlist, and set
*pidlist to NULL.
*/
{
  a_param_id_ptr pip;

  db_enter(5, "free_param_id_list");
  while (*pidlist != NULL) {
    pip = *pidlist;
    *pidlist = pip->next;
    free_param_id(&pip);
  }  /* while */
  db_exit();
}  /* free_param_id_list */


a_param_id_ptr param_id_on_list(a_symbol_locator *locator,
                                a_param_id_ptr    param_id_list)
/*
Search the parameter id list given by param_id_list to see if the identifier
given by *locator is on it.  If so, return a pointer to the entry; if not,
return NULL.
*/
{
  register a_param_id_ptr param_id = param_id_list;

  while (param_id != NULL) {
    if (param_id->symbol != NULL &&
        param_id->symbol->header == locator->symbol_header) {
      /* Found a match. */
      break;
    }  /* if */
    /* Keep searching the param id list for this name. */
    param_id = param_id->next;
  }  /* while */
  return(param_id);
}  /* param_id_on_list */


#if !GENERATE_SOURCE_SEQUENCE_LISTS
/* ARGSUSED */ /* <-- param_ssep is only used with source sequence lists. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
void add_to_param_id_list(a_symbol_locator            *locator,
                          a_type_ptr                  type_ptr,
                          a_source_position           *type_pos,
                          a_storage_class             storage_class,
                          a_func_info_block_ptr       func_info,
                          a_source_sequence_entry_ptr param_ssep,
                          a_param_id_ptr              *last_param_id)
/*
Create a new param_id entry and an sk_parameter symbol to go with it,
and add the former to the parameter id list pointed to by func_info;
*last_param_id points to the last entry on it.  type_ptr and
storage_class are the type and storage class for the parameter.
*/
{
  a_param_id_ptr  new_param_id;
  a_symbol_ptr    sym;
  a_boolean       unnamed_param = FALSE;
  a_boolean       is_prototype_param_decl = (type_ptr != NULL);

  /* See if this identifier name already appears on the list.  If so, issue
     an error.  Create a param_id entry if this is a prototype parameter
     list, but not otherwise. */
  if (!is_error_locator(*locator)) {
    if (param_id_on_list(locator, func_info->param_id_list) != NULL) {
      error(ec_dupl_param_name);
      set_to_error_locator(*locator);
    } /* if */
  } else if (is_prototype_param_decl) {
    /* Assume that if an error locator is passed in and this is a prototype
       parameter declaration that we have an unnamed parameter.  We'll need
       a param_id entry to keep track of the type. */
    unnamed_param = TRUE;
  } /* if */
  /* Create a param_id entry and enter it onto the param_id list.  Skip
     this if we have an old-style param id list in which a duplicate was
     encountered. */
  if (is_prototype_param_decl || !is_error_locator(*locator)) {
    new_param_id = alloc_param_id();
    /* Save the type and storage class for the later declaration. */
    if (is_prototype_param_decl) {
      new_param_id->type = type_ptr;
      copy_source_position(*type_pos, new_param_id->type_pos);
      new_param_id->storage_class = storage_class;
    }  /* if */
    /* Create a parameter symbol.  It is used during parameter processing
       only.  The corresponding symbol in the function scope itself is a
       variable symbol for which the variable's is_parameter flag is set to
       TRUE. */
    if (unnamed_param) {
      /* Create no symbol for an unnamed parameter. */
      sym = NULL;
    } else {
      if (type_ptr != NULL) {
        /* Prototyped parameter list.  The symbol is entered in the function
           prototype scope.  It will later be copied to the function scope
           when it is changed to sk_variable. */
        sym = enter_symbol((a_symbol_kind)sk_parameter, locator,
                           depth_scope_stack, /*suppress_redecl_error=*/FALSE);
      } else {
        /* Must be an old-style parameter declaration.  The type and storage
           class will be supplied later.  We won't actually enter this symbol
           until the function scope is pushed. */
        sym = make_parameter_symbol(locator);
      }  /* if */
      new_param_id->symbol = sym;
      sym->variant.param_id = new_param_id;
      set_decl_sequence_number(sym);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    new_param_id->source_sequence_entry = param_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Put this entry on the end of the list of param ids. */
    if (func_info->param_id_list == NULL) {
      func_info->param_id_list = new_param_id;
    } else {
      (*last_param_id)->next = new_param_id;
    }  /* if */
    (*last_param_id) = new_param_id;
  }  /* if */
}  /* add_to_param_id_list */


void clear_func_info(a_func_info_block *func_info)
/*
Clear the fields of a function information block to default values.
*/
{
  func_info->prototype_scope_symbols     = NULL;
  func_info->param_id_list               = NULL;
  func_info->exception_specification     = NULL;
  func_info->throw_position              = null_source_position;
  func_info->scope_number                = NO_SCOPE_NUMBER;
  func_info->any_prototype_names_omitted = FALSE;
  func_info->is_inline                   = FALSE;
  func_info->is_definition               = FALSE;
  func_info->is_main_function            = FALSE;
  func_info->is_implicit_declaration     = FALSE;
  func_info->function_type_from_typedef  = FALSE;
  func_info->any_default_args            = FALSE;
#if ASM_FUNCTION_ALLOWED
  func_info->is_asm_function             = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  func_info->declarator_ssep                = NULL;
  func_info->prototype_scope_ss_entry_start = NULL;
  func_info->prototype_scope_ss_entry_end   = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* clear_func_info */


void add_to_dependent_type_fixup_list(a_type_ptr                   class_type,
                                      a_dependent_type_fixup_kind  fixup_kind,
                                      char                         *ptr,
                                      a_byte_il_entry_kind         entity_kind,
                                      a_source_position            *pos)
/*
class_type is a pointer an incomplete class/struct/union type, and type or
param_type is dependent on it, so an entry is created so that type or
param_type can be fixed up appropriately when the class type is finally
defined.  Either type or param_type (but not both) is non-NULL.
*/
{
  a_dependent_type_fixup_ptr     dtfp;
  a_class_symbol_supplement_ptr  cssp;

  db_enter(5, "add_to_dependent_type_fixup_list");
  /* A dependent type fixup entry is required for the type or param type. */
  if (avail_dependent_type_fixups != NULL) {
    /* Reuse a previously freed entry. */
    dtfp = avail_dependent_type_fixups;
    avail_dependent_type_fixups = avail_dependent_type_fixups->next;
  } else {
    /* Allocate a new entry. */
    dtfp = (a_dependent_type_fixup_ptr)alloc_fe(
                                            sizeof(a_dependent_type_fixup));
#if DEBUG
    num_dependent_type_fixups_allocated++;
#endif /* DEBUG */
  }  /* if */
  dtfp->fixup_kind = fixup_kind;
  dtfp->entity.ptr = ptr;
  dtfp->entity.kind = entity_kind;
  dtfp->decl_position = *pos;
  dtfp->next = NULL;
  /* Add a fixup entry to the end of the list associated with the class. */
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->dependent_type_fixup_list == NULL) {
    cssp->dependent_type_fixup_list = dtfp;
  } else {
    a_dependent_type_fixup_ptr  end_of_list = cssp->dependent_type_fixup_list;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = dtfp;
  }  /* if */
  db_exit();
}  /* add_to_dependent_type_fixup_list */


void check_dependent_type_fixup_list(a_type_ptr  class_type)
/*
Go through the entries on the dependent-type-fixup list for class_type.  The
list is a registry of entities that depend on class_type but were declared
before class_type was defined.  Now that the definition is there, the rest
of the declaration can be completed for the dependent types, too.
*/
{
  a_dependent_type_fixup_ptr     dtfp, next_dtfp, prev_dtfp, list;
  a_class_symbol_supplement_ptr  cssp;
  a_type_ptr                     tp;

  cssp = symbol_supplement_for_class(class_type);
  list = cssp->dependent_type_fixup_list;
  if (list != NULL) {
#if CHECKING
    a_boolean  any_entries_removed = FALSE;
#endif /* CHECKING */
    /* Keep going through the list until all its entries have been removed.
       Usually only one traversal is required, but multidimensional arrays
       may require extra trips. */
    do {
      /* Traverse the list. */
      prev_dtfp = NULL;
      for (dtfp = list; dtfp != NULL; dtfp = next_dtfp) {
        next_dtfp = dtfp->next;
        switch (dtfp->fixup_kind) {
          case dtfk_arg_transfer_method:
            check_assertion(dtfp->entity.kind ==
                                    (a_byte_il_entry_kind)iek_param_type);
            /* A parameter of class type.  Set the flag indicating whether
               passing it requires a copy constructor call. */
            set_arg_transfer_method_flag((a_param_type_ptr)dtfp->entity.ptr,
                                           &dtfp->decl_position);
            break;
          case dtfk_array_type_size:
            check_assertion(dtfp->entity.kind ==
                                    (a_byte_il_entry_kind)iek_type);
            tp = (a_type_ptr)dtfp->entity.ptr;
            if (!is_error_type(tp)) {
              check_assertion(is_array_type(tp));
              if (is_incomplete_type(tp->variant.array.element_type)) {
                /* The array is still incomplete.  This can happen if it is
                   dependent on another array that is still to be checked (the
                   case of "array of array of T").  Leave dtfp on the list and
                   continue. */
                prev_dtfp = dtfp;
                goto next_list_entry;
              } else {
                /* An array of elements of the (now complete) class type.  The
                   array's size can be computed. */
                error_position = dtfp->decl_position;
                set_type_size(tp);
              }  /* if */
            }  /* if */
            break;
          case dtfk_routine_calling_method:
            check_assertion(dtfp->entity.kind ==
                                    (a_byte_il_entry_kind)iek_type);
            tp = (a_type_ptr)dtfp->entity.ptr;
            if (!is_error_type(tp)) {
              check_assertion(is_function_type(tp));
              if (dtfp->fixup_kind ==
                   (a_dependent_type_fixup_kind)dtfk_routine_calling_method) {
                /* A function returning a class type.  Set the flag indicating
                   whether the return involves a copy constructor. */
                set_routine_calling_method_flag(tp, &dtfp->decl_position);
              }  /* if */
            }  /* if */
            break;
          case dtfk_check_op_arrow_return_type:
            check_assertion(dtfp->entity.kind ==
                                    (a_byte_il_entry_kind)iek_routine);
            check_operator_arrow_return_type((a_routine_ptr)dtfp->entity.ptr,
                                             /*is_expr_use=*/FALSE,
                                             &dtfp->decl_position);
            break;
#if CHECKING
          default:
            internal_error("check_dependent_type_fixup_list: bad fixup kind");
#endif /* CHECKING */
        }  /* switch */
        /* If the head of the list is being removed (the common case) reset the
           list pointer. */
        check_assertion((list == dtfp) == (prev_dtfp == NULL));
        if (list == dtfp) {
          list = next_dtfp;
        } else {
          prev_dtfp->next = next_dtfp;
        }  /* if */
        /* Remove dtfp from its list and add it to the available list. */
        dtfp->next = avail_dependent_type_fixups;
        avail_dependent_type_fixups = dtfp;
#if CHECKING
        any_entries_removed = TRUE;
#endif /* CHECKING */
next_list_entry:;
      }  /* for */
      /* If the inner loop is completed without eliminating all the entries on
         the list, go though it again. */
#if CHECKING
      if (!any_entries_removed) {
        /* No entries were removed on the last traversal of the list. */
        internal_error("check_dependent_type_fixup_list: looping error");
      }  /* if */
      /* Reset the flag for the next trip through the list. */
      any_entries_removed = FALSE;
#endif /* CHECKING */
    } while (list != NULL);
    /* Null out the list pointer before returning. */
    cssp->dependent_type_fixup_list = NULL;
  }  /* if */
}  /* check_dependent_type_fixup_list */


a_template_param_ptr alloc_template_param(a_symbol_ptr sym)
/*
Allocate a new template parameter list entry, initialize it,
and return a pointer to it.
*/
{
  register a_template_param_ptr ptr;

  db_enter(5, "alloc_template_param");
  ptr = (a_template_param_ptr)alloc_fe(sizeof(a_template_param));
#if DEBUG
  num_template_params_allocated++;
#endif /* DEBUG */
  check_assertion(sym != NULL);
  ptr->next           = NULL;
  ptr->param_symbol   = sym;
  clear_token_cache(&ptr->token_cache, /*reusable=*/TRUE);
  if (sym->kind == (a_symbol_kind)sk_type) {
    ptr->variant.param_type     = sym->variant.type;
  } else {
    check_assertion(sym->kind == (a_symbol_kind)sk_constant);
    ptr->variant.param_constant.ptr = sym->variant.constant;
    ptr->variant.param_constant.has_default_arg = FALSE;
    ptr->variant.param_constant.type_involves_template_param = FALSE;
    ptr->variant.param_constant.constant_involves_template_param = FALSE;
    ptr->variant.param_constant.default_arg.constant = NULL;
#if CHECKING
    ptr->variant.param_constant.dummy = 0;
#endif /* CHECKING */
  }  /* if */
  db_exit();
  return ptr;
}  /* alloc_template_param */


a_template_instance_ptr alloc_template_instance(void)
/*
Allocate a new function instantiation entry and return a pointer to it.
*/
{
  register a_template_instance_ptr  tip;

  db_enter(5, "alloc_template_instance");
  tip = (a_template_instance_ptr)alloc_fe(sizeof(a_template_instance));
#if DEBUG
  num_template_instances_allocated++;
#endif /* DEBUG */
  tip->next                        = NULL;
  tip->next_in_instantiation_list  = NULL;
  tip->instance_sym                = NULL;
  tip->template_sym                = NULL;
  tip->arg_list                    = NULL;	
  tip->template_info               = NULL;
  tip->instantiation_required      = FALSE;
  tip->specific_decl               = FALSE;
  tip->specific_def                = FALSE;
  tip->explicit_instantiation      = FALSE;
  tip->already_instantiated        = FALSE;
  tip->explicit_do_not_instantiate = FALSE;
  tip->explicit_can_instantiate    = FALSE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  tip->in_info_file                = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  tip->explicit_instantiation_pos  = null_source_position;
  db_exit();
  return tip;
}  /* alloc_template_instance */


#if DEBUG
unsigned long show_symbol_space_used(void)
/*
Display and return the amount of memory used for symbol-related entries,
for space tracking purposes.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Symbol table use:");

  db_space_used("symbol", num_symbols_allocated, a_symbol);
  db_space_used("symbol header", num_symbol_headers_allocated,
                a_symbol_header);
  db_space_used_general("scope stack", size_scope_stack, a_scope_stack_entry);
  db_space_used("conversion header", num_conversion_headers_allocated,
                a_conversion_header);
  db_space_used("Name strings", symbol_name_string_space, char);
  db_space_used("extern symbol descr", num_extern_symbol_descrs_allocated,
                an_extern_symbol_descr);
  db_space_used("extern type fixup", num_extern_type_fixups_allocated,
                an_extern_type_fixup);
  db_space_used("class symbol supplement",
                num_class_symbol_supplements_allocated,
                a_class_symbol_supplement);
  db_space_used("template symbol suppl.",
                num_template_symbol_supplements_allocated,
                a_template_symbol_supplement);
  db_space_used("template param", num_template_params_allocated,
                a_template_param);
  db_space_used_lost("param ids", avail_param_ids, num_param_ids_allocated,
                     a_param_id);
  db_space_used_lost("dependent type fixups", avail_dependent_type_fixups,
                     num_dependent_type_fixups_allocated,
                     a_dependent_type_fixup);
  db_space_used("template instance", num_template_instances_allocated,
                a_template_instance);
  db_space_used("symbol list entry", num_symbol_list_entries_allocated,
                a_symbol_list_entry);
  db_space_used("projection symbol descr", num_projection_descrs_allocated,
                a_projection_descr);
  db_space_used_lost("access error descr", avail_access_error_descrs,
                     num_access_error_descrs_allocated, an_access_error_descr);
  grand_total = db_show_routine_fixups_used(grand_total);
  grand_total = db_show_def_arg_expr_fixups_used(grand_total);

  db_space_used_total();

  /* Print some symbol table performance statistics. */
  (void)fputc('\n', f_debug);
  db_space_used_other
                   ("Percent of buckets used",
                    (100 * num_used_symbol_buckets) / SYMBOL_TABLE_SIZE, "");

  if (num_used_symbol_buckets != 0) {
    db_space_used_float_other("Avg non-empty bucket len",
                             (double)num_symbol_headers_in_hash_table /
                             (double)num_used_symbol_buckets, "");
  }  /* if */
  db_space_used_other("Number of searches", num_searches_for_symbols, "");
  if (num_searches_for_symbols != 0) {
    db_space_used_float_other("Avg compares/search",
                             (double)num_compares_for_symbols /
                             (double)num_searches_for_symbols, "");
  }  /* if */
  db_space_used_other("Number of fast id lookups", num_fast_id_lookups, "");
  db_space_used_other("Number of slow id lookups", num_slow_id_lookups, "");

  return grand_total;
}  /* show_symbol_space_used */
#endif /* DEBUG */


void symbol_tbl_one_time_init(void)
/*
Do one-time initialization of variables related to the symbol table.
(Variables that need to be reinitialized with each new translation unit
are handled in symbol_tbl_init.)
*/
{
  a_name_space_kind tag_name_space;
  a_name_space_kind member_name_space;

  /* Variables in symbol_tbl.h: */
  /* Build the table that maps symbol kinds to the corresponding name
     space.  See standard, 3.1.2.3. */
  name_space_for_symbol_kind[(int)sk_keyword]             = nsk_keyword;
  name_space_for_symbol_kind[(int)sk_macro]               = nsk_macro;
  name_space_for_symbol_kind[(int)sk_constant]            = nsk_other;
  name_space_for_symbol_kind[(int)sk_type]                = nsk_other;
  /* In C++, tags (class/struct/union) are in (almost) the same name space as
     normal symbols.  In C, they are in a separate name space. */
  tag_name_space = (C_dialect == C_dialect_cplusplus) ? nsk_other : nsk_tag;
  name_space_for_symbol_kind[(int)sk_class_or_struct_tag] = tag_name_space;
  name_space_for_symbol_kind[(int)sk_union_tag]           = tag_name_space;
  name_space_for_symbol_kind[(int)sk_enum_tag]            = tag_name_space;
  name_space_for_symbol_kind[(int)sk_variable]            = nsk_other;
  /* Note that in C++ mode the class members are nsk_other rather than some
     other kind, which works because the members are on the inactive list
     once the class definition is ended.  Therefore, they won't be
     found inadvertently.  In C mode they are in a separate nsk_member
     name space to prevent them from being found while they are still on
     the active list. */
  member_name_space = C_mode() ? nsk_member : nsk_other;
  name_space_for_symbol_kind[(int)sk_field]               = member_name_space;
  name_space_for_symbol_kind[(int)sk_static_data_member]  = member_name_space;
  name_space_for_symbol_kind[(int)sk_member_function]     = member_name_space;
  name_space_for_symbol_kind[(int)sk_routine]             = nsk_other;
  name_space_for_symbol_kind[(int)sk_label]               = nsk_label;
  name_space_for_symbol_kind[(int)sk_undefined]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_parameter]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_extern_variable]     = nsk_extern;
  name_space_for_symbol_kind[(int)sk_extern_routine]      = nsk_extern;
  name_space_for_symbol_kind[(int)sk_projection]          = nsk_other;
  name_space_for_symbol_kind[(int)sk_overloaded_function] = nsk_other;
  name_space_for_symbol_kind[(int)sk_class_template]      = nsk_other;
  name_space_for_symbol_kind[(int)sk_function_template]   = nsk_other;
#if CHECKING
  /* "undefined" and "routine" must be in the same name space.  See
      decl_default_function. */
  if (name_space_for_symbol_kind[(int)sk_undefined] !=
      name_space_for_symbol_kind[(int)sk_routine]) {
    internal_error(
  "symbol_table_one_time_init: different name spaces for undefined & routine");
  }  /* if */
  /* Check that the table of symbol kind names is correctly initialized.
     This guards against someone changing the enumeration and forgetting to
     update symbol_kind_names. */
  if (symbol_kind_names[(int)sk_last] == NULL ||
      strcmp(symbol_kind_names[(int)sk_last], "last") != 0) {
    internal_error
              ("sym_tbl_init: incorrect initialization of symbol_kind_names");
  }  /* if */
#endif /* CHECKING */
  /* Clear a locator that can be used to make initialization more efficient. */
  cleared_locator.symbol_header                   = NULL;
  cleared_locator.source_position                 = null_source_position;
  cleared_locator.is_qualified_name               = FALSE;
  cleared_locator.is_global_qualified_name        = FALSE;
  cleared_locator.is_file_scope_qualified_name    = FALSE;
  cleared_locator.is_operator_name                = FALSE;
  cleared_locator.is_conversion_name              = FALSE;
  cleared_locator.is_destructor_name              = FALSE;
  cleared_locator.is_semivisible_nested_type      = FALSE;
  cleared_locator.access_control_error_reported   = FALSE;
  cleared_locator.has_been_coalesced              = FALSE;
  cleared_locator.is_vacuous_destructor_reference = FALSE;
  cleared_locator.is_nonclass_destructor          = FALSE;
  cleared_locator.is_error                        = FALSE;
  cleared_locator.do_not_clear_specific_symbol    = FALSE;
  cleared_locator.is_template_id                  = FALSE;
  cleared_locator.specific_symbol                 = NULL;
  cleared_locator.qualifier_class_type            = NULL;
  cleared_locator.variant.conversion_result_type  = NULL;

  /* Static variables in symbol_tbl.c: */
  /* Clear a symbol that can be used to make initialization more efficient. */
  cleared_symbol.header                         = NULL;
  cleared_symbol.next                           = NULL;
  cleared_symbol.next_in_scope                  = NULL;
  cleared_symbol.decl_scope                     = NO_SCOPE_NUMBER;
  cleared_symbol.decl_seq                       = 0;
  cleared_symbol.decl_position                  = null_source_position;
  cleared_symbol.class_of_which_a_member        = NULL;
  cleared_symbol.referenced                     = FALSE;
  cleared_symbol.defined                        = FALSE;
  cleared_symbol.explicit_linkage_specifier     = FALSE;
  cleared_symbol.reentered_from_prototype_scope = FALSE;
  cleared_symbol.is_error                       = FALSE;
  cleared_symbol.is_template_param              = FALSE;
  cleared_symbol.template_param_not_visible     = FALSE;
  cleared_symbol.force_external_linkage         = FALSE;
  /* Save variables from symbol_tbl.h and symbol_tbl.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(conversion_header_list),
      pch_saved_var_array_elem(decl_seq_counter),
      pch_array_saved_var_array_elem(opname_symbol_table),
      /* In effect, only the first element of the scope stack entry is
         copied. */
      pch_indirect_saved_var_array_elem(scope_stack,
                                        sizeof(a_scope_stack_entry)),
      pch_saved_var_array_elem(next_scope_number),
      pch_array_saved_var_array_elem(symbol_table),
      pch_saved_var_array_elem(anonymous_parent_object_symbol_header),
      pch_saved_var_array_elem(avail_access_error_descrs),
      pch_saved_var_array_elem(avail_dependent_type_fixups),
      pch_saved_var_array_elem(avail_param_ids),
      pch_saved_var_array_elem(error_symbol_header),
      pch_saved_var_array_elem(unnamed_class_symbol_header),
      pch_saved_var_array_elem(unnamed_field_symbol_header),
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
      pch_saved_var_array_elem(last_ctor_or_dtor_sym),
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
#if DEBUG
      pch_saved_var_array_elem(db_symbol_buffer_pointer),
      pch_saved_var_array_elem(num_access_error_descrs_allocated),
      pch_saved_var_array_elem(num_class_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_classes_on_scope_stack),
      pch_saved_var_array_elem(num_compares_for_symbols),
      pch_saved_var_array_elem(num_conversion_headers_allocated),
      pch_saved_var_array_elem(num_dependent_type_fixups_allocated),
      pch_saved_var_array_elem(num_extern_symbol_descrs_allocated),
      pch_saved_var_array_elem(num_extern_type_fixups_allocated),
      pch_saved_var_array_elem(num_fast_id_lookups),
      pch_saved_var_array_elem(num_param_ids_allocated),
      pch_saved_var_array_elem(num_projection_descrs_allocated),
      pch_saved_var_array_elem(num_searches_for_symbols),
      pch_saved_var_array_elem(num_slow_id_lookups),
      pch_saved_var_array_elem(num_symbol_headers_allocated),
      pch_saved_var_array_elem(num_symbol_headers_in_hash_table),
      pch_saved_var_array_elem(num_symbol_list_entries_allocated),
      pch_saved_var_array_elem(num_symbols_allocated),
      pch_saved_var_array_elem(num_template_instances_allocated),
      pch_saved_var_array_elem(num_template_params_allocated),
      pch_saved_var_array_elem(num_template_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_used_symbol_buckets),
      pch_saved_var_array_elem(symbol_name_string_space),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* symbol_tbl_one_time_init */

void symbol_tbl_init(void)
/*
Initialize static variables related to the symbol table.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in symbol_tbl.h: */
  /* Clear the symbol table.  Note that this assumes that NULL is a zero
     bit pattern. */
  memzero((char *)symbol_table, sizeof(symbol_table));
  /* Clear the operator name symbol table.  Note that this assumes that
     NULL is a zero bit pattern. */
  memzero((char *)opname_symbol_table, sizeof(opname_symbol_table));
  /* scope_stack is not per-file and should not be reset. */
  depth_scope_stack = NO_SCOPE_DEPTH;
  decl_scope_level = NO_SCOPE_DEPTH;
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
  innermost_function_scope = NULL;
  depth_innermost_instantiation_scope = NO_SCOPE_DEPTH;
  depth_template_declaration_scope = NO_SCOPE_DEPTH;
  curr_deferred_access_scope = NO_SCOPE_DEPTH;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  depth_innermost_ss_list_scope = NO_SCOPE_DEPTH;
  source_sequence_entries_disallowed = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  inside_local_class = FALSE;
  next_scope_number = FILE_SCOPE_NUMBER;

  /* size_scope_stack is not per-file and should not be reset. */
  /* ident_buffer and size_ident_buffer are not per-file and should not
     be reset. */
  avail_param_ids = NULL;
  avail_dependent_type_fixups = NULL;
  avail_access_error_descrs = NULL;
  error_symbol_header = NULL;
  unnamed_class_symbol_header = NULL;
  anonymous_parent_object_symbol_header = NULL;
  unnamed_field_symbol_header = NULL;
  num_classes_on_scope_stack = 0;
  depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
  /* Initialize the conversion header list. */
  conversion_header_list = NULL;
  /* Global variable declared in symbol_ref.c. */
  decl_seq_counter = 0;
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  last_ctor_or_dtor_sym = NULL;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
#if DEBUG
  num_symbols_allocated                        = 0;
  num_symbol_headers_allocated                 = 0;
  num_symbol_headers_in_hash_table             = 0;
  num_conversion_headers_allocated             = 0;
  symbol_name_string_space                     = 0;
  num_class_symbol_supplements_allocated       = 0;
  num_template_symbol_supplements_allocated    = 0;
  num_template_params_allocated                = 0;
  num_param_ids_allocated                      = 0;
  num_dependent_type_fixups_allocated          = 0;
  num_template_instances_allocated             = 0;
  num_symbol_list_entries_allocated            = 0;
  num_extern_symbol_descrs_allocated           = 0;
  num_extern_type_fixups_allocated             = 0;
  num_projection_descrs_allocated              = 0;
  num_used_symbol_buckets                      = 0;
  num_searches_for_symbols                     = 0;
  num_compares_for_symbols                     = 0;
  num_fast_id_lookups                          = 0;
  num_slow_id_lookups                          = 0;
  num_access_error_descrs_allocated            = 0;
#endif /* DEBUG */
}  /* symbol_tbl_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
