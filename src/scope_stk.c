/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

scope_stk.c - Management of the scope stack and related routines.

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
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

/*
Variables and constants related to the scope_stack:
*/
#define SCOPE_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to scope_stack each
			   time it is reallocated; also the initial
			   allocation. */

#if DEBUG
#if DO_IL_LOWERING
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_string_literal_table_entries_allocated,
		num_string_literal_tables_allocated;
#endif /* DO_IL_LOWERING */

int db_scope_kind(a_scope_kind sck)
/*
Put out a scope kind name (for debugging).
*/
{
  char	*s;

  switch (sck) {
    case sck_file:                   s = "file";                     break;
    case sck_namespace:              s = "namespace";                break;
    case sck_namespace_extension:    s = "namespace extension";      break;
    case sck_namespace_reactivation: s = "namespace reactivation";   break;
    case sck_func_prototype:         s = "function prototype";       break;
    case sck_block:                  s = "block";                    break;
    case sck_class_struct_union:     s = "class/struct/union";       break;
    case sck_class_reactivation:     s = "class reactivation";       break;
    case sck_function:               s = "function";                 break;
    case sck_template_declaration:   s = "template declaration";     break;
    case sck_template_instantiation: s = "template instantiation";   break;
    case sck_pragma:		     s = "pragma";		     break;
    case sck_function_access:	     s = "function access";	     break;
    case sck_condition:              s = "condition";                break;
    default:                         s = "***UNKNOWN SCOPE KIND***"; break;
  }  /* switch */
  fputs(s, f_debug);
  return strlen(s);
}  /* db_scope_kind */


void db_scope_stack_entry_at_depth(a_scope_depth  depth)
/*
Display identifying information for the scope stack entry at the indicated
depth in the scope stack, for debugging purposes.
*/
{
  a_scope_stack_entry_ptr  scope_stack_ptr;

  if (depth > depth_scope_stack || depth <= NO_SCOPE_DEPTH) {
    fputs("***BAD SCOPE DEPTH***", f_debug);
  } else {
    scope_stack_ptr = &scope_stack[depth];
    if (scope_stack_ptr->il_scope == NULL) {
      (void)db_scope_kind(scope_stack_ptr->kind);
      fprintf(f_debug, " scope %d", (int)scope_stack_ptr->number);
    } else {
      db_scope(scope_stack_ptr->il_scope);
    }  /* if */
  }  /* if */
}  /* db_scope_stack_entry_at_depth */


void db_scope_stack_entry(a_scope_stack_entry_ptr ssep)
/*
Display one scope stack entry.
*/
{
  int                      len;

  fprintf(f_debug, "%s%3ld %3d ",
          (ssep == &scope_stack[decl_scope_level]) ? "**" : "  ",
          (long)ssep->number, (int)scope_depth_of(ssep));
  len = db_scope_kind(ssep->kind);
  fprintf(f_debug, "%-*s", 25-len, "");
  fprintf(f_debug, "prev=%3d ", ssep->previous_scope);
  switch (ssep->kind) {
    case sck_function:
      if (ssep->il_scope == NULL) {
        fprintf(f_debug, "null IL scope");
      } else {
        db_name_full(&ssep->il_scope->variant.routine.ptr->source_corresp,
                     iek_routine);
      }  /* if */
      break;
    case sck_file:
      break;
    case sck_block:
      if (ssep->il_scope == NULL) {
        fprintf(f_debug, "null IL scope");
      }  /* if */
      break;
    case sck_namespace:
    case sck_namespace_extension:
    case sck_namespace_reactivation:
      if (ssep->il_scope == NULL) {
        fprintf(f_debug, "null IL scope");
      } else {
        a_namespace_ptr  nsp = ssep->il_scope->variant.assoc_namespace;
        if (nsp == NULL) {
          fprintf(f_debug, "null assoc_namespace");
        } else {
          db_name_full(&nsp->source_corresp, iek_namespace);
        }  /* if */
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
        fprintf(f_debug, "<%s> ",
                symbol_kind_names[(int)ssep->template_sym->kind]);
        if (ssep->assoc_type != NULL) {
          db_type_name(ssep->assoc_type);
        } else {
          db_name(source_corresp_entry_for_symbol(ssep->template_sym));
        }  /* if */
      }  /* if */
      break;
    case sck_template_declaration:
    case sck_func_prototype:
    default:;
  }  /* switch */
  fputs("\n", f_debug);
}  /* db_scope_stack_entry */


void db_scope_stack(void)
/*
Dump the entire scope stack (for debugging).
*/
{
  if (depth_scope_stack == NO_SCOPE_DEPTH) {
    fputs("Scope stack is empty.\n", f_debug);
  } else {
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

    for (; ssep != NULL;
         ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
      db_scope_stack_entry(ssep);
    }  /* for */
  }  /* if */
}  /* db_scope_stack */


void db_top_of_scope_stack(int entries)
/*
Dump the top "entries" of the scope stack (for debugging).
*/
{
  if (depth_scope_stack == NO_SCOPE_DEPTH) {
    fputs("Scope stack is empty.\n", f_debug);
  } else {
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

    for (; ssep != NULL;
         ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
      db_scope_stack_entry(ssep);
      if (--entries == 0) break;
    }  /* for */
  }  /* if */
}  /* db_top_of_scope_stack */

#if EXTRA_SOURCE_POSITIONS_IN_IL

static void db_source_range(a_source_range  *range)
/*
Dump the specified range.
*/
{
  fprintf(f_debug, "%4lu/%-3lu -- %4lu/%-3lu",
          (unsigned long)range->start.seq,
          (unsigned long)range->start.column,
          (unsigned long)range->end.seq,
          (unsigned long)range->end.column);
}  /* db_source_range */


void db_decl_pos_info(a_symbol_ptr  sym)
/*
Dump decl-pos information for the specified symbol (for debugging).
*/
{
  a_decl_position_supplement_ptr  dpsp;
  a_source_correspondence         *scp;
  a_boolean                       is_enumerator;

  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    sym = sym->variant.overloaded_function.symbols;
    for (; sym != NULL; sym = sym->next) db_decl_pos_info(sym);
  } else if (!sym->is_error && sym->decl_position.seq != 0) {
    scp = source_corresp_entry_for_symbol(sym);
    if (scp != NULL) {
      fprintf(f_debug, " ");
      db_symbol_name(sym);
      fprintf(f_debug, " <%s>, decl_position: %lu/%lu",
                       symbol_kind_names[(int)sym->kind],
                       (unsigned long)scp->decl_position.seq,
                       (unsigned long)scp->decl_position.column);
      dpsp = scp->decl_pos_info;
      if (dpsp == NULL) {
        fputs(", no decl-pos info\n", f_debug);
      } else {
        is_enumerator = (sym->kind == (a_symbol_kind)sk_constant &&
                         is_enum_constant(sym->variant.constant));
        fputc('\n', f_debug);
        if (!is_enumerator) {
          if (dpsp->specifiers_range.start.seq != 0 ||
              dpsp->specifiers_range.end.seq != 0) {
            fprintf(f_debug, "    specifiers range:  ");
            db_source_range(&dpsp->specifiers_range);
            fputc('\n', f_debug);
          }  /* if */
          if (dpsp->variant.declarator_range.start.seq != 0 ||
              dpsp->variant.declarator_range.end.seq != 0) {
            fprintf(f_debug, "    declarator range:  ");
            db_source_range(&dpsp->variant.declarator_range);
            fputc('\n', f_debug);
          }  /* if */
        }  /* if */
        if (dpsp->identifier_range.start.seq != 0 ||
            dpsp->identifier_range.end.seq != 0) {
          fprintf(f_debug, "    identifier range:  ");
          db_source_range(&dpsp->identifier_range);
          fputc('\n', f_debug);
        }  /* if */
        if (is_enumerator &&
            (dpsp->variant.enum_value_range.start.seq != 0 ||
             dpsp->variant.enum_value_range.end.seq != 0)) {
          fprintf(f_debug, "    enum value range:  ");
          db_source_range(&dpsp->variant.enum_value_range);
          fputc('\n', f_debug);
        } else if (sym->kind == (a_symbol_kind)sk_variable ||
                   sym->kind == (a_symbol_kind)sk_static_data_member) {
          a_variable_ptr  vp = sym->variant.variable.ptr;
          if (vp->initializer_range.start.seq != 0 ||
              vp->initializer_range.end.seq != 0) {
            fprintf(f_debug, "    initializer range: ");
            db_source_range(&vp->initializer_range);
            fputc('\n', f_debug);
          }  /* if */
        } else if (!C_mode() && is_class_struct_union_symbol(sym)) {
          a_base_class_ptr  bcp = base_classes_of(type_symbol_type(sym));
          for (; bcp != NULL; bcp = bcp->next) {
            if (bcp->base_specifier_range.start.seq != 0 ||
                bcp->base_specifier_range.end.seq != 0) {
              fprintf(f_debug, "    base class \"");
              db_type_name(bcp->type);
              fprintf(f_debug, "\", decl_position: %lu/%lu\n",
                      (unsigned long)bcp->decl_position.seq,
                      (unsigned long)bcp->decl_position.column);
              fprintf(f_debug, "      specifier range: ");
              db_source_range(&bcp->base_specifier_range);
              fputc('\n', f_debug);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_function_template) {
      /* Display decl-pos info for each explicit specialization of the
         function template. */
      a_template_instance_ptr  tip;
      a_symbol_ptr             instance_sym;

      tip = sym->variant.template_info->variant.function.instantiations;
      for (; tip != NULL; tip = tip->next) {
        instance_sym = tip->instance_sym;
        if (instance_sym->kind == (a_symbol_kind)sk_routine ||
            instance_sym->kind == (a_symbol_kind)sk_member_function) {
          if (instance_sym->variant.routine.ptr->is_specialized) {
            db_decl_pos_info(instance_sym);
          }  /* if */
        }  /* if */
      }  /* for */
    } else if (sym->kind == (a_symbol_kind)sk_class_template) {
      /* Display decl-pos info for each explicit specialization of the
         class template. */
      a_symbol_ptr  instance_sym;

      for (instance_sym = sym->variant.template_info->
                                  variant.class_template.instantiations;
           instance_sym != NULL;
           instance_sym = next_instance_sym(instance_sym)) {
        if (instance_sym->variant.class_struct_union.type->
                             variant.class_struct_union.is_specialized) {
          db_decl_pos_info(instance_sym);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* db_decl_pos_info */


static void db_decl_pos_info_for_scope(a_scope_ptr            scope_ptr,
                                       a_scope_pointers_block *pointers_block)
/*
Dump decl-pos information for each symbol in the specified scope (for
debugging).
*/
{
  a_symbol_ptr  sym = pointers_block->symbols;

  if (sym != NULL) {
    fprintf(f_debug, "decl-pos info for ");
    db_scope(scope_ptr);
    fprintf(f_debug, "\n");
    for (; sym != NULL; sym = sym->next_in_scope) db_decl_pos_info(sym);
  }  /* if */
}  /* db_decl_pos_info_for_scope */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* DEBUG */

#if IA64_ABI && NEED_NAME_MANGLING

#define LOCAL_NAME_COLLISION_TABLE_SIZE 16

/*
A hash table type to detect name collisions between declarations in function
scope.
*/
typedef union a_collision_table {
  a_symbol_list_entry_ptr
		buckets[LOCAL_NAME_COLLISION_TABLE_SIZE];
			/* The actual table of lists. */
  a_collision_table_ptr
		next_avail;
			/* Pointer to the next available free list. */
} a_collision_table;


/*
Pointer to a list of available collision tables.
*/
static a_collision_table_ptr avail_collision_tables;


static void initialize_local_name_collision_table(a_scope_stack_entry_ptr ssep)
/*
Allocate and initialize a local name collision table for the given scope stack
entry.
*/
{
  if (avail_collision_tables == NULL) {
    ssep->local_name_collision_table = (a_collision_table_ptr)
                                           alloc_fe(sizeof(a_collision_table));
  } else {
    ssep->local_name_collision_table = avail_collision_tables;
    avail_collision_tables = avail_collision_tables->next_avail;
  }  /* if */
  memzero((char *)ssep->local_name_collision_table->buckets,
          size_t_arg(
            sizeof(a_symbol_list_entry_ptr[LOCAL_NAME_COLLISION_TABLE_SIZE])));
}  /* initialize_local_name_collision_table */


static void free_local_name_collision_table(a_scope_stack_entry_ptr ssep)
/*
Release the storage allocated for the name collision table associated with
the given scope stack entry.
*/
{
  int  k;
  a_symbol_list_entry_ptr  *sleps = ssep->local_name_collision_table->buckets;

  /* First free up the lists. */
  for (k = 0; k<LOCAL_NAME_COLLISION_TABLE_SIZE; ++k) {
    if (sleps[k] != NULL) {
      free_list_of_symbol_list_entries(sleps[k]);
    }  /* if */
  }  /* for */
  /* Return the table entry to the available list. */
  ssep->local_name_collision_table->next_avail = avail_collision_tables;
  avail_collision_tables = ssep->local_name_collision_table;
  ssep->local_name_collision_table = NULL;
}  /* free_local_name_collision_table */


void compute_name_collision_discriminator(a_symbol_ptr  sym)
/*
Look in the name collision table associated with current function scope for a
symbol that has the same name (i.e., header) as the given symbol sym.  If
there is one, the current symbol is assigned a discriminator value one higher
than that of the symbol found, and it replaces that symbol in the table.
Otherwise the discriminator value of sym remain zero, and the symbol is added
to the table.  This information is used to generate distinct mangled names of
function-local entities in the IA-64 ABI.
*/
{
  unsigned                 hash_index;
  a_scope_stack_entry_ptr  ssep;
  a_symbol_list_entry_ptr  sep;
  a_symbol_header_ptr      header = sym->header;

  check_assertion(depth_innermost_function_scope != NO_SCOPE_DEPTH);
  ssep = &scope_stack[depth_innermost_function_scope];
  if (ssep->local_name_collision_table == NULL) {
    initialize_local_name_collision_table(ssep);
  }  /* if */
  /* Symbols corresponding to identical names have identical symbol headers.
     So we use the symbol header pointer value as a basis for a hash value.
     The three least significant bits are discarded because they are possibly
     always zero due to alignment requirements. */
  hash_index = (((unsigned)header) >> 3) % LOCAL_NAME_COLLISION_TABLE_SIZE;
  sep = ssep->local_name_collision_table->buckets[hash_index];
  for (; sep != NULL; sep = sep->next) {
    if (sep->symbol->header == header &&
        (sep->symbol->kind == sym->kind ||
         (is_tag_symbol_kind(sep->symbol->kind) &&
          is_tag_symbol_kind(sym->kind)))) {
      /* A previous declaration does collide with the new one. */
      switch (sym->kind) {
        case sk_variable:
          sym->variant.variable.discriminator =
                          sep->symbol->variant.variable.discriminator+1;
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
          /* Note that enumerations and class types use the same numbering. */
          if (sep->symbol->kind == (a_symbol_kind)sk_enum_tag) {
            sym->variant.class_struct_union.extra_info->discriminator =
                 sep->symbol->variant.enumeration.extra_info->discriminator+1;
          } else {
            sym->variant.class_struct_union.extra_info->discriminator =
                            sep->symbol->variant.class_struct_union.extra_info
                                       ->discriminator+1;
          }  /* if */
          break;
        case sk_enum_tag:
          /* Note that enumerations and class types use the same numbering. */
          if (sep->symbol->kind == (a_symbol_kind)sk_enum_tag) {
            sym->variant.enumeration.extra_info->discriminator =
                 sep->symbol->variant.enumeration.extra_info->discriminator+1;
          } else {
            sym->variant.enumeration.extra_info->discriminator =
                            sep->symbol->variant.class_struct_union.extra_info
                                       ->discriminator+1;
          }  /* if */
          break;
        case sk_type:
          sym->variant.type.discriminator =
                            sep->symbol->variant.type.discriminator+1;
          break;
        default:
          unexpected_condition();
      }  /* switch */
      sep->symbol = sym;
      break;
    }  /* if */
  }  /* for */
  if (sep == NULL) {
    /* There were no collisions.  Record this symbol in the collision table. */
    a_symbol_list_entry_ptr  new_entry = alloc_symbol_list_entry();
    new_entry->next = ssep->local_name_collision_table->buckets[hash_index];
    ssep->local_name_collision_table->buckets[hash_index] = new_entry;
    new_entry->symbol = sym;
  }  /* if */
}  /* compute_name_collision_discriminator */

#endif /* IA64_ABI && NEED_NAME_MANGLING */

#if DO_IL_LOWERING

#define STRING_LITERAL_TABLE_SIZE 31

/*
Entry used to construct a hash table of string literal constants.
*/
typedef struct a_string_literal_table_entry *a_string_literal_table_entry_ptr;
typedef struct a_string_literal_table_entry {
  a_string_literal_table_entry_ptr
		next;
			/* Pointer to the next entry in the bucket, or on
			   the available list. */
  a_constant_ptr
		constant;
			/* Pointer to the string literal constant represented
			   by this entry. */
  unsigned long	sequence_number;
			/* The sequence number assigned to this constant. */
} a_string_literal_table_entry;

/*
A hash table used to assign sequence numbers to each unique string literal
used within a function.
*/
typedef struct a_string_literal_table {
  a_string_literal_table_ptr
		next;	/* Pointer to the next available free list. */
  a_string_literal_table_entry_ptr
		buckets[STRING_LITERAL_TABLE_SIZE];
			/* A list of string literals used in the
			   function scope. */
} a_string_literal_table;


static a_string_literal_table_ptr avail_string_literal_tables;
			/* A list of string literal tables that have been
			   freed and are available for reuse. */

static a_string_literal_table_entry_ptr avail_string_literal_table_entries;
			/* A list of string literal table entries that have
			   been freed and are available for reuse. */


static void free_list_of_string_literal_table_entries(
				a_string_literal_table_entry_ptr	sltep)
/*
Free the list of string literal table entries pointed to by "sltep" by
placing them on the available list.   sltep may be NULL, in which case
nothing is done.
*/
{
  a_string_literal_table_entry_ptr	sltep_tail;
  if (sltep != NULL) {
    /* Find the last entry on the list. */
    sltep_tail = sltep;
    while (sltep_tail->next != NULL) sltep_tail = sltep_tail->next;
    /* Add the current available list to the end of the list passed by the
       caller. */
    sltep_tail->next = avail_string_literal_table_entries;
    avail_string_literal_table_entries = sltep;
  }  /* if */
}  /* free_list_of_string_literal_table_entries */


static void initialize_string_literal_table(a_scope_stack_entry_ptr ssep)
/*
Allocate and initialize a string literal table for scope stack entry "ssep".
*/
{
  a_string_literal_table_ptr	sltp;

  if (avail_string_literal_tables == NULL) {
    sltp = alloc_fe_of_type(a_string_literal_table);
#if DEBUG
    num_string_literal_tables_allocated++;
#endif /* DEBUG */
  } else {
    sltp = avail_string_literal_tables;
    avail_string_literal_tables = avail_string_literal_tables->next;
  }  /* if */
  memzero((char *)sltp->buckets, size_t_arg(sizeof(sltp->buckets)));
  sltp->next = NULL;
  ssep->string_literal_table = sltp;
}  /* initialize_string_literal_table */


static void free_string_literal_table(a_scope_stack_entry_ptr ssep)
/*
Free the string literal table for the scope stack entry "ssep".
*/
{
  int					i;
  a_string_literal_table_ptr		sltp = ssep->string_literal_table;
  a_string_literal_table_entry_ptr	*buckets = sltp->buckets;

  /* Go through each bucket and free its entries. */
  for (i = 0;  i < STRING_LITERAL_TABLE_SIZE; ++i) {
    if (buckets[i] != NULL) {
      free_list_of_string_literal_table_entries(buckets[i]);
    }  /* if */
  }  /* for */
  /* Return the table to the available list. */
  sltp->next = avail_string_literal_tables;
  avail_string_literal_tables = sltp;
  ssep->string_literal_table = NULL;
}  /* free_string_literal_table */


void f_assign_string_literal_sequence_number(void)
/*
We have just completed scanning the token for a string literal, or have
retrieved such a token from a token cache.  The macro that calls this routine
has determined that we are inside of a function scope for which string
literal sequence numbers are required.  If the constant associated with
the string literal does not yet have a string literal sequence number,
assign one now.
*/
{
  a_constant_ptr		cp = &const_for_curr_token;
  a_scope_stack_entry_ptr	ssep;

  check_assertion(cp->kind == (a_constant_repr_kind)ck_string ||
                  cp->kind == (a_constant_repr_kind)ck_error);
  ssep = &scope_stack[depth_innermost_function_scope];
  /* If this is the first string literal in the function, create the hash
     table to be used. */
  if (ssep->string_literal_table == NULL) {
    initialize_string_literal_table(ssep);
  }  /* if */
  if (cp->kind == (a_constant_repr_kind)ck_error) {
    /* An error constant.  No action is needed. */
  } else if (cp->variant.string.sequence_number != 0) {
    /* A sequence number has already been assigned.  No action is needed. */
  } else {
    /* Determine whether we have already assigned a sequence number for
       this string literal by looking up the string in a hash table. */
    int					bucket_number;
    a_constant_hash_value		hash;
    a_string_literal_table_entry_ptr	sltep;
    a_string_literal_table_entry_ptr	*bucket;
    hash = hash_constant(cp);
    bucket_number = hash % STRING_LITERAL_TABLE_SIZE;
    bucket = &ssep->string_literal_table->buckets[bucket_number];
    for (sltep = *bucket; sltep != NULL; sltep = sltep->next) {
      if (eq_constants(cp, sltep->constant)) break;
    }  /* for */
    if (sltep == NULL) {
      /* No entry was found.  Create one now. */
      sltep = alloc_fe_of_type(a_string_literal_table_entry);
      sltep->constant = alloc_constant((a_constant_repr_kind)ck_string);
      copy_constant(cp, sltep->constant);
      sltep->sequence_number = ++(ssep->string_literal_sequence_number);
      sltep->next = *bucket;
      *bucket = sltep;
#if DEBUG
      num_string_literal_table_entries_allocated++;
#endif /* DEBUG */
    }  /* if */
    cp->variant.string.sequence_number = sltep->sequence_number;
  }  /* if */
}  /* f_assign_string_literal_sequence_number */

#endif /* DO_IL_LOWERING */

a_scope_pointers_block *get_pointers_block_for_scope(a_scope_ptr scope)
/*
Return the pointers block for the indicated scope, if there is one,
or NULL otherwise.
*/
{
  a_scope_pointers_block *pointers_block = NULL;

  if (scope->kind == (a_scope_kind)sck_file) {
    if (scope == il_header.primary_scope) {
      /* The file scope of the current translation unit. */
      pointers_block = &curr_translation_unit->file_scope_pointers_block;
    } else {
      /* Presumably a file scope of another translation unit.  Look for it. */
      a_translation_unit_ptr tup;
      for (tup = translation_units; ; tup = tup->next) {
        check_assertion_str(tup != NULL,
                         "get_pointers_block_for_scope: file scope not found");
        if (tup->primary_scope == scope) {
          pointers_block = &tup->file_scope_pointers_block;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  } else if (scope->kind == (a_scope_kind)sck_namespace) {
    /* A namespace scope. */
    a_namespace_ptr nsp = scope->variant.assoc_namespace;
    pointers_block = &symbol_supplement_for_namespace(nsp)->pointers_block;
  } else {
    /* For scopes other than file and namespace, we can get the pointers
       block only if the scope is on the scope stack. */
    a_scope_depth depth = scope->depth_in_scope_stack;

    if (depth != NO_SCOPE_DEPTH) {
      check_assertion_str(trans_unit_for_scope[scope->number] ==
                                                         curr_translation_unit,
                          "get_pointers_block_for_scope: wrong trans unit");
      pointers_block = assoc_pointers_block_of(&scope_stack[depth]);
    }  /* if */
  }  /* if */
  return pointers_block;
}  /* get_pointers_block_for_scope */


a_scope_depth scope_depth_of_symbol(a_symbol_ptr  sym,
                                    a_boolean     *is_local_to_function)
/*
Given a symbol with a decl_scope (which is a scope number), search the
scope stack for the scope stack entry that corresponds to it, and return
the depth.  Also return TRUE in *is_local_to_function if the declaration
is in within a function body.
*/
{
  a_scope_depth  scope_depth;

  if (sym->decl_scope == file_scope_number) {
    /* Leave the is_local_to_function flag FALSE. */
    scope_depth = DEPTH_OF_FILE_SCOPE;
  } else if (sym->decl_scope == NO_SCOPE_NUMBER) {
    /* Leave the is_local_to_function flag FALSE. */
    /* Some entities (e.g., macros) have no decl_scope number. */
    scope_depth = NO_SCOPE_DEPTH;
  } else if (sym->decl_scope == scope_stack[decl_scope_level].number) {
    /* The normal case is when the current decl_scope_level corresponds to
       what's in the symbol.  Use the global variables. */
    if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
        inside_local_class) {
      *is_local_to_function = TRUE;
    }  /* if */
    scope_depth = decl_scope_level;
  } else if (is_template_instance_class_symbol(sym)) {
    /* Template classes can be created at arbitrary times and so the
       scope stack cannot be used to determine the scope depth. */
    scope_depth = NO_SCOPE_DEPTH;
  } else {
    /* In certain unusual cases (e.g., when an entity is first seen in a
       friend declaration) it is necessary to compute the scope depth by
       running through the scope stack. */
    for (scope_depth = depth_scope_stack; ; --scope_depth) {
      a_scope_kind	kind;
      if (scope_depth < DEPTH_OF_FILE_SCOPE) {
        scope_depth = NO_SCOPE_DEPTH;
        break;
      }  /* if */
      kind = scope_stack[scope_depth].kind;
      if (kind == (a_scope_kind)sck_class_reactivation ||
          kind == (a_scope_kind)sck_namespace_reactivation) {
        /* Ignore class and namespace reactivations. */
        continue;
      } else if (scope_stack[scope_depth].number == sym->decl_scope) {
        /* This is the scope stack entry corresponding to the declaration
           scope number, where relevant characteristics of the scope are
           recorded. */
        if (scope_stack[scope_depth].depth_innermost_function_scope !=
                                                           NO_SCOPE_DEPTH ||
            scope_stack[scope_depth].inside_local_class) {
          *is_local_to_function = TRUE;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return scope_depth;
}  /* scope_depth_of_symbol */


a_boolean namespace_is_enclosed_by_scope(a_symbol_ptr            sym,
                                         a_scope_stack_entry_ptr ssep)
/*
Determine whether the namespace in which sym is defined is enclosed
within the scope specified by ssep.  Return TRUE if it is, FALSE otherwise.
*/
{
  a_boolean	result = FALSE;

  if (ssep->kind == (a_scope_kind)sck_file) {
    /* Everything is enclosed within the file scope. */
    result = TRUE;
  } else if (ssep->kind != (a_scope_kind)sck_namespace &&
             ssep->kind != (a_scope_kind)sck_namespace_extension) {
    /* This is not a namespace scope.  A namespace cannot be enclosed
       within. */
  } else {
    a_namespace_ptr	nsp = parent_namespace_for_symbol(sym);
    if (nsp == NULL) {
      /* The symbol has no associated namespace, and so, is not enclosed
         within the current namespace. */
    } else {
      /* The symbol has a namespace.  See if its namespace, or one of
         its parent namespaces, matches the current namespace. */
      a_namespace_ptr     curr_nsp;
      curr_nsp = ssep->il_scope->variant.assoc_namespace;
      while (curr_nsp != nsp && nsp != NULL) {
        nsp = nsp->source_corresp.parent.namespace_ptr;
      }  /* while */
      if (nsp != NULL) result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* namespace_is_enclosed_by_scope */


static an_active_using_directive_ptr alloc_active_using_directive(void)
/*
Allocate a new active using directive entry, initialize its fields, and
return a pointer to the new entry. Reuse a freed entry if possible.
*/
{
  an_active_using_directive_ptr  audp;

  if (avail_active_using_directives != NULL) {
    /* Reuse a freed entry. */
    audp = avail_active_using_directives;
    avail_active_using_directives = avail_active_using_directives->next;
  } else {
    /* Allocate a new entry. */
    audp = (an_active_using_directive_ptr)
                                  alloc_fe(sizeof(an_active_using_directive));
#if DEBUG
    num_active_using_directives_allocated++;
#endif /* DEBUG */
  }  /* if */
  audp->entry = NULL;
  audp->next  = NULL;
  audp->scope_depth_at_which_using_directive_applies = NO_SCOPE_DEPTH;
  audp->namespace_supplement = NULL;
  audp->effective_decl_seq = 0;
  return audp;
}  /* alloc_active_using_directive */


static a_scope_depth
determine_scope_at_which_using_directive_applies(a_symbol_ptr            sym,
                                                 a_scope_stack_entry_ptr ssep)
/*
Given a symbol that points to a namespace (sym), find the scope at which
names in that namespace should be visible as a consequence of a using
directive.  This is done by going back through the scope stack, starting
with ssep, looking for namespace scopes.
*/
{
  a_scope_depth	depth;
  /* Compute the scope depth associated with the scope stack entry pointer
     passed by the caller. */
  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    depth = scope_depth_of(ssep);
    if (ssep->kind == (a_scope_kind)sck_file ||
        ssep->kind == (a_scope_kind)sck_namespace ||
        ssep->kind == (a_scope_kind)sck_namespace_extension ||
        ssep->kind == (a_scope_kind)sck_namespace_reactivation) {
      /* This is a namespace scope (the file scope is treated as a
         namespace scope).  See if it encloses the namespace from
         the using directive. */
      if (namespace_is_enclosed_by_scope(sym, ssep)) break;
    }  /* if */
  }  /* for */
  return depth;
}  /* determine_scope_at_which_using_directive_applies */


/* Forward declaration. */
static void add_active_using_directive_to_scope(
				a_using_decl_ptr	udp,
				a_scope_stack_entry_ptr	ssep,
				a_decl_sequence_number	effective_decl_seq);

static void add_active_using_directives_for_scope(
				a_scope_ptr		scope,
				a_scope_stack_entry_ptr	ssep,
				a_decl_sequence_number	parent_decl_seq)
/*
Create active using directive entries for any using directives present
in the specified scope (which is either a namespace scope or the file scope).
If scope is a namespace scope, and was made visible by a using-directive,
parent_decl_seq is the declaration sequence number of the using-directive
that made it visible.
*/
{
  a_using_decl_ptr  udp = scope->using_decls;

  while (udp != NULL) {
    if (udp->is_using_directive) {
      a_decl_sequence_number	effective_decl_seq;
      /* A using-directive.  Use the higher of the parent's declaration
         sequence number or the one associated with the using directive
         now being processed.  If the namespace that nominated this namespace
         should not be visible then neither should this one. */
      effective_decl_seq = parent_decl_seq > udp->decl_sequence_number ?
                                   parent_decl_seq : udp->decl_sequence_number;
      add_active_using_directive_to_scope(udp, ssep, effective_decl_seq);
    }  /* if */
    udp = udp->next;
  }  /* while */
}  /* add_active_using_directives_for_scope */


static void add_active_using_directive_to_scope(
				a_using_decl_ptr	udp,
				a_scope_stack_entry_ptr	ssep,
				a_decl_sequence_number	effective_decl_seq)
/*
Allocate a new active using directive entry, initialize its fields, and
link it into a list of active using directives for the current scope.
Reuse a freed entry if possible.  If the specified namespace is already
on the list for the scope, a new entry is not added.  effective_decl_seq
is the declaration sequence point at which the using-directive is
effective.  This is used during template instantiations to ignore
using-directives specified after the point of definition of the template.
*/
{
  an_active_using_directive_ptr  	audp;
  a_namespace_ptr		 	nsp;
  a_symbol_ptr			 	ns_sym;
  a_scope_depth			 	new_depth;
  a_namespace_symbol_supplement_ptr	nssp;
  a_scope_depth				curr_depth = scope_depth_of(ssep);
  a_boolean				add_to_list = FALSE;

  check_assertion(udp->entity.kind == (a_byte_il_entry_kind)iek_namespace);
  /* Get a pointer to the namespace to be used. */
  nsp = skip_namespace_aliases((a_namespace_ptr)udp->entity.ptr);
  ns_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
  nssp = ns_sym->variant.namespace_info.extra_info;
  /* Determine the depth at which this using directive applies. */
  new_depth = determine_scope_at_which_using_directive_applies(ns_sym, ssep);
  /* Determine whether this namespace is already on the active using list
     for this scope. */
  if (curr_depth == nssp->depth_innermost_active_using_directive) {
    /* The namespace is already on the active using list for this scope. */
  } else if (nssp->depth_innermost_active_using_directive == NO_SCOPE_DEPTH) {
    /* The namespace is not on any list, so we know we have to add it to this
       one. */
    add_to_list = TRUE;
  } else {
    /* We can't quickly tell whether or not the namespace is on the list
       for this scope.  Look through the list to find out. */
    audp = ssep->active_using_directives;
    for (; audp != NULL; audp = audp->next) {
      a_namespace_ptr  nsp2 = (a_namespace_ptr)audp->entry->entity.ptr;
      if (skip_namespace_aliases(nsp2) == nsp) break;
    }  /* for */
    add_to_list = (audp == NULL);
  }  /* if */
  if (add_to_list) {
    /* Add the using directive to the active list for this scope. */
    if (new_depth > nssp->scope_depth_at_which_using_directive_applies) {
      /* Only set the scope depth if it is greated than the existing value.
         When entries are added to previous scopes, we don't want to
         reset this value. */
      nssp->scope_depth_at_which_using_directive_applies = new_depth;
    }  /* if */
    /* Record the depth of the innermost scope for which this namespace
       is on the scopes active using list.  This is done so that it
       is possible to quickly determine whether a namespace is on the
       active using list of the innermost scope. */
    if (curr_depth > nssp->depth_innermost_active_using_directive) {
      nssp->depth_innermost_active_using_directive = curr_depth;
    }  /* if */
    audp = alloc_active_using_directive();
    audp->entry = udp;
    audp->namespace_supplement = nssp;
    audp->next = ssep->active_using_directives;
    audp->scope_depth_at_which_using_directive_applies = new_depth;
    audp->effective_decl_seq = effective_decl_seq;
    ssep->active_using_directives = audp;
    /* Set a flag in the scope at which this using directive applies that
       indicates that the using directive processing must be done for
       that scope. */
    scope_stack[new_depth].using_directives_apply = TRUE;
    /* Add active using directives for the namespaces that should be
       visible because of the transitivity of using directives. */
    add_active_using_directives_for_scope(nsp->variant.assoc_scope, ssep,
                                          effective_decl_seq);
    /* Now that a using directive is active, inactive symbols may be
       visible. */
    scope_stack[depth_scope_stack].inactive_symbols_may_be_visible = TRUE;
  }  /* if */
}  /* add_active_using_directive_to_scope */


void add_active_using_directive(a_using_decl_ptr udp,
				a_scope_depth    depth)
/*
Add a new active using directive entry to the scope specified by depth.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth];

  add_active_using_directive_to_scope(udp, ssep,
                            (a_decl_sequence_number)udp->decl_sequence_number);
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension) {
    a_namespace_ptr	namespace_added_to;
    /* When a using directive is added to a namespace scope, we need to
       go through any previous scope stack entries to see if they reference
       the enclosing namespace.  If so, the new using directive, and any
       new namespaces transitively referenced by the new using directive,
       must be added to the previous scope stack entries. */
    /* Note: This code does not need to deal with scopes being skipped
       as a consequence of instantiation scopes because there is no way that
       a using directive can be added to a namespace scope as a consequence
       of a template instantiation. */
    namespace_added_to = ssep->il_scope->variant.assoc_namespace;
    namespace_added_to = skip_namespace_aliases(namespace_added_to);
    for (;; ssep--) {
      an_active_using_directive_ptr	audp;
      a_namespace_ptr                   audp_nsp;

      /* Look for namespace_added_to on the list of active using directives for
         this scope. */
      audp = ssep->active_using_directives;
      for (; audp != NULL; audp = audp->next) {
        audp_nsp = (a_namespace_ptr)audp->entry->entity.ptr;
        if (skip_namespace_aliases(audp_nsp) == namespace_added_to) {
          /* The enclosing namespace is on the list.  Add the using directive
             to this scope. */
          add_active_using_directive_to_scope(udp, ssep, decl_seq_counter);
          break;
        }  /* if */
      }  /* for */
      if (ssep->kind == (a_scope_kind)sck_file) break;
    }  /* for */
  }  /* if */
}  /* add_active_using_directive */


static
void free_active_using_directive_list(an_active_using_directive_ptr audp)
/*
Free the specified list of active using directives to the available list.
*/
{
  an_active_using_directive_ptr	last_audp = audp;

  /* Find the end of the list. */
  while (last_audp->next != NULL) last_audp = last_audp->next;
  /* Add the current available list to the end of this one.  Set the
     pointer to the start of the available list to the start of the
     list passed by the caller. */
  last_audp->next = avail_active_using_directives;
  avail_active_using_directives = audp;
}  /* free_active_using_directive_list */


/*
The name-linkage stack is used to save and restore the name-linkage state
for the currently active scopes.  a_name_linkage_stack_entry is the type of
each entry on the name-linkage stack.
*/
typedef struct a_name_linkage_stack_entry *a_name_linkage_stack_entry_ptr;
typedef struct a_name_linkage_stack_entry {
  a_name_linkage_stack_entry_ptr
		next;
			/* Next in name-linkage stack or available list;
			   NULL for last entry. */
  a_name_linkage_kind
		saved_name_linkage;
			/* Saved value of default_name_linkage for the
			   current scope. */
  a_byte_boolean
		saved_name_linkage_is_explicit;
			/* Saved value of name_linkage_is_explicit for the
			   current scope. */
} a_name_linkage_stack_entry;


static a_name_linkage_stack_entry_ptr
		name_linkage_stack;
			/* Linked list of entries recording saved name-linkage
			   states from the currently active scopes. */

static a_name_linkage_stack_entry_ptr
		avail_name_linkage_stack_entries;
			/* Freed name-linkage-stack entries that are available
			   for reuse. */

void push_name_linkage(a_name_linkage_kind  kind)
/*
Set the name-linkage field in the scope stack to "kind" and save its current
value.  The value will be restored when pop_name_linkage is called.
*/
{
  a_name_linkage_stack_entry_ptr  nlsep;
  a_scope_stack_entry_ptr         ssep = &scope_stack[depth_scope_stack];

  /* Allocate a new name-linkage-stack entry if necessary. */
  if (avail_name_linkage_stack_entries == NULL) {
    nlsep = (a_name_linkage_stack_entry_ptr)alloc_fe(
                                   sizeof(a_name_linkage_stack_entry));
  } else {
    nlsep = avail_name_linkage_stack_entries;
    avail_name_linkage_stack_entries = nlsep->next;
  }  /* if */
  /* Save the values for the current scope. */
  nlsep->saved_name_linkage = ssep->default_name_linkage;
  nlsep->saved_name_linkage_is_explicit = ssep->name_linkage_is_explicit;
  /* Add the new entry to the name linkage stack. */
  nlsep->next = name_linkage_stack;
  name_linkage_stack = nlsep;
  /* Reset the values in the current scope. */
  ssep->default_name_linkage = kind;
  ssep->name_linkage_is_explicit = TRUE;
}  /* push_name_linkage */


void pop_name_linkage(void)
/*
Restore to the scope stack the name linkage state at the point of the
corresponding call to push_name_linkage.
*/
{
  a_name_linkage_stack_entry_ptr  nlsep = name_linkage_stack;
  a_scope_stack_entry_ptr         ssep = &scope_stack[depth_scope_stack];

  check_assertion(nlsep != NULL);
  /* Restore to the current scope the values saved in the name-linkage
     stack. */
  ssep->default_name_linkage = nlsep->saved_name_linkage;
  ssep->name_linkage_is_explicit = nlsep->saved_name_linkage_is_explicit;
  /* Pop the entry from the name linkage stack and return it to the
     available list. */
  name_linkage_stack = nlsep->next;
  nlsep->next = avail_name_linkage_stack_entries;
  avail_name_linkage_stack_entries = nlsep;
}  /* pop_name_linkage */


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

  if (is_injected_template_symbol(*sym)) {
    /* The symbol is the injected name of a class template.  Substitute
       the symbol of the associated class template. */
    *sym = class_template_for_injected_template_symbol(*sym);
  }  /* if */
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
          if (!is_instantiation_scope ||
              (microsoft_bugs && ssep->assoc_type != NULL)) {
            /* Normally, we look for a class or class reactivation scope
               for a class that is an instance of the template specified
               by "sym".  The Microsoft compiler also accepts a class name
               as being the "current instantiation" when referenced in the
               base class list of the class.  To emulated this, also check
               the class associated with a template instantiation scope. */
            a_symbol_ptr	template_sym;
            check_assertion_str(ssep->assoc_type != NULL,
				"ccsict: assoc_type is NULL");
            instance_sym = (a_symbol_ptr)(ssep->assoc_type->
                                                    source_corresp.assoc_info);
            /* A class/struct/union scope or reactivation scope.  Get the
               template from which this instantiation was generated.  If it
               is a partial specialization, get the primary template. */
            template_sym = instance_sym->variant.class_struct_union.
                                                    extra_info->class_template;
            if (template_sym != NULL) {
              template_sym = primary_template_of(template_sym);
            }  /* if */
            if (template_sym == *sym) {
              found = TRUE;
              break;
            }  /* if */
          }  /* if */
          /* Don't look beyond the innermost instantiation scope that
             is not a nested instantiation. */
          if (is_instantiation_scope && !ssep->nested_instantiation) break;
        }  /* if */
      }  /* for */
      if (found) *sym = instance_sym;
    }  /* if */
  }  /* if */
  return found;
}  /* current_class_symbol_if_class_template */


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
     scanning the template argument list of a template class reference. */
  while (tpp != NULL) {
    register a_symbol_ptr  param_symbol = tpp->param_symbol;
    if (tap != NULL) {
      /* A template argument exists for this parameter. */
      if (is_type_templ_arg(tap)) {
        check_assertion(param_symbol->kind == (a_symbol_kind)sk_type);
        param_symbol->variant.type.ptr = tap->variant.type;
      } else if (is_template_templ_arg(tap)) {
        /* A template template argument. */
        a_template_symbol_supplement_ptr	param_tssp;
        check_assertion(param_symbol->kind ==
                                           (a_symbol_kind)sk_class_template);
        /* Unlike the type and nontype cases, the symbol for a template
           template parameter is not updated directly.  Instead, the
           argument_template field of the template supplement is updated
           to point to the template that is to be used as the actual
           argument. */
        param_tssp = param_symbol->variant.template_info;
        param_tssp->variant.class_template.argument_template =
                                       symbol_for_template(tap->variant.templ);
      } else {
        check_assertion(param_symbol->kind == (a_symbol_kind)sk_constant);
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
  /* Loop through the parameters and set them to either the original
     template type or the original template constant (as specified by the
     type or constant field). */
  while (tpp != NULL) {
    register a_symbol_ptr  param_symbol = tpp->param_symbol;
    if (param_symbol->kind == (a_symbol_kind)sk_type) {
      param_symbol->variant.type.ptr = tpp->variant.type;
    } else if (param_symbol->kind == (a_symbol_kind)sk_constant) {
      param_symbol->variant.constant = tpp->variant.constant.ptr;
    } else {
      a_template_symbol_supplement_ptr	param_tssp;
      check_assertion(param_symbol->kind == (a_symbol_kind)sk_class_template);
      param_tssp = param_symbol->variant.template_info;
      param_tssp->variant.class_template.argument_template = param_symbol;
    }  /* if */
    param_symbol->template_param_not_visible = FALSE;
    tpp = tpp->next;
  }  /* while */
 db_exit();
}  /* restore_default_template_params */


void set_active_using_list_scope_depths(
				a_scope_depth		starting_depth,
                                a_boolean		set_value,
				a_decl_sequence_number	effective_decl_seq)
/*
This routine goes through the active using list for the scopes that
are now active and updates the scope at which the using directive
applies for each of the namespaces referenced.  If set_value is TRUE,
the flag is set to the value specified in the active using directive
entry.  If set_value is FALSE the flag is set to NO_SCOPE_DEPTH.
starting_depth is the innermost scope to be processed.

If effective_decl_seq is not NO_DECL_SEQUENCE_NUMBER, it specifies a
point after which any symbols that are declared should not be visible. 
This is used during template instantiations to ignore using-directives
specified after the point of definition of the template.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack[starting_depth];
  /* Go through the list of scopes.  When setting the flags, use the
     list of scopes linked by the previous scope pointer.  When clearing
     the flags, consider all scopes. */
  for (; ssep != NULL;
       ssep = set_value ? previous_scope_of(ssep) :
                          (ssep == &scope_stack[0] ? NULL : ssep - 1)) {
    a_scope_depth			curr_depth = scope_depth_of(ssep);
    an_active_using_directive_ptr	audp = ssep->active_using_directives;
    /* Set the flag for any active using directives for this scope. */
    for (; audp != NULL; audp = audp->next) {
      a_namespace_symbol_supplement_ptr	nssp;
      a_scope_depth			new_depth;
      if (do_dependent_name_processing && set_value &&
          ssep->kind != (a_scope_kind)sck_block &&
          ssep->kind != (a_scope_kind)sck_function &&
          effective_decl_seq != NO_DECL_SEQUENCE_NUMBER &&
          (audp->effective_decl_seq > effective_decl_seq)) {
        /* This using-directive became effective after the point that the
           using-directive appeared.  Ignore this using-directive.  This
           test is ignored for block scope using directives, because any
           ones that are on the list are visible, and the declaration sequence
           number test will fail for these declarations. */
        continue;
      }  /* if */
      new_depth = set_value 
                    ? audp->scope_depth_at_which_using_directive_applies
                    : NO_SCOPE_DEPTH;
      nssp = audp->namespace_supplement;
      nssp->scope_depth_at_which_using_directive_applies = new_depth;
      /* Record the scope depth of the innermost active using directive for
         this namespace.  If we are clearing the flags, reset this depth
         to NO_SCOPE_DEPTH. */
      if (set_value) {
        if (curr_depth > nssp->depth_innermost_active_using_directive) {
          nssp->depth_innermost_active_using_directive = curr_depth;
        }  /* if */
        /* Set the flag in the scope entry for which this using directive
           applies. */
        scope_stack[new_depth].using_directives_apply = TRUE;
      } else {
        nssp->depth_innermost_active_using_directive = NO_SCOPE_DEPTH;
      }  /* if */
    }  /* for */
    /* If we are clearing the flags, clear the flag for this scope that
       indicates that there are using directives that must be processed
       when this scope is reached. */
    if (!set_value) ssep->using_directives_apply = FALSE;
  }  /* for */
}  /* set_active_using_list_scope_depths */


void clear_scope_pointers_block(a_scope_pointers_block_ptr  spbp)
/*
Initialize the fields in a scope-pointers-block substructure.
*/
{
  spbp->symbols                      = NULL;
  spbp->synth_namespace_projection_symbols
                                     = NULL;
  spbp->last_symbol                  = NULL;
  spbp->last_constant                = NULL;
  spbp->last_type                    = NULL;
  spbp->last_variable                = NULL;
  spbp->last_routine                 = NULL;
  spbp->last_asm_entry               = NULL;
  spbp->last_dynamic_init            = NULL;
  spbp->last_namespace               = NULL;
  spbp->last_using_decl              = NULL;
  spbp->last_pragma                  = NULL;
#if RECORD_HIDDEN_NAMES_IN_IL
  spbp->last_hidden_name             = NULL;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  spbp->last_template                = NULL;
  spbp->unnamed_namespace_sym        = NULL;
  spbp->add_symbols_to_inactive_list = FALSE;
#if CHECKING 
  spbp->avoid_codecenter_warnings    = FALSE;
#endif /* CHECKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  spbp->last_source_sequence_entry   = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* clear_scope_pointers_block */


/*
Return TRUE if the scope stack entry kind given by kind is for something
that has an effect on access control (a class, class reactivation, or
function).  Access control only exists in C++.
*/
#define is_scope_kind_that_affects_access_control(kind)               \
   ((kind) == (a_scope_kind)sck_class_struct_union ||                 \
    (kind) == (a_scope_kind)sck_class_reactivation ||                 \
    (kind) == (a_scope_kind)sck_template_instantiation ||	      \
    (kind) == (a_scope_kind)sck_function ||			      \
    (kind) == (a_scope_kind)sck_function_access)


/*
Return TRUE if the scope stack entry kind is for something that should
affect the current declarative level.  In C, the current declarative
level is the same as depth_scope_stack except when struct/union field
scopes are active; when they are, it indicates the first non-struct-or-union
scope.  In C++, struct/union/class scopes are real scopes; however,
class and namespace reactivations and template instantiations are not real
scopes.  In neither C or C++ is a pragma scope is treated as a real scope.
*/
#define is_scope_kind_that_affects_declarative_level(kind)              \
   ((kind) != (a_scope_kind)sck_pragma &&                               \
   ((C_dialect != C_dialect_cplusplus) ?                                \
       /* C -- struct/union classes are not real scopes. */             \
        ((kind) != (a_scope_kind)sck_class_struct_union) :              \
        /* C++ -- class reactivations are not real scopes. */           \
        ((kind) != (a_scope_kind)sck_class_reactivation &&              \
         (kind) != (a_scope_kind)sck_namespace_reactivation &&          \
         (kind) != (a_scope_kind)sck_template_instantiation)))


static a_scope_ptr push_scope_full(
                               a_scope_kind             kind,
			       a_scope_number           scope_number_to_reuse,
			       a_type_ptr               assoc_type,
			       a_routine_ptr            assoc_routine,
                               a_namespace_ptr          assoc_namespace,
			       a_symbol_ptr             instance_sym,
			       a_symbol_ptr             template_sym,
			       a_template_arg_ptr       template_arg_list,
                               a_template_decl_info_ptr template_decl_info,
			       a_push_scope_options_set	options)
/*
Begin a new name scope by pushing an entry on the scope stack.  kind indicates
the kind of scope (file, function, block, function prototype, etc.).  Returns
a pointer to the IL scope allocated (or NULL if no IL scope is allocated,
as happens, for example, with function prototype scopes).  For function
scopes, scope_number_to_reuse is the scope number to be used (it was chosen
when the function prototype was scanned, or is NO_SCOPE_NUMBER if it hasn't
been chosen yet); for class reactivation scopes, scope_number_to_reuse is
the class scope number; for the file scope, scope_number_to_reuse is
file_scope_number assigned earlier; for the other cases, a new scope number
is generated. assoc_type points to an associated type for the cases where
that's meaningful (function prototype, class, class reactivation, and template
instantiation (for class templates only) scopes); it must be NULL in other
cases.  assoc_routine points to a routine for the function scope case and
for function access scopes; it must be NULL in other cases.  instance_symbol,
template_symbol, and template_arg_list are non-NULL only when a template
instantiation scope is being pushed; they represent, respectively, the symbol
for the class or function being instantiated or the static data member being
defined; the symbol identifying the template on which the instantiation or
definition is based; and the template argument list the produces the
specific version of the template.  template_decl_info is used for template
instantiation scopes, and describes the context being created by the
instantiation scope (the template parameters to be used, etc.).
template_decl_info is also used for template declaration scopes and points
to the declaration information for the template declaration scope being pushed.

options is a bit set of option flags that specify additional information about
the scope being pushed.
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
        kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation ||
        kind == (a_scope_kind)sck_class_reactivation ||
        kind == (a_scope_kind)sck_template_instantiation) {
    /* For function scopes, reuse the scope used for the parameters
       in the function declarator. */
    /* For class reactivations, re-establish the class scope and for template
       instantiations re-establish the template declaration scope.
       For the file scope, use the specified scope number. */
    ssep->number       = scope_number_to_reuse;
  } else {
    /* Assign a new scope number for other kinds of scopes. */
    ssep->number       = take_next_scope_number();
  }  /* if */
  /* Save the current IL memory region for restoration by pop_scope.  That's
     important if we have temporarily switched into the file scope memory
     region. */
  ssep->prev_il_memory_region = curr_il_region_number;
  /* Allocate the IL scope entry if one is needed. */
  switch (kind) {
    case sck_file:
      /* Activate or reactivate the file scope. */
      sp = curr_translation_unit->primary_scope;
      sp->depth_in_scope_stack = depth_scope_stack;
      switch_il_region(file_scope_region_number);
      ssep->il_memory_region = curr_il_region_number;
      break;
    case sck_function:
      /* Start a new memory region for a function scope.
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
      if (curr_il_region_number != file_scope_region_number) {
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      break;
    case sck_namespace:
    case sck_namespace_extension:
      if (curr_il_region_number != file_scope_region_number) {
        /* In a legal program we should already be in the file-scope memory
           region -- there must have been an error. */
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      if (kind == (a_scope_kind)sck_namespace) {
        sp = alloc_scope(kind, ssep->number, (a_routine_ptr)NULL);
        sp->variant.assoc_namespace = assoc_namespace;
        assoc_namespace->variant.assoc_scope = sp;
      } else {
        sp = assoc_namespace->variant.assoc_scope;
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
      break;
    case sck_class_struct_union:
      /* Class/struct/union definitions require the file-scope memory region,
         since the entities created to represent the members are pointed to
         from the type entry. */
      if (curr_il_region_number != file_scope_region_number) {
        switch_il_region(file_scope_region_number);
      }  /* if */
      ssep->il_memory_region = file_scope_region_number;
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
    case sck_condition:
      /* A C++ condition scope is only created when there is a declaration,
         so we know an IL scope will be required. */
      check_assertion_str(curr_il_region_number != file_scope_region_number,
                          "push_scope_full: bad region number for condition");
      sp = alloc_scope((a_scope_kind)sck_condition, ssep->number,
                       (a_routine_ptr)NULL);
      ssep->il_memory_region = curr_il_region_number;
      /* Add it to the scopes list for the enclosing scope. */
      add_to_scopes_list(sp, ssep-1);
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
  if (sp != NULL && sp->depth_in_scope_stack == NO_SCOPE_DEPTH) {
    /* Update the depth at which this scope is on the stack.  Only do this
       if the scope is not already on the stack somewhere else. */
    sp->depth_in_scope_stack = depth_scope_stack;
  }  /* if */
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
  ssep->in_nonreal_instantiation = FALSE;
  ssep->in_class_specialization  = FALSE;
  ssep->defer_access_checks      = FALSE;
  ssep->nested_instantiation     = FALSE;
  ssep->is_try_block             = FALSE;
  ssep->within_try_block         = FALSE;
  ssep->is_catch_in_function_try = FALSE;
  ssep->using_directives_apply   = FALSE;
  ssep->within_unnamed_namespace = FALSE;
  ssep->reactivated_class_being_defined = FALSE;
  ssep->is_for_init_block        = FALSE;
  ssep->namespace_pushed         = FALSE;
  ssep->exclude_from_context_output = FALSE;
  ssep->instantiation_scope_pushed = FALSE;
  ssep->microsoft_specialization_scope_pushed = FALSE;
  ssep->stop_token_stack_pushed  = FALSE;
  ssep->explicitly_declared_namespace_extension = FALSE;
  ssep->microsoft_specialization_instantiation_scope = FALSE;
#if USER_CONTROL_OF_STRUCT_PACKING
  ssep->pragma_pack_is_local     = FALSE;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  ssep->is_reactivation          = (options & PS_IS_REACTIVATION) != 0;
#if DO_IL_LOWERING
  ssep->assign_string_literal_sequence_numbers = FALSE;
  ssep->string_literal_table = NULL;
  ssep->string_literal_sequence_number = 0;
#endif /* DO_IL_LOWERING */
#if CHECKING 
  ssep->avoid_codecenter_warnings    = FALSE;
#endif /* CHECKING */
  ssep->il_scope                 = sp;
  ssep->assoc_type               = assoc_type;
  ssep->assoc_routine            = assoc_routine;
  ssep->assoc_namespace          = assoc_namespace;
  ssep->vla_fixup_list           = NULL;
  ssep->extern_type_fixup_list   = NULL;
  ssep->generated_entities       = NULL;
  ssep->shareable_constants_list = NULL;
  ssep->last_routine_fixup       = NULL;
  ssep->last_parameter           = NULL;
  ssep->last_nonstatic_variable  = NULL;
  ssep->last_label               = NULL;
  ssep->first_scope              = NULL;
  ssep->last_scope               = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  ssep->source_sequence_avail_list = NULL;
  ssep->source_sequence_entries_disallowed =
                                       source_sequence_entries_disallowed;
  ssep->ss_list_instantiation_insert_point
                                 = NULL;
  ssep->source_sequence_list     = NULL;
  ssep->end_of_source_sequence_list = NULL;
  if (kind == (a_scope_kind)sck_file && ssep->is_reactivation) {
    /* For a reactivation of the file scope, restore the source sequence list
       that was built up on the previous push/pop. */
    ssep->source_sequence_list = il_header.primary_scope->source_sequence_list;
    ssep->end_of_source_sequence_list = curr_translation_unit->
                          file_scope_pointers_block.last_source_sequence_entry;
    curr_translation_unit->
                   file_scope_pointers_block.last_source_sequence_entry = NULL;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  ssep->depth_template_declaration_scope = depth_template_declaration_scope;
  ssep->depth_innermost_instantiation_scope =
                                       depth_innermost_instantiation_scope;
  ssep->instance_sym             = instance_sym;
  ssep->template_sym             = template_sym;
  ssep->template_arg_list        = template_arg_list;
  ssep->source_position          = pos_curr_token;
  ssep->depth_innermost_function_scope = depth_innermost_function_scope;
  ssep->template_decl_info       = template_decl_info;
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
  ssep->templ_member_class_sym   = NULL;
  ssep->depth_innermost_namespace_scope = depth_innermost_namespace_scope;
  ssep->num_of_extra_times_pushed = 0;;
  ssep->number_of_local_classes   = 0;
  ssep->active_using_directives   = NULL;
  ssep->previous_scope            = NO_SCOPE_DEPTH;
  ssep->instantiation_context_depth = NO_SCOPE_DEPTH;
  ssep->instantiation_common_depth = NO_SCOPE_DEPTH;
  ssep->saved_depth_of_initial_lookup_scope = depth_of_initial_lookup_scope;
  ssep->orig_depth               = NO_SCOPE_DEPTH;
  ssep->saved_innermost_scope_that_affects_access = NO_SCOPE_DEPTH;
  ssep->first_template_cache_segment = NULL;
  ssep->last_template_cache_segment = NULL;
  ssep->class_def_state          = NULL;
  ssep->names_hidden_by_old_for_init = NULL;
  ssep->tmpl_decl_state		 = NULL;
  ssep->pending_templ_arg_lists  = 0;
  ssep->next_nondependent_call   = NULL;
#if IA64_ABI && NEED_NAME_MANGLING
  ssep->local_name_collision_table = NULL;
#endif /* IA64_ABI && NEED_NAME_MANGLING */
  /* Clear the substructure shared with namespace symbol supplements. */
  ssep->assoc_pointers_block     = NULL;
  clear_scope_pointers_block(&ssep->pointers_block);
  if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
    /* By default, the previous scope is the one that precedes this one
       on the scope stack.  This may be adjusted for instantiation scopes. */
    ssep->previous_scope = depth_of_initial_lookup_scope;
  }  /* if */
  /* Set the point at which name lookups should start to the newly
     created scope. */
  depth_of_initial_lookup_scope = depth_scope_stack;
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
  if (kind == (a_scope_kind)sck_file && ssep->is_reactivation) {
    /* When a file scope is reactivated, its symbols will be on the inactive
       list. */
    ssep->inactive_symbols_may_be_visible = TRUE;
  }  /* if */
  /* Check for class reactivations, classes with base classes,
     namespace extensions, and template instantiations.  When these
     are found name lookup is more involved.  If the new scope is neither,
     we can just use the state from the previous scope. */
  if (!C_mode() &&
      (kind == (a_scope_kind)sck_class_reactivation ||
       kind == (a_scope_kind)sck_template_instantiation ||
       kind == (a_scope_kind)sck_namespace_extension ||
       kind == (a_scope_kind)sck_namespace_reactivation ||
       (kind == (a_scope_kind)sck_class_struct_union &&
        base_classes_of(assoc_type) != NULL))) {
    ssep->inactive_symbols_may_be_visible = TRUE;
  } else if (kind != (a_scope_kind)sck_file) {
    ssep->inactive_symbols_may_be_visible =
                                   (ssep-1)->inactive_symbols_may_be_visible;
  }  /* if */
  if (!C_mode()) {
    if (kind == (a_scope_kind)sck_class_reactivation) {
      /* Determine whether the class being reactivated is still in the process
         of being defined.  This can occur when a class nested within a
         class template is instantiated while the enclosing class is still
         in the process of being instantiated. */
      ssep->reactivated_class_being_defined = is_incomplete_type(assoc_type);
    }  /* if */
    /* Pragma, template declaration and template instantiation scopes
       require that the slow lookup algorithm be used because they require
       that certain symbols on the active list not be considered. */
    if (kind == (a_scope_kind)sck_pragma ||
        kind == (a_scope_kind)sck_template_declaration ||
        kind == (a_scope_kind)sck_template_instantiation) {
      ssep->slow_lookup_required = TRUE;
    } else if (kind != (a_scope_kind)sck_file) {
      ssep->slow_lookup_required = (ssep-1)->slow_lookup_required;
    }  /* if */
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
      /* Determine whether this scope represents the specialization of a
         class. */
      ssep->in_class_specialization =
                       assoc_type->variant.class_struct_union.is_specialized;
 
    }  /* if */
    /* The class specialization flag is also set if the parent scope is
       a specialization scope (or nested within one). */
    if (kind != (a_scope_kind)sck_file) {
      ssep->in_class_specialization |= (ssep-1)->in_class_specialization;
    }  /* if */
    if (kind == (a_scope_kind)sck_template_instantiation) {
      /* Save the depth of the innermost instantiation scope. */
      depth_innermost_instantiation_scope = depth_scope_stack;
      /* Update the symbols of the template parameters to represent the
         values of the actual arguments by simply changing each to point to
         the type or constant specified by the corresponding template argument.
         The old values do not need to be saved because they can be easily
         recreated by pop_scope. */
      update_template_param_symbols(template_decl_info->parameters,
                                    template_arg_list);
      /* The current stack state is suspended when an template instantiation
         is done.  It will be restored in pop_scope.  inside_local_class
         is reset by push_template_instantiation_scope. */
      depth_innermost_function_scope =
              ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
      innermost_function_scope = NULL;
      ssep->in_prototype_instantiation =
                                   (options & PS_PROTOTYPE_INSTANTIATION) != 0;
      ssep->in_nonreal_instantiation =
                                   (options & PS_NONREAL_INSTANTIATION) != 0;
      if (template_sym->kind == (a_symbol_kind)sk_static_data_member) {
        /* Static data members don't have their own scope so the
           template parameters are added at the instantiation scope. */
        reactivate_template_params = TRUE;
      }  /* if */
      /* Initialize the pointer to the dependent call list for this
         template. */
      ssep->next_nondependent_call = template_decl_info->nondependent_calls;
    } else if (kind != (a_scope_kind)sck_file &&
               kind != (a_scope_kind)sck_namespace &&
               kind != (a_scope_kind)sck_namespace_extension) {
      if (kind == (a_scope_kind)sck_function &&
          assoc_routine->compiler_generated &&
          !assoc_routine->is_prototype_instantiation) {
        /* If the definition of a compiler-generated routine is kicked off
           within a prototype instantiation, the compiler generated routine
           should not be considered to be within a prototype instantiation. */
        ssep->in_prototype_instantiation = FALSE;
        ssep->in_nonreal_instantiation = FALSE;
      } else {
        ssep->in_prototype_instantiation =
                                          (ssep-1)->in_prototype_instantiation;
        ssep->in_nonreal_instantiation =
                                          (ssep-1)->in_nonreal_instantiation;
      }  /* if */
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
#if DO_IL_LOWERING
    if (!C_mode() && !ssep->in_prototype_instantiation) {
      /* Determine whether this is a function for which we need to
         compute string literal sequence numbers.  These are computed for
         routines for which there is the potential of having multiple copies
         in a program. */
      check_assertion(assoc_routine != NULL);
      ssep->assign_string_literal_sequence_numbers =
                         routine_might_exist_in_multiple_copies(assoc_routine);
    }  /* if */
#endif /* DO_IL_LOWERING */
  } else if (kind == (a_scope_kind)sck_file) {
    /* Note (1) depth_innermost_namespace_scope is set to the file scope's
       depth to give it the sense of "depth_innermost_global_scope", and (2)
       it's intentionally set even in C mode. */
    depth_innermost_namespace_scope =
            ssep->depth_innermost_namespace_scope = depth_scope_stack;
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
  } else if (kind == (a_scope_kind)sck_namespace ||
             kind == (a_scope_kind)sck_namespace_extension) {
    /* The following is required only to handle illegal programs gracefully.
       Ordinarily these will already be set correctly. */
    depth_innermost_function_scope =
            ssep->depth_innermost_function_scope = NO_SCOPE_DEPTH;
    innermost_function_scope = NULL;
  }  /* if */
  /* Set the default language linkage for the current scope. */
  if (C_dialect == C_dialect_cplusplus) {
    if (kind == (a_scope_kind)sck_file) {
      /* File scope has extern "C++" linkage by default. */
      ssep->default_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
      ssep->name_linkage_is_explicit = FALSE;
    } else if (kind == (a_scope_kind)sck_template_instantiation) {
      /* For template instantiations use the linkage of the template
         declaration. */
      ssep->default_name_linkage = template_decl_info->name_linkage;
      ssep->name_linkage_is_explicit = FALSE;
    } else {
      ssep->default_name_linkage = (ssep-1)->default_name_linkage;
      ssep->name_linkage_is_explicit = (ssep-1)->name_linkage_is_explicit;
    }  /* if */
  } else {
    ssep->default_name_linkage = (a_name_linkage_kind)nlk_external;
    ssep->name_linkage_is_explicit = FALSE;
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Maintain the depth of the innermost stack entry that affects access
       control.  Special handing for template instantiation scopes is done
       in fixup_instantiation_scopes. */
    if (is_scope_kind_that_affects_access_control(kind)) {
      depth_of_innermost_scope_that_affects_access_control = depth_scope_stack;
    }  /* if */
    /* Determine whether this scope affects whether access checks can
       be deferred. */
    if (kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_namespace ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_pragma ||
        (kind == (a_scope_kind)sck_template_instantiation &&
         (options & PS_MICROSOFT_SPECIALIZATION) == 0) ||
        kind == (a_scope_kind)sck_class_struct_union) {
      /* A scope that introduces a new level at which deferred access
         checks may be recorded. */
      curr_deferred_access_scope = depth_scope_stack;
    } else if (kind == (a_scope_kind)sck_template_declaration ||
               kind == (a_scope_kind)sck_func_prototype ||
               kind == (a_scope_kind)sck_function_access ||
               kind == (a_scope_kind)sck_namespace_reactivation ||
               (kind == (a_scope_kind)sck_template_instantiation &&
                (options & PS_MICROSOFT_SPECIALIZATION) != 0) ||
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
         become invisible.  Those scopes are visible if this is a Microsoft
         specialization instantiation scope, however. */
      if ((options & PS_MICROSOFT_SPECIALIZATION) == 0) {
        ssep->depth_template_declaration_scope =
          depth_template_declaration_scope = NO_SCOPE_DEPTH;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_namespace ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation) {
      /* Set the scope-pointers-block pointer to refer to the namespace
         symbol supplement. */
      a_symbol_ptr  sym =
                      (a_symbol_ptr)assoc_namespace->source_corresp.assoc_info;
      ssep->assoc_pointers_block =
                     &sym->variant.namespace_info.extra_info->pointers_block;
      if (kind != (a_scope_kind)sck_namespace_reactivation) {
        /* If this is a namespace scope that affects the declarative level
           (i.e., not just a reactivation) update the information about
           the current namespace. */
        ssep->within_unnamed_namespace =
              sym->variant.namespace_info.extra_info->within_unnamed_namespace;
        /* Maintain the depth of the innermost namespace scope. */
        depth_innermost_namespace_scope =
              ssep->depth_innermost_namespace_scope = depth_scope_stack;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_template_instantiation ||
        kind == (a_scope_kind)sck_pragma) {
      /* When beginning a nested context, clear the expression stack. */
      expr_stack = NULL;
    }  /* if */
  }  /* if */
  if (kind == (a_scope_kind)sck_file) {
    /* For the file scope, use the pointers block allocated in the
       translation unit entry. */
    ssep->assoc_pointers_block = &curr_translation_unit->
                                                    file_scope_pointers_block;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* The creation of source sequence entries is suppressed in certain
     contexts. */
  if (!is_primary_translation_unit) {
    /* No source sequence entries are created for secondary translation
       units. */
  } else if (kind == (a_scope_kind)sck_template_declaration) {
    if (!prototype_instantiations_in_il) {
      /* Source sequence entries are generated in template declaration scopes
         when prototype instantiations are passed in the IL (prototype
         instantiations for default template arguments are done in
         template declaration scopes). */
      ssep->source_sequence_entries_disallowed =
        source_sequence_entries_disallowed = TRUE;
    }  /* if */
  } else if (kind == (a_scope_kind)sck_pragma) {
    ssep->source_sequence_entries_disallowed =
      source_sequence_entries_disallowed = TRUE;
  } else if (kind == (a_scope_kind)sck_template_instantiation) {
    if (instance_sym == NULL) {
      /* If instance_sym is NULL we are pushing the scope for the declaration
         (but not the body) of a template function -- no source sequence
         entries would be involved. */
      source_sequence_entries_disallowed = TRUE;
    } else if (ssep->in_prototype_instantiation) {
      /* If prototype instantiations are not recorded in the IL, source
         sequence entries are normally not generated during a prototype
         instantiation.  (When they are, they are placed on a list that
         is not part of the IL proper.) */
      a_boolean  prototype_in_real_instance = FALSE;
      /* assoc_type != NULL means we're in a class template scope. */
      if (assoc_type != NULL) {
        /* Prototype instantiations inside real instantiations should not
           generate source sequence entries. */
        a_template_symbol_supplement_ptr tssp =
                     template_supplement_for_symbol(
                         (a_symbol_ptr)assoc_type->source_corresp.assoc_info);
        prototype_in_real_instance = (tssp->prototype_template != NULL &&
                                      !tssp->is_specific_definition);
      }  /* if */
      source_sequence_entries_disallowed = !prototype_instantiations_in_il ||
                                           prototype_in_real_instance;
    } else if (scope_stack[DEPTH_OF_FILE_SCOPE].
                                    source_sequence_entries_disallowed) {
      check_assertion(source_sequence_entries_disallowed == TRUE);
    } else {
      /* If they are allowed at file scope, they may be permitted for a
         template instantiation. */
      if (assoc_type != NULL) {
        /* We are pushing the scope for a class template instantiation.
           Source sequence entries are normally disallowed for instantiations,
           but should not be disallowed for the instantiation scope pushed
           around a template class specialization in Microsoft mode.
           The "!is_incomplete_type" test is done to detect the reactivation of
           a class scope.  Source sequence entries should also not be
           disallowed for the instantiation scope pushed for the reactivation
           of a template class. */
        source_sequence_entries_disallowed =
        !(use_microsoft_specialization_scope &&
          (assoc_type->variant.class_struct_union.is_specialized ||
           !is_incomplete_type(assoc_type))) &&
        !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS; /*lint !e506*/
      } else {
        /* We are pushing the scope for a function template instantiation
           or for the definition of a template static data member. */
        source_sequence_entries_disallowed =
     !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS; /*lint !e506*/
      }  /* if */
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
    if (kind == (a_scope_kind)sck_file) {
      /* Push an object lifetime for the file scope, or reuse one if this
         is a reactivation. */
      curr_object_lifetime = sp->lifetime;
      if (curr_object_lifetime == NULL) {
        push_object_lifetime((an_il_entry_kind)iek_scope, (char *)sp,
                             (an_object_lifetime_kind)(olk_global_static));
      }  /* if */
      ssep->curr_scope_object_lifetime = curr_object_lifetime;
    } else if (kind == (a_scope_kind)sck_function ||
               kind == (a_scope_kind)sck_block ||
               kind == (a_scope_kind)sck_condition) {
      /* This is the sort of scope for which a new block object lifetime is
         pushed. */
      push_object_lifetime((an_il_entry_kind)iek_scope, (char *)sp,
                           (an_object_lifetime_kind)(olk_block));
      ssep->curr_scope_object_lifetime = curr_object_lifetime;
    }  /* if */
  }  /* if */
  /* Propagate the flag indicating that this scope is inside the compound
     statement of a try block. */
  if (kind == (a_scope_kind)sck_block && (ssep-1)->within_try_block) {
    ssep->within_try_block = TRUE;
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
                          assoc_routine, (a_namespace_ptr)NULL,
                          (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                          (a_template_arg_ptr)NULL,
                          (a_template_decl_info_ptr)NULL, PS_NO_OPTIONS);
  return scope;
}  /* push_scope */


void push_file_scope(a_boolean	is_reactivation)
/*
Activate or reactivate the file scope of the current translation unit.
is_reactivation is TRUE if this is not the first push of the file
scope.
*/
{
  a_push_scope_options_set	ps_options = PS_NO_OPTIONS;

  if (is_reactivation) ps_options |= PS_IS_REACTIVATION;
  (void)push_scope_full((a_scope_kind)sck_file, file_scope_number,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL,
                        (a_template_decl_info_ptr)NULL, ps_options);
  /* Add active using directives for the namespaces that should be
     visible because of the transitivity of using directives. */
  add_active_using_directives_for_scope(curr_translation_unit->primary_scope,
                                        &scope_stack[depth_scope_stack],
					NO_DECL_SEQUENCE_NUMBER);
}  /* push_file_scope */


a_scope_ptr push_for_init_scope(void)
/*
Push a block scope for a for-init declaration and return a pointer to the IL
scope.
*/
{
  a_scope_stack_entry_ptr  ssep;
  a_scope_ptr              sp;

  (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL);
  ssep = &scope_stack[depth_scope_stack];
  ssep->is_for_init_block = TRUE;
  sp = ensure_il_scope_exists(ssep);
  return sp;
}  /* push_for_init_scope */


a_scope_ptr push_namespace_scope(a_scope_kind    kind,
                                 a_namespace_ptr assoc_namespace)
/*
Interface to push_scope_full that is used for sck_namespace scopes
("original" namespace definitions).  It is also used to reopen an
sck_namespace IL scope by pushing an sck_namespace_extension scope stack
entry (for "extension-namespace-definitions").  This is used both when
a namespace is defined and when an instantiation scope is pushed for a
template defined in a namespace.
*/
{
  a_scope_ptr     scope;
  a_scope_number  scope_number_to_reuse = NO_SCOPE_NUMBER;

  check_assertion_str(assoc_namespace != NULL &&
                      !assoc_namespace->is_namespace_alias &&
                      ((assoc_namespace->variant.assoc_scope == NULL) ==
                                       (kind == (a_scope_kind)sck_namespace)),
                      "push_namespace_scope: bad assoc_namespace ptr");
  if (kind == (a_scope_kind)sck_namespace_extension ||
      kind == (a_scope_kind)sck_namespace_reactivation) {
    scope_number_to_reuse = assoc_namespace->variant.assoc_scope->number;
  }  /* if */
  scope = push_scope_full(kind, scope_number_to_reuse, (a_type_ptr)NULL,
                          (a_routine_ptr)NULL, assoc_namespace,
                          (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                          (a_template_arg_ptr)NULL,
                          (a_template_decl_info_ptr)NULL, PS_NO_OPTIONS);
  /* Add active using directives for the namespaces that should be
     visible because of the transitivity of using directives. */
  add_active_using_directives_for_scope(assoc_namespace->variant.assoc_scope,
                                        &scope_stack[depth_scope_stack],
					NO_DECL_SEQUENCE_NUMBER);
  return scope;
}  /* push_namespace_scope */


static void microsoft_using_directive_bug_processing(a_namespace_ptr	nsp)
/*
The Microsoft compiler (as of Visual C++ 6.0) has a bug that causes a
namespace nominated by a using-directive to be visible in the file scope.

  namespace M  { 
    class C{};
  }
  namespace N {
    using namespace M;
  }
  namespace N {}
  void f(C*); // C is visible in the global namespace

This bug only occurs when the using-directive is in a namespace that
has been extended (i.e., not one for which there has only been a
primary namespace definition).

This routine implements this bug by taking the using-directives from the
namespace being popped and applying them to the file scope.
*/
{
  a_using_decl_ptr	udp = nsp->variant.assoc_scope->using_decls;
  a_scope_depth		depth;

  /* Create using-directives for each of the namespaces nominated in a
     using-directive of the namespace scope specified by nsp. */
  while (udp != NULL) {
    if (udp->is_using_directive) {
      a_namespace_ptr	udp_nsp;
      check_assertion(udp->entity.kind == (a_byte_il_entry_kind)iek_namespace);
      /* Get a pointer to the namespace to be used. */
      udp_nsp = skip_namespace_aliases((a_namespace_ptr)udp->entity.ptr);
      make_using_directive(udp_nsp, DEPTH_OF_FILE_SCOPE, &null_source_position,
                           /*compiler_generated=*/TRUE);
    }  /* if */
    udp = udp->next;
  }  /* while */
  /* Update all of the scopes on the scope stack to indicate that symbols
     visible as a result of a using-directive may be visible. */
  for (depth = depth_scope_stack; depth >= DEPTH_OF_FILE_SCOPE; depth--) {
    scope_stack[depth].inactive_symbols_may_be_visible = TRUE;
  }  /* for */
}  /* microsoft_using_directive_bug_processing */


void pop_namespace_scope(void)
/*
Pop a namespace or namespace extension scope.  Unlike push_namespace_scope,
this routine is used only for namespace scopes that appear in the source
program, not used when popping the namespace scope pushed as part of the
template instantiation process.
*/
{
  a_namespace_ptr		assoc_namespace;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
  a_scope_kind			kind;

  kind = ssep->kind;
  check_assertion(kind == (a_scope_kind)sck_namespace ||
                  kind == (a_scope_kind)sck_namespace_extension);
  assoc_namespace = ssep->assoc_namespace;
  pop_scope();
  if (microsoft_bugs && microsoft_version <= 1200 &&
      kind == (a_scope_kind)sck_namespace_extension) {
    /* Make any using-directives in this namespace visible in the file
       scope (to emulate a Microsoft bug). */
    microsoft_using_directive_bug_processing(assoc_namespace);
  }  /* if */
}  /* pop_namespace_scope */


static a_scope_depth find_depth_of_common_scope(a_namespace_ptr nsp)
/*
Find the innermost scope on the scope stack that is also either the
namespace pointed to by nsp or is a parent of nsp.  Return the
depth of the scope stack entry associated with the common scope or
DEPTH_FILE_SCOPE if there is none.
*/
{
  a_scope_stack_entry_ptr	ssep = NULL;
  a_scope_depth			common_depth;

  for (; nsp != NULL; nsp = nsp->source_corresp.parent.namespace_ptr) {
    a_scope_ptr	ns_scope = nsp->variant.assoc_scope;
    if (ns_scope->depth_in_scope_stack == NO_SCOPE_DEPTH) {
      /* The scope associated with this namespace is not on the scope stack.
         Keep looking. */
      continue;
    }  /* if */
    /* Even if the scope is on the stack, it could be invisible because of
       an intervening instantiation scope.  Look for the namespace in the
       currently visible scopes. */
    for (ssep = &scope_stack[depth_innermost_namespace_scope];
         ssep != NULL;
         ssep = previous_scope_of(ssep)) {
      if (ssep->il_scope == ns_scope) break;
    }  /* for */
    /* If we found a match, exit the loop. */
    if (ssep != NULL) break;
  }  /* for */
  /* Return DEPTH_FILE_SCOPE if there is no common namespace scope. */
  common_depth = ssep != NULL ? scope_depth_of(ssep) : DEPTH_OF_FILE_SCOPE;
  return common_depth;
}  /* find_depth_of_common_scope */


static
void push_namespace_extension_for_instantiation(a_namespace_ptr nsp,
                                                a_namespace_ptr common_nsp,
                                                a_scope_depth   prev_scope)
/*
Push namespace extension scopes needed to instantiate an entity defined
in the namespace pointed to by nsp.  The scopes to be pushed are nsp
and its parent scopes up to, but not including, the scope pointed to
by common_nsp.  Link the previous scope entries so that prev_scope is
the previous scope of the first scope pushed by this routine.
*/
{
  a_namespace_ptr		parent_nsp;

  /* The entry isn't on the stack.  Push any parent namespaces, then push
     the specified namespace. */
  parent_nsp = nsp->source_corresp.parent.namespace_ptr;
  if (parent_nsp != NULL && parent_nsp != common_nsp) {
    /* A namespace nested in another namespace.  Push the parent
       namespace. */
    push_namespace_extension_for_instantiation(parent_nsp, common_nsp,
                                               prev_scope);
    prev_scope = NO_SCOPE_DEPTH;
  }  /* if */
  /* Push an entry for the scope. */
  (void)push_namespace_scope((a_scope_kind)sck_namespace_extension, nsp);
  if (prev_scope != NO_SCOPE_DEPTH) {
    /* For the first scope pushed by this routine (the one whose parent is
       common_nsp), set the previous scope to the one passed from the
       caller.  For subsequent scopes, prev_scope will have been changed to
       NO_SCOPE_DEPTH above, so this assignment won't be done. */
    scope_stack[depth_scope_stack].previous_scope = prev_scope;
  }  /* if */
}  /* push_namespace_extension_for_instantiation */


static
a_namespace_ptr referencing_namespace_for_instance(a_symbol_ptr instance_sym)
/*
Given a symbol that points to a particular instance of a template, return
the namespace pointer of the namespace in which an instantiation of
the template was first required.

instance_sym points to a symbol for an instance of a template.  It may also
be NULL if we don't yet know which instance we are dealing with.
*/
{
  a_namespace_ptr	nsp = NULL;

  if (instance_sym == NULL) {
    /* We don't know which instance is being used yet.  Determine the
       referencing namespace from the scope stack. */
    nsp = determine_referencing_namespace();
  } else if (instance_sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
             instance_sym->kind == (a_symbol_kind)sk_union_tag) {
    /* The instance points to a class symbol.  Return the referencing
       namespace from the class symbol supplement. */
    a_class_symbol_supplement_ptr	cssp;
    cssp = instance_sym->variant.class_struct_union.extra_info;
    nsp = cssp->referencing_namespace;
  } else {
    /* The instance points to a routine or static data member.  Return
       the referencing namespace from the template instance record. */
    a_template_instance_ptr	tip;
    if (instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      tip = instance_sym->variant.static_data_member.instance_ptr;
    } else {
      check_assertion(instance_sym->kind == (a_symbol_kind)sk_routine ||
                      instance_sym->kind == (a_symbol_kind)sk_member_function);
      tip = instance_sym->variant.routine.instance_ptr;
    }  /* if */
    nsp = tip->referencing_namespace;
  }  /* if */
  return nsp;
}  /* referencing_namespace_for_instance */


static
void get_parent_information_for_template(a_scope_ptr	 sp,
                                         a_symbol_ptr    template_sym,
                                         a_symbol_ptr    instance_sym,
 				         a_namespace_ptr *p_nsp,
					 a_type_ptr	 *p_tp)
/*
Determine the namespace and class scopes that must be reactivated in
order for a given instantiation to be done.  sp points to the enclosing
scope of the template declaration, template_sym is the symbol of the
template, instance_sym is the symbol for the instance being created
and may be NULL.  *nsp and *tp are returned by this routine, and point
to the namespace and class that must be reactivated.
*/
{
  a_type_ptr		parent_type;
  a_namespace_ptr	parent_namespace;

  /* Determine the parent type and namespace based on the enclosing scope
     of the template. */
  if (sp->kind == (a_scope_kind)sck_class_struct_union) {
    /* The scope is a class scope.  Get the class type and loop
       through any enclosing classes to find the parent namespace. */
    a_type_ptr	tp;
    parent_type = sp->variant.assoc_type;
    for (tp = parent_type; tp->source_corresp.is_class_member;) {
      tp = tp->source_corresp.parent.class_type;
    }  /* for */
    parent_namespace = tp->source_corresp.parent.namespace_ptr;
  } else if (sp->kind == (a_scope_kind)sck_file) {
    /* File scope.  Both the class and namespace pointer should be NULL. */
    parent_type = NULL;
    parent_namespace = NULL;
  } else {
    /* A namespace scope.  Get the namespace pointer and set the class
       type to NULL. */
    check_assertion(sp->kind == (a_scope_kind)sck_namespace);
    parent_type = NULL;
    parent_namespace = sp->variant.assoc_namespace;
  }  /* if */
  if (instance_sym == NULL &&
      template_sym->kind == (a_symbol_kind)sk_function_template) {
    /* Use the parent information determined above.

       When there is no specific instance being instantiated, that indicates
       that we are instantiating something like a template parameter type
       that depends on another template parameter, or a default template
       argument whose type depends on a template parameter.  This is also
       the case when scanning the function declaration of a function template
       to do the partial instantiation of the function.  Note that this
       is not the case when scanning the default function arguments
       though.  For the function template case, the parent information is
       determined by the scope that contained the template declaration
       based on which the current instantiation is being done. */
    *p_tp = parent_type;
    *p_nsp = parent_namespace;
  } else if (!template_sym->is_class_member && 
             template_sym->kind == (a_symbol_kind)sk_function_template) {
    /* When generating an instance of a function template (that is not a
       member template) that was defined in some other class scope, we need
       to create the context of that class scope in order to do the 
       instantiation.  The namespace to be used is the namespace
       of which the template is a member.  Functions cannot be defined
       using qualified names in friend declarations, so the namespace
       must be the same as the namespace containing the class. */
    *p_tp = parent_type;
    *p_nsp = parent_namespace_for_symbol(instance_sym);
  } else {
    /* If we are instantiating a particular instance of a template, get
       the parent information from the template being instantiated. */
    a_symbol_ptr	sym_to_use;
    sym_to_use = instance_sym != NULL ? instance_sym : template_sym;
    *p_nsp = parent_namespace_for_symbol(sym_to_use);
    if (sym_to_use->is_class_member) {
      *p_tp = sym_to_use->parent.class_type;
    } else {
      *p_tp = NULL;
    }  /* if */
  }  /* if */
}  /* get_parent_information_for_template */


static void push_single_class_reactivation_scope(a_type_ptr class_type)
/*
Reactivate the class indicated by class type.  Do not push the scopes for
any enclosing class types or namespaces.
*/
{
  a_scope_ptr	il_scope;

  /* Find the IL scope to get the scope number. */
  il_scope = class_type->variant.class_struct_union.extra_info->assoc_scope;
  check_assertion_str2(il_scope != NULL,
                       "push_single_class_reactivation_scope:",
                       "NULL assoc_scope");
  (void)push_scope((a_scope_kind)sck_class_reactivation, il_scope->number,
                   class_type, (a_routine_ptr)NULL);
}  /* push_single_class_reactivation_scope */


static
void reactivate_class_and_instantiation_scopes(
                      a_template_decl_info_ptr	decl_info,
                      a_type_ptr		parent_class,
                      a_symbol_ptr		instance_sym,
		      a_type_ptr		assoc_type,
		      a_routine_ptr		assoc_routine)
/*
Reactivate the class specified by parent_class and any classes that enclose
parent class.  If any of the classes are template classes, push
instantiation scopes for those classes too.  parent_class is the class
that must be reactivated.  decl_info is the template declaration
information for the template being instantiated.  instance_sym is the
symbol to be used (if not NULL) as the instance symbol for the outermost
instantiation scope that is pushed.   Likewise, assoc_type and assoc_routine
are non-NULL when they should be used for the outermost instantiation scope.

*/
{
  a_type_ptr                        class_type;
  a_symbol_ptr                      class_sym;
  a_symbol_ptr                      template_sym = NULL;
  a_template_symbol_supplement_ptr  tssp = NULL;
  a_template_arg_ptr                template_arg_list;
  a_template_decl_info_ptr	    enclosing_tdip;
  a_boolean			    is_template;
  a_symbol_ptr			    enclosing_instance_sym = NULL;
  a_type_ptr			    enclosing_assoc_type = NULL;
  a_routine_ptr			    enclosing_assoc_routine = NULL;

  class_type = skip_typerefs(parent_class);
  class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  check_assertion_str2(class_sym != NULL,
                       "reactivate_class_and_instantiation_scopes:",
                       "class type has NULL assoc_info");
  is_template = is_template_class_and_not_specific_def_symbol(class_sym);
  if (is_template) {
    a_class_symbol_supplement_ptr     cssp;
    cssp = class_sym->variant.class_struct_union.extra_info;
    template_sym = cssp->class_template;
    tssp = template_sym->variant.template_info;
    /* Determine which template declaration information is to be used
       when pushing the instantiation scope for the enclosing class.
       For a member template defined outside of the class you need
       to use the template parameter list from the definition of
       the member template (as supplied by the decl_info pointer), not that
       of the enclosing class.  If decl_info is NULL, the information from
       the class is used instead. */
    if (decl_info == NULL) decl_info = cache_for_template(tssp)->decl_info;
    enclosing_tdip = decl_info->enclosing_template_decl;
    /* If no instance symbol is passed from the caller, use the symbol
       and type from the class that is being reactivated.  Otherwise,
       use the symbol, type and/or routine passed by the caller. */
    if (instance_sym == NULL) {
      instance_sym = class_sym;
      assoc_type = class_type;
      assoc_routine = NULL;
    }  /* if */
    enclosing_instance_sym = NULL;
  } else {
    /* If this is not a template, use the decl_info value passed in as
       the value passed to the recursive call below.  If no template
       declaration information was supplied, use the information associated
       with this class. */
    if (decl_info == NULL) {
      if (is_template_instance_class_symbol(class_sym) &&
          !is_template_instance_specific_def_symbol(class_sym)) {
        template_sym = template_symbol_for_class_symbol(class_sym);
        /* Get the template declaration information associated with
           the class. */
        tssp = template_supplement_for_symbol(template_sym);
        decl_info = cache_for_template(tssp)->decl_info;
      }  /* if */
    }  /* if */
    enclosing_tdip = decl_info;
    enclosing_instance_sym = instance_sym;
    enclosing_assoc_type = assoc_type;
    enclosing_assoc_routine = assoc_routine;
  }  /* if */
  if (class_sym->is_class_member) {
    /* If this is not the outermost class, reactivate any enclosing
       classes. */
    reactivate_class_and_instantiation_scopes(enclosing_tdip,
                                              class_sym->parent.class_type,
                                              enclosing_instance_sym,
                                              enclosing_assoc_type,
                                              enclosing_assoc_routine);
  }  /* if */
  if (is_template) {
    if (tssp->variant.class_template.prototype_instantiation_complete) {
      /* Push a template instantiation scope associated with the
         class in which this template was defined.  Don't do this if
         the prototype instantiation is already in process. */
      a_push_scope_options_set	ps_options = PS_NO_OPTIONS;
      /* If the class is a prototype instantiation, pass the appropriate flag
         to push_scope. */
      if (class_type->variant.class_struct_union.is_prototype_instantiation) {
        ps_options = PS_PROTOTYPE_INSTANTIATION;
      }  /* if */
      template_arg_list = templ_arg_list_for_class(class_type);
      (void)push_scope_full((a_scope_kind)sck_template_instantiation,
                          decl_info->declaration_scope, assoc_type,
                          assoc_routine, (a_namespace_ptr)NULL,
                          instance_sym, template_sym, template_arg_list,
                          decl_info, ps_options);
    }  /* if */
  }  /* if */
  /* Reactivate the enclosing class scope. */
  push_single_class_reactivation_scope(class_type);
}  /* reactivate_class_and_instantiation_scopes */


static void push_instantiation_context(
		a_template_decl_info_ptr	decl_info,
		a_namespace_ptr			definition_nsp,
		a_type_ptr			definition_class,
		a_namespace_ptr			reference_nsp,
		a_scope_depth			*p_common_depth,
		a_scope_depth			*p_definition_depth,
		a_scope_depth			*p_context_depth,
                a_scope_depth			*p_after_definition_depth,
                a_symbol_ptr			instance_sym,
                a_type_ptr			assoc_type,
		a_routine_ptr			assoc_routine)
/*
Pushes the scopes necessary to create the appropriate context for a
particular instantiation.  This process includes

- pushing the namespace containing the point of instantiation for the template

- pushing the namespaces necessary to recreate the context in which the
  template was defined, up to the "common" scope (the first scope that is
  part of both the definition and referencing context)

- pushing any class reactivation scopes that may be necessary if the template
  definition context is inside a class

- pushing instantiation scopes associated with the class reactivations if the
  class being reactivated is a class template

definition_nsp is the template definition namespace to be extended.
definition_class is the template definition class to be reactivated.
reference_nsp is the referencing namespace to be reactivated.
p_common_depth is set to the scope depth of the first scope that is common
to both the defining and referencing contexts.  p_definition_depth is
set to the innermost scope that is part of the template definition
context.  p_context_depth is set to the scope depth of the referencing
namespace.  When referencing_nsp is NULL, this is the current scope depth,
otherwise it is the depth of the scope pushed for referencing_nsp.
p_after_definition_depth is the scope depth of the scope whose previous
scope needs to be updated to point to definition_depth when the scope
stack is fixed up later.  instance_sym is the symbol to be used for
the outermost class instantiation scope and is non-NULL for nontemplate
member instantiations when the outermost instantiation scope is actually
the instantiation scope for the member, not the class that is being
reactivated.  Likewise, assoc_type and assoc_routine are non-NULL when
they should be used for the outermost instantiation scope.
*/
{
  a_scope_depth		common_depth;
  a_scope_depth		context_depth;
  a_scope_depth		definition_depth;
  a_namespace_ptr	common_nsp;
  a_scope_depth		depth_of_first_context_scope = NO_SCOPE_DEPTH;

  /* Push the namespace containing the point of instantiation. */
  if (reference_nsp !=
              scope_stack[depth_innermost_namespace_scope].assoc_namespace ||
      depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    /* The namespace that contains the first reference of this template
       that requires its instantiation is different than the current
       namespace.  If we are already in an instantiation we force the
       referencing namespace to be repushed because the earlier context
       may include instantiation scopes that should not be considered.
       Reactivate the namespace associated with that reference.  We must
       force a new entry to be pushed here to make sure that the depth of
       the first context scope is accurate. */
    if (reference_nsp != NULL) {
      depth_of_first_context_scope = depth_scope_stack + 1;
      f_push_namespace_extension_scope(reference_nsp,
                                       /*force_new_entry=*/TRUE);
      check_assertion(depth_scope_stack >= depth_of_first_context_scope);
    }  /* if */
  }  /* if */
  /* The context scope is the innermost namespace scope at this point,
     except when the referencing namespace is the file scope (because
     the file scope cannot be reactivated). */
  if (reference_nsp != NULL) {
    context_depth = depth_innermost_namespace_scope;
  } else {
    context_depth = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* Push the namespace(s) containing the definition of the template. */
  if (definition_nsp != NULL) {
    /* The common depth is the innermost scope that is both part of the
       context and part of the definition scope.  The normal processing
       for this assumes that the referencing context was pushed above,
       which is not the case for the file scope.  When the referencing
       context is the file scope, the common scope is also the file scope.
       Don't try to use common scopes that may have been pushed by a
       previous instantiation scope. */
    if (reference_nsp == NULL ||
        depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
      common_depth = DEPTH_OF_FILE_SCOPE;
    } else {
      common_depth = find_depth_of_common_scope(definition_nsp);
    }  /* if */
    common_nsp = scope_stack[common_depth].assoc_namespace;
    if (common_nsp == definition_nsp) {
      if (common_depth != depth_scope_stack) {
        definition_depth = common_depth;
      } else {
        definition_depth = depth_scope_stack;
      }  /* if */
    } else {
      /* Reactivate the scope from the common namespace scope through the
         parent namespace of the template. */
      push_namespace_extension_for_instantiation(definition_nsp, common_nsp,
                                                 common_depth);
      definition_depth = depth_scope_stack;
    }  /* if */
  } else {
    common_depth = DEPTH_OF_FILE_SCOPE;
    common_nsp = NULL;
    definition_depth = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* If we pushed some context scopes, reset the previous scope of the first
     context scope so that its previous scope is the file scope.
     Strictly speaking, this shouldn't be necessary, but is done for safety.
     When a context scope is pushed, the instantiation_context_lookup is used
     to correctly inspect the definition, context, and common scopes.  In
     other words, when the previous scope is not already the file scope,
     the previous scope pointer of this scope shouldn't be used. */
  if (depth_of_first_context_scope != NO_SCOPE_DEPTH) {
    scope_stack[depth_of_first_context_scope].previous_scope =
                                                           DEPTH_OF_FILE_SCOPE;
  }  /* if */
  /* The next scope pushed needs to have its previous scope set to the
     definition depth.  Save the depth of the scope that will need to
     be fixed up later. */
  *p_after_definition_depth = depth_scope_stack + 1;
  if (definition_class != NULL) {
    /* This template was declared within a class, or classes (either a normal
       class and/or a template class).  Push class reactivation scopes for the
       enclosing classes.  For any enclosing classes that are template classes,
       push an instantiation scope for the class as well. */
    reactivate_class_and_instantiation_scopes(decl_info, definition_class,
                                              instance_sym, assoc_type,
                                              assoc_routine);
  }  /* if */
  /* Return the calculated scope depths to the caller. */
  *p_common_depth = common_depth;
  *p_definition_depth = definition_depth;
  *p_context_depth = context_depth;
}  /* push_instantiation_context */


static void fixup_instantiation_scopes(a_scope_depth	orig_depth,
				       a_scope_depth	common_depth,
				       a_scope_depth	definition_depth,
				       a_scope_depth	context_depth,
                                       a_scope_depth	after_definition_depth)
/*
Fix up the entries on the scope stack.  At this point the scope stack
looks like:

	7 instantiation scope for this entity
	6 reactivation for class containing template (optional)
	5 instantiation for class containing template (optional)
	  (any number of classes may be reactivated)
	4 namespace containing template (optional)
	  (any number of namespaces may be pushed)
	3 namespace containing point of instantiation (optional)
	2 scope containing reference that caused the instantiation
	1 some scope
	0 file scope

The following fixups need to be performed:

- all instantiation scopes pushed, except the first one, must be
  flagged as nested instantiations
- the context and common scopes must be recorded in the nonnested
  instantiation scope
- the previous_scope of the outermost definition context scope and the
  referencing context scope must be set to point to the common scope
*/
{
  a_scope_depth			primary_instantiation_depth = NO_SCOPE_DEPTH;
  a_scope_depth			depth;
  a_scope_stack_entry_ptr	primary_ssep;
  a_boolean			exclude_from_context_output = FALSE;

  /* Mark all instantiation scopes that have been pushed as nested
     instantiations.  Save the depth of the outermost instantiation
     scope that was pushed.  It will be changed back to nonnested later.
     When this loop completes, primary_instantiation_depth will
     mark the first instantiation scope pushed.  All instantiation
     scopes pushed as part of this set, except the innermost one,
     should have the "exclude from context output" flag set so that
     only the context information associated with the innermost scope
     will be included in error output. */
  for (depth = depth_scope_stack; depth > orig_depth; depth--) {
    a_scope_stack_entry_ptr	ssep = scope_stack_entry_for(depth);
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      primary_instantiation_depth = depth;
      ssep->nested_instantiation = TRUE;
      ssep->exclude_from_context_output = exclude_from_context_output;
      exclude_from_context_output = TRUE;
    }  /* if */
    if (is_scope_kind_that_affects_access_control(ssep->kind)) {
      /* If the next_scope_that_affects_access_control field points to
         a scope that is not part of the instantiation context, set it
         to NO_SCOPE_DEPTH because it should not be considered for purposes
         of this instantiation. */
      if (ssep->next_scope_that_affects_access_control <= orig_depth) {
        ssep->next_scope_that_affects_access_control = NO_SCOPE_DEPTH;
      }  /* if */
    }  /* if */
  }  /* for */
  primary_ssep = &scope_stack[primary_instantiation_depth];
  /* Mark the initial instantiation scope as nonnested. */
  primary_ssep->nested_instantiation = FALSE;
  primary_ssep->instantiation_context_depth = context_depth;
  primary_ssep->instantiation_common_depth = common_depth;
  /* The previous_scope of the initial definition context scope will have
     already been set properly. */
  /* The scope pushed after the definition namespace context has been
     restored needs to have its previous scope field updated to point
     to the definition context. */
  scope_stack[after_definition_depth].previous_scope = definition_depth;
}  /* fixup_instantiation_scopes */


static void push_simple_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_push_scope_options_set	options)
/*
Push a template instantiation scope, but not all of the surrounding context
scopes.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			saved_innermost_scope_that_affects_access;

  saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
 (void)push_scope_full((a_scope_kind)sck_template_instantiation,
                        decl_info->declaration_scope, assoc_type,
                        assoc_routine, (a_namespace_ptr)NULL, instance_sym,
                        template_sym, template_arg_list, decl_info, options);
  ssep = scope_stack_entry_for(depth_scope_stack);
  /* Template instantiation scopes need to record the scope depth before
     the set of scopes that represent the instantiation is pushed.
     The previous saved innermost access scope must also be recorded. */
  ssep->orig_depth = depth_scope_stack-1;
  ssep->saved_innermost_scope_that_affects_access =
                                     saved_innermost_scope_that_affects_access;
  /* Set the instantiation context depth to the enclosing scope.  Other parts
     of the front end require this to be set. */
  ssep->instantiation_context_depth = depth_scope_stack-1;
}  /* push_simple_instantiation_scope */


static a_boolean is_nested_in_prototype_instantiation(
                            a_symbol_ptr		template_sym)
/*
See if this is an instantiation scope nested within a prototype
instantiation.  This could be the prototype instantiation of a member
template that is defined inside of the enclosing template.  It could
also be a partial instantiation of a member function template in a
prototype instantiation (this can occur when a Microsoft mode in-class
specialization of a member class template is declared).  In such cases
the prototype instantiation of the enclosing template is still in
progress.  When processing an instantiation nested within a prototype
instantiation we simply push a new template instantiation scope onto
the existing context and flag it as a nested instantiation.

The caller is responsible for checking that the current scope has
its "in_prototype_intantiation" flag set.

template_sym is the template that is being instantiated.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_stack_entry_ptr	instantiation_ssep;
  a_type_ptr			assoc_type = NULL;
  a_boolean			result = FALSE;

  /* Find the innermost instantiation scope, but stop searching
     if we're inside a function. */
  instantiation_ssep = &scope_stack[depth_innermost_instantiation_scope];
  for (ssep = &scope_stack[depth_scope_stack]; ssep != instantiation_ssep;
       ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_function) {
      ssep = NULL;
      break;
    } else if (assoc_type == NULL) {
      /* Save the type of the first class scope that we encounter. */
      if (ssep->kind == (a_scope_kind)sck_class_struct_union ||
          ssep->kind == (a_scope_kind)sck_class_reactivation) {
        assoc_type = ssep->assoc_type;
      }  /* if */
    }  /* if */
  }  /* for */
  if (assoc_type != NULL) {
    /* If the class being instantiated is the same as the parent of the
       template to be instantiated, this is nested in the enclosing
       prototype instantiation. */
    if (template_sym->is_class_member &&
        same_entities(template_sym->parent.class_type, assoc_type)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_nested_in_prototype_instantiation */


void push_template_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_boolean			push_stop_tokens,
			    a_push_scope_options_set	options)
/*
Interface to push_scope_full that is used for template instantiation
scopes.  If push_stop_tokens is TRUE, a new stop token stack entry
is pushed here, and popped when the instantiation scope is popped.
*/
{
  a_namespace_ptr		parent_nsp;
  a_type_ptr			parent_class;
  a_scope_depth			common_depth;
  a_scope_depth			new_innermost_namespace_scope;
  a_scope_depth			context_depth;
  a_scope_depth			definition_depth;
  a_scope_depth			after_definition_depth;
  a_scope_depth			orig_depth = depth_scope_stack;
  a_scope_depth			saved_innermost_scope_that_affects_access;
  a_namespace_ptr		reference_nsp;
  a_boolean			is_template = FALSE;
  a_template_decl_info_ptr	enclosing_tdip;
  a_symbol_ptr			enclosing_instance_sym;
  a_type_ptr			enclosing_assoc_type;
  a_routine_ptr			enclosing_assoc_routine;
  a_boolean			nested_in_prototype_instantiation = FALSE;

  /* Clear the flag that indicates that we are in a local class so that any
     scopes pushed by this routine will not be indicated as being within
     a local class.  This will be restored to the correct state when the
     last scope pushed by this routine is popped.  The same is done for
     the flag that indicates whether we are within a function scope. */
  inside_local_class = FALSE;
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
  innermost_function_scope = NULL;
  saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
  /* Make sure we are in the right translation unit. */
  check_assertion_str2(symbol_is_from_trans_unit(template_sym,
                                                 curr_translation_unit),
                       "push_template_instantiation_scope:",
                       "wrong translation unit");
  /* Determine whether the bottom-level entity is a template.  In some
     cases the only instantiation scopes that are pushed are ones
     that make up the context for the definition of an entity.  This
     is the case for any nontemplate entity (member function, class, etc.)
     defined within a template.  If this routine will push an instantiation
     scope, get a pointer to the enclosing template declaration information
     to be passed to the routine that pushes the context scopes. */
  is_template = template_sym->kind == (a_symbol_kind)sk_class_template ||
                template_sym->kind == (a_symbol_kind)sk_function_template;
  if (is_template) {
    /* Get a pointer to the enclosing template declaration information.  If
       this pointer is NULL, the template declaration information from the
       template symbol supplement will be used instead. */
    enclosing_tdip = decl_info->enclosing_template_decl;
    enclosing_instance_sym = NULL;
    enclosing_assoc_type = NULL;
    enclosing_assoc_routine = NULL;
  } else {
    /* This entity is not a template, use the template declaration information
       that was passed in. */
    enclosing_tdip = decl_info;
    /* For nontemplate members of template classes use the symbol for the
       nontemplate member that is being instantiated as the instance
       symbol for the outermost instantiation scope. */
    enclosing_instance_sym = instance_sym;
    enclosing_assoc_type = assoc_type;
    enclosing_assoc_routine = assoc_routine;
  }  /* if */
  /* Determine whether this instantiation is a prototype instantiation of
     something within another prototype instantiation.  This affects the
     way that the scope stack is manipulated. */
  if (scope_stack[depth_scope_stack].in_prototype_instantiation &&
      ((options & PS_PROTOTYPE_INSTANTIATION) != 0 ||
       (options & PS_NONREAL_INSTANTIATION) != 0)) {
    nested_in_prototype_instantiation = is_nested_in_prototype_instantiation(
                                                                 template_sym);
  }  /* if */
  if (!nested_in_prototype_instantiation) {
    /* If the template was defined in a namespace, reactivate the namespace
       scope before pushing the instantiation scope. */
    get_parent_information_for_template(decl_info->enclosing_scope,
                                        template_sym, instance_sym,
                                        &parent_nsp, &parent_class);
    reference_nsp = referencing_namespace_for_instance(instance_sym);
    push_instantiation_context(enclosing_tdip, parent_nsp, parent_class,
                               reference_nsp, &common_depth, &definition_depth,
                               &context_depth, &after_definition_depth,
                               enclosing_instance_sym, enclosing_assoc_type,
                               enclosing_assoc_routine);
    /* At this point, definition_depth points to the parent scope
       of the template being instantiated.  Save this value before it
       is potentially modified below. */
    new_innermost_namespace_scope = definition_depth;
  }  /* if */
  if (is_template) {
    (void)push_scope_full((a_scope_kind)sck_template_instantiation,
                          decl_info->declaration_scope, assoc_type,
                          assoc_routine, (a_namespace_ptr)NULL, instance_sym,
                          template_sym, template_arg_list, decl_info, options);
  }  /* if */
  if (!nested_in_prototype_instantiation) {
    a_scope_stack_entry_ptr	ssep;
    /* Update the scope stack entries that have been pushed so that the
       special instantiation context lookups can be done correctly. */
    fixup_instantiation_scopes(orig_depth, common_depth, definition_depth,
                               context_depth, after_definition_depth);
    /* Update the depth of the innermost instantiation scope so that it points
       to the namespace that is the parent of the template being
       instantiated. */
    ssep = &scope_stack[depth_scope_stack];
    depth_innermost_namespace_scope =
         ssep->depth_innermost_namespace_scope = new_innermost_namespace_scope;
    /* Because a template instantiation introduces a new context for
       name lookup purposes, we need to clear the active using list
       flags for any namespaces for which it is currently set. */
    set_active_using_list_scope_depths(depth_scope_stack,
                                       /*set_value=*/FALSE,
                                       NO_DECL_SEQUENCE_NUMBER);
    /* Set the active using flags for the newly created context. */
    set_active_using_list_scope_depths(depth_scope_stack, /*set_value=*/TRUE,
                                       decl_info->decl_seq);
    check_assertion(scope_stack[new_innermost_namespace_scope].assoc_namespace
                                                                == parent_nsp);
  } else {
    /* A nested prototype instantiation must be flagged as a nested
       instantiation. */
    scope_stack[depth_scope_stack].nested_instantiation = TRUE;
  }  /* if */
  { a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];
    /* Save the original scope depth in the last scope pushed by this
       routine.  This will be used later when popping the stack. */
    ssep->orig_depth = orig_depth;
    /* Save the original value of the depth of the innermost scope that affects
       access control.  This is necessary because the scope fixup routine
       may adjust some of the next scope that affects access control links
       in the scope stack resulting in an incorrect value for the global
       variable after the instantiation scopes have been popped. */
    ssep->saved_innermost_scope_that_affects_access =
                                    saved_innermost_scope_that_affects_access;
    if (push_stop_tokens) {
      /* Start a new stop token context for the instantiation. */
      push_stop_token_stack();
      ssep->stop_token_stack_pushed = TRUE;
    }  /* if */
  }
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("instantiation_scope")) {
    fprintf(f_debug, "Pushed instantiation scope for: ");
    db_symbol(instance_sym, "", 0);
    if (!nested_in_prototype_instantiation) {
      fprintf(f_debug, "context_depth=%d, common_depth=%d\n", context_depth,
              common_depth);
    }  /* if */
    fprintf(f_debug, "scope stack after instantiation scope:\n");
    db_scope_stack();
  }  /* if */
#endif /* DEBUG */
}  /* push_template_instantiation_scope */


void pop_template_instantiation_scope(void)
/*
Interface to pop_scope that is used for template instantiation scopes.
Pops all of the scopes that were pushed by a given call to
push_template_instantiation_scope.
*/
{
  a_scope_depth			orig_depth;
  a_scope_depth			saved_innermost_scope_that_affects_access;

  orig_depth = scope_stack[depth_scope_stack].orig_depth;
  saved_innermost_scope_that_affects_access =
      scope_stack[depth_scope_stack].saved_innermost_scope_that_affects_access;
  check_assertion_str2(orig_depth != NO_SCOPE_DEPTH,
                       "pop_template_instantiation_scope:",
                       "invalid orig_depth");
  if (scope_stack[depth_scope_stack].stop_token_stack_pushed) {
    /* Restore the original stop token context. */
    pop_stop_token_stack();
  }  /* if */
  /* Pop scopes until the depth of the scope stack is equal to orig_depth,
     which is the depth before any of the instantiation context scopes were
     pushed. */
  while (orig_depth < depth_scope_stack) pop_scope();
  /* Restore the original value of the depth of the innermost scope that
     affects access control.  This is necessary because the scope fixup
     routine used when an instantiation scope is pushed may adjust some
     of the next scope that affects access control links in the scope stack
     resulting in an incorrect value for the global variable after the
     instantiation scopes have been popped. */
  depth_of_innermost_scope_that_affects_access_control =
                                    saved_innermost_scope_that_affects_access;
  /* Reset the active using list flags to the values specified by
     the previous scope stack entries. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/TRUE,
                                     get_effective_decl_seq());
}  /* pop_template_instantiation_scope */


void push_template_declaration_scope(a_template_decl_info_ptr decl_info)
/*
Push a template declaration scope.
*/
{
  (void)push_scope_full((a_scope_kind)sck_template_declaration,
                        NO_SCOPE_NUMBER,
                        (a_type_ptr)NULL, (a_routine_ptr)NULL,
                        (a_namespace_ptr)NULL, (a_symbol_ptr)NULL,
                        (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL,
                        decl_info, PS_NO_OPTIONS);
}  /* push_template_declaration_scope */


#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
static
a_boolean check_for_file_scope_type_with_same_name(a_symbol_ptr sym_to_find)
/*
This routine is used to implement the "transitional model" for nested types.
Given a symbol this routine looks for a file scope type (class, struct, union,
typedef, or enum) with the same name.  Returns TRUE if one is found, FALSE
otherwise.
*/
{
  a_symbol_ptr		sym;
  a_boolean		found = FALSE;
  a_symbol_header_ptr	sym_header;

  sym_header = sym_to_find->header;
  /* Get the symbol list on which file scope symbols can be found. */
  sym = symbol_list_for_file_scope_symbols(sym_header);
  while (sym != NULL) {
    if (sym->decl_scope == file_scope_number) {
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


static
a_symbol_ptr find_cfront_transitional_nested_type_symbol
                                             (a_symbol_ptr sym_to_find)
/*
Given a symbol looks through the inactive list for a type symbol
of the same name whose type has the transitional name mangling flag set.
This is used for error generation of the transitional model for nested
type support.
*/
{
  a_symbol_ptr  sym;

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
  return sym;
}  /* find_cfront_transitional_nested_type_symbol */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */


a_boolean routine_defined(a_routine_ptr	rp)
/*
Return TRUE if a definition has been supplied for the routine pointed to
by "rp".  If "rp" is a template instance that has not been explicitly
specialized, return TRUE if a definition has been supplied for the
associated template.
*/
{
  a_boolean	result = FALSE;

  if (rp->is_template_function &&
      !rp->is_specialized && !rp->compiler_generated) {
    a_template_symbol_supplement_ptr	tssp;
    a_symbol_ptr			sym;
    a_symbol_ptr			template_sym;
    sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
    check_assertion(sym != NULL);
    template_sym = sym->variant.routine.instance_ptr->template_sym;
    tssp = template_supplement_for_symbol(template_sym);
    result = cache_for_template(tssp)->tokens.first_token != NULL;
#if GNU_EXTENSIONS_ALLOWED
  } else if (rp->aliased_routine != NULL) {
    /* Consider an alias defined if the entity it aliases is defined. */
    result = routine_defined(rp->aliased_routine);
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (routine_has_been_defined(rp)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* routine_defined */


static void check_referenced_member_functions(a_scope_ptr scope,
                                              a_boolean   is_function_local,
                                              a_boolean   within_unnamed_class)
/*
If scope is a class scope, issue an error for any of its member functions
that have been referenced but are undefined and lack external linkage (i.e.,
inline member functions and member functions of local classes); if the class
has nested classes, check their member functions, too.  If it is not a class
scope, apply that check to the member functions of each class declared in
the scope.  is_function_local is TRUE if this scope belongs to a function
body.  Only called in C++ mode.
*/
{
  a_routine_ptr            rp;
  a_type_ptr               tp;
  a_scope_ptr              class_scope;
  a_symbol_ptr             sym;
  a_template_instance_ptr  tip;
  a_boolean                is_inline_virtual;

  /* Examine each of the class types on the types list of the scope.  If
     this is a class scope, it picks up the nested classes. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp)) {
      class_scope = tp->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) {
        /* Check the member functions of the nested class. */
        check_referenced_member_functions(class_scope, is_function_local,
                                          (within_unnamed_class ||
                                           tp->variant.class_struct_union.
                                                         originally_unnamed));
      }  /* if */
    }  /* if */
  }  /* for */
  /* If this is a file, function, or block scope, do nothing more.  If it's
     a class scope, check its member functions. */
  if (scope->kind == (a_scope_kind)sck_class_struct_union) {
    /* Now go though each routine entry for the current class. */
    for (rp = scope->routines; rp != NULL; rp = rp->next) {
      sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
      if (!(routine_defined(rp) || (sym != NULL && sym->defined)) &&
          !rp->compiler_generated) {
        /* An undefined member function. */
        is_inline_virtual =
                    (rp->is_inline && rp->is_virtual && !rp->pure_virtual);
        if (rp->source_corresp.referenced || is_inline_virtual) {
          /* Either the function was actually referenced or could be
             referenced using the virtual function call mechanism. */
          if (sym != NULL) {
            tip = sym->variant.routine.instance_ptr;
            if (tip != NULL &&
                (tip->explicit_instantiation ||
                 tip->suppress_instantiation)) {
              /* A template function that has either been explicitly
                 instantiated (in which case an error would have been issued
                 on the explicit instantiation attempt) or for which some
                 error that should prevent the instantiation has already been
                 issued. */
            } else if (within_unnamed_class && rp->is_virtual) {
              /* Diagnostic has already been issued. */
            } else if (is_function_local) {
              /* An undefined member function of a local class. */
              if (rp->is_virtual) {
                /* Diagnostic has already been put out. */
              } else {
                pos_sy_error(ec_local_class_function_def_missing,
                             &sym->decl_position, sym);
              }  /* if */
            } else if (rp->source_corresp.referenced &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                       !(microsoft_mode &&
                         (rp->decl_modifiers & DM_DLLIMPORT)) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                       (rp->is_inline ||
                        rp->storage_class != (a_storage_class)sc_extern)) {
              /* A referenced but undefined member function that is either
                 extern-inline or has internal linkage. */
              pos_sy_diagnostic(microsoft_mode ? es_warning : es_error,
                                ec_never_defined, &sym->decl_position, sym);
            } else if (is_inline_virtual) {
              /* A non-local inline virtual function that is undefined and
                 unreferenced. */
              if (strict_ansi_mode) {
                /* In strict mode this is an error, because simply being
                   declared virtual counts as a use, and inline functions
                   that are used must be defined. */
                pos_sy_diagnostic(strict_ansi_discretionary_severity,
                                  ec_virtual_inline_never_defined,
                                  &sym->decl_position, sym);
#if DO_IL_LOWERING
              } else if (inline_virtual_function_definitions_needed(
                                                 scope->variant.assoc_type)) {
                /* The virtual function table will be generated for the
                   class in this translation unit, so the function's
                   address will be taken.  Since there's no definition,
                   this will usually result in a linker error, so the user
                   may benefit from an earlier diagnostic. */
                pos_sy_warning(ec_virtual_inline_never_defined,
                               &sym->decl_position, sym);
#endif /* DO_IL_LOWERING */
              } else {
                /* By default we issue no diagnostic. */
              }  /* if */
              /* Modify the routine entry so that the IL will be valid.
                 (Otherwise, for example, if a virtual function table is put
                 out for the class of which this function is a member, there
                 may appear to be a reference to an undefined function with
                 static storage class.  This way, a linker error will be
                 forced.) */
              rp->storage_class = (a_storage_class)sc_extern;
              rp->source_corresp.name_linkage =
                               (a_name_linkage_kind)nlk_cplusplus_external;
              rp->is_inline = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
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
  a_type_ptr      var_type;
  a_variable_ptr  var_ptr;
  a_routine_ptr   rout_ptr;
#if CHECKING
  a_source_correspondence  *scp = NULL;
#endif /* CHECKING */

  switch (sym->kind) {
    case sk_variable:
      /* Variable or parameter. */
      var_ptr = sym->variant.variable.ptr;
      var_type = skip_typerefs(var_ptr->type);
      storage_class = var_ptr->storage_class;
      if (storage_class == (a_storage_class)sc_unspecified &&
          (!is_member_of_unnamed_namespace(&var_ptr->source_corresp) ||
           var_ptr->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external)) {
        /* Note that if this test succeeds (i.e., the variable has
           storage class sc_unspecified), we do not do the test for
           referenced.  That's because an external variable can be assumed
           to be referenced from another compilation unit.  The referenced
           flag in the IL entry is set slightly later, in the
           sk_extern_variable processing. */
      } else if (storage_class == (a_storage_class)sc_extern) {
        /* Usually, we issue no warning for unused "extern" variables; this is
           a long-standing C convention.  In unnamed namespaces, however, a
           warning is issued for unused declarations, and an error for missing
           definitions that were referenced. */
        if (is_member_of_unnamed_namespace(&var_ptr->source_corresp)) {
          if (sym->referenced) {
            pos_sy_error(ec_never_defined, &sym->decl_position, sym);
          } else {
            report_unreferenced(sym, ec_declared_but_not_referenced,
                                es_warning);
          }  /* if */
        }  /* if */
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
            report_unreferenced(sym, ec_unreferenced_function_param,
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
      } else if ((!sym->referenced ||
                  (sym->variant.variable.value_has_been_set &&
                   !sym->variant.variable.used)) &&
#if GNU_EXTENSIONS_ALLOWED
                 !var_type->variables_are_implicitly_referenced &&
                 var_ptr->section == NULL &&
                 !var_ptr->has_gnu_unused_attribute &&
#endif /* GNU_EXTENSIONS_ALLOWED */
                 !(is_immediate_class_type(var_type) &&
                   (var_type->variant.class_struct_union.is_nonreal_class ||
                    symbol_supplement_for_class(var_type)->
                                              any_template_dependent_fields ||
                    symbol_supplement_for_class(var_type)->
                                                 any_nonreal_base_classes))) {
        /* An unreferenced or unused variable or an unused parameter.
           If a class is nonreal or if it has a template-dependent field or
           base, it may yet have side effects and no diagnostic should be
           issued.  In GNU C, variables with internal linkage are concatenated
           within their section, which is sometimes used to create link-time
           chains. */
        a_boolean           suppress_warning;
        an_error_code       error_code;
        an_error_severity   severity = es_warning;
        an_init_kind        init_kind;
        an_initializer_ptr  ip;

        /* Check for a dynamic initialization that has side effects (such as
           a constructor call).  If such an initialization exists, suppress
           the warning. */
        get_variable_initializer(var_ptr,
                                 scope_stack[depth_scope_stack].il_scope,
                                 &init_kind, &ip);
        if (init_kind == (an_init_kind)initk_dynamic) {
          if (ip->dynamic->kind == (a_dynamic_init_kind)dik_constructor) {
            /* Issue no diagnostic when a variable is initialized by
               a constructor, to avoid spurious diagnostics when the user
               defines a variable simply to assure that the constructor is
               called. */
            severity = es_none;
          } else if (dynamic_init_has_side_effects(ip->dynamic,
                                                   &suppress_warning) ||
                     suppress_warning) {
            /* Initialization has side-effects -- issue a remark. */
            severity = es_remark;
          }  /* if */
        }  /* if */
        if (severity != es_none) {
          /* Issue different diagnostics depending on whether the variable
             was completely unreferenced or was set but not used. */
          if (!sym->referenced) {
            error_code = ec_declared_but_not_referenced;
          } else {
            check_assertion(sym->variant.variable.value_has_been_set);
            error_code = ec_set_but_not_used;
          }  /* if */
          report_unreferenced(sym, error_code, severity);
        }  /* if */
      }  /* if */
#if CHECKING
      scp = &var_ptr->source_corresp;
#endif /* CHECKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Do fixup on the source sequence entry for a tentative definition. */
      if (C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
          sym->defined && is_primary_translation_unit) {
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
      if (c99_mode && strict_ansi_mode &&
          storage_class == (a_storage_class)sc_extern &&
          rout_ptr->is_inline) {
        /* In C99, an inline function with external linkage must be
           defined in the current translation unit.  We require this only
           in strict mode.  Make sure we issue an error, not a warning,
           if the function is actually referenced. */
        pos_sy_diagnostic(rout_ptr->source_corresp.referenced ?
                                            es_discretionary_error :
                                            strict_ansi_discretionary_severity,
                          ec_inline_never_defined,
                          &sym->decl_position, sym);
      } else if (storage_class == (a_storage_class)sc_unspecified &&
                 (!is_member_of_unnamed_namespace(&rout_ptr->source_corresp) ||
                  rout_ptr->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external)) {
        /* Regard functions with "unspecified" storage class to be referenced
           somewhere, even if not in the current translation unit. extern
           inline functions are an exception, since their callability is
           "as if" they had static storage, but no warning is issued if they
           are unreferenced.  Note also that unnamed namespace members with
           extern "C" linkage can be referenced in other translation units. */
        if (!rout_ptr->is_inline) {
          rout_ptr->source_corresp.referenced = TRUE;
          sym->referenced = TRUE;
        }  /* if */
      } else if (rout_ptr->source_corresp.referenced) {
        /* Referenced function.  We check the IL referenced flag because
           a reference in, say, a sizeof operation doesn't count. */
        if ((storage_class == (a_storage_class)sc_static ||
             is_member_of_unnamed_namespace(&rout_ptr->source_corresp)) &&
            !routine_defined(rout_ptr)) {
          /* A routine with internal linkage or in an unnamed namespace was
             never given a definition.  For ordinary nontemplate functions we
             check the corresponding sk_extern_routine symbol; for template
             instances there is no such symbol and hence we check it here. */
          a_template_instance_ptr  tip = sym->variant.routine.instance_ptr;
          /* If an attempt was made to explicitly instantiate the function
             template, an error will have been issued already. */
          if (tip != NULL && !tip->explicit_instantiation) {
            pos_sy_error(ec_never_defined, &sym->decl_position, sym);
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_mode &&
                   (rout_ptr->decl_modifiers & DM_DLLIMPORT)) {
          /* No diagnostic. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (rout_ptr->is_inline &&
                   !routine_defined(rout_ptr)) {
          /* An extern-inline function that was referenced but not defined.
             Note that the Microsoft and GNU compilers issue no diagnostic on
             this (though linker errors may result from this). */
          pos_sy_diagnostic((microsoft_mode || gnu_mode) ? es_warning
                                                         : es_error,
                            ec_extern_inline_never_defined,
                            &sym->decl_position, sym);
        }  /* if */
      } else if (!sym->referenced) {
        /* Unreferenced function. */
        if (storage_class == (a_storage_class)sc_extern &&
            !is_member_of_unnamed_namespace(&rout_ptr->source_corresp)) {
          /* No warning on unused "extern" routines; this is a long-standing
             C tradition.  An exception are routines in unnamed namespaces. */
        } else if (rout_ptr->is_inline && sym->defined &&
                   seq_is_in_include_file(sym->decl_position.seq)) {
          /* No diagnostic on inline non-member functions defined in a header
             file. */
#if GNU_EXTENSIONS_ALLOWED
        } else if (rout_ptr->has_gnu_unused_attribute) {
          /* Do not diagnose an unused function that carries the "unused"
             attribute. */
#endif /* GNU_EXTENSIONS_ALLOWED */
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
              check_assertion(!has_unknown_specified_bound(array_type));
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
    case sk_extern_routine:
       /* Check for a routine with internal linkage (or from an unnamed
          namespace) that is referenced but not defined (3.7, constraints).
          This is checked only at the file scope because there can be symbols
          with linkage defined in inner scopes, but only the file scope
          declaration can have a body.
          The template instance case is handled under sk_routine because no
          sk_extern_routine symbol is created for those.  For nontemplates the
          sk_routine symbol might have been removed too early in the case of a
          block-scope declaration in an unnamed namespace. */
      rout_ptr = sym->variant.extern_symbol_descr->variant.routine.ptr;
      if (rout_ptr->source_corresp.referenced) {
        /* Referenced function.  We check the IL referenced flag because
           a reference in, say, a sizeof operation doesn't count. */
        if ((rout_ptr->storage_class == (a_storage_class)sc_static ||
             is_member_of_unnamed_namespace(&rout_ptr->source_corresp)) &&
             !routine_defined(rout_ptr)) {
          if (C_dialect == C_dialect_pcc) {
            /* In pcc mode, just change the routine to extern. */
            rout_ptr->storage_class = (a_storage_class)sc_extern;
            rout_ptr->source_corresp.name_linkage =
                                         (a_name_linkage_kind)nlk_external;
          } else {
            pos_sy_error(ec_never_defined, &sym->decl_position, sym);
          }  /* if */
        }  /* if */
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
      scp = &sym->variant.type.ptr->source_corresp;
      break;
    case sk_enum_tag:
      /* Struct, union, or enum tag. */
      scp = &type_symbol_type(sym)->source_corresp;
      break;
#endif /* CHECKING */
    case sk_class_or_struct_tag:
    case sk_union_tag:
      if (is_member_of_unnamed_namespace(
                                    &type_symbol_type(sym)->source_corresp)) {
        /* Member functions and static data members of classes declared in
           unnamed namespaces can be checked: they should be defined if used,
           and they're useless if not used. */
        a_type_ptr      type = type_symbol_type(sym);
        a_class_type_supplement_ptr
                        ctsp = type->variant.class_struct_union.extra_info;

        if (ctsp->assoc_scope != NULL) {
          /* A class definition was provided. */
          a_routine_ptr   rp = ctsp->assoc_scope->routines;
          a_variable_ptr  vp = ctsp->assoc_scope->variables;
          /* Diagnose undefined and unused member functions: */
          for (; rp != NULL; rp = rp->next) {
            if (rp->source_corresp.referenced ||
                (rp->is_virtual && !rp->pure_virtual &&
                 !rp->compiler_generated)) {
              /* Virtual functions are in some way always "referenced" by the
                 virtual function table, but pure virtual functions and
                 compiler generated virtual functions (destructors) do not
                 always need to have a definition. */
              if (!routine_defined(rp)) {
                pos_sy_error(ec_never_defined,
                             &rp->source_corresp.decl_position,
                             (a_symbol_ptr)rp->source_corresp.assoc_info);
              }  /* if */
            } else if (!rp->source_corresp.referenced &&
                       !rp->compiler_generated &&
                       !rp->is_virtual &&
                       /* Don't warn about members that might be declared
                          to avoid compiler generated declarations. */
                       !((rp->special_kind ==
                                  (a_special_function_kind)sfk_constructor ||
                          rp->special_kind ==
                                  (a_special_function_kind)sfk_destructor ||
                          (rp->special_kind ==
                                  (a_special_function_kind)sfk_operator && 
                           rp->variant.opname_kind == 
                                  (an_opname_kind)onk_assign)) &&
                         !routine_defined(rp))) {
              report_unreferenced((a_symbol_ptr)rp->source_corresp.assoc_info,
                                  ec_declared_but_not_referenced,
                                  es_warning);
            }  /* if */
          }  /* for */
          /* Diagnose undefined and unused static data members: */
          for (; vp != NULL; vp = vp->next) {
            if (vp->source_corresp.referenced &&
                vp->storage_class == (a_storage_class)sc_extern &&
                !vp->is_member_constant) {
              pos_sy_error(ec_never_defined,
                           &vp->source_corresp.decl_position,
                           (a_symbol_ptr)vp->source_corresp.assoc_info);
            } else if (!vp->source_corresp.referenced) {
              report_unreferenced((a_symbol_ptr)vp->source_corresp.assoc_info,
                                  ec_declared_but_not_referenced,
                                  es_warning);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
#if CHECKING
      scp = &type_symbol_type(sym)->source_corresp;
#endif /* CHECKING */
      break;
    case sk_class_template:
      {
      a_template_symbol_supplement_ptr  tssp;
      a_symbol_ptr                      template_class_sym;
      tssp = sym->variant.template_info;
      template_class_sym = tssp->variant.class_template.instantiations;
      for (; template_class_sym != NULL;
             template_class_sym = next_instance_sym(template_class_sym)) {

        if (template_class_sym->variant.class_struct_union.type->
                                variant.class_struct_union.is_nonreal_class) {
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
        if (tip->is_guiding_decl) {
          /* A user declaration was provided, so the associated symbol should
             be on the overload list -- ignore it here. */
        } else {
          end_of_scope_symbol_check(tip->instance_sym, curr_routine);
        }  /* if */
      }  /* for */
      }
      break;
    default:
      /* No processing for other kinds. */
      break;
  }  /* switch */
#if CHECKING
  if (scp != NULL) {
    if (sym->is_class_member == scp->is_class_member &&
        (!sym->is_class_member ||
         sym->parent.class_type == scp->parent.class_type)) {
      /* Okay */
    } else if (scp->is_class_member &&
               !has_name_before_mangling(scp->parent.class_type)) {
      /* Okay: probably a member of a possibly nonstandard anonymous union. */
    } else if (sym->kind == (a_symbol_kind)sk_type &&
               sym->variant.type.is_injected_class_name) {
      /* The symbol for the injected class name points to the class of which
         it's a member, which is also its parent. */
    } else {
      unexpected_condition_str2("end_of_scope_symbol_check:",
                                "sym/il-entry parent-class mismatch");
    }  /* if */
  }  /* if */
#endif /* if */
}  /* end_of_scope_symbol_check */


/*
Available list of entries of type a_name_hidden_by_old_for_init.
*/
static a_name_hidden_by_old_for_init_ptr avail_names_hidden_by_old_for_init;


static 
a_name_hidden_by_old_for_init_ptr alloc_name_hidden_by_old_for_init(void)
/*
Return an entry of type a_name_hidden_by_old_for_init, with its fields
cleared, either allocating a brand new one or taking one from the available
list.
*/
{
  a_name_hidden_by_old_for_init_ptr  nhp;

  if (avail_names_hidden_by_old_for_init == NULL) {
    nhp = (a_name_hidden_by_old_for_init_ptr)
                            alloc_fe(sizeof(a_name_hidden_by_old_for_init));
  } else {
    nhp = avail_names_hidden_by_old_for_init;
    avail_names_hidden_by_old_for_init = nhp->next;
  }  /* if */
  nhp->next = NULL;
  nhp->symbol = NULL;
  nhp->for_init_decl_sym = NULL;
  nhp->already_hidden = FALSE;
  return nhp;
}  /* alloc_name_hidden_by_old_for_init */


static void free_names_hidden_by_old_for_init(
                                      a_name_hidden_by_old_for_init_ptr  nhp)
/*
Return each of the entries of type a_name_hidden_by_old_for_init on the list
headed by nhp to the available list.  Update the hidden_by_old_for_init
flag in the associated symbol.
*/
{
  a_name_hidden_by_old_for_init_ptr  next_nhp;

  while (nhp != NULL) {
    next_nhp = nhp->next;
    /* Update the hidden_by_old_for_init flag in the associated symbol. */
    nhp->symbol->hidden_by_old_for_init = nhp->already_hidden;
    nhp->symbol = NULL;
    nhp->for_init_decl_sym = NULL;
    /* Return the entry to the available list. */
    nhp->next = avail_names_hidden_by_old_for_init;
    avail_names_hidden_by_old_for_init = nhp;
    nhp = next_nhp;
  }  /* while */
}  /* free_names_hidden_by_old_for_init */


static void record_names_hidden_by_old_for_init(a_symbol_ptr for_init_decl)
/*
for_init_decl is head of a list of symbols declared in a for-init block scope
that has just been popped of the scope stack -- the scoping rules governing
these symbols are the standard rules.  build a list of names that would have
been hidden in the current scope under the old rules but are visible under
the new rules.  This list can be used to issue diagnostics -- to avoid
silently giving programs different behavior than they had under the old
(cfront-style) rules.
*/
{
  a_scope_stack_entry_ptr            ssep = &scope_stack[depth_scope_stack];
  a_symbol_ptr                       previously_hidden_sym;
  a_symbol_locator                   locator;
  a_name_hidden_by_old_for_init_ptr  nhp;

  check_assertion(ssep->kind == (a_scope_kind)sck_block ||
                  ssep->kind == (a_scope_kind)sck_function);
  while (for_init_decl != NULL) {
    /* Manufacture a locator to do a lookup. */
    make_locator_for_symbol(for_init_decl, &locator);
    clear_specific_symbol(locator);
    locator.specific_symbol = NULL;
    previously_hidden_sym = normal_id_lookup(&locator, IDL_NO_OPTIONS);
    if (previously_hidden_sym != NULL) {
      /* Lookup was successful. */
      if (previously_hidden_sym->decl_scope == ssep->number) {
        /* Ignore symbols found in the current scope. */
      } else {
        /* A symbol from a surrounding scope.  Be sure it hasn't already been
           entered. */
        nhp = ssep->names_hidden_by_old_for_init;
        for (; nhp != NULL; nhp = nhp->next) {
          if (nhp->symbol == previously_hidden_sym) break;
        }  /* for */
        if (nhp == NULL) {
          /* Create a name-[would-have-been-]hidden-by-old-for-init entry. */
          nhp = alloc_name_hidden_by_old_for_init();
          nhp->symbol = previously_hidden_sym;
          nhp->for_init_decl_sym = for_init_decl;
          /* Save the flag in the symbol so it can be restored later. */
          nhp->already_hidden = previously_hidden_sym->hidden_by_old_for_init;
          previously_hidden_sym->hidden_by_old_for_init = TRUE;
          /* Add the entry to the list for the current scope. */
          nhp->next = ssep->names_hidden_by_old_for_init;
          ssep->names_hidden_by_old_for_init = nhp;
        }  /* if */
      }  /* if */
    }  /* if */
    for_init_decl = for_init_decl->next_in_scope;
  }  /* while */
}  /* record_names_hidden_by_old_for_init */


void report_for_init_difference(a_symbol_ptr       sym,
                                a_source_position  *pos)
/*
The new for-init declaration scoping rules are in effect, and sym is a symbol
that (possibly) would not have been found with the old (cfront-compatible)
scoping rules.  If that's the case, issue a warning (so that users will not
be bitten by silent changes in how their programs behave).  This routine is
called only if global variable warning_on_for_init_difference is TRUE.
*/
{
  a_scope_depth                      depth = depth_scope_stack;
  a_scope_stack_entry_ptr            ssep;
  a_name_hidden_by_old_for_init_ptr  nhp;

  /* The outer loop marches up the scope stack until a match is found in
     a names-hidden-by-old-for-init list. */
  for (;;) {
    ssep = &scope_stack[depth];
    /* The inner loop goes through a list for a given scope. */
    nhp = ssep->names_hidden_by_old_for_init;
    for (; nhp != NULL; nhp = nhp->next) {
      if (nhp->symbol == sym) {
        /* A match is found. */
        break;
      }  /* if */
    }  /* for */
    /* If either a match was found or we've reached the end of the stack,
       stop looping. */
    if (nhp != NULL || depth == DEPTH_OF_FILE_SCOPE) break;
    /* Otherwise, continue though the scope stack. */
    depth = ssep->previous_scope;
  }  /* for */
  if (nhp != NULL) {
    /* Issue the diagnostic. */
    pos_sy2_warning(ec_hidden_by_old_for_init, pos, sym,
                    nhp->for_init_decl_sym);
#if CHECKING
  } else {
    /* No entry was found.  This should only happen inside a template
       instantiation, since the hidden_by_old_for_init flag in sym is not
       necessarily reliable.  Here's an example:
         int i = 13;
         template <class T> int g(T t) {
           return i;
         }
         main() {
           for (int i = 0; i < 10; ++i) { ... }
           (void)g(0);
         }
       After the for-loop, ::i has hidden_by_old_for_init set to TRUE, but
       that has no effect within the instantiation of g. */
    check_assertion_str(depth_innermost_instantiation_scope != NO_SCOPE_DEPTH,
                        "report_for_init_difference: entry not found");
#endif /* CHECKING */
  }  /* if */
}  /* report_for_init_difference */


static void nested_class_anachronism_processing(a_symbol_ptr symbol_list,
                                                a_boolean    do_tags,
                                                a_boolean    do_typedefs)
/*
This routine is called to process the symbols of a class scope to
determine whether any nested types should be visible for
nested class anachronism processing.  It is also used in
cfront 2.1 object compatibility mode to determine whether any of
the symbols should receive special treatment when generating
the mangled named for the nested type.  do_tags is TRUE if tag symbols
should be processed, otherwise they should be ignored.  do_typedefs
is TRUE if typedef symbols should be processed.  Both may be TRUE.
When cfront 2.1 object compatibility and cfront 2.1 mode are being used,
this routine is called twice.  Tag symbols are processed the first time,
and typedefs the second time.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbol_list; sym != NULL; sym = sym->next_in_scope) {
    /* Check for nested class/struct/unions on the inactive list.  If
       there are any, set the flag in the symbol header.  This is
       used to support the nonnested class anachronism.  We do not
       apply the anachronism to template classes. */
    if (((do_tags && is_tag_symbol(sym)) ||
         (do_typedefs && sym->kind == (a_symbol_kind)sk_type)) &&
         !is_injected_class_symbol(sym)) {
      sym->header->any_nested_types_on_inactive_list = TRUE;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
      /* Cfront 2.1 implements a special "transitional model" for nested
         types.  Under this model cfront promotes nested types to the file
         scope unless a file scope type of the same name is already
         defined.  Subsequent definition of additional nested types with
         the same name is an error.  This code, which simulates the
         cfront behavior, sets a flag for the first nested type with
         a given name and issues errors on subsequent definitions. */
      if (cfront_2_1_mode) {
        /* Only do this if the name is not a type name at file
           scope. */
        if (!check_for_file_scope_type_with_same_name(sym)) {
          if (!sym->header->has_cfront_transitional_nested_type_mangled_name) {
            a_type_ptr   sym_type;
            sym_type = type_symbol_type(sym);
            sym->header->
                       has_cfront_transitional_nested_type_mangled_name = TRUE;
            sym_type->use_cfront_transitional_nested_type_name_mangling = TRUE;
          } else {
            a_symbol_ptr other_sym;
            other_sym = find_cfront_transitional_nested_type_symbol(sym);
            if (other_sym != NULL) {
              /* other_sym can be NULL in certain cases where the transitional
                 nested type flag has already been set for another symbol
                 in the same class.  This can occur for cases like
                   typedef class {} A;
                 which results in two symbols being created in cfront mode. */
              pos_sy2_error(ec_cfront_multiple_nested_types,
                            &sym->decl_position, sym, other_sym);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* for */
}  /* nested_class_anachronism_processing */


static void do_nested_class_anachronism_processing(a_symbol_ptr symbol_list)
/*
This is an interface routine to nested_class_anachronism_processing.
When generating cfront 2.1 compatible object code, and in cfront 2.1 mode,
the processing is done in two passes: first any tag symbols are processed,
then any typedef symbols.  This ensures that if both a tag and a typedef
have the same name, the tag will receive the special transitional
nested type name mangling.

In other modes, nested_class_anachronism_processing is only called once,
and all symbols are processed in that one pass.
*/
{
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  a_boolean	separate_typedef_pass = cfront_2_1_mode;
  nested_class_anachronism_processing(symbol_list,
                                      /*do_tags=*/TRUE,
                                      !separate_typedef_pass);
  if (separate_typedef_pass) {
    nested_class_anachronism_processing(symbol_list,
                                        /*do_tags=*/FALSE,
                                        /*do_typedefs=*/TRUE);
  }  /* if */
#else /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  nested_class_anachronism_processing(symbol_list,
                                      /*do_tags=*/TRUE,
                                      /*do_typedefs=*/TRUE);
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
}  /* do_nested_class_anachronism_processing */


#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
static
void file_scope_transitional_nested_type_processing(a_symbol_ptr symbol_list)
/*
Look for file scope symbols that have the "cfront transitional" nested
type flag set.  This indicates that a file scope symbol with the same
name was defined after the nested class was seen.  This is an error
in cfront compatibility mode.
*/
{
  a_symbol_ptr	sym;
  for (sym = symbol_list; sym != NULL; sym = sym->next_in_scope) {
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
  }  /* for */
}  /* file_scope_transitional_nested_type_processing */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */


void wrapup_scope(a_scope_ptr			scope_ptr,
                  a_scope_kind			kind,
                  a_scope_pointers_block_ptr	pointers_block,
                  a_boolean 	                is_namespace_wrapup)
/*
Do the processing required when a scope is closed.  This includes
doing any necessary end-of-scope processing on the symbols from
the scope.  This routine does processing that is done for scopes as they
are popped off of the stack and also for namespace scopes and the end
of the translation unit.

scope_ptr points to the IL scope entry associated with this scope,
and may be NULL.  is_namespace_wrapup is TRUE if this call is
used to do the final namespace processing at the end of the translation
unit.
*/
{
  a_symbol_ptr			sym;

  db_enter(3, "wrapup_scope");
#if DEBUG
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
    switch(kind) {
      case sck_func_prototype:
      case sck_namespace_extension:
      case sck_namespace_reactivation:
        break;
      case sck_namespace:
        if (!is_namespace_wrapup) break;
        /*FALLTHROUGH*/
      default:
        db_decl_pos_info_for_scope(scope_ptr, pointers_block);
    }  /* switch */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* DEBUG */
  /* Determine whether types defined in this scope should be handled
     as semivisible types.  Template classes and classes nested within
     template classes do not have this processing done. */
  if (allow_anachronisms &&
      depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE) {
    a_boolean do_semivisible_type_processing = TRUE;
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
    if (kind == (a_scope_kind)sck_class_struct_union &&
        do_semivisible_type_processing) {
      /* Determine whether any of the symbols from this class scope should
         be treated as semivisible types. */
      do_nested_class_anachronism_processing(pointers_block->symbols);
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
    } else if (kind == (a_scope_kind)sck_file && cfront_2_1_mode &&
               is_namespace_wrapup) {
      /* See if any of the file scope symbols conflict with semivisible
         nested types. */
      file_scope_transitional_nested_type_processing(pointers_block->symbols);
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
  if (kind == (a_scope_kind)sck_file && is_namespace_wrapup) {
    /* The GNU alias attribute can refer to names of entities before those
       entities are declared.  The actual IL connection is therefore set up
       when all the entities in a translation unit have been seen.  This must
       occur before unneeded entities are determined. */
    process_alias_fixup_list();
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if RECORD_HIDDEN_NAMES_IN_IL
  if (!C_mode() && total_errors == 0 && is_primary_translation_unit) {
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_block ||
        (kind == (a_scope_kind)sck_file && is_namespace_wrapup)) {
      /* Now that all declarations in the scope have been seen, check for
         name hiding.  The hidden name table assists the C++-generating back
         end to determine when to put out qualified names and elaborated
         type specifiers.  Note that namespace and class scopes are handled
         when the scopes in which they are directly nested are processed. */
      check_name_hiding_for_scope(scope_ptr);
    }  /* if */
  }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  if (kind == (a_scope_kind)sck_namespace_extension ||
      kind == (a_scope_kind)sck_namespace_reactivation) {
    /* Symbol processing is not done for namespace extension and
       reactivation scopes. */
  } else {
    a_boolean                is_prototype_instantiation = FALSE;
    a_routine_ptr            curr_routine = NULL;

    /* Check for prototype instantiation of a class template. */
    if (kind == (a_scope_kind)sck_function) {
      /* If the scope is for a routine, get a pointer to the routine. */
      curr_routine = scope_ptr->variant.routine.ptr;
    }  /* if */
    if (kind == (a_scope_kind)sck_class_struct_union && !C_mode() &&
        scope_ptr->variant.assoc_type->
                                 variant.class_struct_union.is_nonreal_class) {
      is_prototype_instantiation = TRUE;
    }  /* if */
    /* Remove the symbols declared in this scope from the symbol table.
       Check for unreferenced symbols, and issue warnings for those. */
    for (sym = pointers_block->symbols;
         sym != NULL;
         sym = sym->next_in_scope) {
#if DEBUG
      if (db_active && 
          (debug_level >= 3 ||
           db_flag_is_set("dump_symbols"))) {
        if ((kind != (a_scope_kind)sck_namespace &&
             kind != (a_scope_kind)sck_file) || is_namespace_wrapup) {
          if (sym == pointers_block->symbols) {
            fputs("Wrapping up ", f_debug);
            if (scope_ptr != NULL) {
              db_scope(scope_ptr);
            } else {
              (void)db_scope_kind(kind);
              (void)fprintf(f_debug, " scope");
            }  /* if */
            fputs(":\n", f_debug);
          }  /* if */
          db_symbol(sym, "", 2);
        }  /* if */
      }  /* if */
#endif /* CHECKING */
      if (kind == (a_scope_kind)sck_func_prototype && !is_tag_symbol(sym)) {
        /* Don't check on symbols entered in the scope of a function prototype.
           They will be reentered in the scope of the function and should be
           checked when the function scope is popped.  Tag symbols are
           checked because tags associated with incomplete types need to
           be put on the types list of the prototype scope. */
      } else if (is_prototype_instantiation) {
        /* Don't check on symbols entered in the scope of a class template
           prototype instantiation -- the information may not be complete. */
      } else if (kind == (a_scope_kind)sck_template_instantiation) {
        /* Don't check on symbols entered in instantiation scopes.  (These
           should only be injected friends of prototype instantiations.) */
      } else if (kind == (a_scope_kind)sck_namespace && !is_namespace_wrapup) {
        /* Don't check symbols in namespaces and namespace extensions because
           we don't have complete information yet.  This will be done at the
           end of the file scope. */
      } else if (!is_namespace_wrapup && is_class_struct_union_symbol(sym) &&
                 is_member_of_unnamed_namespace(
                                    &type_symbol_type(sym)->source_corresp)) {
        /* Same for unnamed namespace class members. */
      } else if (kind == (a_scope_kind)sck_file && !is_namespace_wrapup) {
        /* File scope symbols are not checked until the file scope is popped
           again after processing all translation units. */
      } else {
        end_of_scope_symbol_check(sym, curr_routine);
#if RECORD_HIDDEN_NAMES_IN_IL
#if CHECKING
        if (!C_mode() && total_errors == 0) {
          if (!sym->is_error &&
              sym->kind != (a_symbol_kind)sk_undefined &&
              sym->kind != (a_symbol_kind)sk_macro) {
            if (is_tag_symbol(sym)) {
              check_assertion(sym->header->any_tag_decl);
            }  /* if */
            if (kind == (a_scope_kind)sck_file ||
                kind == (a_scope_kind)sck_namespace) {
              check_assertion(sym->header->
                                    any_decl_in_file_or_namespace_scope);
            }  /* if */
          }  /* if */
        }  /* if */
#endif /* CHECKING */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_extern_variable ||
          sym->kind == (a_symbol_kind)sk_extern_routine) {
        /* Extern variable and routine symbols were not really entered into
           the symbol table proper, so don't try to remove them or add them to
           the inactive list. */
        continue;
      }  /* if */
      if ((kind == (a_scope_kind)sck_namespace ||
           kind == (a_scope_kind)sck_file) && is_namespace_wrapup) {
        /* File scope and namespace scope symbols were removed from the
           symbol table when the scope was first closed.  Don't do it again
           now. */
      } else {
        /* Remove the symbol from the symbol table.  This is not done for
           namespace extension scopes because symbols from namespace extension
           scopes are put directly on the inactive list.  The symbol list from
           the scope entry includes all symbols in the namespace, not only
           those added as a result of this extension.  (Note that symbols are
           not removed from the scope list.  This is because they must
           sometimes remain accessible and the scope list, saved away in some
           other data structure, is a convenient way to get at them again.) */
        unlink_symbol_from_symbol_table(sym);
      }  /* if */
      /* Put struct/union/class members, namespace members, and template
         parameters on the inactive list of the proper symbol header.  For
         namespace members, this only needs to be done for the initial
         definition.  When a namespace extension is done, the symbols are
         added directly to the inactive list. */
      if (kind == (a_scope_kind)sck_class_struct_union ||
          ((kind == (a_scope_kind)sck_namespace ||
            kind == (a_scope_kind)sck_file) && !is_namespace_wrapup) ||
          kind == (a_scope_kind)sck_template_declaration) {
        add_symbol_to_inactive_list(sym);
      }  /* if */
    }  /* for */
    if (kind == (a_scope_kind)sck_namespace ||
        kind == (a_scope_kind)sck_file) {
      /* Synthesized namespace projections are not removed from file
         and namespace scopes because they may be reused if an extension
         scope is opened. */
    } else {
      /* Remove any synthesized namespace projection symbols from the
         others_symbols list of the symbol header. */
      for (sym = pointers_block->synth_namespace_projection_symbols;
           sym != NULL;
           sym = sym->next_in_scope) {
        a_symbol_ptr	prev_sym = NULL;
        a_symbol_ptr	other_sym;
        /* Symbols that are not reusable will not be on the other symbols
           list. */
        if (sym->do_not_reuse) continue;
        for (other_sym = sym->header->other_symbols;
             other_sym != NULL; other_sym = other_sym->next) {
          if (other_sym == sym) break;
          prev_sym = other_sym;
        }  /* for */
        check_assertion_str2(other_sym != NULL, "wrapup_scope:",
                             "synth sym not on other_symbols list");
        if (prev_sym == NULL) {
          /* The symbol is the first one on the list.  Remove it by setting
             the list to its next pointer. */
          sym->header->other_symbols = sym->next;
        } else {
          /* Remove the symbol from the linked list. */
          prev_sym->next = sym->next;
        }  /* if */
      }  /* for */
    }  /* if */
    if (C_dialect == C_dialect_cplusplus && scope_ptr != NULL) {
      if (kind == (a_scope_kind)sck_function ||
          kind == (a_scope_kind)sck_block ||
          (kind == (a_scope_kind)sck_file && is_namespace_wrapup) ||
          (kind == (a_scope_kind)sck_namespace && is_namespace_wrapup)) {
        /* Issue a diagnostic on non-extern member functions that have been
           referenced but not defined. */
        a_boolean  is_function_local = (kind == (a_scope_kind)sck_function ||
                                        kind == (a_scope_kind)sck_block);
        check_referenced_member_functions(scope_ptr, is_function_local,
                                          /*within_unnamed_class=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* wrapup_scope */


void wrapup_namespace_scopes(a_scope_ptr scope_ptr)
/*
Call wrapup_scope for any namespace scopes defined within the scope
pointed to by scope_ptr.
*/
{
  a_namespace_ptr	nsp = scope_ptr->namespaces;

  while (nsp != NULL) {
    if (!nsp->is_namespace_alias) {
      a_scope_pointers_block_ptr  pointers_block;
      a_scope_ptr                 assoc_scope = nsp->variant.assoc_scope;
      pointers_block = &symbol_supplement_for_namespace(nsp)->pointers_block;
      wrapup_scope(assoc_scope, assoc_scope->kind,
                   pointers_block, /*is_namespace_wrapup=*/TRUE);
      /* Process any namespaces defined within this one. */
      wrapup_namespace_scopes(assoc_scope);
    }  /* if */
    nsp = nsp->next;
  }  /* while */
}  /* wrapup_namespace_scopes */

#if MAINTAIN_NEEDED_FLAGS

static a_boolean variable_needed_even_if_unreferenced(a_variable_ptr	var)
/*
Return TRUE if "var" is needed even if it is unreferenced (e.g., because
it is an external definition).
*/
{
  a_boolean		is_needed = FALSE;

  if (!var->is_template_static_data_member) {
    /* A non-template variable. */
    if ((var->storage_class == (a_storage_class)sc_unspecified
#if DO_IL_LOWERING
         && !var->promoted_local_static
#endif /* DO_IL_LOWERING */
                                       ) ||
         var->init_kind == (an_init_kind)initk_dynamic) {
      /* This is an externally linked variable that has been defined, or
         it is a variable local to this translation unit but with
         dynamic initialization, in which case it is treated as "needed"
         because the initialization may have side effects.  Mark it as
         needed now. */
      is_needed = TRUE;
#if GNU_EXTENSIONS_ALLOWED
    } else if (var->section != NULL) {
      /* GNU C concatenates all variables in the same named section.
         This creates tables that can be accessed from any translation unit
         even though the individual entries may have had internal linkage. */
      is_needed = TRUE;
    } else if (var->storage_class == (a_storage_class)sc_static &&
               (var->has_gnu_unused_attribute || var->is_weak)) {
      /* GNU C doesn't eliminate unreferenced static variables.  This front end
         may do so, but some attributes are taken as an indication that the
         entry should be kept. */
      is_needed = TRUE;
    } else if (var->aliased_variable != NULL) {
      is_needed = variable_needed_even_if_unreferenced(var->aliased_variable);
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  } else {
    /* A template static data member. */
    if (!is_primary_translation_unit) {
      /* Assume that all static data members from secondary translation units
         are needed.  This will be reconsidered after the routine has
         been copied to the primary translation unit. */
      is_needed = TRUE;
    } else if (var->is_specialized) {
      /* A specialized static data member is always needed. */
      is_needed = TRUE;
    } else {
      /* An instantiation of a static data member must be considered
         to be needed if it was automatically instantiated (i.e, it is
         in the instantiation request file for this compilation), or if
         it was explicitly instantiated.  For other cases, which include
         things instantiated in -tused mode and things adopted by this
         compilation, use the needed flag mechanism to determine whether
         the instantiation is really needed. */
      a_symbol_ptr		sym;
      a_template_instance_ptr	tip;
      a_master_instance_ptr	mip;

      sym = (a_symbol_ptr)var->source_corresp.assoc_info;
      check_assertion(sym != NULL);
      tip = sym->variant.static_data_member.instance_ptr;
      check_assertion(tip != NULL);
      mip = tip->master_instance;
      if (tip->explicit_instantiation ||
	  (mip != NULL &&
	   (mip->automatically_instantiated && !mip->add_to_request_file))) {
        /* The instance exists as a result of an explicit instantiation
	   directive, or as a result of being assigned to this file by
	   the automatic instantiation mechanism.  The automatically
           instantiated flag will be set for adopted entities.  The test
           of add_to_request_file is used so that adopted entities will
           not necessarily be considered to be needed. */
        is_needed = TRUE;
      } else {
	is_needed = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_needed;
}  /* variable_needed_even_if_unreferenced */


static void set_needed_flags_for_typedefs(a_scope_ptr scope)
/*
Make a pass through the IL tree looking for typeref types that should be
marked as "needed" because they are involved in declarations of unnamed
classes -- e.g.,
  typedef struct { ... } S;
If struct S is needed, the typeref type that points to it and from which
it acquired its name should be marked as needed, too.  The reason this is
done separately from set_needed_flags_at_end_of_file_scope, and after it
is done, is that all the classes have to have been marked first.
*/
{
  a_type_ptr                   tp, under_type;
  a_class_type_supplement_ptr  ctsp;
  a_namespace_ptr              nsp;

  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Nested namespace scope.  Apply the check to each of its types. */
      set_needed_flags_for_typedefs(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (tp->kind == (a_type_kind)tk_typeref) {
      /* If a typeref type points to a class that is needed, has a name, and
         was originally unnamed, the typeref type is needed, too. */
      /* Also keep placeholder typerefs if the referenced type is needed. */
      under_type = tp->variant.typeref.type;
      if ((is_immediate_class_type(under_type) &&
           under_type->variant.class_struct_union.originally_unnamed) ||
          !has_name(tp)) {
        mark_as_needed_like((char *)tp, (an_il_entry_kind)iek_type,
                            &under_type->source_corresp,
                            /*set_class_defn_needed=*/FALSE);
      }  /* if */
    } else if (is_immediate_class_type(tp)) {
      ctsp = tp->variant.class_struct_union.extra_info;
      if (ctsp != NULL && ctsp->assoc_scope != NULL) {
        /* Apply the check to each of the types defined in the class. */
        set_needed_flags_for_typedefs(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* set_needed_flags_for_typedefs */


void set_needed_flags_at_end_of_file_scope(a_scope_ptr scope)
/*
scope is a pointer to the file scope, a namespace scope, or a class scope.
Set the "needed" flags on classes, variables, and static data members now
that processing for the file scope (including IL lowering, if applicable) has
been completed.
*/
{
  a_type_ptr                   tp;
  a_class_type_supplement_ptr  ctsp;
  a_namespace_ptr              nsp;
  a_variable_ptr               vp;
  a_routine_ptr                rp;

  if (scope->kind == (a_scope_kind)sck_file) {
    /* Top-level call. */
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "Start of set_needed_flags_at_end_of_file_scope\n");
    }  /* if */
#endif /* DEBUG */
    end_of_file_scope_needed_flags_phase = TRUE;
  } else {
    check_assertion_str2(scope->kind == (a_scope_kind)sck_namespace ||
                         scope->kind == (a_scope_kind)sck_class_struct_union,
                       "set_needed_flags_at_end_of_file_scope:",
                       "bad scope kind");
  }  /* if */
  /* Apply this check to namespaces defined in the current scope, if there
     are any. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Nested namespace scope. */
      set_needed_flags_at_end_of_file_scope(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Check for classes defined in the current scope. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp)) {
      /* If the class has been marked to indicate that it is needed,
         then we need to walk the subtree of the class; if not, we can ignore
         it. */
      remark_as_needed((char *)tp, (an_il_entry_kind)iek_type);
      ctsp = tp->variant.class_struct_union.extra_info;
      if (ctsp != NULL && ctsp->assoc_scope != NULL) {
        /* Check nested classes and static data members, too.  Note that this
           may be done even if the class itself is not needed. */
        set_needed_flags_at_end_of_file_scope(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Do processing on variables (or, if this a class scope, static data
     members). */
  for (vp = scope->variables; vp != NULL; vp = vp->next) {
    a_boolean is_needed;
    /* Determine whether this variable should be considered "needed".  This
       is true for variables that are referenced and for most external
       definitions. */
    is_needed = (vp->source_corresp.needed ||
                 variable_needed_even_if_unreferenced(vp));
    if (is_needed) {
      mark_as_needed((char *)vp, (an_il_entry_kind)iek_variable);
    }  /* if */
    /* If the variable is marked as needed, remark it to visit its
       subtree.  The subtree is not visited until this phase, because it
       can change. */
    remark_as_needed((char *)vp, (an_il_entry_kind)iek_variable);
  }  /* for */
  for (rp = scope->routines; rp != NULL; rp = rp->next) {
    a_boolean saved_defined = rp->defined;
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_mode && rp->aliased_routine != NULL) {
      /* Routine aliases are needed because they may be accessed from other
         translation units. */
      mark_as_needed((char *)rp, (an_il_entry_kind)iek_routine);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* If the routine is marked as needed, remark it to visit its
       subtree.  The subtree is not visited until this phase, because it
       can change.  Note that the subtree here is the function type,
       not the body, which is handled elsewhere. */
    /* If the "defined" flag is TRUE, the body will already have been
       walked to mark its constituents as needed; we clear the flag to
       keep it from being walked again. */
    rp->defined = FALSE;
    remark_as_needed((char *)rp, (an_il_entry_kind)iek_routine);
    /* Restore the "defined" flag. */
    rp->defined = saved_defined;
  }  /* for */
#if BACK_END_IS_CP_GEN_BE
  if (scope->templates != NULL) {
    /* The very presence of templates in the IL means pruning the IL of
       apparently unneeded entries must be suppressed.  This is because an
       IL entry may be needed by an instantiation of a template without the
       front end being able to tell. */
    okay_to_eliminate_unneeded_il_entries = FALSE;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* End of top-level call. */
    if (!C_mode()) {
      /* Check for cases like this:
           typedef struct { ... } T;
         where the struct has been marked as needed but the typedef has not.
         The typedef really is needed in some cases -- e.g., in producing the
         proper definition by the C++-generating back end.  Mark the typedef,
         too. */
      set_needed_flags_for_typedefs(scope);
    }  /* if */
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "End of set_needed_flags_at_end_of_file_scope\n");
    }  /* if */
#endif /* DEBUG */
    end_of_file_scope_needed_flags_phase = FALSE;
  }  /* if */
}  /* set_needed_flags_at_end_of_file_scope */

#endif /* MAINTAIN_NEEDED_FLAGS */
#if MAINTAIN_NEEDED_FLAGS

static a_boolean routine_needed_even_if_unreferenced(a_routine_ptr rout)
/*
Return TRUE if the indicated routine is needed even if it is unreferenced,
e.g., because it's externally defined.
*/
{
  a_boolean is_needed = FALSE;

#if GNU_EXTENSIONS_ALLOWED
  if (rout->is_initialization_routine || rout->is_finalization_routine) {
    /* An initialization or finalization routine is always needed,
       even if not otherwise referenced, because it will be called
       at program startup. */
    is_needed = TRUE;
  } else if (rout->storage_class == (a_storage_class)sc_static &&
             (rout->has_gnu_unused_attribute || rout->is_weak)) {
    /* GNU C doesn't eliminate unreferenced static functions.  This front end
       may do so, but some attributes are taken as an indication that the
       entry should be kept. */
    is_needed = TRUE;
  } else 
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* Generally, externally-defined routines are needed, because they might
       be referenced from some other compilation unit. */
    if (rout->storage_class == (a_storage_class)sc_unspecified) {
      is_needed = TRUE;
      if (rout->is_trivial_default_constructor) {
	/* Trivial constructors have no bodies so are never needed. */
	is_needed = FALSE;
      } else if (rout->is_inline &&
#if GNU_EXTENSIONS_ALLOWED
                 !(gcc_mode && !rout->suppress_inline_body) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
		 !(c99_mode && !rout->suppress_inline_body)) {
	/* An exception is "extern inline" functions, which are not regarded
	   as referenced from elsewhere.  Each compilation unit has its own
	   copy, and this copy is needed only if it is referenced in this
	   compilation unit.  In C99 mode, however, an out-of-line copy that
	   can be referenced from somewhere else may have been generated (if
	   there was also a non-inline declaration of the function).
           In GCC mode, an inline function can be referenced from
           other compilation units unless it is explicitly declared
           "extern inline". */
	is_needed = FALSE;
      } else if (!is_primary_translation_unit) {
        /* Assume that all external routines from secondary translation units
           are needed.  This will be reconsidered after the routine has
           been copied to the primary translation unit. */
      } else if (rout->is_template_function &&
		 !rout->is_specialized) {
        /* An instantiation of an external template function must be considered
           to be needed if it was automatically instantiated (i.e, it is
           in the instantiation request file for this compilation), or if
           it was explicitly instantiated.  For other cases, which include
           things instantiated in -tused mode and things adopted by this
           compilation, use the needed flag mechanism to determine whether
           the instantiation is really needed. */
	a_symbol_ptr             rout_sym;
	a_template_instance_ptr  tip;
	a_master_instance_ptr	 mip;

	rout_sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
	check_assertion(rout_sym != NULL);
	tip = rout_sym->variant.routine.instance_ptr;
	check_assertion(tip != NULL);
        mip = tip->master_instance;
	if (tip->explicit_instantiation ||
            instantiation_mode == tim_all ||
	    (mip != NULL &&
	     (mip->automatically_instantiated && !mip->add_to_request_file))) {
	  /* The instance exists as a result of an explicit instantiation
	     directive, or as a result of being assigned to this file by
	     the automatic instantiation mechanism.  The automatically
             instantiated flag will be set for adopted entities.  The test
             of add_to_request_file is used so that adopted entities will
             not necessarily be considered to be needed.  In -tall mode,
             instantiations should always be considered needed. */
	} else {
	  is_needed = FALSE;
	}  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_needed;
}  /* routine_needed_even_if_unreferenced */

#endif /* MAINTAIN_NEEDED_FLAGS */

a_boolean keep_function_body_for_possible_inlining(a_routine_ptr routine)
/*
Return TRUE if it's desirable to keep the body of the indicated
function around for possible use in inlining calls to it.
*/
{
  a_boolean keep = FALSE;

  if (routine->is_inline) {
#if MINIMAL_INLINING
    if (inlining_enabled) {
      keep = TRUE;
    }  /* if */
#endif /* MINIMAL_INLINING */
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object) {
      /* In one-instantiation-per-object mode, keep an inline function
         around so that its body can be swept for each instantiation that
         needs it. */
      keep = TRUE;
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  }  /* if */
  return keep;
}  /* keep_function_body_for_possible_inlining */

  
static a_boolean function_body_should_be_discarded(a_routine_ptr routine)
/*
Return TRUE if the body of the indicated function should be discarded.
For example, the bodies of generated trivial default constructors are
discarded right after they have been generated.
*/
{
  a_boolean discard = FALSE;

  if (routine->is_trivial_default_constructor) {
    /* Discard trivial default constructors. */
    discard = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode && (routine->decl_modifiers & DM_DLLIMPORT)) {
    /* In Microsoft mode, routines marked __declspec(dllimport) can
       have bodies, which are discarded. */
    discard = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (routine->is_prototype_instantiation &&
             !prototype_instantiations_in_il) {
    /* This is a prototype instantiation, and we're not keeping prototype
       instantiations in the IL. */
    discard = TRUE;
  } else if (is_nontemplate_routine_from_exported_trans_unit(routine)) {
    /* This is a non-template or a specialization in a secondary translation
       unit that is being compiled only for its exported templates.
       Discard it. */
    discard = TRUE;
    if (keep_function_body_for_possible_inlining(routine)) {
      /* ... but keep inline functions so we can inline from them. */
      discard = FALSE;
    }  /* if */
  }  /* if */
  if (!discard && routine->source_corresp.is_local_to_function) {
    /* Member functions of local classes must be discarded if the surrounding
       function is discarded. */
    a_scope_depth depth;
    /* Find the entry for "routine" in the scope stack.  The loop is
       necessary for recursive calls to this routine. */
    for (depth = depth_scope_stack;
         ;
         depth = scope_stack[depth].previous_scope) {
      check_assertion(depth != NO_SCOPE_DEPTH);
      if (scope_stack[depth].kind == (a_scope_kind)sck_function &&
          scope_stack[depth].assoc_routine == routine) break;
    }  /* for */
    check_assertion(depth != NO_SCOPE_DEPTH);
    /* Found the routine.  The next entry on the stack tells us the
       innermost function scope depth for the surrounding function, if any. */
    depth = scope_stack[depth].previous_scope;
    depth = scope_stack[depth].depth_innermost_function_scope;
    if (depth != NO_SCOPE_DEPTH) {
      /* This routine is inside another routine.  Do a recursive call to
         find out whether that routine is to be discarded. */
      a_routine_ptr encl_routine = scope_stack[depth].assoc_routine;
      if (function_body_should_be_discarded(encl_routine)) {
        discard = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return discard;
}  /* function_body_should_be_discarded */


void finish_function_body_processing(a_scope_ptr scope,
                                     a_boolean   will_discard_function_body)
/*
Do final processing on the body of the function with the indicated scope.
This includes IL lowering if appropriate.  This routine is called
immediately after the body is scanned, and also after the body is copied
over to the primary IL for functions in a secondary translation unit.
That is, for functions in a secondary translation unit it is called twice.
If will_discard_function_body is TRUE, the function body will be
thrown away by the caller.
*/
{
  a_routine_ptr routine = scope->variant.routine.ptr;
#if MAINTAIN_NEEDED_FLAGS
  a_boolean     lowering_done = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */

  db_enter(1, "finish_function_body_processing");
#if DEBUG
  if (debug_level >= 1 || db_has_traced_name(routine, iek_routine)) {
    fprintf(f_debug, "Finishing function body processing for ");
    db_name_full(&routine->source_corresp, iek_routine);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  check_assertion(!scope->function_body_processing_finished);
  /* Do not do lowering and related processing of functions in secondary
     translation units until they are copied to the primary IL.
     When this routine is called after copying for functions from
     secondary translation units, is_primary_translation_unit is TRUE. */
  if (is_primary_translation_unit) {
#if ONE_INSTANTIATION_PER_OBJECT
    set_routine_instantiation_needed_bit_number(routine);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if DO_IL_LOWERING
    if (!will_discard_function_body &&
        !scope_stack[depth_scope_stack].in_prototype_instantiation) {
      /* Do IL lowering (change the C++ IL into C IL). */
      lower_il_memory_region(routine->assoc_scope);
#if MAINTAIN_NEEDED_FLAGS
      lowering_done = TRUE;
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
    if (il_lowering_needed()) {
      /* If we're not supposed to pass object lifetime information to the
         back end, unlink all object lifetimes from the IL tree. */
      clean_up_all_object_lifetimes(scope);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  if (!will_discard_function_body) {
    /* If a function or block scope has local types or static variables,
       make a special entry to record those orphan lists on the il_header
       scope_orphaned_list_headers list so they can be found when
       processing the file scope memory region.  Note that processing
       for block scopes is done at the end of the function scope to give
       IL lowering a chance to add variables and types in block scopes.
       Also note that for functions in secondary translation units
       the lists are generated anew after the function body is (lowered
       and) moved over, rather than copying the lists. */
    add_scope_orphaned_il_lists(scope);
  } else {
    eliminate_pragmas_for_file_scope_entities(scope);
  }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    /* Clear out the shareable constants table for the function scope. */
    empty_func_shareable_constants_table();
  }  /* if */
  scope->function_body_processing_finished = TRUE;
#if MAINTAIN_NEEDED_FLAGS
  { a_boolean is_needed;
    /* Walk subtrees of local types and variables that have already been
       marked as needed.  If they are not marked as needed yet, this
       allows the subtrees to be swept in the future because they
       can no longer change.*/
    walk_subtrees_of_local_entities(scope);
    if (routine->defined && lowering_done) { /*lint !e774*/
      /* If the definition_needed flag is set already, sweep the body
         now that lowering has been done. */
      remark_routine_definition_needed(routine);
    }  /* if */
    /* If the function is globally visible and presumably needed by code
       in another translation unit, set the "needed" flag on the function. */
    is_needed = (routine->source_corresp.needed ||
                 routine_needed_even_if_unreferenced(routine));
    if (is_needed) {
      mark_as_needed((char *)routine, (an_il_entry_kind)iek_routine);
#if DEBUG
    } else if (debug_level >= 3) {
      fprintf(f_debug, "Not calling mark_as_needed for \"");
      db_name_full(&routine->source_corresp, iek_routine);
      fprintf(f_debug, "\", storage class is %s\n",
              db_storage_class_names[(int)routine->storage_class]);
#endif /* DEBUG */
    }  /* if */
  }
#endif /* MAINTAIN_NEEDED_FLAGS */
  db_exit();
}  /* finish_function_body_processing */

#if DO_IL_LOWERING

static a_boolean should_delay_lowering_on_function(a_routine_ptr routine)
/*
The scope for the body of the indicated routine is currently being popped.
Return TRUE if there is a reason why the lowering of the function should
be delayed until the end of the compilation.
*/
{
  a_boolean   delay_lowering = FALSE;
  a_scope_ptr scope = il_header.region_scope_entry[routine->assoc_scope];

  check_assertion(!in_secondary_trans_unit(routine));
  if (secondary_translation_unit_seen()) {
    /* Don't lower template instantiations in the primary translation unit
       if there are exported templates, because we want to eliminate
       references to entities in the secondary translation unit IL first.
       The lowering will be done later -- see
       finish_processing_for_function_bodies. */
    delay_lowering = TRUE;
  } else if (!C_mode() && export_template_allowed &&
             routine->storage_class == (a_storage_class)sc_static &&
             (scope->variables != NULL || scope->types != NULL ||
              scope->scopes != NULL)) {
    /* When exported templates are allowed, a static function might be
       externalized because it might be referenced by a template.
       If it has local static variables, they might have to be
       externalized too, and we can't generate the externalized name
       now because we don't have the module id yet. */
    delay_lowering = TRUE;
  }  /* if */
  if (delay_lowering) {
    function_body_processing_delayed_on_some_func_in_primary_il = TRUE;
  }  /* if */
  return delay_lowering;
}  /* should_delay_lowering_on_function */

#endif /* DO_IL_LOWERING */

static void wrap_up_symbols_with_no_scope(void)
/*
Certain symbols (such as keywords and predefined macros) are not entered
on the scope list of any scope.  When the file scope is popped for the
first time, such symbols must be moved to the inactive list.
*/
{
  a_symbol_ptr	sym;

  for (sym = symbols_with_no_scope; sym != NULL; sym = sym->next_in_scope) {
    unlink_symbol_from_symbol_table(sym);
    add_symbol_to_inactive_list(sym);
  }  /* for */
  /* Indicate that the file scope symbols have been moved to the inactive
     list. */
  file_scope_symbols_are_on_inactive_list = TRUE;
}  /* wrap_up_symbols_with_no_scope */


void pop_scope(void)
/*
End a name scope by popping an entry off the scope stack.
*/
{
  a_scope_stack_entry_ptr  ssep, parent_ssep;
  a_scope_pointers_block_ptr pointers_block;
  a_memory_region_number   old_memory_region_number, new_memory_region_number;
  a_scope_kind             kind;
  an_extern_type_fixup_ptr etfp;
  a_scope_depth            scope_depth;
  a_boolean                old_region_still_needed;
  a_scope_ptr              il_scope;
  a_routine_ptr            curr_routine = NULL;
  a_boolean                discard_function_body = FALSE;

  db_enter(3, "pop_scope");
  ssep = &scope_stack[depth_scope_stack];
  pointers_block = assoc_pointers_block_of(ssep);
  kind = ssep->kind;
  if (kind == (a_scope_kind)sck_function) {
    /* If the scope is for a routine, get a pointer to the routine. */
    curr_routine = ssep->il_scope->variant.routine.ptr;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (pointers_block->symbols != NULL || debug_level >= 4) {
      fprintf(f_debug, "pop_scope: number = %ld, depth = %d",
              (long)ssep->number, depth_scope_stack);
      if (curr_routine != NULL) {
        (void)fputs(", curr_routine = \"", f_debug);
        db_name_full(&curr_routine->source_corresp, iek_routine);
        (void)fputc('"', f_debug);
      } else if ((kind == (a_scope_kind)sck_class_struct_union ||
                  kind == (a_scope_kind)sck_class_reactivation) &&
                 ssep->assoc_type != NULL) {
        (void)fputs(", class = \"", f_debug);
        db_name_full(&ssep->assoc_type->source_corresp, iek_type);
        (void)fputc('"', f_debug);
      } else if ((kind == (a_scope_kind)sck_namespace ||
                  kind == (a_scope_kind)sck_namespace_reactivation ||
                  kind == (a_scope_kind)sck_namespace_extension) &&
                 ssep->il_scope != NULL &&
                 ssep->il_scope->variant.assoc_namespace != NULL) {
        (void)fprintf(f_debug, ", namespace%s = \"",
                      kind == (a_scope_kind)sck_namespace ? "" : "-ext");
        db_name_full(&ssep->il_scope->variant.assoc_namespace->source_corresp,
                     iek_namespace);
        (void)fputc('"', f_debug);
      } else {
        fputs(", kind = ", f_debug);
        if (ssep->is_for_init_block) (void)fputs("for-init ", f_debug);
        (void)db_scope_kind(kind);
      }  /* if */
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Get a new pointer to the current scope stack entry in case the scope
     stack has been reallocated (e.g., by check_name_hiding_for_scope). */
  ssep = &scope_stack[depth_scope_stack];
  pointers_block = assoc_pointers_block_of(ssep);
  if (!(ssep->kind == (a_scope_kind)sck_file && ssep->is_reactivation)) {
    /* Remove symbols from the symbol table, and reenter them on the
       inactive list if necessary.  For the file scope, this is only done
       the first time that it is popped. */
    wrapup_scope(ssep->il_scope, kind, pointers_block,
                 /*is_namespace_wrapup=*/FALSE);
    if (ssep->kind == (a_scope_kind)sck_file) {
      /* When the file scope is popped the first time, transfer any symbols
         that are not associated with a scope list from the active list to
         the inactive list. */
      wrap_up_symbols_with_no_scope();
    }  /* if */
  }  /* if */
  il_scope = ssep->il_scope;
  if (C_dialect == C_dialect_cplusplus && il_scope != NULL) {
    if (kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_block) {
      /* If there are any local classes, check for compiler-generated
         virtual destructors for which bodies should be put out. */
      generate_required_virtual_destructor_bodies(il_scope);
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
    /* If the scope specified additional using directives, clear all of the
       active using list flags, and reset them to the values specified
       by the previous scope stack entries. */
    if (ssep->active_using_directives != NULL) {
      set_active_using_list_scope_depths(depth_scope_stack,
                                         /*set_value=*/FALSE,
                                         NO_DECL_SEQUENCE_NUMBER);
      if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
        set_active_using_list_scope_depths(ssep->previous_scope,
                                           /*set_value=*/TRUE,
                                           NO_DECL_SEQUENCE_NUMBER);
      }  /* if */
    }  /* if */
    /* Free any active using directive entries. */
    if (ssep->active_using_directives != NULL) {
      free_active_using_directive_list(ssep->active_using_directives);
      ssep->active_using_directives = NULL;
    }  /* if */
    /* Unlink scope-specific constants that might have been generated. */
    if (ssep->generated_entities != NULL) {
      ssep->generated_entities->function_name = NULL;
      ssep->generated_entities->decorated_function_name = NULL;
    }  /* if */
    /* Do management related to the object lifetime stack.  Don't pop the
       file scope object lifetime yet, though, because we need it in IL
       lowering; see below */
    if (kind == (a_scope_kind)sck_block ||
        kind == (a_scope_kind)sck_function ||
        kind == (a_scope_kind)sck_condition) {
      /* For a function, block, or condition scope, pop the current object
         lifetime, which ought to be the one created when this scope was
         pushed. */
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
    /* Dispose of the list of entries of type a_name_hidden_by_old_for_init.
       They are no longer needed once the scope has been completed. */
    if (ssep->names_hidden_by_old_for_init != NULL) {
      free_names_hidden_by_old_for_init(ssep->names_hidden_by_old_for_init);
    }  /* if */
  }  /* if */      
  check_assertion_str2(ssep->defer_access_checks == FALSE &&
                       ssep->deferred_access_checks == NULL,
                       "pop_scope:", "deferred access checks still on list");
  /* If a primary definition of a namespace is being popped (i.e., the
     initial definition of the namespace is complete), set the flag
     in the namespace's scope_pointers_block to indicate that any
     symbols that are subsequently added to the scope should be added
     directly to the inactive list.  This is also done with then file
     scope is popped, so that if the file scope is re-pushed, symbols
     will be added directly to the inactive list. */
  if (kind == (a_scope_kind)sck_namespace || kind == (a_scope_kind)sck_file) {
    ssep->assoc_pointers_block->add_symbols_to_inactive_list = TRUE;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ssep->kind == (a_scope_kind)sck_file ||
      ssep->kind == (a_scope_kind)sck_function) {
    if (il_scope != NULL && ssep->source_sequence_list != NULL) {
      il_scope->source_sequence_list = ssep->source_sequence_list;
      if (ssep->kind == (a_scope_kind)sck_file) {
        /* Save the "last" pointer for the file scope for use on a
           later file scope reactivation. */
        curr_translation_unit->
                         file_scope_pointers_block.last_source_sequence_entry =
                                             ssep->end_of_source_sequence_list;
      }  /* if */
      ssep->source_sequence_list = NULL;
      ssep->end_of_source_sequence_list = NULL;
      if (ssep->kind == (a_scope_kind)sck_function) {
        fixup_function_scope_source_sequence_list(il_scope);
      }  /* if */
#if DEBUG
      if (debug_level >= 3 ||
          db_flag_is_set("dump_ss") ||
          db_flag_is_set("dump_ss_full")) {
        /* Display source sequence lists for debug purposes. */
        db_ss_list_for_scope(il_scope);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
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
#if DEBUG
  if (kind == (a_scope_kind)sck_file ||
      kind == (a_scope_kind)sck_function) {
    /* Dump type list and object lifetime information. */
    if (db_flag_is_set("dump_type_lists")) {
      db_type_lists(il_scope, 0);
    }  /* if */
    if (db_flag_is_set("dump_lifetimes")) {
      fprintf(f_debug, "Object lifetime for ");
      db_scope(il_scope);
      fprintf(f_debug, ":\n");
      db_object_lifetime_tree(il_scope->lifetime);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Determine and remember the current (old) memory region, to see
     if it changes when returning to the outer scope. */
  old_memory_region_number = ssep->il_memory_region;
  /* Don't finish the file scope at this point.  That is deferred until
     the compilation unit (not translation unit) is completed. */
  if (old_memory_region_number == file_scope_region_number) {
    old_region_still_needed = TRUE;
  } else {
    /* If the old memory region number does not appear anywhere in the
       remaining stack, the region is no longer needed by the front end. */
    old_region_still_needed = FALSE;
    for (scope_depth = depth_scope_stack-1; scope_depth >= 0; scope_depth--) {
      if (scope_stack[scope_depth].il_memory_region ==
                                                     old_memory_region_number){
        old_region_still_needed = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (!old_region_still_needed) {
    /* The old memory region is no longer needed. */
    check_assertion(kind == (a_scope_kind)sck_function);
    /* See whether this is a function whose body should be discarded. */
    discard_function_body = function_body_should_be_discarded(curr_routine);
#if DO_IL_LOWERING
    if (is_primary_translation_unit && !discard_function_body &&
        should_delay_lowering_on_function(curr_routine)) {
      /* Delay lowering of functions in some cases. */
      /* Note that il_lowering_needed() is not tested on purpose, to get
         proper error recovery behavior.  Also, it doesn't cover lowering
         needed in C99 mode. */
    } else
#endif /* DO_IL_LOWERING */
    {
      /* Do final processing on the function body.  That includes
         IL lowering if appropriate. */
      finish_function_body_processing(il_scope, discard_function_body);
    }  /* if */
    if (!discard_function_body) {
      /* The definition of the function is complete, so set the defined flag.
         Note: this allows sweeping the routine definition, and (except for
         inline functions) writing out of the body of the function, so it's
         done late.  Note that the body of a function in a secondary
         translation unit is swept twice -- first when unlowered, as
         part of the secondary translation unit, and again after
         lowering as part of the primary translation unit.  The first
         sweep is required to allow retention of referenced file-scope
         entities (e.g., types) during the elimination of unneeded
         entities in the secondary translation unit, before copying of
         IL from the secondary to the primary.  The second sweep is
         required to get the needed flags right in the final IL in
         the primary translation unit.  It is done by the call of
         remark_routine_definition_needed in
         finish_function_body_processing. */
      set_routine_defined(curr_routine);
    }  /* if */
  }  /* if */

  /* The IL scope, if any, is no longer on the stack.  This must occur
     after IL lowering and before check_for_done_with_memory_region. */
  if (il_scope != NULL &&
      il_scope->depth_in_scope_stack == depth_scope_stack) {
    il_scope->depth_in_scope_stack = NO_SCOPE_DEPTH;
  }  /* if */
  if (!old_region_still_needed) {
    if (discard_function_body) {
      /* This is a function whose body should be discarded (e.g., a
         trivial default constructor).  Discard it now. */
      clear_function_body(il_scope);
      /* Put the "defined" flag back on. */
      curr_routine->defined = TRUE;
    } else {
      /* Write the memory region and free it as appropriate. */
      check_for_done_with_memory_region(old_memory_region_number);
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
  /* For template instantiation scopes, restore the template parameters
     to their previous state.  Normally this just involves setting the
     parameters to point to the "resting" values assigned when the
     template declaration is scanned.  In the event of a recursive
     instantiation, however, this requires restoring the values from the
     previous instantiation. */
  if (kind == (a_scope_kind)sck_template_instantiation) {
    a_scope_depth                     prev_depth;
    a_template_decl_info_ptr	      template_decl_info =
                                                     ssep->template_decl_info;

    check_assertion(template_decl_info != NULL);
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
          scope_stack[scope_depth].template_decl_info->parameters ==
                                              template_decl_info->parameters) {
          prev_depth = scope_depth;
        break;
      }  /* if */
    }  /* for */
    if (prev_depth == NO_SCOPE_DEPTH) {
      /* Restore the default values of the parameters. */
      restore_default_template_params(template_decl_info->parameters);
    } else {
      /* Restore the parameter values from the previous instantiation. */
      update_template_param_symbols(template_decl_info->parameters,
                                    scope_stack[prev_depth].template_arg_list);
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (ssep->source_sequence_list != NULL) {
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("popping ", f_debug);
      db_scope_stack_entry_at_depth(depth_scope_stack);
      fputs("\n", f_debug);
    }  /* if */
#endif /* DEBUG */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (ssep->kind == (a_scope_kind)sck_template_instantiation &&
        /* The prototype instantiation should not be moved away from the
           associated template. */
        !ssep->in_prototype_instantiation &&
        !ssep->microsoft_specialization_instantiation_scope) {
      insert_instantiation_src_seq_list(ssep);
    } else
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    /* Do not add code here. */
    {
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("moving source sequences to ", f_debug);
        db_scope_stack_entry_at_depth(depth_scope_stack-1);
        fputs(":\n", f_debug);
        db_ss_list_for_scope_depth(depth_scope_stack);
      }  /* if */
#endif /* DEBUG */
    }
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Set the initial name lookup scope to the previous scope on the
     stack.  Note that this could be different than the previous scope
     value in the scope stack entry. */
  depth_of_initial_lookup_scope = ssep->saved_depth_of_initial_lookup_scope;
  /* Determine the memory region to restore for the outer scope. */
  new_memory_region_number = ssep->prev_il_memory_region;
#if IA64_ABI && NEED_NAME_MANGLING
  if (ssep->local_name_collision_table != NULL) {
    free_local_name_collision_table(ssep);
  }  /* if */
#endif /* IA64_ABI && NEED_NAME_MANGLING */
#if DO_IL_LOWERING
  /* If a string literal table was allocated for the scope, free it now. */
  if (ssep->string_literal_table != NULL) {
    free_string_literal_table(ssep);
  }  /* if */
#endif /* DO_IL_LOWERING */
  /* Pop the stack. */
  if (--depth_scope_stack >= 0) {
    /* The stack is not empty, so do anything necessary to activate the
       new top entry. */
    a_scope_stack_entry_ptr  new_ssep = &scope_stack[depth_scope_stack];

    /* If the new memory region is not the same as the old, activate it. */
    if (new_memory_region_number != old_memory_region_number) {
      switch_il_region(new_memory_region_number);
    }  /* if */
    /* Restore state variables. */
    inside_local_class = new_ssep->inside_local_class;
    depth_innermost_function_scope = new_ssep->depth_innermost_function_scope;
    depth_innermost_namespace_scope =
                                    new_ssep->depth_innermost_namespace_scope;
    innermost_function_scope =
                   (depth_innermost_function_scope != NO_SCOPE_DEPTH) ?
                         scope_stack[depth_innermost_function_scope].il_scope :
                         NULL;
    depth_template_declaration_scope =
                                   new_ssep->depth_template_declaration_scope;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    source_sequence_entries_disallowed =
                                 new_ssep->source_sequence_entries_disallowed;
    if (ssep->source_sequence_list != NULL) {
      if (new_ssep->end_of_source_sequence_list == NULL) {
        new_ssep->source_sequence_list = ssep->source_sequence_list;
      } else {
        new_ssep->end_of_source_sequence_list->next =
                                      ssep->source_sequence_list;
        ssep->source_sequence_list->prev =
                                      new_ssep->end_of_source_sequence_list;
      }  /* if */
      new_ssep->end_of_source_sequence_list =
                                         ssep->end_of_source_sequence_list;
    }  /* if */
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
  if (ssep->is_for_init_block) {
    /* A for-init block is being popped.  Its declarations are going out of
       scope.  But in older versions of C++ they would have remained in scope
       till the end of the containing block.  Track declarations that were
       formerly hidden (say, with cfront) but are now visible (under the new
       for-init scoping rules). */
    record_names_hidden_by_old_for_init(
                                   assoc_pointers_block_of(ssep)->symbols);
  }  /* if */
  db_exit();
}  /* pop_scope */


void f_push_namespace_extension_scope(a_namespace_ptr nsp,
				      a_boolean	      force_new_entry)
/*
Push one or more scopes that will be used to extend the indicated namespace.
This is used, for example, when scanning functions defined in the namespace.
A namespace extension scope is pushed in contexts where members can be
added to a namespace either directly or as a result of a name injection.
This routine is called only in C++.

If force_new_entry is TRUE, a new extension is pushed even if the
current scope is already an extension of the requested scope.
*/
{
  a_namespace_ptr		parent_nsp;
  a_namespace_ptr		curr_nsp = NULL;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];

  /* If the current scope is a namespace (or namespace extension) scope,
     see if it matches the one that we are pushing.  If so, don't actually
     push the scope, just increment the count of the number of excess
     pushes done on this scope. */
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension) {
    curr_nsp = ssep->il_scope->variant.assoc_namespace;
  }  /* if */
  if (curr_nsp == nsp && !force_new_entry) {
    /* The scope is already on the stack. */
    ssep->num_of_extra_times_pushed++;
  } else {
    /* The entry isn't on the stack.  Push any parent namespaces, then push
       the specified namespace. */
    parent_nsp = nsp->source_corresp.parent.namespace_ptr;
    if (parent_nsp != NULL) {
      /* A namespace nested in another namespace.  Push the parent
         namespace. */
      f_push_namespace_extension_scope(parent_nsp, force_new_entry);
    }  /* if */
    /* Push an entry for the scope. */
    (void)push_namespace_scope((a_scope_kind)sck_namespace_extension, nsp);
  }  /* if */
}  /* f_push_namespace_extension_scope */


void pop_namespace_extension_scope(void)
/*
Pop one or more scopes pushed by push_namespace_extension_scope.
This routine is called only in C++.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_namespace_ptr		parent_nsp;

  ssep = &scope_stack[depth_scope_stack];
  check_assertion_str2(ssep->kind == (a_scope_kind)sck_namespace_extension ||
                       ssep->kind == (a_scope_kind)sck_namespace,
                       "pop_namespace_extension_scope:",
                       "entry not namespace extension");
  if (ssep->num_of_extra_times_pushed > 0) {
    /* This namespace had already been pushed when the call to
       push_namespace_extension_scope was done.  So, we don't want to
       actually pop the scope at this point.  Just decrement the count
       of excess pushes. */
    ssep->num_of_extra_times_pushed--;
  } else {
    /* Pop the reactivation scope. */
    parent_nsp = ssep->il_scope->variant.assoc_namespace->
                                           source_corresp.parent.namespace_ptr;
    pop_scope();
    if (parent_nsp != NULL) {
      /* A nested namespace.  Pop the enclosing namespaces too. */
      pop_namespace_extension_scope();
    }  /* if */
  }  /* if */
}  /* pop_namespace_extension_scope */


static
void set_template_decl_lookup_sequence(a_scope_depth initial_depth)
/*
If a namespace reactivation scope is pushed on top of a template
declaration scope some special processing needs to be done so that
name lookup works properly.  The namespace reactivation scopes must be
considered after the template declaration scope (which contains the
template parameters).  This is done by altering the previous_scope
links to reflect the desired name lookup sequence.  The following
table illustrates how the scope stack is updated as each scope is
pushed for this example:

  namespace A { namespace B { template <class T> void f(); } }
  template <class T> void A::B::f(){}
 
Each entry contains "N xxx (P)", where N is the scope depth, xxx is
the scope kind, and P is the previous scope for name lookup purposes.

  Initial                 After namespace A      After namespace B
  state                   is reactivated         is reactivated
  -----------------       -----------------      -----------------
                                                 3 ns react B (2)
                          2 ns react A (0)       2 ns react A (0)
  1 templ. decl. (0)      1 templ. decl. (2)     1 templ. decl. (3)
  0 file (none)           0 file (none)          0 file (none)

initial_depth is the depth of the template declaration scope.

This routine is also used when a template instantiation scope is
pushed for a Microsoft template specialization scope.  This is done
to make template parameters from a template declaration scope visible
inside of the instantiation scope pushed for the specialization.
*/
{
  a_scope_stack_entry_ptr	initial_ssep = &scope_stack[initial_depth];
  a_scope_stack_entry_ptr	curr_ssep = &scope_stack[depth_scope_stack];
  a_scope_stack_entry_ptr	prev_ssep = curr_ssep-1;

  if (initial_ssep == prev_ssep) {
    /* Make the previous scope for the first namespace reactivation
       point to the previous scope of the template declaration scope. */
    curr_ssep->previous_scope = initial_ssep->previous_scope;
  } else {
    /* Subsequent reactivation scopes will have their previous scope entry
       set to point to the template declaration scope.  Reset the previous
       pointer to point to the previous namespace reactivation. */
    curr_ssep->previous_scope = depth_scope_stack-1;
  }  /* if */
  /* Make the previous scope of the template declaration scope the innermost
     namespace reactivation scope. */
  initial_ssep->previous_scope = depth_scope_stack;
  /* Name lookups should begin at the template declaration scope. */
  depth_of_initial_lookup_scope = scope_depth_of(initial_ssep);
}  /* set_template_decl_lookup_sequence */


static void reset_template_decl_lookup_sequence(void)
/*
Undo the lookup sequence updates done by set_template_decl_lookup_sequence.
This is called when the template declaration scope is at the top of
the stack.
*/
{
  a_scope_stack_entry_ptr	ssep;

  ssep = &scope_stack[depth_scope_stack];
  ssep->previous_scope = depth_scope_stack-1;
  depth_of_initial_lookup_scope = depth_scope_stack;
}  /* reset_template_decl_lookup_sequence */


void f_push_namespace_reactivation_scope(
				a_namespace_ptr		nsp,
				a_boolean		force_new_entry)
/*
Push one or more scopes that will reactivate the indicated namespace.
This is used in contexts where the names from a namespace need to be
visible, but new members cannot be added to the namespace.
This routine is called only in C++.

If force_new_entry is TRUE, a new reactivation is pushed even if the
current scope is already a reactivation of the requested scope.
*/
{
  a_namespace_ptr		parent_nsp;
  a_namespace_ptr		curr_nsp = NULL;
  a_scope_depth			initial_depth = depth_scope_stack;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
  a_boolean			initial_scope_is_template_decl;

  initial_scope_is_template_decl = ssep->kind ==
                                        (a_scope_kind)sck_template_declaration;
  /* If the current scope is a namespace (or namespace extension) scope,
     see if it matches the one that we are pushing.  If so, don't actually
     push the scope, just increment the count of the number of excess
     pushes done on this scope. */
  if (ssep->kind == (a_scope_kind)sck_namespace ||
      ssep->kind == (a_scope_kind)sck_namespace_extension) {
    curr_nsp = ssep->il_scope->variant.assoc_namespace;
  }  /* if */
  if (curr_nsp == nsp && !force_new_entry) {
    /* The scope is already on the stack. */
    ssep->num_of_extra_times_pushed++;
  } else {
    /* The entry isn't on the stack.  Push any parent namespaces, then push
       the specified namespace. */
    parent_nsp = nsp->source_corresp.parent.namespace_ptr;
    if (parent_nsp != NULL) {
      /* A namespace nested in another namespace.  Push the parent
         namespace. */
      push_namespace_reactivation_scope(parent_nsp);
    }  /* if */
    /* Push an entry for the scope. */
    (void)push_namespace_scope((a_scope_kind)sck_namespace_reactivation, nsp);
    if (initial_scope_is_template_decl) {
      set_template_decl_lookup_sequence(initial_depth);
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ns_react_on_templ_decl")) {
      fprintf(f_debug, "Scope stack after namespace reactivation:\n");
      db_scope_stack();
    }  /* if */
#endif /* DEBUG */
  }  /* if */
}  /* f_push_namespace_reactivation_scope */


void pop_namespace_reactivation_scope(void)
/*
Pop one or more scopes pushed by push_namespace_reactivation_scope.
This routine is called only in C++.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_namespace_ptr		parent_nsp;

  ssep = &scope_stack[depth_scope_stack];
  check_assertion_str2(ssep->kind == (a_scope_kind)sck_namespace_extension ||
                       ssep->kind == (a_scope_kind)sck_namespace ||
                       ssep->kind == (a_scope_kind)sck_namespace_reactivation,
                       "pop_namespace_reactiveation_scope:",
                       "entry not reactivation extension");
  if (ssep->num_of_extra_times_pushed > 0) {
    /* This namespace had already been pushed when the call to
       push_namespace_reactivation_scope was done.  So, we don't want to
       actually pop the scope at this point.  Just decrement the count
       of excess pushes. */
    ssep->num_of_extra_times_pushed--;
  } else {
    /* Pop the reactivation scope. */
    parent_nsp = ssep->assoc_namespace->source_corresp.parent.namespace_ptr;
    pop_scope();
    if (parent_nsp != NULL) {
      /* A nested namespace.  Pop the enclosing namespaces too. */
      pop_namespace_reactivation_scope();
    }  /* if */
  }  /* if */
  ssep = &scope_stack[depth_scope_stack];
  if (ssep->kind == (a_scope_kind)sck_template_declaration) {
    /* When a namespace reactivation is placed on top of a template
       declaration scope, the template declaration scope will have had
       its previous pointer updated.  Restore it to the original value. */
    reset_template_decl_lookup_sequence();
  }  /* if */
}  /* pop_namespace_reactivation_scope */


static a_scope_depth reactivate_class_scope(a_type_ptr class_type,
					    a_boolean  extend_namespace)
/*
Push one or more scopes that will reactivate the indicated class type.
Return the scope depth prior to the reactivation of the first class
reactivation scope that is pushed.  If the class is a member of a namespace,
a namespace reactivation or extension scope is pushed (depending on the
value of extend_namespace).
*/
{
  a_symbol_ptr	class_sym;
  a_boolean	namespace_pushed = FALSE;
  a_scope_depth	orig_depth = NO_SCOPE_DEPTH;

  /* Get the symbol associated with the class. */
  class_sym = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  if (class_sym->is_class_member) {
    /* Nested class.  Push the containing class(es) first. */
    orig_depth = reactivate_class_scope(class_sym->parent.class_type,
                                        extend_namespace);
    /* Propagate the namespace pushed flag up to the innermost class
       reactivation scope. */
    namespace_pushed = scope_stack[depth_scope_stack].namespace_pushed;
  } else if (class_sym->parent.namespace_ptr != NULL) {
    /* The class is nested in a namespace -- push enclosing namespace(s). */
    a_namespace_ptr		parent_nsp;
    parent_nsp = class_sym->parent.namespace_ptr;
    /* The namespace is either extended or reactivated depending on the
       value of extend_namespace. */
    if (extend_namespace) {
      push_namespace_extension_scope(parent_nsp);
    } else {
      push_namespace_reactivation_scope(parent_nsp);
    }  /* if */
    namespace_pushed = TRUE;
  }  /* if */
  if (orig_depth == NO_SCOPE_DEPTH) orig_depth = depth_scope_stack;
  push_single_class_reactivation_scope(class_type);
  scope_stack[depth_scope_stack].namespace_pushed = namespace_pushed;
  return orig_depth;
}  /* reactivate_class_scope */


void push_instantiation_scope_for_class(
			a_type_ptr	class_type,
			a_boolean	is_microsoft_specialization_scope)
/*
Push a template instantiation scope for "class_type".  This is used
to reactivate a template instantiation scope after the class has been
instantiated, and in Microsoft mode to push an instantiate scope used
when a class specialization is defined (is_microsoft_specialization_scope
is TRUE in this case).  As a result, partial specializations need not be
taken into account (if the class has been instantiated, the class_template of
the class symbol supplement points to the partial specialization).
*/
{
  a_symbol_ptr				template_sym;
  a_template_arg_ptr			template_arg_list;
  a_template_decl_info_ptr		decl_info;
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				class_sym;

  /* Get the symbol associated with the class. */
  class_sym = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  /* Get a pointer to the symbol associated with the template from
     which this class was generated. */
  template_sym = template_symbol_for_class_symbol(class_sym);
  template_arg_list = templ_arg_list_for_class(class_type);
  /* Get the template declaration information associated with the class. */
  tssp = template_supplement_for_symbol(template_sym);
  decl_info = cache_for_template(tssp)->decl_info;
  if (is_microsoft_specialization_scope) {
    /* When pushing a Microsoft specialization scope, don't do the full
       instantiation scope processing.  This is done because the
       enclosing scopes should be visible for these cases (Microsoft
       specialization scopes are pushed for class scopes for
       explicitly specialized classes, and for class reactivation
       scopes for all template classes).  This has the effect of
       making the class's template parameters visible while possibly
       hiding a set of template parameters that really should have
       been used (as in the case of a definition of a member of a
       class template).  The Microsoft compiler actually has two sets
       of parameters visible (the incorrect ones and then the correct
       ones).  The Sun compiler has only the wrong ones visible, but
       we don't emulate that exactly (we do the same as in Microsoft
       mode). */
    if (class_type->source_corresp.is_class_member) {
      /* Reactivate the parent class. */
      a_type_ptr	parent_class;
      parent_class = class_type->source_corresp.parent.class_type;
      push_class_reactivation_scope(parent_class, /*entend_namespace=*/FALSE);
    } else if (class_type->source_corresp.parent.namespace_ptr != NULL) {
      /* Reactivate the parent namespace.  A new entry is forced because we
         later must be able to pop back to the previous scope state based
         only on the scope depth. */
      f_push_namespace_reactivation_scope(
                             class_type->source_corresp.parent.namespace_ptr,
                             /*force_new_entry=*/TRUE);
    }  /* if */
    push_simple_instantiation_scope(decl_info, class_type,
                                    (a_routine_ptr)NULL, class_sym,
                                    template_sym, template_arg_list,
				    PS_MICROSOFT_SPECIALIZATION);
    scope_stack[depth_scope_stack].nested_instantiation = TRUE;
  } else {
    a_push_scope_options_set		options;
    options = PS_NO_OPTIONS;
    if (is_prototype_instantiation_symbol(class_sym)) {
      options |= PS_PROTOTYPE_INSTANTIATION;
    }  /* if */
    push_template_instantiation_scope(decl_info, class_type,
                                      (a_routine_ptr)NULL, class_sym,
                                      template_sym, template_arg_list,
				      /*push_stop_tokens=*/FALSE,
				      options);
  }  /* if */
}  /* push_instantiation_scope_for_class */


void push_class_and_template_reactivation_scope(
                                 a_type_ptr	class_type,
                                 a_boolean      reactivate_template_params,
				 a_boolean	extend_namespace)
/*
Push the scopes needed to reactivate the context of the specified class.
If the class is a template class, or a class defined within a template class,
this involves pushing the necessary template instantiation scopes as well.
If reactivate_template_params is FALSE, the template instantiation scopes
are not reactivated.  This is FALSE when called for normal class
reactivations.  If the class is a member of a namespace, a namespace
reactivation or extension scope is pushed (depending on the value of
extend_namespace).
*/
{
  a_boolean	is_template = FALSE;
  a_symbol_ptr	class_sym;
  a_scope_depth	orig_depth = depth_scope_stack;
  a_scope_depth	initial_depth = depth_scope_stack;
  a_boolean	is_microsoft_specialization_scope = FALSE;
  a_boolean	initial_scope_is_template_decl = FALSE;
  a_scope_depth	saved_innermost_scope_that_affects_access;

  saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
  /* Get the symbol associated with the class. */
  class_sym = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  check_assertion_str2(class_sym != NULL,
                       "push_class_and_template_reactivation_scope:",
                       "class type has NULL assoc_info");
  /* Template instantiation scopes must be pushed for template
     instances.  Normally, instantiation scopes are not pushed for
     specialized classes.  But in Microsoft mode, an instantiation scope
     is pushed because the template parameters are visible, even in
     specializations. */
  if (is_any_template_instance_class_symbol(class_sym)) {
    is_template = ((!is_template_instance_specific_def_symbol(class_sym) &&
                  reactivate_template_params));
    if (use_microsoft_specialization_scope) {
      /* Determine whether the instantiation scope is being pushed only
         because we are in Microsoft mode. */
      is_microsoft_specialization_scope = !is_template;
      is_template = TRUE;
    }  /* if */
    if (is_microsoft_specialization_scope) {
      a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
      initial_scope_is_template_decl = ssep->kind ==
                                        (a_scope_kind)sck_template_declaration;
    }  /* if */
  }  /* if */
  if (is_template) {
    /* The push of the template instantiation scope will not reactivate the
       class type (that it thinks is being instantiated).  Reactivate it
       now. */
    a_scope_stack_entry_ptr	ssep;
    push_instantiation_scope_for_class(class_type,
                                       is_microsoft_specialization_scope);
    ssep = scope_stack_entry_for(depth_scope_stack);
    if (initial_scope_is_template_decl &&
        !class_type->source_corresp.is_class_member) {
      /* In Microsoft mode, if a specialization instantiation scope is
         pushed inside a template declaration scope, the previous pointers
         need to be updated in order for name lookup to consider the
         template declaration scope at the appropriate time.  This is not
         done for member classes, because it will have already been done for
         the parent class. */
      set_template_decl_lookup_sequence(initial_depth);
    }  /* if */
    push_single_class_reactivation_scope(class_type);
    /* Indicate that a template instantiation scope was pushed so that,
       when popping the class and template reactivation, we know how the
       scopes should be popped. */
    ssep = scope_stack_entry_for(depth_scope_stack);
    ssep->instantiation_scope_pushed = TRUE;
    ssep->microsoft_specialization_scope_pushed = TRUE;
  } else {
    /* A nontemplate class.  Just do a normal class reactivation.  The
       original depth returned is used instead of the one saved above
       because the original depth should not include any namespace
       reactivation scopes that may have been pushed. */
    orig_depth = reactivate_class_scope(class_type, extend_namespace);
  }  /* if */
  {
    /* Record the scope depth prior to the reactivation so that the scopes
     reactivated can be popped later. */
    a_scope_stack_entry_ptr	ssep;
    ssep = scope_stack_entry_for(depth_scope_stack);
    ssep->orig_depth = orig_depth;
    ssep->saved_innermost_scope_that_affects_access =
                                     saved_innermost_scope_that_affects_access;
  }
}  /* push_class_and_template_reactivation_scope */


void push_class_reactivation_scope(a_type_ptr class_type,
				   a_boolean  extend_namespace)
/*
Push the scopes needed to reactivate the context of the specified class.
This is an interface to push_class_and_template_reactivation_scope that
causes only the class reactivation (and not any template instantiation
scopes) to be pushed.  If the class is a member of a namespace, a
namespace reactivation or extension scope is pushed (depending on
the value of extend_namespace).
*/
{
  push_class_and_template_reactivation_scope
                           (class_type, /*reactivate_template_params=*/FALSE,
                            extend_namespace);
}  /* push_class_reactivation_scope */


void pop_class_reactivation_scope(void)
/*
Pop one or more scopes pushed by push_class_reactivation_scope.  This routine
is called only in C++.
*/
{
  a_scope_stack_entry_ptr	ssep;
  a_scope_depth			orig_depth;
  a_boolean			namespace_pushed = FALSE;
  a_boolean			microsoft_specialization_scope_pushed;
  a_boolean			instantiation_scope_pushed;
  a_scope_depth			saved_innermost_scope_that_affects_access;

  ssep = &scope_stack[depth_scope_stack];
  microsoft_specialization_scope_pushed =
                                   ssep->microsoft_specialization_scope_pushed;
  namespace_pushed = ssep->namespace_pushed;
  instantiation_scope_pushed = ssep->instantiation_scope_pushed;
  orig_depth = scope_stack[depth_scope_stack].orig_depth;
  saved_innermost_scope_that_affects_access =
      scope_stack[depth_scope_stack].saved_innermost_scope_that_affects_access;
  /* Pop scopes until the depth of the scope stack is equal to orig_depth,
     which is the depth before any of the class reactivation scopes were
     pushed. */
  check_assertion_str2(orig_depth != NO_SCOPE_DEPTH,
                       "pop_class_reactivation_scope:",
                       "invalid orig_depth");
  /* Clear the information about any using-directives currently in effect. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/FALSE,
                                     NO_DECL_SEQUENCE_NUMBER);
  /* Do the actual popping of the scopes. */
  while (orig_depth < depth_scope_stack) pop_scope();
  /* Restore the using-directive information to the appropriate state. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/TRUE,
                                     get_effective_decl_seq());
  if (instantiation_scope_pushed) {
    if (microsoft_specialization_scope_pushed &&
        scope_stack[depth_scope_stack].kind ==
                                     (a_scope_kind)sck_template_declaration) {
      /* When a Microsoft specialization scope is placed on top of a template
         declaration scope, the template declaration scope will have had
         its previous pointer updated.  Restore it to the original value. */
      reset_template_decl_lookup_sequence();
    }  /* if */
  } else {
    /* Pop the class and namespace reactivation scopes. */
    if (namespace_pushed) {
      /* The class is nested in a namespace -- pop enclosing namespace(s).
         The enclosing scopes could have been pushed as either extension
         or reactivation scopes.  Pop the appropriate kind of scope. */
      if (scope_stack[depth_scope_stack].kind ==
                                      (a_scope_kind)sck_namespace_extension) {
        pop_namespace_extension_scope();
      } else {
        pop_namespace_reactivation_scope();
      }  /* if */
    }  /* if */
  }  /* if */
  /* Restore the innermost scope that affects access.  The current value
     could be incorrect if if the reactivation involved pushing instantiation
     scopes. */
  depth_of_innermost_scope_that_affects_access_control =
                                    saved_innermost_scope_that_affects_access;
}  /* pop_class_reactivation_scope */

#if DEBUG

unsigned long db_show_scope_stack_space_used(unsigned long grand_total)
/*
Show space used by the scope_stack routines.  This is called by
the symbol table space used routine.  The space used by the scope_stack
routines is reported as part of the symbol table memory used.
*/
{
#if DO_IL_LOWERING
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used_lost("string literal tables", avail_string_literal_tables,
                     num_string_literal_tables_allocated,
                     a_string_literal_table);
  db_space_used_lost("string literal entries",
                     avail_string_literal_table_entries,
                     num_string_literal_table_entries_allocated,
                     a_string_literal_table_entry);
#endif /* DO_IL_LOWERING */
  return grand_total;
}  /* db_show_scope_stack_space_used */

#endif /* DEBUG */

void scope_stk_one_time_init(void)
/*
Do one-time initialization of variables related to scope stack management.
(Variables that need to be reinitialized with each new translation unit
are handled in scope_stk_init.)
*/
{
  /* Save variables from scope_stk.h and scope_stk.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(num_classes_on_scope_stack),
      pch_saved_var_array_elem(avail_names_hidden_by_old_for_init),
      pch_saved_var_array_elem(name_linkage_stack),
      pch_saved_var_array_elem(avail_name_linkage_stack_entries),
      pch_saved_var_array_elem(
                  function_body_processing_delayed_on_some_func_in_primary_il),
#if IA64_ABI && NEED_NAME_MANGLING
      pch_saved_var_array_elem(avail_collision_tables),
#endif /* IA64_ABI && NEED_NAME_MANGLING */
#if DO_IL_LOWERING
      pch_saved_var_array_elem(avail_string_literal_tables),
      pch_saved_var_array_elem(avail_string_literal_table_entries),
#if DEBUG
      pch_saved_var_array_elem(num_string_literal_tables_allocated),
      pch_saved_var_array_elem(num_string_literal_table_entries_allocated),
#endif /* DEBUG */
#endif /* DO_IL_LOWERING */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(scope_stack);
  register_trans_unit_variable(size_scope_stack);
  register_trans_unit_variable(depth_scope_stack);
  register_trans_unit_variable(depth_of_initial_lookup_scope);
  register_trans_unit_variable(decl_scope_level);
  register_trans_unit_variable(depth_innermost_function_scope);
  register_trans_unit_variable(innermost_function_scope);
  register_trans_unit_variable(depth_innermost_instantiation_scope);
  register_trans_unit_variable(depth_template_declaration_scope);
  register_trans_unit_variable(curr_deferred_access_scope);
  register_trans_unit_variable(inside_local_class);
  register_trans_unit_variable(depth_innermost_namespace_scope);
  register_trans_unit_variable(
                         depth_of_innermost_scope_that_affects_access_control);
  register_trans_unit_variable(num_classes_on_scope_stack);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  register_trans_unit_variable(source_sequence_entries_disallowed);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* scope_stk_one_time_init */


void scope_stk_trans_unit_init(void)
/*
Initialize variables related to scope stack processing that are specific to a
given translation unit.
*/
{
  scope_stack = NULL;
  size_scope_stack = 0;
  depth_scope_stack = NO_SCOPE_DEPTH;
  depth_of_initial_lookup_scope = NO_SCOPE_DEPTH;
  decl_scope_level = NO_SCOPE_DEPTH;
  depth_innermost_function_scope = NO_SCOPE_DEPTH;
  innermost_function_scope = NULL;
  depth_innermost_instantiation_scope = NO_SCOPE_DEPTH;
  depth_template_declaration_scope = NO_SCOPE_DEPTH;
  curr_deferred_access_scope = NO_SCOPE_DEPTH;
  inside_local_class = FALSE;
  depth_innermost_namespace_scope = NO_SCOPE_DEPTH;
  depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
  num_classes_on_scope_stack = 0;
  /* Source sequence entries are always suppressed when compiling secondary
     translation units. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DO_IL_LOWERING
  source_sequence_entries_disallowed = !is_primary_translation_unit ||
                                       il_lowering_needed();
#else /* !DO_IL_LOWERING */
  source_sequence_entries_disallowed = !is_primary_translation_unit;
#endif /* DO_IL_LOWERING */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* scope_stk_trans_unit_init */


void scope_stk_init(void)
/*
Initialize static variables related to scope stack management.  This is
done as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
  depth_of_initial_lookup_scope = NO_SCOPE_DEPTH;
  num_classes_on_scope_stack = 0;
  avail_names_hidden_by_old_for_init = NULL;
  name_linkage_stack = NULL;
  avail_name_linkage_stack_entries = NULL;
#if IA64_ABI && NEED_NAME_MANGLING
  avail_collision_tables = NULL;
#endif /* IA64_ABI && NEED_NAME_MANGLING */
#if DO_IL_LOWERING
  avail_string_literal_tables = NULL;
  avail_string_literal_table_entries = NULL;
#if DEBUG
  num_string_literal_table_entries_allocated = 0;
  num_string_literal_tables_allocated = 0;
#endif /* DEBUG */
#endif /* DO_IL_LOWERING */
  function_body_processing_delayed_on_some_func_in_primary_il = FALSE;
}  /* scope_stk_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
