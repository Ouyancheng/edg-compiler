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
#ifdef GUARD_MACRO_FOR_VA_LIST
/* macro.h is needed for enter_predef_macro. */
#include "macro.h"
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */


/* The multiplier used in the hash algorithm that generates an index
   in the hash table from an identifier name string.  Do not change
   without investigating the hash table performance that results.
   Prime values are likely to work better than non-prime values. */
#define HASH_FACTOR ((unsigned int)73)

/*
Dummy symbol headers used for compiler-generated error symbols and for
unnamed class symbols.
*/
static a_symbol_header_ptr
		error_symbol_header,
		unnamed_tag_symbol_header,
		anonymous_parent_object_symbol_header,
		unnamed_field_symbol_header,
		unnamed_namespace_symbol_header;

static a_symbol_ptr
		symbols_with_no_scope_tail;
			/* End of the symbols_with_no_scope list. */

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
		num_namespace_symbol_supplements_allocated,
		num_template_params_allocated,
		num_param_ids_allocated,
		num_dependent_type_fixups_allocated,
		num_template_instances_allocated,
		num_master_instances_allocated,
		num_symbol_list_entries_allocated,
		num_type_list_entries_allocated,
		num_substituted_type_list_entries_allocated,
		num_template_cache_segments_allocated,
		num_template_decl_info_allocated,
		num_nondependent_call_info_allocated,
		num_templ_friend_info_allocated,
		num_namespace_list_entries_allocated,
		num_extern_symbol_descrs_allocated,
		num_vla_fixups_allocated,
		num_extern_type_fixups_allocated,
		num_projection_descrs_allocated,
		num_used_symbol_buckets,
		num_searches_for_symbols,
		num_compares_for_symbols,
		num_access_error_descrs_allocated,
		num_progenitors_allocated,
		num_exception_spec_error_descrs_allocated;
#endif /* DEBUG */

static a_namespace_list_entry_ptr
		global_namespace_list_entry;
			/* Pointer to a namespace list entry for the
			   global scope.  This contains a NULL namespace
			   pointer. */
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

static a_symbol_list_entry_ptr
		avail_symbol_list_entries;
			/* List of symbol list entries freed and available for
			   reuse. */

static a_type_list_entry_ptr
		avail_type_list_entries;
			/* List of type list entries freed and available for
			   reuse. */

static a_namespace_list_entry_ptr
		avail_namespace_list_entries;
			/* List of namespace list entries freed and available
			   for reuse. */

static a_substituted_type_list_entry_ptr
		avail_substituted_type_list_entries;
			/* List of substituted type list entries freed and
			   available for reuse. */

static a_template_cache_segment_ptr
		avail_template_cache_segments;
			/* List of template cache segments freed and available
			   for reuse. */

static a_vla_fixup_ptr
		avail_vla_fixups;
			/* List of vla fixup entries freed and available for
			   reuse. */

static a_symbol_ptr
		error_class_template_symbol;
			/* Pointer to a shared error class template entry. */

static sizeof_t	size_of_trans_unit_for_scope;
			/* Allocated size of the trans_unit_for_scope table. */

#define TRANS_UNIT_FOR_SCOPE_INCREMENTAL_ALLOCATION 16384
			/* Incremental allocation for the trans_unit_for_scope
			   table. */

void form_optionally_qualified_symbol_name(
		a_symbol_ptr				sym,
		an_il_to_str_output_control_block_ptr	octl,
		a_boolean				suppress_qualifier)
/*
Output the (possibly qualified) name of the indicated symbol.  The output
is done according to the output control block octl.  If suppress_qualifier
is TRUE, output only the final portion of the name, not any qualifier
that might normally precede it.
*/
{
  char                    *entry;
  an_il_entry_kind        kind;
  a_source_correspondence *scp;

  /* See whether the symbol has an associated IL entry and whether the IL
     entry has a source correspondence field -- but use it only if its
     parent matches that of the symbol (the parent class can differ for
     symbols promoted from anonymous unions, in which case preference is
     given to the symbol; the parent namespace can differ for extern-C
     declarations). */
  entry = il_entry_for_symbol_null_okay(sym, &kind);
  if (entry != NULL &&
      (scp = source_corresp_for_il_entry(entry, kind)) != NULL &&
      sym->is_class_member == scp->is_class_member &&
      (sym->is_class_member ?
         sym->parent.class_type == scp->parent.class_type :
         sym->parent.namespace_ptr == scp->parent.namespace_ptr)) {
    /* Use the IL entry to generate the name. */
    if (suppress_qualifier) {
      form_unqualified_name(scp, kind, octl);
    } else {
      form_name(scp, kind, octl);
    }  /* if */
  } else {
    /* No source correspondence entry, or else it has a different class
       parent; use the symbol name directly. */
    if (il_header.source_language == sl_Cplusplus && !suppress_qualifier) {
      /* Put out the class or namespace qualifier on a member. */
      form_class_or_namespace_qualifier((a_boolean)sym->is_class_member,
                                        sym->parent, octl);
    }  /* if */
    octl->output_str(sym->header->identifier);
  }  /* if */
}  /* form_optionally_qualified_symbol_name */


void form_symbol_name(a_symbol_ptr                          sym,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output the (possibly qualified) name of the indicated symbol.  The output
is done according to the output control block octl.
*/
{
  form_optionally_qualified_symbol_name(sym, octl,
                                        /*suppress_qualifier=*/FALSE);
}  /* form_symbol_name */


#if DEBUG
#define DEBUG_LINE_LENGTH 79
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
{ char *local_str = (str);					\
  put_separator(",", strlen(local_str));			\
  fputs((local_str), f_debug);					\
  col += strlen((local_str));					\
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
  octl.debug_output = TRUE;
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
    default:              s = "<bad access>"; break;
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


static char *str_function_name_and_param_list(char          buffer[],
                                              a_symbol_ptr  sym)
/*
Construct a string the buffer that represents a qualified name plus
function param list -- called from db_symbol.
*/
{
  a_type_ptr  tp;

  set_up_for_output_to_buffer(buffer);
  form_symbol_name(sym, &octl);
  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    tp = sym->variant.routine.ptr->type;
    tp = skip_typerefs(tp);
    form_function_declarator(tp, &octl);
  }  /* if */
  return buffer;
}  /* str_function_name_and_param_list */


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
  char *str = name_linkage_kind_names[(int)source_corresp->name_linkage];
  (void)sprintf(buffer, "%s linkage", str);
  return buffer;
}  /* str_name_linkage */


void db_symbol_name(a_symbol_ptr  sym)
/*
Write out the name (including function parameters if there are any) of the
specified symbol.
*/
{
  char  *str, buffer[1000];

  str = str_qualified_name(buffer, sym);
  fprintf(f_debug, "\"%s", str);
  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    a_type_ptr  tp = routine_symbol_type(sym);
    if (tp != NULL) db_function_param_list(tp);
  }  /* if */
  fprintf(f_debug, "\"");
}  /* db_symbol_name */


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

  if (sym == NULL) {
    /* The symbol passed by the caller is NULL. */
    fprintf(f_debug, "<NULL>");
    goto done;
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
  } else if (sym->kind == (a_symbol_kind)sk_namespace_projection) {
    a_symbol_ptr fsym = sym->variant.namespace_projection.fundamental_symbol;
    if (fsym != NULL) {
      str = str_qualified_name(buffer, fsym);
      put_separator("", strlen(str) + 6);
      fprintf(f_debug, "(= \"%s\")", str);
      col += strlen(str) + 6;
    }  /* if */
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

  /* If this symbol is for a secondary translation unit, display the
     translation unit. */
  if (sym->decl_scope != NO_SCOPE_NUMBER) {
     a_translation_unit_ptr	tup;
     tup = trans_unit_for_scope[sym->decl_scope];
     if (tup != NULL && tup != translation_units) {
       (void)sprintf(buffer, "trans unit %s",
                     tup->source_file->name_as_written);
       put_string(buffer);
     }  /* if */
  }  /* if */

  /* Display the file name (if not the primary source file) and the line
     number of the symbol declaration. */
  {
    char	  *file_name;
    char	  *full_name;
    a_line_number line_number;
    a_boolean	  at_end_of_source;
    conv_seq_to_file_and_line(sym->decl_position.seq, &file_name, &full_name,
                              &line_number, &at_end_of_source);
    if (seq_is_in_include_file(sym->decl_position.seq)) {
      (void)sprintf(buffer, "file %s", file_name);
      put_string(buffer);
    }  /* if */
    if (at_end_of_source) {
      (void)sprintf(buffer, "line <end of source>");
    } else {
      (void)sprintf(buffer, "line %lu", (unsigned long)line_number);
    }  /* if */
    put_string(buffer);
  }

  (void)sprintf(buffer, "scope %ld", sym->decl_scope);
  put_string(buffer);

  if (sym->referenced) put_string("ref'd");
  if (sym->defined) put_string("def'd");
  if (sym->ambiguous) put_string("ambig");
  if (sym->synthesized_namespace_projection) {
    put_string("synth_namespace_proj");
  }  /* if */
  if (sym->is_invisible) put_string("invisible");
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
      if (sym->variant.type.is_injected_class_name) {
        put_string("injected class name");
      }  /* if */
      type = sym->variant.type.ptr;
      break;
    case sk_enum_tag:
      type = sym->variant.enumeration.type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      type = sym->variant.class_struct_union.type;
      if (type == NULL) break;
      /* The result of skip_typerefs() is copied to a temporary variable to
         work around a problem with Borland C++. */
      temp_type = skip_typerefs(type);
#if MAINTAIN_NEEDED_FLAGS
      if (temp_type->variant.class_struct_union.definition_needed) {
        put_string("def needed");
      } else if (temp_type->source_corresp.needed) {
        put_string("needed");
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
      (void)str_name_linkage(buffer, &(temp_type->source_corresp));
      put_string(buffer);
      {
        a_class_symbol_supplement_ptr  cssp;
        cssp = sym->variant.class_struct_union.extra_info;
        if (cssp->is_POD) {
          put_string("POD");
        } else if (cssp->is_class_aggregate) {
          put_string("aggregate");
        }  /* if */
        if (cssp->constructor != NULL) put_string("has ctor");
        if (cssp->trivial_default_constructor != NULL) {
          put_string("has trivial default-ctor");
        }  /* if */
        if (cssp->has_nontrivial_default_constructor) {
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
        if (cssp->target_of_conversion_function) {
          put_string("conv target");
        }  /* if */
        if (cssp->class_template != NULL) {
          if (debug_level >= 4) put_string("has class template ptr");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_template_class) {
          put_string("is instance");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_nonreal_class) {
          put_string("nonreal");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_prototype_instantiation) {
          put_string("prototype instantiation");
        }  /* if */
        if (cssp->any_nonreal_base_classes) {
          put_string("has nonreal base class");
        }  /* if */
        if (cssp->any_template_dependent_fields) {
          put_string("has dependent field");
        }  /* if */
        if (cssp->template_param_for_proxy_class != NULL) {
          if (debug_level >= 4) put_string("has ptr for proxy");
        }  /* if */
        if (temp_type->variant.class_struct_union.
                                          contains_flexible_array_member) {
          put_string("contains flexible array member");
        }  /* if */
        if (temp_type->variant.class_struct_union.any_const_member) {
          put_string("has const member");
        }  /* if */
        if (temp_type->variant.class_struct_union.any_mutable_member) {
          put_string("has mutable member");
        }  /* if */
        if (temp_type->declared_in_function_prototype) {
          put_string("in func prototype");
        }  /* if */
        if (temp_type->variant.class_struct_union.is_specialized) {
          (void)sprintf(buffer, "%sspecialization",
                        temp_type->variant.class_struct_union.
                             specialized_with_old_syntax ? "old-style " : "");
          put_string(buffer);
        }  /* if */
        if (cssp->friend_functions != NULL) {
          a_symbol_ptr  friend_sym, overload_sym, fund_sym;
          char          *sep;

          friend_sym = cssp->friend_functions;
          put_string("invisible friends =");
          (void)sprintf(buffer, "[ ");
          sep = "";
          friend_sym = cssp->friend_functions;
          overload_sym = NULL;
          do {
            if (friend_sym->kind == (a_symbol_kind)sk_overloaded_function) {
              overload_sym = friend_sym;
              friend_sym = overload_sym->variant.overloaded_function.symbols;
            }  /* if */
            fund_sym = fundamental_symbol_of(friend_sym);
            if (fund_sym->overload_set_member || overload_sym != NULL) {
              (void)str_function_name_and_param_list(&buffer[strlen(buffer)],
                                                     fund_sym);
            } else {
              (void)str_qualified_name(&buffer[strlen(buffer)], fund_sym);
            }  /* if */
            friend_sym = friend_sym->next;
            if (overload_sym != NULL && friend_sym == NULL) {
              friend_sym = overload_sym->next;
              overload_sym = NULL;
            }  /* if */
            if (friend_sym == NULL) {
              (void)sprintf(&buffer[strlen(buffer)], " ]");
            }  /* if */
            put_separator(sep, strlen(buffer));
            fprintf(f_debug, "%s", buffer);
            col += strlen(buffer);
            sep = ",";
            buffer[0] = '\0';
          } while (friend_sym != NULL);
        }  /* if */
      }
      break;
    case sk_field:
      if (sym->variant.field.ptr == NULL) {
        put_string("<null>");
      } else {
        a_field_ptr  fp = sym->variant.field.ptr;
        if (C_dialect == C_dialect_cplusplus) {
          put_access(fp->source_corresp.access);
        }  /* if */
        if (fp->is_mutable) {
          put_string("mutable");
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
          if (var->is_specialized) {
            (void)sprintf(buffer, "%sspecialization",
                          var->specialized_with_old_syntax ?
                                 "old-style " : "");
            put_string(buffer);
          }  /* if */
        } else {
          if (sym->variant.variable.value_has_been_set) put_string("set");
          if (sym->variant.variable.used) put_string("used");
#if MAINTAIN_NEEDED_FLAGS
          if (var->source_corresp.needed) put_string("needed");
#endif /* MAINTAIN_NEEDED_FLAGS */
          if (var->is_parameter || var->is_handler_param) {
            put_string((char *)(var->is_parameter ? "is param"
                                                  : "is handler param"));
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
        if (rp->suppress_inline_body) put_string("suppress inline body");
        if (rp->compiler_generated) put_string("compiler generated");
        if (rp->is_trivial_default_constructor) {
          put_string("trivial default-ctor");
        }  /* if */
        (void)sprintf(buffer, "sc_%s",
                      db_storage_class_names[(int)rp->storage_class]);
        put_string(buffer);
        (void)str_name_linkage(buffer, &(rp->source_corresp));
        put_string(buffer);
        if (rp->is_template_function) put_string("is instance");
        if (rp->is_specialized) {
          (void)sprintf(buffer, "%sspecialization",
                        rp->specialized_with_old_syntax ? "old-style " : "");
          put_string(buffer);
        }  /* if */
#if MAINTAIN_NEEDED_FLAGS
        if (rp->source_corresp.needed) put_string("needed");
#endif /* MAINTAIN_NEEDED_FLAGS */
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
      if (sym->variant.projection.is_using_decl) {
        put_string("using decl");
      }  /* if */
      if (sym->variant.projection.any_intervening_using_decl) {
        put_string("intervening using decl");
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
        a_symbol_ptr                      inst_sym;
        a_template_param_ptr		  templ_param_list;
        a_template_decl_info_ptr	  template_decl_info;

        tssp = sym->variant.template_info;
        if (tssp->cache.tokens.first_token != NULL) {
          put_string("template body cached");
        }  /* if */
        if (sym->kind == (a_symbol_kind)sk_class_template) {
          a_symbol_ptr	prototype_sym;
          switch (tssp->variant.class_template.type_kind) {
            case tk_class:  put_string("class");           break;
            case tk_struct: put_string("struct");          break;
            case tk_union:  put_string("union");           break;
            case tk_error:  put_string("no type kind");    break;
            default:        put_string("<BAD TYPE KIND>"); break;
          }  /* switch */
          prototype_sym = tssp->variant.class_template.prototype_instantiation;
          if (prototype_sym != NULL) {
            /* If the prototype instantiation has a partial specialization
               template argument list (i.e., it is for a partial
               specialization), display the primary template argument list
               to identify the partial specialization. */
            a_type_ptr			prototype_type;
            a_class_type_supplement_ptr	ctsp;
            prototype_type = type_symbol_type(prototype_sym);
            ctsp = prototype_type->variant.class_struct_union.extra_info;
            if (ctsp->partial_spec_template_arg_list != NULL) {
              db_template_arg_list(ctsp->template_arg_list);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Output information from the template symbol supplement. */
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          template_decl_info = tssp->variant.function.decl_cache.decl_info;
        } else {
          template_decl_info = tssp->cache.decl_info;
        }  /* if */
        templ_param_list = template_decl_info != NULL ?
                                       template_decl_info->parameters : NULL;
        put_string("template parameters =\n");
        for (tplep = templ_param_list; tplep != NULL; tplep = tplep->next) {
          fprintf(f_debug, "%*s", indentation + 2, "");
          db_symbol(tplep->param_symbol, "", indentation + 4);
          switch (tplep->param_symbol->kind) {
            case sk_type:
              fprintf(f_debug, "%*sparameter type: ", indentation + 4, "");
              /* Display the proxy class type if one exists. */
              if (tplep->variant.type != NULL) {
                a_type_ptr ptype = tplep->variant.type;
                db_type(ptype);
                if (ptype->variant.template_param.extra_info != NULL) {
                  a_type_ptr  class_type;
                  class_type =
                          ptype->variant.template_param.extra_info->class_type;
                  if (class_type != NULL) {
                    fprintf(f_debug, "\n%*sproxy class: ",
                            indentation + 6, "");
                    db_type(class_type);
                  }  /* if */
                }  /* if */
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              if (tplep->has_default_arg) {
		if (!tplep->def_arg_involves_template_param) {
		  put_string("= ");
		  db_type(tplep->default_arg.type);
		} else {
		  put_string("= <token cache>");
		}  /* if */
	      }  /* if */
              break;
            case sk_constant:
              fprintf(f_debug, "%*sparameter constant: ", indentation + 4, "");
              if (tplep->variant.constant.ptr != NULL) {
                db_constant(tplep->variant.constant.ptr);
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              if (tplep->has_default_arg) {
		if (!tplep->def_arg_involves_template_param) {
		  put_string("= ");
		  db_constant(tplep->default_arg.constant);
		} else {
		  put_string("= <token cache>");
		}  /* if */
	      }  /* if */
              break;
            case sk_class_template:
              fprintf(f_debug, "%*sparameter template: ", indentation + 4, "");
              if (tplep->variant.templ != NULL) {
                a_template_ptr	templ_ptr;
                templ_ptr = tplep->variant.templ->il_template_entry;
                db_symbol((a_symbol_ptr)templ_ptr->source_corresp.assoc_info,
                          "", indentation + 4);
              } else {
                fprintf(f_debug, "NULL");
              }  /* if */
              break;
            default:
              fprintf(f_debug, "<BAD TEMPLATE PARAM SYMBOL KIND>");
          }  /* if */
          fprintf(f_debug, "\n");
          col = 0;
        }  /* for */
        if (sym->kind == (a_symbol_kind)sk_class_template) {
          /* Display the prototype instantiation. */
          inst_sym = tssp->variant.class_template.prototype_instantiation;
          if (inst_sym != NULL) {
            fprintf(f_debug, "%*sprototype instantiation:\n", indentation, "");
            fprintf(f_debug, "%*s", indentation + 2, "");
            db_symbol(inst_sym, "", indentation + 4);
          }  /* if */
          /* Display any partial specializations. */
          inst_sym = tssp->variant.class_template.partial_specializations;
          while (inst_sym != NULL) {
            fprintf(f_debug, "%*spartial specialization:\n", indentation, "");
            fprintf(f_debug, "%*s", indentation + 2, "");
            db_symbol(inst_sym, "", indentation + 4);
            inst_sym = inst_sym->next;
          }  /* while */
          /* Display instantiations based on this template. */
          inst_sym = tssp->variant.class_template.instantiations;
          while (inst_sym != NULL) {
            fprintf(f_debug, "%*sinstantiation:\n", indentation, "");
            fprintf(f_debug, "%*s", indentation + 2, "");
            db_symbol(inst_sym, "", indentation + 4);
            inst_sym = next_instance_sym(inst_sym);
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
          tip = tssp->variant.function.instantiations;
          while (tip != NULL) {
            fprintf(f_debug, "%*sinstance", indentation, "");
            if (tip->instance_sym == NULL) {
              fputs(": NULL instance sym\n", f_debug);
            } else {
              a_routine_ptr inst_rp = tip->instance_sym->variant.routine.ptr;
              if (tip->instantiation_required || tip->is_guiding_decl ||
                  inst_rp->is_specialized) {
                char* comma = "";
                fputs(" (", f_debug);
                if (tip->instantiation_required) {
                  fputs("instantiation req'd", f_debug);
                  comma = ", ";
                }  /* if */
                if (tip->is_guiding_decl) {
                  fprintf(f_debug, "%sguiding decl", comma);
                  comma = ", ";
                }  /* if */
                if (inst_rp->is_specialized) {
                  fprintf(f_debug, "%s%sspecialization", comma,
                          inst_rp->specialized_with_old_syntax ?
                                                            "old-style " : "");
                }  /* if */
                fputc(')', f_debug);
              }  /* if */
              fputs(":\n", f_debug);
              fprintf(f_debug, "%*s", indentation + 2, "");
              db_symbol(tip->instance_sym, "", indentation + 4);
            }  /* if */
            tip = tip->next;
          }  /* while */
        }  /* if */
        col = 0;
        suppress_newline = TRUE;
      }
      break;
    case sk_namespace:
      break;
    case sk_namespace_projection:
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
	 ((is_array_type(type_pointed_to(type)) && !space_left(35)) ||
	  is_function_type(type_pointed_to(type)))) ||
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
done:
  /* Recursive calls to db_symbol can create unwanted newlines in the
     output.  Don't output a newline if the last thing we did was
     a call to db_symbol. */
  if (!suppress_newline) (void)fputc('\n', f_debug);
  if (var != NULL) {
    db_initializer(var, indentation);
  }  /* if */
}  /* db_symbol */


void db_sym(a_symbol_ptr sym)
/*
A short-hand version of db_symbol for convenient access from a debugger.
*/
{
  db_symbol(sym, "", 2);
}  /* db_sym */

#endif /* DEBUG */

void set_source_corresp(a_source_correspondence *sc,
                        a_symbol_ptr            sp)
/*
Set the given source correspondence to point to the given symbol.  The
scope for the symbol must still be active.
*/
{
  a_boolean  is_local_to_function = FALSE;

  sc->assoc_info = (char *)sp;
  if (sp->header == unnamed_tag_symbol_header) {
    /* Let the name pointer in the IL entry remain NULL. */
  } else {
    /* Note that the identifier name was allocated in the intermediate language
       memory area (see find_symbol); it can therefore be used without
       copying. */
    sc->name = sp->header->identifier;
  }  /* if */
  if (sc->decl_position.seq != 0) {
    /* The decl-position is already set, so this must be a resetting of
       the source correspondence.  Let the caller decide whether and how the
       current value should be overwritten. */
  } else {
    /* Set the source position. */
    sc->decl_position = sp->decl_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (sc->decl_pos_info != NULL) {
      /* If decl_position is being reset, assume that the related source
         position information (which should be tied to the same declaration)
         has been invalidated. */
      clear_decl_position_supplement(sc->decl_pos_info);
    } else if (sp->decl_position.seq != 0) {
      /* Create a decl-position-supplement for this entry. */
      sc->decl_pos_info = alloc_decl_position_supplement(in_file_scope(sc));
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
  sc->scope_depth = scope_depth_of_symbol(sp, &is_local_to_function);
#else /* RECORD_SCOPE_DEPTH_IN_IL */
  (void)scope_depth_of_symbol(sp, &is_local_to_function);
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Set the is_local_to_function flag. */
  sc->is_local_to_function = is_local_to_function;
}  /* set_source_corresp */


void set_class_membership(a_symbol_ptr             sym,
                          a_source_correspondence  *scp,
                          a_type_ptr               class_type)
/*
Set the is_class_member and parent.class_type fields of the indicated
symbol and source-correspondence entries.  class_type may be NULL to
indicate that the entity is not actually a member.  If sym or scp is
NULL (which can happen,. e.g., with anonymous unions, unnamed fields, or
fields generated by IL lowering) ignore the entity.
*/
{
  if (class_type != NULL) {
    if (sym != NULL) {
      sym->is_class_member = TRUE;
      sym->parent.class_type = class_type;
    }  /* if */
    if (scp != NULL) {
      scp->is_class_member = TRUE;
      scp->parent.class_type = class_type;
    }  /* if */
  }  /* if */
}  /* set_class_membership */


void set_class_membership_for_template(a_symbol_ptr	sym,
				       a_template_ptr	templ,
				       a_type_ptr	class_type)
/*
This routine calls set_class_membership for the template entry specified
by templ, but does not do so for members of nonreal classes unless we
are including prototype instantiations in the IL.
*/
{
  a_boolean	set_membership = FALSE;

  if (prototype_instantiations_in_il) {
    set_membership = TRUE;
  } else if (!class_type->variant.class_struct_union.is_nonreal_class) {
    set_membership = TRUE;
  }  /* if */
  if (set_membership) {
    set_class_membership(sym, &templ->source_corresp, class_type);
  }  /* if */
}  /* set_class_membership_for_template */


void set_namespace_membership(a_symbol_ptr             sym,
                              a_source_correspondence  *scp,
                              a_namespace_ptr          nsp)
/*
Clear is_class_member and set parent.namespace_ptr for the indicated symbol
and source-correspondence entries.  If nsp is NULL use the current scope
depth to determine the namespace.  If sym or scp is NULL, ignore the entity.
*/
{
  a_scope_stack_entry_ptr  ssep;

  if (nsp == NULL) {
    if (depth_scope_stack > DEPTH_OF_FILE_SCOPE &&
        depth_scope_stack <= depth_innermost_namespace_scope) {
      ssep = &scope_stack[depth_scope_stack];
      check_assertion_str(ssep->il_scope != NULL &&
                          ssep->il_scope->kind == (a_scope_kind)sck_namespace,
                          "set_namespace_membership: unexpected scope kind");
      nsp = ssep->il_scope->variant.assoc_namespace;
    }  /* if */
  } else {
    /* If the namespace supplied is an alias, get the "base" namespace. */
    while (nsp->is_namespace_alias) nsp = nsp->variant.assoc_namespace;
  }  /* if */
  if (nsp != NULL) {
    if (sym != NULL) {
      sym->is_class_member = FALSE;
      sym->parent.namespace_ptr = nsp;
    }  /* if */
    if (scp != NULL) {
      scp->is_class_member = FALSE;
      scp->parent.namespace_ptr = nsp;
    }  /* if */
  }  /* if */
}  /* set_namespace_membership */


void set_membership_in_source_corresp(a_source_correspondence  *scp,
                                      a_symbol_ptr             sym)
/*
Set the class/namespace membership information in the source correspondence
information pointed to by scp based on the membership information in the
symbol entry pointed to by sym.
*/
{
  if (sym->is_class_member) {
    set_class_membership((a_symbol_ptr)NULL, scp, sym->parent.class_type);
  } else if (sym->parent.namespace_ptr != NULL) {
    set_namespace_membership((a_symbol_ptr)NULL, scp,
                             sym->parent.namespace_ptr);
  }  /* if */
}  /* set_membership_in_source_corresp */


#if !RECORD_SCOPE_DEPTH_IN_IL
/*ARGSUSED*/ /* <-- depth is only used when local entities are promoted. */
#endif /* !RECORD_SCOPE_DEPTH_IN_IL */
void set_source_corresp_with_scope_depth(a_source_correspondence *sc,
                                         a_symbol_ptr            sp,
			                 a_scope_depth		depth)
/*
Set the source correspondence to point to a given symbol for which
the scope is not still active.  This routine works by temporarily
changing the scope of the symbol to NO_SCOPE_NUMBER and calling
set_source_corresp.  The scope number is set to its original value
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


a_special_function_kind special_function_kind_for_symbol(a_symbol_ptr	sym)
/*
Return the special function kind of the routine associated with sym.
If sym is not a routine, or is not a special function, return sfk_none.
*/
{
  a_special_function_kind	kind;

  switch (sym->kind) {
    case sk_routine:
    case sk_member_function:
      kind = sym->variant.routine.ptr->special_kind;
      break;
    case sk_overloaded_function:
      /* All entries on a list of overloaded functions should have the same
         special function kind, so looking at the first on the list is
         sufficient. */
      sym = sym->variant.overloaded_function.symbols;
      kind = special_function_kind_for_symbol(sym);
      break;
    case sk_function_template:
      kind = sym->variant.template_info->
                                        variant.function.routine->special_kind;
      break;
    default:
      kind = (a_special_function_kind)sfk_none;
  }  /* switch */
  return kind;
}  /* special_function_kind_for_symbol */


a_type_ptr underlying_function_type(a_symbol_ptr  sym)
/*
If the given symbol refers to a function, a typedef, a variable or a data
member, extract its type and see if it is a function type or a type composed
from a function type.  If so, return the underlying function type; otherwise,
return NULL.
*/
{
  a_type_ptr  result;

  /* First extract a type pointer: */
  switch (sym->kind) {
    case sk_routine:
    case sk_member_function:
      result = sym->variant.routine.ptr->type;
      break;
    case sk_function_template:
      result = sym->variant.template_info->variant.function.routine->type;
      break;
    case sk_variable:
      result = sym->variant.variable.ptr->type;
      break;
    case sk_type:
      result = sym->variant.type.ptr;
      break;
    case sk_static_data_member:
      result = sym->variant.static_data_member.variable->type;
      break;
    case sk_field:
      result = sym->variant.field.ptr->type;
      break;
    default:
      result = NULL;
  }  /* switch */
  /* Now peel off pointer and reference operators until we find a routine
     type (if at all): */
  while (result != NULL && !is_function_type(result)) {
    result = skip_typerefs(result);
    if (is_ptr_or_ref_type(result)) {
      result = type_pointed_to(result);
    } else if (is_ptr_to_member_type(result)) {
      result = pm_member_type(result);
    } else if (is_array_type(result)) {
      result = array_element_type(result);
    } else if (!is_function_type(result)) {
      result = NULL;
    }  /* if */
  }  /* while */
  return result;
}  /* underlying_routine_type */


a_boolean is_member_enum_symbol(a_symbol_ptr sym)
/*
Used in Microsoft mode to determine whether a symbol refers to an
enumeration that is a class member.  Such enumerations are accepted
by the Microsoft compiler in the qualifier portion of a name.
Return TRUE if the symbol refers to a class member enumeration.
*/
{
  a_type_ptr	tp;

  /* If the symbol refers to a type that is an enumeration, set tp to
     the type that represents the enumeration. */
  if (sym->kind == (a_symbol_kind)sk_enum_tag) {
    tp = sym->variant.enumeration.type;
  } else if (sym->kind == (a_symbol_kind)sk_type) {
    tp = sym->variant.type.ptr;
    tp = skip_typerefs(tp);
    if (!is_enum_type(tp)) tp = NULL;
  } else {
    tp = NULL;
  }  /* if */
  /* If a type was found, does it refer to a class member?  If not, set
     the type pointer to NULL. */
  if (tp != NULL && !tp->source_corresp.is_class_member) tp = NULL;
  return tp != NULL;
}  /* is_member_enum_symbol */


a_boolean overload_set_contains_template(a_symbol_ptr sym)
/*
Return TRUE if sym points to an overload set containing a function
template symbol.
*/
{
  a_boolean	result = FALSE;

  check_assertion(sym->kind == (a_symbol_kind)sk_overloaded_function);
  for (sym = sym->variant.overloaded_function.symbols;
       sym != NULL && !result; sym = sym->next) {
    a_symbol_ptr	fund_sym;
    fund_sym = fundamental_symbol_of(sym);
    if (fund_sym->kind == (a_symbol_kind)sk_function_template) result = TRUE;
  }  /* for */
  return result;
}  /* overload_set_contains_template */


a_boolean is_proxy_member_symbol(a_symbol_ptr  sym)
/*
Return TRUE if the symbol sym refers to a hypothetical member of a proxy
class.  (E.g., the symbol returned for T::f, where T is a template parameter.
*/
{
  a_boolean  result = FALSE;
  if (sym->kind == (a_symbol_kind)sk_constant) {
    a_constant_ptr  constant = sym->variant.constant;
    if (constant != NULL &&
        constant->kind == (a_constant_repr_kind)ck_template_param &&
        constant->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_member) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_proxy_member_symbol */


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
  ptr->other_symbols     = NULL;
  ptr->identifier        = NULL;
  ptr->identifier_length = 0;
  ptr->any_nested_types_on_inactive_list = FALSE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  ptr->has_cfront_transitional_nested_type_mangled_name = FALSE;
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if RECORD_HIDDEN_NAMES_IN_IL
  ptr->any_tag_decl = FALSE;
  ptr->any_decl_in_file_or_namespace_scope = FALSE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
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


a_substituted_type_list_entry_ptr alloc_substituted_type_list_entry(void)
/*
Allocate a new type list entry and return a pointer to it.
*/
{
  register a_substituted_type_list_entry_ptr ptr;

  if (avail_substituted_type_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_substituted_type_list_entries;
    avail_substituted_type_list_entries =
                                     avail_substituted_type_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_substituted_type_list_entry_ptr)
                               alloc_fe(sizeof(a_substituted_type_list_entry));
  }  /* if */
#if DEBUG
  num_substituted_type_list_entries_allocated++;
#endif /* DEBUG */
  ptr->next = NULL;
  ptr->templ_arg_list = NULL;
  ptr->type = NULL;
  return ptr;
}  /* alloc_substituted_type_list_entry */


void free_list_of_substituted_type_list_entries(
				a_substituted_type_list_entry_ptr stlep)
/*
Add a list of symbol list entries to the available list.  stlep may
be NULL, in which case nothing is done.
*/
{
  a_substituted_type_list_entry_ptr	stlep_tail;
  if (stlep != NULL) {
    /* Find the last entry on the list. */
    stlep_tail = stlep;
    while (stlep_tail->next != NULL) stlep_tail = stlep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    stlep_tail->next = avail_substituted_type_list_entries;
    avail_substituted_type_list_entries = stlep;
  }  /* if */
}  /* free_list_of_substituted_type_list_entries */


a_symbol_list_entry_ptr alloc_symbol_list_entry(void)
/*
Allocate a new symbol list entry and return a pointer to it.
*/
{
  register a_symbol_list_entry_ptr ptr;

  db_enter(5, "alloc_symbol_list_entry");
  if (avail_symbol_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_symbol_list_entries;
    avail_symbol_list_entries = avail_symbol_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_symbol_list_entry_ptr)alloc_fe(sizeof(a_symbol_list_entry));
#if DEBUG
   num_symbol_list_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  ptr->next    = NULL;
  ptr->symbol  = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_symbol_list_entry */


void free_list_of_symbol_list_entries(a_symbol_list_entry_ptr slep)
/*
Add a list of symbol list entries to the available list.  slep may
be NULL, in which case nothing is done.
*/
{
  a_symbol_list_entry_ptr	slep_tail;
  if (slep != NULL) {
    /* Find the last entry on the list. */
    slep_tail = slep;
    while (slep_tail->next != NULL) slep_tail = slep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    slep_tail->next = avail_symbol_list_entries;
    avail_symbol_list_entries = slep;
  }  /* if */
}  /* free_list_of_symbol_list_entries */


a_type_list_entry_ptr alloc_type_list_entry(void)
/*
Allocate a new type list entry and return a pointer to it.
*/
{
  register a_type_list_entry_ptr ptr;

  db_enter(5, "alloc_type_list_entry");
  if (avail_type_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_type_list_entries;
    avail_type_list_entries = avail_type_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_type_list_entry_ptr)alloc_fe(sizeof(a_type_list_entry));
#if DEBUG
   num_type_list_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  ptr->next    = NULL;
  ptr->type  = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_type_list_entry */


void free_list_of_type_list_entries(a_type_list_entry_ptr tlep)
/*
Add a list of type list entries to the available list.  tlep may
be NULL, in which case nothing is done.
*/
{
  a_type_list_entry_ptr	tlep_tail;
  if (tlep != NULL) {
    /* Find the last entry on the list. */
    tlep_tail = tlep;
    while (tlep_tail->next != NULL) tlep_tail = tlep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    tlep_tail->next = avail_type_list_entries;
    avail_type_list_entries = tlep;
  }  /* if */
}  /* free_list_of_type_list_entries */


a_symbol_ptr find_symbol(char             *identifier,
			 sizeof_t         length,
			 a_symbol_locator *location)
/*
Look up a symbol in the symbol table.  Return a pointer to the first symbol
under the symbol header for that name.  If the symbol header is not there,
create one and set the symbol locator to point to the header.  Note that
the source position in the locator is not changed; usually, it will have
been set by get_token when an identifier is scanned, but sometimes the
caller may have to set it directly.
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
     first 3, last 3, and middle 3 characters.  Of course, if the identifier
     has 9 or fewer characters, take the entire identifier. */
  ptr = identifier;
  if (length > 9) {
    hash_value = (unsigned char)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr;
    ptr = identifier + (length >> 1) - 1;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr;
    ptr = identifier + length - 3;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr;
  } else {
    for (a = 0; a < length; a++) {
      hash_value = (hash_value * HASH_FACTOR) + (unsigned char)*ptr++;
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


a_boolean looks_like_ctor_or_dtor(a_symbol_locator  *loc)
/*
Return TRUE if the given symbol locator looks like that for a constructor or
destructor.  The answer can be TRUE even when an error symbol is given (in
which case the symbol is never actually marked as being a special function).
This is useful to inhibit some diagnostics that are not meaningful on
constructors or destructors (e.g., missing return statements and implicit
return types).
*/
{
  a_boolean  answer = FALSE;

  if (loc->is_class_member && loc->symbol_header != NULL) {
    a_symbol_ptr  parent = (a_symbol_ptr)loc->parent.class_type
                                                  ->source_corresp.assoc_info;
    if (loc->symbol_header->identifier != NULL &&
        parent->header->identifier != NULL &&
        strcmp(loc->symbol_header->identifier,
               parent->header->identifier) == 0) {
      answer = TRUE;
    }  /* if */
  } /* if */
  if (!answer && loc->symbol_header != NULL) {
    /* Misdeclared destructors may not be marked as class members: */
    char *name = loc->symbol_header->identifier;
    if (name != NULL && name[0] == '~') {
      answer = TRUE;
    }  /* if */
  }  /* if */
  return answer;
}  /* looks_like_ctor_or_dtor */


void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                             a_symbol_locator *location)
/*
Create in *location a locator for the symbol pointed to by sym_ptr.
*/
{
  clear_locator(location, &sym_ptr->decl_position);
  location->symbol_header = sym_ptr->header;
  location->specific_symbol = sym_ptr;
  location->is_class_member = sym_ptr->is_class_member;
  location->parent = sym_ptr->parent;
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


void clear_qualifier_from_locator(a_symbol_locator  *locator)
/*
Reset the fields in the specified locator to remove traces of a class,
global, or namespace qualifier.
*/
{
  locator->is_qualified_name = FALSE;
  locator->is_file_scope_qualified_name = FALSE;
  locator->is_global_qualified_name = FALSE;
  if (locator->is_class_member) {
    locator->is_class_member = FALSE;
    locator->parent.class_type = NULL;
  } else {
    locator->parent.namespace_ptr = NULL;
  }  /* if */
}  /* clear_qualifier_from_locator */


a_namespace_list_entry_ptr alloc_namespace_list_entry(void)
/*
Allocate a namespace list entry and return a pointer to it.
*/
{
  a_namespace_list_entry_ptr ptr;

  if (avail_namespace_list_entries != NULL) {
    /* Reuse an existing entry. */
    ptr = avail_namespace_list_entries;
    avail_namespace_list_entries = avail_namespace_list_entries->next;
  } else {
    /* Allocate a new entry. */
    ptr = (a_namespace_list_entry_ptr)alloc_fe(sizeof(a_namespace_list_entry));
#if DEBUG
   num_namespace_list_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  ptr->next = NULL;
  ptr->ptr = NULL;
  return ptr;
}  /* alloc_namespace_list_entry */


void free_list_of_namespace_list_entries(a_namespace_list_entry_ptr nlep)
/*
Add a list of namespace list entries to the available list.  nlep may
be NULL, in which case nothing is done.
*/
{
  a_namespace_list_entry_ptr	nlep_tail;
  if (nlep != NULL) {
    /* Find the last entry on the list. */
    nlep_tail = nlep;
    while (nlep_tail->next != NULL) nlep_tail = nlep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    nlep_tail->next = avail_namespace_list_entries;
    avail_namespace_list_entries = nlep;
  }  /* if */
}  /* free_list_of_namespace_list_entries */


a_symbol_ptr corresp_prototype_for_class_symbol(a_symbol_ptr sym)
/*
sym points to a symbol entry for a class.  If the class is an instance
of a template, but not a specialized template or a nonreal class, return
the corresponding prototype symbol from the class symbol supplement.
Otherwise, return NULL.
*/
{
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_ptr			result_sym = NULL;
  a_type_ptr                    class_type;

  check_assertion(is_class_struct_union_symbol(sym));
  cssp = sym->variant.class_struct_union.extra_info;
  class_type = sym->variant.class_struct_union.type;
  if (class_type->variant.class_struct_union.is_template_class &&
      !class_type->variant.class_struct_union.is_nonreal_class) {
    if (!class_type->variant.class_struct_union.is_specialized) {
      result_sym = cssp->corresp_prototype_sym;
      check_assertion_str2(result_sym != NULL,
                           "corresp_prototype_for_class_symbol:",
                           "no corresponding prototype symbol for instance");
    }  /* if */
  }  /* if */
  return result_sym;
}  /* corresp_prototype_for_class_symbol */


a_symbol_ptr template_symbol_for_class_symbol(a_symbol_ptr class_sym)
/*
Return the template symbol for the template from which the class
associated with class_sym was generated.  If class_sym points to an
instance of a class template, the template symbol returned points to
the class template symbol associated with the template definition.  If
class_sym points to a class nested within a class template, the template
symbol returned points to the prototype instantiation of the nested
class.
*/
{
  a_symbol_ptr			template_sym;
  a_class_symbol_supplement_ptr	cssp;

  cssp = class_sym->variant.class_struct_union.extra_info;
  if (cssp->class_template == NULL) {
    /* If the class_template pointer is NULL, this is expected to be a class
       nested within a class template. */
    template_sym = cssp->corresp_prototype_sym;
  } else {
    template_sym = cssp->class_template;
  }  /* if */
  return template_sym;
}  /* template_symbol_for_class_symbol */


a_template_cache_segment_ptr alloc_template_cache_segment(
                                a_symbol_ptr				sym,
                                a_template_symbol_supplement_ptr	tssp)
/*
Allocate a new template segment descriptor entry, initialize its fields, and
return a pointer to it.  sym points to the symbol for the member class or
function for which the cache segment entry is being created.  tssp points
to the symbol supplement associated with sym.
*/
{
  a_template_cache_segment_ptr  tcsp;
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			depth_to_use;

  if (avail_template_cache_segments != NULL) {
    /* Reuse an existing entry. */
    tcsp = avail_template_cache_segments;
    avail_template_cache_segments = tcsp->next;
  } else {
    /* Allocate a new entry. */
    tcsp = (a_template_cache_segment_ptr)
                   alloc_fe(sizeof(a_template_cache_segment));
#if DEBUG
    num_template_cache_segments_allocated++;
#endif /* DEBUG */
  }  /* if */
  tcsp->next = NULL;
  tcsp->symbol = sym;
  tcsp->template_info = tssp;
  tcsp->first_token_number = NO_TOKEN_SEQUENCE_NUMBER;
  tcsp->last_token_number = NO_TOKEN_SEQUENCE_NUMBER;
  tcsp->before_first_token = NULL;
  tcsp->last_token = NULL;
  tcsp->is_friend = FALSE;
  tcsp->is_default_arg = FALSE;
  tcsp->default_arg_missing = FALSE;
  /* Add the new entry to the list of template cache segments associated
     with the current instantiation.  If there is no current instantiation,
     use the current template declaration scope. */
  depth_to_use = depth_innermost_instantiation_scope;
  if (depth_to_use == NO_SCOPE_DEPTH) {
    depth_to_use = depth_template_declaration_scope;
    check_assertion(depth_to_use != NO_SCOPE_DEPTH);
  }  /* if */
  ssep = &scope_stack[depth_to_use];
  check_assertion_str2(ssep->in_prototype_instantiation ||
                       ssep->kind == (a_scope_kind)sck_template_declaration,
                       "alloc_template_cache_segment:",
                       "not in prototype instantiation");
  if (ssep->first_template_cache_segment == NULL) {
    ssep->first_template_cache_segment = tcsp;
  }  /* if */
  if (ssep->last_template_cache_segment != NULL) {
    ssep->last_template_cache_segment->next = tcsp;
  }  /* if */
  ssep->last_template_cache_segment = tcsp;
  return tcsp;
}  /* alloc_template_cache_segment */


void free_template_cache_segment(a_template_cache_segment_ptr tcsp)
/*
Free a template cache segment entry and return it to the available list.
*/
{
  tcsp->next = avail_template_cache_segments;
  avail_template_cache_segments = tcsp;
}  /* free_template_cache_segment */


a_template_decl_info_ptr alloc_template_decl_info(void)
/*
Allocate a new template declaration information entry, initialize its
fields, and return a pointer to it.
*/
{
  a_template_decl_info_ptr  tdip;

  /* Allocate a template declaration information entry. */
  tdip = (a_template_decl_info_ptr)alloc_fe(sizeof(a_template_decl_info));
  tdip->parameters = NULL;
  tdip->declaration_scope = NO_SCOPE_NUMBER;
  tdip->enclosing_scope = NULL;
  tdip->enclosing_template_decl = NULL;
  tdip->name_linkage = (a_name_linkage_kind)nlk_none;
  tdip->decl_seq = 0;
  tdip->nondependent_calls = NULL;
  tdip->last_entry_added = NULL;
#if DEBUG
  num_template_decl_info_allocated++;
#endif /* DEBUG */

  return tdip;
}  /* alloc_template_decl_info */


static a_nondependent_call_info_ptr alloc_nondependent_call_info(void)
/*
Allocate a new nondependent call information entry, initialize its
fields, and return a pointer to it.
*/
{
  a_nondependent_call_info_ptr  ndcip;

  /* Allocate the entry. */
  ndcip = (a_nondependent_call_info_ptr)
                                   alloc_fe(sizeof(a_nondependent_call_info));
  ndcip->next = NULL;
  ndcip->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  ndcip->symbol = NULL;
#if DEBUG
  num_nondependent_call_info_allocated++;
#endif /* DEBUG */
  return ndcip;
}  /* alloc_nondependent_call_info */


a_nondependent_call_info_ptr get_nondependent_call_info(
				a_token_sequence_number		tsn)
/*
If "tsn" is the token sequence number of a nondependent call in the
nondependent call list of the current template, return a pointer to
the associated information block.  Otherwise (i.e., if the call is
dependent), return NULL.  The list is maintained in token sequence
number order, and is pointed to from the template decl info block for
the template.  The current position on the list is maintained in the
next_nondependent_call field of the scope stack entry for the innermost
instantiation scope.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_nondependent_call_info_ptr	list_ptr, result = NULL;

  check_assertion(depth_innermost_instantiation_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth_innermost_instantiation_scope];
  list_ptr = ssep->next_nondependent_call;
  /* Find the next entry on the list whose token sequence number is not
     before the one that we are looking for.  Entries should only
     be skipped in error recovery cases. */
  while (list_ptr != NULL && tsn > list_ptr->token_sequence_number) {
    list_ptr = list_ptr->next;
  }  /* while */
  if (list_ptr != NULL) {
    if (tsn == list_ptr->token_sequence_number) {
      /* The token sequence number matches the next entry on the list.
         Return the entry and move to the next entry on the list. */
      result = list_ptr;
      list_ptr = list_ptr->next;
    }  /* if */
  }  /* if */
  /* Save the updated list pointer back into the scope stack entry. */
  ssep->next_nondependent_call = list_ptr;
  return result;
}  /* get_nondependent_call_info */


void record_nondependent_call(a_symbol_ptr		symbol,
			      a_token_sequence_number	tsn)
/*
This routine is called within the scope of a template (either a
template declaration scope or a prototype instantiation) to record
the result of overload resolution for a nondependent call.  "symbol"
is the function symbol for the function to be called.  "tsn" is a
token sequence number used to represent this call so that the
entry can be found during a real instantiation.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			depth_to_use;
  a_template_decl_info_ptr	tdip;
  a_nondependent_call_info_ptr	ndcip;

  /* Find the innermost template declaration or template instantiation
     scope. */
  depth_to_use = depth_innermost_instantiation_scope;
  if (depth_to_use < depth_template_declaration_scope) {
    depth_to_use = depth_template_declaration_scope;
  } else {
    /* A template instantiation scope must be for a prototype instantiation. */
    check_assertion(depth_to_use != NO_SCOPE_DEPTH);
    check_assertion(scope_stack[depth_to_use].in_prototype_instantiation);
  }  /* if */
  check_assertion(depth_to_use != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth_to_use];
  tdip = ssep->template_decl_info;
  check_assertion(tdip != NULL);
  /* Create a nondependent call entry. */
  ndcip = alloc_nondependent_call_info();
  ndcip->symbol = symbol;
  ndcip->token_sequence_number = tsn;
  /* Add the entry to the appropriate point in the list.  This is usually
     immediately after the last entry added, but in certain cases we need
     to locate the appropriate insertion point. */
  if (tdip->nondependent_calls == NULL ||
      tdip->nondependent_calls->token_sequence_number > tsn) {
    /* Either the list is entry, or the token sequence number of this entry
       precedes the previous start of the list. */
    ndcip->next = tdip->nondependent_calls;
    tdip->nondependent_calls = ndcip;
  } else {
    /* The new entry does not go at the start of the list.  See if the
       last_entry_added points to the right insert location. */
    a_nondependent_call_info_ptr	insert_loc;
    insert_loc = tdip->last_entry_added;
    /* If the token sequence number of the insert location is after the
       desired location, restart the search from the beginning of the list. */
    if (insert_loc->token_sequence_number > tsn) {
      insert_loc = tdip->nondependent_calls;
    }  /* if */
    /* Find an entry with a token sequence number greater than the one we
       are inserting, or the end of the list.  We are usually at the
       right place (i.e., nothing needs to be done). */
    while (insert_loc->next != NULL &&
           insert_loc->next->token_sequence_number < tsn) {
      insert_loc = insert_loc->next;
    }  /* while */
    ndcip->next = insert_loc->next;
    insert_loc->next = ndcip;
  }  /* if */
  tdip->last_entry_added = ndcip;
}  /* record_nondependent_call */
				

a_templ_friend_info_ptr alloc_templ_friend_info(void)
/*
Allocate a new template friend information entry, initialize its fields,
and return a pointer to it.
*/
{
  a_templ_friend_info_ptr  tfip;

  /* Allocate a template friend default argument entry. */
  tfip = (a_templ_friend_info_ptr)alloc_fe(sizeof(a_templ_friend_info));
  tfip->next = NULL;
  tfip->symbol = NULL;
  tfip->token_number = NO_TOKEN_SEQUENCE_NUMBER;
#if DEBUG
  num_templ_friend_info_allocated++;
#endif /* DEBUG */
  return tfip;
}  /* alloc_templ_friend_info */


static a_namespace_symbol_supplement_ptr
                                  alloc_namespace_symbol_supplement(void)
/*
Allocate a new template symbol supplement entry, initialize its fields, and
return a pointer to it.
*/
{
  a_namespace_symbol_supplement_ptr  nssp;

  /* Allocate a namespace symbol supplement. */
  nssp = (a_namespace_symbol_supplement_ptr)
                   alloc_fe(sizeof(a_namespace_symbol_supplement));
  nssp->scope_depth_at_which_using_directive_applies = NO_SCOPE_DEPTH;
  nssp->depth_innermost_active_using_directive = NO_SCOPE_DEPTH;
  nssp->namespace_list_entry = NULL;
  nssp->visited_by_qualified_lookup = FALSE;
  nssp->within_unnamed_namespace = FALSE;
#if DEBUG
  num_namespace_symbol_supplements_allocated++;
#endif /* DEBUG */
  clear_scope_pointers_block(&nssp->pointers_block);

  return nssp;
}  /* alloc_namespace_symbol_supplement */


void clear_template_cache(a_template_cache_ptr	tcp,
                          a_boolean		is_reusable)
/*
Initialize a template cache.
*/
{
  clear_token_cache(&tcp->tokens, is_reusable);
  tcp->decl_info = NULL;
}  /* clear_template_cache */


void set_template_cache_info(a_template_cache_ptr	tcp,
			     a_token_cache_ptr		tokens,
			     a_template_decl_info_ptr	tdip)
/*
Set the fields of a template cache entry.  Only set the field if a non-NULL
value is passed in.
*/
{
  if (tokens != NULL) tcp->tokens = *tokens;
  if (tdip != NULL) tcp->decl_info = tdip;
}  /* set_template_cache_info */


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
  tssp->pending_instantiations = 0;
  tssp->pragmas_bound_to_template = NULL;
  tssp->token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
  clear_template_cache(&tssp->cache, /*reusable=*/TRUE);
  tssp->befriending_classes = NULL;
  tssp->cache_segment = NULL;
  tssp->prototype_template = NULL;
  tssp->subordinate_templates = NULL;
  tssp->il_template_entry = NULL;
  tssp->all_instantiations = NULL;
  tssp->is_specific_definition = FALSE;
  tssp->is_nonreal_member = FALSE;
  tssp->is_error = FALSE;
#if CHECKING 
  tssp->avoid_codecenter_warnings = FALSE;
#endif /* CHECKING */
  switch (kind) {
    case sk_class_template:
    case sk_class_or_struct_tag:
    case sk_union_tag:
      tssp->variant.class_template.instantiations = NULL;
      tssp->variant.class_template.type_kind = (a_type_kind)tk_error;
      tssp->variant.class_template.prototype_instantiation = NULL;
      tssp->variant.class_template.partial_specializations = NULL;
      tssp->variant.class_template.primary_template_sym = NULL;
      tssp->variant.class_template.friend_info = NULL;
      tssp->variant.class_template.prototype_instantiation_complete = FALSE;
      tssp->variant.class_template.access =
                                         (an_access_specifier)as_inaccessible;
      tssp->variant.class_template.name_linkage =
                                            (a_name_linkage_kind)nlk_none;
      tssp->variant.class_template.not_standalone_nested_class = FALSE;
      tssp->variant.class_template.template_template_param = FALSE;
      tssp->variant.class_template.argument_template = NULL;
#if CHECKING 
      tssp->variant.class_template.avoid_codecenter_warnings = FALSE;
#endif /* CHECKING */
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      tssp->variant.class_template.source_sequence_list = NULL;
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
      break;
    case sk_function_template:
    case sk_member_function:
      tssp->variant.function.instantiations = NULL;
      tssp->variant.function.routine = NULL;
      clear_func_info(&tssp->variant.function.func_info);
      tssp->variant.function.def_arg_expr_list = NULL;
      clear_template_cache(&tssp->variant.function.decl_cache,
                          /*reusable=*/TRUE);
      tssp->variant.function.substituted_types = FALSE;
      tssp->variant.function.unused_instantiations = 0;
      tssp->variant.function.pending_partial_instantiations = 0;
      tssp->variant.function.prototype_friend_symbol = NULL;
      tssp->variant.function.template_param_not_in_function_type = FALSE;
#if CHECKING 
      tssp->variant.function.avoid_codecenter_warnings = FALSE;
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
      sym_ptr->variant.keyword.token = (a_byte_token_kind)tok_error;
      sym_ptr->variant.keyword.is_preprocessing_op_or_punc = FALSE;
      sym_ptr->variant.keyword.diagnostic_issued_if_used = ec_no_error;
      break;
    case sk_macro:
      sym_ptr->variant.macro_def = NULL;
      break;
    case sk_constant:
      sym_ptr->variant.constant = NULL;
      break;
    case sk_type:
      sym_ptr->variant.type.ptr = NULL;
      sym_ptr->variant.type.is_injected_class_name = FALSE;
      break;
    case sk_enum_tag:
      sym_ptr->variant.enumeration.type = NULL;
      sym_ptr->variant.enumeration.dependent_type_fixup_list = NULL;
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
        cssp->trivial_default_constructor = NULL;
        cssp->destructor = NULL;
        cssp->assignment_operator = NULL;
        cssp->conversion_list = NULL;
        cssp->conversion_template_list = NULL;
        cssp->routine_fixup_list = NULL;
        cssp->class_template = NULL;
        cssp->template_info = NULL;
        cssp->next_in_instantiations_list = NULL;
        cssp->member_decl_scope = NO_SCOPE_NUMBER;
        cssp->template_param_for_proxy_class = NULL;
        cssp->corresp_prototype_sym = NULL;
        cssp->prototype_token_sequence_number = NO_TOKEN_SEQUENCE_NUMBER;
        cssp->referencing_namespace = NULL;
        cssp->dependent_type_fixup_list = NULL;
        cssp->operator_lookup_namespaces = NULL;
        cssp->friend_functions = NULL;
        cssp->has_nontrivial_default_constructor = FALSE;
        cssp->has_user_declared_default_constructor = FALSE;
        cssp->has_copy_constructor = FALSE;
        cssp->has_copy_constructor_for_const_object = FALSE;
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
        cssp->construction_by_bitwise_copy_allowed = FALSE;
        cssp->target_of_conversion_function = FALSE;
        cssp->any_ref_member = FALSE;
        /* The is_class_aggregate flag is initialized to TRUE when we are not
           in C++ mode. */
        cssp->is_class_aggregate = (C_dialect != C_dialect_cplusplus);
        cssp->is_POD = FALSE;
        cssp->any_template_dependent_fields = FALSE;
        cssp->has_operator_new = FALSE;
        cssp->has_operator_array_new = FALSE;
        cssp->has_operator_delete = FALSE;
        cssp->has_operator_array_delete = FALSE;
        cssp->any_nonstatic_data_members = FALSE;
        cssp->any_nonreal_base_classes = FALSE;
        cssp->instantiation_in_progress = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        cssp->definition_is_first_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if CHECKING
        cssp->avoid_codecenter_warnings = FALSE;
#endif /* CHECKING */
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
        /* Both the variable and routine pointer are cleared even though
           they share the same location.  This is done to support a
           testing mode in which they do not actually share the same
           location. */
        esdp->variant.variable = NULL;
        esdp->variant.routine.ptr = NULL;
	esdp->variant.routine.is_implicit_declaration = FALSE;
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
        sym_ptr->variant.projection.is_using_decl = FALSE;
        sym_ptr->variant.projection.any_intervening_using_decl = FALSE;
        sym_ptr->variant.projection.fund_sym_is_nonreal_member = FALSE;
        sym_ptr->variant.projection.
                     injected_class_template_name_is_unambiguous = FALSE;
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
    case sk_namespace:
      sym_ptr->variant.namespace_info.ptr = NULL;
      sym_ptr->variant.namespace_info.extra_info =
                                  alloc_namespace_symbol_supplement();
      break;
    case sk_namespace_projection:
      sym_ptr->variant.namespace_projection.fundamental_symbol = NULL;
      break;
#if CHECKING
    default:
      internal_error("set_symbol_kind: bad symbol kind");
#endif /* CHECKING */
  }  /* switch */

  db_exit();
}  /* set_symbol_kind */


a_symbol_ptr alloc_symbol(a_symbol_kind       kind,
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


void unlink_symbol_from_symbol_table(a_symbol_ptr sym_ptr)
/*
Remove a symbol from the symbol table, i.e., unlink it from either the main
(active) symbol list or the inactive list of its symbol header.
*/
{
  register a_symbol_ptr        ptr, prev_ptr;
  register a_symbol_header_ptr hdr_ptr;

  db_enter(4, "unlink_symbol_from_symbol_table");
  if (sym_ptr->is_error) {
    /* Error symbols are never added to a symbol list and cannot be removed. */
  } else if (sym_ptr->kind == (a_symbol_kind)sk_extern_variable ||
             sym_ptr->kind == (a_symbol_kind)sk_extern_routine) {
    /* These symbols are not in the symbol table proper. */
  } else {
    hdr_ptr = sym_ptr->header;
    if (sym_ptr == hdr_ptr->symbol) {
      /* The symbol is the first on the header list.  Link around it. */
      hdr_ptr->symbol = sym_ptr->next;
    } else if (sym_ptr == hdr_ptr->inactive_symbols) {
      /* The symbol is the first on the inactive list.  Link around it. */
      hdr_ptr->inactive_symbols = sym_ptr->next;
    } else {
      /* The symbol is not the first on either of the lists.  Find it on one
         of the lists, remembering the preceding symbol. */
      ptr = NULL;
      if (hdr_ptr->symbol != NULL) {
        /* Check the active list. */
        prev_ptr = hdr_ptr->symbol;
        ptr = prev_ptr->next;
        while (ptr != NULL && ptr != sym_ptr) {
          prev_ptr = ptr;
          ptr = ptr->next;
        }  /* while */
      }  /* if */
      if (ptr == NULL) {
        /* It wasn't found on the active list.  Check the inactive list. */
        if (hdr_ptr->inactive_symbols != NULL) {
          prev_ptr = hdr_ptr->inactive_symbols;
          ptr = prev_ptr->next;
          while (ptr != NULL && ptr != sym_ptr) {
            prev_ptr = ptr;
            ptr = ptr->next;
          }  /* while */
        }  /* if */
      }  /* if */
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
      /* Found the entry on one of the lists.  Link around it. */
      prev_ptr->next = sym_ptr->next;
    }  /* if */
  }  /* if */
  sym_ptr->next = NULL;
  db_exit();
}  /* unlink_symbol_from_symbol_table */


static void remove_symbol_from_no_scope_list(a_symbol_ptr sym_ptr)
/*
Remove a symbol from the symbols_with_no_scope list.
*/
{
  a_symbol_ptr       ptr;
  a_symbol_ptr       prev_ptr;

  if (sym_ptr == symbols_with_no_scope) {
    symbols_with_no_scope = sym_ptr->next_in_scope;
    prev_ptr = NULL;
  } else {
    for (prev_ptr = symbols_with_no_scope;
         (ptr = prev_ptr->next_in_scope) != sym_ptr;
         prev_ptr = ptr) {
      check_assertion_str2(ptr != NULL, "remove_symbol_from_no_scope_list:",
                           "could not find symbol");
    }  /* for */
    prev_ptr->next_in_scope = sym_ptr->next_in_scope;
  }  /* if */
  /* If the removed entry is the last entry on the list, update the
     last-pointer. */
  if (sym_ptr == symbols_with_no_scope_tail) {
    symbols_with_no_scope_tail = prev_ptr;
  }  /* if */
}  /* remove_symbol_from_no_scope_list */


static void remove_symbol_from_scope_list(a_symbol_ptr sym_ptr)
/*
Remove the given symbol from the list of symbols for its scope.
*/
{
  register a_symbol_ptr       ptr, prev_ptr;
  a_scope_stack_entry_ptr     ssep;
  a_scope_pointers_block_ptr  pointers_block;

  if (sym_ptr->is_error) {
    /* Error symbols are not on the scope list and cannot be removed. */
  } else if (sym_ptr->decl_scope == NO_SCOPE_NUMBER) {
    /* Symbols removed by a command-line -U option can be outside of any
       scope. */
    remove_symbol_from_no_scope_list(sym_ptr);
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
    pointers_block = assoc_pointers_block_of(ssep);
    if (sym_ptr == pointers_block->symbols) {
      pointers_block->symbols = sym_ptr->next_in_scope;
      prev_ptr = NULL;
    } else {
      for (prev_ptr = pointers_block->symbols;
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
    if (sym_ptr == pointers_block->last_symbol) {
      pointers_block->last_symbol = prev_ptr;
    }  /* if */
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
    /* If this is a type symbol that has previously been designated as the
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


static a_boolean is_using_decl_to_same_type(a_symbol_ptr	sym1,
					    a_symbol_ptr	sym2)
/*
Returns TRUE if sym1 and sym2 refer to the same type after removing any
namespace projection symbols.
*/
{
  a_boolean	result = FALSE;

  if (sym1->kind == (a_symbol_kind)sk_namespace_projection ||
      sym2->kind == (a_symbol_kind)sk_namespace_projection) {
    sym1 = fundamental_symbol_of(sym1);
    sym2 = fundamental_symbol_of(sym2);
    if (sym1->kind == (a_symbol_kind)sk_type &&
        sym2->kind == (a_symbol_kind)sk_type) {
      if (identical_types(sym1->variant.type.ptr, sym2->variant.type.ptr)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_using_decl_to_same_type */


static
a_boolean symbols_may_coexist_in_curr_scope(a_symbol_ptr  old_sym,
                                            a_symbol_ptr  new_sym,
                                            a_symbol_ptr  *insert_sym,
                                            a_scope_depth scope_depth,
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

  if (old_sym->kind == (a_symbol_kind)sk_undefined) {
    /* The old symbol was created for an undefined symbol that
       was referenced.  A new symbol can always coexist with
       an undefined one. */
    err = FALSE;
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
  } else if (!C_mode()) {
    /* Some checks specific to C++ mode. */
    if (is_namespace_symbol(new_sym) || is_namespace_symbol(old_sym)) {
      /* A namespace name must be unique in its scope. */
      /* err = TRUE; */
    } else if (scope_stack[scope_depth].in_prototype_instantiation &&
               scope_stack[scope_depth].kind ==
                                 (a_scope_kind)sck_template_instantiation) {
      /* This must be a template friend declaration during prototype
         instantiation.  The symbol is injected into the template instantiation
         scope, and overloading is not performed at this point.  Let the two
         symbols coexist. */
      err = FALSE;
    } else if (is_injected_class_symbol(old_sym)) {
      /* The old symbol is an injected class-name.  It is hidden by the
         current declaration. */
      err = FALSE;
    } else if (!strict_ansi_mode &&
               is_using_decl_to_same_type(new_sym, old_sym)) {
      /* At least one of the symbols is a namespace projection that points
         to the same type as the other symbol.  Enter the new symbol.  The
         insert point is not changed, so the newly entered symbol will be
         used. */
      err = FALSE;
    } else {
      a_symbol_ptr fund_new_sym = fundamental_symbol_of(new_sym);
      a_symbol_ptr fund_old_sym = fundamental_symbol_of(old_sym);

      if (strict_ansi_mode &&
          (is_template_symbol(fund_new_sym) ||
           is_template_symbol(fund_old_sym) ||
           (fund_old_sym->kind == (a_symbol_kind)sk_overloaded_function &&
            overload_set_contains_template(fund_old_sym)))) {
        /* In strict mode a template name must be unique in its scope.  It
           can be part of an overload set, but otherwise there can be no
           declaration of the same name. */
        /* err = TRUE; */
      } else if (is_tag_symbol(fund_new_sym)) {
        /* New symbol is a tag symbol. */
        if (!is_type_symbol(fund_old_sym) &&
            !is_class_template_symbol(fund_old_sym)) {
          /* The old symbol is a non-type name.  Be sure the new symbol
             inserted into the list after the old one. */
          err = FALSE;
          if (insert_sym != NULL) *insert_sym = old_sym;
        }  /* if */
      } else if (is_tag_symbol(fund_old_sym)) {
        /* The old symbol is a tag symbol. */
        if (!is_type_symbol(fund_new_sym) &&
            !is_class_template_symbol(fund_new_sym)) {
          /* The new one is not a type symbol or a class template name.  It
             will be placed at the front of the list automatically. */
          err = FALSE;
        }  /* if */
      } else if (scope_stack[scope_depth].in_prototype_instantiation &&
                 scope_stack[scope_depth].kind ==
                                 (a_scope_kind)sck_class_struct_union) {
        if (is_class_member_using_decl_symbol(old_sym)) {
          if (is_nontype_template_param_symbol(fund_old_sym) &&
              (is_function_or_template_symbol(fund_new_sym) ||
               is_nontype_template_param_symbol(fund_new_sym))) {
            /* During a prototype instantiation a nontype using-declaration
               (which *could* represent a function) is followed by function
               declaration or another nontype using-declaration.  Assume
               these do not conflict.  No insert point needs to be set since
               the default places the function declaration (new_sym) in front
               of the using-decl (old_sym) in the active list. */
            err = FALSE;
          }  /* if */
        } else if (is_class_member_using_decl_symbol(new_sym)) {
          if (is_nontype_template_param_symbol(fund_new_sym) &&
              is_function_or_template_symbol(old_sym)) {
            /* The opposite case: during a prototype instantiation a function
               declaration (or several -- old_sym could be an overload set)
               is followed by a nontype using-declaration (which *could*
               represent another function).  Assume these do not conflict.
               Set the insert_sym so that the using-decl (new_sym) will not
               hide the function declaration (old-sym); this is especially
               important for building overload sets. */
            err = FALSE;
            if (insert_sym != NULL) *insert_sym = old_sym;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return !err;
}  /* symbols_may_coexist_in_curr_scope */


static a_boolean is_redeclared_template_param(a_symbol_ptr      sym,
                                              an_error_severity *severity)
/*
Look through the template parameters associated with the innermost
instantiation scope for a symbol whose header matches the header of sym.
Return TRUE if a match is found.  Return in *severity the error
severity to be used for the diagnostic when TRUE is returned.
*/
{
  a_template_param_ptr		tpp;
  a_boolean			result = FALSE;
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			starting_depth;

  /* If the current scope is the one in which the template parameters are
     considered to be declared, then a redeclaration is an error.  Otherwise,
     the severity depends on the mode. */
  *severity = scope_stack[depth_scope_stack].template_param_decl_scope
                                      ? es_error : strict_ansi_error_severity;
  /* Start searching at the innermost instantiation scope or the
     innermost template declaration scope, which ever is at a greater
     depth. */
  starting_depth = depth_innermost_instantiation_scope;
  if (starting_depth < depth_template_declaration_scope) {
    /* The innermost template declaration scope is at a greater depth
       than the innermost instantiation scope. Start at the innermost template
       declaration scope.  For redeclarations in template declaration
       scopes, always issue an error at the innermost template declaration
       scope. */
    starting_depth = depth_template_declaration_scope;
    *severity = es_error;
  }  /* if */
  for (ssep = scope_stack_entry_for(starting_depth);
       ssep != NULL; ssep = previous_scope_of(ssep)) {
    /* Only look at template instantiation and declaration scopes. */
    if (ssep->kind != (a_scope_kind)sck_template_instantiation &&
        ssep->kind != (a_scope_kind)sck_template_declaration) continue;
    tpp = ssep->template_decl_info->parameters;
    /* There should be a template parameter list present, except when we
       are in the process of scanning the template parameter list. */
    check_assertion(tpp != NULL ||
                    ssep->kind == (a_scope_kind)sck_template_declaration);
    while (tpp != NULL && !result) {
      a_symbol_ptr  param_symbol = tpp->param_symbol;
      if (param_symbol->header == sym->header) {
        result = TRUE;
      }  /* if */
      tpp = tpp->next;
    }  /* while */
    /* The parameter was declared in this scope.  Exit the loop. */
    if (result) break;
    /* Only redeclarations of the innermost instantiation scope are
       always errors. */
    *severity = strict_ansi_error_severity;
  }  /* for */
  return result;
}  /* is_redeclared_template_param */


static a_boolean is_redeclared_for_init_decl_name(a_symbol_header_ptr  hdr,
                                                  a_scope_depth  scope_depth)
/*
Return TRUE if scope_depth specifies a scope that, for purposes of name
lookup, is the same scope as an enclosing for-init scope and if hdr matches
the symbol header of a symbol declared in the for-init scope.
*/
{
  a_scope_stack_entry_ptr  curr_ssep, ssep;
  a_boolean                match = FALSE;
  a_symbol_ptr             sym;

  if (use_nonstandard_for_init_scope) {
    /* No need to check, since a for-init scope will not have been created. */
  } else {
    /* Three scopes are treated as "the same" for lookup purposes (based on
       language in WP 6.5.3 [stmt.for] and in 6.4 [stmt.select] para 2-3). */
    curr_ssep = &scope_stack[scope_depth];
    ssep = NULL;
    if (curr_ssep->kind == (a_scope_kind)sck_block) {
      if (curr_ssep->is_loop_scope) {
        /* The current scope is a block scope for a loop (possibly a
           for-loop); check whether the immediately surrounding scope is a
           for-init scope, a case like this:
             for (int i = 0; ; ) { int i; }              // Error
           or else a condition scope that is surrounded by a for-init scope,
           a case like this:
             for (int i = 0; int j = 1; ) { int i; }     // Error
        */
        ssep = curr_ssep - 1;
        if ((ssep-1)->kind == (a_scope_kind)sck_condition) {
          ssep = ssep - 1;
        }  /* if */
      }  /* if */
    } else if (curr_ssep->kind == (a_scope_kind)sck_condition) {
      /* The current scope is a condition scope; see if the immediately
         surrounding scope is a for-init scope -- a case like this:
           for (int i = 0; int i = 1; )                 // Error
      */
      ssep = curr_ssep - 1;
    }  /* if */
    if (ssep != NULL && ssep->is_for_init_block) {
      /* ssep is a for-init scope -- check for a name mismatch. */
      for (sym = (assoc_pointers_block_of(ssep))->symbols;
           sym != NULL;
           sym = sym->next_in_scope) {
        if (sym->header == hdr) {
          match = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return match;
}  /* is_redeclared_for_init_decl_name */


static a_boolean is_redeclared_condition_decl_name(a_symbol_header_ptr  hdr,
                                                   a_scope_depth  scope_depth)
/*
Return TRUE if scope_depth specifies a scope immediately enclosed by a
condition scope and hdr matches the symbol header of a symbol (normally
there's only one) declared in the condition scope.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[scope_depth];
  a_boolean                match = FALSE;
  a_symbol_ptr             sym;

  if (ssep->kind == (a_scope_kind)sck_block &&
      (ssep-1)->kind == (a_scope_kind)sck_condition) {
    for (sym = (assoc_pointers_block_of(ssep-1))->symbols;
         sym != NULL;
         sym = sym->next_in_scope) {
      if (sym->header == hdr) {
        match = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return match;
}  /* is_redeclared_condition_decl_name */


void add_symbol_to_inactive_list(a_symbol_ptr sym_ptr)
/*
Add the given symbol to its symbol header's inactive list.
*/
{
  a_symbol_header_ptr sym_hdr = sym_ptr->header;

#if DEBUG
  if (debug_level >= 4) {
    db_symbol(sym_ptr, "add_symbol_to_inactive_list: ", 2);
  }  /* if */
#endif /* DEBUG */
  check_assertion_str(sym_ptr->kind != (a_symbol_kind)sk_extern_variable &&
                      sym_ptr->kind != (a_symbol_kind)sk_extern_routine,
                      "add_symbol_to_inactive_list: bad symbol kind");
  sym_ptr->next = sym_hdr->inactive_symbols;
  sym_hdr->inactive_symbols = sym_ptr;
}  /* add_symbol_to_inactive_list */


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

Symbols are usually added to the symbol header's active list.  When
a symbol is added to a a sck_namespace_extension scope, however, the
symbol must be added to the inactive list.
*/
{
  register a_symbol_ptr        old_sym_ptr;
  register a_scope_number      scope_number;
  register a_name_space_kind   sym_name_space_kind;
  a_symbol_header_ptr          hdr_ptr = sym_ptr->header;
  a_symbol_ptr                 insert_after;
  a_scope_depth                curr_depth;
  a_boolean                    redecl_err = FALSE;

  if (sym_ptr->is_error) {
    /* Error symbols are never added to the symbol table. */
  } else {
    a_boolean	add_sym_to_inactive_list = FALSE;
#if CHECKING
    if (hdr_ptr == NULL || hdr_ptr == error_symbol_header) {
      internal_error("link_symbol_into_symbol_table: NULL or error header");
    }  /* if */
    check_assertion_str(sym_ptr->kind != (a_symbol_kind)sk_extern_variable &&
                        sym_ptr->kind != (a_symbol_kind)sk_extern_routine,
                        "link_symbol_into_symbol_table: bad symbol kind");
#endif /* CHECKING */
    insert_after = NULL;
    if (scope_depth == NO_SCOPE_DEPTH) {
      /* The symbol is being entered outside of any scope; this happens
         for keywords and command-line -D options, for example.  No error
         check is done. */
      add_sym_to_inactive_list = file_scope_symbols_are_on_inactive_list;
    } else {
      check_assertion_str2(scope_stack[scope_depth].kind !=
                                                   (a_scope_kind)sck_pragma,
                           "link_symbol_into_symbol_table:",
                           "attemping to add symbol to pragma scope");
      if (scope_stack[scope_depth].kind ==
                                      (a_scope_kind)sck_namespace_extension ||
          scope_stack[scope_depth].kind == (a_scope_kind)sck_file) {
        /* Once the initial namespace definition has been closed, additional
           symbols for the namespace are added to the inactive list.  The
           flag in the assoc_pointers_block needs to be tested because it
           is possible for namespace extension scopes to be pushed while the
           initial namespace definition is still in progress.  This is also
           true of the file scope.  File scope symbols are on the inactive list
           once the file scope has been popped for the first time. */
        a_scope_pointers_block_ptr	spbp;
        spbp = assoc_pointers_block_of(&scope_stack[scope_depth]);
        add_sym_to_inactive_list = spbp->add_symbols_to_inactive_list;
      }  /* if */
      if (add_sym_to_inactive_list) {
        /* Symbols entered into namespace extension scopes are added
           directly to the inactive list.  The sequence of symbols on the
           inactive list is not significant. */
        old_sym_ptr = hdr_ptr->inactive_symbols;
        scope_number = scope_stack[scope_depth].number;
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
      }  /* if */
      sym_name_space_kind = name_space_for_symbol_kind[(int)sym_ptr->kind];
      if (!C_mode()) {
        /* See if this is a redeclaration of a for-init or condition variable
           name. */
        if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
          if (is_redeclared_condition_decl_name(hdr_ptr, scope_depth)) {
            /* The name of the variable declared in a condition may not be
               redeclared in the topmost scope of if, switch, while, or for
               statement. */
            if (!suppress_error) {
              pos_st_error(ec_redeclaration_of_condition_decl_name,
                           &(sym_ptr->decl_position),
                           sym_ptr->header->identifier);
            }  /* if */
            redecl_err = TRUE;
          } else if (is_redeclared_for_init_decl_name(hdr_ptr, scope_depth)) {
            /* The name of the variable declared in a for-init may not be
               redeclared in the condition or the topmost scope of the for
               statement. */
            if (!suppress_error) {
              pos_st_error(ec_redeclaration_of_for_init_decl_name,
                           &(sym_ptr->decl_position),
                           sym_ptr->header->identifier);
            }  /* if */
            redecl_err = TRUE;
          }  /* if */
        }  /* if */
        /* See if this name a redeclaration of a template parameter name.
           Redeclarations are permitted in Microsoft mode.  Injected class
           names are ignored because the test will have already been done
           on the declaration of the class in the enclosing scope. */
        if (!redecl_err &&
            (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH ||
             depth_template_declaration_scope != NO_SCOPE_DEPTH) &&
            sym_name_space_kind == nsk_other &&
            sym_ptr->kind != (a_symbol_kind)sk_undefined) {
          an_error_severity severity;
          if (!suppress_error &&
              !microsoft_mode &&
              !is_injected_class_symbol(sym_ptr) &&
              is_redeclared_template_param(sym_ptr, &severity)) {
            if (severity == es_error) {
              /* A template parameter name has been reused in the first scope
                 associated with the instantiation that affects the
                 declarative level (or in a scope nested within that scope,
                 in strict mode). Note that we pass the identifier string to
                 the error routine rather than using the standard symbol name
                 fill-in.  This is done because the variable pointer may not
                 have been filled in at the time the symbol is entered. */
              pos_st_error(ec_redeclaration_of_template_param_name,
                           &(sym_ptr->decl_position),
                           sym_ptr->header->identifier);
              redecl_err = TRUE;
            } else {
              /* A template parameter name has been reused in an inner scope
                 of a template class or function.  Issue a warning that the
                 template parameter will be hidden. */
              pos_st_warning(ec_decl_hides_template_parameter,
                             &(sym_ptr->decl_position),
                             sym_ptr->header->identifier);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (!redecl_err) {
        a_boolean	set_insert_after = TRUE;
        /* See if there is already a definition of this identifier in the same
           scope and name space.  (For name spaces, see C standard, 3.1.2.3.)
           Because of the code above and because the active list is ordered,
           if there are any symbols in the same scope they will be at the
           front of the list.  Note that this test must be done even if
           suppress_error is TRUE, because tag names must still be entered
           behind existing non-tag names in C++.  suppress_error is only TRUE
           when there has already been an error issued, so in practical terms
           the extra check costs nothing.  If the symbol is being entered
           into a namespace extension scope, then the list being scanned is
           the inactive list (and not the active list).  The inactive list
           is not ordered, so we have to scan the entire list looking for
           symbols from the appropriate scope. */
        for (; old_sym_ptr != NULL &&
               (old_sym_ptr->decl_scope == scope_number ||
                add_sym_to_inactive_list);
             old_sym_ptr = old_sym_ptr->next) {
          /* If this is a symbol from another scope (which can only occur
             when adding to a namespace extension scope) skip this symbol. */
          if (old_sym_ptr->decl_scope != scope_number) continue;
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
                             scope_depth, suppress_error)) {
              /* Error, this identifier has already been declared. */
              if (!suppress_error) {
                if (!C_mode() && is_type_symbol(sym_ptr) &&
                    is_type_symbol(old_sym_ptr)) {
                  /* Typedef names can sometimes be redeclared, as long as the
                     underlying type is the same.  That's not the case here. */
                  pos_sy_error(ec_bad_type_name_redeclaration,
                               &(sym_ptr->decl_position),
                               old_sym_ptr);
                } else {
                  /* Note that we pass the identifier string to the error
                     routine rather than using the standard symbol name
                     fill-in. This is done because the variable pointer
                     may not have been filled in at the time the symbol is
                     entered. */
                  pos_st_error(ec_id_already_declared,
                               &(sym_ptr->decl_position),
                               sym_ptr->header->identifier);
                }  /* if */
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
    if (add_sym_to_inactive_list) {
      /* In namespace extension scopes, just add the symbol to the
         inactive list.  The sequence is not significant. */
      add_symbol_to_inactive_list(sym_ptr);
    } else if (insert_after == NULL) {
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
which it is a member (class_type), issue an error.  Except in strict mode,
an exception is made for nonstatic data members in a class with no
constructors (ARM 9.2).  Constructors of named classes are another special
case, but since they are not actually entered into the symbol table, this
routine is not called for them; however, implicitly declared constructors of
unnamed classes are checked for and ignored. Finally, injected class names
are also allowed.
*/
{
  a_symbol_ptr class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  a_boolean    err = FALSE;
  a_field_ptr  fp;

  if (class_sym->header == member_sym->header) {
    /* Member has the same name as the class to which it belongs. */
    /* Except in strict mode, if no constructor already exists, a field is
       allowed to have the same name as its class, as long as it's not an
       anonymous union field being promoted to a containing class with the
       same name. */
    if (member_sym->kind == (a_symbol_kind)sk_field &&
        class_sym->variant.
                    class_struct_union.extra_info->constructor == NULL &&
        ((fp = member_sym->variant.field.ptr) == NULL ||
         member_sym->parent.class_type ==
                                   fp->source_corresp.parent.class_type)) {
        /* Note: the last checks serve to exclude anonymous union promotions.
           It is never the case that the field is not yet bound to the symbol
           when an anonymous union member is being promoted, nor will the
           parent classes correspond. */
    } else if (class_sym->header == unnamed_tag_symbol_header) {
      /* This must be a constructor for an unnamed class. */
    } else if (is_injected_class_symbol(member_sym)) {
      /* Okay. */
    } else {
      /* Error: an identifier that is not a constructor and that has the
         same name as a class is being declared within the class. */
      pos_error(is_function_symbol(member_sym) ?
                     ec_class_and_member_function_name_conflict :
                     ec_class_and_member_name_conflict,
                &member_sym->decl_position);
      err = TRUE;
      member_sym->is_error = TRUE;
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
  a_scope_stack_entry_ptr     ssep;
  a_scope_pointers_block_ptr  pointers_block;
  a_namespace_ptr             nsp;

  if (scope_depth == NO_SCOPE_DEPTH) {
    /* The scope to which this symbol belongs is not on the scope stack. */
    ssep = NULL;
    nsp = sym_ptr->parent.namespace_ptr;
    if (nsp == NULL) {
      /* The symbol is being entered outside of any scope (e.g., a macro
         defined by a command-line -D option). */
      sym_ptr->decl_scope = NO_SCOPE_NUMBER;
      pointers_block = NULL;
    } else {
      /* The symbol belongs to a namespace scope that may not actually be on
         the stack.  This can happen with a friend declaration that causes
         instantiation of a function template that is a namespace member:
           namespace N { template <class T> void f(T); }
           class A { friend void N::f(int); };
      */
      nsp = skip_namespace_aliases(nsp);
      sym_ptr->decl_scope = nsp->variant.assoc_scope->number;
      pointers_block = &((a_symbol_ptr)nsp->source_corresp.assoc_info)->
                            variant.namespace_info.extra_info->pointers_block;
    }  /* if */
  } else {
#if CHECKING
    if (scope_depth < 0 || scope_depth > depth_scope_stack) {
      internal_error("add_symbol_to_scope_list: bad scope depth");
    }  /* if */
#endif /* CHECKING */
    ssep = &scope_stack[scope_depth];
    pointers_block = assoc_pointers_block_of(ssep);
    /* Put the proper scope number into the symbol entry. */
    sym_ptr->decl_scope = ssep->number;
    if (C_dialect == C_dialect_cplusplus && !sym_ptr->is_error) {
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
  if (sym_ptr->is_error) {
    /* Error symbols are not added to the scope list. */
  } else if (pointers_block == NULL) {
    /* A symbol that is not associated with a scope (e.g., a keyword
       or predefined macro).  Keep a special list of such symbols. */
    if (symbols_with_no_scope == NULL) {
      symbols_with_no_scope = sym_ptr;
    } else {
      symbols_with_no_scope_tail->next_in_scope = sym_ptr;
    }  /* if */
    symbols_with_no_scope_tail = sym_ptr;
  } else if (pointers_block != NULL) {
    /* Add the symbol to the end of the symbols list for the scope. */
    if (pointers_block->symbols == NULL) {
      pointers_block->symbols = sym_ptr;
    } else {
      pointers_block->last_symbol->next_in_scope = sym_ptr;
    }  /* if */
    pointers_block->last_symbol = sym_ptr;
  }  /* if */
}  /* add_symbol_to_scope_list */


void set_namespace_projection_symbol(a_symbol_ptr     proj_sym,
                                     a_symbol_ptr     fund_sym,
                                     a_scope_depth    scope_depth)
/*
Initialize the fields of the symbol proj_sym to point to be a namespace
projection symbol that points to fund_sym.  proj_sym must already point to
an sk_namespace_projection symbol. scope_depth is the depth in the scope
stack of the projection symbol.
*/
{
  /* Make sure fund_sym is really a fundamental symbol and not another
     projection. */
  fund_sym = fundamental_symbol_of(fund_sym);
  proj_sym->variant.namespace_projection.fundamental_symbol = fund_sym;
  proj_sym->decl_scope = scope_stack[scope_depth].number;
}  /* set_namespace_projection_symbol */


a_symbol_ptr make_namespace_projection_symbol(a_symbol_ptr       fund_sym,
                                              a_source_position  *pos,
                                              a_scope_depth      scope_depth)
/*
Create a namespace projection symbol and set it to point to fund_sym.  pos
is the source position to be associated with the projection symbol, and
scope_depth is its depth in the scope stack.
*/
{
  a_symbol_ptr	sym;

  sym = alloc_symbol((a_symbol_kind)sk_namespace_projection,
                     fund_sym->header, pos);
  set_namespace_projection_symbol(sym, fund_sym, scope_depth);
  set_decl_sequence_number(sym);
  return sym;
}  /* make_namespace_projection_symbol */


a_symbol_ptr enter_namespace_projection_symbol(a_symbol_ptr     fund_sym,
                                               a_symbol_locator *location,
                                               a_scope_depth    scope_depth,
                                               a_boolean        suppress_error)
/*
Create a namespace projection symbol, set it to point to fund_sym,
and enter it in the symbol table.  synthesized is TRUE if this is a
synthesized namespace projection symbol.  This routine is like enter_symbol,
but it is only used to create namespace projection symbols.  enter_symbol
should not be used to create namespace projection symbols because the
fundamental symbol pointer must be set before link_symbol_into_symbol_table
is called.
*/
{
  a_symbol_ptr	sym_ptr;

  sym_ptr = make_namespace_projection_symbol(fund_sym,
                                             &location->source_position,
                                             scope_depth);
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
  return sym_ptr;
}  /* enter_namespace_projection_symbol */


void add_friend_function_to_lookup_list_for_class(a_symbol_ptr  rout_sym,
                                                  a_type_ptr    class_type)
/*
rout_sym represents a non-class-member function that has been declared a
friend of the specified class.  Enter it on a list that is used by
in argument dependent lookup.  
*/
{
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym, other_sym, overload_sym = NULL;
  a_boolean                      duplicate = FALSE, is_list;

  /* Check for other functions with the same name that have been declared
     friends of the current class. */
  cssp = symbol_supplement_for_class(class_type);
  for (other_sym = cssp->friend_functions;
       other_sym != NULL;
       other_sym = other_sym->next) {
    if (other_sym->header == rout_sym->header) break;
  }  /* for */
  if (other_sym != NULL) {
    /* Ignore this symbol if it's a duplicate. */
    sym = other_sym;
    is_list = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    if (is_list) {
      /* An overload set already exists. */
      overload_sym = sym;
      sym = overload_sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = (is_list ? sym->next : NULL)) {
      if (fundamental_symbol_of(sym) == rout_sym) {
        duplicate = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!duplicate) {
    /* Create a namespace projection symbol (one that won't actually be
       entered in the symbol table) to point to the friend function symbol. */
    sym = make_namespace_projection_symbol(rout_sym,
                                           &rout_sym->decl_position,
                                           depth_innermost_namespace_scope);
    /* Now add the projection symbol to the class symbol supplement. */
    if (other_sym == NULL) {
      /* No overloading -- add it directly. */
      sym->next = cssp->friend_functions;
      cssp->friend_functions = sym;
    } else if (overload_sym != NULL) {
      /* An overload set already exists.  Add the new symbol to the overload
         set. */
      sym->next = overload_sym->variant.overloaded_function.symbols;
      overload_sym->variant.overloaded_function.symbols = sym;
      sym->overload_set_member = TRUE;
    } else {
      /* An overload set will have to be created.  First remove the other
         symbol from the main list; it will be added to the overload set
         later. */
      if (cssp->friend_functions == other_sym) {
        cssp->friend_functions = other_sym->next;
      } else {
        a_symbol_ptr  prev = cssp->friend_functions;
        while (prev->next != other_sym) prev = prev->next;
        prev->next = other_sym->next;
      }  /* if */
      other_sym->next = NULL;
      /* Create a symbol for the overload set. */
      overload_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                   sym->header, &other_sym->decl_position);
      overload_sym->decl_scope = sym->decl_scope;
      /* Add the two symbols to the overload set. */
      overload_sym->variant.overloaded_function.symbols = sym;
      sym->overload_set_member = TRUE;
      sym->next = other_sym;
      other_sym->overload_set_member = TRUE;
      /* Add the overload set to the list. */
      overload_sym->next = cssp->friend_functions;
      cssp->friend_functions = overload_sym;
    }  /* if */
  }  /* if */
}  /* add_friend_function_to_lookup_list_for_class */


static
a_scope_pointers_block_ptr pointers_block_for_namespace(a_namespace_ptr nsp)
/*
Return a pointer to the scope pointers block for nsp.  If nsp is NULL, return
the scope pointers block for the global scope.
*/
{
  a_scope_pointers_block_ptr	result;

  if (nsp == NULL) {
    result = assoc_pointers_block_of(&scope_stack[DEPTH_OF_FILE_SCOPE]);
  } else {
    result = &symbol_supplement_for_namespace(nsp)->pointers_block;
  }  /* if */
  return result;
}  /* pointers_block_for_namespace */


a_symbol_ptr enter_synthesized_projection_symbol(
                               a_symbol_ptr		fund_sym,
                               a_symbol_locator		*location,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options)
/*
Create a synthesized namespace projection symbol, set it to point to fund_sym,
add it to the "other" symbols list of the symbol header, and the
synthesized namespace projections symbols list of the scope it is being
added to.

qualified_lookup is TRUE if the symbol being created is the result of
a namespace or file scope qualified lookup.  For qualified lookups
qualifier_namespace points to the namespace in which the lookup is
being done, or is NULL for a file scope lookup.  options specifies
the options being used for the lookup.
*/
{
  a_symbol_ptr			sym_ptr;
  a_symbol_header_ptr		sym_hdr;
  a_scope_pointers_block_ptr	pointers_block;
  a_boolean			can_be_reused;

  sym_ptr = make_namespace_projection_symbol(fund_sym,
                                             &location->source_position,
                                             depth_scope_stack);
  sym_ptr->is_error = location->is_error;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = qualified_lookup;
  /* Synthesized projection symbols are not entered into the symbol table.
     They are put on the "other" symbols list and are added to a separate
     list in the scope stack. */
  sym_hdr = sym_ptr->header;
  /* For a qualified lookup, add the symbol to the scope in which the
     name is being looked up.  For unqualified lookups, add it to the
     current scope. */
  if (qualified_lookup) {
    pointers_block = pointers_block_for_namespace(qualifier_namespace);
  } else {
    pointers_block = assoc_pointers_block_of(&scope_stack[depth_scope_stack]);
  }  /* if */
  sym_ptr->next_in_scope = pointers_block->synth_namespace_projection_symbols;
  pointers_block->synth_namespace_projection_symbols = sym_ptr;
  /* See if the specified lookup options represent a reusable lookup. */
  can_be_reused = is_reusable_using_directive_lookup(options);
  if (can_be_reused) {
    /* Only reusable symbols are entered on the "other" symbols list. */
    sym_ptr->next = sym_hdr->other_symbols;
    sym_hdr->other_symbols = sym_ptr;
  }  /* if */
  /* Set the flags that describe the kind of that created this symbol. */
  sym_ptr->synthesized_namespace_projection = TRUE;
  sym_ptr->do_not_reuse = !can_be_reused;
  sym_ptr->qualified_lookup = qualified_lookup;
  if (qualified_lookup && qualifier_namespace != NULL) {
    set_namespace_membership(sym_ptr, (a_source_correspondence*)NULL,
                             qualifier_namespace);
  }  /* if */
  if (qualified_lookup) {
    /* Set the decl_scope of the symbol to the scope number associated
       with the qualifiers namespace. */
    if (qualifier_namespace == NULL) {
      sym_ptr->decl_scope = scope_stack[DEPTH_OF_FILE_SCOPE].number;
    } else {
      sym_ptr->decl_scope = qualifier_namespace->variant.assoc_scope->number;
    }  /* if */
  }  /* if */
  sym_ptr->must_be_class_or_namespace_lookup =
                              (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
  sym_ptr->must_be_tag_lookup = (options & IDL_MUST_BE_TAG) != 0;
  sym_ptr->tentative_type_lookup = (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
  sym_ptr->must_be_class_lookup = (options & IDL_MUST_BE_CLASS) != 0;
  sym_ptr->must_be_namespace_lookup = (options & IDL_MUST_BE_NAMESPACE) != 0;
  return sym_ptr;
}  /* enter_synthesized_projection_symbol */


static void copy_symbol_lookup_flags(a_symbol_ptr from,
                                     a_symbol_ptr to)
/*
Copy the lookup flags for synthesized namespace projection symbols from
the symbol pointed to by "from" to the symbol pointed to by "to".
*/
{
  to->synthesized_namespace_projection
                                     = from->synthesized_namespace_projection;
  to->qualified_lookup               = from->qualified_lookup;
  to->must_be_class_or_namespace_lookup
                                     = from->must_be_class_or_namespace_lookup;
  to->must_be_tag_lookup             = from->must_be_tag_lookup;
  to->do_not_reuse                   = from->do_not_reuse;
  to->must_be_class_lookup           = from->must_be_class_lookup;
  to->must_be_namespace_lookup       = from->must_be_namespace_lookup;
}  /* copy_symbol_lookup_flags */


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

If this routine is changed, enter_namespace_projection_symbol may need to
be changed too.
*/
{
  register a_symbol_ptr sym_ptr;

  db_enter(4, "enter_symbol");

  /* Allocate and initialize the symbol. */
  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->is_error = location->is_error;
  if (sym_ptr->is_error) {
    /* In the error case it can be useful to remember the membership (e.g.,
       to recognize constructor-like symbols). */
    sym_ptr->is_class_member = location->is_class_member;
    sym_ptr->parent = location->parent;
  }  /* if */
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


a_symbol_ptr enter_extern_symbol(a_symbol_kind    sym_kind,
                                 a_symbol_locator *locator)
/*
Enter an sk_extern_variable or sk_extern_routine symbol into the symbol
table.  Note that these symbols are put on the "other" symbols list -- they
are not actually put onto the active list, and they are not found during
normal symbol lookup.
*/
{
  a_symbol_header_ptr  header = locator->symbol_header;
  a_symbol_ptr         sym;
  a_boolean            err;
  a_namespace_ptr      nsp;

  db_enter(4, "enter_extern_symbol");
  sym = alloc_symbol(sym_kind, header, &locator->source_position);
  /* Extern symbol entries are associated with the file scope of the current
     translation units.  This allows find_external_symbol to discard extern
     symbols created in other file scopes. */
  sym->decl_scope = file_scope_number;
  if (is_error_locator(*locator)) {
    sym->is_error = TRUE;
  } else {
    /* Just add the entry to the front of the list. */
    sym->next = header->other_symbols;
    header->other_symbols = sym;
  }  /* if */
  if (!C_mode()) {
    /* See if this symbol is directly or indirectly a namespace member. */
    nsp = qualifier_namespace_ptr(*locator);
    if (nsp == NULL &&
        depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
      nsp = scope_stack[depth_innermost_namespace_scope].
                                  il_scope->variant.assoc_namespace;
    }  /* if */
    /* Set namespace membership, if required. */
    if (nsp != NULL) {
      set_namespace_membership(sym, (a_source_correspondence *)NULL, nsp);
    }  /* if */
  }  /* if */
  /* Add the symbol to the file scope's symbol list, for checking when the
     file scope is popped.  (Note: we do not add sk_extern_variable and
     sk_extern_routine symbols to the symbol table proper.) */
  add_symbol_to_scope_list(sym, DEPTH_OF_FILE_SCOPE, &err);
  db_exit();
  return sym;
}  /* enter_extern_symbol */


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


void relink_unnamed_tag_symbol(a_symbol_ptr      sym,
                               a_symbol_locator  *locator)
/*
A name is belatedly specified for a class, and so the tag symbol originally
created for it must be modified to bear the new name.  Give the symbol
the new name and relink it into the symbol table under the new header.
*/
{
  db_enter(4, "relink_unnamed_tag_symbol");
#if CHECKING
  /* The symbol should not have been linked onto the symbol list for its
     header. */
  if (sym->header != unnamed_tag_symbol_header) {
    internal_error("relink_unnamed_tag_symbol: unexpected symbol header");
  }  /* if */
  /* The declaration scope should not be changed. */
  if (scope_stack[decl_scope_level].number != sym->decl_scope) {
    internal_error("relink_unnamed_tag_symbol: bad scope");
  }  /* if */
#endif /* CHECKING */
  /* Replace the special symbol header for unnamed class symbols with the
     header associated with its new name. */
  sym->header = locator->symbol_header;
  /* Add the symbol to the symbol table. */
  reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
  db_exit();
}  /* relink_unnamed_tag_symbol */


void enter_undefined_symbol(a_symbol_ptr sym)
/*
The indicated symbol is an sk_undefined symbol created because of an
undefined identifier.  It is now known that this is an error.  Enter
the symbol into the symbol table so it can be found on subsequent
uses of the name.
*/
{
  /* Note that the is_error flag is not set on this symbol.  Error
     symbols are not entered into the symbol table, but undefined
     symbols need to be (for error recovery purposes). */
  reenter_symbol(sym, decl_scope_level, /*suppress_error=*/TRUE);
}  /* enter_undefined_symbol */


a_symbol_ptr enter_undefined_member_symbol(a_symbol_locator *locator)
/*
Enter a symbol for an undefined class member, or find an existing one, and
return a pointer to the symbol.  This is used for invalid member references
so that there is a symbol against which a reference can be recorded.
*locator gives the necessary information about the member name and position.
*/
{
  a_symbol_ptr sym_ptr;

  db_enter(4, "enter_undefined_member_symbol");
  /* Look for an existing sk_undefined symbol on the inactive list. */
  for (sym_ptr = inactive_symbol_list_from_locator(*locator);
       sym_ptr != NULL;
       sym_ptr = sym_ptr->next) {
    if (sym_ptr->kind == (a_symbol_kind)sk_undefined &&
        sym_ptr->decl_scope == NO_SCOPE_NUMBER) {
      /* Found one. */
      break;
    }  /* if */
  }  /* for */
  if (sym_ptr == NULL ) {
    /* Allocate and initialize the symbol. */
    a_symbol_header_ptr sym_hdr = locator->symbol_header;
    sym_ptr = alloc_symbol((a_symbol_kind)sk_undefined, sym_hdr,
                           &locator->source_position);
    sym_ptr->is_error = TRUE;
    /* Add the symbol to the front of the inactive list. */
    add_symbol_to_inactive_list(sym_ptr);
  }  /* if */
  db_exit();
  return sym_ptr;
}  /* enter_undefined_member_symbol */


a_symbol_ptr add_symbol_to_overload_list(a_symbol_ptr    new_sym,
                                         a_symbol_ptr    other_sym,
                                         a_boolean       use_namespace,
                                         a_namespace_ptr ns_ptr)
/*
new_sym is a newly created function (or function template) symbol that
shares a name with other_sym, which is either an overloaded function symbol
or another function or function template symbol.  If necessary, create an
overloaded function symbol and add other_sym to its list.  Add new_sym to
the new or existing list of overloaded functions, and return a pointer to
the overloaded function symbol.

If other_sym is not already an sk_overloaded_function symbol, it
may need to be removed from the symbol header and scope stack lists
and replaced with the newly created overloaded function symbol.
Normally, the scope list is found by looking through the scope stack
for the decl_scope of other_sym.  However, if use_namespace
is TRUE, the pointers block associated with the namespace pointed to
by ns_ptr is used instead.  If ns_ptr is NULL, the pointers block for
the file scope is used.
*/
{
  a_symbol_ptr        overload_sym, prev_sym_ptr;
  a_symbol_header_ptr hdr_ptr;
  a_scope_stack_entry *ssep;
  a_scope_pointers_block_ptr  pointers_block;

  
  if (other_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    overload_sym = other_sym;
    other_sym = overload_sym->variant.overloaded_function.symbols;
  } else {
    /* The existing symbol is not an sk_overloaded_function symbol
       (i.e., it's a simple function symbol of some kind). */
    if (!use_namespace) {
      /* Find the scope stack entry associated with this declaration.
         depth_scope_stack is used instead of decl_scope_level because
         the previous "declaration" may actually be a synthesized 
         namespace projection symbol that can be created in any scope,
         not just a declarative scope. */
      ssep = &scope_stack[depth_scope_stack];
      /* If the scope stack entry for the overloaded function is not that of
         the current scope (e.g., when a friend declaration refers to a 
         function at file scope), find the correct one. */
      while (ssep->number != other_sym->decl_scope) {
#if CHECKING
        if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) {
          internal_error("enter_overloaded_symbol: scope stack overrun error");
        }  /* if */
#endif /* CHECKING */
        --ssep;
      }  /* if */
      pointers_block = assoc_pointers_block_of(ssep);
    } else {
      /* A namespace pointer was passed by the caller.  Use the pointers
         block associated with this namespace. */
      pointers_block = pointers_block_for_namespace(ns_ptr);
    }  /* if */
    /* Create an sk_overloaded_function symbol and attach the old
       function symbol to it. */
    hdr_ptr = other_sym->header;
    overload_sym = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                   hdr_ptr, &(other_sym->decl_position));
    overload_sym->decl_scope = other_sym->decl_scope;
    overload_sym->decl_seq = other_sym->decl_seq;
    /* If the symbol is a member of a class or namespace, set the membership
       of the new symbol. */
    if (other_sym->is_class_member) {
      set_class_membership(overload_sym, (a_source_correspondence *)NULL,
                           other_sym->parent.class_type);
    } else if (other_sym->parent.namespace_ptr != NULL) {
      set_namespace_membership(overload_sym,  (a_source_correspondence *)NULL,
                               other_sym->parent.namespace_ptr);
    }  /* if */
    /* Synthesized projection symbols marked "do not reuse" are not on
       any of the symbol header lists, so don't try to fine them. */
    if (!other_sym->do_not_reuse) {
      /* Put overload_sym into the symbol list in place of other_sym.
         This will normally use the active symbol list, but may use the
         inactive list for namespace extensions.  If other_sym is a
         synthesized namespace projection symbol, it will be on the
         "other" symbols list. */
      /* Find the symbol preceding other_sym on its list. */
      if (other_sym->synthesized_namespace_projection) {
        prev_sym_ptr = hdr_ptr->other_symbols;
        if (prev_sym_ptr == other_sym) hdr_ptr->other_symbols = overload_sym;
      } else if (pointers_block->add_symbols_to_inactive_list) {
        prev_sym_ptr = hdr_ptr->inactive_symbols;
        if (prev_sym_ptr == other_sym) {
          hdr_ptr->inactive_symbols = overload_sym;
        }  /* if */
      } else {
        prev_sym_ptr = hdr_ptr->symbol;
        if (prev_sym_ptr == other_sym) hdr_ptr->symbol = overload_sym;
      }  /* if */
      if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the header list.  This case is handled
           above. */
      } else {
        while (prev_sym_ptr != NULL && prev_sym_ptr->next != other_sym) {
          prev_sym_ptr = prev_sym_ptr->next;
        }  /* while */
        check_assertion_str2(prev_sym_ptr != NULL,
                             "add_symbol_to_overload_list:",
                             "symbol not in symbol header list");
        prev_sym_ptr->next = overload_sym;
      }  /* if */
      overload_sym->next = other_sym->next;
      other_sym->next = NULL;
    }  /* if */
    /* Also put overload_sym into the scope list in place of other_sym. */
    if (other_sym->synthesized_namespace_projection) {
      prev_sym_ptr = pointers_block->synth_namespace_projection_symbols;
      if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the scope's symbol list. */
        pointers_block->synth_namespace_projection_symbols = overload_sym;
      }  /* if */
      /* Transfer any symbol lookup flags that are set to the newly
         created overloaded function symbol. */
      copy_symbol_lookup_flags(other_sym, overload_sym);
    } else {
      prev_sym_ptr = pointers_block->symbols;
      if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the scope's symbol list. */
        pointers_block->symbols = overload_sym;
      }  /* if */
    }  /* if */
    if (prev_sym_ptr == other_sym) {
        /* The entry is the first on the list.  This case is handled above. */
    } else {
       while (prev_sym_ptr != NULL &&
              prev_sym_ptr->next_in_scope != other_sym) {
         prev_sym_ptr = prev_sym_ptr->next_in_scope;
       }  /* while */
      check_assertion_str2(prev_sym_ptr != NULL,
                           "add_symbol_to_overload_list:",
                           "symbol not in scope stack list");
       prev_sym_ptr->next_in_scope = overload_sym;
    }  /* if */
    overload_sym->next_in_scope = other_sym->next_in_scope;
    other_sym->next_in_scope = NULL;
    if (pointers_block->last_symbol == other_sym) {
      pointers_block->last_symbol = overload_sym;
    }  /* if */
    /* Attach the old symbol under the overloaded symbol. */
    overload_sym->variant.overloaded_function.symbols = other_sym;
    other_sym->overload_set_member = TRUE;
  }  /* if */
  /* Attach the new symbol to the front of the list under the overloaded
     symbol. */
  new_sym->next = overload_sym->variant.overloaded_function.symbols;
  overload_sym->variant.overloaded_function.symbols = new_sym;
  new_sym->overload_set_member = TRUE;
  /* Return a pointer to the sk_overloaded_function symbol. */
  return overload_sym;
}  /* add_symbol_to_overload_list */


a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                     a_symbol_locator *location,
                                     a_boolean        is_constructor,
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
  a_boolean	      use_namespace;
  a_namespace_ptr     ns_ptr = NULL;

  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->decl_scope = other_sym->decl_scope;
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  /* Check for the obscure case in which a non-constructor is being added
     to an overload set of constructors. */
  if (other_sym->is_class_member && !is_constructor &&
      is_constructor_symbol(other_sym)) {
    pos_error(ec_class_and_member_function_name_conflict,
              &location->source_position);
    set_to_error_locator(*location);
    sym_ptr->is_error = TRUE;
    *overload_sym = NULL;
  } else {
    use_namespace = !other_sym->is_class_member &&
                                      other_sym->parent.namespace_ptr != NULL;
    if (use_namespace) ns_ptr = other_sym->parent.namespace_ptr;
    /* Add the symbol to the overloaded function list. */
    *overload_sym = 
            add_symbol_to_overload_list(sym_ptr, other_sym, use_namespace,
                                        ns_ptr);
  }  /* if */
  /* Return a pointer to the newly created symbol as well. */
  return sym_ptr;
}  /* enter_overloaded_symbol */


a_base_class_ptr find_base_with_type(a_type_ptr        base_type,
                                     a_type_ptr        class_type,
                                     a_base_class_ptr  ref_bcp)
/*
Determine the base class of class_type whose type it base_type and whose
derivation path to class_type goes through ref_bcp.
*/
{
  a_base_class_ptr  result = NULL, bcp = base_classes_of(class_type);

  check_assertion(ref_bcp->direct || ref_bcp->is_virtual);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->type == base_type) {
      /* ref_bcp is the root of a path to the base class where the inherited
         name was found.  Be sure ref_bcp is also on the path to bcp before
         deciding that bcp is the base class containing sym.  Note that if the
         inheritance is ambiguous there may be several base classes that match
         the type in question, and there may be more than one for which
         ref_bcp is on the path. */
      if (!bcp->ambiguous || ref_bcp == bcp ||
          is_on_any_derivation_of(bcp, ref_bcp)) {
        result = bcp;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}


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
  register a_symbol_ptr   sym;
  a_projection_descr_ptr  pdp, progenitor_pdp = NULL;
  a_base_class_ptr        bcp;

  db_enter(4, "make_projection_symbol");

  /* Allocate and initialize the symbol. */
  sym = alloc_symbol((a_symbol_kind)sk_projection, progenitor_sym->header,
                     &progenitor_sym->decl_position);
  set_class_membership(sym, (a_source_correspondence *)NULL, class_ptr);
  sym->decl_scope = class_ptr->variant.class_struct_union.
                                    extra_info->assoc_scope->number;
  sym->ambiguous = ambiguous;
  pdp = sym->variant.projection.extra_info;
  if (progenitor_sym->kind == (a_symbol_kind)sk_projection) {
    /* The "progenitor" of this new projection symbol is itself a projection
       symbol. */
    progenitor_pdp = progenitor_sym->variant.projection.extra_info;
    pdp->fundamental_symbol = progenitor_pdp->fundamental_symbol;
    /* Set the flag indicating whether there are any intervening access
       declarations in the inheritance path. */
    if (progenitor_sym->variant.projection.is_using_decl ||
        progenitor_sym->variant.projection.any_intervening_using_decl) {
      sym->variant.projection.any_intervening_using_decl = TRUE;
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
    a_type_ptr  tp = pdp->fundamental_symbol->parent.class_type;
    bcp = class_ptr->variant.class_struct_union.extra_info->base_classes;
    if (!ambiguous) {
      /* There is no ambiguity in the use of this name, so a simple type match
         is enough to identify the base class of the fundamental symbol. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->type == tp) {
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
      pdp->fundamental_base_class = find_base_with_type(tp, class_ptr,
                                                        ref_bcp);
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


static
a_symbol_header_ptr symbol_header_for_conversion_function(a_type_ptr type)
/*
Look up the symbol header for a given conversion function.  If there
is none, create a new one.
*/
{
  a_conversion_header_ptr  conv_hdr;
  a_conversion_header_ptr  prev_conv_hdr;
  a_symbol_header_ptr      sym_hdr;
  char                     *name;
  sizeof_t                 name_length;
#define OPERATOR_LEN 9 /* Length of "operator " */

  /* Search the conversion header list for an entry of the required type.
     If one is found, it is moved to the front of the list. */
  prev_conv_hdr = NULL;
  conv_hdr = conversion_header_list;
  for (; conv_hdr != NULL; conv_hdr = conv_hdr->next) {
    if (types_are_strictly_compatible(type, conv_hdr->type)) {
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
    name = format_type_string(type, &name_length);
    sym_hdr->identifier_length = (sizeof_t)OPERATOR_LEN + name_length;
    sym_hdr->identifier = alloc_il(sym_hdr->identifier_length + 1);
    (void)memcpy(sym_hdr->identifier, "operator ", OPERATOR_LEN);
    (void)strcpy((sym_hdr->identifier + OPERATOR_LEN), name);
#if DEBUG
    symbol_name_string_space += sym_hdr->identifier_length;
#endif /* DEBUG */
  }  /* if */
  return conv_hdr->symbol_header;
#undef OPERATOR_LEN
}  /* symbol_header_for_conversion_function */


static a_symbol_ptr make_parameter_symbol(a_symbol_locator  *locator)
/*
Create but do not yet enter an sk_parameter symbol.  This routine is called
for old style parameter declarations.
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


a_symbol_ptr make_template_class_symbol(a_symbol_ptr  ct_symbol)
/*
Create a symbol for an instance of a class template.  Link the symbol to
the class template symbol but do not enter it into the symbol table.
ct_symbol is the symbol of the class template.
*/
{
  a_symbol_ptr 				sym;
  a_symbol_kind 			kind;
  a_class_symbol_supplement_ptr		cssp;

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
  /* Create the symbol.  Use the position of the template declaration as its
     declaration position. */
  sym = alloc_symbol(kind, ct_symbol->header, &ct_symbol->decl_position);
  /* Set the pointer that points back to the original class template symbol. */
  cssp = sym->variant.class_struct_union.extra_info;
  cssp->class_template = ct_symbol;
  /* Make the declaration scope the same as the class template's. */
  sym->decl_scope = ct_symbol->decl_scope;
  /* Set the new symbol to have the same class or namespace membership as
     the template from which it was created. */
  if (ct_symbol->is_class_member) {
    set_class_membership(sym, (a_source_correspondence *)NULL,
                         ct_symbol->parent.class_type);
  } else if (ct_symbol->parent.namespace_ptr != NULL) {
    set_namespace_membership(sym, (a_source_correspondence *)NULL,
                             ct_symbol->parent.namespace_ptr);
  }  /* if */
  return sym;
}  /* make_template_class_symbol */


a_symbol_ptr make_function_template_prototype_symbol(
				a_symbol_ptr		template_sym,
				a_routine_ptr		rout_ptr,
				a_template_param_ptr	templ_param_list)
/*
Create the symbol for the prototype instantiation of a function template.
Return the newly created symbol.  template_sym points to the function
template symbol.  rout_ptr points to the routine entry for the prototype
instantiation.
*/
{
  a_symbol_kind			kind;
  a_symbol_ptr			sym;
  a_template_instance_ptr	tip;

  /* If the template is a class member make the prototype instantiation
     a member function, otherwise make it a normal routine. */
  kind = template_sym->is_class_member ? (a_symbol_kind)sk_member_function
                                       : (a_symbol_kind)sk_routine;
  sym = alloc_symbol(kind, template_sym->header, &template_sym->decl_position);
  tip = alloc_template_instance();
  tip->template_sym = template_sym;
  tip->instance_sym = sym;
  sym->variant.routine.instance_ptr = tip;
  sym->variant.routine.ptr = rout_ptr;
  sym->is_class_member = template_sym->is_class_member;
  sym->parent = template_sym->parent;
  /* Create the template argument list for the prototype routine. */
  rout_ptr->template_arg_list = create_prototype_arg_list(templ_param_list);
  rout_ptr->is_prototype_instantiation = TRUE;
  rout_ptr->is_template_function = TRUE;
  return sym;
}  /* make_function_template_prototype_symbol */


a_symbol_ptr make_template_function_symbol(a_symbol_ptr       templ_sym,
                                           a_source_position  *pos,
                                           a_type_ptr         conv_type)
/*
Create a symbol for a template function.  Do not enter it into the symbol
table, since it is accessed from the associated function instantiation entry.
conv_type is the return type of the function being created.  When the
function is a conversion function, conv_type is used to generate the name
of the instance symbol.  For example, the template may be called "operator T",
but the instance needs to be called "operator int".
*/
{
  a_symbol_ptr  	sym;
  a_symbol_header_ptr	sym_hdr;

  /* Determine which symbol header should be used for this symbol.  This
     is usually the same symbol header as the template.  But for conversion
     operators, a new name must be generated based on the type. */
  if (is_conversion_function_symbol(templ_sym)) {
    sym_hdr = symbol_header_for_conversion_function(conv_type);
  } else {
    sym_hdr = templ_sym->header;
  }  /* if */
  sym = alloc_symbol((a_symbol_kind) (templ_sym->is_class_member
                                          ? (a_symbol_kind)sk_member_function
                                          : (a_symbol_kind)sk_routine),
                     sym_hdr, pos);
  /* Template functions will be in the same scope as the template (which
     should always be the file scope. */
  sym->decl_scope = templ_sym->decl_scope;;
  /* Set the new symbol to have the same class or namespace membership as
     the template from which it was created. */
  if (templ_sym->is_class_member) {
    set_class_membership(sym, (a_source_correspondence *)NULL,
                         templ_sym->parent.class_type);
  } else if (templ_sym->parent.namespace_ptr != NULL) {
    set_namespace_membership(sym, (a_source_correspondence *)NULL,
                             templ_sym->parent.namespace_ptr);
  }  /* if */
  return sym;
}  /* make_template_function_symbol */


a_symbol_ptr error_class_template(void)
/*
Return a pointer to an error class template.
*/
{
  if (error_class_template_symbol == NULL) {
    /* Create a class template symbol for this template template parameter. */
    a_symbol_ptr			sym;
    a_template_symbol_supplement_ptr	tssp;
    a_template_ptr			templ_ptr;
    sym = alloc_symbol((a_symbol_kind)sk_class_template,
                       (a_symbol_header_ptr)NULL, &null_source_position);
    sym->is_error = TRUE;
    sym->is_template_param = TRUE;
    tssp = sym->variant.template_info;
    templ_ptr = alloc_template();
    templ_ptr->template_info = tssp;
    set_source_corresp(&templ_ptr->source_corresp, sym);
    templ_ptr->kind = (a_template_kind)templk_template_template_param;
    /* Note that this is not marked as a nonreal member.  In this way,
       instances based on an error class template will not necessarily
       be nonreal. */
    tssp->variant.class_template.type_kind = (a_type_kind)tk_class;
    tssp->il_template_entry = templ_ptr;
    tssp->is_error = TRUE;
    {
      /* Create a template_decl_info entry for the error template for
         error recovery purposes. */
      a_template_decl_info_ptr	    template_decl_info;
      template_decl_info = alloc_template_decl_info();
      tssp->cache.decl_info = template_decl_info;
    }
    error_class_template_symbol = sym;
  }  /* if */
  return error_class_template_symbol;
}  /* error_class_template */


a_template_symbol_supplement_ptr template_supplement_for_template(
						a_template_ptr	templ_ptr)
/*
Given an IL template entry, return the template symbols supplement.  If
the template entry has a template_info field, use that.  Otherwise, get
the template supplement information from the associated symbol.  Nonreal
templates and template template parameter templates have a template_info
field.
*/
{
  a_symbol_ptr				sym;
  a_template_symbol_supplement_ptr	tssp;

  tssp = templ_ptr->template_info;
  if (tssp == NULL) {
    sym = (a_symbol_ptr)templ_ptr->source_corresp.assoc_info;
    tssp = template_supplement_for_symbol(sym);
  }  /* if */
  return tssp;
}  /* template_supplement_for_template */


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
  if (!rout_sym->parent.class_type->
                                 variant.class_struct_union.is_nonreal_class) {
    internal_error("make_member_function_template_symbol: real class member");
  }  /* if */
#endif /* CHECKING */
  tip = rout_sym->variant.routine.instance_ptr;
  if (tip != NULL) {
    template_sym = tip->template_sym;
  } else {
    /* Note that the function template symbol is not entered in the symbol
       table, since it need only be accessed through the corresponding
       member function symbol rout_sym. */
    template_sym = alloc_symbol((a_symbol_kind)sk_function_template,
                                rout_sym->header, &rout_sym->decl_position);
    template_sym->is_class_member = TRUE;
    template_sym->parent.class_type = rout_sym->parent.class_type;
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


a_symbol_ptr make_unnamed_tag_symbol(a_symbol_kind      sym_kind,
                                     a_source_position  *pos)
/*
Create a symbol for a tagless class, struct, or union symbol.  Do not enter
it into the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(4, "make_unnamed_tag_symbol");
  /* Use the unnamed tag symbol header.  Allocate it if necessary. */
  if (unnamed_tag_symbol_header == NULL) {
    unnamed_tag_symbol_header = alloc_symbol_header();
    unnamed_tag_symbol_header->identifier = "<unnamed>";
    unnamed_tag_symbol_header->identifier_length = 9;
  }  /* if */
  sym = alloc_symbol(sym_kind, unnamed_tag_symbol_header, pos);
  sym->decl_scope = scope_stack[decl_scope_level].number;
  db_exit();
  return sym;
}  /* make_unnamed_tag_symbol */


a_boolean is_unnamed_tag_symbol(a_symbol_ptr  sym)
/*
Return TRUE if sym represents an unnamed class type.
*/
{
  return (sym->header == unnamed_tag_symbol_header);
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


a_symbol_ptr make_unnamed_namespace_symbol(a_source_position  *pos)
/*
Create a symbol for an unnamed namespace.  Do not enter it into the symbol
table.
*/
{
  a_symbol_ptr  sym;

  /* Use the unnamed namespace symbol header.  Allocate it if necessary. */
  if (unnamed_namespace_symbol_header == NULL) {
    unnamed_namespace_symbol_header = alloc_symbol_header();
    unnamed_namespace_symbol_header->identifier = "<unnamed>";
    unnamed_namespace_symbol_header->identifier_length = 9;
  }  /* if */
  sym = alloc_symbol((a_symbol_kind)sk_namespace,
                     unnamed_namespace_symbol_header, pos);
  sym->decl_scope = scope_stack[depth_scope_stack].number;
  return sym;
}  /* make_unnamed_namespace_symbol */


static a_symbol_header_ptr make_unnamed_symbol_header(void)
/*
Return a unique unnamed symbol header.
*/
{
  a_symbol_header_ptr	sym_hdr;

  sym_hdr = alloc_symbol_header();
  sym_hdr->identifier = "<unnamed>";
  sym_hdr->identifier_length = 9;
  return sym_hdr;
}  /* make_unnamed_symbol_header */


a_symbol_ptr make_unnamed_template_param_symbol(a_symbol_kind		kind,
						a_source_position	*pos)
/*
Create a symbol for an unnamed template parameter.  Such symbols are not
entered into the symbol table.  Each unnamed template parameter is given
a unique symbol header.
*/
{
  a_symbol_ptr		sym;

  sym = alloc_symbol(kind, make_unnamed_symbol_header(), pos);
  sym->decl_scope = scope_stack[decl_scope_level].number;
  return sym;
}  /* make_unnamed_template_param_symbol */


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


void enter_keyword(a_token_kind token,
                   char         *keyword)
/*
Enter a keyword.  keyword is the keyword string, token is the lexical
token that corresponds to it.
*/
{
  register a_symbol_ptr sym_ptr;

  sym_ptr = full_enter_symbol(keyword, (sizeof_t)(strlen(keyword)),
			      (a_symbol_kind)sk_keyword, NO_SCOPE_DEPTH);
  sym_ptr->variant.keyword.token = (a_byte_token_kind)token;
}  /* enter_keyword */


void enter_injected_class_name_symbol(a_symbol_ptr  tag_sym)
/*
Allocate a symbol to represent an injected class-name for the class
specified by tag_sym, and enter it into the symbol table.
*/
{
  a_symbol_ptr      sym;
  a_type_ptr        class_type = tag_sym->variant.class_struct_union.type;
  a_boolean         suppress_error = FALSE;

  if (!is_unnamed_tag_symbol(tag_sym) && !tag_sym->is_error) {
    sym = alloc_symbol((a_symbol_kind)sk_type, tag_sym->header,
                       &tag_sym->decl_position);
    sym->variant.type.ptr = class_type;
    sym->variant.type.is_injected_class_name = TRUE;
    sym->is_class_member = TRUE;
    sym->parent.class_type = class_type;
    add_symbol_to_scope_list(sym, depth_scope_stack, &suppress_error);
    link_symbol_into_symbol_table(sym, depth_scope_stack, suppress_error);
  }  /* if */
}  /* enter_injected_class_name_symbol */


a_symbol_ptr enter_typedef_symbol(a_type_ptr       type_ptr,
                                  a_symbol_locator *locator,
                                  a_scope_depth    scope_depth,
                                  a_boolean        suppress_error)
/*
Allocate an sk_type symbol to represent a typedef declaration for the type
specified by type_ptr and enter it into the symbol table.  This routine
is called for typedef declarations (in place of enter_local_symbol, which
calls enter_symbol) because some error checks require the type to be added
to the symbol entry before the symbol is added to the symbol table.
*/
{
  a_symbol_ptr  sym;

  db_enter(3, "enter_typedef_symbol");
  if (scope_stack[scope_depth].kind == (a_scope_kind)sck_func_prototype &&
      !is_error_locator(*locator) && !is_error_type(type_ptr)) {
    pos_warning(ec_decl_in_prototype_scope, &locator->source_position);
  }  /* if */
  sym = alloc_symbol((a_symbol_kind)sk_type, locator->symbol_header,
                         &locator->source_position);
  sym->is_error = locator->is_error;
  /* Set the locator to point to the symbol entered. */
  locator->specific_symbol = sym;
  locator->is_qualified_name = FALSE;
  /* Bind the type in the symbol.  This has to be done before adding the
     symbol to the symbol table. */
  sym->variant.type.ptr = type_ptr;
  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(sym, scope_depth, &suppress_error);
  /* Add the symbol to the symbol table.  This must be done after the symbol
     is added to the scope list, because that sets the scope number, which
     is needed to check for redeclaration. */
  link_symbol_into_symbol_table(sym, scope_depth, suppress_error);
  db_exit();
  return sym;
}  /* enter_typedef_symbol */


void make_symbol_for_predeclared_type(a_type_ptr  predeclared_type,
                                      char        *name)
/*
Create a symbol of the specified name for the type entry pointed to by
predeclared_type, binding them to one another.  (predeclared_type should
point to an undefined class or struct type generated by the compiler.)
*/
{
  a_symbol_locator  loc;
  a_symbol_ptr      sym;

  check_assertion(predeclared_type != NULL &&
                  predeclared_type->source_corresp.assoc_info == NULL);
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  check_assertion(predeclared_type->kind == (a_type_kind)tk_struct ||
                  predeclared_type->kind == (a_type_kind)tk_class);
  sym = alloc_symbol((a_symbol_kind)sk_class_or_struct_tag,
                     loc.symbol_header, &null_source_position);
  set_source_corresp(&(predeclared_type->source_corresp), sym);
  sym->variant.class_struct_union.type = predeclared_type;
}  /* make_symbol_for_predeclared_type */


void make_symbol_for_namespace_std(void)
/*
Predeclare namespace "std" -- that is, create the namespace entry and add
it to the namespaces list in the file scope, and create the symbol entry for
"std".  However, don't actually add the symbol to the symbol table (since
the user is actually free to use the name "std" for other entities as long
as "namespace std" is never declared).
*/
{
  a_namespace_ptr  nsp;
  a_symbol_locator loc;

  /* Be sure symbol_for_namespace_std hasn't been created yet. */
  check_assertion(symbol_for_namespace_std == NULL);
  /* Create the symbol header for "std". */
  clear_locator(&loc, &null_source_position);
  (void)find_symbol("std", (sizeof_t)3, &loc);
  /* Allocate the symbol. */
  symbol_for_namespace_std = alloc_symbol((a_symbol_kind)sk_namespace,
                                          loc.symbol_header,
                                          &null_source_position);
  /* Create the namespace entry and bind it to the symbol. */
  nsp = alloc_namespace(/*is_alias=*/FALSE);
  set_source_corresp(&nsp->source_corresp, symbol_for_namespace_std);
  nsp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
  symbol_for_namespace_std->variant.namespace_info.ptr = nsp;
  /* Add the namespace to the namespaces list for the file scope. */
  check_assertion(depth_scope_stack == DEPTH_OF_FILE_SCOPE);
  add_to_namespaces_list(nsp);
  /* Push a scope to be sure a scope entry is recorded in the new
     namespace; then pop it off the stack again. */
  (void)push_namespace_scope((a_scope_kind)sck_namespace, nsp);
  pop_scope();
}  /* make_symbol_for_namespace_std */


void enter_symbol_for_namespace_std(a_symbol_locator  *locator)
/*
Namespace std was predeclared: its symbol was created but wasn't added to
the symbol table.  Do that now.  *locator indicates where a declaration of
namespace std was encountered in the source.
*/
{
  a_boolean  suppress_error = FALSE;

  /* Be sure the symbol hasn't already been entered. */
  check_assertion(symbol_for_namespace_std->decl_position.seq == 0);
  /* Update the source position in the existing symbol. */
  symbol_for_namespace_std->decl_position = locator->source_position;
  locator->specific_symbol = symbol_for_namespace_std;
  /* Add the symbol to the proper scope's symbol list. */
  add_symbol_to_scope_list(symbol_for_namespace_std, DEPTH_OF_FILE_SCOPE,
                           &suppress_error);
  /* Add the symbol to the symbol table. */
  link_symbol_into_symbol_table(symbol_for_namespace_std, DEPTH_OF_FILE_SCOPE,
                                suppress_error);
#if RECORD_HIDDEN_NAMES_IN_IL
  /* In some cases, this symbol is predeclared.  Make sure this flag is
     set in case this is such an implicit declaration. */
  symbol_for_namespace_std->header->any_decl_in_file_or_namespace_scope = TRUE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
}  /* enter_symbol_for_namespace_std */


#if defined(GUARD_MACRO_FOR_VA_LIST) || defined(GUARD_MACRO2_FOR_VA_LIST)

static a_boolean define_guard_macro(char *macro_name)
/*
If the guard macro with the indicated name is not defined already, define
it and return FALSE.  If it is defined already, do nothing and return TRUE.
*/
{
  a_boolean        already_defined = FALSE;
  a_symbol_locator locator;
  a_symbol_ptr     macro_sym;

  macro_sym = find_macro_symbol_by_name(macro_name,
                                        (sizeof_t)(strlen(macro_name)),
                                        &locator);
  if (macro_sym != NULL) {
    /* The macro is defined already. */
    already_defined = TRUE;
  } else {
    /* The macro is not defined.  Define it. */
    (void)enter_predef_macro("1", macro_name,
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  return already_defined;
}  /* define_guard_macro */

#endif /* defined(GUARD_MACRO_FOR_VA_LIST) || ... */

void declare_builtin_va_list_type(void)
/*
Declare the type va_list when <stdarg.h> is treated as a builtin.  This is
called at the point where the #include <stdarg.h> appears.
*/
{
  a_symbol_ptr     sym;
  a_type_ptr       va_list_type, va_list_typedef;
  a_symbol_locator locator;

  if (builtin_va_list_type == NULL) {
    /* Look for an existing va_list symbol.  Such a symbol would exist
       if declared in other headers, e.g., stdio.h.  That would be
       nonstandard, but we accommodate it. */
    clear_locator(&locator, &null_source_position);
#define VA_LIST_NAME "va_list"
    (void)find_symbol(VA_LIST_NAME, (sizeof_t)(sizeof(VA_LIST_NAME)-1),
                      &locator);
    sym = file_scope_id_lookup(il_header.primary_scope, &locator,
                               IDL_NO_OPTIONS);
    if (sym != NULL && is_type_symbol(sym)) {
      /* Yes, there is a global type called va_list.  Use it rather than
         declaring a new symbol. */
      va_list_type = type_symbol_type(sym);
    } else {
      /* There is no existing va_list.  Create one. */
      /* Look for a special predefined name (e.g., __edg_va_list).  If it's
         declared as a file-scope type, use that type as the type for the
         built-in va_list. */
      clear_locator(&locator, &null_source_position);
      (void)find_symbol(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME,
                      (sizeof_t)(sizeof(BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME)-1),
                        &locator);
      sym = file_scope_id_lookup(il_header.primary_scope, &locator,
                                 IDL_NO_OPTIONS);
      if (sym != NULL && is_type_symbol(sym)) {
        va_list_type = type_symbol_type(sym);
      } else {
        /* The special symbol does not exist, so use the default "void *". */
        va_list_type = make_pointer_type(void_type());
      }  /* if */
      /* Enter a file-scope symbol "va_list" that is a typedef to the
         proper type. */
      sym = full_enter_symbol(VA_LIST_NAME, (sizeof_t)(sizeof(VA_LIST_NAME)-1),
                              (a_symbol_kind)sk_type, DEPTH_OF_FILE_SCOPE);
#if RECORD_HIDDEN_NAMES_IN_IL
      /* Set the flag directly, since record_symbol_declaration is not
         called. */
      sym->header->any_decl_in_file_or_namespace_scope = TRUE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    }  /* if */
    /* Build a typedef for va_list.  This is done even when there is
       an existing symbol, because we need a declaration at the right
       place to tell the C- or C++-generating back end where to put the
       include of <stdarg.h>. */
    va_list_typedef = alloc_type((a_type_kind)tk_typeref);
    va_list_typedef->variant.typeref.type = va_list_type;
    va_list_typedef->is_builtin_va_list = TRUE;
    add_to_types_list(va_list_typedef, DEPTH_OF_FILE_SCOPE);
    set_source_corresp(&va_list_typedef->source_corresp, sym);
    va_list_typedef->source_corresp.decl_position = null_source_position;
    /* Note that we update a pre-existing symbol to point to the typedef.
       this is necessary so that the needed and referenced flags will be
       set appropriately on uses of va_list after this point. */
    sym->variant.type.ptr = va_list_typedef;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Put out a source sequence entry for the type. */
    add_to_source_sequence_list((char *)va_list_typedef,
                                (an_il_entry_kind)iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    builtin_va_list_type = type_symbol_type(sym);
#undef VA_LIST_NAME
#ifdef GUARD_MACRO_FOR_VA_LIST
    /* Define a macro that tells the headers that va_list has been
       defined. */
    va_list_typedef->va_list_guard_macro_was_defined =
                                   define_guard_macro(GUARD_MACRO_FOR_VA_LIST);
#endif /* ifdef GUARD_MACRO_FOR_VA_LIST */
#ifdef GUARD_MACRO2_FOR_VA_LIST
    /* Define a macro that tells the headers that va_list has been
       defined. */
    va_list_typedef->va_list_guard_macro2_was_defined =
                                  define_guard_macro(GUARD_MACRO2_FOR_VA_LIST);
#endif /* ifdef GUARD_MACRO2_FOR_VA_LIST */
  }  /* if */
}  /* declare_builtin_va_list_type */


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
  a_symbol_ptr        sym, second_best_match;
  a_namespace_ptr     nsp = NULL;
  a_boolean           extern_C_linkage_specified = FALSE;
  a_boolean           extern_C_overload = FALSE;

  db_enter(4, "find_external_symbol");
  /* Start with the external locator the same as the normal locator.  This
     is usually correct. */
  *ext_location = *location;
  if (is_error_locator(*ext_location)) {
    /* This is a compiler-generated error symbol (probably generated because
       an identifier was missing). */
    sym = NULL;
  } else {
    hdr_ptr = ext_location->symbol_header;
    if (linkage != (a_name_linkage_kind)nlk_external) {
      /* Either static or C++ external name linkage.  We can use the name and
         locator as passed in. */
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
        (void)find_symbol(new_ident, count, ext_location);
        hdr_ptr = ext_location->symbol_header;
      }
#else /* TARG_CASE_SENSITIVE_EXTERNAL_NAMES */
#if TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0
      /* See if the symbol's name is longer than the maximum number of
         significant characters in an external name. */
      if (hdr_ptr->identifier_length > TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME) {
        /* The name is overlong; the external version must be truncated.
           Create the header/locator by looking up the truncated name. */
        (void)find_symbol(hdr_ptr->identifier,
                          TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME, ext_location);
        hdr_ptr = ext_location->symbol_header;
      } else {
        /* The name is not overlong; the external name will be the same as the 
           source name, and therefore the locator for the new symbol is the
           same as that for the current symbol. */
      }  /* if */
#endif /* TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME > 0 */
#endif /* !TARG_CASE_SENSITIVE_EXTERNAL_NAMES */
    }  /* if */
    if (!C_mode() && linkage == (a_name_linkage_kind)nlk_external) {
      extern_C_linkage_specified = TRUE;
    }  /* if */
    /* See if there is already an external symbol with this name and belonging
       to the appropriate namespace. */
    if (!C_mode() && !location->is_file_scope_qualified_name) {
      /* See if the current declaration is a namespace-qualified name. */
      nsp = qualifier_namespace_ptr(*location);
      /* If not, use the innermost enclosing namespace by default. */
      if (nsp == NULL &&
          depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
        nsp = scope_stack[depth_innermost_namespace_scope].
                                           il_scope->variant.assoc_namespace;
      }  /* if */
    }  /* if */
    second_best_match = NULL;
    for (sym = hdr_ptr->other_symbols; sym != NULL; sym = sym->next) {
      /* Ignore symbols not associated with the current file scope.  These
         could be extern entities associated with other translation units. */
      if (sym->decl_scope != file_scope_number) continue;
      if (extern_C_linkage_specified) {
        a_source_correspondence  *scp;
        if (sym->kind == (a_symbol_kind)sk_extern_variable) {
          scp = &sym->variant.extern_symbol_descr->
                                    variant.variable->source_corresp;
        } else if (sym->kind == (a_symbol_kind)sk_extern_routine) {
          scp = &sym->variant.extern_symbol_descr->
                                    variant.routine.ptr->source_corresp;
        } else {
          /* Ignore other symbols on the list.  These include synthesized
             namespace projection symbols, and unknown function symbols. */
          continue;
        }  /* if */
        if (scp->name_linkage == (a_name_linkage_kind)nlk_external) {
          if (rout_type != NULL &&
              sym->kind == (a_symbol_kind)sk_extern_routine &&
              !is_error_type(sym->variant.extern_symbol_descr->type) &&
              sym->parent.namespace_ptr == nsp) {
            /* A special case -- two extern "C" routine declarations in the
               same namespace.  Consider them a match only if their
               signatures match; it they don't match, this will be treated
               as an overloading error. */
            extern_C_overload = TRUE;
          } else {
            /* Except for the special case noted above, two extern "C"
               entities with the same name are always a match, even if they
               belong to different namespaces or have different signatures,
               and even if one is a routine and the other is a variable:
               the names are identical to the linker. */
            if (second_best_match == NULL) second_best_match = sym;
            continue;
          }  /* if */
        }  /* if */
      }  /* if */
      if (sym->parent.namespace_ptr != nsp) {
        /* Namespaces do not match -- keep looking. */
      } else if (sym->kind == (a_symbol_kind)sk_extern_variable) {
        if (rout_type == NULL) break;
        if (second_best_match == NULL) second_best_match = sym;
      } else if (sym->kind == (a_symbol_kind)sk_extern_routine) {
        an_extern_symbol_descr_ptr esdp = sym->variant.extern_symbol_descr;
        /* A type compatibility check may also be required for routines. */
        if (rout_type == NULL || C_dialect != C_dialect_cplusplus ||
            esdp->variant.routine.ptr == il_header.main_routine) {
          /* A name match is enough in C (and for C++, if the routine is
             "::main"). */
          break;
        } else if (is_error_type(esdp->type)) {
          /* Assume this is not a match.  Keep looking. */
        } else {
          /* In C++ the function's type signature is effectively part of the
             name.  Therefore we check for parameter type compatibility (the
             return type is not decisive, since functions with the same
             param types and different return types are not allowed). */
          /* Note that error types are not considered compatible with
             anything here, and that's deliberate to avoid a false clash
             on something like
               int f(int)   { return 0; }
               int f(undef) { return 0; }
          */
          a_type_ptr           other_type;

          other_type = esdp->type;
          other_type = skip_typerefs(other_type);
          rout_type = skip_typerefs(rout_type);
          if (param_types_are_compatible(rout_type, other_type,
                                         TCF_NO_FLAGS)) {
            /* Param types are compatible, so we have a match. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      /* No match found yet, so keep looking.  If none is found, a NULL
         sym is returned to the caller. */
    }  /* for */
    if (sym == NULL && !extern_C_overload) {
      sym = second_best_match;
    }  /* if */
  }  /* if */
  /* Make the ext_location source position the same as the original source
     position. */
  ext_location->source_position = location->source_position;
  db_exit();
  return sym;
}  /* find_external_symbol */


a_symbol_header_ptr find_symbol_header(char             *identifier,
				       sizeof_t         length,
				       a_symbol_locator	*locator)
/*
Return the symbol header for the specified identifier.
*/
{
  a_symbol_header_ptr	sym_hdr;

  (void)find_symbol(identifier, length, locator);
  sym_hdr = locator->symbol_header;
  return sym_hdr;
}  /* find_symbol_header */


a_symbol_ptr find_label_symbol(a_symbol_header_ptr	sym_hdr,
			       a_scope_number		scope_number)
/*
Look through the active list of sym_hdr for a label symbol declared in
scope_number.
*/
{
  a_symbol_ptr	sym;

  for (sym = sym_hdr->symbol; sym != NULL; sym = sym->next) {
    if (sym->kind == (a_symbol_kind)sk_label &&
        sym->decl_scope == scope_number) break;
  }  /* for */
  return sym;
}  /* find_label_symbol */


a_symbol_ptr find_macro_symbol(a_symbol_header_ptr	sym_hdr)
/*
Look for a macro symbol on the symbol list of sym_hdr.  Return the macro
symbol or NULL if none is found.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbol_list_for_file_scope_symbols(sym_hdr);
       sym != NULL; sym = sym->next) {
    if (sym->kind == (a_symbol_kind)sk_macro) break;
  }  /* for */
  return sym;
}  /* find_macro_symbol */


a_symbol_ptr find_macro_symbol_by_name(char             *identifier,
				       sizeof_t         length,
				       a_symbol_locator	*locator)
/*
Look for a macro symbol with the specified name.  Return the macro symbol
if found, or NULL.
*/
{
  a_symbol_ptr	sym;

  sym = find_macro_symbol(find_symbol_header(identifier, length, locator));
  return sym;
}  /* find_macro_symbol_by_name */


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
  if (locator->symbol_header == unnamed_tag_symbol_header) {
    /* Let the symbols for an unnamed class and its constructor share the
       same symbol header. */
    hdr_ptr = locator->symbol_header;
  } else {
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
    blank_needed = (is_id_char[opstr[1]-CHAR_MIN] != FALSE);
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
type "type" is recorded in the locator.
*/
{
  if (is_error_type(type)) {
    set_to_error_locator(*locator);
  } else {
    clear_locator(locator, pos);
    locator->symbol_header = symbol_header_for_conversion_function(type);
  }  /* if */
  locator->is_conversion_name = TRUE;
  locator->variant.conversion_result_type = type;
}  /* make_type_conversion_locator */


a_symbol_ptr find_default_operator_new_sym(a_symbol_ptr sym,
                                           a_boolean    *ambiguous)
/*
Given the symbol for an operator new() (which may be overloaded), find the
default (i.e., single-argument) version and return a pointer to its symbol,
or NULL if it is not found or there is an ambiguity (e.g., resulting from
default arguments).  The symbol might be for a class-specific operator new(),
and therefore might be a projection symbol.  If there is an ambiguity return
*ambiguous set to TRUE.
*/
{
  a_boolean        is_overloaded;
  a_param_type_ptr ptp;
  a_symbol_ptr     default_sym = NULL;

  *ambiguous = FALSE;
  reduce_projection_symbol_to_fundamental_symbol(sym);
  is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
  if (is_overloaded) sym = sym->variant.overloaded_function.symbols;
  for (; sym != NULL; sym = is_overloaded ? sym->next : NULL) {
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* An overload set can contain a projection symbol as the result of a
         using declaration. */
      if (sym->ambiguous) {
        /* All bets are off if the symbol is ambiguous. */
        *ambiguous = TRUE;
        default_sym = NULL;
        break;
      }  /* if */
      reduce_projection_symbol_to_fundamental_symbol(sym);
    }  /* if */
    /* Ignore function templates. */
    if (is_function_symbol(sym)) {
      /* Look for a symbol for a function with just one parameter.  A default
         argument is not allowed on the first argument and need not be checked
         for; however, one may appear on the second argument. */
      ptp = skip_typerefs(sym->variant.routine.ptr->type)->
                                  variant.routine.extra_info->param_type_list;
      check_assertion(ptp != NULL);
      if (!is_error_type(ptp->type)) {
        if (ptp->next == NULL || ptp->next->has_default_arg) {
          if (default_sym == NULL) {
            /* A match.  But keep looking in case there's an ambiguity. */
            default_sym = sym;
          } else {
            /* It's ambiguous, so just return NULL.  No error is issued at
               this point. */
            default_sym = NULL;
            *ambiguous = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return default_sym;
}  /* find_default_operator_new_sym */


a_boolean is_default_operator_delete(a_routine_ptr routine)
/*
Return TRUE if the indicated routine (an operator delete function) is
a default operator delete function (including the class variant with
a second parameter of type size_t).
*/
{
  a_boolean                      is_default = FALSE;
  a_routine_type_supplement_ptr  rtsp;
  a_param_type_ptr               ptp;

  rtsp = skip_typerefs(routine->type)->variant.routine.extra_info;
  if (rtsp->has_ellipsis) {
    /* An operator delete declared with ellipsis can't be a default operator
       delete. */
  } else {
    ptp = rtsp->param_type_list;
    check_assertion(ptp != NULL);
    if (ptp->next == NULL) {
      /* operator delete(void *), a default operator delete. */
      is_default = TRUE;
    } else if (routine->source_corresp.is_class_member) {
      /* Look for a class member operator delete with a second parameter of
         type size_t. */
      ptp = ptp->next;
      if (ptp->next == NULL) {
        /* The function has two parameters. */
        a_type_ptr param_type = skip_typerefs(ptp->type);

        if (is_integral_type(param_type) &&
            param_type->variant.integer.int_kind == targ_size_t_int_kind) {
          /* operator delete(void *, size_t), a default operator delete. */
          is_default = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_default;
}  /* is_default_operator_delete */


a_symbol_ptr find_default_operator_delete_sym(a_symbol_ptr sym,
                                              a_boolean    *ambiguous)
/*
Given the symbol for an operator delete() (which may be overloaded and/or
be a projection symbol), find the default version (usually the single-argument
version) and return a pointer to its symbol (which may be a projection
symbol), or NULL if it is not found or there is an ambiguity.  If there is an
ambiguity return *ambiguous set to TRUE.
*/
{
  a_boolean      is_overloaded, ambiguous_alternate = FALSE, is_class_member;
  a_symbol_ptr   fund_sym, default_sym = NULL, alternate_default_sym = NULL;
  a_routine_ptr  rp;

  *ambiguous = FALSE;
  is_class_member = sym->is_class_member;
  reduce_projection_symbol_to_fundamental_symbol(sym);
  is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
  if (is_overloaded) sym = sym->variant.overloaded_function.symbols;
  for (; sym != NULL; sym = is_overloaded ? sym->next : NULL) {
    fund_sym = sym;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* An overload set can contain a projection symbol as the result of a
         using declaration. */
      if (sym->ambiguous) {
        /* All bets are off if the symbol is ambiguous. */
        *ambiguous = TRUE;
        break;
      }  /* if */
      fund_sym = fundamental_symbol_of(sym);
    }  /* if */
    /* Ignore function templates. */
    if (is_function_symbol(fund_sym)) {
      /* See if this is a default operator delete. */
      rp = fund_sym->variant.routine.ptr;
      if (is_default_operator_delete(rp)) {
        /* This is a default operator delete.  See whether it is the single-
           parameter version or the two-parameter version. */
        a_param_type_ptr ptp = skip_typerefs(rp->type)->
                                  variant.routine.extra_info->param_type_list;
        if (ptp->next == NULL) {
          /* "operator delete(void *)" is always the default version. */
          if (default_sym == NULL) {
            default_sym = sym;
            /* Keep looping in case there's an ambiguity. */
          } else {
            /* Ambiguities can be introduced into the overload set by using-
               declarations. */
            *ambiguous = TRUE;
            break;
          }  /* if */
        } else {
          /* "operator delete(void *, size_t)" is the default version
              unless "operator delete(void *)" also appears in the overload
              set (WP 3.7.3.2).  Keep looking. */
          if (alternate_default_sym == NULL) {
            alternate_default_sym = sym;
          } else {
            /* Ambiguities can be introduced into the overload set by using-
               declarations. */
            ambiguous_alternate = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (*ambiguous) {
    default_sym = NULL;
  } else if (is_class_member && default_sym == NULL) {
    /* No ordinary default operator delete has been found, but maybe an
       alternative version was declared.  If so, return it. */
    if (ambiguous_alternate) {
      *ambiguous = TRUE;
    } else {
      default_sym = alternate_default_sym;
    }  /* if */
  }  /* if */
  return default_sym;
}  /* find_default_operator_delete_sym */


a_symbol_ptr find_corresponding_operator_delete_sym(a_symbol_ptr op_new_sym,
                                                    a_type_ptr   class_type,
                                                    a_boolean    template_okay,
                                                    a_boolean    *ambiguous,
                                                    a_symbol_ptr *overload_sym)
/*
op_new_sym is a symbol for an operator new function; it cannot be a projection
symbol or an overload set.  Looking in the scope of class_type, or in the
global scope if class_type is NULL, find and return the corresponding
operator delete function (i.e., the operator delete function with identical
parameter types as the operator new function, excluding the first parameter
in each).  Return NULL if no match is found or if there is an ambiguity; in
the latter case, return *ambiguous set to TRUE.  If template_okay is TRUE,
simply return the symbol for a matching function template, if appropriate;
otherwise, return the symbol for the instance.  Also return in *overload_sym
the result of looking up the delete operator; it may be the same as the
symbol that is returned as the corresponding operator delete symbol, but it
may an overload symbol instead.
*/
{
  a_symbol_ptr                   sym = NULL;
  a_symbol_ptr                   corresp_op_delete_sym = NULL, fund_sym;
  a_routine_ptr                  rp;
  an_opname_kind                 delete_opname_kind;
  a_param_type_ptr               op_new_param_type_list, op_new_ptp, ptp;
  a_boolean                      is_overloaded, any_template_seen;
  a_routine_type_supplement_ptr  rtsp;
  a_boolean                      op_new_has_ellipsis = FALSE;

  db_enter(4, "find_corresponding_operator_delete_sym");
  check_assertion(op_new_sym->kind == (a_symbol_kind)sk_routine ||
                  op_new_sym->kind == (a_symbol_kind)sk_member_function);
  *ambiguous = FALSE;
  rp = op_new_sym->variant.routine.ptr;
  delete_opname_kind = (rp->opname_kind == (an_opname_kind)onk_new) ?
                         (an_opname_kind)onk_delete :
                         (an_opname_kind)onk_array_delete;
  if (class_type != NULL)  {
    /* Class member. */
    sym = opname_member_function_symbol(delete_opname_kind, class_type);
    if (sym == NULL) {
      /* There is no delete/array-delete operator declared in the given
         class, so we need to find a corresponding operator in the global
         scope instead (as required by WP 5.3.4 [expr.new] and 12.5
         [class.free]). */
      sym = opname_function_symbol(delete_opname_kind);
    }  /* if */
  } else {
    /* Global operator new. */
    sym = opname_function_symbol(delete_opname_kind);
  }  /* if */
  *overload_sym = sym;
  if (sym != NULL) {
    rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
    op_new_has_ellipsis = rtsp->has_ellipsis;
    op_new_param_type_list = rtsp->param_type_list;
    if (op_new_param_type_list->next == NULL && !op_new_has_ellipsis) {
      /* This is default (single-argument) operator new, so find the default
         operator delete. */
      corresp_op_delete_sym = find_default_operator_delete_sym(sym, ambiguous);
    } else {
      /* Placement new.  We need to examine all the delete operators and look
         for a type match. */
      fund_sym = fundamental_symbol_of(sym);
      if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
        is_overloaded = TRUE;
        sym = fund_sym->variant.overloaded_function.symbols;
      } else {
        is_overloaded = FALSE;
      }  /* if */
      any_template_seen = FALSE;
      for (; sym != NULL; sym = is_overloaded ? sym->next : NULL) {
        if (sym->kind == (a_symbol_kind)sk_projection) {
          /* An overload set can contain a projection symbol as the result of
             a using-declaration. */
          if (sym->ambiguous) {
            /* All bets are off if the symbol is ambiguous. */
            sym = NULL;
            *ambiguous = TRUE;
            break;
          }  /* if */
          fund_sym = fundamental_symbol_of(sym);
        } else {
          fund_sym = sym;
        }  /* if */
        if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
          /* Look for a matching template only if there's no match among the
             non-template functions. */
          any_template_seen = TRUE;
        } else {
          check_assertion(is_function_symbol(fund_sym));
          rtsp = skip_typerefs(fund_sym->variant.routine.ptr->type)->
                                               variant.routine.extra_info;
          if ((a_boolean)rtsp->has_ellipsis != op_new_has_ellipsis) {
            /* There can't be a match unless both were declared with ellipsis
               or neither was. */
            continue;
          }  /* if */
          ptp = rtsp->param_type_list;
          check_assertion(ptp != NULL);
          for (ptp = ptp->next, op_new_ptp = op_new_param_type_list->next;
               ptp != NULL && op_new_ptp != NULL;
               ptp = ptp->next, op_new_ptp = op_new_ptp->next) {
            if (!identical_types(ptp->type, op_new_ptp->type)) {
              /* No match. */
              goto next_delete_symbol;
            }  /* if */
            /* Keep looping as long as the types are identical and as long as
               there are still entries to compare on both lists. */
          }  /* for */
          if (ptp == NULL && op_new_ptp == NULL) {
            /* Both lists were the same length: a match was found. */
            if (corresp_op_delete_sym == NULL) {
              corresp_op_delete_sym = sym;
              /* Keep looping, in case there is an ambiguity. */
            } else {
              /* An ambiguity, presumably introduced into the overload set by
                 a using-declaration, has been encountered. */
              *ambiguous = TRUE;
              corresp_op_delete_sym = NULL;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
next_delete_symbol:;
      }  /* for */
      if (any_template_seen) {
        a_partial_order_candidate_ptr	candidate_list = NULL;
        a_symbol_ptr			template_sym;
        a_template_arg_ptr		templ_arg_list;
        /* The overload set included at least one function template. */
        if (corresp_op_delete_sym == NULL && !(*ambiguous)) {
          /* There was no match among the ordinary functions, so see if the
             template function(s) satisfy the need. */
          a_type_ptr  tp, saved_return_type, saved_first_param_type;

          tp = skip_typerefs(rp->type);
          /* Change the return type from void * to void. */
          saved_return_type = tp->variant.routine.return_type;
          tp->variant.routine.return_type = void_type();
          /* Change the first parameter type from size_t to void *. */
          saved_first_param_type = op_new_param_type_list->type;
          op_new_param_type_list->type = make_pointer_type(void_type());
          sym = *overload_sym;
          if (is_overloaded) {
            reduce_projection_symbol_to_fundamental_symbol(sym);
            sym = sym->variant.overloaded_function.symbols;
          }  /* if */
          for (; sym != NULL; sym = is_overloaded ? sym->next : NULL) {
            fund_sym = fundamental_symbol_of(sym);
            if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
              if (has_matching_template_function(
                                       fund_sym, tp, (a_template_arg_ptr)NULL,
                                       /*is_decl_context=*/TRUE)) {
                /* We have a match.  Add the matching template to a list of
                   matching candidates.  Any poorer matches will be removed
                   by this process. */
                add_to_partial_order_candidates_list(&candidate_list, sym,
                                                     (a_template_arg_ptr)NULL);
              }  /* if */
            }  /* if */
          }  /* for */
          if (candidate_list != NULL) {
            /* If any of the templates matched, select the best one using
               the partial ordering rules. */
            select_best_partial_order_candidate(
                           candidate_list, (a_symbol_ptr)NULL, &template_sym,
                           &templ_arg_list, ambiguous);
            if (!*ambiguous) {
              if (template_okay) {
                /* A template can be returned to the caller. */
                corresp_op_delete_sym = template_sym;
              } else {
                /* Do a partial instantiation if a match is found so that the
                   template instance can be returned. */
                a_boolean  is_new_template_instance;
                corresp_op_delete_sym = matching_template_function(
 					   template_sym, tp,
                                           (a_template_arg_ptr)NULL,
				           /*explicit_arg_list_present=*/FALSE,
                                           /*is_decl_context=*/TRUE,
                                           &is_new_template_instance);
              }  /* if */
            }  /* if */
          }  /* if */
          /* Restore the function type for the operator new. */
          tp->variant.routine.return_type = saved_return_type;
          op_new_param_type_list->type = saved_first_param_type;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    db_symbol(op_new_sym, "operator new is: ", 2);
    if (class_type != NULL) {
      fputs("lookup class is: ", f_debug);
      db_type_name(class_type);
      fputc('\n', f_debug);
    }  /* if */
    if (corresp_op_delete_sym == NULL) {
      fputs("no corresponding operator delete was found\n", f_debug);
    } else {
      db_symbol(corresp_op_delete_sym,
                "corresponding operator delete is: ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return corresp_op_delete_sym;
}  /* find_corresponding_operator_delete_sym */


a_symbol_ptr make_predeclared_function_symbol(a_symbol_locator  *locator,
                                              a_type_ptr        return_type,
                                              a_type_ptr        param1_type,
                                              a_type_ptr        param2_type,
                                              a_type_ptr        param3_type)
/*
Create a symbol and routine entry for a predeclared function.  locator points
to a symbol locator created to represent the entity's name.  return_type
(which must be non-NULL) and the parameter types (which may be NULL) indicate
how to form the function's signature.
*/
{
  a_symbol_ptr                   sym = NULL, ext_sym;
  a_type_ptr                     rout_type, old_type;
  a_routine_type_supplement_ptr  extra_info;
  an_id_linkage_kind             linkage;
  a_func_info_block              func_info;
  a_decl_modifiers_block         decl_modifiers;

  /* Create a routine type. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  extra_info = rout_type->variant.routine.extra_info;
  /* Return type. */
  rout_type->variant.routine.return_type = return_type;
  if (param1_type != NULL) {
    /* Set the first parameter. */
    extra_info->param_type_list = alloc_param_type(param1_type);
    /* Set the first parameter, if any. */
    if (param2_type != NULL) {
      a_param_type_ptr  ptp = extra_info->param_type_list;
      ptp->next = alloc_param_type(param2_type);
      /* Set the third parameter, if any. */
      if (param3_type != NULL) {
        ptp = ptp->next;
        ptp->next = alloc_param_type(param3_type);
      }  /* if */
    }  /* if */
  }  /* if */
  extra_info->prototyped = TRUE;
  set_routine_calling_method_flag(rout_type, &null_source_position);
  clear_func_info(&func_info);
  clear_decl_modifiers_block(&decl_modifiers);
  /* Create the symbol and routine entry.  Note that the routine entry
     is given a storage class of sc_extern since there is no definition
     in the current translation unit. */
  decl_routine(locator, (a_storage_class)sc_extern, rout_type, &func_info,
               (a_source_sequence_entry_ptr)NULL, SRK_DECLARATION,
               &decl_modifiers, &sym, &linkage, &old_type, &ext_sym,
               (a_decl_pos_block_ptr)NULL);
  sym->variant.routine.ptr->compiler_generated = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Predeclared functions should use __cdecl calling convention.  If that's
       not the default for the compilation, set it now. */
    if (default_calling_convention != (a_calling_convention)cc_cdecl) {
      extra_info->calling_convention = (a_calling_convention)cc_cdecl;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return sym;
}  /* make_predeclared_function_symbol */


void make_global_operator_new_or_delete_symbol(an_opname_kind  opname)
/*
Create a symbol and routine entry for ::operator new, ::operator new[],
::operator delete, or ::operator delete[].  These are entered into the
symbol table as part of initialization, so the locator has a default value
(as used with keywords).  The routine entry is marked as compiler generated;
if a user declaration appears later, the compiler-generated flag should be
cleared.
*/
{
  a_symbol_locator               locator;
  a_type_ptr                     return_type, param1_type;
  a_symbol_ptr                   sym;
  a_routine_type_supplement_ptr  rtsp;

  db_enter(5, "make_global_operator_new_or_delete_symbol");
  check_assertion_str(is_new_operator(opname) || is_delete_operator(opname),
                      "global_operator_new_or_delete_symbol: bad opname kind");
  /* Create a locator for the symbol that is to be created. This will also
     create the symbol header. */
  make_opname_locator(opname, &locator, &null_source_position);
  if (is_new_operator(opname)) {
    /* Return type for operator new is void *. */
    return_type = make_pointer_type(void_type());
    /* Type of the one parameter for operator new is size_t. */
    param1_type = integer_type(targ_size_t_int_kind);
  } else {
    /* Return type of operator delete is void. */
    return_type = void_type();
    /* Type of the one parameter for operator delete is void *. */
    param1_type = make_pointer_type(void_type());
  }  /* if */
  sym = make_predeclared_function_symbol(&locator, return_type, param1_type,
                                        (a_type_ptr)NULL, (a_type_ptr)NULL);
  if (exceptions_enabled && !ignore_exception_specifications) {
    /* Add exception specifications (but not if exception specifications are
       normally just parsed and discarded -- e.g., in Microsoft mode). */
    rtsp = sym->variant.routine.ptr->type->variant.routine.extra_info;
    if (is_delete_operator(opname)) {
      /* Mark the predeclared operator delete function with "throw()". */
      rtsp->exception_specification = alloc_exception_specification();
    } else {
      /* Putting out "throw(std::bad_alloc)" for the predeclared operator new
         is not yet implemented; it would entail predeclaring namespace std
         and class std::bad_alloc (and probably class std::exception). */
    }  /* if */
  }  /* if */
  db_exit();
}  /* make_global_operator_new_or_delete_symbol */

#if MICROSOFT_EXTENSIONS_ALLOWED

void make_predeclared_alloca_symbol(void)
/*
Create a symbol and routine entry for predeclared _alloca (only in Microsoft
C compatibility mode).
*/
{
  a_symbol_locator               locator;
  a_type_ptr                     return_type, param1_type;

  db_enter(5, "make_predeclared_alloca_symbol");
  check_assertion(microsoft_mode && C_mode());
  /* Create a locator for the symbol that is to be created.  This will also
     create the symbol header. */
  clear_locator(&locator, &null_source_position);
  (void)find_symbol("_alloca", (sizeof_t)7, &locator);
  /* Return type for _alloca is void *. */
  return_type = make_pointer_type(void_type());
  /* One parameter -- the size. */
  param1_type = integer_type(targ_size_t_int_kind);
  (void)make_predeclared_function_symbol(&locator, return_type, param1_type,
                                         (a_type_ptr)NULL, (a_type_ptr)NULL);
  db_exit();
}  /* make_predeclared_alloca_symbol */


void make_predeclared_size_t_symbol(void)
/*
Create a symbol and type entry for size_t (only in Microsoft mode).
C++ note: for Microsoft compatibility, the entries are recorded in the file
scope, not in namespace std.
*/
{
  a_symbol_locator  locator;
  a_type_ptr        tp;

  db_enter(5, "make_predeclared_size_t_symbol");
  check_assertion(microsoft_mode);
  clear_locator(&locator, &null_source_position);
  (void)find_symbol("size_t", (sizeof_t)6, &locator);
  tp = integer_type(targ_size_t_int_kind);
  decl_typedef(&locator, tp, (a_type_ptr)NULL, &predeclared_size_t_symbol,
               (a_source_sequence_entry_ptr)NULL,
               (a_decl_pos_block_ptr)NULL);
  /* Setting the defined flag to FALSE indicates there is (as yet) no explicit
     definition in the source program. */
  predeclared_size_t_symbol->defined = FALSE;
  db_exit();
}  /* make_predeclared_size_t_symbol */


void make_predeclared_bool_symbol(void)
/*
Create a symbol and type entry for bool (only in Microsoft mode); the entries
are recorded in the file scope.
*/
{
  a_symbol_locator  locator;
  a_symbol_ptr      sym;

  db_enter(5, "make_predeclared_bool_symbol");
  check_assertion(microsoft_mode);
  clear_locator(&locator, &null_source_position);
  (void)find_symbol("bool", (sizeof_t)4, &locator);
  decl_typedef(&locator, bool_type(), (a_type_ptr)NULL, &sym,
               (a_source_sequence_entry_ptr)NULL,
               (a_decl_pos_block_ptr)NULL);
  db_exit();
}  /* make_predeclared_bool_symbol */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_symbol_ptr find_default_constructor(a_type_ptr  class_type,
                                      a_boolean   *ambiguous)
/*
Find and return a pointer to a symbol representing a default constructor for
the class indicated by class_type.  (A default constructor is a constructor
that requires no arguments.)  If more than one acceptable constructor is
found, set *ambiguous to TRUE and return one of the symbols.  Return NULL
if no default constructor is found.  This routine is only used in C++ mode.
*/
{
  a_symbol_ptr  sym, ctor_sym = NULL;
  a_boolean     is_overloaded_function;

  *ambiguous = FALSE;
  sym = (symbol_supplement_for_class(class_type))->constructor;
  if (sym != NULL) {
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
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* Function templates are not considered. */
      } else if (is_default_constructor(sym->variant.routine.ptr,
                                       /*is_declarative_context=*/FALSE)) {
        /* sym is a default constructor. */
        if (ctor_sym != NULL) {
          /* A default constructor had already been found, so there's
             more than one.  We have an ambiguous reference. */
          *ambiguous = TRUE;
          break;
        } else {
          /* We've found one.  Record it, but keep looking.  If there's an
             ambiguity we need to report it. */
          ctor_sym = sym;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return ctor_sym;
}  /* find_default_constructor */


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
  a_routine_ptr ctor_routine = NULL;
  a_symbol_ptr  ctor_sym;
  a_boolean     ambiguous;

  /* This routine is similar to select_overloaded_function. */
  class_type = skip_typerefs(class_type);
  ctor_sym = find_default_constructor(class_type, &ambiguous);
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
                                             /*instantiate=*/TRUE);
    ctor_routine = ctor_sym->variant.routine.ptr;
  }  /* if */
  return ctor_routine;
}  /* select_default_constructor */


a_routine_ptr select_destructor(a_type_ptr        class_type,
				a_type_ptr        object_class_type,
                                a_source_position *position,
                                a_boolean         honor_virtual,
                                a_boolean         evaluated)
/*
If the indicated class has a destructor, check that it is accessible, mark
it as referenced, and return a pointer to the routine entry.  Otherwise,
return NULL.  object_class_type points to the type of the object being
destroyed; class_type may be a base class of object_class_type.  This is
needed for protected member access checking.  If honor_virtual is TRUE, and
if the destructor is virtual, consider this reference a virtual function
call.  If evaluated is FALSE, the reference is within an unevaluated
expression.  *position is the source position of the reference.
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
                                               /*instantiate=*/TRUE);
      dtor_routine = dtor_sym->variant.routine.ptr;
    }  /* if */
  }  /* if */
  return dtor_routine;
}  /* select_destructor */


a_routine_ptr select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  qualifiers_required,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             evaluated)
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
*/
{
  a_symbol_ptr  cctor_sym;
  a_routine_ptr cctor_routine = NULL;
  a_boolean     ambiguous;

  cctor_sym = find_copy_constructor(class_type, qualifiers_required,
                                    err_pos, &ambiguous, class_bitwise_copy);
  if (*class_bitwise_copy) {
    /* A bitwise copy is allowed. */
  } else if (ambiguous) {
    /* More than one applicable copy constructor. */
    pos_ty_error(ec_ambiguous_copy_constructor, err_pos, class_type);
  } else if (cctor_sym == NULL) {
    /* No applicable copy constructor. */
    if (qualifiers_required == TQ_CONST) {
      /* The common case:  missing const copy constructor. */
      pos_ty_error(ec_missing_const_copy_constructor, err_pos, class_type);
    } else {
      /* Unusual case: volatile or const-volatile expected. */
      pos_ty_error(ec_no_suitable_copy_constructor, err_pos, class_type);
    }  /* if */
  } else {
    /* Exactly one copy constructor is best. */
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(cctor_sym, err_pos,
                                             object_class_type,
                                             /*honor_virtual=*/FALSE,
                                             evaluated,
                                             /*instantiate=*/TRUE);
    cctor_routine = cctor_sym->variant.routine.ptr;
  }  /* if */
  return cctor_routine;
}  /* select_copy_constructor */


a_symbol_ptr find_copy_assignment_operator(
                                    a_type_ptr            class_type,
                                    a_type_qualifier_set  required_qualifiers,
                                    a_boolean             *ambiguous,
                                    a_boolean             *pass_by_value)
/*
Return a pointer to the symbol entry for the copy assignment operator for
class_type that is consistent with the type qualifiers indicated by
required_qualifiers; return NULL if none is found.  *ambiguous is returned
TRUE if there is more than one matching assignment operator.  *pass_by_value
is returned TRUE if the parameter is not a reference parameter.
*/
{
  a_symbol_ptr                   sym, opass_sym = NULL;
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      is_overloaded_function;
  a_boolean                      opass_sym_matches_exactly = FALSE;
  a_boolean                      base_class_match_allowed = FALSE;
  a_boolean                      any_base_class_match = FALSE;

  *ambiguous = FALSE;
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->assignment_by_bitwise_copy_allowed ||
      cssp->assignment_operator == NULL) {
    /* A NULL assignment operator when bitwise copies are not allowed can
       occur in certain error cases.  Return NULL. */
    check_assertion(cssp->assignment_by_bitwise_copy_allowed ||
                    total_errors != 0);
  } else {
    sym = cssp->assignment_operator;
    /* If sym is an overloaded function symbol we need to go through the whole
       list. */
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_overloaded_function = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    } else {
      is_overloaded_function = FALSE;
    }  /* if */
    /* This is potentially a two-pass loop.  The first time through
       base_class_match_allowed is FALSE, the second time it's TRUE.  A "base
       class match" is a cfront compatibility feature, where D::operator=(B&)
       is treated as a copy assignment operator when B is a base class of D.
       It is coded this way to avoid finding ambiguity when, for instance,
       D::operator=(D&) and D::operator=(B&) exist side-by-side -- the former
       is always preferred. */
    for (;;) {
      /* Find an assignment operator whose argument is ref-class (pass by
         reference) or class (pass_by_value). */
      for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
        a_boolean             sym_matches_exactly;
        a_boolean             is_ref_arg;
        a_type_qualifier_set  qualifiers = TQ_NONE;
        a_boolean             is_base_class_match = FALSE;

        if (sym->kind != (a_symbol_kind)sk_member_function) {
          /* Function templates are not considered, and projection symbols
             can also be ignored, since using-declarations cannot introduce
             a copy assignment operator from a base class into a derived
             class (see member_using_declaration in class_decl.c). */
        } else if (is_assignment_operator_for_copy(sym, &is_ref_arg,
                                                   &qualifiers,
                                                   &is_base_class_match)) {
          /* Found an assignment operator that can copy the current class. */
          if (is_base_class_match) {
            any_base_class_match = TRUE;
            /* Ignore a base class match on the first pass. */
            if (!base_class_match_allowed) continue;
          }  /* if */
          if (!is_ref_arg) {
            /* Not a reference type, so qualifiers are ignored. */
            sym_matches_exactly = TRUE;
          } else {
            /* Reference type. */
            if ((required_qualifiers & qualifiers) != required_qualifiers) {
              /* No match -- keep looking. */
              continue;
            } else if (required_qualifiers == qualifiers) {
              /* It's an exact match. */
              sym_matches_exactly = TRUE;
            } else {
              /* It's not quite an exact match. */
              sym_matches_exactly = FALSE;
            }  /* if */
          }  /* if */
          if (opass_sym != NULL) {
            /* We have a match on this symbol, but we've already had one
               before as well.  If one but not the other is an exact match,
               take the one that matches.  Otherwise it's an ambiguity.  */
            *ambiguous = (sym_matches_exactly == opass_sym_matches_exactly);
            if (!sym_matches_exactly) continue;
          }  /* if */
          opass_sym = sym;
          opass_sym_matches_exactly = sym_matches_exactly;
          *pass_by_value = !is_ref_arg;
        }  /* if */
      }  /* for */
      /* If this is already the second pass or if we've found a match break
         out of the loop. */
      if (opass_sym != NULL || base_class_match_allowed)  break;
      /* If no base class match was found on the first pass don't bother doing
         a second. */
      if (!any_base_class_match) break;
      /* Reset variables for a second pass. */
      base_class_match_allowed = TRUE;
      sym = symbol_supplement_for_class(class_type)->assignment_operator;
      if (is_overloaded_function) {
        sym = sym->variant.overloaded_function.symbols;
      }  /* if */
    }  /* for */
  }  /* if */
  return opass_sym;
}  /* find_copy_assignment_operator */


a_routine_ptr select_copy_assignment_operator(
                                    a_type_ptr            class_type,
                                    a_type_qualifier_set  required_qualifiers,
                                    a_source_position     *err_pos,
                                    a_boolean             *pass_by_value)
/*
Return a pointer to the routine entry for the copy assignment operator for
class_type that is consistent with the type qualifiers indicated by
required_qualifiers.  *err_pos indicates the source position at which to issue
an error (e.g., for ambiguous or missing assignment operator).  *pass_by_value
is returned TRUE if the parameter is not a reference parameter.
*/
{
  a_symbol_ptr    opass_sym = NULL;
  a_boolean       ambiguous;
  a_routine_ptr   opass_routine = NULL;

  db_enter(4, "select_copy_assignment_operator");
  opass_sym = find_copy_assignment_operator(class_type, required_qualifiers,
                                            &ambiguous, pass_by_value);
  if (ambiguous) {
    /* More than one applicable assignment operator function. */
    pos_ty_error(ec_ambiguous_assignment_operator, err_pos, class_type);
  } else if (opass_sym == NULL) {
    /* No applicable assignment operator function. */
    if (required_qualifiers == TQ_CONST) {
      /* The common case:  missing const assignment operator function. */
      pos_ty_error(ec_missing_const_assignment_operator, err_pos, class_type);
    } else {
      /* Unusual case: volatile or const-volatile expected. */
      pos_ty_error(ec_no_suitable_assignment_operator, err_pos, class_type);
    }  /* if */
  } else {
    /* Exactly one assignment operator function is best. */
    /* Check that the function is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(
                                  opass_sym, err_pos, (a_type_ptr)NULL,
                                  /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                                  /*instantiate=*/TRUE);
    opass_routine = opass_sym->variant.routine.ptr;
  }  /* if */
  db_exit();
  return opass_routine;
}  /* select_copy_assignment_operator */


char *il_entry_for_symbol_null_okay(a_symbol_ptr      sym,
                                    an_il_entry_kind  *kind)
/*
Return a pointer to the IL entry to which the specified symbol refers.  Also
return the kind of IL entry that is found (if kind is not NULL).  If the
symbol is not associated with an IL entry, return NULL, and return kind
set to iek_none.
*/
{
  char             *entry_ptr = NULL;
  an_il_entry_kind lkind;

  switch (sym->kind) {
    case sk_macro:
#if RECORD_MACROS_IN_IL
      entry_ptr = (char *)sym->variant.macro_def->macro;
      lkind = iek_macro;
#endif /* RECORD_MACROS_IN_IL */
      break;
    case sk_constant:
      entry_ptr = (char *)sym->variant.constant;
      lkind = iek_constant;
      break;
    case sk_type:
      entry_ptr = (char *)sym->variant.type.ptr;
      lkind = iek_type;
      break;
    case sk_enum_tag:
      entry_ptr = (char *)sym->variant.enumeration.type;
      lkind = iek_type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      entry_ptr = (char *)sym->variant.class_struct_union.type;
      lkind = iek_type;
      break;
    case sk_variable:
      entry_ptr = (char *)sym->variant.variable.ptr;
      lkind = iek_variable;
      break;
    case sk_static_data_member:
      entry_ptr = (char *)sym->variant.static_data_member.variable;
      lkind = iek_variable;
      break;
    case sk_field:
      entry_ptr = (char *)sym->variant.field.ptr;
      lkind = iek_field;
      break;
    case sk_routine:
    case sk_member_function:
      entry_ptr = (char *)sym->variant.routine.ptr;
      lkind = iek_routine;
      break;
    case sk_label:
      entry_ptr = (char *)sym->variant.label.ptr;
      lkind = iek_label;
      break;
    case sk_namespace:
      entry_ptr = (char *)sym->variant.namespace_info.ptr;
      lkind = iek_namespace;
      break;
    case sk_function_template:
    case sk_class_template:
      entry_ptr = (char *)sym->variant.template_info->il_template_entry;
      lkind = iek_template;
      break;
    default:;
      /* Other cases ignored. */
  }  /* switch */
  if (entry_ptr == NULL) lkind = iek_none;
  if (kind != NULL) *kind = lkind;
  return entry_ptr;
}  /* il_entry_for_symbol_null_okay */


char *il_entry_for_symbol(a_symbol_ptr      sym,
                          an_il_entry_kind  *kind)
/*
Return a pointer to the IL entry to which the specified symbol refers.  Also
return the kind of IL entry that is found.  Always returns a non-NULL value.
*/
{
  char *entry_ptr = il_entry_for_symbol_null_okay(sym, kind);

  check_assertion_str(entry_ptr != NULL,
                      "il_entry_for_symbol: NULL assoc IL entry ptr");
  return entry_ptr;
}  /* il_entry_for_symbol */


a_source_correspondence *source_corresp_entry_for_symbol(a_symbol_ptr sym_ptr)
/*
Return a pointer to the source correspondence entry in the IL entry
for the given symbol.  Return NULL if there isn't one.
*/
{
  char              *entity_ptr;
  an_il_entry_kind  entity_kind;

  entity_ptr = il_entry_for_symbol_null_okay(sym_ptr, &entity_kind);
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
  } else if (sym_ptr->kind == (a_symbol_kind)sk_class_template) {
    /* Access for class templates is stored in the template symbol
       supplement. */
    access = (an_access_specifier)sym_ptr->
                         variant.template_info->variant.class_template.access;
  } else if (sym_ptr->kind == (a_symbol_kind)sk_function_template) {
    /* Access for function templates is stored in routine entry pointed to
       by the template symbol supplement. */
    access = sym_ptr->variant.template_info->
                              variant.function.routine->source_corresp.access;
  } else if (sym_ptr->kind == (a_symbol_kind)sk_type &&
             sym_ptr->variant.type.is_injected_class_name) {
    /* Symbols for injected class names are always public. */
    access = (an_access_specifier)as_public;
  } else if (sym_ptr->kind == (a_symbol_kind)sk_undefined) {
    /* Error case; assume public. */
    access = (an_access_specifier)as_public;
  } else {
    /* Normal symbol (not projection or overloaded function). */
    a_source_correspondence *scp = source_corresp_entry_for_symbol(sym_ptr);
    check_assertion_str2(sym_ptr->kind
                                    != (a_symbol_kind)sk_namespace_projection,
                         "access_for_symbol:", "invalid symbol kind");
    check_assertion(scp != NULL);
    access = scp->access;
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


/*
Function pointer types for routines to be passed into
have_particular_member_access_privilege.
*/
typedef a_boolean a_befriending_list_test_function(
                                     a_class_list_entry_ptr befriending_list,
                                     a_type_ptr             class_type);
typedef a_befriending_list_test_function *a_befriending_list_test_function_ptr;
typedef a_boolean a_member_access_from_class_scope_test_function(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep);
typedef a_member_access_from_class_scope_test_function
                           *a_member_access_from_class_scope_test_function_ptr;


static a_boolean have_particular_member_access_privilege(
      a_type_ptr                                         class_type,
      a_befriending_list_test_function_ptr               befriending_list_test,
      a_member_access_from_class_scope_test_function_ptr class_scope_test)
/*
Return TRUE if we currently have member access privilege to the class
indicated by class_type in the particular way tested for by the
functions befriending_list_test and class_scope_test.
*/
{
  a_boolean               have_member_privilege = FALSE;
  a_scope_stack_entry_ptr ssep;
  a_routine_ptr           scope_routine;
  a_scope_depth           scope_depth;
  a_boolean               skipping_to_function = FALSE;

  /* Consider each scope on the scope stack that affects access control.
     They are linked together on a list. */
  for (scope_depth = depth_of_innermost_scope_that_affects_access_control;
       scope_depth != NO_SCOPE_DEPTH;
       scope_depth = ssep->next_scope_that_affects_access_control) {
    a_scope_kind kind;
    ssep = &scope_stack[scope_depth];
    kind = ssep->kind;
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_function_access) {
      /* A function or function access scope.  See if class_type is on
         its befriending list. */
      if (kind == (a_scope_kind)sck_function_access) {
        scope_routine = ssep->assoc_routine;
      } else {
        scope_routine = ssep->il_scope->variant.routine.ptr;
      }  /* if */
      if (befriending_list_test(scope_routine->befriending_classes,
                                class_type)) {
        /* We are inside a function that is a friend of class_type. */
        have_member_privilege = TRUE;
        break;
      }  /* if */
      /* Terminate the skip of enclosing classes of nested functions now that
         a function scope has been found. */
      skipping_to_function = FALSE;
    } else {
      check_assertion_str(kind == (a_scope_kind)sck_class_struct_union ||
                          kind == (a_scope_kind)sck_class_reactivation,
                   "have_particular_member_access_privilege: bad stack entry");
      /* A class or class reactivation. */
      if (skipping_to_function) {
        /* We're skipping class scopes until we get to a function.  The
           class scopes being skipped are parent classes of a nested class. */
      } else {
        /* Check for access granted by being a member of the class. */
        if (class_scope_test(class_type, ssep)) {
          /* We are inside a class that gives us member access. */
          have_member_privilege = TRUE;
          break;
        }  /* if */
        /* Continue through the scope stack.  However, skip classes within
           which the current one is nested, because nested classes have no
           special access to the members of the enclosing classes.  In
           non-strict mode, allow access as an extension. */
        if (strict_ansi_mode) {
          skipping_to_function = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return have_member_privilege;
}  /* have_particular_member_access_privilege */
  

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
  a_boolean have_member_privilege = have_particular_member_access_privilege(
                                          class_type,
                                          on_befriending_list,
                                          have_member_access_from_class_scope);
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
  a_boolean have_member_privilege =
                 have_particular_member_access_privilege(
                                   class_type,
                                   have_protected_access_from_befriending_list,
                                   have_protected_access_from_class_scope);
  return have_member_privilege;
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


/* Declaration needed because of forward declaration. */
static a_boolean have_access_across_derivations(a_symbol_ptr symbol,
                                                a_symbol_ptr view_sym);


static a_boolean have_access_across_path(
                             a_symbol_ptr                   sym,
                             a_type_ptr                     viewpoint_class,
                             a_derivation_step_ptr          path,
                             a_base_class_derivation_ptr    bcdp,
                             a_symbol_ptr                   proj_sym,
                             a_virtual_step_stack_entry_ptr virtual_step_stack)
/*
Return TRUE if the symbol sym is accessible at the current location
in the source program when viewed from the class viewpoint_class.
path is the derivation path from viewpoint_class to sym;
it is NULL if sym is in viewpoint_class.  If non-NULL, it is part of
the path of the base class derivation bcdp.  proj_sym is the projection
symbol from which we started this access check, or an updated one picked
up during the recursive descent through the derivation; it is ignored if
path == NULL, but otherwise it must be a projection symbol (although
its fundamental symbol might not be sym, i.e., in the overloaded
function case; in that case sym might be a projection symbol as well).
virtual_step_stack is a pointer to a linked list that describes a stack
of virtual steps being expanded by invocations of this routine above
this one.
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
    access = access_for_symbol(sym);
  } else {
    a_symbol_ptr fund_proj_sym;
    /* Determine the effective access in the viewpoint class. */
#if CHECKING
    if (proj_sym == NULL) {
      internal_error("have_access_across_path: proj_sym is NULL");
    }  /* if */
    if (proj_sym->kind != (a_symbol_kind)sk_projection) {
      internal_error("have_access_across_path: proj_sym not projection");
    }  /* if */
#endif /* CHECKING */
    /* Note that in the overloaded function case proj_sym is a projection
       of an sk_overloaded_function symbol, not of sym. */
    fund_proj_sym= proj_sym->variant.projection.extra_info->fundamental_symbol;
    need_to_compute_access = TRUE;
    if (proj_sym->parent.class_type == viewpoint_class) {
      /* The step we are looking at is the first one, so the effective
         access is available from the projection symbol. */
      access = proj_sym->variant.projection.access;
      need_to_compute_access = FALSE;
    } else if (proj_sym->variant.projection.any_intervening_using_decl) {
      /* There is a using declaration somewhere on some derivation path, so
         we must look for a projection symbol that applies at this step of
         the path in case it is a using declaration.  It's okay not to find
         such a projection symbol since the using declaration might not be
         at the current level. */
      /* Run two loops -- first over the inactive list and then (if needed)
         over the active list. */
      int iter;
      for (iter = 1; iter <= 2; ++iter) {
        step_proj_sym = (iter == 1) ? proj_sym->header->inactive_symbols :
                                      proj_sym->header->symbol;
        for (; step_proj_sym != NULL; step_proj_sym = step_proj_sym->next) {
          if (step_proj_sym->parent.class_type == viewpoint_class &&
              step_proj_sym->kind == (a_symbol_kind)sk_projection &&
              step_proj_sym->variant.projection.extra_info->
                                         fundamental_symbol == fund_proj_sym) {
            /* Replace the projection symbol we have by the new one.  Note
               that it will get passed down in the recursive call below,
               which is good, because once we get past the using declarations
               we can use the faster technique. */
            proj_sym = step_proj_sym;
            access = proj_sym->variant.projection.access;
            need_to_compute_access = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (!need_to_compute_access) break;
      }  /* for */
    }  /* if */
    if (!need_to_compute_access) {
      /* If the symbol is for an overloaded function, we can use the access
         computed only if the projection symbol is a using declaration.
         Otherwise, the individual functions in the overload sets can have
         distinct access settings, and the projection symbol cannot
         indicate all of them. */
      if (fund_proj_sym->kind == (a_symbol_kind)sk_overloaded_function) {
        if (!proj_sym->variant.projection.is_using_decl) {
          need_to_compute_access = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (need_to_compute_access) {
      /* The access must be determined by looking at the derivation steps.
         This is probably a little faster than looking for the projection
         symbol (and the projection symbol might not exist and would have
         to be created if that approach were used). */
      access = access_for_symbol(sym);
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
                              have_member_access_privilege(viewpoint_class),
              have_member_access)) {
    /* The member is not inaccessible (i.e., there is some access to it),
       and we have member access privilege to the class, so we have access
       to the member. */
    have_access = TRUE;
  } else if (access == (an_access_specifier)as_protected &&
             (determined_protected_member_access = TRUE,
              have_protected_member_access =
                    have_protected_member_access_privilege(viewpoint_class),
              have_protected_member_access)) {
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
      /* If sym is the specific symbol chosen from an overload set
         designated by proj_sym, it might be a projection symbol itself.
         Look down from it to its fundamental symbol, looking for access. */
      if (sym->kind != (a_symbol_kind)sk_projection) {
        /* Normal case. */
        /* We're already in the class of the fundamental symbol, so we do
           not have access. */
        /* have_access = FALSE;  -- already set. */
      } else {
        /* This code really is needed, for obscure cases involving member
           access due to protected derivations. */
        proj_sym = sym;
        sym = fundamental_symbol_of(sym);
        have_access = have_access_across_derivations(sym, proj_sym);
      }  /* if */
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
        bcp = path->base_class;
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
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (microsoft_mode &&
                     have_member_access_privilege(bcp->type)) {
            /* Microsoft considers a base class accessible if we have member
               access to it.  This is presumably because of the WP wording
               that says "A base class is said to be accessible if an invented
               public member of the base class is accessible." */
            base_class_accessible = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
          if (have_access_across_path(sym, bcp->type, path_next,
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
        bcp = path->base_class;
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
member.  If view_sym is an overloaded function symbol or a projection
thereof, symbol is the specific symbol chosen from that overload set
(and possibly a projection symbol); otherwise symbol is not a projection
symbol, and view_sym is either the same as symbol or a projection thereof.
*/
{
  a_boolean                   have_access = FALSE;
  a_base_class_ptr            bcp;
  a_base_class_derivation_ptr derivations, preferred_derivation, bcdp;
  a_derivation_step_ptr       preferred_path;
  a_type_ptr                  viewpoint_class;

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
  viewpoint_class = view_sym->parent.class_type;
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
  a_symbol_ptr	fund_sym = fundamental_symbol_of(symbol);
  a_boolean	have_access = TRUE;

  if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Suppress access checking during prototype instantiations.  Access
       checking cannot be done for a template, only for instances. */
  } else if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* For overloaded functions, do not check access now.  The check will
       be done after the specific function is determined. */
  } else if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
    /* Likewise treat templates as sets of overloaded functions. */
  } else {
    have_access = have_access_across_derivations(fund_sym, symbol);
  }  /* if */
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


void f_check_ambiguity_and_verify_access(a_symbol_locator *locator,
					 a_boolean	  is_template_context)
/*
Verify that the indicated symbol is not ambiguous and that we have
access to it.  In case of an ambiguity, the locator is set to an error
locator.  Note that no access checking is done on overloaded function symbols.

This routine will only be called for member symbols or symbols that
are ambiguous.  In other words, if the symbol is not ambiguous, it
must be a member symbol.

If an error is detected, the scope stack is consulted to see if access
errors should be deferred and rechecked later.  When access errors are
to be deferred, and an access error is detected, instead of issuing the
error immediately an access error descriptor is created that provides
information about the error that was detected.  Later
perform_deferred_access_checks will be called to repeat the access
checks that had failed earlier.  If the access checks still fail,
the errors may be issued or retained for yet another check.

The access deferral mechanism is used when processing definitions
of member functions and friend functions.  For member functions,
access to the return type cannot be checked until we know the parent
class of the member being defined.  For friend functions, access to the
return type and parameter types of the function cannot be checked until
we have scanned the entire function declarator.

is_template_context is TRUE if the token following the identifier is a
"<" token and an unambiguous injected class template symbol should be
accepted even though the injected class symbol is ambiguous.
*/
{
  a_symbol_ptr   sym = locator->specific_symbol;
  a_symbol_ptr   fund_sym = fundamental_symbol_of(sym);

  /* This routine looks like overload_check_ambiguity_and_verify_access. */
  /* Issue an error if the symbol is ambiguous.  Symbols can be ambiguous
     either as a result of using directives or as a result of inheritance.
     Ambiguity checking must precede access control (ARM, 10.1.1). */
  if (sym->ambiguous &&
      !(is_template_context && sym->kind == (a_symbol_kind)sk_projection &&
        sym->variant.projection.injected_class_template_name_is_unambiguous)) {

    pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
    set_to_error_locator(*locator);
  } else if (locator->is_template_id) {
    /* The access of the template is checked when the template name
       is looked up.  For functions, access is checked after overload
       resolution has been done. */
  } else if (microsoft_mode && !locator->is_qualified_name &&
             is_type_symbol(fund_sym)) {
    /* The Microsoft compiler allows access to private types in base
       classes as long as they are named by the inherited name. */
  } else if (!have_access_to_symbol(sym)) {
    /* The symbol is not accessible. */
    a_boolean			defer_access_checks = FALSE;
    a_scope_stack_entry_ptr	ssep;

    if (curr_deferred_access_scope != NO_SCOPE_DEPTH) {
      ssep = &scope_stack[curr_deferred_access_scope];
      defer_access_checks = ssep->defer_access_checks;
    }  /* if */
    if (!defer_access_checks) {
      if (!locator->access_control_error_reported) {
        issue_access_error(fundamental_symbol_of(sym),
                           &locator->source_position);
        locator->access_control_error_reported = TRUE;
      }  /* if */
    } else {
      /* Access checks are deferred, so put an entry on a list for later
         checking. */
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
}  /* f_check_ambiguity_and_verify_access */


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
    a_boolean			remove_from_list = TRUE;
    if (aedp != NULL) {
      for (; aedp != NULL; aedp = next_aedp) {
        next_aedp = aedp->next;
        aedp->next = NULL;
        if (!have_access_to_symbol(aedp->sym)) {
          /* The access check still failed. */
          if (ssep->defer_access_checks) {
            /* Keep the entry on the list. */
            remove_from_list = FALSE;
          } else {
            issue_access_error(fundamental_symbol_of(aedp->sym),
                               &aedp->position);
          }  /* if */
        }  /* if */
        if (remove_from_list) {
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
  a_scope_stack_entry_ptr  ssep;

  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[curr_deferred_access_scope];
  /* This routine is always called last, so we can reset this flag now. */
  ssep->defer_access_checks = FALSE;
  if (ssep->deferred_access_checks != NULL) {
    if (ssep->deferred_access_checks != NULL) {
      if (rp->source_corresp.is_class_member) {
        push_class_reactivation_scope(rp->source_corresp.parent.class_type,
                                      /*extend_namespace=*/FALSE);
      }  /* if */
      (void)push_scope((a_scope_kind)sck_function_access, NO_SCOPE_NUMBER,
                       (a_type_ptr)NULL, rp);
      perform_deferred_access_checks();
      pop_scope();
      if (rp->source_corresp.is_class_member) pop_class_reactivation_scope();
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
identifier at which point curr_token_sequence_number is the number of the first
token that is part of the generalized identifier.  All tokens after
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
an sk_function_template symbol from which the locator symbol was
instantiated, or a projection symbol pointing to one of those two
kinds of symbols.
*/
{
  /* This routine looks like f_check_ambiguity_and_verify_access. */
  /* Issue an error if the symbol is ambiguous.  Symbols can be ambiguous
     either as a result of using directives or as a result of inheritance.
     Ambiguity checking must precede access control (ARM, 10.1.1). */
  if (overloaded_symbol->ambiguous) {
    pos_sy_error(ec_ambiguous_name, &locator->source_position,
                 overloaded_symbol);
    set_to_error_locator(*locator);
  } else if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Suppress access checking during prototype instantiations.  Access
       checking cannot be done for a template, only for instances. */
  } else if (!overloaded_symbol->is_class_member) {
    /* Non-class-members are always accessible. */
  } else {
    /* See if we have access to the symbol.  Note that we do not strip
       projection symbols from the specific symbol. */
    if (!have_access_across_derivations(locator->specific_symbol,
                                        overloaded_symbol)) {
      /* The symbol is not accessible. */
      issue_access_error(fundamental_symbol_of(locator->specific_symbol),
                         &locator->source_position);
      locator->access_control_error_reported = TRUE;
    }  /* if */
  }  /* if */
}  /* overload_check_ambiguity_and_verify_access */


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


static a_boolean have_derived_class_access_from_befriending_list(
                                       a_class_list_entry_ptr befriending_list,
                                       a_type_ptr             class_type)
/*
We have member access privilege to the classes on the given befriending list.
Return TRUE if that means that we have member access to any derived
class of class_type.  This is used for the ARM 11.5 protected member
access check.
*/
{
  a_boolean accessible = FALSE;

  for (; befriending_list != NULL; befriending_list = befriending_list->next) {
    if (find_base_class_of(befriending_list->class_type, class_type) != NULL) {
      accessible = TRUE;
      break;
    }  /* if */
  }  /* for */
  return accessible;
}  /* have_derived_class_access_from_befriending_list */


static a_boolean have_derived_class_access_from_class_scope(
                                            a_type_ptr              class_type,
                                            a_scope_stack_entry_ptr ssep)
/*
We have member access privilege to the class indicated by the scope stack
entry pointed to by ssep.  Return TRUE if that means that we have member
access to any derived class of class_type.  This is used for the ARM 11.5
protected member access check.
*/
{
  a_boolean  have_derived_class_access = FALSE;
  a_type_ptr scope_class = ssep->assoc_type;

  if (find_base_class_of(scope_class, class_type) != NULL) {
    /* We are in a class that is a derived class of class_type. */
    have_derived_class_access = TRUE;
  } else if (have_derived_class_access_from_befriending_list(scope_class->
                    variant.class_struct_union.extra_info->befriending_classes,
                                                                 class_type)) {
    /* We are in a class that is a friend of a derived class of class_type. */
    have_derived_class_access = TRUE;
  }  /* if */
  return have_derived_class_access;
}  /* have_derived_class_access_from_class_scope */


static a_boolean have_member_access_to_derived_class(a_type_ptr class_type)
/*
Return TRUE if we have member access to some derived class of class_type.
This is used for the ARM 11.5 protected member access check.
*/
{
  a_boolean have_member_privilege =
                 have_particular_member_access_privilege(
                               class_type,
                               have_derived_class_access_from_befriending_list,
                               have_derived_class_access_from_class_scope);
  return have_member_privilege;
}  /* have_member_access_to_derived_class */


static a_boolean have_member_access_to_some_class_on_derivation(
                                                          a_base_class_ptr bcp)
/*
Return TRUE if we have member access to some class on the derivation of
the base class bcp.
*/
{
  a_boolean                   have_access = FALSE;
  a_base_class_derivation_ptr bcdp;
  a_derivation_step_ptr       dsp;

  /* For each derivation (virtual base classes can have more than one): */
  for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
    /* For each step on the derivation path, check to see if we have
       member access to the class. */
    for (dsp = bcdp->path; dsp != NULL; dsp = dsp->next) {
      a_base_class_ptr base_class = dsp->base_class;
      if (dsp->next != NULL && base_class->is_virtual) {
        /* Virtual base class.  Do a recursive call to process the
           derivations of the virtual base class. */
        if (have_member_access_to_some_class_on_derivation(base_class)) {
          have_access = TRUE;
          goto have_accessibility;
        }  /* if */
      } else {
        /* Simple base class case. */
        if (have_member_access_privilege(base_class->type)) {
          /* Found a class to which we have member access. */
          have_access = TRUE;
          goto have_accessibility;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* for */
have_accessibility:
  return have_access;
}  /* have_member_access_to_some_class_on_derivation */


void f_check_protected_member_access(a_symbol_ptr      sym_param,
				     a_source_position *err_pos,
                                     a_type_ptr        access_class)
/*
This routine implements the access control check mandated by ARM 11.5, which
requires that a protected nonstatic member accessed from a friend or member
function of a derived class be accessed through an object of the derived
class or a class further derived from that.  sym_param points to the
symbol being referenced, which may be a projection symbol.
access_class is the class of the pointer or object through which the
member is being accessed.  access_class is NULL if we don't know the object
type (which will cause an error).  access_class may also be an error type
(which will cause no error).  *err_pos is the source position for an error.
See the macro check_protected_member_access for a convenient way to invoke
this function.  This routine is only called for protected nonstatic members.
*/
{
  a_boolean        have_access;
  a_symbol_ptr	   sym = fundamental_symbol_of(sym_param);
  a_type_ptr       base_class = sym->parent.class_type;
  a_base_class_ptr bcp;

  if (access_class == NULL) {
    /* Class is unknown; error. */
    have_access = FALSE;
  } else if (is_error_type(access_class)) {
    /* Class is an error type; no error. */
    have_access = TRUE;
  } else {
    access_class = skip_typerefs(access_class);
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
      have_access = have_member_access_to_some_class_on_derivation(bcp);
    }  /* if */
  }  /* if */
  if (!have_access) {
    /* The fact that this routine is called means that the protected
       member is accessible under the normal rules.  (If an accessibility
       error is issued, the call of this routine is suppressed.)
       Ordinarily, that means we know we are in a friend of member of
       a class derived from the class of the protected member, or in a friend
       or member of the member class itself.  Namespace using declarations
       in classes, however, bring up the strange case that a protected
       member can be made public in a derived class, which means we might
       get to this routine even though we do not have any special member
       access to the protected member.  Check for that, and suppress the
       error if so. */
    if (!have_member_access_to_derived_class(base_class)) {
      have_access = TRUE;
    }  /* if */
  }  /* if */
  if (!have_access) {
    pos_syty_diagnostic(es_discretionary_error, ec_protected_access_problem,
                        err_pos, sym, access_class);
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
          is_accessible_direct_base_class_derivation(base_class, step_bcdp,
                                                     curr_type)) {
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


/*
Representation of a base class declaration that is a candidate for
inheritance; that is, it represents the base class entity that is projected
into a derived class as an sk_projection symbol.
*/
typedef struct a_progenitor *a_progenitor_ptr;
typedef struct a_progenitor {
  a_progenitor_ptr
		next;
			/* Next in a linked list of progenitors; NULL for the
			   last entry on the list. */
  a_symbol_ptr	sym;
			/* Pointer to the progenitor symbol -- the symbol that
			   is projected into the derived class when the name
			   is inherited. */
  a_derivation_step_ptr
		path;
			/* Derivation path from the most derived class to the
			   base class in which the progenitor declaration
			   appears. */
  an_access_specifier
		access;
			/* The access of the progenitor symbol within the
			   derived class. */
} a_progenitor;

static a_progenitor_ptr
		avail_progenitors;
			/* Linked list of progenitor entries that are
			   available for reuse; may be NULL. */

static a_progenitor_ptr alloc_progenitor(void)
/*
Allocate a progenitor entry, initialize its fields, and return a pointer to it.
*/
{
  a_progenitor_ptr  pp;

  if (avail_progenitors == NULL) {
    /* Nothing on the available list to use. */
    pp = (a_progenitor_ptr)alloc_fe(sizeof(a_progenitor));
#if DEBUG
    num_progenitors_allocated++;
#endif /* DEBUG */
  } else {
    /* Use the entry that heads the available list. */
    pp = avail_progenitors;
    avail_progenitors = pp->next;
  }  /* if */
  pp->next = NULL;
  pp->sym = NULL;
  pp->path = NULL;
  pp->access = (an_access_specifier)as_public;
  return pp;
}  /* alloc_progenitor */


static void free_progenitor(a_progenitor_ptr  pp)
/*
Return a progenitor entry to the available list.
*/
{
  if (pp->path != NULL) free_derivation_step(pp->path);
  pp->next = avail_progenitors;
  avail_progenitors = pp;
}  /* free_progenitor */


static void free_progenitor_list(a_progenitor_ptr  pp)
/*
Return a linked list of progenitor entries to the available list.
*/
{
  a_progenitor_ptr  next;

  for (; pp != NULL; pp = next) {
    next = pp->next;
    free_progenitor(pp);
  }  /* while */
}  /* free_progenitor_list */


/* Forward declaration. */
static a_progenitor_ptr find_progenitor(
			a_type_ptr                class_ptr,
                        a_symbol_locator          *locator,
                        an_id_lookup_options_set  options,
		        a_boolean		  look_in_dependent_bases);

static a_progenitor_ptr find_progenitor_in_base_class(
                        a_base_class_ptr          base_class,
                        a_symbol_locator          *locator,
                        an_id_lookup_options_set  options,
		        a_boolean		  look_in_dependent_bases)
/*
Given a pointer to a base class and a locator, determine whether the name
specified in the locator is declared either in the base class itself or in
a class from which the base class is derived.  Such a declaration is
referred to as the "progenitor" of a projection symbol, which may or may
not be created later.  If such a progenitor is found, return a pointer to
a progenitor entry (which, in the case of ambiguity, may be the head of a
linked list of progenitor entries); otherwise, return NULL.
*/
{
  a_symbol_ptr      sym, tag_sym, using_decl_sym = NULL;
  a_scope_ptr       scope;
  a_boolean	    must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
  a_progenitor_ptr  progenitor, pp;

  db_enter(4, "find_progenitor_in_base_class");
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
        /* Ignore sk_undefined symbols. */
        if (sym->kind == (a_symbol_kind)sk_undefined) continue;
        /* Ignore this symbol if it doesn't match the lookup options
           specified by the caller. */
        if (!sym_matches_lookup_options(sym, options)) continue;
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
            /* In C++ mode members are always in the nsk_other name space. */
            check_assertion_str2(
                     name_space_for_symbol_kind[(int)sym->kind] == nsk_other,
                     "find_progenitor_in_base_class:",
                     "unexpected name space kind");
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
    if (sym != NULL && sym->ambiguous) {
      /* Don't treat an ambiguous symbol as a progenitor; add each of its
         own progenitor symbols to the progenitor set that's returned to
         the caller. */
      if (is_class_member_using_decl_symbol(sym)) {
        /* Remember this symbol, however, so that the access recorded in the
           progenitor entry can be corrected. */
        using_decl_sym = sym;
      }  /* if */
      /* Set sym to NULL to force a call to find_progenitor. */
      sym = NULL;
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    /* Found in the base class itself. */
#if DEBUG
    if (debug_level >= 4) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
    progenitor = alloc_progenitor();
    progenitor->sym = sym;
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      progenitor->access = max_access_of_overloaded_function(sym);
    } else if (sym->kind == (a_symbol_kind)sk_projection) {
      progenitor->access = sym->variant.projection.access;
    } else {
      progenitor->access = access_for_symbol(sym);
    }  /* if */
  } else {
    /* Not found in the current base class, so examine its own base classes,
       if any.  Note that a linked list of progenitor entries may be returned
       -- this usually represents an ambiguity. */
    progenitor = find_progenitor(base_class->type, locator, options,
				 look_in_dependent_bases);
  }  /* if */
  /* Update the path and access fields of each entry in the set of
     progenitors.  (There will usually be only one.) */
  for (pp = progenitor; pp != NULL; pp = pp->next) {
    /* The path is not augmented if it starts with a virtual base class,
       unless it is only a single step.  This is consistent with the way
       derivations are constructed for base classes: the steps between the
       most derived class and an intermediate virtual base class are elided. */
    if (pp->path == NULL || pp->path->next == NULL ||
        !pp->path->base_class->is_virtual) {
      pp->path = make_derivation_step(base_class, pp->path);
    }  /* if */
    if (using_decl_sym != NULL) {
      pp->access = using_decl_sym->variant.projection.access;
    }  /* if */
    pp->access = compute_access(pp->access,
                                preferred_derivation_of(base_class)->access);
  }  /* for */
  db_exit();
  return progenitor;
}  /* find_progenitor_in_base_class */


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

  db_enter(4, "path_to_fundamental_symbol_base_class");
  /* Note that corresponding_base_class is not called, since it is hard to
     compute a disambiguator that is immediately derived from the base
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
  check_assertion_str(path != NULL,
                      "path_to_fundamental_symbol_base_class: not found");
  db_exit();
  return path;
}  /* path_to_fundamental_symbol_base_class */


static a_boolean progenitors_are_equivalent(a_progenitor_ptr  progenitor1,
                                            a_progenitor_ptr  progenitor2)
/*
Given two progenitors (referring to symbols projected into the same class
from two different base classes), return TRUE if their respective fundamental
symbols are the same (not only the same members of the same class but with
equivalent derivations).
*/
{
  a_symbol_ptr           sym1 = progenitor1->sym, sym2 = progenitor2->sym;
  a_derivation_step_ptr  path1 = progenitor1->path, path2 = progenitor2->path;
  a_boolean              equiv = FALSE;
  a_symbol_ptr           fundamental_sym1;
  a_type_ptr             rout_type;
  a_derivation_step_ptr  tail1, tail2;

  db_enter(4, "progenitors_are_equivalent");
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
          a_symbol_ptr  sym = fundamental_sym1->
                                    variant.overloaded_function.symbols;
          if (sym->kind != (a_symbol_kind)sk_function_template) {
            rout_type = routine_symbol_type(sym);
          } else {
            rout_type = sym->variant.template_info->
                                         variant.function.routine->type;
          }  /* if */
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
}  /* progenitors_are_equivalent */


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
  for (bcp = base_classes_of(sym1->parent.class_type);
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
        for (step = path_to_sym2; step->next != NULL; step = step->next) {}
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


static a_progenitor_ptr find_progenitor(
			a_type_ptr               class_ptr,
                        a_symbol_locator         *locator,
                        an_id_lookup_options_set options,
			a_boolean		 look_in_dependent_bases)
/*
Given a pointer to a class (or struct or union) type and a locator, find
in the classes from which the current class is derived symbols that would
serve as "progenitors" if the name specified in the locator is inherited in
a derived class.  Return a pointer to one or more progenitor entries (or NULL
if no such base-class symbol is found).
*/
{
  a_base_class_ptr       bcp;
  a_progenitor_ptr       progenitor_set = NULL, pp, prev, next;
  a_progenitor_ptr       new_set, new_pp, prev_in_new_set, next_in_new_set;
  a_boolean              retain_pp, retain_new_pp;  

  db_enter(4, "find_progenitor");
  bcp = class_ptr->variant.class_struct_union.extra_info->base_classes;
  /* Loop through the base classes. */
  for (; bcp != NULL; bcp = bcp->next) {
    /* When doing dependent name lookup certain base classes should be
       ignored for unqualified lookups. */
    if (do_dependent_name_processing && !look_in_dependent_bases &&
        bcp->ignore_during_dependent_lookup) continue;
    /* For the most part, we are only interested in the direct base classes
       (either virtual or nonvirtual).  However, it may happen that a virtual
       base class is marked as "direct" yet the path of greatest access is
       that of an indirect derivation; such cases are treated as indirect
       base classes. */
    if (preferred_derivation_is_direct(bcp)) {
      new_set = find_progenitor_in_base_class(bcp, locator, options,
					      look_in_dependent_bases);
      if (new_set != NULL) {
        if (progenitor_set == NULL) {
          progenitor_set = new_set;
        } else {
          /* There's at least one item in each set.  Look for equivalence
             and dominance (which will allow eliminating some items) and
             then merge the sets. */
          prev_in_new_set = NULL;
          for (new_pp = new_set; new_pp != NULL; new_pp = next_in_new_set) {
            next_in_new_set = new_pp->next;
            prev = NULL;
            retain_new_pp = TRUE;
            for (pp = progenitor_set; pp != NULL; pp = next) {
              next = pp->next;
              retain_pp = TRUE;
              if (pp->sym->ambiguous || new_pp->sym->ambiguous) {
                /* Don't do any of the comparison tests (equivalence,
                   dominance) -- they depend on the fundamental symbol, but
                   that's not really reliable with an ambiguous name. */
              } else if (progenitors_are_equivalent(pp, new_pp)) {
                /* No ambiguity (presumably because sym and other_sym are the
                   same member of a virtually derived class); choose between
                   the two projections based on access. */
                if (is_more_accessible(new_pp->access, pp->access)) {
                  /* Remove pp from the progenitor set. */
                  retain_pp = FALSE;
                } else {
                  /* Remove new_pp from the new progenitor set. */
                  retain_new_pp = FALSE;
                }  /* if */
              } else if (check_for_dominance(pp->sym, new_pp->sym,
                                             new_pp->path, class_ptr)) {
                /* pp->sym dominates new_pp->sym, resolving a potential
                   ambiguity.  Remove new_pp from the new progenitor set. */
                retain_new_pp = FALSE;
              } else if (check_for_dominance(new_pp->sym, pp->sym, pp->path,
                                             class_ptr)) {
                /* new_pp->sym dominates pp->sym, resolving a potential
                   ambiguity.  Remove pp from the progenitor set. */
                retain_pp = FALSE;
              } else {
                /* An unresolved ambiguity.  Both entries will be retained. */
              }  /* if */
              if (!retain_pp) {
                /* The current entry in progenitor_set is to be eliminated.
                   Branch around pp and return it to the available list. */
                if (prev == NULL) {
                  progenitor_set = next;
                } else {
                  prev->next = next;
                }  /* if */
                free_progenitor(pp);
                /* Continue the inner loop.  Note that prev is not changed. */
              } else if (!retain_new_pp) {
                /* The current entry in new_set is to be eliminated.  Branch
                   around new_pp and return it to the available list. */
                if (prev_in_new_set == NULL) {
                  new_set = next_in_new_set;
                } else {
                  prev_in_new_set->next = next_in_new_set;
                }  /* if */
                free_progenitor(new_pp);
                /* Break out of the inner loop and advance to the next
                   member of the new progenitor set.  prev_new_pp should not
                   be adjusted before continuing the outer loop. */
                break;
              } else {
                /* Nothing was eliminated from either list.  Reset the
                   pointer that tracks the previous item on the list, for
                   use the next time through the inner loop. */
                prev = pp;
              }  /* if */
            }  /* for */
            if (retain_new_pp) {
              /* Reset the pointer that tracks the previous item on the list,
                 for use the next time through the outer loop.  (This is not
                 done when new_pp is eliminated -- in that case, the current
                 pointer is still valid.) */
              prev_in_new_set = new_pp;
            }  /* if */
          }  /* for */
          /* Now merge what's left of the two lists. */
          if (new_set != NULL) {
            if (progenitor_set == NULL) {
              progenitor_set = new_set;
            } else {
              for (pp = progenitor_set; pp->next != NULL; pp = pp->next) { }
              pp->next = new_set;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
  return progenitor_set;
}  /* find_progenitor */


a_symbol_ptr find_progenitor_symbol(
                      a_type_ptr               class_ptr,
                      a_symbol_locator         *locator,
                      an_id_lookup_options_set options,
		      a_boolean		       look_in_dependent_bases,
                      a_derivation_step_ptr    *path,
                      an_access_specifier      *access,
                      a_boolean                *ambiguous,
                      a_boolean                *any_using_decl,
                      a_boolean                *unambiguous_injected_template)
/*
Given a pointer to a class (or struct or union) type and a locator, find
in the classes from which the current class is derived a symbol that
would serve as progenitor of the name specified in the locator.  Return the
progenitor symbol or NULL is none is found.  Set *ambiguous to TRUE if
there is more than one progenitor.  *path and *access (and *ambiguous as well
under certain circumstances) may be set by subroutines and are just passed
through back to the caller.  *any_using_decl is set if any progenitor candidate
represents a using declaration or is or the projection of symbol that does.
*unambiguous_injected_template is set when class_name_injection_enabled is
TRUE, when *ambiguous is also set, and when all members of the progenitor
set are instances of the same template.  look_in_dependent_bases is TRUE if
the lookup should consider dependent bases classes of generated template
classes.

*/
{
  a_symbol_ptr      progenitor_sym;
  a_progenitor_ptr  progenitor_set, progenitor, pp;

  db_enter(4, "find_progenitor_symbol");
  /* Get what may be a linked list of progenitor entries. */
  progenitor_set = find_progenitor(class_ptr, locator, options,
                                   look_in_dependent_bases);
  if (progenitor_set == NULL) {
    /* Empty list.  Return NULL. */
    progenitor_sym = NULL;
  } else {
    progenitor = progenitor_set;
    progenitor_sym = progenitor->sym;
    if (progenitor->next == NULL) {
      /* Only one entry on the list. */
      *ambiguous = progenitor_sym->ambiguous;
    } else {
      /* A list of entries.  Select one to return as the progenitor symbol.
         If one of the symbols represents a type name, return that symbol.
         (This makes a difference in declaration processing, whereas in
         executable expression processing only the ambiguity is of interest.)
         Otherwise just return the first symbol seen. */
      if (!is_type_symbol(fundamental_symbol_of(progenitor_sym))) {
        for (pp = progenitor->next; pp != NULL; pp = pp->next) {
          if (is_type_symbol(fundamental_symbol_of(pp->sym))) {
            progenitor = pp;
            progenitor_sym = progenitor->sym;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      *ambiguous = TRUE;
    }  /* if */
    /* If this projection is ambiguous, it may be appropriate to set a
       flag indicating that it is ambiguous for instances of a class template
       but not for the template itself.  E.g.,
         struct B : A<int>, A<double> { ... };
       A reference to "A" within B is ambiguous if one is interested in a
       class but unambiguous if one is interested in a template.  (This is
       an issue only when class-name-injection is enabled.) */
    if (*ambiguous && class_name_injection_enabled) {
      a_symbol_ptr  sym, templ_sym = NULL;

      /* Set the flag to TRUE and look to change it back to FALSE. */
      *unambiguous_injected_template = TRUE;
      /* Traverse the list of progenitor symbols. */
      for (pp = progenitor_set; pp != NULL; pp = pp->next) {
        sym = pp->sym;
        if (progenitor_sym->kind == (a_symbol_kind)sk_projection) {
          /* Special handling when the progenitor is itself a projection. */
          if (progenitor_sym->ambiguous &&
              !progenitor_sym->variant.projection.
                              injected_class_template_name_is_unambiguous) {
            *unambiguous_injected_template = FALSE;
            break;
          }  /* if */
          sym = fundamental_symbol_of(sym);
        }  /* if */
        if (is_injected_template_symbol(sym)) {
          /* The name is a projection of an injected class template name. */
          sym = class_template_for_injected_template_symbol(sym);
          if (templ_sym == NULL) {
            /* Must be the first time through the loop. */
            templ_sym = sym;
          } else if (templ_sym != sym) {
            /* The templates don't match. */
            *unambiguous_injected_template = FALSE;
            break;
          }  /* if */
        } else {
          *unambiguous_injected_template = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    *access = progenitor->access;
    /* Move the derivation path from the progenitor entry and return it to
       the caller.  Clear the pointer in the progenitor; otherwise the path
       would be freed. */
    *path = progenitor->path;
    progenitor->path = NULL;
    *any_using_decl =
          (progenitor_sym->kind == (a_symbol_kind)sk_projection &&
           (progenitor_sym->variant.projection.is_using_decl ||
            progenitor_sym->variant.projection.any_intervening_using_decl));
    /* Return the progenitor entries to the available list. */
    free_progenitor_list(progenitor_set);
  }   /* if */
  db_exit();
  return progenitor_sym;
}  /* find_progenitor_symbol */

    
static a_symbol_ptr create_nonreal_progenitor_symbol(
					 a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator,
                                         a_derivation_step_ptr	  *path)
/*
Find a nonreal base class of class_type, create a member of that nonreal
base class, and return the symbol to the caller.  Create a derivation step
entry that points to the class in which the nonreal member is created.
*/
{
  a_symbol_ptr		sym;
  a_base_class_ptr	bcp = base_classes_of(class_type);
  a_base_class_ptr	nonreal_bcp = NULL;

  /* Loop through the base classes to find a nonreal base.  A direct
     nonreal base is found, if possible.  Otherwise, the first indirect
     nonreal base is used. */
  for (; bcp != NULL; bcp = bcp->next) {
    a_type_ptr		base_type = bcp->type;
    if (base_type->variant.class_struct_union.is_nonreal_class) {
      if (bcp->direct) {
        nonreal_bcp = bcp;
        break;
      }  /* if */
      if (nonreal_bcp != NULL) nonreal_bcp = bcp;
    }  /* if */
  }  /* for */
  check_assertion_str2(nonreal_bcp != NULL,
                       "create_nonreal_progenitor_symbol:", "no nonreal base");
  sym = class_qualified_id_lookup(locator, nonreal_bcp->type,
                                  options | IDL_MEMBER_OF_UNKNOWN_BASE);
  check_assertion(sym != NULL);
  *path = make_derivation_step(nonreal_bcp, (a_derivation_step_ptr)NULL);
  return sym;
}  /* create_nonreal_progenitor_symbol */


static a_boolean check_for_microsoft_template_lookup_bug(a_symbol_ptr sym)
/*
The Microsoft compiler (as of version 4.2) includes a bug in the lookup
of template names that, in the following example, will find the global
template x instead of the base class member.

  template <class T> struct x {};
  class A {
    int x;
  };
  class B : public A {
    typedef x<int> xi;  // Microsoft compiler finds template ::x
  };

For this to occur the base class member must be a nonstatic member, or an
overload set containing nonstatic members.

sym is the progenitor symbol that was found.  Return TRUE if it represents
a symbol that should be ignored in favor of a template to be found later.
*/
{
  a_boolean	result = FALSE;

  if (sym->kind == (a_symbol_kind)sk_field) {
    /* The name found is a nonstatic data member -- discard it. */
    result = TRUE;
  } else if (is_function_symbol(sym)) {
    a_boolean		mixed_static_nonstatic = FALSE;
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      mixed_static_nonstatic =
                      sym->variant.overloaded_function.mixed_static_nonstatic;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    if (mixed_static_nonstatic) {
      /* There is at least one static member function.  This symbol must
         not be ignored. */
    } else {
      a_routine_ptr			rp;
      a_routine_type_supplement_ptr	rtsp;
      /* All of the symbols are either static or all are nonstatic.
         Check the first symbol on the list to see which. */
      rp = sym->variant.routine.ptr;
      rtsp = rp->type->variant.routine.extra_info;
      /* If the name found is a nonstatic member function, discard it. */
      if (rtsp->this_class != NULL) result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* check_for_microsoft_template_lookup_bug */


static a_boolean check_for_microsoft_type_lookup_bug(a_symbol_ptr sym)
/*
The Microsoft compiler (as of version 6.0) includes a bug in the lookup
of type names in class definitions.  The caller is responsible for verifying
that the current scope is a class definition.  The bug does not occur
in class reactivations.

In Microsoft mode, the injected class name is not normally found, so
a reference to "Y" from within struct Y normally finds the base class
member and not the injected class name.

The Microsoft compiler seems to do a special lookup of the type name
in a declaration in a class definition.  This lookup considers only type
names from base classes (i.e., it ignores nontypes):

  struct A {
    int Y;
  };
  template <class T> struct Y : public T {
    Y* p;  // ::Y not A::Y
  };

sym is the symbol found from a base class.  Return TRUE if this is a
symbol that should be ignored as a result of the Microsoft bug.
*/
{
  a_boolean	result = FALSE;
  a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);

  if (!is_type_symbol(fund_sym)) {
    /* Not a type -- should always be ignored. */
    result = TRUE;
  }  /* if */
  return result;
}  /* check_for_microsoft_type_lookup_bug */


static a_boolean found_in_dependent_base(a_derivation_step_ptr	dsp)
/*
Return TRUE if any of the base classes on the derivation path specified
by "dsp" is a base class that should be ignored during dependent
lookup.
*/
{
  a_boolean	result = FALSE;

  for (; dsp != NULL; dsp = dsp->next) {
    if (dsp->base_class->ignore_during_dependent_lookup) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* found_in_dependent_base */


a_boolean find_projected_symbol(
			a_type_ptr               class_ptr,
                        a_symbol_locator         *locator,
                        an_id_lookup_options_set options,
			a_boolean		 look_in_dependent_bases,
                        a_boolean                tentative_type_lookup,
                        a_boolean                tentative_template_lookup,
			a_boolean		 do_not_create_proj_sym,
                        a_boolean                add_to_active_list,
                        a_symbol_ptr             insert_sym,
                        a_symbol_ptr             *projected_symbol,
                        a_boolean		 can_create_nonreal)
/*
Given class_ptr, which identifies a class (or struct or union) type,
search its base classes for a symbol that projects the name specified
in *locator into the class.  If such a symbol is found, create a
projection symbol for it (marked "ambiguous" if there is more than one
possible progenitor) and return it to the caller; otherwise, return
NULL.  The new symbol is added to the symbol table in one of two ways,
depending on how add_to_active_list is set: if the flag is FALSE, the
new symbol is added to the beginning of the locator's inactive list;
if it is TRUE, it is inserted in the locator's active list (which is
order dependent) immediately following insert_sym (or, if insert_sym
is NULL, at the beginning of the list), and in addition it is added to
the end of the scope entry symbol list for the class.  The symbol
found must meet the criteria indicated by "options".

look_in_dependent_bases is TRUE if the lookup should consider dependent
base classes of generated temple classes.  If tentative_type_lookup is
TRUE, a projection symbol is only created if the symbol returned by
find_progenitor_symbol is a type.  Likewise, if tentative_template_lookup
is TRUE, a projection symbol is only created if the symbol returned by
find_progenitor_symbol is a template.  If do_not_create_proj_sym is TRUE
the creation of a projection symbol is unconditionally suppressed.  Note
that "options" and tentative_type_lookup are handled differently: a symbol
that fails the lookup options test does not hide symbols from deeper base
classes, while a symbol that is not a type does hide symbols from deeper
base classes that may be types.

can_create_nonreal is TRUE if, when looking for a projected symbol in a
class with a nonreal base, a member of the nonreal base should be
created if a projected symbol cannot be found in any of the real bases.
*/
{
  a_derivation_step_ptr		path = NULL;
  a_symbol_ptr			progenitor_sym;
  a_symbol_ptr			new_sym = NULL;
  an_access_specifier		access;
  a_boolean			ambiguous = FALSE;
  a_boolean			found;
  a_scope_stack_entry_ptr	ssep;
  a_scope_pointers_block_ptr	pointers_block;
  a_symbol_ptr			class_sym;
  a_class_symbol_supplement_ptr	cssp;
  a_boolean			any_using_decl = FALSE;
  a_boolean		        fund_sym_is_nonreal_member = FALSE;
  a_boolean                     unambiguous_injected_template = FALSE;

  db_enter(4, "find_projected_symbol");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for projection of \"%s\" into class \"%s\"\n",
                     locator->symbol_header->identifier,
                     class_ptr->source_corresp.name != NULL ?
                         class_ptr->source_corresp.name : "<unnamed>");
  }  /* if */
#endif /* DEBUG */
  class_sym = (a_symbol_ptr)class_ptr->source_corresp.assoc_info;
  cssp = class_sym->variant.class_struct_union.extra_info;
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
    progenitor_sym = find_progenitor_symbol(class_ptr, locator, options,
                                            look_in_dependent_bases,
                                            &path, &access, &ambiguous,
                                            &any_using_decl,
                                            &unambiguous_injected_template);
    if (microsoft_bugs && progenitor_sym != NULL) {
      if (tentative_template_lookup) {
        /* In Microsoft bugs mode, if the progenitor symbol is for a nonstatic
           member (data or function), and we are doing a tentative template
           lookup, ignore this symbol. */
        if (check_for_microsoft_template_lookup_bug(progenitor_sym)) {
          progenitor_sym = NULL;
        }  /* if */
      } else if (tentative_type_lookup &&
                 scope_stack[depth_scope_stack].kind ==
                                        (a_scope_kind)sck_class_struct_union) {
        /* In Microsoft bugs mode, ignore non-types found by a tentative
           type lookup and continue looking for the symbol in enclosing
           scopes.  This bug only occurs in certain contexts in class
           definitions. */
        if (check_for_microsoft_type_lookup_bug(progenitor_sym)) {
          progenitor_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (progenitor_sym == NULL && can_create_nonreal &&
      cssp->any_nonreal_base_classes) {
    /* The symbol was not found in the class or in any of its "real"
       base classes.  This class has nonreal base classes, so we will
       assume that the name being looked up is a member of one of the
       nonreal base classes. */
    progenitor_sym = create_nonreal_progenitor_symbol(class_ptr, options,
                                                      locator, &path);
    /* Assume that the member is publicly accessible. */
    access = (an_access_specifier)as_public;
    fund_sym_is_nonreal_member = TRUE;
  }  /* if */
  if (progenitor_sym == NULL) {
    /* Indicate that no symbol was found and return a NULL pointer. */
    found = FALSE;
  } else {
    /* A symbol was found. */
    a_symbol_ptr	fund_progenitor_sym =
                                fundamental_symbol_of(progenitor_sym);
    found = TRUE;
    if (do_not_create_proj_sym) {
      /* Don't create a projection symbol and return the progenitor symbols
         as the result of the lookup (for hidden name lookups and other special
         lookups). */
      new_sym = progenitor_sym;
    } else if (tentative_type_lookup &&
               !(is_type_symbol(fund_progenitor_sym) ||
                 is_class_template_symbol(fund_progenitor_sym))) {
      /* The symbol found is not a type name symbol, so do not create a
         projection for it.  A class template symbol is returned on a
         tentative type lookup for improved error recovery. */
    } else if (tentative_template_lookup &&
               !symbol_is_or_contains_template(fund_progenitor_sym)) {
      /* The symbol found is not a template name symbol, so do not create a
         projection for it. */
    } else {
      /* Create a new symbol based on the symbol returned. */
      new_sym = make_projection_symbol(progenitor_sym, class_ptr,
                                       (a_base_class_ptr)NULL, path,
                                       ambiguous);
      new_sym->variant.projection.access = access;
      /* Set the flag indicating whether there are any intervening access
         declarations on any inheritance path linking the current class to
         the fundamental symbol. */
      new_sym->variant.projection.any_intervening_using_decl = any_using_decl;
      new_sym->variant.projection.fund_sym_is_nonreal_member =
                                                   fund_sym_is_nonreal_member;
      /* Mark projection symbols for names in dependent base classes as
         invisible.  Such projection symbols should not be found by normal
         lookup (because the underlying symbol would not be found). */
      if (do_dependent_name_processing &&
          is_unspecialized_template_class(class_ptr)) {
        new_sym->is_invisible = found_in_dependent_base(path);
      }  /* if */
      if (new_sym->ambiguous) {
        new_sym->
          variant.projection.injected_class_template_name_is_unambiguous =
                                                unambiguous_injected_template;
      }  /* if */
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
        pointers_block = assoc_pointers_block_of(ssep);
        if (pointers_block->symbols != NULL) {
          pointers_block->last_symbol->next_in_scope = new_sym;
        } else {
          pointers_block->symbols = new_sym;
        }  /* if */
        pointers_block->last_symbol = new_sym;
      } else {
        /* Add it to the inactive list.  It can go at the beginning. */
        add_symbol_to_inactive_list(new_sym);
      }  /* if */
#if DEBUG
      if (debug_level >= 4) db_symbol(new_sym, "symbol created: ", 2);
#endif /* DEBUG */
    }  /* if */
    check_assertion(path != NULL);
    free_derivation_step(path);
  }  /* if */
  *projected_symbol = new_sym;

  db_exit();
  return found;
}  /* find_projected_symbol */


void add_vla_fixup_entry(a_type_ptr        array_type,
                         an_expr_node_ptr  expr_node,
                         a_symbol_ptr      param_sym,
                         a_source_position *position)
/*
Allocate and initialize a VLA fixup entry.  expr_node is an expression node
and will never be NULL.  Either array_type or param_sym will be non-NULL (but
not both).  (See comments on the definition of a_vla_fixup for further
details.)  The current scope will be a function prototype scope.  Add the
fixup entry to the end of the vla_fixup_list of the current scope stack entry.
*/
{
  a_vla_fixup_ptr          vfp;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  db_enter(5, "add_vla_fixup_entry");
  if (avail_vla_fixups != NULL) {
    vfp = avail_vla_fixups;
    avail_vla_fixups = vfp->next;
  } else {
    vfp = (a_vla_fixup_ptr)alloc_fe(sizeof(a_vla_fixup));
#if DEBUG
    num_vla_fixups_allocated++;
#endif /* DEBUG */
  }  /* if */
#if CHECKING
  check_assertion_str(ssep->kind == (a_scope_kind)sck_func_prototype,
                      "add_vla_fixup_entry: not func-prototype scope");
  check_assertion_str(expr_node != NULL,
                      "add_vla_fixup_entry: NULL expr node");
  if (array_type == NULL) {
    /* param_sym must be non-NULL and must be an sk_parameter symbol. */
    check_assertion_str(param_sym != NULL &&
                          param_sym->kind == (a_symbol_kind)sk_parameter,
                        "add_vla_fixup_entry: bad parameter symbol");
    /* The expression node should be the result of scanning the dummy
       parameter variable. */
    check_assertion_str(expr_node->kind == (an_expr_node_kind)enk_variable ||
                        expr_node->kind ==
                                    (an_expr_node_kind)enk_variable_address,
                        "add_vla_fixup_entry: bad expression node");
  } else {
    /* param_sym must be NULL and array_type must refer to a tk_array. */
    check_assertion_str(param_sym == NULL &&
                          array_type->kind == (a_type_kind)tk_array,
                        "add_vla_fixup_entry: bad array type");
  }  /* if */
#endif /* CHECKING */
  vfp->next = NULL;
  vfp->array_type = array_type;
  vfp->expr = expr_node;
  vfp->param_sym = param_sym;
  vfp->position = *position;
  if (ssep->vla_fixup_list == NULL) {
    ssep->vla_fixup_list = vfp;
  } else {
    a_vla_fixup_ptr  end_of_list = ssep->vla_fixup_list;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = vfp;
  }  /* if */
  db_exit();
}  /* add_vla_fixup_entry */


void free_vla_fixup_list(a_vla_fixup_ptr vfp)
/*
Add the indicated list of vla fixup entries to the available list.
*/
{
  if (avail_vla_fixups == NULL) {
    avail_vla_fixups = vfp;
  } else if (vfp != NULL) {
    /* Find the last entry on the list. */
    a_vla_fixup_ptr  end_of_list = vfp;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    end_of_list->next = avail_vla_fixups;
    avail_vla_fixups = vfp;
  }  /* if */
}  /* free_vla_fixup_list */


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
  pip->declared_type = NULL;
  pip->type_pos = null_source_position;
  pip->storage_class = (a_storage_class)sc_unspecified;
  pip->implicitly_declared = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  pip->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  pip->dummy_vla_variable = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pip->specifiers_range.start = null_source_position;
  pip->specifiers_range.end = null_source_position;
  pip->declarator_range.start = null_source_position;
  pip->declarator_range.end = null_source_position;
  pip->identifier_range.start = null_source_position;
  pip->identifier_range.end = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  pip->old_style_id_pos = null_source_position;
  db_exit();
  return(pip);
}  /* alloc_param_id */


static void free_param_id(a_param_id_ptr *ppip)
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


void defer_exception_spec_error(a_func_info_block  *func_info,
                                an_error_code      error_code,
                                a_source_position  *pos)
/*
Create an entry to record the need for an incomplete-type diagnostic on an
exception specification.  At this point in processing (while the exception
specification is being scanned), it is not known whether it belongs to a
function definition or to a non-defining declaration; since this may affect
the severity of the diagnostic, issuing the message is deferred.  func_info
points to the block in which the deferral is recorded; error_code and *pos
indicate the particular diagnostic required and the error position.
*/
{
  an_exception_spec_error_descr_ptr  esedp, end_of_list;

  db_enter(5, "defer_exception_spec_error");
  esedp = (an_exception_spec_error_descr_ptr)alloc_fe(
                                      sizeof(an_exception_spec_error_descr));
#if DEBUG
  num_exception_spec_error_descrs_allocated++;
#endif /* DEBUG */
  esedp->next       = NULL;
  esedp->position   = *pos;
  esedp->error_code = error_code;
  if (func_info->exception_spec_errors == NULL) {
    func_info->exception_spec_errors = esedp;
  } else {
    end_of_list = func_info->exception_spec_errors;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = esedp;
  }  /* if */
  db_exit();
}  /* defer_exception_spec_error */


void report_exception_spec_errors(a_func_info_block  *func_info)
/*
Report one or more deferred exception-specification errors.  Suppress the
diagnostic if this is not a definition and not in strict conformance mode.
*/
{
  an_exception_spec_error_descr_ptr  esedp;
  an_error_severity                  severity;

  esedp = func_info->exception_spec_errors;
  if (esedp != NULL) {
    if (func_info->is_definition) {
      /* Always an error on a definition. */
      severity = es_error;
    } else if (strict_ansi_mode) {
      /* Always some diagnostic in strict mode. */
      severity = strict_ansi_discretionary_severity;
    } else {
      /* Except in strict mode, suppress the diagnostic on a declaration that
         does not define a function. */
      severity = es_none;
    }  /* if */
    if (severity != es_none) {
      /* Note that a diagnostic may have been deferred for more than one
         type. */
      for (; esedp != NULL; esedp = esedp->next) {
        pos_diagnostic(severity, esedp->error_code, &esedp->position);
      }  /* for */
    }  /* if */
  }  /* if */
}  /* report_exception_spec_errors */


void clear_func_info(a_func_info_block *func_info)
/*
Clear the fields of a function information block to default values.
*/
{
  func_info->prototype_scope_symbols     = NULL;
  func_info->param_id_list               = NULL;
  func_info->exception_specification     = NULL;
  func_info->throw_position              = null_source_position;
  func_info->exception_spec_errors       = NULL;
  func_info->scope_number                = NO_SCOPE_NUMBER;
  func_info->vla_fixup_list              = NULL;
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
  func_info->is_movable_member_or_friend_def = FALSE;
  func_info->declarator_ssep                = NULL;
  func_info->declared_type                  = NULL;
  func_info->prototype_scope_ss_entry_start = NULL;
  func_info->prototype_scope_ss_entry_end   = NULL;
  func_info->prototype_scope_ss_list        = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if USER_CONTROL_OF_STRUCT_PACKING
  func_info->max_member_alignment           = 0;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
}  /* clear_func_info */


void clear_decl_modifiers_block(a_decl_modifiers_block_ptr  decl_modifiers)
/*
Clear the block passed around the declaration routines to represent
declaration modifiers.
*/
{
  decl_modifiers->flags = DM_NONE;
  decl_modifiers->direct_linkage_specifier = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  decl_modifiers->uuid_string = NULL;
  decl_modifiers->get_property_name = NULL;
  decl_modifiers->put_property_name = NULL;
  decl_modifiers->allocate_segname = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_decl_modifiers_block */


void add_to_dependent_type_fixup_list(a_type_ptr                   type_ptr,
                                      a_dependent_type_fixup_kind  fixup_kind,
                                      char                         *entity_ptr,
                                      a_byte_il_entry_kind         entity_kind,
                                      a_source_position            *pos)
/*
type_ptr is a pointer an incomplete class/struct/union or enum type, and
entity_ptr and entity_kind together identify a type or param_type dependent
on it.  An fixup entry is created and put on a list so that the dependent
entity can be modified appropriately when the class or enum type is finally
defined.
*/
{
  a_dependent_type_fixup_ptr     dtfp, end_of_list, *start_of_list;
  a_symbol_ptr                   sym;

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
  dtfp->entity.ptr = entity_ptr;
  dtfp->entity.kind = entity_kind;
  dtfp->decl_position = *pos;
  dtfp->next = NULL;
  /* Add the entry to the end of the appropriate list. */
  sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
  if (is_class_symbol(sym)) {
    /* Use the list associated with the class. */
    start_of_list = &sym->variant.class_struct_union.extra_info->
                                              dependent_type_fixup_list;
  } else {
    /* Use the list associated with the enum type. */
    check_assertion(sym->kind == (a_symbol_kind)sk_enum_tag);
    start_of_list = &sym->variant.enumeration.dependent_type_fixup_list;
  }  /* if */
  if (*start_of_list == NULL) {
    *start_of_list = dtfp;
  } else {
    end_of_list = *start_of_list;
    while (end_of_list->next != NULL) end_of_list = end_of_list->next;
    end_of_list->next = dtfp;
  }  /* if */
  db_exit();
}  /* add_to_dependent_type_fixup_list */


void check_dependent_type_fixup_list(a_symbol_ptr  sym)
/*
Go through the entries on the dependent-type-fixup list for sym, which
identifies either a class type or an enumeration type.  The list is a
registry of entities that depend on the type but were declared before it
was defined. Now that the definition is there, the rest of the declaration
can be completed for the dependent types, too.
*/
{
  a_dependent_type_fixup_ptr     dtfp, next_dtfp, prev_dtfp;
  a_dependent_type_fixup_ptr     *start_of_list, list;
  a_type_ptr                     tp;

  if (is_class_symbol(sym)) {
    /* Check the list associated with the class. */
    start_of_list = &sym->variant.class_struct_union.extra_info->
                                              dependent_type_fixup_list;
  } else {
    /* Check the list associated with the enum type. */
    check_assertion(sym->kind == (a_symbol_kind)sk_enum_tag);
    start_of_list = &sym->variant.enumeration.dependent_type_fixup_list;
  }  /* if */
  list = *start_of_list;
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
                /* An array of elements of the (now complete) class or enum
                   type.  The array's size can be computed. */
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
    *start_of_list = NULL;
  }  /* if */
}  /* check_dependent_type_fixup_list */


static
a_namespace_list_entry_ptr list_entry_for_namespace(a_namespace_ptr nsp,
                                                    a_boolean       shared)
/*
Return a pointer to a namespace list entry that points to the namespace
nsp.  The namespace symbol supplement contains a pointer to a shared
namespace list entry.  This routine allocates and initializes that entry
if it has not yet been generated.  shared is TRUE if the caller can
use an entry that will be shared between multiple lists (i.e., it is
known to be the last entry on the list).  If a shared entry is
acceptable, the entry pointed to by the namespace symbol supplement
is returned.  If a nonshared entry is required, a new entry is
allocated and returned.

Note that nsp can be NULL, in which case the variable
global_namespace_list_entry is used to save a pointer to the shared
list entry (because the global namespace has no namespace symbol
supplement).
*/
{
  a_symbol_ptr				ns_sym;
  a_namespace_list_entry_ptr		nlep;
  a_namespace_symbol_supplement_ptr	nssp;

  if (nsp == NULL) {
    /* When nsp is NULL, the namespace is the global namespace. */
    nlep = global_namespace_list_entry;
    /* If the sharable entry has not yet been allocated, allocate one now. */
    if (nlep == NULL) {
      nlep = alloc_namespace_list_entry();
      nlep->ptr = nsp;
      global_namespace_list_entry = nlep;
    }  /* if */
  } else {
    /* A "real" namespace. */
    nsp = skip_namespace_aliases(nsp);
    ns_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
    nssp = ns_sym->variant.namespace_info.extra_info;
    /* If the sharable entry has not yet been allocated, allocate one now. */
    if (nssp->namespace_list_entry == NULL) {
      nlep = alloc_namespace_list_entry();
      nlep->ptr = nsp;
      nssp->namespace_list_entry = nlep;
    }  /* if */
    nlep = nssp->namespace_list_entry;
  }  /* if */
  /* If the caller wants a shared entry, return the shared pointer.
     Otherwise allocate a new entry. */
  if (shared) {
    /* Use the value of nlep set above that points to the shared entry. */
  } else {
    /* Allocate a new entry and point it to the namespace. */
    nlep = alloc_namespace_list_entry();
    nlep->ptr = nsp;
  }  /* if */
  return nlep;
}  /* list_entry_for_namespace */


static
void add_to_operator_lookup_namespaces(a_class_symbol_supplement_ptr cssp,
                                       a_namespace_ptr		 nsp_to_add)
/*
See if the namespace pointed to by nsp_to_add is already on the
operator_lookup_namespaces list of cssp.  If it is not on
the list, add it to the front of the list.
*/
{
  a_namespace_list_entry_ptr	nlep;
  for (nlep = cssp->operator_lookup_namespaces;
       nlep != NULL; nlep = nlep->next) {
    /* If the namespaces match, exit the loop. */
    if (nlep->ptr == nsp_to_add) break;
  }  /* for */
  if (nlep == NULL) {
    nlep = list_entry_for_namespace(nsp_to_add, /*shared=*/FALSE);
    nlep->next = cssp->operator_lookup_namespaces;
    cssp->operator_lookup_namespaces = nlep;
  }  /* if */
}  /* add_to_operator_lookup_namespaces */
           


void determine_operator_lookup_namespaces(a_type_ptr	class_type)
/*
Build a list of the namespace of which the class or one of its base classes
is a member.  This list is used to determine which operator functions
should be considered for operands of a given class type.  The list that
is constructed may be partially or completely shared with a base class
of the class.
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_class_symbol_supplement_ptr	cssp;
  a_namespace_ptr		class_nsp;
  a_base_class_ptr		bcp;

  check_assertion_str2(class_type->kind == (a_type_kind)tk_class ||
                       class_type->kind == (a_type_kind)tk_struct ||
                       class_type->kind == (a_type_kind)tk_union,
                       "deterine_operator_lookup_namespace:",
                       "type is not class type");
  ctsp = class_type->variant.class_struct_union.extra_info;
  cssp = symbol_supplement_for_class(class_type);
  check_assertion(cssp != NULL);
  /* Determine the namespace of this class. */
  if (class_type->source_corresp.is_class_member) {
    /* If this is a class member, skip out to the outermost class type. */
    a_type_ptr	tp = class_type->source_corresp.parent.class_type;
    while (tp->source_corresp.is_class_member) {
      tp = tp->source_corresp.parent.class_type;
    }  /* while */
    /* Get the namespace pointer from the outermost class. */
    class_nsp = tp->source_corresp.parent.namespace_ptr;
  } else {
    /* This is not a nested class, get the immediate parent namespace. */
    class_nsp = class_type->source_corresp.parent.namespace_ptr;
  }  /* if */
  if (ctsp->base_classes == NULL) {
    /* No base classes.  The only namespace is the namespace of this class. */
    a_namespace_list_entry_ptr	nlep;
    nlep = list_entry_for_namespace(class_nsp, /*shared=*/TRUE);
    cssp->operator_lookup_namespaces = nlep;
  } else {
    /* This class has one or more direct base.  Share the list with the
       first base class, then add the namespace for this class, and the
       namespaces from any other direct bases to the list. */
    a_boolean				first_base_found = FALSE;
    /* Find any other direct bases in the base class list. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        a_type_ptr			base_type = bcp->type;
        a_class_symbol_supplement_ptr	base_cssp;
        base_cssp = symbol_supplement_for_class(base_type);
        if (!first_base_found) {
          /* This is the direct base class with which we are sharing a list.
             Copy the namespace list pointer to the current class and add
             the namespace for this class to the front of the list. */
          cssp->operator_lookup_namespaces =
                                         base_cssp->operator_lookup_namespaces;
          add_to_operator_lookup_namespaces(cssp, class_nsp);
          first_base_found = TRUE;
        } else {
          /* Add the namespace from this base class to the front of the
             list for this class. */
          a_namespace_list_entry_ptr	nlep;
          for (nlep = base_cssp->operator_lookup_namespaces;
               nlep != NULL; nlep = nlep->next) {
            add_to_operator_lookup_namespaces(cssp, nlep->ptr);
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("operator_namespaces")) {
    a_namespace_list_entry_ptr	nlep = cssp->operator_lookup_namespaces;
    fprintf(f_debug, "operator namespaces for class: ");
    db_type_name(class_type);
    fprintf(f_debug, "\n");
    for (; nlep != NULL; nlep = nlep->next) {
      fprintf(f_debug, "  ");
      if (nlep->ptr == NULL) {
        fprintf(f_debug, "<global>");
      } else {
        db_name(&nlep->ptr->source_corresp);
      }  /* if */
      fprintf(f_debug, "\n");
    }  /* for */
  }  /* if */
#endif /* DEBUG */
}  /* determine_operator_lookup_namespaces */


a_namespace_ptr parent_namespace_for_symbol(a_symbol_ptr sym)
/*
Return the parent namespace of sym.  If sym is a class member, return
the parent of the outermost enclosing class.
*/
{
  a_namespace_ptr	nsp;

  /* If this is a class member, skip out to the outermost class type. */
  if (sym->is_class_member) {
    a_type_ptr	tp = sym->parent.class_type;
    while (tp->source_corresp.is_class_member) {
      tp = tp->source_corresp.parent.class_type;
    }  /* while */
    /* Get the namespace pointer from the outermost class. */
    nsp = tp->source_corresp.parent.namespace_ptr;
  } else {
    nsp = sym->parent.namespace_ptr;
  }  /* if */
  return nsp;
}  /* parent_namespace_for_symbol */


a_boolean is_local_symbol(a_symbol_ptr sym)
/*
Return TRUE if the indicated symbol is a function-local symbol.
*/
{
  a_boolean               is_local = FALSE;
  a_scope_stack_entry_ptr ssep;

  /* Reject the easy cases, i.e., class and namespace members. */
  if (sym->is_class_member ||
      sym->parent.namespace_ptr != NULL ||
      sym->decl_scope == file_scope_number ||
      sym->synthesized_namespace_projection) {
    /* is_local = FALSE;  -- already set. */
  } else {
    /* Look through the scope stack for the scope of the symbol, to see
       whether the scope is a block scope. */
    for (ssep = &scope_stack[depth_scope_stack];
         ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      if (ssep->number == sym->decl_scope) {
        /* Found the scope. */
        if (ssep->kind == (a_scope_kind)sck_block ||
            ssep->kind == (a_scope_kind)sck_function) {
          is_local = TRUE;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return is_local;
}  /* is_local_symbol */


a_boolean is_block_extern_symbol(a_symbol_ptr sym)
/*
Return TRUE if the indicated symbol is a function-local block extern symbol.
This includes symbols for overload sets of block extern declarations.
*/
{
  a_boolean is_block_extern = FALSE;

  if (is_local_symbol(sym)) {
    /* Rule out using-declarations. */
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      /* For an overload set, return TRUE if the set contains at least
         one block extern declaration. */
      a_symbol_ptr sym2;
      for (sym2 = sym->variant.overloaded_function.symbols;
           sym2 != NULL;
           sym2 = sym2->next) {
        if (sym2->kind != (a_symbol_kind)sk_namespace_projection) {
          is_block_extern = TRUE;
          break;
        }  /* if */
      }  /* for */
    } else if (sym->kind != (a_symbol_kind)sk_namespace_projection) {
      is_block_extern = TRUE;
    }  /* if */
  }  /* if */
  return is_block_extern;
}  /* is_block_extern_symbol */


static void clear_template_param_default_arg_info(
		a_template_param_ptr	ptr,
		a_boolean		def_arg_involves_template_param)
/*
Clear the default argument fields of a template parameter entry based
on kind of default argument it has.
*/
{
  a_symbol_kind	kind = ptr->param_symbol->kind;

  ptr->def_arg_involves_template_param = def_arg_involves_template_param;
  if (kind == (a_symbol_kind)sk_type) {
    ptr->default_arg.type = NULL;
  } else if (kind == (a_symbol_kind)sk_constant) {
    ptr->default_arg.constant = NULL;
  } else {
    ptr->default_arg.templ = NULL;
  }  /* if */    
  clear_template_cache(&ptr->default_arg_cache, /*reusable=*/TRUE);
}  /* clear_template_param_default_arg_info */


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
  clear_template_cache(&ptr->cache, /*reusable=*/TRUE);
  ptr->has_default_arg = FALSE;
  ptr->def_arg_involves_template_param = FALSE;
#if CHECKING
  ptr->avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  if (sym->kind == (a_symbol_kind)sk_type) {
    ptr->variant.type     = sym->variant.type.ptr;
  } else if (sym->kind == (a_symbol_kind)sk_constant) {
    ptr->variant.constant.ptr = sym->variant.constant;
    ptr->variant.constant.type_involves_template_param = FALSE;
#if CHECKING
    ptr->variant.constant.avoid_codecenter_warnings = 0;
#endif /* CHECKING */
  } else {
    /* A template template parameter. */
    check_assertion(sym->kind == (a_symbol_kind)sk_class_template);
    ptr->variant.templ = sym->variant.template_info;
  }  /* if */
  clear_template_param_default_arg_info(
                               ptr, /*def_arg_involves_template_param=*/FALSE);
  db_exit();
  return ptr;
}  /* alloc_template_param */


a_master_instance_ptr alloc_master_instance(void)
/*
Allocate a master instance entry, initialize its fields, and return a
pointer to it.
*/
{
  a_master_instance_ptr  mip;

  mip = (a_master_instance_ptr)alloc_fe(sizeof(a_master_instance));
#if DEBUG
  num_master_instances_allocated++;
#endif /* DEBUG */
  mip->next                        = NULL;
  mip->instance                    = NULL;
  mip->name                        = NULL;
  mip->instantiation_required      = FALSE;
  mip->already_instantiated        = FALSE;
  mip->automatically_instantiated  = FALSE;
  mip->add_to_request_file	   = FALSE;
  return mip;
}  /* alloc_master_instance */


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
  tip->master_instance             = NULL;
  tip->instance_sym                = NULL;
  tip->template_sym                = NULL;
  tip->referencing_namespace       = NULL;
  tip->template_info               = NULL;
  tip->prototype_scope_symbols     = NULL;
  tip->exported_template_file      = NULL;
  tip->instantiation_required      = FALSE;
  tip->suppress_instantiation      = FALSE;
  tip->is_guiding_decl             = FALSE;
  tip->explicit_instantiation      = FALSE;
  tip->class_explicitly_instantiated
                                   = FALSE;
  tip->explicit_do_not_instantiate = FALSE;
  tip->explicit_can_instantiate    = FALSE;
  tip->can_be_instantiated	   = FALSE;
  tip->is_static_or_inline	   = FALSE;
  tip->explicit_instantiation_pos  = null_source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  tip->declared_type               = NULL;
  tip->declared_type_for_default_arg_fixup
                                   = NULL;
  tip->param_id_list               = NULL;
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  tip->partial_instantiation       = NULL;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return tip;
}  /* alloc_template_instance */


a_scope_number take_next_scope_number(void)
/*
Assign the next scope number in sequence, and return it.
*/
{
  a_scope_number	result;

  if (next_scope_number == MAX_SCOPE_NUMBER) {
    /* The number of scopes exceeds the size of the scope number field. */
    catastrophe(ec_program_too_large);
  }  /* if */
  result = next_scope_number++;
  if (result >= (long)size_of_trans_unit_for_scope) {
    /* The table used to map scope numbers to translation unit pointers
       is full.  Expand it by reallocating it. */
    sizeof_t new_size = size_of_trans_unit_for_scope +
                        TRANS_UNIT_FOR_SCOPE_INCREMENTAL_ALLOCATION;
    trans_unit_for_scope = (a_translation_unit_ptr*)realloc_general(
                      (char *)trans_unit_for_scope,
                      (sizeof_t)(size_of_trans_unit_for_scope *
                                 sizeof(a_translation_unit_ptr)),
                      (sizeof_t)(new_size * sizeof(a_translation_unit_ptr)));
    size_of_trans_unit_for_scope = new_size;
  }  /* if */
  /* Record the translation unit with which this scope is associated. */
  trans_unit_for_scope[result] = curr_translation_unit;
  return result;
}  /* take_next_scope_number */


a_boolean symbol_is_from_trans_unit(a_symbol_ptr		sym,
				    a_translation_unit_ptr	tup)
/*
Return TRUE if the declaration scope of sym is from a scope associated
with the translation unit specified by tup.
*/
{
  a_scope_number	scope_number;
  a_boolean		result = FALSE;

  scope_number = sym->decl_scope;
  if (scope_number != NO_SCOPE_NUMBER) {
    result = trans_unit_for_scope[scope_number] == tup;
  }  /* if */
  return result;
}  /* symbol_is_from_trans_unit */


a_symbol_ptr f_class_template_for_type(a_type_ptr	type)
/*
If "type" is based on a class template, return the class template on which
it is based; otherwise, return NULL.  This routine is only called for
types that are known to be template classes.
*/
{
  a_symbol_ptr	result;
  a_class_symbol_supplement_ptr	cssp;

  cssp = symbol_supplement_for_class(type);
  result = cssp->class_template;
  return result;
}  /* f_class_template_for_type */


a_symbol_ptr class_template_for_injected_template_symbol(a_symbol_ptr sym)
/*
sym is an injected template name symbol.  Return the class template symbol
for the class template of which this class is an instance.
*/
{
  a_type_ptr			templ_class_type;
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_ptr			template_sym;

  templ_class_type = sym->variant.type.ptr;
  cssp = symbol_supplement_for_class(templ_class_type);
  template_sym = cssp->class_template;
  /* If this class is from a partial specialization, get the symbol for the
     primary template. */
  template_sym = primary_template_of(template_sym);
  return template_sym;
}  /* class_template_for_injected_template_symbol */


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
  db_space_used("namespace symbol suppl.",
                num_namespace_symbol_supplements_allocated,
                a_namespace_symbol_supplement);
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
  db_space_used_lost("vla fixup", avail_vla_fixups, num_vla_fixups_allocated,
                     a_vla_fixup);
  db_space_used("template instance", num_template_instances_allocated,
                a_template_instance);
  db_space_used("master instance", num_master_instances_allocated,
                a_master_instance);
  db_space_used("symbol list entry", num_symbol_list_entries_allocated,
                a_symbol_list_entry);
  db_space_used("type list entry", num_type_list_entries_allocated,
                a_type_list_entry);
  db_space_used("subst. type list entry",
                num_substituted_type_list_entries_allocated,
                a_substituted_type_list_entry);
  db_space_used_lost("template cache segment", avail_template_cache_segments,
                     num_template_cache_segments_allocated,
                     a_template_cache_segment);
  db_space_used("template decl info", num_template_decl_info_allocated,
                a_template_decl_info);
  db_space_used("nodependent call info", num_nondependent_call_info_allocated,
                a_nondependent_call_info);
  db_space_used("templ friend def arg", num_templ_friend_info_allocated,
                a_templ_friend_info);
  db_space_used("namespace list entry", num_namespace_list_entries_allocated,
                a_namespace_list_entry);
  db_space_used("projection symbol descr", num_projection_descrs_allocated,
                a_projection_descr);
  db_space_used_lost("access error descr", avail_access_error_descrs,
                     num_access_error_descrs_allocated, an_access_error_descr);
  db_space_used_lost("active using directives", avail_active_using_directives,
                     num_active_using_directives_allocated,
                     an_active_using_directive);
  db_space_used("exception spec err descr",
                num_exception_spec_error_descrs_allocated,
                an_exception_spec_error_descr);
  db_space_used_general("generated entity blocks",
                        num_generated_entity_blocks_allocated,
                        a_generated_entity_block);
  grand_total = db_show_pch_space_used(grand_total);
  grand_total = db_show_template_space_used(grand_total);
  grand_total = db_show_routine_fixups_used(grand_total);
  grand_total = db_show_class_fixups_used(grand_total);
  grand_total = db_show_def_arg_expr_fixups_used(grand_total);
  grand_total = db_show_based_type_fixups_used(grand_total);
  grand_total = db_show_trans_unit_space_used(grand_total);

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
  name_space_for_symbol_kind[(int)sk_namespace]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_namespace_projection] = nsk_other;
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
  cleared_locator.is_unknown_template_reference   = FALSE;
  cleared_locator.is_class_member                 = FALSE;
  cleared_locator.specific_symbol                 = NULL;
  cleared_locator.parent.class_type               = NULL;
  cleared_locator.template_arg_list               = NULL;
  cleared_locator.variant.conversion_result_type  = NULL;

  /* Static variables in symbol_tbl.c: */
  /* Clear a symbol that can be used to make initialization more efficient. */
  cleared_symbol.header                            = NULL;
  cleared_symbol.next                              = NULL;
  cleared_symbol.next_in_scope                     = NULL;
  cleared_symbol.decl_scope                        = NO_SCOPE_NUMBER;
  cleared_symbol.decl_seq                          = 0;
  cleared_symbol.decl_position                     = null_source_position;
  cleared_symbol.parent.class_type                 = NULL;
  cleared_symbol.referenced                        = FALSE;
  cleared_symbol.defined                           = FALSE;
  cleared_symbol.explicit_linkage_specifier        = FALSE;
  cleared_symbol.reentered_from_prototype_scope    = FALSE;
  cleared_symbol.is_class_member                   = FALSE;
  cleared_symbol.is_error                          = FALSE;
  cleared_symbol.is_template_param                 = FALSE;
  cleared_symbol.template_param_not_visible        = FALSE;
  cleared_symbol.force_external_linkage            = FALSE;
  cleared_symbol.synthesized_namespace_projection  = FALSE;
  cleared_symbol.qualified_lookup                  = FALSE;
  cleared_symbol.must_be_class_or_namespace_lookup = FALSE;
  cleared_symbol.must_be_tag_lookup                = FALSE;
  cleared_symbol.tentative_type_lookup             = FALSE;
  cleared_symbol.do_not_reuse                      = FALSE;
  cleared_symbol.instantiation_context_lookup      = FALSE;
  cleared_symbol.must_be_class_lookup              = FALSE;
  cleared_symbol.must_be_namespace_lookup          = FALSE;
  cleared_symbol.ambiguous                         = FALSE;
  cleared_symbol.hidden_by_old_for_init            = FALSE;
  cleared_symbol.overload_set_member               = FALSE;
  cleared_symbol.is_invisible                      = FALSE;
  cleared_symbol.is_unknown_function               = FALSE;
  cleared_symbol.is_nonreal_member                 = FALSE;
#if CHECKING
  /* Not needed right now -- at byte boundary.
  cleared_symbol.avoid_codecenter_warnings         = FALSE;
  */
#endif /* CHECKING */
  size_of_trans_unit_for_scope = 0;
  trans_unit_for_scope = NULL;
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
      pch_saved_var_array_elem(avail_active_using_directives),
      pch_saved_var_array_elem(avail_symbol_list_entries),
      pch_saved_var_array_elem(avail_type_list_entries),
      pch_saved_var_array_elem(avail_namespace_list_entries),
      pch_saved_var_array_elem(avail_substituted_type_list_entries),
      pch_saved_var_array_elem(avail_template_cache_segments),
      pch_saved_var_array_elem(avail_dependent_type_fixups),
      pch_saved_var_array_elem(avail_param_ids),
      pch_saved_var_array_elem(avail_vla_fixups),
      pch_saved_var_array_elem(avail_progenitors),
      pch_saved_var_array_elem(error_symbol_header),
      pch_saved_var_array_elem(unnamed_tag_symbol_header),
      pch_saved_var_array_elem(unnamed_namespace_symbol_header),
      pch_saved_var_array_elem(anonymous_parent_object_symbol_header),
      pch_saved_var_array_elem(unnamed_field_symbol_header),
      pch_saved_var_array_elem(global_namespace_list_entry),
      pch_saved_var_array_elem(symbol_for_namespace_std),
      pch_saved_var_array_elem(builtin_va_list_type),
      pch_saved_var_array_elem(conversion_header_list),
      pch_saved_var_array_elem(error_class_template_symbol),
      pch_saved_var_array_elem(file_scope_number),
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
      pch_saved_var_array_elem(last_ctor_or_dtor_sym),
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
#if DEBUG
      pch_saved_var_array_elem(db_symbol_buffer_pointer),
      pch_saved_var_array_elem(num_access_error_descrs_allocated),
      pch_saved_var_array_elem(num_active_using_directives_allocated),
      pch_saved_var_array_elem(num_generated_entity_blocks_allocated),
      pch_saved_var_array_elem(num_class_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_compares_for_symbols),
      pch_saved_var_array_elem(num_conversion_headers_allocated),
      pch_saved_var_array_elem(num_dependent_type_fixups_allocated),
      pch_saved_var_array_elem(num_extern_symbol_descrs_allocated),
      pch_saved_var_array_elem(num_vla_fixups_allocated),
      pch_saved_var_array_elem(num_extern_type_fixups_allocated),
      pch_saved_var_array_elem(num_fast_id_lookups),
      pch_saved_var_array_elem(num_namespace_list_entries_allocated),
      pch_saved_var_array_elem(num_param_ids_allocated),
      pch_saved_var_array_elem(num_projection_descrs_allocated),
      pch_saved_var_array_elem(num_searches_for_symbols),
      pch_saved_var_array_elem(num_slow_id_lookups),
      pch_saved_var_array_elem(num_symbol_headers_allocated),
      pch_saved_var_array_elem(num_symbol_headers_in_hash_table),
      pch_saved_var_array_elem(num_symbol_list_entries_allocated),
      pch_saved_var_array_elem(num_type_list_entries_allocated),
      pch_saved_var_array_elem(num_substituted_type_list_entries_allocated),
      pch_saved_var_array_elem(num_symbols_allocated),
      pch_saved_var_array_elem(num_template_instances_allocated),
      pch_saved_var_array_elem(num_master_instances_allocated),
      pch_saved_var_array_elem(num_template_params_allocated),
      pch_saved_var_array_elem(num_template_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_namespace_symbol_supplements_allocated),
      pch_saved_var_array_elem(num_progenitors_allocated),
      pch_saved_var_array_elem(num_exception_spec_error_descrs_allocated),
      pch_saved_var_array_elem(num_used_symbol_buckets),
      pch_saved_var_array_elem(symbol_name_string_space),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(global_namespace_list_entry);
  register_trans_unit_variable(symbol_for_namespace_std);
  register_trans_unit_variable(builtin_va_list_type);
  register_trans_unit_variable(symbols_with_no_scope);
  register_trans_unit_variable(symbols_with_no_scope_tail);
  register_trans_unit_variable(file_scope_symbols_are_on_inactive_list);
#if MICROSOFT_EXTENSIONS_ALLOWED
  register_trans_unit_variable(predeclared_size_t_symbol);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  register_trans_unit_variable(conversion_header_list);
  register_trans_unit_variable(decl_seq_counter);
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  register_trans_unit_variable(last_ctor_or_dtor_sym);
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  register_trans_unit_variable(error_class_template_symbol);
  register_trans_unit_variable(file_scope_number);
}  /* symbol_tbl_one_time_init */


void symbol_tbl_trans_unit_init(void)
/*
Initialize variables related to the symbol table that are specific to a
given translation unit.
*/
{
  global_namespace_list_entry = NULL;
  /* Initialize the predeclared symbol for namespace "std". */
  symbol_for_namespace_std = NULL;
  builtin_va_list_type = NULL;
  symbols_with_no_scope = NULL;
  symbols_with_no_scope_tail = NULL;
  file_scope_symbols_are_on_inactive_list = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  predeclared_size_t_symbol = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Initialize the conversion header list. */
  conversion_header_list = NULL;
  /* Global variable declared in symbol_ref.c. */
  decl_seq_counter = 0;
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  last_ctor_or_dtor_sym = NULL;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  error_class_template_symbol = NULL;
  if (!is_primary_translation_unit) {
    /* In secondary translation units, the scope numbering continues where it
       left off, and the file scope number is the next available number.
       For primary translation units, file_scope_number is already set to 0. */
    file_scope_number = take_next_scope_number();
  } else {
    /* The primary translation unit file scope number is assigned during
       compilation initialization.  Record the translation unit for that
       scope now. */
    trans_unit_for_scope[file_scope_number] = curr_translation_unit;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!is_primary_translation_unit) {
    /* Disable source sequence entries for secondary translation units. */
    source_sequence_entries_disallowed = TRUE;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* symbol_tbl_trans_unit_init */


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
  next_scope_number = FILE_SCOPE_NUMBER;
  file_scope_number = take_next_scope_number();

  /* size_scope_stack is not per-file and should not be reset. */
  /* ident_buffer and size_ident_buffer are not per-file and should not
     be reset. */
  avail_param_ids = NULL;
  avail_dependent_type_fixups = NULL;
  avail_access_error_descrs = NULL;
  avail_active_using_directives = NULL;
  avail_symbol_list_entries = NULL;
  avail_type_list_entries = NULL;
  avail_namespace_list_entries = NULL;
  avail_substituted_type_list_entries = NULL;
  avail_template_cache_segments = NULL;
  avail_vla_fixups = NULL;
  avail_progenitors = NULL;
  error_symbol_header = NULL;
  unnamed_tag_symbol_header = NULL;
  unnamed_namespace_symbol_header = NULL;
  anonymous_parent_object_symbol_header = NULL;
  unnamed_field_symbol_header = NULL;
#if DEBUG
  num_symbols_allocated                        = 0;
  num_symbol_headers_allocated                 = 0;
  num_symbol_headers_in_hash_table             = 0;
  num_conversion_headers_allocated             = 0;
  symbol_name_string_space                     = 0;
  num_class_symbol_supplements_allocated       = 0;
  num_template_symbol_supplements_allocated    = 0;
  num_namespace_symbol_supplements_allocated   = 0;
  num_template_params_allocated                = 0;
  num_param_ids_allocated                      = 0;
  num_dependent_type_fixups_allocated          = 0;
  num_template_instances_allocated             = 0;
  num_master_instances_allocated               = 0;
  num_symbol_list_entries_allocated            = 0;
  num_type_list_entries_allocated              = 0;
  num_substituted_type_list_entries_allocated  = 0;
  num_template_cache_segments_allocated        = 0;
  num_template_decl_info_allocated             = 0;
  num_nondependent_call_info_allocated         = 0;
  num_templ_friend_info_allocated              = 0;
  num_namespace_list_entries_allocated         = 0;
  num_extern_symbol_descrs_allocated           = 0;
  num_vla_fixups_allocated                     = 0;
  num_extern_type_fixups_allocated             = 0;
  num_projection_descrs_allocated              = 0;
  num_used_symbol_buckets                      = 0;
  num_searches_for_symbols                     = 0;
  num_compares_for_symbols                     = 0;
  num_access_error_descrs_allocated            = 0;
  num_fast_id_lookups                          = 0;
  num_slow_id_lookups                          = 0;
  num_active_using_directives_allocated        = 0;
  num_generated_entity_blocks_allocated        = 0;
  num_progenitors_allocated                    = 0;
  num_exception_spec_error_descrs_allocated    = 0;
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

