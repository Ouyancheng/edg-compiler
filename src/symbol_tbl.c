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

symbol_tbl.c - Symbol table management routines.

*/

#include "basics.h"
#include "symbol_tbl.h"
#include "mem_manage.h"
#include "error.h"
#include "il.h"
#include "types.h"
#include "cmd_line.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */

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
		unnamed_class_symbol_header;

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
                num_conversion_list_entries_allocated,
		num_extern_symbol_descrs_allocated,
		num_extern_type_fixups_allocated,
                num_projection_descrs_allocated,
		num_used_symbol_buckets,
		num_searches_for_symbols,
		num_compares_for_symbols,
                num_fast_id_lookups,
                num_slow_id_lookups;
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
static a_type_ptr
		class_currently_inside_of;
			/* Pointer to the type for a class to which we
			   currently have member access, or NULL if there
			   is no such type.  Non-NULL when inside a member
			   function, class definition, or the like. */
static a_class_list_entry_ptr
		curr_class_befriending_classes,
		curr_function_befriending_classes;
			/* Pointers to lists of befriending classes for the
			   current class or function (i.e., a class or function
			   that we are inside of).  NULL if not applicable. */


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


#if DEBUG
/* Two macros used within db_symbol, referencing local variables defined
   in that routine. */
/* put_separator appends the separator to the current line, along with a
   blank if the line still has room for sting_len additional characters;
   otherwise, it puts a new-line character and indents the next line.
   Variable col is updated in both cases. */
#define put_separator(separator, string_len)			\
{ col += strlen(separator) + 1;					\
  if (col + (string_len) > 79) {				\
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


static char *str_access(char                *buffer,
                        an_access_specifier access)
/*
Write an access specifier for db_symbol.
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


static char *str_qualified_name(char         *buffer,
                                a_symbol_ptr sym)
/*
Write a qualified name for db_symbol.
*/
{
  if (sym->class_of_which_a_member != NULL) {
    (void)sprintf(buffer, "%s::%s",
                  sym->class_of_which_a_member->source_corresp.name,
                  sym->header->identifier);
  } else {
    (void)sprintf(buffer, "%s", sym->header->identifier);
  }  /* if */
  return buffer;
}  /* str_qualified_name */


static char *str_path(char               buffer[],
                      a_derivation_step  *path,
                      char               *initial_string,
                      char               *separator)
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


#define put_access(access)                                      \
{ (void)str_access(buffer, (access)); put_string(buffer); }

#define put_qualified_name(class_name, name)                    \
{ (void)str_qualfied_name(buffer, (class_name), (name));        \
  put_string(buffer); }


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
  int				col = 0;
  a_type_ptr			type = NULL;
  a_variable_ptr		var;
  a_routine_ptr                 rp;

  if (string != NULL && strlen(string) > 0) {
    fputs(string, f_debug);
    col = strlen(string);
  }  /* if */

  str = db_sym_names[(int)sym->kind];
  if (col + strlen(str) + 2 > 79) {
    fprintf(f_debug, "\n%*s", indentation, "");
    col = indentation;
  }  /* if */
  fprintf(f_debug, "<%s>", str);
  col += strlen(str) + 2;

  str = sym->header->identifier;
  if (sym->kind == (a_symbol_kind)sk_projection) {
    a_symbol_ptr fsym = sym->variant.projection.extra_info->fundamental_symbol;
    if (fsym != NULL) str = str_qualified_name(buffer, fsym);
  }  /* if */
  put_separator("", strlen(str) + 2);
  fprintf(f_debug, "\"%s\"", str);
  col += strlen(str) + 2;

  (void)sprintf(buffer, "(%d/%d)", sym->decl_position.seq,
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
      break;
    case sk_keyword:
      fprintf(f_debug, "\"%s\"",
                       token_names[(int)sym->variant.keyword_token]);
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
      (void)str_name_linkage(buffer, &(skip_typerefs(type))->source_corresp);
      put_string(buffer);
      {
        a_class_symbol_supplement_ptr  cssp;
        cssp = sym->variant.class_struct_union.extra_info;
        if (cssp->constructor_required) put_string("ctor req'd");
        if (cssp->destructor_required) put_string("dtor req'd");
        if (cssp->constructor != NULL) put_string("has ctor");
        if (cssp->has_default_constructor != NULL) {
          put_string("has default-ctor");
        }  /* if */
        if (cssp->has_copy_constructor_for_const_object) {
          put_string("has const-copy-ctor");
        } else if (cssp->has_copy_constructor) {
          put_string("has copy-ctor");
        }  /* if */
        if (cssp->destructor != NULL) put_string("has dtor");
      }
      break;
    case sk_field:
      if (sym->variant.field == NULL) {
        put_string("<null>");
      } else {
        if (C_dialect == C_dialect_cplusplus) {
          put_access(sym->variant.field->source_corresp.access);
        }  /* if */
        (void)sprintf(buffer, "offset = %d",
                      sym->variant.field->bit_offset);
        put_string(buffer);
        type = sym->variant.field->type;
      }  /* if */
      break;
    case sk_label:
      break;
    case sk_static_data_member:
    case sk_variable:
      var = sym->variant.variable;
      if (var == NULL) {
        put_string("<null>");
      } else {
        if (sym->kind == (a_symbol_kind)sk_static_data_member) {
          put_access(var->source_corresp.access);
        }  /* if */
        (void)sprintf(buffer, "sc_%s",
	              db_storage_class_names[(int)var->storage_class]);
        put_string(buffer);
        if (var->is_parameter) put_string("is param");
        type = var->type;
      }  /* if */
      break;
    case sk_member_function:
    case sk_routine:
      rp = sym->variant.routine;
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
        (void)sprintf(buffer, "sc_%s",
                      db_storage_class_names[(int)rp->storage_class]);
        put_string(buffer);
        type = rp->type;
      }  /* if */
      break;
    case sk_projection:
      put_access(sym->variant.projection.access);
      if (sym->variant.projection.ambiguous) put_string("ambig");
      { a_projection_descr_ptr pdp = sym->variant.projection.extra_info;
        if (pdp->fundamental_base_class != NULL &&
            pdp->fundamental_base_class->derivation != NULL) {
          put_string(str_path(buffer, pdp->fundamental_base_class->derivation,
                              "path = ==>", "==>"));
        }  /* if */
      }
      break;
    case sk_overloaded_function:
      if (sym->variant.overloaded_function.mixed_access) {
        put_string ("mixed access");
      }  /* if */
      if (sym->variant.overloaded_function.mixed_static_nonstatic) {
        put_string ("mixed static/nonstatic");
      }  /* if */
      if (sym->variant.overloaded_function.any_virtual_functions) {
        put_string ("virtuals");
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
      }  /* if */
      break;
#if CHECKING
    default:
      put_string("UNEXPECTED SYMBOL KIND");
      break;
#endif /* CHECKING */
  }  /* switch */
  if (type != NULL) {
    if (col > 60 ||
        (col > 50 && is_array_type(type)) || is_function_type(type) ||
        ((is_pointer_type(type) || is_reference_type(type)) &&
	 ((is_array_type(type->variant.pointer.type) && col > 45) ||
	  is_function_type(type->variant.pointer.type)))) {
      fprintf(f_debug, ",\n%*stype = ", indentation, "");
    } else {
      fputs(", type = ", f_debug);
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
        sym->kind == (a_symbol_kind)sk_union_tag) {
      db_type(type);
    } else {
      db_abbreviated_type(type);
    } 
  }  /* if */
  fputc('\n', f_debug);
  if (sym->kind == (a_symbol_kind)sk_variable ||
      sym->kind == (a_symbol_kind)sk_static_data_member) {
    if (sym->variant.variable != NULL) {
      db_initializer(sym->variant.variable, indentation);
    }  /* if */
  }  /* if */
}  /* db_symbol */


static db_scope_kind(a_scope_kind sck)
/*
Put out a scope kind name (for debugging).
*/
{
  char	*s;

  switch (sck) {
    case sck_file:               s = "file";                     break;
    case sck_func_prototype:     s = "function prototype";       break;
    case sck_block:              s = "block";                    break;
    case sck_class_struct_union: s = "class/struct/union";       break;
    case sck_class_reactivation: s = "class reactivation";       break;
    case sck_function:           s = "function";                 break;
    default:                     s = "***UNKNOWN SCOPE KIND***"; break;
  }  /* switch */
  fputs(s, f_debug);
}  /* db_scope_kind */
#endif /* DEBUG */


void add_name_of_type(a_type_ptr  type,
                      char        *buffer)
{
  char  *str;

  if (type->kind == (a_type_kind)tk_typeref &&
      (type->variant.typeref.is_const || type->variant.typeref.is_volatile)) {
    if (type->variant.typeref.is_const) {
      (void)sprintf(&buffer[strlen(buffer)], "const ");
    }  /* if */
    type = type->variant.typeref.type;
    add_name_of_type(type, buffer);
  } else if (is_reference_type(type)) {
    add_name_of_type(type_pointed_to(type), buffer);
    (void)sprintf(&buffer[strlen(buffer)], "&");
  } else if (is_pointer_type(type)) {
    add_name_of_type(type_pointed_to(type), buffer);
    (void)sprintf(&buffer[strlen(buffer)], "*");
  } else {
    switch (type->kind) {
      case tk_class:
      case tk_struct:
      case tk_union:
      case tk_typeref:
	str = type->source_corresp.name;
	break;
      case tk_integer:
	switch (type->variant.integer.int_kind) {
	  case ik_char:            str = "char";            break;
	  case ik_signed_char:     str = "signed char";     break;
	  case ik_unsigned_char:   str = "unsigned char";   break;
	  case ik_short:           str = "short";           break;
	  case ik_unsigned_short:  str = "unsigned short";  break;
	  case ik_int:             str = "int";             break;
	  case ik_unsigned_int:    str = "unsigned int";    break;
	  case ik_long:            str = "long";            break;
	  case ik_unsigned_long:   str = "unsigned long";   break;
#if CHECKING
          default:
            internal_error("add_name_of_type:  bad integer kind");
#endif /* CHECKING */
            str = "int";
            break;
        }  /* switch */
        break;
      case tk_float:
        switch (type->variant.float_kind) {
          case fk_float:        str = "float";        break;
          case fk_double:       str = "double";       break;
          case fk_long_double:  str = "long double";  break;
#if CHECKING
          default:
            internal_error("add_name_of_type:  bad float kind");
#endif /* CHECKING */
            str = "float";
            break;
        }  /* switch */
        break;
#if 0
#else
#if CHECKING
      default:
        internal_error("add_name_of_type: unsupported type kind");
#endif /* CHECKING */
#endif /* if 0 */
    }  /* switch */
    (void)sprintf(&buffer[strlen(buffer)], str);
  }  /* if */
}  /* add_name_of_type */


char *name_of_symbol(a_symbol_ptr  sym)
{
  char              *str, buffer[1000];
  a_param_type_ptr  ptp;
  a_boolean         has_ellipsis;

  buffer[0] = '\0';
  switch (sym->kind) {
    case sk_member_function:
      (void)sprintf(buffer, "%s::%s(",
                    sym->class_of_which_a_member->source_corresp.name,
                    sym->header->identifier);
      has_ellipsis = sym->variant.routine->type->
                              variant.routine.extra_info->has_ellipsis;
      ptp = sym->variant.routine->type->
                              variant.routine.extra_info->param_type_list;
      for (; ptp != NULL; ptp = ptp->next) {
        add_name_of_type(ptp->type, buffer);
        if (ptp->next != NULL || has_ellipsis) {
          (void)sprintf(&buffer[strlen(buffer)], ", ");
        }  /* if */
      }  /* for */
      (void)sprintf(&buffer[strlen(buffer)], "%s)", has_ellipsis ? "..." : "");
      break;
#if 0
#else
#if CHECKING
    default:
      internal_error("name_of_symbol: unsupported symbol kind");
#endif /* CHECKING */
#endif /* if 0 */
  }  /* switch */
  str = alloc_fe(strlen(buffer));
  (void)strcpy(str, buffer);
  return str;
}  /* name_of_symbol */


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


a_conversion_list_entry_ptr alloc_conversion_list_entry(void)
/*
Allocate a new conversion list entry and return a pointer to it.
*/
{
  register a_conversion_list_entry_ptr ptr;

  db_enter(5, "alloc_conversion_list_entry");
  ptr = (a_conversion_list_entry_ptr)alloc_fe(sizeof(a_conversion_list_entry));
#if DEBUG
  num_conversion_list_entries_allocated++;
#endif /* DEBUG */
  ptr->next    = NULL;
  ptr->symbol  = NULL;
  
  db_exit();
  return ptr;
}  /* alloc_conversion_header */


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
  register int                 a;
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
        if (memcmp(identifier, hdr_ptr->identifier, (int)length) == 0) {
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
  memcpy(hdr_ptr->identifier, identifier, (int)length);

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
}  /* make_locator_for_symbol */


void make_specific_symbol_error_locator(a_symbol_locator *locator)
/*
Make a specific symbol error locator in *locator.  This identifies a
specific symbol which is an error symbol.
*/
{
  clear_locator(locator, &error_position);
  locator->specific_symbol = enter_symbol((a_symbol_kind)sk_undefined,
                                          locator,
                                          DEPTH_OF_FILE_SCOPE,
                                          /*suppress_error=*/TRUE);
}  /* make_specific_symbol_error_locator */


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
      sym_ptr->variant.keyword_token = tok_error;
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
#endif
        sym_ptr->variant.class_struct_union.extra_info = cssp;
        cssp->symbols = NULL;
        cssp->constructor = NULL;
        cssp->destructor = NULL;
        cssp->assignment_operator = NULL;
        cssp->conversion_list = NULL;
        cssp->any_nonpublic_members = FALSE;
        cssp->constructor_required = FALSE;
        cssp->destructor_required = FALSE;
        cssp->has_default_constructor = FALSE;
        cssp->has_copy_constructor = FALSE;
        cssp->has_copy_constructor_for_const_object = FALSE;
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
        cssp->target_of_user_defined_conversion = FALSE;
      }
      break;
    case sk_variable:
    case sk_static_data_member:
      sym_ptr->variant.variable = NULL;
      break;
    case sk_field:
      sym_ptr->variant.field = NULL;
      break;
    case sk_routine:
    case sk_member_function:
      sym_ptr->variant.routine = NULL;
      break;
    case sk_label:
      sym_ptr->variant.label = NULL;
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
      }
      break;
    case sk_overloaded_function:
      sym_ptr->variant.overloaded_function.symbols = NULL;
      sym_ptr->variant.overloaded_function.mixed_access  = FALSE;
      sym_ptr->variant.overloaded_function.mixed_static_nonstatic = FALSE;
      sym_ptr->variant.overloaded_function.any_virtual_functions = FALSE;
      break;
#if CHECKING
    default:
      internal_error("set_symbol_kind: bad symbol kind");
#endif /* CHECKING */
  }  /* switch */

  db_exit();
}  /* set_symbol_kind */


static void clear_symbol(register a_symbol_ptr sym_ptr,
                         a_symbol_kind         kind)
/*
Set the fixed fields of a symbol to some safe state, and set its kind 
to the indicated kind (and the associated variant fields to safe values).
*/
{
  db_enter(5, "clear_symbol");

  sym_ptr->header                    = NULL;
  sym_ptr->next                      = NULL;
  sym_ptr->next_in_scope             = NULL;
  sym_ptr->decl_scope                = NO_SCOPE_NUMBER;
  sym_ptr->decl_position.seq         = 0;
  sym_ptr->decl_position.column      = SP_COL_UNKNOWN;
  sym_ptr->class_of_which_a_member   = NULL;
  sym_ptr->referenced                = FALSE;
  sym_ptr->defined                   = FALSE;
  sym_ptr->explicit_linkage_specifier= FALSE;
  set_symbol_kind(sym_ptr, kind);

  db_exit();
}  /* clear_symbol */


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
  clear_symbol(sym_ptr, kind);
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
  if (debug_level >= 3) {
    db_symbol(sym_ptr, "unlinking: ", 2);
  }  /* if */
#endif /* CHECKING */

  hdr_ptr = sym_ptr->header;
  if (hdr_ptr == error_symbol_header) {
    /* Error symbols are never added to a symbol list and cannot be removed. */
  } else {
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

  if (sym_ptr->header == error_symbol_header) {
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

#if CHECKING
  if (hdr_ptr == NULL) {
    internal_error("link_symbol_into_symbol_table: NULL header pointer");
  }  /* if */
#endif /* CHECKING */
  if (hdr_ptr == error_symbol_header) {
    /* Error symbols are never added to the symbol table. */
  } else {
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
      /* See if there is already a definition of this identifier in the same
         scope and name space.  (For name spaces, see C standard, 3.1.2.3.)
         Because of the code above and because the active list is ordered,
         if there are any symbols in the same scope they will be at the
         front of the list.  Note that this test must be done even if
         suppress_error is TRUE, because tag names must still be entered
         behind existing non-tag names in C++.  suppress_error is only TRUE
         when there has already been an error issued, so in practical terms
         the extra check costs nothing. */
      sym_name_space_kind = name_space_for_symbol_kind[(int)sym_ptr->kind];
      for (; old_sym_ptr != NULL && old_sym_ptr->decl_scope == scope_number;
           old_sym_ptr = old_sym_ptr->next) {
        if (name_space_for_symbol_kind[(int)old_sym_ptr->kind] ==
                                                         sym_name_space_kind) {
          /* Two declarations in the same name space in the same scope:
             in most cases, this is an error. */
          /* In C++, one is allowed to define a tag name and a non-type
             name in the same scope (see ARM 3.2, 3.1c, and 7.1.3). */
          if (C_dialect == C_dialect_cplusplus &&
              ((is_tag_symbol(sym_ptr) && !is_type_symbol(old_sym_ptr)) ||
               (is_tag_symbol(old_sym_ptr) && !is_type_symbol(sym_ptr)))) {
            /* One symbol is a tag name (class, struct, union, or enum)
               and the other is a non-type name, so they can coexist in
               the same scope.  If the new symbol is a tag, put it
               behind the other (non-type) symbol, so the non-type symbol
               will be found on a normal id lookup. */
            if (is_tag_symbol(sym_ptr)) {
              /* Put the new (tag) symbol in back of the existing (non-tag)
                 symbol. */
              insert_after = old_sym_ptr;
            }  /* if */
          } else {
            /* Error, this identifier has already been declared. */
            if (!suppress_error) {
              /* Issue a redeclaration error.  A special version of the message
                 is used for type name redeclarations in C++. */
              pos_error((is_type_symbol(sym_ptr) &&
                         is_type_symbol(old_sym_ptr) &&
                         C_dialect == C_dialect_cplusplus) ?
                                               ec_bad_type_name_redeclaration :
                                               ec_id_already_declared,
                        &(sym_ptr->decl_position));
            }  /* if */
          }  /* if */
          /* Go ahead and enter the symbol anyway.  Both symbols will be
             in the symbol table. */
          break;
        }  /* if */
      }  /* for */
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
    if (sym_ptr->header == error_symbol_header) {
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
         be defined within the class unless it's a constructor. */
      if (ssep->kind == (a_scope_kind)sck_class_struct_union) {
        a_type_ptr   class_type = ssep->assoc_type;
        a_symbol_ptr class_symbol = 
                           (a_symbol_ptr)class_type->source_corresp.assoc_info;
        /* See if the class name is the same as the name of the symbol we've
           just entered.  Constructors will have gotten special handling
           so that the header here will be different from the class header
           even though the name is the same. */
        if (class_symbol->header == sym_ptr->header) {
          /* Error: an identifier that is not a constructor and that
             has the same name as a class is being defined within the
             class. */
          pos_error(ec_id_has_same_name_as_class, &sym_ptr->decl_position);
          *err = TRUE;
        }  /* if */
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
  mark_declared(sym_ptr, &location->source_position,
                /*save_as_decl_position=*/FALSE);  /* FALSE because done by
                                                      alloc_symbol. */
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
  a_symbol_ptr        sym_ptr, overload_symbol, prev_sym_ptr;
  a_symbol_header_ptr hdr_ptr;
  a_scope_stack_entry *ssep;

  sym_ptr = alloc_symbol(sym_kind, location->symbol_header,
                         &location->source_position);
  sym_ptr->decl_scope = other_sym->decl_scope;
  mark_declared(sym_ptr, &location->source_position,
                /*save_as_decl_position=*/FALSE);  /* FALSE because done by
                                                      alloc_symbol. */
  /* Set the locator to point to the symbol entered. */
  location->specific_symbol = sym_ptr;
  location->is_qualified_name = FALSE;
  if (other_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    overload_symbol = other_sym;
    other_sym = overload_symbol->variant.overloaded_function.symbols;
  } else {
    /* The existing symbol is not an sk_overloaded_function symbol
       (i.e., it's a simple function symbol of some kind). */
    /* Create an sk_overloaded_function symbol and attach the old
       function symbol to it. */
    hdr_ptr = other_sym->header;
    overload_symbol = alloc_symbol((a_symbol_kind)sk_overloaded_function,
                                   hdr_ptr, &(sym_ptr->decl_position));
    overload_symbol->decl_scope = other_sym->decl_scope;
    overload_symbol->class_of_which_a_member =
                                       other_sym->class_of_which_a_member;
    /* Mark the overloaded function symbol appropriately if other_sym
       represents a virtual function. */
    if (other_sym->variant.routine->is_virtual) {
      overload_symbol->
                 variant.overloaded_function.any_virtual_functions = TRUE;
    }  /* if */
    /* Put overload_symbol into the primary list in place of other_sym. */
    /* Find the symbol preceding other_sym on its list. */
    prev_sym_ptr = hdr_ptr->symbol;
    if (prev_sym_ptr == other_sym) {
      /* The entry is the first on the header list. */
      hdr_ptr->symbol = overload_symbol;
    } else {
      while (prev_sym_ptr->next != other_sym) {
        prev_sym_ptr = prev_sym_ptr->next;
      }  /* while */
      prev_sym_ptr->next = overload_symbol;
    }  /* if */
    overload_symbol->next = other_sym->next;
    other_sym->next = NULL;
    /* Also put overload_symbol into the scope list in place of other_sym. */
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
       ssep->symbols = overload_symbol;
    } else {
       while (prev_sym_ptr->next_in_scope != other_sym) {
         prev_sym_ptr = prev_sym_ptr->next_in_scope;
       }  /* while */
       prev_sym_ptr->next_in_scope = overload_symbol;
    }  /* if */
    overload_symbol->next_in_scope = other_sym->next_in_scope;
    other_sym->next_in_scope = NULL;
    if (ssep->last_symbol == other_sym) ssep->last_symbol = overload_symbol;
    /* Attach the old symbol under the overloaded symbol. */
    overload_symbol->variant.overloaded_function.symbols = other_sym;
  }  /* if */
  /* Attach the new symbol to the front of the list under the overloaded
     symbol. */
  sym_ptr->next = overload_symbol->variant.overloaded_function.symbols;
  overload_symbol->variant.overloaded_function.symbols = sym_ptr;
  /* Return a pointer to the sk_overloaded_function symbol. */
  *overload_sym = overload_symbol;
  /* Return a pointer to the newly created symbol as well. */
  return sym_ptr;
}  /* enter_overloaded_symbol */


static a_symbol_ptr make_projection_symbol(a_symbol_ptr      progenitor_sym,
                                           a_type_ptr        class_ptr,
                                           a_derivation_step *path,
                                           a_boolean         ambiguous)
/*
Create a new projection symbol entry and return a pointer to it.  The symbol
is a projection of progenitor_sym into the current scope.  The symbol is not
added to the scope symbols list and is not linked into the symbol table.
*/
{
  register a_symbol_ptr        sym;
  a_projection_descr_ptr       pdp, progenitor_pdp = NULL;
  a_base_class_ptr             bcp;
  a_derivation_step_ptr        tail;

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
    progenitor_pdp = progenitor_sym->variant.projection.extra_info;
    pdp->fundamental_symbol = progenitor_pdp->fundamental_symbol;
  } else {
    pdp->fundamental_symbol = progenitor_sym;
  }  /* if */
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
#if CHECKING
        /* Confirm that the type match was in fact for sufficient.  The code
           corresponds to the more expensive search used in the presence of
           ambiguity (see below). */
        for (tail = path; tail->next != NULL; tail = tail->next) {}
        if (progenitor_pdp != NULL) {
          tail->next = progenitor_pdp->fundamental_base_class->derivation;
        }  /* if */
        if (!equivalent_paths(path, bcp->derivation)) {
          internal_error("make_projection_symbol: bad path match");
        }  /* if */
        tail->next = NULL;
#endif /* CHECKING */
        break;
      }  /* if */
    }  /* for */
  } else {
    /* When there is an ambiguity, we must check the paths as well as the
       type. */
    /* Find the last step in the linked list of derivation step entries
       pointed to by path. */
    for (tail = path; tail->next != NULL; tail = tail->next) {}
    if (progenitor_pdp != NULL) {
      /* The progenitor symbol is a itself a projection symbol.  At this
         point "path" represents the path from the current class
         (represented by class_ptr) to the class of which progenitor_sym
         is a member (by inheritance).  We need to temporarily extend "path"
         to include the rest of the path on to the fundamental symbol. */
      tail->next = progenitor_pdp->fundamental_base_class->derivation;
    }  /* if */
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->type == pdp->fundamental_symbol->class_of_which_a_member &&
          equivalent_paths(path, bcp->derivation)) {
        pdp->fundamental_base_class = bcp;
        break;
      }  /* if */
    }  /* for */
    /* Restore the path to what it was originally. */
    tail->next = NULL;
  }  /* if */
#if CHECKING
  if (pdp->fundamental_base_class == NULL) {
    internal_error("make_projection_symbol: no fundamental base class");
  }  /* if */
#endif /* CHECKING */
  db_exit();
  return sym;
}  /* make_projection_symbol */


a_symbol_ptr make_unnamed_class_symbol(a_symbol_kind      sym_kind,
                                       a_type_ptr         class_type,
                                       a_source_position  *pos)
/*
Create a symbol for a tagless class, struct, or union symbol.  Do not enter
into the symbol table.
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
  /* set_source_corresp will not be called, to be sure the name field is left
     NULL and the referenced flag is left TRUE (which is how it is set in
     set_default_source_corresp).  But do the rest of its processing. */
  class_type->source_corresp.assoc_info = (char *)sym;
  copy_source_position(*pos, class_type->source_corresp.decl_position);
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

  (void)find_symbol(identifier, length, &location);
  location.source_position.seq = 0;
  location.source_position.column = SP_COL_UNKNOWN;
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
  a_symbol_kind       kind;

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
          if (islower(ch)) ch = toupper(ch);
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
          if (islower(ch)) ch = toupper(ch);
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
      kind = sym_ptr->kind;
      if (kind == (a_symbol_kind)sk_extern_variable) {
        break;
      } else if (kind == (a_symbol_kind)sk_extern_routine) {
        /* A type compatibility check may also be required for routines. */
        if (rout_type != NULL && C_dialect == C_dialect_cplusplus &&
            linkage != (a_name_linkage_kind)nlk_external) {
          a_type_ptr  tp = sym_ptr->variant.extern_symbol_descr->type;
          if (!is_error_type(tp) && !arg_types_are_compatible(rout_type, tp)) {
            /* This looks like a C++ overloaded function name.   Continue
               searching until the type matches as well as the name.  */
            continue;
          }  /* if */
        }  /* if */
        /* This is a case in which the name match is enough or the routine's
           argument types are compatible.  (Note that if two routines have the
           same arg types but differ in return type they are not treated as
           instances of overloading.)  */
        break;
      }  /* if */
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
  memcpy(ident_buffer+1, locator->symbol_header->identifier,
         (int)ident_length);
  ident_buffer[0] = '~';
  ident_length++;
  position = locator->source_position;
  clear_locator(locator, &position);
  (void)find_symbol(ident_buffer, ident_length, locator);
}  /* tildize_locator */


void change_class_locator_into_constructor_locator(a_symbol_locator *locator)
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
 "change_class_locator_into_constructor_locator: locator not specific symbol");
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
  clear_locator(locator, &pos_curr_token);
  locator->symbol_header = hdr_ptr;
}  /* change_class_locator_into_constructor_locator */


void make_opname_locator(a_token_kind       token,
                         an_opname_kind     opname,
                         a_symbol_locator   *locator,
                         a_source_position  *pos)
/*
Make a locator for the operator name associated with token.  This is
used for C++ constructs like "operator+".  Use pos as the source position.
*/
{
  a_symbol_header_ptr *table_entry = &opname_symbol_table[opname];
  a_symbol_header_ptr hdr_ptr;
  char                *opstr, *str;
  sizeof_t            opname_length;
#define OPERATOR_LEN 8 /* Length of "operator" */

  clear_locator(locator, pos);
  hdr_ptr = *table_entry;
  if (hdr_ptr == NULL) {
    /* First use of this opname.  Allocate the header. */
    *table_entry = hdr_ptr = alloc_symbol_header();
    /* Give the header the name "operatorX" where "X" is the string for the
       operator. */
    switch (opname) {
      case onk_function_call:  opstr = "()";          break;
      case onk_subscript:      opstr = "[]";          break;
      case onk_new:            opstr = " new";        break;
      case onk_delete:         opstr = " delete";     break;
      default:                 opstr = token_names[token];
    }  /* switch */
    opname_length = OPERATOR_LEN + strlen(opstr);
    hdr_ptr->identifier_length = opname_length;
    hdr_ptr->identifier = str = alloc_il((sizeof_t)(opname_length + 1));
    (void)memcpy(str, "operator", OPERATOR_LEN);
    (void)strcpy(str+OPERATOR_LEN, opstr);
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
  char                     *default_name = "conversion";
#define DEFAULT_NAME_LEN 13 /* Length of "conversion" */

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
      /* All the conversion symbols are given the same name.  This is
         because the name is not needed for anything but debug output. */
      sym_hdr->identifier_length = DEFAULT_NAME_LEN;
      sym_hdr->identifier = alloc_il((sizeof_t)DEFAULT_NAME_LEN + 1);
      (void)strcpy(sym_hdr->identifier, default_name, DEFAULT_NAME_LEN);
#if DEBUG
      symbol_name_string_space += DEFAULT_NAME_LEN+1;
#endif /* DEBUG */
    }  /* if */
    locator->symbol_header = conv_hdr->symbol_header;
  }  /* if */
  locator->is_conversion_name = TRUE;
  locator->variant.conversion_result_type = type;
#undef DEFAULT_NAME_LEN
}  /* make_type_conversion_locator */


static a_source_correspondence *source_corresp_entry_for_symbol(
                                                          a_symbol_ptr sym_ptr)
/*
Return a pointer to the source correspondence entry in the IL entry
for the given symbol.  Return NULL if there isn't one (if that's
allowed for that kind of symbol).
*/
{
  a_source_correspondence *scptr = NULL;
  a_constant_ptr          entry_ptr;

  switch (sym_ptr->kind) {
    case sk_macro:
      /* Only manifest constant macros have an associated IL entry. */
      if (!sym_ptr->variant.macro_def->is_manifest_constant) goto no_il_entry;
      entry_ptr = sym_ptr->variant.macro_def->constant_value;
      break;
    case sk_constant:
      entry_ptr = sym_ptr->variant.constant;
      break;
    case sk_type:
    case sk_enum_tag:
      entry_ptr = (a_constant_ptr)sym_ptr->variant.type;
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
      entry_ptr = (a_constant_ptr)sym_ptr->variant.class_struct_union.type;
      break;
    case sk_variable:
    case sk_static_data_member:
      entry_ptr = (a_constant_ptr)sym_ptr->variant.variable;
      break;
    case sk_field:
      entry_ptr = (a_constant_ptr)sym_ptr->variant.field;
      break;
    case sk_routine:
    case sk_member_function:
      entry_ptr = (a_constant_ptr)sym_ptr->variant.routine;
      break;
    case sk_label:
      entry_ptr = (a_constant_ptr)sym_ptr->variant.label;
      break;
    default:
      /* Other cases ignored. */
      goto no_il_entry;
  }  /* switch */
#if CHECKING
  if (entry_ptr == NULL) {
    internal_error("reference_to_symbol: NULL assoc IL entry ptr");
  }  /* if */
#endif /* CHECKING */
  scptr = &entry_ptr->source_corresp;
no_il_entry:;
  return scptr;
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

  if (sym_ptr->kind == (a_symbol_kind)sk_projection) {
    /* Projection symbol. */
    access = sym_ptr->variant.projection.access;
  } else if (sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
    /* Overloaded function.  If all of the functions have the same access,
       the access can be checked.  If not, it will have to be checked later
       when the specific function is selected.
       See overloaded_function_catch_up. */
    if (sym_ptr->variant.overloaded_function.mixed_access) {
      /* Access is unknown at this point. */
      access = (an_access_specifier)as_public;
    } else {
      /* Since the access for all the functions is the same, get it from the
         first function on the list. */
      a_symbol_ptr first_function =
                                  sym_ptr->variant.overloaded_function.symbols;
      access = source_corresp_entry_for_symbol(first_function)->access;
    }  /* if */
  } else {
    /* Normal symbol (not projection or overloaded function). */
    access = source_corresp_entry_for_symbol(sym_ptr)->access;
  }  /* if */
  return access;
}  /* access_for_symbol */
  

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


a_boolean have_member_access_privilege(a_type_ptr class_type)
/*
Return TRUE if we currently have member access privilege to the class
indicated by class_type.
*/
{
  a_boolean have_member_privilege = FALSE;

  if (class_currently_inside_of != NULL &&
      class_type == class_currently_inside_of) {
    /* We are currently in the class of which the symbol is a member, so
       we have member access privilege. */
    have_member_privilege = TRUE;
  } else if (curr_class_befriending_classes != NULL &&
             on_befriending_list(curr_class_befriending_classes, class_type)) {
    /* The symbol is a member of a class that has made the current class
       a friend, so we have member access privilege. */
    have_member_privilege = TRUE;
  } else if (curr_function_befriending_classes != NULL &&
             on_befriending_list(curr_function_befriending_classes,
                                 class_type)) {
    /* The symbol is a member of a class that has made the current function
       a friend, so we have member access privilege. */
    have_member_privilege = TRUE;
  }  /* if */
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
is used is determining access related to protected derivations.
*/
{
  a_boolean             accessible = FALSE;
  a_base_class_ptr      bcp;
  a_derivation_step_ptr dsp;

  /* See if class_type is a base class of derived_class. */
  bcp = find_base_class_of(derived_class, class_type);
  if (bcp != NULL) {
    /* Yes.  See if there are any private derivation steps on the
       derivation. */
    /* The requirement that there be no private derivation steps in the
       derivation from derived_class to class_type guarantees that,
       since we have member access to the derived class, we have access to
       protected members of class_type, which means we have access to
       public members of a protected base class of class_type (which is
       the definition of "accessible" for the protected base class).
       Note that since we have member access to the derived class and
       all the steps are public or protected, friendship on any of the
       steps between the derived class and class_type cannot affect the
       outcome.  Note that this aspect of the C++ language is particularly
       poorly specified, so it wouldn't be surprising if this has to be
       changed. */
    for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
      if (dsp->base_class->access == (an_access_specifier)as_private) {
        /* Private step: no access. */
        goto end_of_routine;
      }  /* if */
    }  /* for */
    /* There are no private steps in the derivation, so there is access
       via this derived class. */
    accessible = TRUE;
  }  /* if */
end_of_routine:;
  return accessible;
}  /* have_protected_access_from_derived_class */
  

static a_boolean have_protected_access_from_befriending_list(
                                       a_class_list_entry_ptr befriending_list,
                                       a_type_ptr             class_type)
/*
Return TRUE if the class type indicated by class_type is accessible
(in the sense required when processing a protected derivation) from
any class on the list of befriending classes given by befriending_list.
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


a_boolean have_protected_member_access_privilege(a_type_ptr class_type)
/*
Return TRUE if we currently have member access privilege to some class
derived from the class indicated by class_type.  This is important in
determining base class/symbol accessibility when there is a protected
derivation.  See p. 214 of "The C++ Programming Language", 2nd Edition.
*/
{
  a_boolean have_member_privilege = FALSE;

  /* This routine looks a lot like have_member_access_privilege. */
  if (class_currently_inside_of != NULL &&
      have_protected_access_from_derived_class(class_type,
                                               class_currently_inside_of)) {
    /* We are currently in a class that is an appropriate derived class
       of class_type, so we have member access privilege. */
    have_member_privilege = TRUE;
  } else if (curr_class_befriending_classes != NULL &&
             have_protected_access_from_befriending_list(
                                 curr_class_befriending_classes, class_type)) {
    /* We are in a class that is a friend of an appropriate derived class
       of class_type, so we have member access privilege. */
    have_member_privilege = TRUE;
  } else if (curr_function_befriending_classes != NULL &&
             have_protected_access_from_befriending_list(
                              curr_function_befriending_classes, class_type)) {
    /* We are in a function that is a friend of an appropriate derived class
       of class_type, so we have member access privilege. */
    have_member_privilege = TRUE;
  }  /* if */
  return have_member_privilege;
}  /* have_protected_member_access_privilege */


a_boolean have_access_to_symbol(a_symbol_ptr symbol)
/*
Return TRUE if the indicated symbol is accessible from the current location
in the source program.
*/
{
  a_boolean             accessible, have_member_access_to_curr_type;
  a_type_ptr            curr_type;
  a_derivation_step_ptr dsp;
  an_access_specifier   access, min_access_needed;

  if (access_for_symbol(symbol) == (an_access_specifier)as_public) {
    /* The member is public, so we don't need to know whether or not we have
       any special member access privileges.  We have access.  This
       applies equally for projection symbols (where it implies that all the
       derivation steps are public) and for symbols created for access 
       declarations. */
    accessible = TRUE;
  } else {
    /* Keep track of the minimum level of access needed on the fundamental
       symbol in order for it to be accessible.  Start with public, since
       we have not established any special member access so far. */
    min_access_needed = (an_access_specifier)as_public;
    /* If the member is inherited from a base class, look at the derivation
       to see if we have access.  Note that projection symbols created for
       access declarations are considered members of the derived class,
       and so do not go through this special handling. */
    if (symbol->kind == (a_symbol_kind)sk_projection &&
        !symbol->variant.projection.access_adjustment_made) {
      curr_type = symbol->class_of_which_a_member;
      /* Work down the derivation from the projection symbol to the fundamental
         symbol to see if the base class is accessible.  Along the way,
         keep track of the minimum access needed in the base class to retain
         accessibility in the derived class. */
      for (dsp = symbol->variant.projection.extra_info->
                                            fundamental_base_class->derivation;
           dsp != NULL;
           dsp = dsp->next) {
        /* The rules for derivation are as follows:  If the derivation
           access for the class is "public", the access of public and
           protected members stays as it is; if it is "protected", public
           members become protected and protected members are unaffected;
           if it is "private", public and protected symbols become private.
           Members private to the base class become inaccessible to the
           derived class in every case. (ARM 11.2.)  The following table
           summarizes the transformations:

                      derivation:
                             private        protected      public
           symbol:        -----------------------------------------------
             public       |  private        protected      public
                          |
             protected    |  private        protected      protected
                          |
             private      |  inaccessible   inaccessible   inaccessible
                          |
             inaccessible |  inaccessible   inaccessible   inaccessible

           Here, we work with the transformation in the other direction,
           i.e., given the minimum access needed in the derived class and
           the kind of derivation, what minimum access is needed in the
           base class:

                      derivation:
           min needed        private        protected      public
           in derived:    -----------------------------------------------
             public       |  impossible     impossible     public
                          |
             protected    |  impossible     protected      protected
                          |
             private      |  protected      protected      protected

        */
        /* See if we have member access privilege to the current type.  In
           the case of a protected derivation, we might have member access here
           because we have member access to a derived class of the current
           type. */
        have_member_access_to_curr_type =
               have_member_access_privilege(curr_type) ||
               (dsp->base_class->access == (an_access_specifier)as_protected &&
                have_protected_member_access_privilege(curr_type));
        if (have_member_access_to_curr_type) {
          /* If we have member access privilege to the current type, 
             the minimum access needed is private. */
          min_access_needed = (an_access_specifier)as_private;
        }  /* if */
        /* First step: Detect the "impossible" cases, the ones where there
           is no access in the base class that will produce the needed
           access in the derived class. */
        if (is_more_accessible(min_access_needed, dsp->base_class->access)) {
          /* We need more access than is possible.  Therefore the symbol
             is inaccessible. */
          accessible = FALSE;
          goto access_determined;
        }  /* if */
        /* Second step: If the minimum access needed is private, we will need
           at least protected access in the base class. */
        if (min_access_needed == (an_access_specifier)as_private) {
          min_access_needed = (an_access_specifier)as_protected;
        }  /* if */
        /* See if the base class is accessible.  This code is like
           is_accessible_base_class but is written in this way to
           avoid another call of have_member_access_privilege.  Also, the
           protected derivation special case was done above. */
        if (dsp->base_class->access == (an_access_specifier)as_public) {
          /* Public base class.  The base class is accessible. */
        } else if (have_member_access_to_curr_type) {
          /* Private or protected base class, and we have member access to
             the current type; or protected base class, and we have member
             access to a derived class of the current type.  The base class
             is accessible. */
        } else {
          /* Other cases.  The base class is inaccessible. */
          accessible = FALSE;
          goto access_determined;
        }  /* if */
        /* The base class is accessible; continue at the next level. */
        curr_type = dsp->base_class->type;
      }  /* for */
      /* The fundamental symbol class is accessible.  Continue with access
         checking on the fundamental symbol. */
      reduce_projection_symbol_to_fundamental_symbol(symbol);
    }  /* if */
    access = access_for_symbol(symbol);
    /* If we have the needed minimum access, we have access to the symbol
       without special member access to its class. */
    if (!is_more_accessible(min_access_needed, access)) {
      accessible = TRUE;
    } else {
      /* We have access if and only if we have member access privilege to the
         class of which the symbol is a member. */
      accessible = have_member_access_privilege(
                                              symbol->class_of_which_a_member);
    }  /* if */
  }  /* if */
access_determined:
  return accessible;
}  /* have_access_to_symbol */


void member_check_ambiguity_and_verify_access(a_symbol_locator *locator)
/*
Verify that the indicated member symbol is not ambiguous and that we have
access to it; issue an error if appropriate.  This is called from the macro
check_ambiguity_and_verify_access.  In case of an ambiguity, the locator
is set to an error locator.
*/
{
  a_symbol_ptr symbol = locator->specific_symbol;

  /* Issue an error if the symbol is ambiguous.  Only a symbol projected
     into a derived class by inheritance can be ambiguous.  Ambiguity checking
     must precede access control (ARM, 10.1.1). */
  if (symbol->kind == (a_symbol_kind)sk_projection &&
      symbol->variant.projection.ambiguous) {
    pos_error(ec_ambiguous_name, &locator->source_position);
    set_to_error_locator(*locator);
  } else if (!have_access_to_symbol(symbol)) {
    /* The symbol is not accessible. */
    pos_error(ec_no_access_to_name, &locator->source_position);
  }  /* if */
}  /* member_check_ambiguity_and_verify_access */


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
class) and the access specification *access.
*/
{
  a_symbol_ptr     sym;
  a_scope_number   scope_number;

  db_enter(4, "symbol_projected_from_base_class");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for \"%s\" in base class \"%s\"\n",
                     locator->symbol_header->identifier,
                     base_class->type->source_corresp.name);
  }  /* if */
#endif /* DEBUG */
  /* First look in the scope of the base class itself. */
  /* We need search only the inactive symbols list, since a class cannot be
     declared as a base class unless it has been fully defined (at which point
     each of its member symbols is moved off the active list and onto the
     inactive list).  class_qualified_id_lookup is not called for two
     reasons:  to avoid unnecessary overhead and to prevent extra projection
     symbols from being created. */
  scope_number = base_class->type->
                    variant.class_struct_union.extra_info->assoc_scope->number;
  sym = inactive_symbol_list_from_locator(*locator);
  for (; sym != NULL; sym = sym->next) {
    if (sym->decl_scope == scope_number) {
      if (must_be_tag && !is_tag_symbol(fundamental_symbol_of(sym))) {
        /* Tag symbol is required and this (or the symbol of which it is the
           projection) isn't one.  Keep looking. */
      } else {
        /* Found it. */
#if CHECKING
        if (name_space_for_symbol_kind[(int)sym->kind] != nsk_other) {
          internal_error(
               "symbol_projected_from_base_class: unexpected name space kind");
        }  /* if */
#endif /* CHECKING */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  if (sym != NULL) {
    /* Found in the base class itself. */
#if DEBUG
    if (debug_level >= 4) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
    *access = access_for_symbol(sym);
    if (sym->kind == (a_symbol_kind)sk_projection) {
      *ambiguous = sym->variant.projection.ambiguous;
    }  /* if */
  } else {
    /* Not found in the base class, so examine its own base classes, if any. */
    sym = find_progenitor_symbol(base_class->type, locator, must_be_tag,
                                 path, access, ambiguous);
  }  /* if */
  if (sym != NULL) {
    /* Some symbol was found.  Determine its derivation and access
       specification. */
    *path = make_derivation_step(base_class, *path);
    *access = compute_access(*access, base_class->access);
  }  /* if */
  db_exit();
  return sym;
}  /* symbol_projected_from_base_class */


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
  a_derivation_step_ptr  tail1, tail2;
  a_boolean              equiv = FALSE;

  db_enter(4, "projections_are_equivalent");
  equiv = FALSE;
  if (fundamental_symbol_of(sym1) == fundamental_symbol_of(sym2)) {
    /* Fundamental symbols are the same.  See if the paths are equivalent.
       Note that if either symbol is a projection symbol, it path must
       be (temporarily) modified to reflect the path all the way to
       the fundamental symbol. */
    tail1 = tail2 = NULL;
    if (sym1->kind == (a_symbol_kind)sk_projection) {
      for (tail1 = path1; tail1->next != NULL; tail1 = tail1->next) {}
      tail1->next = sym1->variant.projection.extra_info->
                                      fundamental_base_class->derivation;
    }  /* if */
    if (sym2->kind == (a_symbol_kind)sk_projection) {
      for (tail2 = path2; tail2->next != NULL; tail2 = tail2->next) {}
      tail2->next = sym2->variant.projection.extra_info->
                                      fundamental_base_class->derivation;
    }  /* if */
    /* Compare the modified paths. */
    if (equivalent_paths(path1, path2)) equiv = TRUE;
    /* Restore the paths to the original state, if necessary. */
    if (tail1 != NULL) tail1->next = NULL;
    if (tail2 != NULL) tail2->next = NULL;
  }  /* if */
  db_exit();
  return equiv;
}  /* projections_are_equivalent */


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
    if (bcp->direct && (!bcp->is_virtual || bcp->derivation->next == NULL)) {
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
            if (check_for_dominance(sym, other_sym, other_path)) {
              /* sym dominates other_sym, resolving a potential ambiguity. */
              free_derivation_step(other_path);
            } else if (check_for_dominance(other_sym, sym, *path)) {
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
              if (is_type_symbol(other_sym)) {
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


a_symbol_ptr find_projected_symbol(a_type_ptr        class_ptr,
                                   a_symbol_locator  *locator,
                                   a_boolean         must_be_tag,
                                   a_boolean         must_be_type_name,
                                   a_boolean         add_to_active_list,
                                   a_symbol_ptr      insert_sym)
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
  a_symbol_ptr                 new_sym, progenitor_sym;
  an_access_specifier          access;
  a_boolean                    ambiguous = FALSE;
  a_scope_stack_entry_ptr      ssep;

  db_enter(4, "find_projected_symbol");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "looking for projection of \"%s\" into class \"%s\"\n",
                     locator->symbol_header->identifier,
                     class_ptr->source_corresp.name);
  }  /* if */
#endif /* DEBUG */
  progenitor_sym = find_progenitor_symbol(class_ptr, locator, must_be_tag,
                                          &path, &access, &ambiguous);
  if (progenitor_sym == NULL) {
    /* Indicate that no symbol was found by returning a NULL pointer. */
    new_sym = NULL;
  } else if (must_be_type_name && !is_type_symbol(progenitor_sym)) {
    /* The symbol found is not a type name symbol, so do not create a
       projection for it. */
    new_sym = NULL;
  } else {
    /* Create a new symbol based on the symbol returned. */
    new_sym = make_projection_symbol(progenitor_sym, class_ptr,
                                     path, ambiguous);
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
  db_exit();
  return new_sym;
}  /* find_projected_symbol */


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
  a_boolean               must_be_type_name;
  a_boolean               need_complicated_search, look_for_projected_symbol;
  a_boolean               add_to_active_list;
  a_name_space_kind       required_name_space_kind =
                            (C_dialect != C_dialect_cplusplus && must_be_tag) ?
                                                           nsk_tag : nsk_other;

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym)                                     \
  ((!must_be_class || is_class_symbol(fundamental_symbol_of(sym))) && \
   (!must_be_tag   || is_tag_symbol  (fundamental_symbol_of(sym))))
/* Local macro that tests whether or not a symbol on the active list
   is acceptable.  See if the symbol is in the proper name space. */
#define is_acceptable_active_symbol(sym)                              \
  (name_space_for_symbol_kind[(int)sym->kind] == required_name_space_kind && \
   is_acceptable_symbol(sym))

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
    /* If there are no classes and no class reactivations on the scope stack,
       or if the symbol cannot be a member symbol, the fast algorithm can be
       used.  This is always the case in C. */
    need_complicated_search = FALSE;
    if (num_classes_on_scope_stack != 0) {
      /* There is at least one class or class reactivation on the scope stack.
         We may need to use the complicated algorithm, but if we can show
         that the name does not appear as a member symbol on the inactive
         list, we can still use the fast algorithm.  Since all symbols on
         the inactive list are member symbols, the existence of an inactive
         list means the symbol in question is potentially a member symbol.
         Member symbols on the active list do not matter because they cannot
         be projected into another class by inheritance (the class they're
         in is not fully defined yet, so it can't have been used in a
         derivation), and because they would be found properly on the search
         of the active list in the fast algorithm. */
      if (inactive_symbol_list != NULL) need_complicated_search = TRUE;
    }  /* if */
    if (!need_complicated_search) {
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
      /* There is at least one class or class reactivation on the stack, so
         a more complicated search is required. */
#if DEBUG
      num_slow_id_lookups++;
#endif /* DEBUG */
      /* Work out from the innermost scope on the stack, and look at each
         scope.  If the scope is a class reactivation, look on the inactive
         list for a symbol from that scope.  Otherwise, search part of the
         active list to look for an active symbol.  We maintain a pointer
         to the point in the active list up to which we've searched.  This
         works because the symbols on the active list are in order according
         to the scope they're in, from innermost scope to outermost. */
      prev_active_sym = NULL;
      active_sym = active_symbol_list;
      ssep = &scope_stack[depth_scope_stack];
      /* Since there is a class or class reactivation on the stack, we know the
         stack has at least two entries (the file scope and the class or
         class reactivation). */
      for (;;) {
        if (ssep->kind == (a_scope_kind)sck_class_reactivation) {
          /* Look on the inactive list for a symbol from this reactivated
             scope. */
          tag_symbol = NULL;
          for (inactive_sym = inactive_symbol_list;
               inactive_sym != NULL;
               inactive_sym = inactive_sym->next) {
            if (inactive_sym->decl_scope == ssep->number) {
              if (is_acceptable_symbol(inactive_sym)) {
                /* Found a symbol. */
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
          /* There is no inactive symbol that is in this class. */
          /* Look for a symbol projected (inherited) into this class. */
          look_for_projected_symbol = TRUE;
          add_to_active_list = FALSE;
          insert_sym = NULL;
        } else {
          /* Not a class reactivation, i.e., normal scope.  Search through
             any symbols on the front of the active list that are from the
             associated scope, and see if any one is the symbol desired. */
          for (;active_sym != NULL && active_sym->decl_scope == ssep->number;
               prev_active_sym = active_sym, active_sym = active_sym->next) {
            if (is_acceptable_active_symbol(active_sym)) {
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
          sym = find_projected_symbol(ssep->assoc_type, locator, must_be_tag,
                                      must_be_type_name, add_to_active_list,
                                      insert_sym);
          if (sym != NULL) {
            /* A projection symbol was created.  It must still satisfy the
               constraints for this lookup. */
            if (is_acceptable_symbol(sym)) goto end_lookup;
            sym = NULL;
          }  /* if */
        }  /* if */
        /* End the loop when we reach the bottom of the scope stack. */
        if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) break;
        ssep--;
      }  /* for */
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
  a_class_symbol_supplement_ptr
               extra_info;
  a_symbol_ptr insert_sym;
  a_boolean    add_to_active_list;

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym)                                     \
  ((sym)->class_of_which_a_member == class_type &&                    \
   (!must_be_class || is_class_symbol(sym)))

  db_enter(4, "class_qualified_id_lookup");
#if CHECKING
  if (options & IDL_MUST_BE_TAG) {
    /* IDL_MUST_BE_TAG is not implemented. */
    internal_error("class_qualified_id_lookup: IDL_MUST_BE_TAG specified");
  }  /* if */
  if (locator->is_qualified_name) {
    internal_error("class_qualified_id_lookup: is_qualified_name");
  }  /* if */
#endif /* CHECKING */
  /* Remove any typedef on the class type. */
  class_type = skip_typerefs(class_type);
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
      extra_info = class_symbol->variant.class_struct_union.extra_info;
      if (locator->symbol_header == class_symbol->header) {
        /* Looking up the class name within itself.  Return the constructor if
           there is one. */
        sym = extra_info->constructor;
        if (sym != NULL) {
          /* There is a constructor.  Change the locator symbol header to
             the header for the constructor rather than the header for the
             class.  They have the same name, but different headers. */
          locator->symbol_header = sym->header;
          goto end_lookup;
        }  /* if */
      } else if (extra_info->destructor != NULL &&
                 locator->symbol_header == extra_info->destructor->header) {
        /* This is the destructor. */
        sym = extra_info->destructor;
        goto end_lookup;
      }  /* if */
      /* The name was not found.  Try looking for a member symbol that can
         be projected into the class. */
      determine_projected_symbol_insert_location(locator,
                                                 class_type,
                                                 &add_to_active_list,
                                                 &insert_sym);
      sym = find_projected_symbol(class_type, locator, /*must_be_tag=*/FALSE,
                                  /*must_be_type_name=*/FALSE,
                                  add_to_active_list, insert_sym);
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
  a_scope_number file_scope_number = scope_stack[DEPTH_OF_FILE_SCOPE].number;

/* Local macro that tests whether or not a symbol is acceptable. */
/* The name space test is needed when searching the file scope, so
   sk_extern_variable and sk_extern_routine are not found. */
#define is_acceptable_symbol(sym)                                     \
  ((sym)->decl_scope == file_scope_number &&                          \
   name_space_for_symbol_kind[sym->kind] == nsk_other &&              \
   (!must_be_class || is_class_symbol(sym)))

  db_enter(4, "file_scope_id_lookup");
#if CHECKING
  if (options & IDL_MUST_BE_TAG) {
    /* IDL_MUST_BE_TAG is not implemented. */
    internal_error("file_scope_id_lookup: IDL_MUST_BE_TAG specified");
  }  /* if */
  if (locator->is_qualified_name) {
    internal_error("file_scope_id_lookup: is_qualified_name");
  }  /* if */
#endif /* CHECKING */
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
operator.
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
    sym = class_qualified_id_lookup(&locator, class_type, IDL_NO_OPTIONS);
  }  /* if */
  return sym;
}  /* opname_member_function_symbol */


a_symbol_ptr opname_function_symbol(an_opname_kind kind)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind, or NULL if there is no such operator.
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;
  a_symbol_locator    locator;

  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Yes.  Look for one that's visible. */
    clear_locator(&locator, &pos_curr_token);
    locator.symbol_header = symhdr;
    sym = normal_id_lookup(&locator, IDL_NO_OPTIONS);
  }  /* if */
  return sym;
}  /* opname_function_symbol */


/*
Return TRUE if the scope stack entry kind given by kind is for something
that has an effect on access control (a class, class reactivation, or
function).  Access control only exists in C++.
*/
#define is_scope_kind_that_affects_access_control(kind)               \
  (C_dialect == C_dialect_cplusplus &&                                \
   ((kind) == (a_scope_kind)sck_class_struct_union ||                 \
    (kind) == (a_scope_kind)sck_class_reactivation ||                 \
    (kind) == (a_scope_kind)sck_function))


static void clear_access_control_scope(void)
/*
Clear the access control variables class_currently_inside_of,
curr_class_befriending_classes, and curr_function_befriending_classes.
*/
{
  class_currently_inside_of = NULL;
  curr_class_befriending_classes = NULL;
  curr_function_befriending_classes = NULL;
}  /* clear_access_control_scope */


static void set_access_control_scope(a_scope_stack_entry_ptr ssep)
/*
Set the access control variables class_currently_inside_of,
curr_class_befriending_classes, and curr_function_befriending_classes
for the scope stack entry pointed to by ssep.
is_scope_kind_that_affects_access_control(ssep->kind) must be TRUE.
*/
{
  a_scope_stack_entry_ptr prev_ssep;

  clear_access_control_scope();
  if (ssep->kind == (a_scope_kind)sck_function) {
    /* The scope entry is for a function.  Remember the classes that have
       befriended it, if any. */
    curr_function_befriending_classes =
                      ssep->il_scope->variant.routine.ptr->befriending_classes;
    /* If there is a preceding class reactivation entry on the scope stack
       (because this is a member function) or a class entry (because this is
       an inline function definition), save the class as the active
       one.  Note that it is guaranteed that there is an entry below the
       function entry. */
    prev_ssep = ssep-1;
    if (prev_ssep->kind == (a_scope_kind)sck_class_struct_union ||
        prev_ssep->kind == (a_scope_kind)sck_class_reactivation) {
      class_currently_inside_of = prev_ssep->assoc_type;
    }  /* if */
  } else {
#if CHECKING
    if (ssep->kind != (a_scope_kind)sck_class_struct_union &&
        ssep->kind != (a_scope_kind)sck_class_reactivation) {
      internal_error("set_access_control_scope: bad scope kind");
    }  /* if */
#endif /* CHECKING */
    /* Class scope or class reactivation scope. */
    class_currently_inside_of = ssep->assoc_type;
  }  /* if */
  /* If we are currently inside a class (as determined above), remember
     also the classes that have befriended the active class. */
  if (class_currently_inside_of != NULL) {
    curr_class_befriending_classes = class_currently_inside_of->variant.
                            class_struct_union.extra_info->befriending_classes;
  }  /* if */
}  /* set_access_control_scope */


a_scope_ptr push_scope(a_scope_kind   kind,
		       a_scope_number scope_number_to_reuse,
                       a_type_ptr     assoc_type,
                       a_routine_ptr  assoc_routine)
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
meaningful (function prototype, class, and class reactivation scopes);
it must be NULL in other cases.  assoc_routine points to a routine
for the function scope case; it must be NULL in other cases.
*/
{
  a_scope_stack_entry_ptr ssep;
  a_scope_ptr             sp = NULL;

  db_enter(3, "push_scope");
  if (depth_scope_stack+1 == size_scope_stack) {
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
  if ((kind == (a_scope_kind)sck_function &&
                                   scope_number_to_reuse != NO_SCOPE_NUMBER) ||
      kind == (a_scope_kind)sck_class_reactivation) {
    /* For function scopes, reuse the scope used for the parameters
       in the function declarator. */
    /* For class reactivations, re-establish the class scope. */
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
  if (kind == (a_scope_kind)sck_file || kind == (a_scope_kind)sck_function) {
    /* Start a new memory region for the file scope or a function scope.
       This ensures that the intermediate language is divided into 
       manageable pieces.  This call also allocates the top-level
       scope entry for the region. */
    sp = new_il_region(kind, ssep->number, assoc_routine);
    ssep->il_memory_region = curr_il_region_number;
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        kind == (a_scope_kind)sck_class_struct_union) {
      /* In C++, a class/struct/union has an associated IL scope (but no new
         memory region). */
      sp = alloc_scope(kind, ssep->number, (a_routine_ptr)NULL);
    } else {
      /* For function prototype and block scopes, the IL scope is not
         allocated until it is needed, because usually it will not be needed.
         For struct/unions in C, no scope is ever allocated.  For class
         reactivations in C++, no scope is ever allocated. */
      sp = NULL;
    }  /* if */
    /* For scopes for which a new memory region is not begun, the associated
       memory region is the same as for the enclosing scope (there must be an
       enclosing scope, because the scope we're opening here is not the file
       scope).  Note that because of "temporary" switches into the file scope
       memory region, the new scope may in fact spend no time at all in
       its official associated memory region. */
    ssep->il_memory_region = (ssep-1)->il_memory_region;
  }  /* if */
  /* Fill in the fields of the scope entry. */
  ssep->kind                     = kind;
  ssep->current_access           = (an_access_specifier)as_public;
  ssep->symbols                  = NULL;
  ssep->last_symbol              = NULL;
  ssep->il_scope                 = sp;
  ssep->assoc_type               = assoc_type;
  ssep->array_type_fixup_list    = NULL;
  ssep->extern_type_fixup_list   = NULL;
  ssep->shareable_constants_list = NULL;
  ssep->delayed_scan_fixup_list  = NULL;
  ssep->last_delayed_scan_fixup  = NULL;
  ssep->last_parameter           = NULL;
  ssep->last_constant            = NULL;
  ssep->last_type                = NULL;
  ssep->last_variable            = NULL;
  ssep->last_label               = NULL;
  ssep->last_routine             = NULL;
  ssep->first_scope              = NULL;
  ssep->last_scope               = NULL;
  ssep->last_dynamic_init        = NULL;
  /* Put the associated type (if any) into the IL scope (if any). */
  /* Note that the corresponding routine case was handled by the
     new_il_region call. */
  if (assoc_type != NULL && sp != NULL) sp->variant.assoc_type = assoc_type;
  /* Maintain the current declarative level.  It is the same as 
     depth_scope_stack except when struct/union field scopes are
     active; when they are, it indicates the first non-struct-or-union
     scope.  In C++, struct/union/class scopes are real scopes; however,
     class reactivations are not real scopes. */
  if ((C_dialect != C_dialect_cplusplus) ?
          /* C -- struct/union classes are not real scopes. */
          (kind != (a_scope_kind)sck_class_struct_union) :
          /* C++ -- class reactivations are not real scopes. */
          (kind != (a_scope_kind)sck_class_reactivation)) {
    decl_scope_level = depth_scope_stack;
  }  /* if */
  /* Maintain the depth of the innermost function scope. */
  if (kind == (a_scope_kind)sck_function) {
    depth_innermost_function_scope = depth_scope_stack;
  }  /* if */
  /* Keep track of the number of current classes and class reactivations.
     (If either count is non-zero name lookup is more involved.) */
  if (C_dialect == C_dialect_cplusplus) {
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      num_classes_on_scope_stack++;
    }  /* if */
  }  /* if */
  /* Maintain the depth of the innermost stack entry that affects access
     control. */
  if (is_scope_kind_that_affects_access_control(kind)) {
    set_access_control_scope(ssep);
  }  /* if */
  db_exit();
  return sp;
}  /* push_scope */


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
  a_type_ptr      var_type;
  a_variable_ptr  var_ptr;
  a_routine_ptr   rout_ptr;
  an_error_code   warning_code;

  warning_code = ec_no_error;
  switch (sym->kind) {
    case sk_variable:
      /* Variable or parameter. */
      var_ptr = sym->variant.variable;
      storage_class = var_ptr->storage_class;
      /* Check for a file-scope variable with no storage class and
         an incomplete type, as in "struct incomplete v;".  Issue an
         error.  The case of an incomplete array is handled by the
         sk_extern_variable case, below (a file-scope variable always
         has linkage, so it will always have an sk_extern_variable
         entry). */
      /* sc_unspecified implies file scope, so no need to check that. */
      /* Note also that if this test succeeds (i.e., the variable has
         storage class sc_unspecified), we do not do the test for
         referenced.  That's because an external variable can be assumed
         to be referenced from another compilation unit.  The referenced
         flag in the IL entry is set slightly later, in the
         sk_extern_variable processing. */
      if (storage_class == (a_storage_class)sc_unspecified) {
        var_type = skip_typerefs(var_ptr->type);
        if (is_completable_type(var_type)) {
          if (is_array_with_complete_element_type(var_type)) {
            /* Array type.  The test for complete element type disallows
               arrays of incomplete struct/unions (which are an
               extension).  Ignore this case here; it's handled by the
               sk_extern_variable case. */
          } else {
            /* Something other than an array -- an error. */
            /* E.g., struct xyz v; where struct xyz is never resolved. */
            pos_st_error(ec_var_retained_incomp_type, &sym->decl_position,
                         sym->header->identifier);
          }  /* if */
        }  /* if */
      } else if (!sym->referenced) {
        /* An unreferenced variable or parameter. */
        if (var_ptr->is_parameter) {
          /* A parameter.  Warn unless a lint-style "argsused" comment
             appeared.  Also do not warn for parameters of "main". */
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
            warning_code = ec_parameter_declared_but_not_referenced;
          }  /* if */
        } else {
          /* A normal variable (not a parameter). */
          if (storage_class == (a_storage_class)sc_extern) {
            /* No warning for unused "extern" variables; this is a
               long-standing C tradition. */
          } else {
            /* An unreferenced variable. */
            warning_code = ec_variable_declared_but_not_referenced;
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case sk_routine:
      /* Function. */
      rout_ptr = sym->variant.routine;
      if (sym->referenced) {
        /* Referenced function. */
        if (rout_ptr->storage_class == (a_storage_class)sc_static &&
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
            pos_st_error(ec_routine_definition_missing,
                         &sym->decl_position, sym->header->identifier);
          }  /* if */
        }  /* if */
      } else {
        /* Unreferenced function. */
        storage_class = rout_ptr->storage_class;
        if (storage_class == (a_storage_class)sc_unspecified) {
          /* Externally-visible function.  Assume a reference from some
             other compilation unit. */
          rout_ptr->source_corresp.referenced = TRUE;
        } else if (storage_class == (a_storage_class)sc_extern) {
          /* No warning on unused "extern" routines; this is a
             long-standing C tradition. */
#if ASM_FUNCTION_ALLOWED
        } else if (storage_class == (a_storage_class)sc_asm) {
          /* "asm" functions don't generate any code unless referenced,
             and may appear in header files, so no warning is generated. */
#endif /* ASM_FUNCTION_ALLOWED */
        } else {
          /* An unreferenced routine. */
          warning_code = ec_routine_declared_but_not_referenced;
        }  /* if */
      }  /* if */
      break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
    case sk_enum_tag:
      /* Struct, union, or enum tag. */
      if (is_incomplete_type(sym->variant.type)) {
        /* A tag that was never completed.  This is not an error.
           It's not even a warning, because people really do this
           intentionally.  Declaring something of this type would
           be an error; declaring something a pointer to this type
           would be allowed. */
        /* Add it now to the scope's type list.  It was not added
           previously because no actual definition appeared. */
        add_to_types_list(sym->variant.type, /*at_file_scope=*/FALSE,
                          /*in_old_style_param_decl_list=*/FALSE);
      }  /* if */
      break;
    case sk_label:
      /* Label. */
      if (sym->variant.label->variant.exec_stmt == NULL) {
        /* A label that was used but never defined. */
        pos_st_error(ec_label_never_defined, &sym->decl_position,
                     sym->header->identifier);
      } else if (!sym->referenced) {
        /* An unreferenced label. */
        warning_code = ec_label_declared_but_not_referenced;
      }  /* if */
      break;
    case sk_extern_variable:
      /* Symbol for a variable with linkage. */
      /* Check for a file-scope incomplete array with no storage class,
         for example "int a[];" at file scope.  Such an array is
         defined by the standard (3.7.2 semantics) to be equivalent
         to "int a[] = {0};"; change the size to 1 here.  However, if
         the extern_variable entry has more complete type information
         (i.e., an exact dimension), use that. */
      var_ptr = sym->variant.extern_symbol_descr->variant.variable;
      /* sc_unspecified implies file scope, so no need to check that. */
      if (var_ptr->storage_class == (a_storage_class)sc_unspecified) {
        var_type = skip_typerefs(var_ptr->type);
        if (is_completable_type(var_type)) {
          if (is_array_with_complete_element_type(var_type)) {
            /* Array type.  The test for complete element type disallows
               arrays of incomplete struct/unions (which are an
               extension). */
            if (!is_incomplete_type(sym->variant.extern_symbol_descr->type)) {
              /* The external symbol entry has a dimension for the array.
                 Use it.  This would happen for
                   int a[];
                   main () {extern int a[5];}
              */
              var_ptr->type = sym->variant.extern_symbol_descr->type;
            } else {
              /* There is no additional information; the array has an
                 unknown size. */
              if (C_dialect != C_dialect_pcc) {
                /* Change the array size to 1. */
                a_type_ptr array_type = alloc_type((a_type_kind)tk_array);
                copy_type(var_type, array_type);
                array_type->variant.array.number_of_elements = 1;
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
          }  /* if */
        }  /* if */
        /* If the storage class remains sc_unspecified, set the
           referenced flag now to indicate possible references from other
           compilation units. */
        if (var_ptr->storage_class == (a_storage_class)sc_unspecified) {
          var_ptr->source_corresp.referenced = TRUE;
        }  /* if */
      }  /* if */
      break;
    default:
      /* No processing for other kinds. */
      break;
  }  /* switch */
  if (warning_code != ec_no_error) {
    /* Issue a warning for an unreferenced entity.  However, demote the
       warning to a remark if the entity is declared in an include file. */
    if (seq_is_in_include_file(sym->decl_position.seq)) {
      pos_st_remark(warning_code, &sym->decl_position,
                    sym->header->identifier);
    } else {
      pos_st_warning(warning_code, &sym->decl_position,
                     sym->header->identifier);
    }  /* if */
  }  /* if */
}  /* end_of_scope_symbol_check */


void pop_scope(void)
/*
End a name scope by popping an entry off the scope stack.
*/
{
  a_scope_stack_entry_ptr  ssep, parent_ssep;
  a_symbol_ptr             sym;
  a_routine_ptr            curr_routine = NULL;
  a_symbol_ptr             prev_sym, next_sym;
  a_memory_region_number   old_memory_region_number, new_memory_region_number;
  a_scope_kind             kind;
  an_extern_type_fixup_ptr etfp;
  a_scope_depth            scope_depth;
  a_boolean                old_region_still_needed;

  db_enter(3, "pop_scope");
  ssep = &scope_stack[depth_scope_stack];
  kind = ssep->kind;
  if (kind == (a_scope_kind)sck_function) {
    /* If the scope is for a routine, get a pointer to the routine. */
    curr_routine = ssep->il_scope->variant.routine.ptr;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (ssep->symbols != NULL) {
      fprintf(f_debug, "pop_scope: number = %d, depth = %d",
              ssep->number, depth_scope_stack);
      if (curr_routine != NULL) {
        fprintf(f_debug, ", curr_routine = %s",
                curr_routine->source_corresp.name);
      } else if ((kind == (a_scope_kind)sck_class_struct_union ||
                  kind == (a_scope_kind)sck_class_reactivation) &&
                 ssep->assoc_type != NULL) {
        fprintf(f_debug, ", class = \"%s\"",
                ssep->assoc_type->source_corresp.name);
      } else {
        fputs(", kind = ", f_debug);
        db_scope_kind(kind);
      }  /* if */
      fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Remove the symbols declared in this scope from the symbol table.
     Check for unreferenced symbols, and issue warnings for those. */
  prev_sym = NULL;
  for (sym = ssep->symbols; sym != NULL; sym = next_sym) {
    next_sym = sym->next_in_scope;
    end_of_scope_symbol_check(sym, curr_routine);
    /* Remove the symbol from the symbol table. */
    remove_symbol(sym);
    /* Keep all the removed symbols of the scope linked together.  This is
       needed to keep the list of prototype scope symbols together for
       later re-entry, and for the member symbols of a class. */
    if (prev_sym != NULL) prev_sym->next_in_scope = sym;
    prev_sym = sym;
    /* Put struct/union/class members on the inactive list of the proper symbol
       header. */
    if (kind == (a_scope_kind)sck_class_struct_union) {
      sym->next = sym->header->inactive_symbols;
      sym->header->inactive_symbols = sym;
    }  /* if */
  }  /* for */
  if (ssep->first_scope != NULL) {
    /* Transfer the list of scopes nested within the current scope
       to the IL scope entry if there is one, or otherwise add it to
       the local scopes list for the parent scope.  We are doing this
       to avoid allocating IL scopes for empty block scopes. */
    if (ssep->il_scope != NULL) {
      /* There is an allocated IL scope entry. */
      ssep->il_scope->scopes = ssep->first_scope;
    } else {
      /* Add the list of scopes to the list for the parent scope. */
      parent_ssep = ssep-1;
      if (parent_ssep->first_scope == NULL) {
        parent_ssep->first_scope = ssep->first_scope;
      } else {
        parent_ssep->last_scope->next = ssep->first_scope;
      }  /* if */
      parent_ssep->last_scope = ssep->last_scope;
    }  /* if */
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
  /* Keep track of the number of current classes and class reactivations.
     (If either count is non-zero name lookup is more involved.) */
  if (C_dialect == C_dialect_cplusplus) {
    if (kind == (a_scope_kind)sck_class_struct_union ||
        kind == (a_scope_kind)sck_class_reactivation) {
      num_classes_on_scope_stack--;
    }  /* if */
  }  /* if */
  /* Maintain the depth of the innermost stack entry that affects access
     control. */
  if (is_scope_kind_that_affects_access_control(kind)) {
    a_scope_stack_entry_ptr access_control_ssep;
    for (access_control_ssep = ssep-1;
         access_control_ssep != &scope_stack[DEPTH_OF_FILE_SCOPE];
         access_control_ssep--) {
      if (is_scope_kind_that_affects_access_control(
                                                  access_control_ssep->kind)) {
        set_access_control_scope(access_control_ssep);
        goto done_with_access_control_setting;
      }  /* if */
    }  /* for */
    /* No entry remaining on the stack affects access control. */
    clear_access_control_scope();
done_with_access_control_setting:;
  }  /* if */
  /* Determine and remember the current (old) memory region, to see
     if it changes when returning to the outer scope. */
  old_memory_region_number = ssep->il_memory_region;
  /* Determine the memory region to restore for the outer scope. */
  new_memory_region_number = ssep->prev_il_memory_region;
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
    lower_il_memory_region(old_memory_region_number);
#endif /* DO_IL_LOWERING */
    done_with_memory_region(old_memory_region_number);
  }  /* if */
  /* Pop the stack. */
  if (--depth_scope_stack >= 0) {
    /* The stack is not empty, so do anything necessary to activate the
       new top entry. */
    /* If the new memory region is not the same as the old, activate it. */
    if (new_memory_region_number != old_memory_region_number) {
      switch_il_region(new_memory_region_number);
    }  /* if */
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
    a_scope_kind kind = scope_stack[decl_scope_level].kind;
    if ((C_dialect != C_dialect_cplusplus) ?
            /* C -- struct/union scopes are not real scopes. */
            (kind != (a_scope_kind)sck_class_struct_union) :
            /* C++ -- class reactivations are not real scopes. */
            (kind != (a_scope_kind)sck_class_reactivation)) break;
  }  /* for */
  /* Maintain the depth of the innermost function scope. */
  if (depth_innermost_function_scope > depth_scope_stack) {
    for (depth_innermost_function_scope = depth_scope_stack;
         depth_innermost_function_scope >= DEPTH_OF_FILE_SCOPE &&
              scope_stack[depth_innermost_function_scope].kind !=
                                                    (a_scope_kind)sck_function;
         depth_innermost_function_scope--) {}
  }  /* if */
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
  push_scope((a_scope_kind)sck_class_reactivation, il_scope->number,
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


static void write_xref_entry(a_symbol_reference_kind kind,
                             a_symbol_ptr            sym_ptr,
                             a_source_position       *source_position)
/*
Write information to f_xref_info describing a reference of kind "kind"
to symbol "sym_ptr" at source position "source_position".  This routine
should only be called if cross-reference information is being generated
(i.e., f_xref_info != NULL).
*/
{
  char          code;
  char          *file_name, *full_name;
  a_line_number line_number;
  a_boolean     at_end_of_source;

  /* Ignore compiler-generated symbols and references whose position
     is the command line. */
  if (sym_ptr->header != error_symbol_header &&
      sym_ptr->kind != (a_symbol_kind)sk_extern_variable &&
      sym_ptr->kind != (a_symbol_kind)sk_extern_routine &&
      source_position->seq != 0) {
    /* The record written to the file is a text line that looks like

       symbol-id name X file-name line-number column-number

       where X is "D" for declaration,
                  "M" for modification,
                  "A" for address taken,
                  "R" for reference.
       The symbol-id is a unique number for the symbol, generated by 
       casting the symbol pointer to unsigned long.
    */
    switch (kind) {
      case srk_declaration:   code = 'D'; break;
      case srk_modification:  code = 'M'; break;
      case srk_address_taken: code = 'A'; break;
      case srk_reference:     code = 'R'; break;
#if CHECKING
      default: internal_error("xrite_xref_entry: bad ref kind");
#endif /* CHECKING */
    }  /* switch */
    /* Convert the source position to file name/line number. */
    conv_seq_to_file_and_line(source_position->seq, &file_name, &full_name,
                              &line_number, &at_end_of_source);
    fprintf(f_xref_info, "%lu %s %c %s %lu %d\n",
                         (unsigned long)sym_ptr,
                         sym_ptr->header->identifier,
                         code,
                         file_name,
                         line_number,
                         source_position->column);
  }  /* if */
}  /* write_xref_entry */


void mark_declared(a_symbol_ptr      sym_ptr,
                   a_source_position *source_position,
                   a_boolean         save_as_decl_position)
/*
Indicate that the given symbol is declared at the given position.  If
save_as_decl_position is TRUE, the position is saved as the decl_position
for the symbol.
*/
{
  if (f_xref_info != NULL) {
    /* If writing cross-reference information, write an entry for this
       declaration. */
    write_xref_entry(srk_declaration, sym_ptr, source_position);
  }  /* if */
  /* Put the decl_position in the symbol. */
  if (save_as_decl_position) {
    copy_source_position(*source_position, sym_ptr->decl_position);
  }  /* if */
}  /* mark_declared */


void mark_symbol_referenced(a_symbol_reference_kind kind,
                            a_symbol_ptr            sym_ptr,
                            a_source_position       *source_position)
/*
Record a reference of the indicated kind to the indicated symbol.
Set the reference flag in the symbol entry.  Do not set the referenced
flag in the associated IL entry.
*/
{
  if (f_xref_info != NULL) {
    /* If writing cross-reference information, write an entry for this
       declaration. */
    write_xref_entry(kind, sym_ptr, source_position);
  }  /* if */
  /* Set the referenced flag in the symbol. */
  sym_ptr->referenced = TRUE;
}  /* mark_symbol_referenced. */


void reference_to_symbol(a_symbol_reference_kind kind,
                         a_symbol_ptr            sym_ptr,
                         a_source_position       *source_position)
/*
Record a reference of the indicated kind to the indicated symbol.
Set the reference flag in the symbol entry.  Also set the referenced
flag in the associated IL entry, if any.
*/
{
  a_source_correspondence *scptr;

  mark_symbol_referenced(kind, sym_ptr, source_position);
  /* Set the referenced flag in the associated intermediate language entry,
     if there is one.  Note that more than one symbol can point to the same
     IL entry.  Use the fact that all the IL tables begin with
     a_source_correspondence. */
  scptr = source_corresp_entry_for_symbol(sym_ptr);
  if (scptr != NULL) scptr->referenced = TRUE;
}  /* reference_to_symbol */


void mark_referenced(a_symbol_ptr      sym_ptr,
                     a_source_position *source_position)
/*
Indicate that the given symbol is referenced at the given position.
*/
{
  reference_to_symbol(srk_reference, sym_ptr, source_position);
}  /* mark_referenced */


void set_source_corresp(a_source_correspondence *sc,
                        a_symbol_ptr            sp)
/*
Set the given source correspondence to point to the given symbol.  The
scope for the symbol must still be active.
*/
{
  sc->assoc_info = (char *)sp;
  /* Note that the identifier name was allocated in the intermediate language
     memory area (see find_symbol); it can therefore be used without
     copying. */
  sc->name = sp->header->identifier;
  copy_source_position(sp->decl_position, sc->decl_position);
  /* Clear the referenced flag.  It was set to TRUE in
     set_default_source_corresp, so that unassociated entities will
     all have the referenced flag set.  Here it's cleared, now that we
     know this is an entity with some corresponding entity in the
     source program.  The flag will later be set to TRUE again if
     there is an actual reference. */
  sc->referenced = FALSE;
}  /* set_source_corresp */


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


#if DEBUG
unsigned long show_symbol_space_used(void)
/*
Display and return the amount of memory used for symbol-related entries,
for space tracking purposes.
*/
{
  unsigned long num, size, total, grand_total = 0;

  fprintf(f_debug, "\nSymbol table use:\n");
  fprintf(f_debug, "%25s %8s %8s %8s\n", "Table", "Number", "Each", "Total");

#define write_one(name, counter, type)                                \
{ num = counter; size = sizeof(type); total = num*size;               \
  fprintf(f_debug, "%25s %8lu %8lu %8lu\n", name, num, size, total);  \
  grand_total += total;                                               \
}  /* write_one */

  write_one("symbol", num_symbols_allocated, a_symbol);
  write_one("symbol header", num_symbol_headers_allocated, a_symbol_header);
  write_one("conversion header", num_conversion_headers_allocated,
            a_conversion_header);
  write_one("Name strings", symbol_name_string_space, char);
  write_one("extern symbol descr", num_extern_symbol_descrs_allocated,
            an_extern_symbol_descr);
  write_one("extern type fixup", num_extern_type_fixups_allocated,
            an_extern_type_fixup);
  write_one("class symbol supplements", num_class_symbol_supplements_allocated,
            a_class_symbol_supplement);
  write_one("conversion list entry", num_conversion_list_entries_allocated,
            a_conversion_list_entry);
  write_one("projection symbol descr", num_projection_descrs_allocated,
            a_projection_descr);

  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total", "", "", grand_total);

  /* Print some symbol table performance statistics. */
  fputc('\n', f_debug);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Percent of buckets used", "", "",
                   (100 * num_used_symbol_buckets) / SYMBOL_TABLE_SIZE);
  if (num_used_symbol_buckets != 0) {
    fprintf(f_debug, "%25s %8s %8s %8.2f\n", "Avg non-empty bucket len","", "",
                     (double)num_symbol_headers_in_hash_table /
                     (double)num_used_symbol_buckets);
  }  /* if */
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Number of searches", "", "",
                                         num_searches_for_symbols);
  if (num_searches_for_symbols != 0) {
    fprintf(f_debug, "%25s %8s %8s %8.2f\n", "Avg compares/search", "", "",
                     (double)num_compares_for_symbols /
                     (double)num_searches_for_symbols);
  }  /* if */
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Number of fast id lookups", "", "",
                                         num_fast_id_lookups);
  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Number of slow id lookups", "", "",
                                         num_slow_id_lookups);

  return grand_total;
#undef write_one
}  /* show_symbol_space_used */
#endif /* DEBUG */


void sym_tbl_init(void)
/*
Initialize static variables related to the symbol table.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
(The name of this routine is sym_tbl_init rather than symbol_tbl_init
to avoid an 8-character external name clash with symbol_table.)
*/
{
  a_name_space_kind tag_name_space;

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
  /* Note that the class members are nsk_other rather than some other
     kind, which works because the members are on the inactive list
     once the class definition is ended.  Therefore, they won't be
     found inadvertently. */
  name_space_for_symbol_kind[(int)sk_field]               = nsk_other;
  name_space_for_symbol_kind[(int)sk_static_data_member]  = nsk_other;
  name_space_for_symbol_kind[(int)sk_member_function]     = nsk_other;
  name_space_for_symbol_kind[(int)sk_routine]             = nsk_other;
  name_space_for_symbol_kind[(int)sk_label]               = nsk_label;
  name_space_for_symbol_kind[(int)sk_undefined]           = nsk_other;
  name_space_for_symbol_kind[(int)sk_extern_variable]     = nsk_extern;
  name_space_for_symbol_kind[(int)sk_extern_routine]      = nsk_extern;
  name_space_for_symbol_kind[(int)sk_projection]          = nsk_other;
  name_space_for_symbol_kind[(int)sk_overloaded_function] = nsk_other;
#if CHECKING
  /* "undefined" and "routine" must be in the same name space.  See
      decl_default_function. */
  if (name_space_for_symbol_kind[(int)sk_undefined] !=
      name_space_for_symbol_kind[(int)sk_routine]) {
    internal_error(
   "symbol_table_init: name space for undefined and routine must be the same");
  }  /* if */
#endif /* CHECKING */
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
  next_scope_number = FILE_SCOPE_NUMBER;

  /* Static variables in symbol_tbl.c: */
  /* size_scope_stack is not per-file and should not be reset. */
  /* ident_buffer and size_ident_buffer are not per-file and should not
     be reset. */
  error_symbol_header = NULL;
  unnamed_class_symbol_header = NULL;
  num_classes_on_scope_stack = 0;
  class_currently_inside_of = NULL;
  curr_class_befriending_classes = NULL;
  curr_function_befriending_classes = NULL;
  /* Initialize the conversion header list. */
  conversion_header_list = NULL;
#if DEBUG
  num_symbols_allocated                  = 0;
  num_symbol_headers_allocated           = 0;
  num_symbol_headers_in_hash_table       = 0;
  num_conversion_headers_allocated       = 0;
  symbol_name_string_space               = 0;
  num_class_symbol_supplements_allocated = 0;
  num_conversion_list_entries_allocated  = 0;
  num_extern_symbol_descrs_allocated     = 0;
  num_extern_type_fixups_allocated       = 0;
  num_projection_descrs_allocated        = 0;
  num_used_symbol_buckets                = 0;
  num_searches_for_symbols               = 0;
  num_compares_for_symbols               = 0;
  num_fast_id_lookups                    = 0;
  num_slow_id_lookups                    = 0;
#if CHECKING
  /* Check that the table of symbol kind names is correctly initialized.
     This guards against someone changing the enumeration and forgetting to
     update db_sym_names. */

  if (db_sym_names[(int)sk_last] == NULL ||
      strcmp(db_sym_names[(int)sk_last], "last") != 0) {
    internal_error("sym_tbl_init: incorrect initialization of db_sym_names");
  }  /* if */
#endif /* CHECKING */
#endif /* DEBUG */
}  /* sym_tbl_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
