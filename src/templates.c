/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1996 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

templates.c -- Support for C++ templates.

*/

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
#include "disambig.h"
#include "lower_name.h"
#include "statements.h"


#if AUTOMATIC_TEMPLATE_INSTANTIATION
typedef struct an_instance_lookup_entry *an_instance_lookup_entry_ptr;
typedef struct an_instance_lookup_entry {
  /* Structure used to represent entries in the hash table of template
     instantiation names.  This is used to match entries from the
     instantiation list file with entries on the compilers instantiation
     required list. */
  an_instance_lookup_entry_ptr
		next;
			/* Pointer to the next instance in a given hash
			   table bucket. */
  char		*name;
			/* Name of the instance.  This is the name read from
			   the instantiation list file.  This will typically
			   be the mangled name of the function or static
			   data member. */
} an_instance_lookup_entry;

#define INSTANCE_LOOKUP_TABLE_SIZE 127
			/* The number of buckets in the instance lookup table.
			   This number should be prime. */

static an_instance_lookup_entry_ptr
		instance_lookup_table[INSTANCE_LOOKUP_TABLE_SIZE];
			/* Each element of the array points to a list of
			   entries associated with instantiations that hashed
			   to a given group. */

#define HASH_FACTOR 73
			/* The multiplier used in the hash algorithm that
			   generates an index in the hash table from an
                           identifier name string.
			   Do not change without investigating the
			   hash table performance that results.  Prime
			   values are likely to work better than
			   non-prime values. */
#define INFO_FILE_LINE_INCREMENTAL_ALLOCATION 256
			/* The number of bytes added to the information file
			   input line each time it is reallocated; also the
			   initial allocation. */

static a_boolean
		any_instantiations_required;
			/* TRUE if there are any template instantiations
			   needed for this compilation.  This is TRUE
			   whether or not the instantiations are provided
			   by this file.  This is used to determine whether
			   to create an instantiation information file. */

static char	*instantiation_info_file_name;
                        /* The name of a file containing a list of names
			   of template functions and static data members to
			   be instantiated.  Intended to be used for linker
			   feedback mechanisms to provide automatic
			   instantiation. */
static FILE	*f_instantiation_info;
			/* File from which the instantiation list should be
			   read.  Only valid when do_auto_instantiation is
			   TRUE. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

typedef struct a_can_instantiate_entry *a_can_instantiate_entry_ptr;
typedef struct a_can_instantiate_entry {
  /* Structure used to build a list of classes that have been used
     in can_instantiate pragmas.  This is used during instantiation
     wrapup to instantiate classes that have not been otherwise used.
     The instantiation needs to be delayed so that any template entities
     generated as a result of the instantiation of the class can be
     specially flagged. */
  a_can_instantiate_entry_ptr
		next;
			/* Pointer to the next instance in a given hash
			   table bucket. */
  a_type_ptr	class_type;
			/* Pointer to the template class type to
			   be instantiated later. */
} a_can_instantiate_entry;

static a_can_instantiate_entry_ptr can_instantiate_list;
	
static a_def_arg_expr_fixup_ptr	curr_default_args;
			/* Pointer to the default argument entries for
                           the function template being scanned. */

static a_template_instance_ptr instantiations_required;
			/* Points to the first entry on a list of template
			   instance entries for which either function
			   instantiations or compiler-generated static data
			   member definitions are required.  Entries are
			   added to the end of the list.  */

static a_template_instance_ptr instantiations_required_tail;
			/* Points to the end of the instantiations_required
			   list; needed because entries added to this list
			   must be added at the end. */

static a_boolean
		in_instantiation_wrapup;
			/* TRUE when instantiation wrapup has been called
			   to do end-of-compilation unit instantiations. */

static a_boolean
		entries_updated_during_instantiation_wrapup;
			/* TRUE when entries on the instantiation required
			   list have their instantiation required flag
			   set during instantiation wrapup.  This is used to
			   detect situations when an entry that may have
			   already been visited by instantiation wrapup
			   has its instantiation required flag updated
			   while processing an entry later on the list. */

static a_symbol_list_entry_ptr
		deferred_instantiations;
			/* A list of symbol entries for instantiations that
			   were deferred while a class definition was
			   pending.  These entities are instantiated when
			   a class definition is no longer pending. */

static a_symbol_list_entry_ptr
		deferred_instantiations_tail;
			/* The end of the deferred_instantiations list. */

/*
Structure used to pass information about the current template declaration
between the routines used to implement the processing of template
declarations.
*/
typedef struct a_decl_state *a_decl_state_ptr;
typedef struct a_decl_state {
  a_boolean	is_template_friend;
			/* TRUE if this is a friend declaration. */
  a_boolean	is_member_decl;
			/* TRUE if this declaration appeared in a class
			   scope. */
  a_boolean	is_specialization;
			/* TRUE if the declaration is a specialization.
			   A specialization contains one or more template
			   parameter clauses with empty parameter lists. */
  a_boolean	is_full_specialization;
			/* TRUE if the declaration is a full specialization
			   of a template entity.  A full specialization
			   declares a real function or class (i.e., not a
			   template).  In a full specialization all the
			   template parameter clauses contain empty parameter
			   lists (i.e., "template <>"). */
  a_boolean	defines_something;
			/* TRUE if the declaration is a definition. */
  a_boolean	in_prototype_instantiation;
			/* TRUE if the declaration is being processed as
			   part of the prototype instantiation of an
			   enclosing class template. */
  a_boolean	decl_scope_err;
			/* TRUE if the template declaration is invalid in the
			   current scope. */
  a_boolean	no_advance_past_final_token;
			/* TRUE if the right brace or semicolon terminating
			   the template declaration is left as the current
			   token. */
  an_access_specifier
		access;
			/* When the declaration appears in a class scope,
			   contains the current access. */
  a_template_nesting_depth
		nesting_depth;
			/* Nesting depth of this template declaration (i.e.,
			   the number of enclosing template scopes.  The
			   outermost template declaration has a nesting
			   depth of 1. */
  a_template_decl_info_ptr
		decl_info;
			/* Points to the template declaration information
			   associated with the innermost template declaration
			   scope.  Contains NULL for full specializations. */
  a_scope_depth	effective_decl_level;
			/* The scope depth of the scope containing the
			   template declaration.  This is initially set
			   to the scope that contains the template
			   declaration and may be adjusted later for
			   friend declarations. */
  unsigned long	number_of_template_decl_scopes;
			/* The number of template declaration scopes pushed
			   while processing this template declaration. */
  unsigned long	number_of_template_param_clauses;
			/* The number of template parameter clauses (including
			   ones with empty parameter lists in specialization
			   declarations) in the current template
                           declaration. */
  a_scope_ptr	enclosing_scope;
			/* Points to the scope entry for the scope that
			   contains the template declaration. */
  a_type_ptr	class_declared_in;
			/* When the template definition appears in a class
			   scope, this points to the class type of the
			   enclosing class, otherwise contains NULL. */
  a_source_position
		start_pos;
			/* Source position of the first token of the
			   template declaration. */
  a_token_cache	param_list_cache;
			/* Token cache containing the template parameter
			   list(s). */
  a_token_cache	decl_token_cache;
			/* Token cache containing the template declaration
			   (the portion that follows the template parameter
			   list(s)). */
  a_boolean	decl_token_cache_used;
			/* TRUE if the declaration token cache was saved as
			   part of the template that was declared. */
  a_pending_pragma_ptr
		pragmas_bound_to_template;
			/* A list of next-construct pragmas that appeared
			   before this template declaration. */
#if RECORD_TEMPLATES_IN_IL
  a_template_ptr
		il_template_entry;
			/* Pointer to the IL template entry created for this
			   template declaration, or NULL if no entry has been
			   created. */
#endif /* RECORD_TEMPLATES_IN_IL */
} a_decl_state;


static void init_templ_decl_state(a_decl_state_ptr	tdsp)
/*
Initialize a template declaration state block.
*/
{
  tdsp->is_template_friend = FALSE;
  tdsp->is_member_decl = FALSE;
  tdsp->is_specialization = FALSE;
  tdsp->is_full_specialization = FALSE;
  tdsp->defines_something = FALSE;
  tdsp->in_prototype_instantiation = FALSE;
  tdsp->decl_scope_err = FALSE;
  tdsp->no_advance_past_final_token = FALSE;
  tdsp->access = (an_access_specifier)as_public;
  tdsp->nesting_depth = 0;
  tdsp->decl_info = NULL;
  tdsp->number_of_template_decl_scopes = 0;
  tdsp->number_of_template_param_clauses = 0;
  tdsp->enclosing_scope = NULL;
  tdsp->class_declared_in = FALSE;
  tdsp->start_pos = null_source_position;
  tdsp->pragmas_bound_to_template = NULL;
  clear_token_cache(&tdsp->param_list_cache, /*reusable=*/TRUE);
  clear_token_cache(&tdsp->decl_token_cache, /*reusable=*/TRUE);
  tdsp->decl_token_cache_used = FALSE;
#if RECORD_TEMPLATES_IN_IL
  tdsp->il_template_entry = NULL;
#endif /* RECORD_TEMPLATES_IN_IL */
}  /* init_templ_decl_state */


#if RECORD_TEMPLATES_IN_IL
/*
Static variables and routines that are used to build up a string representation
of a template.  Characters are added to the temp_text_buffer, which is
indefinitely expandable, and then a string is created -- a copy of the
assigned portion of the buffer, with a null terminator appended -- that
the IL template entry can point to.
*/
static a_seq_number
		curr_seq;
			/* The sequence number of the source position of the
			   most recently added token, etc., in the buffer. */

static an_il_to_str_output_control_block
		octl;
			/* Output control block used to interface to the
			   il_to_str routines. */


static void add_whitespace_to_template_string(a_seq_number     seq_incr,
                                              a_column_number  column_incr)

/*
Add seq_incr newline characters and column_incr blanks to temp_text_buffer,
incrementing pos_in_temp_text_buffer accordingly.
*/
{
  for (; seq_incr > 0; --seq_incr) {
    put_ch_to_temp_text_buffer('\n');
  }  /* for */
  for (; column_incr > 0; --column_incr) {
    put_ch_to_temp_text_buffer(' ');
  }  /* for */
}  /* add_whitespace_to_template_string */


static void add_token_to_template_string(void)
/*
Copy characters representing the current token into temp_text_buffer, and
increase pos_in_temp_text_buffer by the number of characters added.
*/
{
  a_seq_number     seq_incr;
  a_column_number  column_incr;

  db_enter(5, "add_token_to_template_string");
  if (pos_curr_token.seq <= curr_seq) {
    /* We're on the same line as the previous token processed, so just add
       a space (in most cases) to separate the tokens.  (Note: the line for
       the current token may be less than curr_seq when a macro expansion
       occurs.  Treat the token as being on the current line.) */
    if (curr_token == tok_comma || curr_token == tok_semicolon ||
        pos_in_temp_text_buffer == 0) {
      /* No space is needed before a comma or semicolon -- or if this is
         the very first token of the declaration. */
      column_incr = 0;
    } else {
      check_assertion(pos_in_temp_text_buffer > 0 ||
                      curr_seq < pos_curr_token.seq);
      /* Add a single space. */
      column_incr = 1;
    }  /* if */
    /* Don't add a line feed. */
    seq_incr = 0;
  } else {
    /* We've moved to a new line.  Compute the indentation. */
    column_incr = pos_curr_token.column - 1;
    /* Compute the number of line feed characters to add. */
    seq_incr =  pos_curr_token.seq - curr_seq;
    /* Reset the current line. */
    curr_seq = pos_curr_token.seq;
  }  /* if */
  /* Add any spaces and line feeds that might be required. */
  if (seq_incr > 0 || column_incr > 0) {
    add_whitespace_to_template_string(seq_incr, column_incr);
  }  /* if */
  /* Now put out the characters representing the token. */
  if (curr_token == tok_int_constant || curr_token == tok_float_constant ||
      curr_token == tok_string_literal || curr_token == tok_char_constant) {
    /* Write out a string that represents the constant. */
    if (const_for_curr_token.kind == (a_constant_repr_kind)ck_error) {
      /* If there was an error in scanning the token, reset the flag to avoid
         an assertion failure in the subroutine.  This means something like
         "<error-const>" will be put out in the template string. */
      octl.gen_compilable_code = FALSE;
    }  /* if */
    form_constant(&const_for_curr_token, /*need_parens=*/TRUE, &octl);
    /* Reset the flag, in case it had been changed. */
    octl.gen_compilable_code = TRUE;
  } else if (curr_token == tok_newline) {
    /* Ignore tok_newline.  It only comes up in pragma token caches, and
       when the sequence number changes the required number of newline
       characters will be added to the cache anyway. */
#if CHECKING
    /* Check for tokens that should not show up in the cached tokens of
       a template declaration. */
  } else if (curr_token == tok_end_of_source ||
             curr_token == tok_header_name ||
             curr_token == tok_pp_number ||
             curr_token == tok_digit_sequence ||
             curr_token == tok_cpp_quote ||
             curr_token == tok_ptr_to_member) {
    internal_error("add_token_to_template_string: unexpected token");
#endif /* CHECKING */
  } else if (curr_token == tok_identifier) {
    /* An identifier. */
    check_assertion(!locator_for_curr_id.has_been_coalesced);
    put_str_to_temp_text_buffer(locator_for_curr_id.symbol_header->
                                                               identifier);
  } else {
    /* A keyword or other token whose literal name can be put out. */
    put_str_to_temp_text_buffer(token_names[(int)curr_token]);
  }  /* if */    
  db_exit();
}  /* add_token_to_template_string */


static void add_curr_token_pragmas_to_template_string(void)
/*
If the current token has any pragmas associated with it, add strings to
represent them to temp_text_buffer.  Note that all pragmas that are
encountered, whatever their other characteristics, are included.
*/
{
  a_pending_pragma_ptr  ppp;
  a_boolean             is_pseudo_pragma;
  a_seq_number          seq_incr;
  a_column_number       column_incr;

  db_enter(5, "add_curr_token_pragmas_to_template_string");
  for (ppp = curr_token_pragmas; ppp != NULL; ppp = ppp->next) {
    if (ppp->pragma_position.seq <= curr_seq) {
      /* We're on the same line as the previous token processed, so just add
         a space (in most cases) to separate the tokens.  (Note: the line for
         the current token may be less than curr_seq when a macro expansion
         occurs.  Treat the token as being on the current line.) */
      column_incr = 1;
      /* Don't add a line feed. */
      seq_incr = 0;
    } else {
      /* We've moved to a new line.  Compute the indentation. */
      column_incr = ppp->pragma_position.column - 1;
      /* Compute the number of line feed characters to add. */
      seq_incr =  ppp->pragma_position.seq - curr_seq;
      /* Reset the current line. */
      curr_seq = ppp->pragma_position.seq;
    }  /* if */
    /* Add any spaces and line feeds that might be required. */
    if (seq_incr > 0 || column_incr > 0) {
      add_whitespace_to_template_string(seq_incr, column_incr);
    }  /* if */
    is_pseudo_pragma = ppp->descr_ptr->is_pseudo_pragma;
    if (is_pseudo_pragma) {
      /* Add comment delimiter to the template string. */
      put_str_to_temp_text_buffer("/*");
    } else {
      /* Add "#pragma " to the template string. */
      put_str_to_temp_text_buffer("#pragma ");
    }  /* if */
    if (ppp->descr_ptr->make_text_not_tokens) {
      /* Note: the pragma id is already part of pragma_text. */
      check_assertion(ppp->pragma_text != NULL);
      put_str_to_temp_text_buffer(ppp->pragma_text);
    } else {
      /* The pragma id is not part of pragma_text, so it has to be added
         explicitly. */
      put_str_to_temp_text_buffer(pragma_ids[(int)ppp->descr_ptr->kind]);
      if (ppp->token_cache.first_token != NULL) {
	/* Activate the cache and then go through each of its tokens. */
	rescan_reusable_cache(&ppp->token_cache);
	while (curr_token != tok_end_of_source) {
	  add_token_to_template_string();
	  /* Advance to the next token in the cache. */
	  (void)get_token();
	}  /* while */
	/* Advance past the end-of-source token. */
	flush_past_token_cache_terminator();
      }  /* if */
    }  /* if */
    if (is_pseudo_pragma) {
      /* Add terminating comment delimiter to the template string. */
      put_str_to_temp_text_buffer("*/");
    }  /* if */
  }  /* for */
  db_exit();
}  /* add_curr_token_pragmas_to_template_string */


static void make_template_string(a_template_ptr  template_ptr,
                                 a_token_cache   *template_param_list_cache,
                                 a_token_cache   *template_decl_cache,
                                 a_token_cache   *template_body_cache)
/*
Go through the three token caches and build a string representation of the
template in a buffer; the string should correspond closely to what the user
wrote in the source program, except for comments (excluded) and formatting
(only line feeds and indentation are preserved).  Then allocate a string of
the appropriate size, copy the contents of the buffer into it, and update
the "text" field of *template_ptr to point to it.
*/
{
  a_token_cache         *cache;
  char                  *il_string;

  db_enter(3, "make_template_string");
  /* curr_seq is initialized to the line on which the template declaration
     begins. */
  curr_seq = template_ptr->source_corresp.decl_position.seq;
  pos_in_temp_text_buffer = 0;
  /* The outer loop goes though the three token caches in order, beginning
     with "template < ... >". */
  cache = template_param_list_cache;
  for (;;) {
    check_assertion(cache != NULL);
    /* Activate the cache and then go through each of its tokens. */
    rescan_reusable_cache(cache);
    while (curr_token != tok_end_of_source) {
      if (curr_token_pragmas != NULL) {
        /* One or more pragmas appeared immediately before the current token.
           Add them to the template string too. */
        add_curr_token_pragmas_to_template_string();
        /* Clear the curr_token_pragmas pointer, since those pragmas should
           not actually be processed at this time. */
        curr_token_pragmas = NULL;
      }  /* if */
      /* Now add something to the template string to represent the current
         token. */
      add_token_to_template_string();
      /* Advance to the next token in the cache. */
      (void)get_token();
    }  /* while */
    /* Advance past the end-of-source token. */
    flush_past_token_cache_terminator();
    /* Advance to the next token cache. */
    if (cache == template_param_list_cache) {
      cache = template_decl_cache;
    } else if (cache == template_decl_cache) {
      cache = template_body_cache;
      /* There will be no cache for the token body if no body was declared --
         in which case, terminate the loop. */
      if (cache == NULL || cache->first_token == NULL) break;
    } else {
      /* All done. */
      break;
    }  /* if */
  }  /* for */
  if ((template_body_cache != NULL &&
       template_body_cache->first_token != NULL) &&
      (template_ptr->kind == (a_template_kind)templk_function ||
       template_ptr->kind == (a_template_kind)templk_member_function)) {
    /* Function template definition -- no semicolon needed. */
  } else {
    /* Terminate the string with a semicolon (which will not have been
       included among the cached tokens). */
    put_ch_to_temp_text_buffer(';');
  }  /* if */
  /* Allocate a block of file scope IL memory into which the string may
     be copied. */
  il_string = (char *)alloc_il((sizeof_t)(pos_in_temp_text_buffer + 1));
  (void)memcpy(il_string, temp_text_buffer,
               size_t_arg(pos_in_temp_text_buffer));
  /* Add a null terminator. */
  il_string[pos_in_temp_text_buffer] = '\0';
  template_ptr->text = il_string;
#if DEBUG
  if (debug_level >= 3 ||
      (db_active && db_flag_is_set("dump_template_strings"))) {
    /* This won't work if the string contains nulls -- is it worth fixing? */
    fprintf(f_debug, "Saved template string:\n%s\n", il_string);
  }  /* if */
#endif /* DEBUG */
  
  db_exit();
}  /* make_template_string */


static a_template_ptr make_il_template_entry(a_source_position *start_pos)
/*  
Allocate an IL template entry.  The source position specified by start_pos
(which should be the first tok_template keyword of the declaration) serves
as the decl_position of the template declaration as a whole.
*/
{
  a_template_ptr  tp;

  db_enter(3, "make_il_template_entry");
  tp = alloc_template();
  tp->source_corresp.decl_position = *start_pos;
  add_to_templates_list(tp);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* There's not yet a name or symbol for the template declaration, so call
     update_source_sequence_list directly. */
  update_source_sequence_list((char *)tp, (an_il_entry_kind)iek_template,
                              (a_source_sequence_entry_ptr)NULL);
  if (depth_scope_stack == depth_innermost_namespace_scope) {
    /* Set the source-sequence insert point for instantiations to NULL -- no
       instantiations should be inserted before it. */
    scope_stack[DEPTH_OF_FILE_SCOPE].ss_list_instantiation_insert_point = NULL;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return tp;
}  /* make_il_template_entry */

#endif /* RECORD_TEMPLATES_IN_IL */

static void set_instantiation_required_for_template_class_members
						(a_type_ptr	class_type)
/*
Calls update_instantiation_required_flag for all member functions and
static data members declared in the class.  This needs to be called after
the class instantiation is complete so that the function or static data
member instantiation has access to the complete class.  This routine calls
itself recursively to process classes nested within this class.
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_variable_ptr		var;
  a_symbol_ptr			sym;
  a_template_instance_ptr	tip;
  a_routine_ptr			rout;
  a_type_ptr			type;

  db_enter(4, "set_instantiation_required_for_template_class_members");  
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* The assoc_scope pointer can be NULL if errors occurred during the
     instantiation of the class. */
  if (ctsp->assoc_scope != NULL) {
    /* Function instantiation entries are not marked for actual instantiation
       (that is, for generation of the function body) until there is
       an invocation of the function.  However, if the function is
       virtual, mark it for instantiation in all cases, since a
       virtual function table may have to be put out for it.  All
       instances are placed on the instantiation list.  In tim_all
       mode the instantiations will be generated even if the
       instantiation required flag is not set. */
    rout = ctsp->assoc_scope->routines;
    while (rout != NULL) {
      sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
      tip = sym->variant.routine.instance_ptr;
      if (tip != NULL && !tip->instantiation_required) {
        /* Under certain conditions the instance pointer will be NULL.  This
           occurs for compiler generated routines and under some error
           conditions.  Simply skip this routine. */
        update_instantiation_required_flag(
                       tip, (a_boolean)(sym->variant.routine.ptr->is_virtual),
                       /*defer_inline=*/TRUE);
      }  /* if */
      rout = rout->next;
    }  /* while */
    
    /* Static data members are eligible for a compiler-generated definition
       only if a template definition appears in the source.  However, it
       still needs to appear on the instantiation-required list (because
       instantiation is required required somewhere in the program even if
       not in the current translation unit). */
    var = class_type->variant.class_struct_union.extra_info->
							assoc_scope->variables;
    while (var != NULL) {
      sym = (a_symbol_ptr)var->source_corresp.assoc_info;
      tip = sym->variant.static_data_member.instance_ptr;
#if 0
      /* Are there error cases when tip can be NULL?  It is probably safer
         to skip setting the instantiation required flag rather than
         generate a possibly spurious internal error. */
#endif /* 0 */
      if (tip != NULL && !tip->instantiation_required) {
        update_instantiation_required_flag(tip, /*value=*/TRUE,
                                           /*defer_inline=*/TRUE);
      }  /* if */
      var = var->next;
    }  /* while */
    /* Process any classes nested within this class. */
    type = ctsp->assoc_scope->types;
    while (type != NULL) {
      a_type_kind	tk = type->kind;
      if (tk == (a_type_kind)tk_class ||
          tk == (a_type_kind)tk_struct || tk == (a_type_kind)tk_union) {
        set_instantiation_required_for_template_class_members(type);
      }  /* if */
      type = type->next;
    }  /* while */
  }  /* if */
  db_exit();
}  /* set_instantiation_required_for_template_class_members */


static a_template_arg_ptr templ_arg_list_for_class(a_type_ptr class_type)
/*
Given a class type, return the template argument list to be used when
generating an instantiation.  This it the template argument list
associated with the nearest enclosing class that has a template
argument list.  Classes without template argument lists are member classes
but not member class templates.
*/
{
  while (class_type->source_corresp.is_class_member &&
         class_type->variant.class_struct_union.extra_info->
                                                  template_arg_list == NULL) {
    class_type = class_type->source_corresp.parent.class_type;
  }  /* while */
  return class_type->variant.class_struct_union.extra_info->template_arg_list;
}  /* templ_arg_list_for_class */


static void update_befriending_classes_for_class
                           (a_template_symbol_supplement_ptr tssp,
			    a_type_ptr                       class_type)
/*
Loop through the list of classes that have declared this template
class a friend and update the friend information.
*/
{
  a_class_list_entry_ptr   clep;
  a_symbol_ptr		   class_sym;

  class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  check_assertion_str2(class_sym != NULL,
                       "update_befriending_classes_for_class:",
                       "NULL assoc_info");
  if (is_real_class_symbol(class_sym)) {
    /* Only update the friend information when the class being made a friend
       is a real class type. */
    for (clep = tssp->befriending_classes; clep != NULL; clep = clep->next) {
      if (clep->class_type != class_type) {
         /* Don't declare the current class as a friend. */
        decl_friend_class(clep->class_type, class_type);
      }  /* if */
    }  /* for */
    if (tssp->prototype_template != NULL) {
      /* This class is an instance of a member template declared in a
         class template.  The template can be made a friend as
         a member of the class template:
           template <class T> template <class T2> friend class A<T>::B
         as a member of an instance:
           template <> template <class T2> friend class A<int>::B
         or a combination of the two.  The code above will handle declarations
         that make a member of an instance a friend.  We call this routine
         recursively to pick up any friend declarations that made the class
         template member a friend. */
      a_symbol_ptr			prototype_sym;
      a_template_symbol_supplement_ptr	prototype_tssp;
      prototype_sym = tssp->prototype_template;
      prototype_tssp = template_supplement_for_symbol(prototype_sym);
      update_befriending_classes_for_class(prototype_tssp, class_type);
    }  /* if */
  }  /* if */
}  /* update_befriending_classes_for_class */


a_template_cache_ptr cache_for_template(a_template_symbol_supplement_ptr tssp)
/*
Returns a pointer to the body cache to be used for a given template.
Typically, this is the body cache stored in the template symbols supplement.
But if the template is a member template declared within a class template,
the body may be associated with the member template from the prototype
instantiation.
*/
{
  a_template_cache_ptr	tcp;

  if (tssp->prototype_template != NULL && !tssp->is_specific_definition) {
    /* Use the cache from the original template. */
    tcp = &tssp->prototype_template->variant.template_info->cache;
  } else {
    tcp = &tssp->cache;
  }  /* if */
  return tcp;
}  /* cache_for_template */


static
a_func_info_block *func_info_for_template(
                                      a_template_symbol_supplement_ptr tssp)
/*
Returns a pointer to the func_info to be used for a given template.
Typically, this is the one stored in the template symbols supplement.
But if the template is a member template declared within a class template,
the func_info may be associated with the member template from the prototype
instantiation.
*/
{
  a_func_info_block	*fibp;

  if (tssp->prototype_template != NULL && !tssp->is_specific_definition) {
    /* Use the cache from the original template. */
    fibp = &tssp->prototype_template->
                           variant.template_info->variant.function.func_info;
  } else {
    fibp = &tssp->variant.function.func_info;
  }  /* if */
  return fibp;
}  /* func_info_for_template */


static
void find_class_template_member(a_symbol_ptr  ct_symbol,
                                a_type_ptr    parent_class)
/*
ct_symbol is a symbol representing a member class template of a real
instantiation of a class template.  Find the sk_class_template symbol
from the prototype instantiation (it serves as the template for the
real member class template), and record it in the template symbol
supplement already associated with ct_symbol.
*/
{
  a_scope_number                    corresp_prototype_decl_scope;
  a_type_ptr                        tp;
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_symbol_supplement_ptr  orig_tssp;
  a_symbol_ptr			    parent_class_sym;
  a_symbol_ptr			    corresp_prototype_tag_sym;
  a_symbol_list_entry_ptr	    slep;

  db_enter(3, "find_class_template_member");
  /* Get the prototype instantiation symbol that corresponds to the parent
     class of this member template. */
  parent_class_sym = (a_symbol_ptr)parent_class->source_corresp.assoc_info;
  check_assertion_str2(parent_class_sym != NULL,
                       "find_class_template_member:",
                       "parent_class_sym is NULL");
  corresp_prototype_tag_sym =
                         corresp_prototype_for_class_symbol(parent_class_sym);
  if (corresp_prototype_tag_sym != NULL) {
    tp = type_symbol_type(corresp_prototype_tag_sym);
    /* Get the scope in which the members of the class represented by
       corresp_prototype_tag_sym were declared. */
    corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
    for (sym = ct_symbol->header->inactive_symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->decl_scope == corresp_prototype_decl_scope &&
          sym->kind == (a_symbol_kind)sk_class_template) {
        break;
      }  /* if */
    }  /* for */
    check_assertion_str2(sym != NULL || total_errors != 0,
                         "find_class_template_member:",
                         "no corresponding template");
    /* The symbol can be NULL in some error cases. */
    if (sym != NULL) {
      /* sym is the template symbol with which ct_symbol is associated.
         Update the template supplement of ct_symbol to point to the
         cache information from the original template.  The NULL template
         declaration information pointer that is passed in causes the template
         to retain its existing template declaration information. */
      tssp = ct_symbol->variant.template_info;
      orig_tssp = sym->variant.template_info;
      /* Create the pointer back to the original template. */
      tssp->prototype_template = sym;
      tssp->variant.class_template.prototype_instantiation_complete = TRUE;
      /* Add the new template to the list of templates based on the original
         template. */
      slep = alloc_symbol_list_entry();
      slep->symbol = ct_symbol;
      slep->next = orig_tssp->subordinate_templates;
      orig_tssp->subordinate_templates = slep;
    }  /* if */
  }  /* if */
  db_exit();
}  /* find_class_template_member */


void f_instantiate_template_class(a_type_ptr  class_type)
/*
class_type is an incomplete class type.  If it is an instance of a class
template, perform a full instantiation of it.  This entails rescanning the
tokens that were cached when the template definition was originally
encountered; the cached tokens include the base specifiers list, if any,
and the class body (from opening left brace through closing right brace).
The template arguments (the real values which the template parameters take
on) have been recorded in class_type and will be substituted for the
template parameters when the instantiation scope is pushed.

This routine should be called from macro instantiatiate_template_class,
which determines that class_type is an incomplete type.  If it also turns
out to be a template type, this routine attempts to instantiate it; it
might not be able to if the template itself has not yet been defined.
*/
{
  a_symbol_ptr                      instance_sym;
  a_symbol_ptr			    template_sym;
  a_symbol_ptr			    template_sym_of_prototype;
  a_template_symbol_supplement_ptr  tssp;
  a_template_symbol_supplement_ptr  tssp_of_prototype;
  a_class_symbol_supplement_ptr     cssp;
  a_template_arg_ptr                template_arg_list;
  an_extern_linkage                 saved_linkage;
  a_boolean			    is_class_member;

  db_enter(3, "f_instantiate_template_class");
#if CHECKING
  if (!is_class_struct_union_type(class_type)) {
    internal_error("f_instantiate_template_class: not a class");
  }  /* if */
#endif /* CHECKING */
  class_type = skip_typerefs(class_type);
  is_class_member = class_type->source_corresp.is_class_member;
  instance_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  cssp = instance_sym->variant.class_struct_union.extra_info;
  /* Record the namespace that is the "referencing context" namespace for
     this instantiation. */
  cssp->referencing_namespace =
                 scope_stack[depth_innermost_namespace_scope].assoc_namespace;
  template_sym = template_symbol_for_class_symbol(instance_sym);
  if (template_sym == NULL) {
    /* Not a class based on a class template. */
  } else if (cssp->is_nonreal_class) {
    /* Don't try to instantiate a template class without real template
       arguments. */
  } else if (class_type->variant.class_struct_union.is_specialized) {
    /* This is an attempt to instantiate an incomplete type that is
       a specific definition.  This can occur in error cases while scanning
       the class definition.  Simply ignore the instantiation request. */
  } else {
    a_template_cache_ptr	body_cache;
    tssp = template_supplement_for_symbol(template_sym);
    /* If this is a class template defined within another class template,
       the prototype instantiation is associated with the definition
       within the original template.  Get a pointer to the template
       symbol that is associated with the prototype instantiation. */
    if (tssp->prototype_template != NULL && !tssp->is_specific_definition) {
      template_sym_of_prototype = tssp->prototype_template;
    } else {
      template_sym_of_prototype = template_sym;
    }  /* if */
    tssp_of_prototype =
                     template_supplement_for_symbol(template_sym_of_prototype);
    body_cache = cache_for_template(tssp);
    /* There is a class template from which to generate this class and it is
       a real instantiation. */
    /* Update the class symbol supplement pointer that points to the
       prototype instantiation.  Instances of a class template can sometimes
       be created before this is known. */
    cssp->corresp_prototype_sym =
             tssp_of_prototype->variant.class_template.prototype_instantiation;
    if (body_cache->tokens.first_token == NULL) {
      /* The template itself has not yet been defined.  The caller will
         issue an incomplete-type error. */
    } else if (!tssp_of_prototype->variant.class_template.
					prototype_instantiation_complete) {
      /* A real instantiation is being requested while the prototype
         instantiation is still being processed.  We simply ignore the
         instantiation request which will typically result in an incomplete
         type not allowed error to be issued by the caller. */
    } else if (cssp->instantiation_in_progress) {
      /* This particular template class (not just some other one based on
         the same template) is currently being instantiated. */
    } else if (tssp->pending_instantiations >= MAX_PENDING_INSTANTIATIONS) {
      /* This class instantiation occurs within the context of other
         instantiations of the same class template.  When the number of
         such instantiations-in-progress exceeds a configuration
         constant value, we assume this to be runaway recursion -- for
         instance (to give a rather unlikely example):
            template <class T, int I> class X {
              X<T,I+1> x;
            };
      */                
      sym_error(ec_runaway_recursive_instantiation, instance_sym);
      /* Set the flag that indicates that this instance s being specialized.
         This will suppress subsequent attempts to instantiate this class. */
      class_type->variant.class_struct_union.is_specialized = TRUE;
    } else {
      /* We proceed with the instantiation. */
      /* Increment the count of instantiations-in-progress for the current
         class template.  It will be decremented when the instantiation is
         complete. */
      ++(tssp->pending_instantiations);
      cssp->instantiation_in_progress = TRUE;
#if DEBUG
      if (debug_level >= 3 || db_flag_is_set("instantiations")) {
        fprintf(f_debug, "Beginning full instantiation of: ");
        db_type(class_type);
        db_symbol(template_sym, "\nbased on: ", 2);
      }  /* if */
#endif /* DEBUG */
      /* Find the outermost enclosing class type.  The template arguments
         are associated with this class type. */
      template_arg_list = templ_arg_list_for_class(class_type);
      /* Push a template instantiation scope.  The real values of the
         the template arguments will be associated with the template
         parameter names. */
      (void)push_template_instantiation_scope(body_cache->decl_info,
					      class_type,
					      (a_routine_ptr)NULL,
					      instance_sym, template_sym,
					      template_arg_list);
      /* Reactivate any pragmas that should be bound to the generated
         instance. */
      reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
      /* Set the default name linkage to that of the template.  It will be
         active while the function body is scanned and then restored. */
      saved_linkage = def_external_linkage;
      def_external_linkage.kind = class_type->source_corresp.name_linkage;
      def_external_linkage.is_explicit = FALSE;
      /* The tokens of the template definition have been cached away.
         Activate the cache so that they can be rescanned in light of
         the new values associated with the template parameters. */
      rescan_reusable_cache(&body_cache->tokens);
#if CHECKING
      if (curr_token != tok_lbrace && curr_token != tok_colon) {
        internal_error("f_instantiate_template_class: bad 1st token in cache");
      }  /* if */
#endif /* CHECKING */
      mark_defined(instance_sym, &instance_sym->decl_position);
      /* Scan the base specifiers list, if any, and the body of the class.
         The pending class definition counter is incremented while processing
         the instantiation.  This ensures that the fixup of the instantiation
         will not be done until the instantiation scope has been popped. */
      pending_class_definitions++;
      /* Scan the base specifiers list, if any, and the body of the class. */
      (void)scan_class_definition
                   (class_type, depth_innermost_namespace_scope,
                    depth_innermost_namespace_scope, /*is_local_class=*/FALSE,
                    /*delayed_nested_class_def=*/is_class_member,
                    /*is_template_instantiation=*/TRUE);
      pending_class_definitions--;
      set_instantiation_required_for_template_class_members(class_type);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* A template instantiation is considered to always be "autonomous",
         even if its instantiation happens to be triggered by a reference
         in the declaration of another entity. */
      set_autonomous_tag_decl_flag(class_type, /*is_definition=*/TRUE);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Restore the default name linkage. */
      def_external_linkage = saved_linkage;
      /* Process any pragmas that are to be bound to this instance. */
      process_curr_construct_pragmas(instance_sym, (a_statement_ptr)NULL);
      pop_template_instantiation_scope();
      /* In the normal case the current token should be end_of_source,
         which was inserted to mark the end of the cached token stream.
         If necessary, keep flushing until end-of-source is found. */
      flush_past_token_cache_terminator();
      /* Do the class fixups for this instantiation. */
      process_deferred_class_fixups_and_instantiations();
      /* Decrement the count of instantiations-in-progress for the current
         class template. */
      cssp->instantiation_in_progress = FALSE;
      --(tssp->pending_instantiations);
    }  /* if */
  }  /* if */
  db_exit();
}  /* f_instantiate_template_class */


void check_for_uninstantiated_template_class(a_type_ptr  tp)
/*
tp is an incomplete type.  If it is a class in need of instantiation or an
array whose underlying element type is such a class, instantiate it.
Otherwise, do nothing.
*/
{
  if (is_array_type(tp)) {
    tp = underlying_array_element_type(tp);
    if (tp == NULL || !is_incomplete_type(tp)) goto done;
  }  /* if */
  if (is_class_struct_union_type(tp)) f_instantiate_template_class(tp);
done:;
}  /* check_for_uninstantiated_template_class */


#if DEBUG
static void db_template_cache_segments(a_template_cache_segment_ptr tcsp)
/*
Display the contents of list of template cache segment entries.
*/
{
  int	count;
  for (count = 0; tcsp != NULL; tcsp = tcsp->next, count++) {
    fprintf(f_debug, "Entry %0d\n", count);
    fprintf(f_debug, "Symbol: ");
    db_symbol(tcsp->symbol, "", 6);
    fprintf(f_debug, "  first_token_number: %0lu\n", tcsp->first_token_number);
    fprintf(f_debug, "  last_token_number: %0lu\n", tcsp->last_token_number);
    fprintf(f_debug, "  before_first_token: %p\n",
            (void*)tcsp->before_first_token);
    fprintf(f_debug, "  last_token: %p\n", (void*)tcsp->last_token);
    fprintf(f_debug, "\n");
  }  /* for */
}  /* db_template_cache_segments */
#endif /* DEBUG */


static
a_template_cache_segment_ptr map_token_numbers_to_cache_pointers(
			a_template_symbol_supplement_ptr	tssp,
			a_template_cache_segment_ptr		cache_segments)
/*
The cache segment entries currently contain the starting and ending token
sequence numbers of the cache segments.  Make a pass through the enclosing
cache and find the token before the first token and the last token.
These token pointers are needed to relink the original cache around
the removed tokens.

The result is a list of cache segments in a different order than the
list passed in.  The pointer to the start of the list is returned.
*/
{
  a_cached_token_ptr		ctp;
  a_cached_token_ptr		prev_ctp = NULL;
  a_template_cache_segment_ptr	start_found_list = NULL;
  a_template_cache_segment_ptr	complete_list = NULL;
  a_template_cache_segment_ptr	curr_tcsp = cache_segments;

#if CHECKING
  /* The entries on the list of cache segments must be in order of
     first token sequence number.  Make sure that this is the case. */
  {
    a_template_cache_segment_ptr	tcsp;
    a_token_sequence_number		prev_tsn = NO_TOKEN_SEQUENCE_NUMBER;
    for (tcsp = cache_segments; tcsp != NULL; tcsp = tcsp->next) {
      check_assertion(tcsp->first_token_number > prev_tsn);
      prev_tsn = tcsp->first_token_number;
    }  /* for */
  }
#endif /* CHECKING */

  for (ctp = tssp->cache.tokens.first_token;
       ctp != NULL; prev_ctp = ctp, ctp = ctp->next) {
    /* Stop searching if there are no more entries to be processed. */
    if (curr_tcsp == NULL && start_found_list == NULL) break;
    if (curr_tcsp != NULL &&
        ctp->token_sequence_number == curr_tcsp->first_token_number) {
      /* We've found the first token of the current segment.  Move it to
         the list of entries for which the start has been found.  It is
         inserted into the list so that the list is sorted by ending
         token number. */
      a_template_cache_segment_ptr	tcsp = start_found_list;
      a_template_cache_segment_ptr	prev_tcsp = NULL;
      a_template_cache_segment_ptr	next_tcsp = curr_tcsp->next;
      curr_tcsp->next = NULL;
      for (; tcsp != NULL &&
           tcsp->last_token_number < curr_tcsp->last_token_number;
           prev_tcsp = tcsp, tcsp = tcsp->next) {}
      if (prev_tcsp == NULL) {
        /* Add the entry to the start of the list. */
        curr_tcsp->next = start_found_list;
        start_found_list = curr_tcsp;
      } else {
        /* Add the entry after the one pointed to by prev_tcsp. */
        curr_tcsp->next = prev_tcsp->next;
        prev_tcsp->next = curr_tcsp;
      }  /* if */
      /* Update the entry with a pointer to the token that immediately
         precedes the first token of the segment. */
      curr_tcsp->before_first_token = prev_ctp;
      curr_tcsp = next_tcsp;
#if DEBUG
      if (db_flag_is_set("map_tokens")) {
        fprintf(f_debug, "start_found_list after addition:\n");
        db_template_cache_segments(start_found_list);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    if (start_found_list != NULL &&
        (ctp->token_sequence_number == start_found_list->last_token_number ||
         start_found_list->last_token_number == NO_TOKEN_SEQUENCE_NUMBER)) {
      /* We've found the last token of the first entry on the "start found"
         list.  Move the entry to the completed list.  The test for
         NO_TOKEN_SEQUENCE_NUMBER is present for error cases in which
         the last token of the body was not found. */
      a_template_cache_segment_ptr	tcsp = start_found_list;
      start_found_list = tcsp->next;
      tcsp->next = complete_list;
      complete_list = tcsp;
      tcsp->last_token = ctp;
#if DEBUG
      if (db_flag_is_set("map_tokens")) {
        fprintf(f_debug, "complete_list after addition:\n");
        db_template_cache_segments(complete_list);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* for */
  check_assertion(curr_tcsp == NULL && start_found_list == NULL);
  return complete_list;
}  /* map_token_numbers_to_cache_pointers */


static void replace_body_with_semicolon(a_template_cache_segment_ptr tcsp)
/*
*/
{
  a_boolean		insert_semicolon = FALSE;
  a_cached_token_ptr	before_first_token = tcsp->before_first_token;
  a_cached_token_ptr	first_token = before_first_token->next;
  a_cached_token_ptr	last_token = tcsp->last_token;
  a_cached_token_ptr	ctp;

  /* See if the last token if the cache is followed by an optional
     semicolon.  Only insert one if there is not already one there. */
  for (ctp = tcsp->last_token->next; ctp != NULL; ctp = ctp->next) {
    /* Ignore pragma tokens. */
    if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
      continue;
    }  /* if */
    insert_semicolon = ctp->token != (a_byte_token_kind)tok_semicolon;
    break;
  }  /* for */
  if (insert_semicolon) {
    /* Make a new cached token entry for a semicolon, the function body
       will be replaced with the semicolon.  Give it the same token
       sequence number as the first token of the body. */
    a_cached_token_ptr	replacement_token;
    replacement_token = build_cached_token(tok_semicolon,
                                           tcsp->first_token_number,
                                           &first_token->source_position);
    /* Link the replacement token into the cache in the place of
       the body. */
    replacement_token->next = last_token->next;
    before_first_token->next = replacement_token;
  } else {
    /* No semicolon is needed.  Link the tokens to remove the member
       body.  Update the token sequence number of the token that now
       follows the function declarator to have the token sequence number
       of the opening brace of the function.  This is needed for matching
       a function declaration in an actual instantiation with the
       corresponding declaration in the prototype instantiation. */
    before_first_token->next = last_token->next;
    last_token->next->token_sequence_number =
                                       first_token->token_sequence_number;
  }  /* if */
  /* Unlink the rest of the cache from the last token of the body. */
  last_token->next = NULL;
}  /* replace_body_with_semicolon */


static
a_template_cache_segment_ptr extract_member_bodies(
			   a_template_symbol_supplement_ptr tssp,
                           a_template_cache_segment_ptr	    cache_segments,
                           a_boolean                        functions_only)
/*
Go through the member functions and nested classes of the class template
associated with tssp, and remove the tokens from the token cache.  If
functions_only is TRUE, only the function bodies are extracted.  Nested
classes remain on the list.  The pointer to the updated list is
returned to the caller.
*/
{
  a_template_cache_segment_ptr		tcsp;
  a_template_cache_segment_ptr		next_tcsp;
  a_template_cache_segment_ptr		new_list = NULL;

  db_enter(4, "extract_member_bodies");
  if (cache_segments != NULL && cache_segments->before_first_token == NULL) {
    /* Get the pointers to the tokens that need to be relinked to remove
       the member bodies.  This routine may be called more than once, but
       this operation must only be done the first time it is called because
       the list is only in the right order for the mapping to be done at
       the time of the first call. */
    cache_segments = map_token_numbers_to_cache_pointers(tssp, cache_segments);
  }  /* if */
  for (tcsp = cache_segments; tcsp != NULL; tcsp = next_tcsp) {
    next_tcsp = tcsp->next;
    /* A missing last_token_number indicates that an error occurred
       while scanning the class definition and no ending token was found.
       Don't attempt to remove the body from the template. */ 
    if (tcsp->last_token_number == NO_TOKEN_SEQUENCE_NUMBER) continue;
    switch (tcsp->symbol->kind) {
      case sk_member_function:
      case sk_class_template:
      case sk_function_template:
        /* A separate copy of the token cache is already maintained for
           member functions and member templates.  Just free the
           tokens that were removed from
           the original cache. */
        { a_token_cache_ptr	class_cache = &tssp->cache.tokens;
          a_cached_token_ptr	first_token = tcsp->before_first_token->next;
          a_cached_token_ptr	ctp;
          replace_body_with_semicolon(tcsp);
          ctp = first_token;
            while (ctp != NULL) {
              a_cached_token_ptr	next_ctp = ctp->next;
              free_cached_token_from_reusable_cache(class_cache, ctp);
              ctp = next_ctp;
          }  /* while */
        }
        break;
      case sk_class_or_struct_tag:
      case sk_union_tag:
        if (functions_only) {
          /* Add this entry to a new list of entries that still need to
             be processed. */
          if (new_list != NULL) new_list->next = tcsp;
          tcsp->next = new_list;
          continue;
        }  /* if */
        tssp = tcsp->template_info;
        /* Only extract the body of the nested class if it is a
           "standalone" nested class (i.e., one that is not anonymous
	   and is not followed by a declarator). */
        if (!tcsp->template_info->
                         variant.class_template.not_standalone_nested_class) {
          a_cached_token_ptr	first_token = tcsp->before_first_token->next;
          replace_body_with_semicolon(tcsp);
          /* Remove the tokens for the nested class from the original
             cache to the cache for the nested class.  The tokens have
             actually already been unliked from the first cache, but
             information such as token counts must be adjusted. */
          move_cached_tokens(first_token, &tssp->cache.tokens,
                             &tcsp->template_info->cache.tokens);
        }  /* if */
        break;
      default:
        unexpected_condition();
    }  /* switch */
    /* Free the template cache segment for the member just removed.  Note
       that when only processing functions, this code is bypassed for entries
       that are put on the new list. */
    free_template_cache_segment(tcsp);
  }  /* for */
  db_exit();
  return new_list;
}  /* extract_member_bodies */


static
void instantiate_class_template(a_symbol_ptr                 template_sym,
                                a_type_ptr                   prototype_type,
                                a_template_cache_segment_ptr *tcsp)
/*
This routine is called to do a "prototype instantiation" of a class template,
namely, to scan the template definition even though the template parameters
have not yet been given "real" values; because declaration/expression
disambiguation often cannot be done based on dummy types, only declarative
information is scanned; inline function bodies and default argument
expressions are ignored.  A side effect of this scan is to detect gross
syntax errors.  The main benefit is to record the names and types of member
functions and static data members, for which template definitions may be
encountered.

While the prototype instantiation is in progress, a list of the cache segments
for the member functions and classes defined within the class is maintained.
A pointer to the head of the list is returned in tcsp.
*/
{
  a_template_symbol_supplement_ptr  tssp;
  a_symbol_ptr                      instance_sym;
  a_template_arg_ptr                template_arg_list;
  a_class_symbol_supplement_ptr     cssp;
  a_boolean			    is_class_member;

  db_enter(3, "instantiate_class_template");
  tssp = template_supplement_for_symbol(template_sym);
  instance_sym = (a_symbol_ptr)prototype_type->source_corresp.assoc_info;
  cssp = instance_sym->variant.class_struct_union.extra_info;
  is_class_member = prototype_type->source_corresp.is_class_member;
#if CHECKING
  if (tssp->cache.tokens.first_token == NULL) {
    /* The template itself has not yet been defined. */
    internal_error("instantiate_class_template: bad cache");
  } else if (cssp->instantiation_in_progress) {
    /* The template is currently being instantiated. */
    internal_error("instantiate_class_template: already being instantiated");
  };
#endif /* CHECKING */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("instantiations")) {
    db_symbol(template_sym, "prototype instantiation of: ", 2);
  }  /* if */
#endif /* DEBUG */
  instance_sym->variant.class_struct_union.extra_info->
                                          is_prototype_instantiation = TRUE;
  /* Save a pointer to the prototype instantiation. */
  tssp->variant.class_template.prototype_instantiation = instance_sym;
  template_arg_list = templ_arg_list_for_class(prototype_type);
  cssp->instantiation_in_progress = TRUE;
  /* Record the namespace that is the "referencing context" namespace for
     this instantiation.  For the prototype instantiation this is the
     same as the namespace in which the template was defined. */
  cssp->referencing_namespace =
                 scope_stack[depth_innermost_namespace_scope].assoc_namespace;
  cssp->template_info = tssp;
  (void)push_template_instantiation_scope(tssp->cache.decl_info,
					  prototype_type,
					  (a_routine_ptr)NULL, instance_sym,
					  template_sym, template_arg_list);
  /* Reactivate any pragmas that should be bound to the generated
     instance. */
  reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
  rescan_reusable_cache(&tssp->cache.tokens);
#if CHECKING
  if (curr_token != tok_lbrace && curr_token != tok_colon) {
    internal_error("instantiate_class_template: bad 1st token in cache");
  }  /* if */
#endif /* CHECKING */
  /* Scan the base specifiers list, if any, and the body of the class.
     The pending class definition counter is incremented while processing
     the instantiation.  This ensures that the fixup of the instantiation
     will not be done until the instantiation scope has been popped. */
  pending_class_definitions++;
  (void)scan_class_definition(prototype_type, depth_innermost_namespace_scope,
                              depth_innermost_namespace_scope,
                              /*is_local_class=*/FALSE,
                              /*delayed_nested_class_def=*/is_class_member,
                              /*is_template_instantiation=*/TRUE);
  pending_class_definitions--;
  /* Process any pragmas that are to be bound to this instance. */
  process_curr_construct_pragmas(instance_sym, (a_statement_ptr)NULL);
  /* Return the pointer to the list of template cache segments associated
     with this prototype instantiation. */
  *tcsp = scope_stack[depth_innermost_instantiation_scope].
                                                  first_template_cache_segment;
  pop_template_instantiation_scope();
  /* Do the class fixups for this instantiation. */
  process_deferred_class_fixups_and_instantiations();
  cssp->instantiation_in_progress = FALSE;
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token stream.
     If necessary, keep flushing until end-of-source is found. */
  flush_past_token_cache_terminator();
  /* Set the flag that indicates that the prototype instantiation has
     been completed. */
  tssp->variant.class_template.prototype_instantiation_complete = TRUE;
  db_exit();
}  /* instantiate_class_template */


static void check_for_definition_in_friend_declaration(
                            a_template_symbol_supplement_ptr tssp,
			    a_routine_ptr                    rout_ptr)
/*
Functions defined in friend declarations are treated specially in the
IL.  The class in which the function is defined appears at the front
of the befriending classes list, and the defined_in_friend_decl flag
is set in the routine entry.  For template functions, we can't tell
until the function is instantiated whether it is defined in a friend
declaration (because there could be a specialization that we have not
yet seen).  This routine is called when the function is instantiated,
when we know whether or not the template was defined in a friend
declaration.
*/
{
  a_scope_ptr		   definition_scope;
  a_type_ptr		   definition_class = NULL;

#if 0
  /* This will need to be updated for member templates.  Member templates
     cannot be defined in friend declarations. */
#endif /* 0 */
  definition_scope = cache_for_template(tssp)->decl_info->enclosing_scope;
  if (definition_scope->kind == (a_scope_kind)sck_class_struct_union) {
    definition_class = definition_scope->variant.assoc_type;
  }  /* if */
  if (definition_class != NULL) {
    update_friend_function_info(rout_ptr, definition_class,
                                /*is_definition=*/TRUE,
                                /*move_to_front=*/TRUE);
    rout_ptr->defined_in_friend_decl = TRUE;
  }  /* if */
}  /* check_for_definition_in_friend_declaration */


static
void find_function_template_member(a_symbol_ptr  ft_symbol,
                                   a_type_ptr    parent_class)
/*
ft_symbol is a symbol representing a member class template of a real
instantiation of a class template.  Find the sk_class_template symbol
from the prototype instantiation (it serves as the template for the
real member class template), and record it in the template symbol
supplement already associated with ft_symbol.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_symbol_supplement_ptr  orig_tssp;
  a_symbol_ptr			    parent_class_sym;
  a_symbol_ptr			    corresp_prototype_tag_sym;
  a_symbol_list_entry_ptr	    slep;


  db_enter(3, "find_function_template_member");
  /* Get the prototype instantiation symbol that corresponds to the parent
     class of this member template. */
  parent_class_sym = (a_symbol_ptr)parent_class->source_corresp.assoc_info;
  check_assertion_str2(parent_class_sym != NULL,
                       "find_function_template_member:",
                       "parent_class_sym is NULL");
  corresp_prototype_tag_sym =
                         corresp_prototype_for_class_symbol(parent_class_sym);
  if (corresp_prototype_tag_sym != NULL) {
    /* In certain error cases, two declarations that are distinct in the
       class template may end up referring to the same function in a
       given instantiation.  For example, the functions
         template <class T2> void f(T2, T);
         template <class T2> void f(T2, int);
       will result in a duplicate declaration of f(T2, int) when T is int.
       An error will be diagnosed when this is encountered in the class body.
       If a pointer to the prototype template already exists, simply skip
       this processing. */
    check_assertion(ft_symbol->kind == (a_symbol_kind)sk_function_template);
    tssp = ft_symbol->variant.template_info;
    if (tssp->prototype_template != NULL) goto error_exit;
    /* Find a function symbol on the inactive list that is in the scope of the
       prototype instantiation.  It should either be a function template or
       overloaded function symbol. */
    if (is_constructor_symbol(ft_symbol)) {
      sym = corresp_prototype_tag_sym->
                           variant.class_struct_union.extra_info->constructor;
    } else if (ft_symbol->variant.routine.ptr->special_kind ==
                                    (a_special_function_kind)sfk_conversion) {
#if 0
    /* Look through the conversion routines of the prototype instantiation.
       The token sequence number associated for the current token is saved
       during the prototype instantiation.  This is used to match this
       declaration with the symbol generated by the prototype instantiation. */
    sym = NULL;
    for (slep = corresp_prototype_tag_sym->
                      variant.class_struct_union.extra_info->conversion_list;
         slep != NULL;
         slep = slep->next) {
      a_template_symbol_supplement_ptr	tssp;
      tssp = slep->symbol->variant.routine.instance_ptr->template_info;
      if (tssp->token_sequence_number == curr_token_sequence_number) {
        /* slep->symbol is the template function symbol for ft_symbol. */
        sym = slep->symbol;
        break;
      }  /* if */
    }  /* for */
    if (sym == NULL) {
      /* If the conversion operator is for a derived to base conversion,
	 the conversion operator will never be called, and so is not on the
	 conversions list.  This will result in a match not being found in
	 the loop above.  Go through the symbol list associated with the
	 prototype instantiation to find the matching symbol.  This will
	 only occur for unusable derived to base conversions (for which a
	 warning is also issued) so the cost of the extra test should not be
	 significant. */
      for (sym = corresp_prototype_tag_sym->
                               variant.class_struct_union.extra_info->symbols;
           sym != NULL;
	   sym = sym->next_in_scope) {
	if (sym->kind == (a_symbol_kind)sk_member_function) {
	  a_template_symbol_supplement_ptr	tssp;
	  tssp = sym->variant.routine.instance_ptr->template_info;
	  if (tssp->token_sequence_number == curr_token_sequence_number) {
	    break;
	  }  /* if */
	}  /* if */
      }  /* for */
    }  /* if */
#else
      unexpected_condition();
#endif
    } else {
      a_type_ptr                    tp;
      a_scope_number                corresp_prototype_decl_scope;

      /* Get the scope in which the members of the class represented by
         corresp_prototype_tag_sym were declared. */
      tp = type_symbol_type(corresp_prototype_tag_sym);
      corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
      for (sym = ft_symbol->header->inactive_symbols;
           sym != NULL;
           sym = sym->next) {
        if (sym->decl_scope == corresp_prototype_decl_scope) {
          if (sym->kind == (a_symbol_kind)sk_function_template ||
              sym->kind == (a_symbol_kind)sk_overloaded_function) {
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (sym != NULL && sym->kind == (a_symbol_kind)sk_overloaded_function) {
      /* An overloaded function was found.  Go through the symbols on its list
         and find the function template symbol that corresponds to ft_symbol.
         The token sequence number associated for the current token is saved
         during the prototype instantiation.  This is used to match this
         declaration with the symbol generated by the prototype
         instantiation. */
      for (sym = sym->variant.overloaded_function.symbols;
           sym != NULL;
           sym = sym->next) {
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          a_template_symbol_supplement_ptr	other_tssp;
          other_tssp = sym->variant.template_info;
          if (other_tssp->token_sequence_number ==
                                                  curr_token_sequence_number) {
            /* sym is the template function symbol for ft_symbol. */
            break;
         }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    check_assertion_str2(!((sym == NULL ||
                            sym->kind != (a_symbol_kind)sk_function_template)
                           && total_errors == 0),
                         "find_function_template_member:",
                         "no corresponding template");
    if (sym == NULL) {
      /* An error must have occurred previously.  Don't create the template
         instance information in this case. */
      goto error_exit;
    }  /* if */
    /* Create the pointer back to the original template. */
    tssp->prototype_template = sym;
    /* Add the new template to the list of templates based on the original
       template. */
    orig_tssp = sym->variant.template_info;
    slep = alloc_symbol_list_entry();
    slep->symbol = ft_symbol;
    slep->next = orig_tssp->subordinate_templates;
    orig_tssp->subordinate_templates = slep;
    { a_routine_ptr	rp = tssp->variant.function.routine;
      a_routine_ptr	orig_rp = orig_tssp->variant.function.routine;;
      /* Copy the information that determines whether this function is inline
         from the prototype template.  This cannot be determined accurately
         for the subordinate template because the body will have already been
         removed. */
      tssp->variant.function.func_info.is_inline =
                               orig_tssp->variant.function.func_info.is_inline;
      rp->is_inline = orig_rp->is_inline;
      rp->storage_class = orig_rp->storage_class;
      rp->source_corresp.name_linkage = orig_rp->source_corresp.name_linkage;
    }
  }  /* if */
#if 0
  /* Is there any friend processing that needs to be done here? */
#endif
error_exit:
  db_exit();
}  /* find_function_template_member */


static void instantiate_template_function(a_template_instance_ptr  tip)
/*
Instantiate the body of the template function associated with tip.
*/
{
  a_symbol_ptr                      rout_sym;
  a_routine_ptr                     rout_ptr;
  a_template_symbol_supplement_ptr  tssp;
  an_extern_linkage                 saved_linkage;
  a_symbol_ptr			    template_sym;
  a_template_cache_ptr		    tcp;
  a_func_info_block		    *func_info_ptr;

  db_enter(3, "instantiate_template_function");
  rout_sym = tip->instance_sym;
  rout_ptr = rout_sym->variant.routine.ptr;
  if (rout_ptr->assoc_scope != NULL_region_number) {
    /* Already instantiated. */
    goto done;
  }  /* if */
  template_sym = tip->template_sym;
  tssp = template_supplement_for_symbol(template_sym);
  func_info_ptr = func_info_for_template(tssp);
  if (tssp->pending_instantiations >= MAX_PENDING_INSTANTIATIONS) {
    /* This function instantiation occurs within the context of other
       instantiations of the same function template.  When the number of
       such instantiations-in-progress exceeds a configuration
       constant value, we assume this to be runaway recursion -- for
       for instance (to give a rather unlikely example):

       template <int i> class A {
         void f() {
           A<i+1> a;
           a.f();
         }
       };
       void main() {
         A<1> a;
         a.f();
       }

       Note that this can only catch recursive instantiations of inline
       functions.  Runaway instantiations of out-of-line functions can
       not be detected this way because they are instantiated serially
       not recursively.
    */
    sym_error(ec_runaway_recursive_instantiation, rout_sym);
    goto done;
  }  /* if */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("instantiations")) {
    fprintf(f_debug, "instantiating: ");
    db_symbol(rout_sym, "", 0);
    db_symbol(template_sym, "\nbased on: ", 2);
  }  /* if */
#endif /* DEBUG */
  if (func_info_ptr->is_inline) {
    rout_ptr->is_inline = TRUE;
    rout_ptr->storage_class = (a_storage_class)sc_static;
    rout_ptr->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
  }  /* if */
  /* In case the source position in the routine instance is different from
     that of the defining template declaration, copy the latter to the
     instance symbol. */
  rout_sym->decl_position =
                tssp->variant.function.routine->source_corresp.decl_position;
  if (rout_ptr->type->kind == (a_type_kind)tk_typeref) {
    /* The function was declared using a typedef.  Now that it is being
       defined (given a body by the instantiation), create an unshared type
       with the typedef stripped off. */
    rout_ptr->type = copy_routine_type_with_param_types(rout_ptr->type);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  rout_ptr->declared_type = rout_ptr->type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Set the linkage and storage class. */
  if (instantiation_mode == tim_local) {
    /* Put out template function as internally linked. */
    rout_ptr->storage_class = (a_storage_class)sc_static;
    rout_ptr->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_internal;
  } else if (!rout_ptr->is_inline &&
             !(rout_ptr->storage_class == (a_storage_class)sc_static)) {
    /* Set the linkage for the definition of an externally linked routine. */
    rout_ptr->storage_class = (a_storage_class)sc_unspecified;
    rout_ptr->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_cplusplus_external;
  }  /* if */
  ++(tssp->pending_instantiations);
  /* Push the template instantiation scope. */
  tcp = cache_for_template(tssp);
  /* For member functions that are not member templates the argument
     list comes from the enclosing class that is reactivated by
     push_template_instantiation_scope and the value from the routine
     entry (which should be NULL) is not used. */
  (void)push_template_instantiation_scope(tcp->decl_info,
					  (a_type_ptr)NULL, rout_ptr,
					  rout_sym, template_sym,
					  rout_ptr->template_arg_list);
  if (rout_sym->defined) {
    /* Member functions of class templates where the definition appears
       inside the class definition will already have been marked as defined
       (when the class was instantiated).  If mark_defined is called for
       such cases, an incorrect source sequence entry can be generated. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    /* If a source sequence entry has not been put out for this member
       function (say, as part of putting out the class), do it now.  Note
       that this is only required when source sequence entries for the bodies
       of template functions are put out -- the source sequence entry for the
       function itself must be in the list to indicate when to make use of
       the source sequence list for the function body. */
    if (rout_ptr->source_corresp.source_sequence_entry == NULL) {
      update_source_sequence_list((char *)rout_ptr,
                                  (an_il_entry_kind)iek_routine,
                                  (a_source_sequence_entry_ptr)NULL);
    }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else {
    /* We wait till after the scope is pushed before calling mark_defined
       because the fact that a template instantiation scope is on the scope
       stack affects some decisions in that routine. */
    mark_defined(rout_sym, &rout_sym->decl_position);
  }  /* if */
  /* Reactivate any pragmas that should be bound to the generated
     instance. */
  reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
  /* If a lint-style "argsused" or "varargs" comment appeared, record that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments.  Note that this is done before calling
     process_curr_construct_pragmas; otherwise the pragmas we're interested
     in would have been disposed of. */
  record_lint_argsused_and_varargs_state(rout_sym);
  if (!exceptions_enabled && !func_info_ptr->is_inline &&
      func_info_ptr->throw_position.seq != 0) {
    /* Issue a diagnostic on attempting to define a noninline function with
       an exception specification when exception support is not enabled.
       (No diagnostic is issued on nondefinition -- the exception
       specification is just ignored.) */
    pos_error(ec_no_exception_support,
              &func_info_ptr->throw_position);
  }  /* if */
  /* Set the default name linkage to that of the template.  It will be
     active while the function body is scanned and then restored. */
  saved_linkage = def_external_linkage;
  def_external_linkage.kind = rout_ptr->source_corresp.name_linkage;
  def_external_linkage.is_explicit = FALSE;
  /* Reactivate the tokens comprising the function body and scan them. */
  rescan_reusable_cache(&tcp->tokens);
  scan_function_body(rout_ptr, func_info_ptr,
                     (SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                      SFB_IS_INSTANTIATION));
  /* scan_function_body does not scan past the right brace. */
  if (curr_token == tok_rbrace) (void)get_token();
  /* Restore the default name linkage. */
  def_external_linkage = saved_linkage;
  /* Process any pragmas that are to be bound to this instance. */
  process_curr_construct_pragmas(rout_sym, (a_statement_ptr)NULL);
  /* Pop the template instantiation scope. */
  pop_template_instantiation_scope();
  --(tssp->pending_instantiations);
  /* In the normal case the current token should be end_of_source, which was
     inserted to mark the end of the cached token stream. If necessary, keep
     flushing until end-of-source is found. */
  flush_past_token_cache_terminator();
  /* Usually template functions are instantiated "on demand" and the
     referenced flag will already have been set.  But if the
     instantiation mode says to instantiate whether or not there is
     a reference, we should set the referenced flag anyway, so that
     the back-end will be sure to generate the function. */ 
  tip->instance_sym->variant.routine.ptr->source_corresp.referenced = TRUE;
  if (tssp->befriending_classes != NULL) {
    /* If this template is a friend of one or more classes, check whether
       the template was defined in a friend declaration.  If so, update
       the friend information accordingly. */
    check_for_definition_in_friend_declaration(tssp, rout_ptr);
  }  /* if */
done:;
  /* The already instantiated flag is set even if certain error conditions
     (such as runaway instantiation) to prevent the compiler from attempting
     to instantiate this function again. */
  tip->already_instantiated = TRUE;
  db_exit();
}  /* instantiate_template_function */


static void define_template_static_data_member(a_template_instance_ptr  tip)
/*
Generate a definition of a static data member of a template class.  The
definition may be based on a template definition of the static data
member or may be a default initialization.  Checking for runaway
instantiation is not necessary for static data members because 
static data members are instantiated as result of class instantiations --
and the class instantiation will detect the runaway case.
*/
{
  a_symbol_ptr                      static_data_member_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_variable_ptr		    var_ptr;

  db_enter(3, "define_template_static_data_member");
  var_ptr = tip->instance_sym->variant.static_data_member.variable;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  var_ptr->declared_type = var_ptr->type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  tssp = tip->template_sym->
                 variant.static_data_member.instance_ptr->template_info;
  static_data_member_sym = tip->instance_sym;
#if CHECKING
  if (!tip->template_sym->defined ||
      tssp->cache.decl_info->parameters == NULL) {
    internal_error("define_template_static_data_member: undef'd template");
  } else if (static_data_member_sym->defined) {
    internal_error("define_template_static_data_member: sym already def'd");
  }  /* if */
#endif /* CHECKING */
  /* If the type of the static data member is a template class, make sure
     it is instantiated. */
  complete_type_is_needed(var_ptr->type);
  /* If the storage class is sc_extern, reset it to sc_unspecified (since the
     variable is being defined).  If it is sc_static (e.g., for a static
     data member of a class declared inside an unnamed namespace), leave it
     alone. */
  if (var_ptr->storage_class == (a_storage_class)sc_extern) {
    var_ptr->storage_class = (a_storage_class)sc_unspecified;
#if CHECKING
    check_assertion_str2(var_ptr->source_corresp.name_linkage ==
                                  (a_name_linkage_kind)nlk_cplusplus_external,
                         "define_template_static_data_member:",
                         "bad name linkage");
  } else {
    check_assertion_str2(var_ptr->source_corresp.name_linkage ==
                                  (a_name_linkage_kind)nlk_internal &&
                          var_ptr->storage_class == (a_storage_class)sc_static,
                         "define_template_static_data_member:",
                         "bad storage class or name linkage");
#endif /* CHECKING */
  }  /* if */
  /* Push a template instantiation scope.  The real values of the template
     arguments will be associated with the template parameter names. */
  /* For static data members, the argument list comes from the enclosing
     class that is reactivated by push_template_instantiation_scope. */
  (void)push_template_instantiation_scope(tssp->cache.decl_info,
                                          (a_type_ptr)NULL,
                                          (a_routine_ptr)NULL,
                                          static_data_member_sym,
                                          tip->template_sym,
                                          (a_template_arg_ptr)NULL);
  /* Reactivate any pragmas that should be bound to the generated
     instance. */
  reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
  /* Call mark_defined *after* the template instantiation scope is pushed --
     correct behavior for source sequence entry generation depends on it. */
  mark_defined(static_data_member_sym, &tip->template_sym->decl_position);
  if (tssp->cache.tokens.first_token != NULL) {
    /* An initializer was specified in the template declaration. */
    a_boolean  incomplete_type_error_reported;
    a_boolean  has_parenthesized_initializer;

    rescan_reusable_cache(&tssp->cache.tokens);
    /* If the first token is an equals sign then this is not a parenthesized
       initializer.   Initializers that begin with an invalid token will
       have already been discarded. */
    has_parenthesized_initializer = (curr_token != tok_assign);
    /* Bypass the "=" or "(". */
    (void)get_token();
    initializer(static_data_member_sym, &tip->template_sym->decl_position,
                idl_external, has_parenthesized_initializer,
                /*is_old_style_param_decl=*/FALSE,
                &incomplete_type_error_reported);
    if (curr_token != tok_end_of_source) {
      pos_error(ec_exp_semicolon, &pos_curr_token);
      while (curr_token != tok_end_of_source) (void)get_token();
    }  /* if */
    /* By pass end-of-source token, which is probably the terminator token
       in the cache. */
    (void)get_token();
  } else {
    /* There's no explicit initializer. */
    (void)def_initializer(static_data_member_sym,
                          &tip->template_sym->decl_position);
  }  /* if */
  /* Process any pragmas that are to be bound to this instance. */
  process_curr_construct_pragmas(static_data_member_sym,
                                 (a_statement_ptr)NULL);
  pop_template_instantiation_scope();
  /* Usually template static data members are instantiated "on demand" and
     so the referenced flag will already have been set.  But if the
     instantiation mode says to instantiate whether or not there is
     a reference, we should set the referenced flag anyway, so that
     the back-end will be sure to generate the function. */ 
  var_ptr->source_corresp.referenced = TRUE;
  var_ptr->is_template_static_data_member = TRUE;
  tip->already_instantiated = TRUE;
#if 0
  /* Note that Microsoft decl_modifiers are not processed on static
     data member definitions.  Microsoft does not allow this either. */
#endif /* 0 */
  db_exit();
}  /* define_template_static_data_member */


a_boolean equiv_template_arg_lists(a_template_arg_ptr list1,
                                   a_template_arg_ptr list2,
                                   a_boolean          error_matches_anything,
				   a_boolean	      is_nonreal_member)
/*
Return TRUE if the two linked lists of template arguments for a given template
class or template function are equivalent -- that is, if corresponding type
arguments refer to the same type and corresponding constant arguments refer to
the same constant.  If error_matches_anything is TRUE, an error type
or constant will match anything (this is used for compatibility checking
instead of equivalence checking).  is_nonreal_member indicates that the
template is a member of a nonreal class and has no template parameter
list.  In such cases, a NULL argument list, and argument lists of different
lengths are permitted. 
*/
{
  a_boolean           equiv;
  a_template_arg_ptr  arg1 = list1, arg2 = list2;

  db_enter(4, "equiv_template_arg_lists");
  /* There is no way to produce a NULL template argument list, so the real
     code doesn't need to check for that. */
  check_assertion_str2(is_nonreal_member || (list1 != NULL && list2 != NULL),
                       "equiv_template_arg_lists:", " NULL arg list");
  /* Assume they are equivalent, until we find evidence to the contrary. */
  equiv = TRUE;
  /* Loop through both lists in step, comparing arguments. */
  while (arg1 != NULL && arg2 != NULL) {
    /* For a given class, argument lists should always have the same sequence
       of type and constant arguments. */
    if (arg1->is_type != arg2->is_type) {
      equiv = FALSE;
      check_assertion_str(is_nonreal_member,
                          "equiv_template_arg_lists: arg inconsistency");
      break;
    } else if (!arg1->is_type) {
      /* Both are constant arguments.  If they are not identical, this is a
         mismatch. */
      a_constant_ptr con1 = arg1->variant.constant;
      a_constant_ptr con2 = arg2->variant.constant;
      /* Unknown array bounds should not escape the type deduction process. */
      check_assertion(!arg1->is_array_bound_of_unknown_type &&
                      !arg2->is_array_bound_of_unknown_type);
      if (eq_constants(con1, con2) ||
          (error_matches_anything &&
           (is_error_constant(con1) || is_error_constant(con2)))) {
        /* Okay. */
      } else {
        equiv = FALSE;
        break;
      }  /* if */
    } else {
      /* Both are type arguments.  If they are not identical, this is a
         mismatch. */
      a_type_ptr type1 = arg1->variant.type;
      a_type_ptr type2 = arg2->variant.type;
      if (identical_types(type1, type2) ||
          (error_matches_anything &&
           (is_error_type(type1) || is_error_type(type2)))) {
        /* Okay. */
      } else {
        equiv = FALSE;
        break;
      }  /* if */
    }  /* if */
    /* Advance to the next arguments in step. */
    arg1 = arg1->next;
    arg2 = arg2->next;
    /* For a given function argument lists should always be exactly the same
       length. */
    check_assertion_str(is_nonreal_member || (arg1 == NULL) == (arg2 == NULL),
                        "equiv_template_arg_lists: unequal arg list lengths");
  }  /* while */
  if (equiv) {
    /* Make sure we are at the end of both argument lists.  This might not
       be the case for nonreal members. */
    if (arg1 != NULL || arg2 != NULL) equiv = FALSE;
  }  /* if */
  db_exit();
  return equiv;
}  /* equiv_template_arg_lists */


static a_boolean template_arg_involves_template_param(a_template_arg_ptr tap)
/*
Return TRUE if the template argument entry pointed to by tap contains
a template parameter (type or constant).
*/
{
  a_boolean  template_param_found;

  if (tap->is_type) {
    template_param_found = is_or_contains_template_param(tap->variant.type);
  } else {
    template_param_found = (tap->variant.constant->kind ==
                                 (a_constant_repr_kind)ck_template_param);
  }  /* if */
  return template_param_found;
}  /* template_arg_involves_template_param */


a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                 a_template_arg_ptr  *new_list,
				 a_boolean	     prototype_allowed)
/*
Given a symbol for a class template and a template argument list (that is,
a list of actual arguments), look for an existing class that is the
corresponding instantiation of the template.  If none is found, create
such an instantiation (i.e., allocate the type entry and create the
symbol, adding the latter to the instantiation list for the template).
Return the symbol that is found or newly created.

*new_list points to the template argument list of the template class
to be found or created.  If a new template instance is created, the
template argument list is attached to that new instance.  If an
existing instance is found, the template argument list passed by
the caller is discarded.  In either case, the pointer provided by
the caller is set to NULL to prevent subsequent use of the argument
list in case it has been freed.

Note that this function does not fully instantiate a class template;
rather, when it creates a class type entry, it is for an incomplete type.
The full instantiation is done later, when it is clearly needed.  This
allows this kind of code to be handled correctly:

  template <class T> class X;  // class template X is not yet defined.
  X<int> *pxi;                 // declares a pointer to an instantiation of X
                               //   which is incomplete at this point.

The class template X may or may not be defined subsequently, and even if it
is the full instantiation of X<int> may not be needed.  This is consistent
with the handling of pointers to incomplete non-template classes:

  class Y;                     // Y is not yet defined.
  Y *py;                       // pointer to incomplete class is okay.

Note, moreover, that even if class template X were defined there would be
no need to actually instantiate X<int> in the example above.

If prototype_allowed is TRUE then the prototype instantiation is checked
before any of the other instantiations and is returned if the argument
lists match.  If it is FALSE the prototype instantiation will not be
included in the search.
*/
{
  a_symbol_ptr                      sym, prev_sym;
  a_symbol_ptr 			    prototype_sym;
  a_template_arg_ptr                old_list;
  a_type_ptr                        class_type;
  a_template_symbol_supplement_ptr  tssp;
  a_template_arg_ptr                tap;

  db_enter(3, "find_template_class");
  check_assertion(class_template_sym->kind ==
                                            (a_symbol_kind)sk_class_template);
  tssp = class_template_sym->variant.template_info;
  sym = NULL;
  prototype_sym = tssp->variant.class_template.prototype_instantiation;
  if (prototype_allowed && prototype_sym != NULL) {
    /* Old list is the template argument list from a template class that has
       already been created.  See if the list passed in matches it. */
    old_list = prototype_sym->variant.class_struct_union.type->
                     variant.class_struct_union.extra_info->template_arg_list;
    if (equiv_template_arg_lists(old_list, *new_list,
                                 /*error_matches_anything=*/FALSE,
                                 (a_boolean)tssp->is_nonreal_member)) {
      /* A match.  Set sym which will suppress any further search. */
      sym = prototype_sym;
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    /* Make a pass over the symbols representing instantiations of the class
       template. */
    sym = tssp->variant.class_template.instantiations;
    prev_sym = NULL;
    for (; sym != NULL; prev_sym = sym, sym = sym->next) {
      /* The prototype instantiation should not be checked.  If
         prototype_allowed is TRUE then we would have already found it
         in the test above. */
      if (sym == prototype_sym) continue;
      /* Old list is the template argument list from a template class that has
         already been created.  See if the list passed in matches it. */
      old_list = sym->variant.type->
                     variant.class_struct_union.extra_info->template_arg_list;
      if (equiv_template_arg_lists(old_list, *new_list,
                                   /*error_matches_anything=*/FALSE,
                                   (a_boolean)tssp->is_nonreal_member)) {
        /* We've found a match.  Remove the found symbol from its current
           position in the instantiation list and add it to the front. */
        if (prev_sym != NULL) {
          prev_sym->next = sym->next;
          sym->next = tssp->variant.class_template.instantiations;
          tssp->variant.class_template.instantiations = sym;
        }  /* if */
#if DEBUG
        if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (sym == NULL) {
    /* No match was found on the list, so do a partial instantiation of the
       template class based on the template arguments.  First create a symbol
       (but do not enter it into the symbol table, since class templates
       are always looked up through the template. */
    sym = make_template_class_symbol(class_template_sym);
    /* Add the new symbol to the head of the instantiation list. */
    sym->next = tssp->variant.class_template.instantiations;
    tssp->variant.class_template.instantiations = sym;
    /* Now create a new type entry. */
    class_type = alloc_type(tssp->variant.class_template.type_kind);
    sym->variant.class_struct_union.type = class_type;
    if (tssp->is_nonreal_member) {
      /* Instantiations of a nonreal member template (for example,
         T::A<int>) are created as nonreal instantiations. */
      sym->variant.class_struct_union.extra_info->is_nonreal_class = TRUE;
    }  /* if */
    /* If this is a "real instantiation" leave the type incomplete; it will
       become complete when it is instantiated.  However, if it is based on
       template parameters and is therefore a "nonreal" instantiation, give
       it a size and alignment to permit it to pass through subsequent
         processing without causing spurious errors. */
      for (tap = *new_list; tap != NULL; tap = tap->next) {
        if (!sym->variant.class_struct_union.extra_info->is_nonreal_class) {
        if (template_arg_involves_template_param(tap)) {
          sym->variant.class_struct_union.extra_info->is_nonreal_class = TRUE;
        }  /* if */
      }  /* if */
      if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
        /* Local typedef names (legal if they refer to nonlocal types) should
           not be part of the type signature of the template class itself,
           which is nonlocal.  Strip them off, if there are any. */
        if (tap->is_type) {
          tap->variant.type =
                           strip_local_and_nonreal_typedefs(tap->variant.type);
        }  /* if */
      }  /* if */
    }  /* for */
    /* Record the argument list in the type.  It should be available in the
       IL at least for name generation and possibly for debuggers, too.  Note,
       however, that the type itself is not added to the scope types list
       until a full instantiation takes place -- or, if there is none, in
       pop_scope, as with ordinary classes. */
    class_type->variant.class_struct_union.extra_info->
                                            template_arg_list = *new_list;
    set_source_corresp(&(class_type->source_corresp), sym);
    set_membership_in_source_corresp(&(class_type->source_corresp), sym);
    if (sym->is_class_member) {
      /* If this is an instance of a member template, set the access of
         the type based on the access stored in the template. */
      class_type->source_corresp.access =
                      (an_access_specifier)tssp->variant.class_template.access;
    }  /* if */
    /* A template instantiation will have the same name-linkage (C++ or
       internal) as the template itself has. */
    class_type->source_corresp.name_linkage =
                             tssp->variant.class_template.name_linkage;
    if (sym->variant.class_struct_union.extra_info->is_nonreal_class) {
      class_type->size = 1;
      class_type->alignment = 1;
    } else if (sym != prototype_sym) {
      /* Update the friend information associated with this template.
         These are the classes that declared this template as a friend. */
      update_befriending_classes_for_class(tssp, class_type);
      /* Add the type to the types list of the appropriate scope.  Pass
         NO_SCOPE_DEPTH to the subroutine to force it to compute which scope's
         list it belongs to.  add_to_types_list also creates the appropriate
         placeholder typerefs (in case this partial instantiation occurs
         inside a class definition and/or a namespace). */
      add_to_types_list(class_type, NO_SCOPE_DEPTH);
    }  /* if */
#if DEBUG
    if (debug_level >= 3 || db_flag_is_set("instantiations")) {
      db_symbol(sym, "Partial instantiation of: ", 2);
      db_symbol(class_template_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* We are reusing a class type that already exists, so *new_list will not
       be used.  Return the entries to the available list for reuse. */
    free_template_arg_list(*new_list);
  }  /* if */
  /* The list is cleared in all cases.  The caller cannot use the list
     after we return because it may have been freed. */
  *new_list = NULL;
  db_exit();
  return sym;
}  /* find_template_class */


static a_template_arg_ptr get_template_arg_by_list_pos(
                                    a_template_param_ptr      templ_param_list,
                                    a_template_arg_ptr        *templ_arg_list,
                                    a_template_param_list_pos pos)
/*
Given a template parameter list position, return a pointer to the template
argument list element that corresponds to that parameter.  If the template
argument list has not yet been created, create one.  When the list
is initially created, the template arguments will contain NULL type
or constant pointers.  These will be filled in as the argument types
are deduced.
*/
{
  a_template_arg_ptr	tap;

  if (*templ_arg_list == NULL) {
    /* The template argument list does not exist yet.  Create an
       argument list with NULL type/constant pointers. */
    a_template_param_ptr	tpp;
    a_template_arg_ptr		prev_tap = NULL;
    /* Loop through the template parameter list and create a template
       argument entry of the appropriate type for each parameter. */
    for (tpp = templ_param_list; tpp != NULL; tpp = tpp->next) {
      a_template_arg_ptr	tap;
      a_boolean			is_type_param;
      is_type_param = tpp->param_symbol->kind == (a_symbol_kind)sk_type;
      tap = alloc_template_arg(is_type_param);
      if (prev_tap == NULL) {
        /* First iteration -- the start of the list. */
        *templ_arg_list = tap;
      } else {
        /* Add to the end of the list. */
        prev_tap->next = tap;
      }  /* if */
      prev_tap = tap;
    }  /* for */
  }  /* if */
  /* For the nth template parameter find the nth template argument. */
  for (tap = *templ_arg_list; pos > 1; pos--, tap = tap->next);
  return tap;
}  /* get_template_arg_by_list_pos */




static a_template_param_ptr get_template_param_by_list_pos(
                                  a_template_param_ptr       templ_param_list,
                                  a_template_param_list_pos  pos)
/*
Given a template parameter list position, return a pointer to 
a specified parameter.
*/
{
  a_template_param_ptr			tpp;

  tpp = templ_param_list;
  /* For the nth template parameter find the nth template parameter. */
  for (; pos > 1; pos--, tpp = tpp->next);
  return tpp;
}  /* get_template_param_by_list_pos */


static a_boolean is_deducible_constant_param(a_constant_ptr templ_constant)
/*
Return TRUE if the specified constant is a template parameter constant
that can be deduced from a function template call.
*/
{
  a_boolean	result = FALSE;
  if (templ_constant->kind == (a_constant_repr_kind)ck_template_param) {
    /* Nontype parameters can only be deduced from simple uses of the
       parameter, like A<I>.  Expressions are not allowed (e.g.,
       A<I+1>) nor are references to members of a template parameter
       (e.g., T::x).  If either of these is found, type deduction fails. */
    if (templ_constant->variant.template_param.kind ==
                             (a_template_param_constant_kind)tpck_param) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_deducible_constant_param */


static
a_boolean matches_template_constant(a_constant_ptr       constant,
                                    a_constant_ptr       templ_constant,
                                    a_template_arg_ptr   *templ_arg_list,
                                    a_template_param_ptr templ_param_list)
/*
Called by the matches_template_type routines to determine whether
a constant matches a constant template parameter from the parameter
list of a template function.  Returns TRUE if a match is found.
*/
{
  a_boolean	match = FALSE;
 
  if (templ_constant->kind == (a_constant_repr_kind)ck_template_param) {
    if (is_deducible_constant_param(templ_constant)) {
      a_template_arg_ptr        tap;
      /* This is a template parameter from the original source program
         and not a synthesized template parameter. */
      a_template_param_list_pos list_pos;
      list_pos =
           templ_constant->variant.template_param.variant.coordinates.position;
      tap = get_template_arg_by_list_pos(templ_param_list, templ_arg_list,
                                         list_pos);
      /* Now we have the nth template argument, which should correspond to
         the nth template parameter, whose constant is templ_constant. */
      if (tap->is_array_bound_of_unknown_type) {
        if (is_integral_type(constant->type)) {
          /* An array bound can only match an integral value.  We have
             a match if the number of elements matches the previously
             deduced constant. */
          match = cmpulit_integer_constant(constant,
                                           tap->variant.integer_value) == 0;
          if (match) {
            /* The values match.  Use the constant value instead of the
               integer array bound as the new value of the argument. */
            tap->is_array_bound_of_unknown_type = FALSE;
            tap->variant.constant = constant;
          }  /* if */
        }  /* if */
      } else {
        if (tap->variant.constant == NULL) {
          /* No constant has been bound to this template argument yet, so
             just use "constant".  This counts as a match. */
          tap->variant.constant = constant;
          match = TRUE;
        } else {
          /* A constant was already bound to this template argument.  We have a
             match if and only if the new constant is the same as the one
             already there.  This check also makes sure that the types
             of the constants match. */
          match = eq_constants(constant, tap->variant.constant);
        }  /* if */
        /* Note that the type of the constant does not participate in
           type deduction.  Once all of the arguments have been deduced
           the types of the nontype parameters are compared with the
           types in the template parameter list. */
        if (match) {
          /* Make sure the type of the nontype parameter is correct.  This
             can only be done for nontype parameters that do not depend on
             other template parameters.  This will be checked later for
             types that do depend on template parameters. */
          a_template_param_ptr tpp;
          tpp = get_template_param_by_list_pos(templ_param_list, list_pos);
          if (!tpp->variant.constant.type_involves_template_param) {
            if (!identical_types(tap->variant.constant->type,
                                 templ_constant->type)) {
               match = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* The template constant does not involve a template parameter.
       Simply make sure the two constants are the same. */
    match = eq_constants(constant, templ_constant);
  }  /* if */
  return match;
}  /* matches_template_constant */


static
a_boolean matches_template_array_bound(a_targ_size_t        elements,
                                       a_constant_ptr       templ_constant,
                                       a_template_arg_ptr   *templ_arg_list,
                                       a_template_param_ptr templ_param_list)
/*
A nontype template parameter is being deduced from an array bound.
For example:

	template <int I> void f(int n[10][I]);

This kind of deduction is different than others in that the context
from which the value is being deduced does not contain a type.  All
we know of "I" when it is scanned is that it must be integral.  In
the example above, we know that "I" must be "int" because that is how
it is declared, but in cases like this

	template <class T, A<T>::X I> void f(int n[10][I], A<T>);

we don't know the type of "I" until T has been deduced.  In these cases
we simply save the actual value of the array bound and do the checking
of types after all of the function arguments have been processed.
*/
{
  a_boolean	match = FALSE;

  if (!is_deducible_constant_param(templ_constant)) {
    /* The array bound from the template is a constant (under an expression)
       but is not a simple template parameter.  No match. */
  } else {
    a_template_arg_ptr        tap;
    /* This is a template parameter from the original source program
       and not a synthesized template parameter. */
    a_template_param_list_pos list_pos;
    list_pos =
           templ_constant->variant.template_param.variant.coordinates.position;
    tap = get_template_arg_by_list_pos(templ_param_list, templ_arg_list,
                                       list_pos);
    /* Now we have the nth template argument, which should correspond to
       the nth template parameter, whose constant is templ_constant. */
    if (tap->is_array_bound_of_unknown_type || tap->variant.constant == NULL) {
      /* This is either an array bound of unknown type, or no value has
         yet been deduced. */
      if (tap->variant.integer_value == 0) {
        /* No value has been deduced yet.  Use this as the value and
           consider it a match. */
        tap->variant.integer_value = elements;
        match = TRUE;
        tap->is_array_bound_of_unknown_type = TRUE;
      } else {
        /* A previous array bound has been seen.  Make sure the values
           match. */
        match = tap->variant.integer_value == elements;
      }  /* if */
    } else {
      /* A constant value has already been deduced for this argument. */
      a_constant_ptr	cp = tap->variant.constant;
      if (is_integral_type(cp->type)) {
        /* An array bound can only match an integral value.  We have
           a match if the number of elements matches the previously
           deduced constant. */
        match = cmpulit_integer_constant(cp, elements) == 0;
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* matches_template_array_bound */


static a_boolean matches_template_type_for_class_type
                                   (a_type_ptr           type,
                                    a_type_ptr           templ_type,
                                    a_template_arg_ptr   *templ_arg_list,
                                    a_template_param_ptr templ_param_list)
/*
Called by matches_template_type to determine whether a given class type
matches a class type from the parameter list of a template function.
*/
{
  a_boolean			match = FALSE;
  a_class_symbol_supplement_ptr templ_cssp;
  /* Non-identical class types match if one represents a template
     and the other is an instantiation of that template.  For
     instance:
        template <class T> class A {  ...  };
        template <class TT> void f(A<TT>) {  ...  };
        f(A<int>);
     We reach this code when examining the argument in the call to f.
     "type" would refer to A<int> and "templ_type" would refer to
     A<TT>.  First we determine that A<int> and A<TT> refer to the same
     class template, and that the latter is a nonreal instantiation.
     Then we call matches_template_type on the template arg types. */
  templ_cssp = symbol_supplement_for_class(templ_type);
  if (templ_cssp->class_template != NULL &&
      symbol_supplement_for_class(type)->class_template ==
                                     templ_cssp->class_template &&
      templ_cssp->is_nonreal_class) {
    /* The two classes refer to the same template, but templ_type
       is a nonreal instantiation -- i.e., one based on template
       parameter types instead of real types. */
    a_template_arg_ptr  tap, templ_tap;
    tap = type->variant.class_struct_union.extra_info->
                                                   template_arg_list;
    templ_tap = templ_type->variant.class_struct_union.
                                       extra_info->template_arg_list;
    do {
      if (tap->is_type) {
        /* A type template parameter.  See if the types match. */
        match = matches_template_type(tap->variant.type,
                                      templ_tap->variant.type,
                                      templ_arg_list,
                                      templ_param_list,
                                      MTT_NO_FLAGS,
                                      (a_base_class_ptr*)NULL);
      } else {
        /* A nontype template parameter. */
        match = matches_template_constant(tap->variant.constant,
                                          templ_tap->variant.constant,
                                          templ_arg_list,
                                          templ_param_list);
      }  /* if */
      tap = tap->next;
      templ_tap = templ_tap->next;
    } while (match && tap != NULL);
  }  /* if */
  return match;
}  /* matches_template_type_for_class_type */


a_boolean matches_template_type(a_type_ptr           type,
                                a_type_ptr           templ_type,
                                a_template_arg_ptr   *templ_arg_list,
				a_template_param_ptr templ_param_list,
				an_mtt_flag_set      flags,
                                a_base_class_ptr     *base_class_conv_needed)
/*
Compare type and templ_type.  The latter is from a parameter list of a
function template (function params, not template params).  If the types are
identical, return TRUE.  If they are identical but for a template parameter,
return TRUE if the type is consistent with other uses of that template
parameter, as represented in the template argument list.  Otherwise, return
FALSE.  When for the nth template parameter, the nth template arg has not
yet been created, extend the template argument list to include n entries.
flags specifies a set of options used to control how the type matching
is done.  See the MTT flag definitions in templates.h.  The value pointed
to by base_class_conv_needed is set to point to the base class description
if such a conversion is required; otherwise it is set to NULL.
base_class_conv_needed may be NULL if the caller does not need to know
whether a conversion was performed.  templ_param_list points to the
template parameter list.
*/
{
  a_boolean                      match = FALSE;
  a_type_ptr                     tp, ttp;
  a_param_type_ptr               ptp, tptp;
  a_template_arg_ptr             tap;
  a_symbol_ptr                   sym, templ_sym;
  an_mtt_flag_set		 new_flags;

  db_enter(5, "matches_template_type");
  if (base_class_conv_needed != NULL) *base_class_conv_needed = NULL;
  /* When this routine calls itself recursively, the recursive calls
     should not allow conversions or the special unknown implicit
     this parameter checks. */
  new_flags = MTT_NO_FLAGS;
  templ_type = skip_typedefs(templ_type);
  if (is_template_param_type(templ_type)) {
    if (is_qualified_type(templ_type)) {
      /* If the template parameter has any type qualifiers, the argument type
         will have to have a set of type qualifiers that includes any on the
         template parameter.  Remove any that are shared in common and then
         do a check. */
      skip_common_type_qualifiers(&type, &templ_type);
    }  /* if */
    if (is_qualified_type(templ_type)) {
      /* The qualifier on templ_type did not also appear on type, so there is
         no match. */
    } else {
      if (templ_type->variant.template_param.kind ==
                             (a_template_param_type_kind)tptk_param) {
        a_template_param_list_pos list_pos;
        /* This is a template parameter from the original source program
           and not a synthesized template parameter. */
        /* A real type "matches" a template parameter type if it is identical
           to the real type, if any, that was previously associated with that
           template type. */
        list_pos = templ_type->
                      variant.template_param.extra_info->coordinates.position;
        tap = get_template_arg_by_list_pos(templ_param_list, templ_arg_list,
                                           list_pos);
        /* Now we have the nth template argument, which should correspond to
           the nth template parameter, whose type is templ_type. */
        if (tap->variant.type == NULL) {
          /* No type has been bound to this template argument yet, so just use
             "type".  This counts as a match. */
          tap->variant.type = type;
          match = TRUE;
        } else {
          /* A type was already bound to this template argument.  We have a
             match if and only if the new type is the same as the one
             already there. */
          if (identical_types(type, tap->variant.type)) {
            /* Okay. */
            match = TRUE;
          } else {
            /* Not a match.  Return FALSE. */
          }  /* if */
        }  /* if */
      } else {
        /* This is a template parameter associated with a member of a
           proxy class (e.g., X in a type like T::X).  The members must have
           the same name (e.g., T::X matches A::X) and the parent classes
           must match. */
        /* Skip typedefs on the real type. */
        type = skip_typedefs(type);
        if (type->source_corresp.is_class_member) {
          if (!templ_type->source_corresp.is_class_member) {
            /* No parent class -- no match. */
          } else {
            sym = (a_symbol_ptr)type->source_corresp.assoc_info;
            templ_sym = (a_symbol_ptr)templ_type->source_corresp.assoc_info;
            if (sym->header != templ_sym->header) {
              /* Members have different names -- no match. */
            } else {
              /* Convert the proxy class into its associated template
                 parameter and call matches_template_type on the parent
                 type. */
              a_class_symbol_supplement_ptr  cssp;

              tp = type->source_corresp.parent.class_type;
              ttp = templ_type->source_corresp.parent.class_type;
              cssp = symbol_supplement_for_class(ttp);
              ttp = cssp->template_param_for_proxy_class;
              if (ttp != NULL) {
                if (matches_template_type(tp, ttp, templ_arg_list,
  				          templ_param_list,
                                          new_flags,
                                          (a_base_class_ptr*)NULL)) {
                  /* Members have the same names and the parent classes
                     "match".  This will handle cases like T::B. */
                  match = TRUE;
                }  /* if */
              }  /* if */
              if (!match) {
                /* Attempt to match on the class of which this is a member. */
                ttp = templ_type->source_corresp.parent.class_type;
                if (matches_template_type(tp, ttp, templ_arg_list,
  				          templ_param_list,
                                          new_flags,
                                          (a_base_class_ptr*)NULL)) {
                  /* Members have the same names and the parent classes
                     "match".  This will handle cases like A<T>::B. */
                  match = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        } /* if */
      }  /* if */
    }  /* if */
  } else {
    /* The type from the template is not a template parameter type.  Before
       checking further, remove typedefs -- but keep the type qualifiers
       in place. */
    a_type_kind	templ_type_kind;
    a_type_kind	type_kind;
    type = skip_typedefs(type);
    templ_type_kind = templ_type->kind;
    type_kind = type->kind;
    /* Normalize the type kinds so that class and struct are treated as the
       same kind. */
    if (templ_type_kind == (a_type_kind)tk_struct) {
      templ_type_kind = (a_type_kind)tk_class;
    }  /* if */
    if (type_kind == (a_type_kind)tk_struct) {
      type_kind = (a_type_kind)tk_class;
    }  /* if */
    if (templ_type == type) {
      /* Identical type entries, so it's a match. */
      match = TRUE;
    } else if (templ_type_kind != type_kind) {
      /* No match. */
    } else {
      if (type->source_corresp.is_class_member) {
        /* The argument type is a class member -- a nested class or enum.  Be
           sure the parent classes match and that the members correspond (i.e.,
           have the same name). */
        if (!templ_type->source_corresp.is_class_member) {
          /* No match. */
        } else {
          sym = (a_symbol_ptr)type->source_corresp.assoc_info;
          templ_sym = (a_symbol_ptr)templ_type->source_corresp.assoc_info;
          if (sym->header != templ_sym->header) {
            /* Members have different names -- no match. */
          } else {
            tp = type->source_corresp.parent.class_type;
            ttp = templ_type->source_corresp.parent.class_type;
            if (matches_template_type(tp, ttp, templ_arg_list,
                                      templ_param_list,
                                      new_flags,
                                      (a_base_class_ptr*)NULL)) {
              /* Members have the same names and the parent classes "match". */
              match = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (match) {
        /* No need to check further. */
      } else {
        switch (type->kind) {
          case tk_class:
          case tk_struct:
          case tk_union:
            match = matches_template_type_for_class_type(type, templ_type,
                                                         templ_arg_list,
                                                         templ_param_list);
            if (!match && (flags & MTT_ALLOW_CONVERSION != 0)) {
              a_base_class_ptr	bcp;
              /* See if the type matches a base class type of actual argument
                 type.  This is allows a Derived<T> to be passed to a function
                 expecting a Base<T> as an argument. */
              bcp = type->variant.class_struct_union.extra_info->base_classes;
              while (bcp != NULL) {
                match = matches_template_type_for_class_type(bcp->type,
                                                             templ_type,
                                                             templ_arg_list,
                                                             templ_param_list);
                if (match) {
                  if (base_class_conv_needed != NULL) {
                    *base_class_conv_needed = bcp;
                  }  /* if */
                  break;
                }  /* if */
                bcp = bcp->next;
              }  /* while */
            }  /* if */
            break;
          case tk_typeref:
            if (!type_qualifiers_match(type, templ_type)) {
              /* Not a match. */
            } else {
              /* Qualifiers match.  See if the underlying types do, too. */
              tp = type->variant.typeref.type;
              ttp = templ_type->variant.typeref.type;
              match = matches_template_type(tp, ttp, templ_arg_list,
                                            templ_param_list,
                                            new_flags,
                                            (a_base_class_ptr*)NULL);
            }  /* if */
            break;
          case tk_array:
            /* Array types match if their element types match and the number of
               elements is the same. */
            if (type->variant.array.is_variable_size_array) {
              /* If the actual argument has a variable size, this is not a
                 match (this shouldn't happen. */
              unexpected_condition();
            } else if (templ_type->variant.array.is_variable_size_array) {
              /* The type from the template has a variable size.  If the
                 variable size is a constant that refers to a template
                 parameter, then this could be a match. */
              an_expr_node_ptr expr;
              expr = templ_type->variant.array.variant.element_count_expr;
              if (expr->kind == (an_expr_node_kind)enk_constant) {
                a_constant_ptr cp = expr->variant.constant;
                a_targ_size_t  elements;
                elements = type->variant.array.variant.number_of_elements;
                match = matches_template_array_bound(elements, cp,
                                                     templ_arg_list,
                                                     templ_param_list);
              }  /* if */
            } else if (type->variant.array.variant.number_of_elements !=
                        templ_type->variant.array.variant.number_of_elements) {
              /* Both have constant bounds but the number of elements do
                 not match. */
            } else {
              /* The bounds match. */
              match = TRUE;
            }  /* if */
            /* If the bounds match, check the element type. */
            if (match) {
              tp = type->variant.array.element_type;
              ttp = templ_type->variant.array.element_type;
              match = matches_template_type(tp, ttp, templ_arg_list,
                                            templ_param_list,
                                            new_flags,
                                            (a_base_class_ptr*)NULL);
            }  /* if */
            break;
          case tk_pointer:
            /* Pointer matches pointer and reference matches reference, but
               they can't be mixed. */
            if (type->variant.pointer.is_reference !=
                           templ_type->variant.pointer.is_reference) {
              /* Not a match. */
            } else {
              tp = type->variant.pointer.type;
              ttp = templ_type->variant.pointer.type;
              match = matches_template_type(tp, ttp, templ_arg_list,
                                            templ_param_list,
                                            new_flags,
                                            (a_base_class_ptr*)NULL);
            }  /* if */
            break;
          case tk_ptr_to_member:
            /* For ptr-to-member types, there needs to be a match on both the
               member types and the class-of-which-a-member. */
            tp = type->variant.ptr_to_member.type;
            ttp = templ_type->variant.ptr_to_member.type;
            if (matches_template_type(tp, ttp, templ_arg_list,
                                      templ_param_list,
                                      new_flags,
                                      (a_base_class_ptr*)NULL)) {
              tp = type->variant.ptr_to_member.class_of_which_a_member;
              ttp = templ_type->variant.ptr_to_member.class_of_which_a_member;
              match = (matches_template_type(tp, ttp, templ_arg_list,
                                             templ_param_list,
                                             new_flags,
                                             (a_base_class_ptr*)NULL));
            }  /* if */
            break;
          case tk_routine:
            /* For routine types there has to be a match both on the return
               types and on all the parameter types.  In addition, the
               has-ellipsis flags should be set the same. */
            tp = type->variant.routine.return_type;
            ttp = templ_type->variant.routine.return_type;
            if (matches_template_type(tp, ttp, templ_arg_list,
                                      templ_param_list,
                                      new_flags,
                                      (a_base_class_ptr*)NULL) &&
                (type->variant.routine.extra_info->has_ellipsis ==
                    templ_type->variant.routine.extra_info->has_ellipsis)) {
              /* Return type and ellipsis are okay.  Check the param types. */
              ptp = type->variant.routine.extra_info->param_type_list;
              tptp = templ_type->variant.routine.extra_info->param_type_list;
              for (;;) {
                if (ptp == NULL || tptp == NULL) {
                  /* One or both of the param type lists is exhausted.  It's a
                     match only if they're both done. */
                  match = (ptp == tptp);
                  break;
                }  /* if */
                tp = ptp->type;
                ttp = tptp->type;
                if (!matches_template_type(tp, ttp, templ_arg_list,
                                           templ_param_list,
                                           new_flags,
                                           (a_base_class_ptr*)NULL)) {
                  /* The first param type for which there is a mismatch causes
                     a mismatch for the entire type.  No need to keep
                     looping. */
                  break;
                }  /* if */
                ptp = ptp->next;
                tptp = tptp->next;
              }  /* for */
              if (match) {
                /* The routine types match so far.  Make sure the implicit
                   this parameters, if present, match. */
                tp =  type->variant.routine.extra_info->
                                                     implicit_this_param_type;
                ttp =  templ_type->variant.routine.extra_info->
                                                     implicit_this_param_type;
                if (tp == NULL || ttp == NULL) {
                  /* One or both of the types does not have an implicit
                     this parameter.  This is okay if they are both NULL. 
                     It is also okay if the type has no implicit this type
                     the unknown implicit this type flag was passed in, and
                     the other this parameter type has no qualifiers. */
                  if (tp == ttp) {
                    /* They are both NULL, this is a match. */
                    match = TRUE;
                  } else if (ttp == NULL) {
                    /* The template type is NULL and the other type is not.
                       This is not a match. */
                    match = FALSE;
                  } else { /* tp == NULL */
                    /* The template type is not NULL.  This is a match when
                       the unknown implicit this flag is set and the this
                       parameter from the template has no qualifiers. */
                    match = FALSE;
                    if ((flags & MTT_UNKNOWN_IMPLICIT_THIS_TYPE) != 0) {
                      /* Get the type pointed to by the this parameter. */
                      a_type_ptr	this_type = type_pointed_to(ttp);
                      match = get_type_qualifiers(this_type) ==
                                                (a_type_qualifier_set)TQ_NONE;
                    }  /* if */
                  }  /* if */
                } else {
                  /* They both have implicit this parameters, make sure the
                     types match. */
                  match = matches_template_type(tp, ttp, templ_arg_list,
                                                templ_param_list,
                                                new_flags,
                                                (a_base_class_ptr*)NULL);
                }  /* if */
              }  /* if */
            }  /* if */
            break;
          default:
            /* They are simple types -- these are leaf nodes in a type tree.
               Check for identity. */
            match = identical_types(templ_type, type);
        }  /* switch */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return match;
}  /* matches_template_type */


a_boolean tentatively_matches_template_type(
			       a_type_ptr           type,
		  	       a_type_ptr           templ_type,
                               a_template_param_ptr templ_param_list)
/*
This routine calls matches_template_type to determine whether the
type specified by "type" matches the type specified by "templ_type" with
appropriate substitution of the template parameters in "templ_type".
We return TRUE if the types match.  This routine is an interface to
matches template type that is used to evaluate the match for a
single function parameter and then discard any template arguments that
may have been deduced.
*/
{
  a_template_arg_ptr   templ_arg_list = NULL;
  a_boolean            result;

  db_enter(5, "tentatively_matches_template_type");
  result = matches_template_type(type, templ_type, &templ_arg_list,
                                 templ_param_list, MTT_NO_FLAGS,
                                 (a_base_class_ptr*)NULL);
  if (templ_arg_list != NULL) free_template_arg_list(templ_arg_list);
  db_exit();
  return result;
}  /* tentatively_matches_template_type */


a_boolean verify_function_template_nontype_args(
                                        a_template_arg_ptr   templ_arg_list,
                                        a_symbol_ptr         rout_templ_sym,
                                        a_template_param_ptr templ_param_list)
/*
This routine is used after doing argument deduction for each function
argument to ensure that any nontype parameter whose type depends on a
template parameter is consistent with the deduced value.  Also,
types are supplied for nontype parameters that are deduced entirely
from array bounds.  templ_param_list is the template parameter list to
be used.  If a NULL pointer is provided, the template parameter list
from the template symbol supplement is used.  The parameter is supplied
because some calls of this routine occur before the field in the
template symbol supplement has been set.
*/
{
  a_boolean				match = TRUE;
  a_template_param_ptr			tpp;
  a_template_arg_ptr			tap;

  if (templ_param_list == NULL) {
    a_template_symbol_supplement_ptr	tssp;
    a_template_decl_info_ptr		tdip;
    tssp = template_supplement_for_symbol(rout_templ_sym);
    /* The decl_info pointer can be NULL if the template parameter list is
       missing (in an error case), and the template declaration information
       has not yet been filled in. */
    tdip = tssp->variant.function.decl_cache.decl_info;
    templ_param_list = tdip != NULL ? tdip->parameters : NULL;
  }  /* if */
  /* Make an initial pass through the argument list to see if all of the
     arguments have deduced values. */
  tpp = templ_param_list;
  tap = templ_arg_list;
  for (; tpp != NULL; tpp = tpp->next, tap = tap->next) {
    a_boolean	arg_okay = FALSE;
    arg_okay = tap != NULL &&
               ((tap->is_type && tap->variant.type != NULL) ||
                (tap->is_array_bound_of_unknown_type) ||
                (tap->variant.constant != NULL));
    if (!arg_okay) {
      match = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (match) {
    tpp = templ_param_list;
    tap = templ_arg_list;
    for (; tpp != NULL; tpp = tpp->next, tap = tap->next) {
      a_type_ptr	constant_type;
      /* Only nontype parameters need to be processed. */
      if (tap->is_type) continue;
      if (tpp->variant.constant.type_involves_template_param) {
        /* Rescan the tokens that make up the parameter declaration. */
        constant_type = rescan_template_constant_parameter
                                   (rout_templ_sym, tpp->param_symbol, tpp,
                                    templ_arg_list, /*do_default_arg=*/FALSE,
                                    (a_constant_ptr*)NULL);
      } else {
        constant_type = tpp->variant.constant.ptr->type;
      }  /* if */
      if (tap->is_array_bound_of_unknown_type) {
        /* The constant was deduced from an array bound and does not yet
           have a type.  Make sure the declared type is integral, then
           create a constant of the appropriate type. */
        if (!is_integral_type(constant_type)) {
          match = FALSE;
        } else {
          a_constant_ptr	constant;
          constant = fs_constant((a_constant_repr_kind)ck_integer);
          set_unsigned_integer_constant
                       (constant, (unsigned long)tap->variant.integer_value,
                        constant_type->variant.integer.int_kind);
          tap->variant.constant = constant;
          tap->is_array_bound_of_unknown_type = FALSE;
        }  /* if */
      } else {
        /* The template argument has a deduced value with a type.  The
           type must match the declared type.  This test is only needed if
           the type involves a template parameter. */
        if (tpp->variant.constant.type_involves_template_param) {
          match = identical_types(constant_type, tap->variant.constant->type);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return match;
}  /* verify_function_template_nontype_args */



#if CHECKING
static void check_function_template_arg_list(
                                    a_template_arg_ptr  templ_arg_list,
                                    a_symbol_ptr        templ_sym)
/*
Do some simple consistency checking on a function template argument list.
*/
{
  a_template_param_ptr  		tpp;
  a_template_arg_ptr    		tap;
  a_template_symbol_supplement_ptr	tssp;

  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  tpp = tssp->variant.function.decl_cache.decl_info->parameters;
  for (tap = templ_arg_list; tap != NULL; tap = tap->next) {
    check_assertion_str2((tap->is_type && tap->variant.type != NULL) ||
                         (!tap->is_type && tap->variant.constant != NULL),
                         "check_template_arg_list:",
                         "missing type or constant pointer");
    if (tpp == NULL) {
      internal_error("check_template_arg_list: too many template args");
    }  /* if */
    tpp = tpp->next;
  }  /* for */
  if (tpp != NULL) {
    internal_error("check_template_arg_list: too few template args");
  }  /* if */
}  /* check_template_arg_list */
#endif /* CHECKING */


void delayed_scan_for_function_template_default_args(
		    a_routine_ptr		     templ_rout,
		    a_routine_ptr		     rout_ptr,
                    a_template_instance_ptr	     tip,
                    a_template_symbol_supplement_ptr tssp,
                    a_boolean                        push_instantiation_scope)
/*
Rescan the default arguments of a function template.  push_instantiation_scope
is TRUE if an instantiation scope should be pushed for each argument scanned.
It is FALSE if the instantiation scope was pushed by the caller.
*/
{
  a_def_arg_expr_fixup_ptr	daefp;
  a_param_type_ptr		templ_ptp;
  a_param_type_ptr		ptp;
  a_type_ptr			templ_rout_type;
  a_type_ptr			rout_type;
  a_func_info_block		*func_info_ptr;

  templ_rout_type = skip_typerefs(templ_rout->type);
  rout_type = skip_typerefs(rout_ptr->type);
  func_info_ptr = func_info_for_template(tssp);
  daefp = tssp->variant.function.def_arg_expr_list;
  if (daefp != NULL) {
    templ_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    /* Loop through the two linked lists of param_type entries and the
       default argument expression fixup entries, and update the default
       arg expressions in the corresponding the param_type entries. */
    for (; ptp != NULL; ptp = ptp->next, templ_ptp = templ_ptp->next) {
      if (templ_ptp->has_default_arg) {
	check_assertion(daefp != NULL);
        if (push_instantiation_scope) {
          /* Push the template instantiation scope. */
          (void)push_template_instantiation_scope(daefp->cache.decl_info,
                                                  (a_type_ptr)NULL, rout_ptr,
                                                  tip->instance_sym,
                                                  tip->template_sym,
                                                  rout_ptr->template_arg_list);
        }  /* if */
        /* The function prototype scope should be reactivated and its symbols
           reentered because parameter names hide names from enclosing scopes
           and, moreover, may not be used in default argument expressions
           (ARM 8.2.6). */
        (void)push_scope((a_scope_kind)sck_func_prototype,
                         daefp->cache.decl_info->declaration_scope,
                         (a_type_ptr)NULL,
                         (a_routine_ptr)NULL);
        if (func_info_ptr->prototype_scope_symbols != NULL) {
          reactivate_prototype_scope_symbols(
                                     func_info_ptr->prototype_scope_symbols);
        }  /* if */
        /* Update the default argument expression entry to point to the
           current param type entry. */
	daefp->param_type = ptp;
        ptp->has_default_arg = TRUE;
        /* It's a default arg expression that needs to be rescanned. */
        /* Let get_token know about the cache. */
        rescan_reusable_cache(&daefp->cache.tokens);
        delayed_scan_of_default_arg_expr(daefp->param_type,
                                         /*check_for_errors=*/FALSE);
        daefp = daefp->next;
        /* Restore the prototype scope symbols pointer in the func_info
           block. It shouldn't have changed, but we do it to be safe. */
        func_info_ptr->prototype_scope_symbols =
             assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
        /* Pop the reactivated function prototype scope off the stack. */
        pop_scope();
        if (push_instantiation_scope) {
          /* Pop the template instantiation scope. */
          pop_template_instantiation_scope();
        }  /* if */
      }  /* if */
    }  /* for */
    check_assertion(daefp == NULL);
  }  /* if */
}  /* delayed_scan_for_function_template_default_args */


static void scan_template_declaration(a_boolean         is_initial_decl,
                                      a_boolean         is_member_decl,
                                      a_type_ptr	parent_class,
				      a_boolean         decl_scope_err,
				      a_boolean		is_specialization,
                                      a_decl_flag_set   *dso_flags,
                                      a_decl_flag_set   *do_flags,
                                      a_symbol_locator  *locator,
                                      a_type_ptr        *type,
                                      a_func_info_block *func_info,
                                      a_storage_class   *storage_class,
                                      a_decl_modifier	*decl_modifiers)
/*
Calls decl_specifiers and declarator to scan a template declaration of
a function or static data member.  is_initial_decl is TRUE if this
is being called to scan the original declaration and is FALSE when
rescanning the tokens to generate a type for a specific instance
of a function template.
*/
{
  a_decl_flag_set              dsi_flags;
  a_decl_flag_set              di_flags;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;
  a_type_qualifier_set         qualifiers;
  a_source_position            decl_start_pos;

  dsi_flags = DSI_INLINE_ALLOWED |
              DSI_TYPE_SPECIFIER_ALLOWED |
              DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
              DSI_STORAGE_CLASS_SPECIFIER_ALLOWED;
  di_flags = DI_REAL_DECLARATOR_ALLOWED |
             DI_QUALIFIED_NAME_ALLOWED |
             DI_PARENTHESIZED_INITIALIZER_ALLOWED |
             DI_OPERATOR_NAME_ALLOWED;
  if (is_initial_decl) {
    dsi_flags |= DSI_IS_TEMPLATE_DECLARATION;
    di_flags |= DI_IS_TEMPLATE_DECLARATION;
    if (is_specialization) di_flags |= DI_IS_SPECIALIZATION;
    /* An end-of-source marker is not present when the initial declaration
       is scanned. */
    add_stop_token(tok_lbrace);
    add_stop_token(tok_colon);
    add_stop_token(tok_semicolon);
    if (!decl_scope_err) {
      /* This should only be done when we know that this is a valid template
         declaration scope. */
      begin_deferral_of_access_checks();
    }  /* if */
  } else {
    add_stop_token(tok_end_of_source);
  }  /* if */
  if (is_member_decl) {
    /* This is a declaration inside a class definition. */
    dsi_flags |= DSI_IS_MEMBER_DECLARATION;
  }  /* if */
  decl_start_pos = pos_curr_token;
  (void)decl_specifiers(dsi_flags, dso_flags, storage_class, type,
                        &qualifiers, decl_modifiers);
  if (is_error_type(*type) && !is_declarator_start()) {
    /* Error of some sort. */
    set_to_error_locator(*locator);
    *do_flags = 0;
  } else {
    a_boolean	friend_specified = (*dso_flags & DSO_FRIEND) != 0;
    if (friend_specified) {
      di_flags |= DI_IS_FRIEND_DECL;
    }  /* if */
    if (*storage_class != (a_storage_class)sc_static &&
        !friend_specified && parent_class != NULL) {
      /* The storage class "static" was not specified and this is a member
         declaration that is not a friend declaration, therefore, if this
         is a member function declaration, it will be a nonstatic member
         function.  This is important because when the routine type
         is created, function_declarator needs to know whether to
         add an implicit this-param pointer to the type. */
      di_flags |= DI_NONSTATIC_MEMBER;
    }  /* if */
    if (is_member_decl && (*dso_flags & DSO_CONSTRUCTOR) != 0) {
      di_flags |= DI_IS_CONSTRUCTOR;
    }  /* if */
    if (!(*dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) &&
        qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
    declarator(di_flags, do_flags, *type,
               !friend_specified ? parent_class : (a_type_ptr)NULL,
               locator, type,
               &declarator_ssep, func_info);
    if (decl_scope_err) {
      /* Just to be sure a template symbol doesn't get added to a scope that
         is not equipped to handle it, create an error locator based on the
         previously reported error. */
      set_to_named_error_locator(*locator);
    }  /* if */
    func_info->is_inline = ((*dso_flags & DSO_INLINE) != 0);
    /* Note whether this is a function type that comes from a typedef.  The
       setting is checked later if this turns out to be a function template
       definition. */
    if (is_function_type(*type) &&
        (*type)->kind == (a_type_kind)tk_typeref) {
      func_info->function_type_from_typedef = TRUE;
    }  /* if */
    if (is_function_type(*type) && parent_class == NULL &&
        locator->is_class_member) {
      /* This is a member template declaration outside the class definition,
         so a storage class may not be specified (as in the nontemplate
         case). */
      if (*storage_class != (a_storage_class)sc_unspecified) {
        pos_error(ec_storage_class_not_allowed, &decl_start_pos);
        *storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_initial_decl) {
    /* An end-of-source marker is not present when the initial declaration
       is scanned. */
    remove_stop_token(tok_lbrace);
    remove_stop_token(tok_colon);
    remove_stop_token(tok_semicolon);
    if (!decl_scope_err) {
      /* We can't reliably check accesses in template declarations.  We need to
         wait until we have an instance. */
      discard_deferred_access_checks();
      end_deferral_of_access_checks();
    }  /* if */
  } else {
    remove_stop_token(tok_end_of_source);
    /* In the normal case the current token should be end_of_source,
       which was inserted to mark the end of the cached token stream.
       If necessary, keep flushing until end-of-source is found. */
    flush_past_token_cache_terminator();
  }  /* if */
}  /* scan_template_declaration */


static a_type_ptr scan_member_declaration(a_type_ptr	parent_class)
/*
Calls rescan_member_template_declaration to rescan the tokens of a
member function template to produce the type for the instance and to
detect any errors that should be diagnosed.
*/
{
  a_type_ptr	instance_type;

  add_stop_token(tok_end_of_source);
  instance_type = rescan_member_template_declaration(parent_class);
  remove_stop_token(tok_end_of_source);
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token stream.
     If necessary, keep flushing until end-of-source is found. */
  flush_past_token_cache_terminator();
  return instance_type;
}  /* scan_member_declaration */


static void update_befriending_classes_for_function
                           (a_template_symbol_supplement_ptr tssp,
			    a_routine_ptr                    rout_ptr)
/*
Loop through the list of classes that have declared this template
function a friend and update the friend information.
*/
{
  a_class_list_entry_ptr   clep;

  for (clep = tssp->befriending_classes; clep != NULL; clep = clep->next) {
    update_friend_function_info(rout_ptr, clep->class_type,
                                /*is_definition=*/FALSE,
                                /*move_to_front=*/FALSE);
  }  /* for */
  if (tssp->prototype_template != NULL) {
    /* This function is an instance of a member template declared in a
       class template.  The template can be made a friend as
       a member of the class template:
         template <class T> template <class T2> friend void A<T>::f(T2);
       as a member of an instance:
         template <> template <class T2> friend void A<int>::f(T2);
       or a combination of the two.  The code above will handle declarations
       that make a member of an instance a friend.  We call this routine
       recursively to pick up any friend declarations that made the function
       template member a friend. */
    a_symbol_ptr			prototype_sym;
    a_template_symbol_supplement_ptr	prototype_tssp;
    prototype_sym = tssp->prototype_template;
    prototype_tssp = template_supplement_for_symbol(prototype_sym);
    update_befriending_classes_for_function(prototype_tssp, rout_ptr);
  }  /* if */
}  /* update_befriending_classes_for_function */


static a_symbol_ptr make_template_function(a_symbol_ptr        templ_sym,
                                           a_template_arg_ptr  templ_arg_list)
/*
Allocate the symbol and routine entry for a template function, based on
the function template (represented by templ_sym), and allocate and enter
the associated function instantiation entry, where the template arg list
for the instantiation (templ_arg_list) is also recorded.  Create a routine
type based on the template argument list and the template parameter list
(reached through templ_sym).
*/
{
  a_symbol_ptr                      sym = NULL;
  a_template_symbol_supplement_ptr  tssp;
  a_memory_region_number            region_to_switch_back_to;
  a_template_instance_ptr           tip;
  a_routine_ptr                     templ_rout, rp;
  a_type_ptr			    rout_type = NULL;
  a_decl_flag_set		    dso_flags;
  a_boolean			    is_member_decl;
  a_type_ptr	      		    parent_class;

  db_enter(4, "make_template_function");
#if CHECKING
  check_function_template_arg_list(templ_arg_list, templ_sym);
#endif /* CHECKING */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
#if 0
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
#else /* 0 */
    unexpected_condition();
#endif /* if 0 */
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  templ_rout = tssp->variant.function.routine;
  /* All IL routines must be at the file scope level, so switch to that
     memory region if necessary to allocate the routine entry. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  rp = alloc_routine();
  {
    /* Create a routine type by rescanning the original declaration
       with the template parameters updated to refer to the appropriate
       template arguments.  This is done even if a type already exists
       because additional error checking is done during the declaration
       processing. */
    a_source_position    saved_pos_curr_token;
    a_source_position    saved_error_position;
#if MICROSOFT_EXTENSIONS_ALLOWED
    a_source_position	 locator_position;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    a_template_cache_ptr tcp;

    /* Push the template instantiation scope.  Note that the instance symbol
       passed to push_template_instantiation_scope is NULL.  This is done
       because the type associated with the symbol is not yet complete
       (it has no routine type).  Using a partially constructed symbol could
       cause problems if errors occur while rescanning the declaration. */
    tcp = &tssp->variant.function.decl_cache;
    (void)push_template_instantiation_scope(tcp->decl_info,
					    (a_type_ptr)NULL,
					    (a_routine_ptr)NULL,
					    (a_symbol_ptr)NULL, templ_sym,
					    templ_arg_list);
    /* Reactivate any pragmas that should be bound to the generated
       instance. */
    reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
    /* Rescan the tokens of the function declaration. */
    saved_pos_curr_token = pos_curr_token;
    saved_error_position = error_position;
    begin_deferral_of_access_checks();
    rescan_reusable_cache(&tcp->tokens);
    /* Note that is_member_decl is TRUE if the declaration was found in a
       class context, while parent_class contains a pointer to the class of
       which the template is a member.  In other words, is_member_decl will
       also be set for friend declarations for which parent_class is
       either NULL, or refers to some other class. */
    is_member_decl =
         tssp->variant.function.decl_cache.decl_info->enclosing_scope->kind ==
                                          (a_scope_kind)sck_class_struct_union;
    parent_class = templ_sym->is_class_member ? templ_sym->parent.class_type
                                              : (a_type_ptr)NULL;
    if (parent_class != NULL) {
#if MICROSOFT_EXTENSIONS_ALLOWED
      locator_position = pos_curr_token;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      rout_type = scan_member_declaration(parent_class);
#if 0
      /* We should get the locator position returned. */
#endif
    } else {
      a_decl_flag_set	 do_flags;
      a_func_info_block	 func_info;
      a_storage_class    storage_class;
      a_symbol_locator	 locator;
      a_decl_modifier	 decl_modifiers;

      clear_func_info(&func_info);
      scan_template_declaration(/*is_initial_decl=*/FALSE,
                                is_member_decl, parent_class,
  			        /*decl_scope_err=*/FALSE,
				/*is_specialization=*/FALSE,
                                &dso_flags, &do_flags, &locator,
                                &rout_type, &func_info, &storage_class,
                                &decl_modifiers);
      done_with_func_info(func_info);
#if MICROSOFT_EXTENSIONS_ALLOWED
      locator_position = locator.source_position;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    error_position = saved_error_position;
    pos_curr_token = saved_pos_curr_token;
    /* Allocate the template function symbol.  Note that it is not entered
       into the symbol table -- it will appear on a function instantiation
       list under the function template symbol and, optionally, in the overload
       list if it is also explicitly declared by the user. */
    { a_type_ptr	return_type = NULL;
      if (is_function_type(rout_type)) {
        return_type = skip_typerefs(rout_type)->variant.routine.return_type;
      }  /* if */
      sym = make_template_function_symbol(templ_sym, &templ_sym->decl_position,
                                          return_type);
      sym->variant.routine.ptr = rp;
    }
    /* Give the routine entry the type passed in, and set other fields in
       accord with the settings in the template. */
    rp->type = rout_type;
    rp->storage_class = templ_rout->storage_class;
    rp->special_kind = templ_rout->special_kind;
    rp->opname_kind = templ_rout->opname_kind;
    rp->is_inline = templ_rout->is_inline;
    rp->is_explicit_constructor = templ_rout->is_explicit_constructor;
    rp->is_template_function = TRUE;
    set_source_corresp(&rp->source_corresp, sym);
    set_membership_in_source_corresp(&rp->source_corresp, sym);
    rp->source_corresp.name_linkage = templ_rout->source_corresp.name_linkage;
    rp->source_corresp.access = templ_rout->source_corresp.access;
    rp->template_arg_list = templ_arg_list;
    update_routine_decl_modifiers(rp, templ_rout->decl_modifiers,
                                  &locator_position,
                                  /*is_redecl=*/FALSE, /*is_definition=*/TRUE);
    /* Add it to the routines list of the appropriate scope; NO_SCOPE_DEPTH
       is passed in to cause the scope to be computed. */
    add_to_routines_list(rp, NO_SCOPE_DEPTH);
    update_befriending_classes_for_function(tssp, rp);
    perform_deferred_access_checks_for_function(rp);
    end_deferral_of_access_checks();
  }
  /* Create the associated function instantiation entry and link it
     onto the front of the instantiation list for the template. */
  tip = alloc_template_instance();
  tip->template_sym = templ_sym;
  tip->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = tip;
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  tip->instance_sym = sym;
  sym->variant.routine.instance_ptr = tip;
  /* Process any pragmas that are to be bound to this instance. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  {
    a_symbol_locator	locator;
    /* If there are default arguments whose types depend on template
       parameters, scan the default argument expressions. */
    if (tssp->variant.function.def_arg_expr_list != NULL) {
      delayed_scan_for_function_template_default_args
			                   (templ_rout, rp, tip, tssp,
                                            /*push_instantiation_scope=*/TRUE);
    }  /* if */
    /* If this is a user-defined conversion or an overloaded operator,
       check for errors in the argument list.  The routine we are
       calling requires a locator.  Make a locator and fill in the
       information needed. */
    make_locator_for_symbol(sym, &locator);
    if (rp->special_kind == (a_special_function_kind)sfk_operator) {
      locator.is_operator_name = TRUE;
      locator.variant.opname = rp->opname_kind;
    } else if (rp->special_kind == (a_special_function_kind)sfk_conversion) {
      locator.is_conversion_name = TRUE;
      locator.variant.conversion_result_type = NULL;
    }  /* if */
    check_operator_function_params(rout_type, parent_class, &locator);
  }
  /* Pop the template instantiation scope. */
  pop_template_instantiation_scope();
  switch_back_to_original_region(region_to_switch_back_to);
  /* Function instantiation entries are not marked for actual instantiation
     (that is, for generation of the function body) until there is an
     invocation of the function.  In tim_all mode the instantiations
     will be generated even if the instantiation required flag is not
     set. */
  if (!tip->instantiation_required) {
    /* If the flag is set then the entry is already on the list and the flag
       should not be reset. */
    update_instantiation_required_flag(tip, /*value=*/FALSE,
                                       /*defer_inline=*/FALSE);
  }  /* if */
  db_exit();
  return sym;
}  /* make_template_function */


a_boolean is_match_for_function_template(a_symbol_ptr         templ_sym,
                                         a_type_ptr           curr_type,
                                         a_template_arg_ptr   *templ_arg_list,
                                         a_symbol_ptr         *instance_sym,
                                         a_template_param_ptr templ_param_list,
					 a_boolean	      is_decl_context)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If such a template
function exists, return its symbol.  Otherwise, try to generate a template
arg list to serve as the basis for creating one.  If either a symbol can
be found or a template arg list can be created, return TRUE; otherwise,
return FALSE.

is_decl_context is TRUE if this routine is called to match a declaration with
a template instance.  In such cases it is not known whether or not the
function has an implicit this parameter type, so the implicit this
type should not be used in the matching process.
*/
{
  a_boolean                         match = FALSE;
  a_symbol_ptr                      sym = NULL;
  a_type_ptr                        rout_type, templ_rout_type;
  a_template_symbol_supplement_ptr  tssp;
  a_template_instance_ptr           tip;
  a_param_type_ptr                  ptp, other_ptp;
  a_routine_type_supplement_ptr	    curr_rtsp;
  a_routine_type_supplement_ptr	    templ_rtsp;

  db_enter(3, "is_match_for_function_template");
  curr_type = skip_typerefs(curr_type);
  curr_rtsp = curr_type->variant.routine.extra_info;
#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("is_match_for_function_template: expected routine type");
  }  /* if */
#endif /* CHECKING */
  *templ_arg_list = NULL;
  *instance_sym = NULL;
  /* sym is the symbol for a template function to be returned.  Returning NULL
     means no template function could be found or created. */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  templ_rout_type = skip_typerefs(tssp->variant.function.routine->type);
  templ_rtsp = templ_rout_type->variant.routine.extra_info;
  /* First be sure the number of parameters in the template function is
     equal to the number in param_type_list. */
  ptp = curr_rtsp->param_type_list;
  other_ptp = templ_rtsp->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (other_ptp == NULL) {
      /* Too many params to match this template. */
      goto done;
    }  /* if */
    other_ptp = other_ptp->next;
  }  /* if */
  if (other_ptp != NULL) {
    /* Too many args in function template (and therefore in each of its
       instantiations) to justify looking any further. */
    goto done;
  }  /* if */
  if (curr_rtsp->has_ellipsis != 
      templ_rtsp->has_ellipsis) {
    /* One routine has an ellipsis argument and the other does not.
       This cannot be a match. */
    goto done;
  }  /* if */
  /* Make a pass over the entries representing instantiations of the function
     template to see if any of them match the current type signature. */
  for (tip = tssp->variant.function.instantiations;
       tip != NULL;
       tip = tip->next) {
    /* We used to skip entries that represent specific declarations.
       This is no longer done because these entries must be examined this
       routine is called during instantiation pragma processing. */
    match = TRUE;
    sym = tip->instance_sym;
    rout_type = skip_typerefs(sym->variant.routine.ptr->type);
    if (is_decl_context) {
      /* In declaration contexts we do not yet know whether the type
         has an implicit this type.  Consequently, a NULL implicit this
         type should be considered a match for a non-NULL one in the
         routine we are matching with. */
      match = unknown_implicit_this_identical_types(curr_type, rout_type);
    } else {
      /* In nondeclarative contexts, the implicit this parameter types must
         match exactly. */
      match = identical_types(curr_type, rout_type);
    }  /* if */
    if (!match) continue;
    /* Falling through to here means curr_type exactly matches the function
       type for sym.  Skip over the remaining processing and return sym to
       the caller. */
    *instance_sym = sym;
    goto done;
  }  /* for */
  /* Falling through to here means the type signature passed in does not
     match any existing template function based on the function template in
     question, but that it is not disqualified on other grounds.  Try to match
     the type signature to the template's type signature.  If successful, a
     template arg list is returned; otherwise, NULL is returned. */
  if (matches_template_type(curr_type, templ_rout_type, 
                            templ_arg_list, templ_param_list,
                            (an_mtt_flag_set)
                            (is_decl_context ? MTT_UNKNOWN_IMPLICIT_THIS_TYPE
                                            : MTT_NO_FLAGS),
                            (a_base_class_ptr*)NULL)) {
    match = TRUE;
  }  /* if */
  /* Make sure that the types of nontype template parameters that depend
     on other template parameters agree with the types of the deduced
     values. */
  if (match) {
    match = verify_function_template_nontype_args(*templ_arg_list, templ_sym,
                                                  templ_param_list);
  }  /* if */
done:
  if (!match && *templ_arg_list != NULL) {
    /* If there was not a match but a template argument list was created, the
       latter will not be used and may be returned for reuse. */
    free_template_arg_list(*templ_arg_list);
    *templ_arg_list = NULL;
  }  /* if */
  db_exit();
  return match;
}  /* is_match_for_function_template */


a_symbol_ptr matching_template_function(a_symbol_ptr        templ_sym,
                                        a_type_ptr          curr_type,
					a_boolean	    is_decl_context)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If no such template
function exists, try to create one.  If the search/creation is successful
return a pointer to the symbol; otherwise, return NULL.

is_decl_context is TRUE if this routine is called to match a declaration with
a template instance.  In such cases it is not known whether or not the
function has an implicit this parameter type, so the implicit this
type should not be used in the matching process.
*/
{
  a_symbol_ptr          		sym;
  a_template_arg_ptr    		templ_arg_list;
  a_template_symbol_supplement_ptr	tssp;
  a_template_param_ptr			templ_param_list;

  db_enter(3, "matching_template_function");
#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("matching_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  curr_type = skip_typerefs(curr_type);
  tssp = template_supplement_for_symbol(templ_sym);
  templ_param_list = tssp->cache.decl_info->parameters;
  if (is_match_for_function_template(templ_sym, curr_type,
                                     &templ_arg_list, &sym,
                                     templ_param_list, is_decl_context)) {
    if (sym != NULL) {
      /* A match has been found -- just return a pointer to it. */
    } else {
      /* Use the template arg list to create a new symbol. */
      sym = make_template_function(templ_sym, templ_arg_list);
    }  /* if */
  }  /* if */
  db_exit();
  return sym;
}  /* matching_template_function */


static
a_boolean has_matching_template_function(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
		  		         a_boolean	    is_decl_context)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  Return TRUE if a
match is found.  This routine is like matching_template_function except
that an actual instance is not generated if one does not already exist.

is_decl_context is TRUE if this routine is called to match a declaration with
a template instance.  In such cases it is not known whether or not the
function has an implicit this parameter type, so the implicit this
type should not be used in the matching process.
*/
{
  a_symbol_ptr          		sym;
  a_template_arg_ptr    		templ_arg_list = NULL;
  a_template_symbol_supplement_ptr	tssp;
  a_template_param_ptr			templ_param_list;
  a_boolean				result;

#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("matching_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  curr_type = skip_typerefs(curr_type);
  tssp = template_supplement_for_symbol(templ_sym);
  templ_param_list = tssp->cache.decl_info->parameters;
  result = is_match_for_function_template(templ_sym, curr_type,
                                          &templ_arg_list, &sym,
                                          templ_param_list, is_decl_context);
  /* Free any template arguments that may have been created. */
  if (templ_arg_list != NULL) free_template_arg_list(templ_arg_list);
  return result;
}  /* has_matching_template_function */


void record_predeclared_template_function(
                                       a_symbol_ptr         templ_sym,
                                       a_symbol_ptr         rout_sym,
                                       a_template_param_ptr templ_param_list)
/*
rout_sym represents a routine that has already been declared, and templ_sym
represents a function template of the same name.  It may be that rout_sym
is a "predeclared" instance of templ_sym, as in the following example:
  void f(int i) { ... }
  template <class T> void f(T t) { ... }
If so, we want to treat the first f as an instance of the template f.  This
means including a reference to it on the list of function instantiation
entries bound to the template f as well as devising a template argument list
for it.  Check for such a case, and when it occurs create and initialize
the function instantiation entry and set all the pointers.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp = NULL;
  a_template_instance_ptr           tip;
  a_type_ptr                        tp;
  a_template_arg_ptr                templ_arg_list;
  a_boolean			    linkage_mismatch = FALSE;

  db_enter(3, "record_predeclared_template_function");
  tip = rout_sym->variant.routine.instance_ptr;
  if (tip != NULL) {
    /* Symbol is already marked as an instantiation. */
    if (tip->template_sym != templ_sym) {
      /* But it's an instance of some other template -- ignore it. */
    } else {
      tssp = templ_sym->variant.template_info;
    }  /* if */
  } else {
    tp = skip_typerefs(rout_sym->variant.routine.ptr->type);
    if (is_match_for_function_template(templ_sym, tp, &templ_arg_list, &sym,
                                       templ_param_list,
				       /*is_decl_context=*/TRUE)) {
      /* A match has been found. */
#if CHECKING
#if 0
      /* This situation might come up in an error case.  We'll figure out what
         to do about it if it ever happens. */
#endif /* if 0 */
      if (sym != NULL) {
        internal_error("record_predeclared_template_function: sym found");
      }  /* if */
      if (templ_sym->kind != (a_symbol_kind)sk_function_template) {
        internal_error("record_predeclared_template_function: bad sym kind");
      }  /* if */
#endif /* CHECKING */
      /* Create the associated function instantiation entry and link it
         onto the front of the instantiation list for the template. */
      tip = alloc_template_instance();
      tip->template_sym = templ_sym;
      /* Mark this function as a "specialization". */
      tip->specific_decl = TRUE;
      tssp = templ_sym->variant.template_info;
      tip->next = tssp->variant.function.instantiations;
      tssp->variant.function.instantiations = tip;
      /* Make the function instantiation entry and its associated symbol
         point at each other. */
      tip->instance_sym = rout_sym;
      rout_sym->variant.routine.instance_ptr = tip;
      rout_sym->variant.routine.ptr->is_template_function = TRUE;
      rout_sym->variant.routine.ptr->template_arg_list = templ_arg_list;
    }  /* if */
  }  /* if */
  if (tssp != NULL) {
    if (rout_sym->defined) {
      /* User-defined, so no instantiation is required. */
      check_old_specialization_allowed(rout_sym, &rout_sym->decl_position);
      rout_sym->variant.routine.ptr->is_specialized = TRUE;
      rout_sym->variant.routine.ptr->specialized_with_old_syntax = TRUE;
    } else {
      /* Not defined by the user, so still a candidate for instantiation
         based on the template. */
      a_routine_ptr  rp = rout_sym->variant.routine.ptr;
      a_routine_ptr  templ_rp = tssp->variant.function.routine;
      if (templ_rp->storage_class == (a_storage_class)sc_static) {
        if (rp->storage_class != (a_storage_class)sc_static) {
          sym_warning(ec_template_and_instance_linkage_conflict, rout_sym);
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
          linkage_mismatch = TRUE;
        }  /* if */
        if (templ_rp->is_inline) {
          if (rp->called) {
            sym_remark(ec_called_function_redeclared_inline, rout_sym);
          }  /* if */
          rp->is_inline = TRUE;
        }  /* if */
      } else {
        if (rp->storage_class == (a_storage_class)sc_static) {
          sym_warning(ec_template_and_instance_linkage_conflict, rout_sym);
          rp->storage_class = (a_storage_class)sc_unspecified;
          rp->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_cplusplus_external;
          rp->is_inline = FALSE;
          linkage_mismatch = TRUE;
        }  /* if */
      }  /* if */
      if (!linkage_mismatch) {
        if (rp->is_inline && !templ_rp->is_inline) {
          sym_warning(ec_incompatible_inline_specifier_on_specific_decl,
                      rout_sym);
        }  /* if */
      }  /* if */
      /* Function instantiation entries are not marked for actual instantiation
         (that is, for generation of the function body) until there is an
         invocation of the function.  In tim_all mode the instantiations
         will be generated even if the instantiation required flag is not
         set. */
      if (!tip->instantiation_required) {
        /* If the flag is set then the entry is already on the list and
           the flag should not be reset.  If the flag is not already set,
           set it based on whether the routine described by the specific
           declaration has been called or has had its address taken. */
        a_boolean	instantiate;
        instantiate = rp->address_taken || rp->called;
        update_instantiation_required_flag(tip, instantiate,
                                           /*defer_inline=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* record_predeclared_template_function */


void find_member_function_template(a_symbol_ptr  rout_sym,
                                   a_symbol_ptr  corresp_prototype_tag_sym)
/*
rout_sym is a member function of a template class.  corresp_prototype_tag_sym
is a symbol representing the corresponding prototype instantiation.  (For
instance, if rout_sym is a member of A<int>, the corresponding prototype
instantiation is A<T>; if rout_sym is a member of A<int>::B, the corresponding
prototype instantiation is A<T>::B.)  Find the function template symbol that
is a member of the corresponding prototype instantiation and that corresponds
to rout_sym (if the name is overloaded, use the source position to decide),
and create a function instantiation entry to bind the two symbols together.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_instance_ptr           tip;
  a_type_ptr                        tp;
  a_scope_number                    corresp_prototype_decl_scope;
  a_symbol_ptr			    sym_from_prototype = NULL;

  db_enter(3, "find_member_function_template");
  /* In certain error cases, two declarations that are distinct in the
     class template may end up referring to the same function in a
     given instantiation.  For example, the functions
       f(T);
       f(int);
     will result in a duplicate declaration of f(int) when T is int.  An
     error will be diagnosed when this is encountered in the class body.
     If the instance pointer already exists, simply skip this processing. */
  if (rout_sym->variant.routine.instance_ptr != NULL) goto error_exit;
  /* Find a function symbol on the inactive list that is in the scope of the
     prototype instantiation.  It should either be a function template or
     overloaded function symbol. */
  if (is_constructor_symbol(rout_sym)) {
    sym = corresp_prototype_tag_sym->
                         variant.class_struct_union.extra_info->constructor;
  } else if (rout_sym->variant.routine.ptr->special_kind ==
                                    (a_special_function_kind)sfk_conversion) {
    /* Look through the conversion routines of the prototype instantiation.
       The token sequence number associated for the current token is saved
       during the prototype instantiation.  This is used to match this
       declaration with the symbol generated by the prototype instantiation. */
    a_symbol_list_entry_ptr   slep;

    sym = NULL;
    for (slep = corresp_prototype_tag_sym->
                      variant.class_struct_union.extra_info->conversion_list;
         slep != NULL;
         slep = slep->next) {
      a_template_symbol_supplement_ptr	tssp;
      tssp = slep->symbol->variant.routine.instance_ptr->template_info;
      if (tssp->token_sequence_number == curr_token_sequence_number) {
        /* slep->symbol is the template function symbol for rout_sym. */
        sym = slep->symbol;
        break;
      }  /* if */
    }  /* for */
    if (sym == NULL) {
      /* If the conversion operator is for a derived to base conversion,
	 the conversion operator will never be called, and so is not on the
	 conversions list.  This will result in a match not being found in
	 the loop above.  Go through the symbol list associated with the
	 prototype instantiation to find the matching symbol.  This will
	 only occur for unusable derived to base conversions (for which a
	 warning is also issued) so the cost of the extra test should not be
	 significant. */
      for (sym = corresp_prototype_tag_sym->
                               variant.class_struct_union.extra_info->symbols;
           sym != NULL;
	   sym = sym->next_in_scope) {
	if (sym->kind == (a_symbol_kind)sk_member_function) {
	  a_template_symbol_supplement_ptr	tssp;
	  tssp = sym->variant.routine.instance_ptr->template_info;
	  if (tssp->token_sequence_number == curr_token_sequence_number) {
	    break;
	  }  /* if */
	}  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* Get the scope in which the members of the class represented by
       corresp_prototype_tag_sym were declared. */
    tp = type_symbol_type(corresp_prototype_tag_sym);
    corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
    for (sym = rout_sym->header->inactive_symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->decl_scope == corresp_prototype_decl_scope) {
        sym_from_prototype = sym;
        if (sym->kind == (a_symbol_kind)sk_member_function ||
            sym->kind == (a_symbol_kind)sk_overloaded_function) {
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* An overloaded function was found.  Go through the symbols on its list
       and find the function template symbol that corresponds to rout_sym.
       The token sequence number associated for the current token is saved
       during the prototype instantiation.  This is used to match this
       declaration with the symbol generated by the prototype instantiation. */
    for (sym = sym->variant.overloaded_function.symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        a_template_symbol_supplement_ptr	tssp;
        tssp = sym->variant.routine.instance_ptr->template_info;
        if (tssp->token_sequence_number == curr_token_sequence_number) {
          /* sym is the template function symbol for rout_sym. */
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (sym == NULL && sym_from_prototype != NULL) {
    /* If we haven't found a match, see if there is a symbol from the
       prototype instantiation with the same name as the function
       being declared, but that is a static data member or variable
       in the prototype instantiation.  This could occur if a template
       parameter were used as the type of a variable (in the prototype)
       but now that template parameter has a function type.  This is
       not permitted by the standard.   For example:
         template <class T> struct A { 	T t; };
         A<int()> a;
       The same situation can occur if a name inherited from a template
       dependent base class turns out to be a function type. */
    if (sym_from_prototype->kind == (a_symbol_kind)sk_static_data_member ||
        sym_from_prototype->kind == (a_symbol_kind)sk_field) {
      error(ec_function_type_not_allowed);
      goto error_exit;
    }  /* if */
  }  /* if */
#if CHECKING
  if ((sym == NULL || sym->kind != (a_symbol_kind)sk_member_function) &&
      total_errors == 0) {
    internal_error("find_member_function_template: no corresponding template");
  }  /* if */
#endif /* CHECKING */
  if (sym == NULL) {
    /* An error must have occurred previously.  Don't create the template
       instance information in this case. */
    goto error_exit;
  }  /* if */
  /* sym is the template symbol for which member function rout_sym is an
     instantiation.  Create the function instantiation entry and set the
     pointers to bind them together. */
  tip = alloc_template_instance();
  tip->template_sym = sym;
  tssp = sym->variant.routine.instance_ptr->template_info;
  update_befriending_classes_for_function(tssp,
					  rout_sym->variant.routine.ptr);
  /* Link the new entry to the start of the instantiation list of the
     function template. */
  tip->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = tip;
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  tip->instance_sym = rout_sym;
  rout_sym->variant.routine.instance_ptr = tip;
  /* Mark the routine entry as an instance of a member function template. */
  rout_sym->variant.routine.ptr->is_template_function = TRUE;
error_exit:
  db_exit();
}  /* find_member_function_template */


void find_static_data_member_template(a_symbol_ptr  static_data_member_sym,
                                      a_symbol_ptr  corresp_prototype_tag_sym)
/*
static_data_member_sym is a symbol representing a static data member of a
real instantiation of a class template.  corresp_prototype_tag_sym identifies
the nonreal prototype instantiation of the same class template.  Find the
sk_static_data_member symbol from the prototype instantiation (it serves as
the template for the real static data member), and record it in the
template instance entry already associated with static_data_member_sym.
Also, add the instance to the definitions list for the template.
*/
{
  a_scope_number                    corresp_prototype_decl_scope;
  a_type_ptr                        tp, member_type;
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_variable_ptr                    vp;

  db_enter(3, "find_static_data_member_template");
  /* Find a static data member symbol belonging to the prototype instantiation
     and corresponding to static_data_member_sym. */
  tp = type_symbol_type(corresp_prototype_tag_sym);
  member_type = static_data_member_sym->
                        variant.static_data_member.variable->type;
  if (member_type->kind == (a_type_kind)tk_union &&
      is_unnamed_tag_symbol(
                  (a_symbol_ptr)member_type->source_corresp.assoc_info)) {
    /* Error case -- the static data member is an anonymous union.  Look
       through the variables list of the prototype instantiation type. */
    vp = tp->variant.class_struct_union.extra_info->assoc_scope->variables;
    sym = NULL;
    for (; vp != NULL; vp = vp->next) {
      sym = (a_symbol_ptr)vp->source_corresp.assoc_info;
      if (sym != NULL) {
        a_template_symbol_supplement_ptr	tssp;
        tssp = sym->variant.static_data_member.instance_ptr->template_info;
        check_assertion(tssp != NULL);
        if (tssp->token_sequence_number == curr_token_sequence_number) {
          break;
        } else {
          sym = NULL;
        }  /* if */
      }  /* if */
    }  /* for */
  } else {
    /* Normal case -- do the lookup by name. */
    /* Get the scope in which the members of the class represented by
       corresp_prototype_tag_sym were declared. */
    corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
    for (sym = static_data_member_sym->header->inactive_symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->decl_scope == corresp_prototype_decl_scope &&
          sym->kind == (a_symbol_kind)sk_static_data_member &&
          sym->variant.static_data_member.instance_ptr != NULL) {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  check_assertion_str2(sym != NULL || total_errors != 0,
                       "find_static_data_member_template:",
                       "no corresponding template");
  if (sym != NULL) {
    /* sym is the template symbol with which static_data_member_sym is
       associated.  Create a static data member def entry and set the pointers
       to bind them all together. */
    a_template_instance_ptr  tip = alloc_template_instance();
    static_data_member_sym->variant.static_data_member.instance_ptr = tip;
    tip->instance_sym = static_data_member_sym;
    tip->template_sym = sym;
    vp = static_data_member_sym->variant.static_data_member.variable;
    /* Link the new entry to the start of the definition list of the static
       data member template. */
    tssp = sym->variant.static_data_member.instance_ptr->template_info;
    tip->next = tssp->variant.static_data_member.definitions;
    tssp->variant.static_data_member.definitions = tip;
    /* Mark the variable entry as an instance of a static data member
       template. */
    vp->is_template_static_data_member = TRUE;
  }  /* if */
  db_exit();
}  /* find_static_data_member_template */


a_symbol_ptr find_template_function(a_symbol_ptr        templ_sym,
                                    a_template_arg_ptr  *new_list,
                                    a_source_position   *source_pos)
/*
templ_sym is a pointer to a symbol representing a function template and
*new_list is a pointer to a linked list of template arg entries.  If
*new_list is equivalent to the template arg list of a previous instantiation,
return the symbol representing the latter and put the entries on *new_list
back onto the available list.  Otherwise, create a new function instantiation
entry, symbol, routine entry, etc., and return the new symbol; in that
case *new_list is not disposed of but rather used in the resulting data
structure.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_instance_ptr           tip, prev_tip;
  a_template_arg_ptr		    tap = *new_list;

  db_enter(3, "find_template_function");
  /* Make a pass over the entries representing instantiations of the function
     template. */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    check_assertion(templ_sym->kind == (a_symbol_kind)sk_function_template);
    tssp = templ_sym->variant.template_info;
  }  /* if */
  /* Check for invalid type arguments.  Local types may not be used as
     arguments nor may unnamed types.  Issue an error if any are found. */
  while (tap != NULL) {
    if (tap->is_type) {
      a_type_ptr	type = tap->variant.type;
      a_boolean		is_unnamed;
      a_boolean		is_local;
      if (is_or_contains_unnamed_or_local_type(type, &is_unnamed, &is_local)) {
        if (is_local) {
          pos_error(ec_local_type_in_template_arg, source_pos);
        } else if (is_unnamed) {
          pos_error(ec_unnamed_type_in_template_arg, source_pos);
        }  /* if */
      }  /* if */
    }  /* if */
    tap = tap->next;
  }  /* while */
  tip = tssp->variant.function.instantiations;
  prev_tip = NULL;
  for (; tip != NULL; tip = tip->next) {
    a_template_arg_ptr	arg_list;
    arg_list = tip->instance_sym->variant.routine.ptr->template_arg_list;
    if (equiv_template_arg_lists(arg_list, *new_list,
                                 /*error_matches_anything=*/FALSE,
                                 /*is_nonreal_member=*/FALSE)) {
      /* We've found a match.  Remove the found function instantiation entry
         from its current position in the instantiation list and add it to
         the front. */
      if (prev_tip != NULL) {
        prev_tip->next = tip->next;
        tip->next = tssp->variant.function.instantiations;
        tssp->variant.function.instantiations = tip;
      }
      sym = tip->instance_sym;
#if DEBUG
      if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
      break;
    }  /* if */
    prev_tip = tip;
  }  /* for */
  if (tip == NULL) {
    /* No match was found, so create a new template function.  That means
       create a symbol entry, a routine entry, a routine type entry, and a
       function instantiation entry, and linking all these appropriately.
       Note that the symbol will not be added to the symbol table, since it
       is accessed through the list of function instantiation entries. */
    sym = make_template_function(templ_sym, *new_list);
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "created: ", 2);
      db_symbol(templ_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
#if RECORD_HIDDEN_NAMES_IN_IL
    check_for_defeatable_name_hiding(sym);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  } else {
    /* We are reusing a template function that already exists, so *new_list
       will not be used.  Return it to the available list for reuse. */
    free_template_arg_list(*new_list);
  }  /* if */
  /* The list is cleared in all cases.  The caller cannot use the list
     after we return because it may have been freed. */
  *new_list = NULL;
  db_exit();
  return sym;
}  /* find_template_function */


static a_template_nesting_depth nesting_depth_of_template_param
                                                   (a_template_param_ptr tpp)
/*
Return the template nesting depth of the specified template parameter.
*/
{
  a_template_nesting_depth	depth;

  if (tpp == NULL) {
    /* This is an error case -- use a depth of zero. */
    depth = 0;
  } else if (tpp->param_symbol->kind == (a_symbol_kind)sk_type) {
    depth = tpp->variant.type->
                         variant.template_param.extra_info->coordinates.depth;
  } else {
    depth = tpp->variant.constant.ptr->
                 variant.template_param.variant.coordinates.depth;
  }  /* if */
  return depth;
}  /* nesting_depth_of_template_param */


static
a_boolean check_template_param_nesting_depths(a_template_param_ptr param_list,
                                              a_symbol_ptr	   class_sym)
/*
Check the template parameter list pointed to by param_list with the
parameter list of the class template pointed to by class_sym and make
sure that they are at the same nesting depth.  Return TRUE if they are.
*/
{
  a_template_param_ptr		class_tpp;

  class_tpp = class_sym->variant.template_info->cache.decl_info->parameters;
  return nesting_depth_of_template_param(param_list) ==
                                  nesting_depth_of_template_param(class_tpp);
}  /* check_template_param_nesting_depths */


static a_boolean reconcile_template_param_lists
					(a_template_param_ptr param_list,
                                         a_symbol_ptr         class_sym,
					 a_source_position    *error_pos)
/*
Compare the template parameter list of the template declaration currently
being scanned with the template parameter list of a previous declaration
of the same class.  Make sure that the parameter lists match and
merge the default argument information from the two lists.  The default
argument information is updated into both lists because we don't know
which version will be used as the "primary" argument list.  This routine
is called for each redeclaration of a template argument list for a class.
For example, this routine will be called for all of these declarations
except for the first one:

	template <class T, int I> class A;
	template <class T, int I> class A { ... };
	template <class T, int I> void A<T,I>::f() { ... };
	template <class T, int I> int A<T,I>::i =  ... ;

Return TRUE if the parameter lists are compatible.  Otherwise, return FALSE.
*/
{
  a_template_param_ptr	new_tpp;
  a_template_param_ptr	old_tpp;
  a_template_param_ptr	prev_new_tpp = NULL;
  a_boolean		any_errors = FALSE;

  new_tpp = param_list;
  old_tpp = class_sym->variant.template_info->cache.decl_info->parameters;
  while (new_tpp != NULL && old_tpp != NULL) {
    a_symbol_ptr	old_sym = old_tpp->param_symbol;
    a_symbol_ptr	new_sym = new_tpp->param_symbol;
    a_boolean		err = FALSE;
    if (old_sym->kind != new_sym->kind) {
      /* One argument is a type and the other is a constant -- this is an
         error. */
      err = TRUE;
    } else if (old_sym->kind == (a_symbol_kind)sk_type) {
      /* Both are types.  Make sure the types match. */
      a_template_param_type_supplement_ptr old_tptsp;
      a_type_ptr        old_type = old_tpp->variant.type;
      a_type_ptr        new_type = new_tpp->variant.type;
      err = !identical_types(old_type, new_type);
      /* Update both type entries to point to the same description entry. */
      old_tptsp = old_type->variant.template_param.extra_info;
      old_type->variant.template_param.extra_info = old_tptsp;
      new_type->variant.template_param.extra_info = old_tptsp;
    } else {
      /* Both are constants.  Make sure the values are the same. */
      check_assertion(old_sym->kind == (a_symbol_kind)sk_constant);
      err = !eq_constants(old_tpp->variant.constant.ptr,
                          new_tpp->variant.constant.ptr);
    }  /* if */
    if (err) {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &new_sym->decl_position, old_sym);
      any_errors = TRUE;
    }  /* if */
    old_tpp = old_tpp->next;
    prev_new_tpp = new_tpp;
    new_tpp = new_tpp->next;
  }  /* while */
  if (old_tpp != NULL || new_tpp != NULL) {
    /* The number of template parameters does not match the previous
       declaration. */
    an_error_code	error_code;
    a_source_position	*pos;
    if (old_tpp == NULL) {
      /* Too many parameters.  Use the position of the first extra
         parameter as the error position. */
      error_code = ec_too_many_template_params;
      pos = &new_tpp->param_symbol->decl_position;
    } else {
      /* Too few parameters.  Use the position of the last parameter present
         to report the error.  If there were no parameters specified,
         use the position supplied by the caller, which will point to
         the thing being declared. */
      error_code = ec_too_few_template_params;
      pos = prev_new_tpp == NULL ? error_pos :
                                   &prev_new_tpp->param_symbol->decl_position;
    }  /* if */
    pos_error(error_code, pos);
    any_errors = TRUE;
  }  /* if */
  /* Merge the default argument information from the two parameter lists.
     This is only done if there were no errors in the previous tests so
     we know that the parameter lists match. */
  if (!any_errors) {
    new_tpp = param_list;
    old_tpp = class_sym->variant.template_info->cache.decl_info->parameters;
    while (new_tpp != NULL && old_tpp != NULL) {
      a_boolean def_arg_involves_template_param;
      a_boolean old_has_default;
      a_boolean new_has_default;
      old_has_default = old_tpp->has_default_arg;
      new_has_default = new_tpp->has_default_arg;
      if (old_has_default && new_has_default) {
        /* This parameter already has a default argument. */
        pos_error(ec_default_arg_already_defined, &
                  new_tpp->param_symbol->decl_position);
      } else if (old_has_default || new_has_default) {
        a_template_param_ptr	from_tpp;
        a_template_param_ptr	to_tpp;
        /* Copy the default information into the other parameter.  We
           end up with two argument lists with complete parameter
           information. This is done because we don't know which parameter
           list is going to end up being the one actually used. */
        if (old_has_default) {
          from_tpp = old_tpp;
          to_tpp = new_tpp;
        } else {
          from_tpp = new_tpp;
          to_tpp = old_tpp;
        }  /* if */
        to_tpp->has_default_arg = TRUE;
        def_arg_involves_template_param =
                                    from_tpp->def_arg_involves_template_param;
        to_tpp->def_arg_involves_template_param =
                                              def_arg_involves_template_param;
        if (new_tpp->param_symbol->kind == (a_symbol_kind)sk_constant) {
          to_tpp->variant.constant.type_involves_template_param =
                      from_tpp->variant.constant.type_involves_template_param;
        }  /* if */
        if (def_arg_involves_template_param) {
          to_tpp->default_arg.cache = from_tpp->default_arg.cache;
        } else {
          if (new_tpp->param_symbol->kind == (a_symbol_kind)sk_constant) {
            to_tpp->default_arg.constant = from_tpp->default_arg.constant;
          } else {
            to_tpp->default_arg.type = from_tpp->default_arg.type;
          }  /* if */
        }  /* if */
      }  /* if */
      old_tpp = old_tpp->next;
      new_tpp = new_tpp->next;
    }  /* while */
  }  /* if */
  return !any_errors;
}  /* reconcile_template_param_lists */


static a_boolean member_template_param_list_matches_class
				(a_template_decl_info_ptr start_decl_info,
                                 a_symbol_ptr             member_sym,
				 a_source_position        *error_pos)
/*
This routine is called for template declarations of members of classes.
It calls reconcile_template_param_lists to compare the template parameters
of this declaration with the parameter list of the class declaration.
If this is a member template, the template parameter lists at each level
are compared.  Return TRUE if the parameter lists are compatible.
Otherwise, return FALSE.
*/
{
  a_type_ptr    		type;
  a_boolean			any_mismatches = FALSE;
  a_template_decl_info_ptr	decl_info;

  /* If this declaration is for a member template, skip out to the
     next enclosing template parameter list because this routine is
     only used for comparing the class template parameter lists of the
     enclosing classes. */
  if (member_sym->kind == (a_symbol_kind)sk_function_template ||
      member_sym->kind == (a_symbol_kind)sk_class_template) {
    start_decl_info = start_decl_info->enclosing_template_decl;
  }  /* if */
  type = member_sym->parent.class_type;
  decl_info = start_decl_info;
  /* Loop as long as we have a decl_info or a parent type.  The loop
     is terminated when both no longer represent templates (as happens
     when processing a specialization) or when a mismatch has been found. */
  for (;;) {
    a_symbol_ptr	class_sym;
    a_symbol_ptr	template_sym = NULL;
    /* Find the nearest enclosing class template (class with a template
       argument list. */
    while (type != NULL && type->source_corresp.is_class_member &&
           type->variant.class_struct_union.extra_info->
                                                  template_arg_list == NULL) {
      type = type->source_corresp.parent.class_type;
    }  /* while */
    if (type == NULL) {
      /* The enclosing class is not a class template.  Okay as long as
         there is also no template declaration information. */
    } else {
      /* Get the symbol associated with the type.  This symbol is the
         template class symbol. */
      class_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
      if (is_prototype_instantiation_symbol(class_sym)) {
        /* Get a pointer to the symbol for the class template. */
        template_sym =
             class_sym->variant.class_struct_union.extra_info->class_template;
      }  /* if */
    }  /* if */
    /* Make sure that the template nesting depth of this parameter list
       matches that of the original declaration.  There is no sense checking
       each of the parameters if the lists are at different levels. */
    if (template_sym == NULL && decl_info == NULL) {
      /* Neither a class template symbol or any declaration information.
         This is okay, the enclosing class is a normal class. */
      break;
    } else if (decl_info == NULL || template_sym == NULL ||
               !check_template_param_nesting_depths(decl_info->parameters,
                                                    template_sym)) {
      pos_sy_error(ec_template_depth_mismatch, error_pos, member_sym);
      any_mismatches = TRUE;
      break;
    }  /* if */
    if (!reconcile_template_param_lists(decl_info->parameters,
                                        template_sym, error_pos)) {
      any_mismatches = TRUE;
    }  /* if */
    /* Skip out to the enclosing class type. */
    type = type->source_corresp.is_class_member ?
                               type->source_corresp.parent.class_type : NULL;
    if (decl_info != NULL) decl_info = decl_info->enclosing_template_decl;
  }  /* for */
  return !any_mismatches;
}  /* member_template_param_list_matches_class */


static void check_template_param_default_args(a_template_param_ptr param_list)
/*
Make sure that any default arguments are at the end of the parameter list.
*/
{
  a_template_param_ptr	tpp;
  a_boolean		any_defaults = FALSE;
  a_template_param_ptr	last_tpp_with_default = NULL;

  tpp = param_list;
  while (tpp != NULL) {
    a_boolean has_default = FALSE;
    /* Does this parameter have a default argument? */
    has_default = tpp->has_default_arg;
    if (has_default) last_tpp_with_default = tpp;
    any_defaults |= has_default;
    /* If there have been parameters with default and this one doesn't have
       a default then issue an error and exit the loop. */
    if (any_defaults && !has_default) {
      pos_error(ec_default_arg_not_at_end,
                &last_tpp_with_default->param_symbol->decl_position);
    }  /* if */
    tpp = tpp->next;
  }  /* while */
}  /* check_template_param_default_args */


static void add_befriending_class_to_class_template
                      (a_template_symbol_supplement_ptr     tssp,
		       a_type_ptr                           class_declared_in)
/*
Indicate that the template designated by tssp is a friend of the class
specified by class_declared_in.  If any instances of the template have already
been instantiated, update the befriending information for the instances.
*/
{
  a_class_list_entry_ptr  clep;
  a_symbol_ptr            instance_sym;

  clep = alloc_list_entry_for_class();
  clep->next = tssp->befriending_classes;
  clep->class_type = class_declared_in;
  tssp->befriending_classes = clep;
  /* Update any instances that have already been created. */
  for (instance_sym = tssp->variant.class_template.instantiations;
       instance_sym != NULL; instance_sym = instance_sym->next) {
    a_type_ptr  tp = instance_sym->variant.class_struct_union.type;
    if (is_real_class_symbol(instance_sym)) {
      /* Don't do this for the nonreal class types. */
      if (class_declared_in != tp) {
         /* Don't declare the current class as a friend. */
        decl_friend_class(class_declared_in, tp);
      }  /* if */
    }  /* if */
  }  /* for */
  if (tssp->subordinate_templates != NULL) {
    /* This is a member class template declared in another class template.
       We need to visit the template symbols for this template in each
       of the instantiations of the enclosing class template and update
       the befriending information for the instantiations of those
       templates. */
    a_symbol_list_entry_ptr	slep;
    for (slep = tssp->subordinate_templates; slep != NULL; slep = slep->next) {
      a_symbol_ptr			subordinate_sym;
      a_template_symbol_supplement_ptr	subordinate_tssp;
      subordinate_sym = slep->symbol;
      subordinate_tssp = template_supplement_for_symbol(subordinate_sym);
      add_befriending_class_to_class_template(subordinate_tssp,
                                              class_declared_in);
    }  /* for */
  }  /* if */
}  /* add_befriending_class_to_class_template */


static a_template_symbol_supplement_ptr
              make_nested_class_template_supplement(a_symbol_ptr sym,
                                                    a_type_kind	 type_kind)
/*
Given a class symbol for a nested class within a class template,
create the template symbol supplement for the class and return the
pointer to the newly created template symbol supplement.
*/
{
  a_template_symbol_supplement_ptr	tssp = NULL;
  a_type_ptr				parent_type = sym->parent.class_type;
  a_class_symbol_supplement_ptr		parent_cssp;
  a_template_symbol_supplement_ptr	parent_tssp = NULL;
  a_class_symbol_supplement_ptr		cssp;

  check_assertion(is_class_struct_union_symbol(sym));
  check_assertion(sym->is_class_member);
  /* The symbol must be for a nested class.  Make sure that the parent class
     is a prototype instantiation.  Note that the immediate parent can
     be used, we don't have to go all the way out to the outermost class
     because all enclosing classes must have been defined before the
     nested class can be defined. */
  parent_cssp = symbol_supplement_for_class(parent_type);
  parent_tssp = parent_cssp->template_info;
  cssp = sym->variant.class_struct_union.extra_info;
  check_assertion(parent_cssp->is_prototype_instantiation);
  cssp->is_prototype_instantiation = TRUE;
  cssp->is_nonreal_class = parent_cssp->is_nonreal_class;
  /* During the prototype instantiation save the token sequence number
     associated with this position in the class symbol supplement
     this will be used during real instantiations to determine which
     declaration in the real instantiation matches this one. */
  cssp->prototype_token_sequence_number = curr_token_sequence_number;
  tssp = alloc_template_symbol_supplement(sym->kind);
  tssp->variant.class_template.name_linkage =
                           parent_tssp->variant.class_template.name_linkage;
  tssp->variant.class_template.type_kind = type_kind;
  cssp->template_info = tssp;
  return tssp;
}  /* make_nested_class_template_supplement */


void set_nested_template_class_symbol_info(a_symbol_ptr  sym,
                                           a_type_kind	 type_kind)
/*
sym is the symbol for a nested class within a template class.  Update
the symbol supplement for sym to contain the necessary template information
so that the nested class can be instantiated.  This routine is called for
both prototype instantiations and real instantiations.  For prototype
instantiations, it creates the template symbol supplement for the nested
class.  For real instantiations, it establishes the correspondence with
the prototype instantiation and updates the friend information for
any classes that declared the nested class as a template friend.
*/
{
  a_symbol_ptr	ct_symbol;
  a_scope_stack_entry_ptr	ssep;

  ssep = &scope_stack[depth_innermost_instantiation_scope];
  if (sym->is_class_member) {
    if (!ssep->in_prototype_instantiation) {
      /* Look for the prototype symbol that corresponds to this nested class
         symbol. */
      ct_symbol = find_corresp_prototype_tag_sym(sym);
      if (ct_symbol != NULL) {
        /* Set the pointer that points back to the original class template
           symbol. */
        a_class_symbol_supplement_ptr		cssp;
        a_type_ptr				class_type;
        a_template_symbol_supplement_ptr	tssp;
        cssp = sym->variant.class_struct_union.extra_info;
        cssp->corresp_prototype_sym = ct_symbol;
        class_type = sym->variant.class_struct_union.type;
        cssp->is_instance = TRUE;
        tssp = template_supplement_for_symbol(ct_symbol);
        /* Update the friend information associated with this template.
           These are the classes that declared this template as a friend. */
        update_befriending_classes_for_class(tssp, class_type);
      } /* if */
    } else {
      /* A nested class within a prototype instantiation.  Create the
         template symbol supplement for this class. */
      (void)make_nested_class_template_supplement(sym, type_kind);
    }  /* if */
  }  /* if */
}  /* set_nested_template_class_symbol_info */


static
void record_specialization(a_decl_state_ptr			decl_state,
                           a_symbol_ptr				template_sym,
   		           a_template_symbol_supplement_ptr	tssp)
/*
Update the template specified by template_sym to indicate that it is
now specialized.  Make sure that no instantiations have already been
generated. 
*/
{
  if (tssp->is_specific_definition) {
    /* This is already marked as a specific definition.  Don't repeat
       the test of referenced entities. */
  } else {
    /* Note that the prototype_template is not set to NULL. */
    tssp->is_specific_definition = TRUE;
    /* Check for any existing instantiations.  A specialization must be
       declared before it is used. */
    if (template_sym->kind == (a_symbol_kind)sk_function_template) {
      a_template_instance_ptr	tip;
      for (tip = tssp->variant.function.instantiations; tip != NULL;
           tip = tip->next) {
        pos_sy2_error(ec_specialization_of_referenced_template,
                      &decl_state->start_pos, template_sym, tip->instance_sym);
      }  /* for */
    } else {
     a_symbol_ptr	sym;
      check_assertion(template_sym->kind == (a_symbol_kind)sk_class_template);
      for (sym = tssp->variant.class_template.instantiations; sym != NULL;
           sym = sym->next) {
        /* It is only an error if the class type is complete and is not a
           itself a specialization. */
        if (is_complete_class_struct_union_type(type_symbol_type(sym)) &&
            !is_template_instance_specific_def_symbol(sym)) {
          pos_sy2_error(ec_specialization_of_referenced_template,
                        &decl_state->start_pos, template_sym, sym);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* specialization_permitted */


static
a_boolean same_name_as_template_param(
                        a_template_decl_info_ptr template_decl_info,
                        a_symbol_locator	 *locator)
/*
See if the class being declared has the same name as one of its
template parameters.  Is so, issue an error.  Return TRUE if an
error was diagnosed.
*/
{
  a_template_param_ptr		tpp;
  a_boolean			err = FALSE;

  /* Compare the symbol header of the class name with each of the template
     parameters. */
  for (tpp = template_decl_info->parameters; tpp != NULL; tpp = tpp->next) {
    if (tpp->param_symbol->header == locator->symbol_header) {
      err = TRUE;
      break;
    }  /* if */
  }  /* for */
  if (err) {
    pos_error(ec_class_template_same_name_as_templ_param,
              &locator->source_position);
  }  /* if */
  return err;
}  /* same_name_as_template_param */


static void class_template_declaration(
                         a_decl_state_ptr      decl_state,
		         a_symbol_ptr          *p_sym_ptr,
		         a_boolean             *resolution,
   		         a_type_ptr            *new_type)

/*
The beginning of a template declaration or definition has been scanned,
e.g.,

  template <class T> struct A { ... };
                    ^current position is here

and this declaration has been prescanned to determine that it is,
in fact, a class template declaration and not a function declaration
that begins with an elaborated type specifier.  Scan the class template and
set *p_sym_ptr to the class template symbol.  If a class template had been
declared previously but not defined, and this is a defining declaration,
return *resolution TRUE.  In addition, if this is a defining declaration,
cache all the tokens that make up the declaration and do a prototype
instantiation.
*/
{
  a_boolean                         suppress_redecl_error = FALSE;
  a_boolean                         is_definition, is_redecl = FALSE;
  a_symbol_locator                  locator;
  a_symbol_ptr                      sym = NULL, prototype_sym, param_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     local_token_cache;
  a_type_kind                       type_kind;
  a_type_ptr                        prototype_type = NULL;
  a_template_arg_ptr                tap, *append_addr;
  a_template_param_ptr              tpp;
  a_boolean			    err;
  a_token_set_array                 stop_tokens;
  a_source_position                 friend_pos;
  a_boolean			    is_nested_class_definition = FALSE;
  a_template_param_ptr		    templ_params =
                                             decl_state->decl_info->parameters;
  a_token_cache_ptr		    definition_token_cache = NULL;
  a_token_kind			    next_tok;

  db_enter(3, "class_template_declaration");
  if (curr_token == tok_typedef || curr_token == tok_auto ||
      curr_token == tok_register) {
    error(ec_bad_storage_class_on_template_decl);
    (void)get_token();
  }  /* if */
  if (curr_token == tok_friend) {
    /* The is_template_friend flag should already be set.  The exception
       is an error case in which "friend" appears outside of a class. */
    check_assertion(!decl_state->is_member_decl ||
                    decl_state->is_template_friend);
    /* Set it, just in case is wasn't already set because of use outside
       of a class.  This permits the error to be diagnosed below. */
    decl_state->is_template_friend = TRUE;
    friend_pos = pos_curr_token;
    (void)get_token();
  }  /* if */
  switch (curr_token) {
    case tok_class:  type_kind = (a_type_kind)tk_class;  break;
    case tok_struct: type_kind = (a_type_kind)tk_struct; break;
    case tok_union:  type_kind = (a_type_kind)tk_union;  break;
    default:	     unexpected_condition();
  }  /* switch */
  /* Bypass "class", "struct", or "union". */
  (void)get_token();
  /* Next should be the class name. */
  if (!is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                       GID_USE_PROTOTYPE_NOT_NONREAL)) {
    /* Not an identifier. */
    error(ec_exp_identifier);
    set_to_error_locator(locator);
    /* Probably a missing class name -- use the current token as the next
       token for lookahead purposes. */
    next_tok = curr_token;
  } else {
    /* Look up the identifier.  If it's a qualified name there will be an
       error down the line.  The options used when coalescing the 
       identifier are specified above. */
#if 0
    /* For member templates, is simplify_curr_class_qualified_name needed? */
#endif /* 0 */
    /* For friend declarations, or declarations in which the template name
       is a qualified name, do a normal lookup.  For unqualified
       references that are not in friend declarations, just look
       in the current scope. */
    if (decl_state->is_template_friend ||
        locator_for_curr_id.is_qualified_name) {
      sym = coalesce_and_lookup_generalized_identifier
                             (GID_CLASS_TEMPLATE_REQUIRED, ilm_linkage, &err);
    } else {
      /* Look up the symbol in the current scope.  To do this we must
         temporarily change the decl. scope level to the effective
         level for this declaration because decl_scope_level currently
         points to the template declaration scope. */
      a_scope_depth	saved_decl_scope_level = decl_scope_level;
      decl_scope_level = decl_state->effective_decl_level;
      sym = curr_scope_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
      decl_scope_level = saved_decl_scope_level;
    }  /* if */
    locator = locator_for_curr_id;
    next_tok = next_token();
  }  /* if */
  is_definition = (next_tok == tok_colon || next_tok == tok_lbrace);
  if (is_definition && locator_for_curr_id.is_qualified_name &&
      any_deferred_access_checks()) {
    /* When defining a class member outside of its class definition
       using a qualified name, any access errors that may have been
       detected when scanning the qualified name should be suppressed.
       This context is not really a declarator, but the concept is
       the same as suppressing access errors when scanning the declarator
       of a member function or static data member. */
    discard_declarator_access_errors();
  }  /* if */
  /* If we didn't report a missing identifier above, skip over the identifier
     token now. */
  if (curr_token == tok_identifier) (void)get_token();
  /* Make sure this declaration is valid in this scope. */
  if (decl_state->is_template_friend) {
    if (decl_state->class_declared_in != NULL) {
      /* A friend declaration in a class scope -- okay (provided it is not
         a definition). */
      if (decl_state->is_template_friend && is_definition) {
        /* Classes cannot be defined in friend declarations. */
        pos_error(ec_template_friend_definition_not_allowed,
                     &locator.source_position);
        decl_state->decl_scope_err = TRUE;
      }  /* if */
      /* Adjust the effective declaration level.  Friend declarations
         are added to the nearest enclosing namespace scope. */
      decl_state->effective_decl_level = depth_innermost_namespace_scope;
    } else {
      /* A friend declaration in a nonclass scope. */
      pos_error(ec_bad_specifier_outside_class_decl, &friend_pos);
      decl_state->decl_scope_err = TRUE;
    }  /* if */
  }  /* if */
  if (sym != NULL && !decl_state->decl_scope_err) {
    /* Make sure the symbol found is a class template symbol or a class
       symbol.  If the class symbol is not a member of a class template,
       that error will be diagnosed later.  Issue an error if this is
       a qualified name.  If it is not a qualified name, clear the symbol
       and let a redeclaration error be reported later. */ 
    if (sym->kind != (a_symbol_kind)sk_class_template &&
        !is_class_struct_union_symbol(sym)) {
      if (locator.is_qualified_name) {
        pos_sy_error(ec_sym_not_a_class_template, &locator.source_position,
                     sym);
        decl_state->decl_scope_err = TRUE;
      }  /* if */
      sym = NULL;
    }  /* if */
  }  /* if */
  if (!locator.is_qualified_name && !decl_state->decl_scope_err &&
      !decl_state->is_template_friend &&
      decl_state->number_of_template_param_clauses > 1) {
    /* This is a declaration of class template that is not a friend, but
       it has multiple template parameter lists.  This is an error
       except for member declarations done outside of the class.  We
       know this is not one of those, the identifier is not a qualified
       name. */
    pos_error(ec_multiple_template_decls_not_allowed,
              &locator.source_position);
    decl_state->decl_scope_err = TRUE;
  }  /* if */
  if (decl_state->decl_scope_err) {
    /* An error has already been issued on a template declaration that
       is not at file scope. */
    set_to_named_error_locator(locator);
    sym = NULL;
  }  /* if */
  /* Determine whether this is a definition of a class nested within
     a class template. */
  if (sym != NULL) {
    /* A definition of a class nested within a template can only appear in
       a template declaration when it is defined later outside of the class. */
    tssp = template_supplement_for_symbol(sym);
    is_nested_class_definition = is_class_struct_union_symbol(sym) &&
                                 sym->is_class_member &&
                                 (!decl_state->is_member_decl ||
                                  decl_state->is_template_friend) &&
                                 tssp != NULL;
  }  /* if */
  /* See if the class being declared has the same name as one of its
     template parameters. */
  if (same_name_as_template_param(decl_state->decl_info, &locator)) {
    sym = NULL;
    suppress_redecl_error = TRUE;
  }  /* if */
  if (!locator.is_qualified_name && sym != NULL) {
    /* Unless this is a friend declaration, an unqualified name must refer
       to a name from the current scope. */
    check_assertion(sym->is_error || decl_state->is_template_friend ||
                    sym->decl_scope ==
                         scope_stack[decl_state->effective_decl_level].number);
  } else if (locator.is_qualified_name && sym != NULL) {
#if 0
    /* Class qualified name checks need to be added here for member
       templates. */
#endif /* 0 */
    a_scope_stack_entry_ptr	ssep =
                                &scope_stack[decl_state->effective_decl_level];
    a_namespace_ptr		nsp;
    a_namespace_ptr		curr_nsp;
    a_boolean			err = FALSE;
    /* Get the namespace that is currently being defined. */
    curr_nsp = scope_stack[depth_innermost_namespace_scope].assoc_namespace;
    nsp = parent_namespace_for_symbol(sym);
    if (!locator.is_class_member && nsp == curr_nsp && nsp != NULL) {
      /* The namespace is the same as the one currently being defined.
         This is an error. */
      pos_error(ec_qualifier_in_namespace_member_decl,
                &locator.source_position);
      err = TRUE;
    } else if (!is_definition) {
      /* A declaration using a qualified name.  This is only allowed if it
         is a friend declaration. */
      if (!decl_state->is_template_friend) {
        pos_sy_error(ec_bad_scope_for_redeclaration,
                     &locator.source_position, sym);
        err = TRUE;
      }  /* if */
    } else if (decl_state->class_declared_in != NULL) {
      /* A definition using a qualified name in a class scope.  This is
         not allowed. */
      pos_error(ec_qualifier_in_member_declaration, &locator.source_position);
      err = TRUE;
    } else if (!namespace_is_enclosed_by_scope(sym, ssep)) {
      /* This definition appears within a namespace scope in which the name
         cannot be defined -- it is a member (directly or indirectly) of a
         namespace that is not enclosed by the current namespace scope
         (see WP 7.3.1.4). */
      pos_sy_error(ec_bad_scope_for_definition,
                   &locator.source_position, sym);
      err = TRUE;
    }  /* if */
    if (err) {
      sym = NULL;
      suppress_redecl_error = TRUE;
      set_to_named_error_locator(locator);
    }  /* if */
  }  /* if */
  {
    a_boolean			err = FALSE;
    if (templ_params == NULL) {
      /* Don't create a real symbol no template parameter list was provided. */
      err = TRUE;
    } else if (sym == NULL) {
      /* Suppress the following error tests if the no symbol was found. */
    } else if ((sym->kind == (a_symbol_kind)sk_class_template ||
        is_nested_class_definition)) {
      /* This is a class template or a nested class within a class
         template. */
      is_redecl = TRUE;
      if ((type_kind == (a_type_kind)tk_union) !=
          (tssp->variant.class_template.type_kind ==
                                                  (a_type_kind)tk_union)) {
        /* Cannot mix union and nonunion declarations. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator.source_position, sym);
        err = TRUE;
      } else if (!sym->defined) {
        /* Not previously defined. */
        *resolution = is_definition;
        if (!is_definition && sym->is_class_member &&
            !decl_state->is_template_friend) {
          /* Redeclaration of a class member is not allowed. */
          pos_sy_error(ec_bad_scope_for_redeclaration,
	                 &locator.source_position, sym);
        }  /* if */
      } else if (is_definition) {
        /* Attempting to redefine a class template. */
        pos_sy_error(ec_already_defined, &locator.source_position, sym);
        err = TRUE;
      }  /* if */
      if ((is_definition || is_redecl) && sym != NULL) {
        /* Either a definition or a redeclaration.  Make sure the template
           parameters are compatible with the previous declaration. */
        if (sym->is_class_member &&
           (decl_state->class_declared_in == NULL ||
            decl_state->is_template_friend)) {
          /* If this is a class member defined outside of its class or a friend
             function declaration in a class.  Make sure that the template
             parameters match those of the original class definition. */
          if (!member_template_param_list_matches_class(decl_state->decl_info,
                                                        sym,
                                                        &error_position)) {
            err = TRUE;
          } /* if */
        }  /* if */
        if (!err && sym->kind == (a_symbol_kind)sk_class_template) {
          /* If this is a class template, make sure the template parameters
             match a previous declaration of the class. */
          if (!reconcile_template_param_lists(templ_params, sym,
                                              &locator.source_position)) {
            err = TRUE;
          }  /* if */
        } /* if */
      }  /* if */
    } else if (locator.is_qualified_name) {
      /* A qualified name that does not refer to a class template
         symbol.  Issue an error and set the locator to an error locator. */
      if (is_template_class_symbol(sym) && locator.is_template_id &&
          !is_real_class_symbol(sym)) {
        /* The class name was followed by a template parameter list in a
           later definition.  This is not permitted. */
        pos_sy_error(ec_templ_param_list_not_allowed, &locator.source_position,
                     sym);
      } else {
        /* Some other kind of invalid symbol. */
        pos_sy_error(ec_sym_not_a_class_template, &locator.source_position,
                     sym);
      }  /* if */
      err = TRUE;
      set_to_named_error_locator(locator);
      sym = NULL;
    } else {
      /* Force the call to enter symbol, which will report the name clash. */
      sym = NULL;
    }  /* if */
    if (err) {
      sym = NULL;
      suppress_redecl_error = TRUE;
      set_to_named_error_locator(locator);
    }  /* if */
  }
  /* Create the symbol entry for this template. */
  /* Make sure that the default arguments for the template parameters
     are valid (i.e., that they are at the end of the parameter list).
     This is done now because we have to wait until the parameter lists
     have been merged to do the test. */
  check_template_param_default_args(templ_params);
  if (sym == NULL) {
    /* Enter the symbol at the scope indicated by effective_decl_level. */
    a_scope_stack_entry_ptr	ssep =
                                &scope_stack[decl_state->effective_decl_level];
    sym = enter_symbol((a_symbol_kind)sk_class_template, &locator,
                       decl_state->effective_decl_level,
                       suppress_redecl_error);
    tssp = sym->variant.template_info;
    if (ssep->kind == (a_scope_kind)sck_namespace ||
        ssep->kind == (a_scope_kind)sck_namespace_extension) {
      set_namespace_membership(sym, (a_source_correspondence *)NULL,
                               ssep->il_scope->variant.assoc_namespace);
    } else if (ssep->kind == (a_scope_kind)sck_class_struct_union) {
      set_class_membership(sym, (a_source_correspondence *)NULL,
                           decl_state->class_declared_in);
      tssp->variant.class_template.access = decl_state->access; 
    }  /* if */
    /* Set the name-linkage for this template -- it will be propagated
       into the instances. */
    if (ssep->within_unnamed_namespace ||
        instantiation_mode == tim_local) {
      /* Templates declared inside an unnamed namespace have internal
         linkage -- as do all templates in "local instantiation mode". */
      tssp->variant.class_template.name_linkage =
                                       (a_name_linkage_kind)nlk_internal;
    } else {
      /* Normally, a template has C++ linkage. */
      tssp->variant.class_template.name_linkage =
                              (a_name_linkage_kind)nlk_cplusplus_external;
    }  /* if */
    is_redecl = FALSE;
  }  /* if */
  if (is_definition || !is_redecl) {
    /* Either this is the first declaration of the template class or a
	defining redeclaration. */

    /* Save the type kind (corresponding to the class/struct/union token)
       in the class template symbol's supplement -- it will be needed when
       type entries for instantiations are created. */
    tssp->variant.class_template.type_kind = type_kind;
  }	/* if */
  if (decl_state->is_template_friend &&
      !decl_state->in_prototype_instantiation) {
    /* This is a template friend declaration, add the current class to
       the list of friend classes associated with this template. */
    add_befriending_class_to_class_template(tssp,
                                            decl_state->class_declared_in);
  }  /* if */
  if (!decl_state->in_prototype_instantiation && sym->is_class_member &&
      sym->kind == (a_symbol_kind)sk_class_template) {
    /* This is a member class template declaration.  See if the enclosing
       class was also generated from a template.  If so, find the
       corresponding class template symbol from the prototype instantiation. */
    if (decl_state->class_declared_in != NULL) {
      /* Only do this for the original declaration inside the class. */
      find_class_template_member(sym, sym->parent.class_type);
    }  /* if */
  }  /* if */
  if (decl_state->is_specialization && !decl_state->is_template_friend) {
    /* This template is a specialization of a member template.  Update the
       template information to reflect this. */
    record_specialization(decl_state, sym, tssp);
  }  /* if */
  if (is_definition) {
    a_token_sequence_number   first_token_number = curr_token_sequence_number;
    a_token_sequence_number   last_token_number = NO_TOKEN_SEQUENCE_NUMBER;
    /* Create a token cache in which to store the tokens that make up the
       definition of the template.  This cache will be copied to the
       template supplement later. */
    clear_token_cache(&local_token_cache, /*reusable=*/TRUE);
    definition_token_cache = &local_token_cache;
    decl_state->defines_something = TRUE;
    if (sym != NULL) {
      mark_defined(sym, &locator.source_position);
      /* Create the symbol for the prototype instantiation (but don't do
         the instantiation yet). */
      if (sym->kind == (a_symbol_kind)sk_class_template) {
        /* This is a class template declaration, not a declaration for
           a normal class nested within a template. */
        prototype_sym = make_template_class_symbol(sym);
        /* Now create a new type entry. */
        prototype_type = alloc_type(tssp->variant.class_template.type_kind);
        prototype_sym->variant.class_struct_union.type = prototype_type;
        set_source_corresp(&(prototype_type->source_corresp), prototype_sym);
        set_membership_in_source_corresp(&(prototype_type->source_corresp),
                                         prototype_sym);
        /* Use the name linkage saved at the point of the original template
           declaration. */
        prototype_type->source_corresp.name_linkage =
                                 tssp->variant.class_template.name_linkage;
        /* Build the template argument list for the prototype instantiation
           of this template.  Loop through the template parameters and
           create a corresponding template argument for each. */
        append_addr = &prototype_type->
                  variant.class_struct_union.extra_info->template_arg_list;
        for (tpp = templ_params; tpp != NULL; tpp = tpp->next) {
          param_sym = tpp->param_symbol;
          if (param_sym->kind == (a_symbol_kind)sk_type) {
            tap = alloc_template_arg(/*is_arg_type=*/TRUE);
            tap->variant.type = param_sym->variant.type;
          } else {
            tap = alloc_template_arg(/*is_arg_type=*/FALSE);
            tap->variant.constant = param_sym->variant.constant;
          }  /* if */
          *append_addr = tap;
          append_addr = &tap->next;
        }  /* for */
      } else {
        /* For a class nested within a class template, the member class
           symbol of the prototype instantiation is used. */
        prototype_sym = sym;
        prototype_type = sym->variant.class_struct_union.type;
      }  /* if */
      /* Add the new symbol to the head of the instantiation list. */
      prototype_sym->next = tssp->variant.class_template.instantiations;
      tssp->variant.class_template.instantiations = prototype_sym;
      prototype_sym->defined = TRUE;
    }  /* if */
    /* Initialize a local stop token set. */
    clear_token_set_array(stop_tokens);
    /* This is a class template definition, so scan all the tokens that
       comprise it and cache them away. */
    incr_token_set_array_element(stop_tokens, tok_semicolon);
    if (curr_token == tok_colon) {
      /* Scan the tokens in the base class declarations, stopping when
	 the "{" is reached. */
      incr_token_set_array_element(stop_tokens, tok_lbrace);
      cache_token_stream(definition_token_cache, stop_tokens);
      decr_token_set_array_element(stop_tokens, tok_lbrace);
    }  /* if */
    decr_token_set_array_element(stop_tokens, tok_semicolon);
    /* Scan the class body.  If the body is missing the error will be
       found during prototype instantiation. */
    if (curr_token == tok_lbrace) {
      /* Swallow the "{" and then cache everything through to the "}". */
      cache_curr_token(definition_token_cache);
      (void)get_token();
      incr_token_set_array_element(stop_tokens, tok_rbrace);
      cache_token_stream(definition_token_cache, stop_tokens);
      /* Now cache the "}" (unless we didn't find one). */
      if (curr_token == tok_rbrace) {
        cache_curr_token(definition_token_cache);
        /* Save the token number of the last token of the definition. */
        last_token_number = curr_token_sequence_number;
        /* Advance past the '}'. */
        (void)get_token();
      }  /* if */
    }  /* if */
    /* Add an end-of-source token to the end of the token cache to assure
       that we don't scan past the end of the cache in the actual scan. */
    terminate_token_cache(definition_token_cache);
    /* Note that the semicolon is not cached. */
    if (sym == NULL) {
      /* An error occurred earlier.  Discard the cached body. */
      discard_token_cache(definition_token_cache);
      definition_token_cache = NULL;
    } else {
      if (decl_state->in_prototype_instantiation &&
          decl_state->class_declared_in != NULL &&
          sym->kind == (a_symbol_kind)sk_class_template) {
        /* This is a member template class definition.  Create a template
           cache segment entry so that the body of this template can
           be removed from the enclosing template cache. */
        tssp->cache_segment = alloc_template_cache_segment(sym, tssp);
        tssp->cache_segment->first_token_number = first_token_number;
        tssp->cache_segment->last_token_number = last_token_number;
      }  /* if */
    }  /* if */
  } else {
    if (!decl_state->in_prototype_instantiation) {
      mark_declared(sym, &locator.source_position);
    }  /* if */
    /* This is not a class template definition, so we have no need to
       cache the tokens. */
  }  /* if */
  if (sym != NULL) {
    if (is_definition || tssp->cache.decl_info == NULL) {
      /* Save the information needed to create an instantiation based
         on the definition of the template.  This information is saved
         for the definition and also for the initial declaration. */
     set_template_cache_info(&tssp->cache, definition_token_cache,
                              decl_state->decl_info);
    }  /* if */
  }  /* if */
  *p_sym_ptr = sym;
  *new_type = prototype_type;

  db_exit();
}  /* class_template_declaration */


static void cache_function_template_body(a_token_cache     *p_token_cache,
                                         a_boolean         is_constructor,
                                         a_boolean         *defines_something,
                                         a_symbol_ptr	   sym)
/*
Scan a function template body and cache the tokens (in *p_token_cache) so
that they can be rescanned for the instantiation.  is_constructor is
TRUE if the function is a constructor.  The current source position is
immediately after the function declarator.  *defines_something is set
to TRUE if either a ctor-initializer or a function body appears.
"sym" is the symbol of the function being scanned.
*/
{
  a_token_set_array  stop_tokens;

  db_enter(3, "cache_function_template_body");
  if (curr_token == tok_lbrace ||
      (curr_token == tok_colon && is_constructor)) {
    *defines_something = TRUE;
    /* Initialize a local stop token set. */
    clear_token_set_array(stop_tokens);
    if (curr_token == tok_colon) {
      /* This is a ctor-initializer list on a constructor.  Cache it. */
      incr_token_set_array_element(stop_tokens, tok_lbrace);
      incr_token_set_array_element(stop_tokens, tok_semicolon);
      cache_token_stream(p_token_cache, stop_tokens);
      decr_token_set_array_element(stop_tokens, tok_lbrace);
      decr_token_set_array_element(stop_tokens, tok_semicolon);
    }  /* if */
    if (curr_token == tok_lbrace) {
      /* This is a compound statement that is the body of the function. */
      /* Cache the "{" and advance past it. */
      cache_curr_token(p_token_cache);
      (void)get_token();
      /* Cache all tokens up to the "}" (or end-of-source). */
      incr_token_set_array_element(stop_tokens, tok_rbrace);
      cache_token_stream(p_token_cache, stop_tokens);
      /* Cache the "}" and append an end-of-source token. */
      if (curr_token == tok_rbrace) {
        cache_curr_token(p_token_cache);
        /* A get_token is intentionally not done -- the caller will
           advance past the end of the template declaration. */
      } else {
        sym_error(ec_template_missing_closing_brace, sym);
      }  /* if */
      /* Add an end-of-source token to the end of the token cache to
         assure that we don't scan past the end of the cache in the actual
         scan. */
      terminate_token_cache(p_token_cache);
    }  /* if */
  } else {
    /* No body to cache. */
  }  /* if */
  db_exit();
}  /* cache_function_template_body */


static void cache_template_param_list(a_decl_state_ptr	decl_state)
/*
Cache the tokens from "template" to the ">" that terminates the template
parameter list.  This is necessary because we need to inspect the
template declaration that follows the template parameter lists to see
if it is a friend declaration.  It is also used to determine in advance
whether we are dealing with a full specialization of a template
entity.  The cache may also be used later to build the string representing
the template declaration if the template needs to appear in the IL.

*/
{
  a_token_set_array	stop_tokens;
  a_boolean		is_specialization = FALSE;
  a_boolean		is_full_specialization = TRUE;
  a_token_cache_ptr	p_token_cache = &decl_state->param_list_cache;

  db_enter(3, "cache_template_param_list");
  clear_token_cache(p_token_cache, /*reusable=*/TRUE);
  while (curr_token == tok_template && next_token() == tok_lt) {
    /* Cache the current token and advance past it. */
    cache_curr_token(p_token_cache);
    (void)get_token();
    if (next_token() == tok_gt) {
      /* Contains a template clause with no parameter list, so this is
         a specialization of some kind. */
      is_specialization = TRUE;
    } else {
      /* One ore more template parameters are present in one of the template
         parameter lists, so this is not a full specialization. */
      is_full_specialization = FALSE;
    }  /* if */
    /* Initialize a local stop token set. */
    clear_token_set_array(stop_tokens);
    /* Cache all tokens up to the ">" that matches the current "<". */
    incr_token_set_array_element(stop_tokens, tok_gt);
    incr_token_set_array_element(stop_tokens, tok_lbrace);
    incr_token_set_array_element(stop_tokens, tok_colon);
    incr_token_set_array_element(stop_tokens, tok_semicolon);
    cache_token_stream(p_token_cache, stop_tokens);
    if (curr_token == tok_gt) {
      cache_curr_token(p_token_cache);
      (void)get_token();
    }  /* if */
  }  /* while */
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(p_token_cache);
  decl_state->is_specialization = is_specialization;
  decl_state->is_full_specialization = is_full_specialization;
  db_exit();
}  /* cache_template_param_list */


static void cache_template_declaration(a_decl_state_ptr decl_state,
                                       a_boolean	skip_params)
/*
Scan one or more template parameter clauses and the declaration that
follows, and cache the tokens so that they can be rescanned for the
instantiation.  The declarations for functions must be saved
so that they may be rescanned with the appropriate values substituted
for the template parameters.  In addition, there are a number of lookahead
operations that must be done to determine the type of entity being processed.
is_full_specialization specifies whether the template parameter clauses
that are cached indicate that this declaration is a full specialization of
a template entity.  A full specialization is one in which all of the template
clauses contain empty template parameter lists.  skip_params is TRUE
when this routine is called a second time in certain error conditions
to rescan just the template declaration and not the template parameter list.
*/
{
  a_token_set_array  stop_tokens;

  db_enter(3, "cache_template_declaration");
  if (!skip_params) {
    /* Cache the tokens of the template parameter list(s).  This step is
       skipped if skip_params is TRUE.  This is the case if the declaration
       is being rescanned because of some kind of a syntax error that caused
       the original declaration that was scanned to be incorrect. */
    cache_template_param_list(decl_state);
  }  /* if */
  if (curr_token != tok_end_of_source) {
    /* In an error case, we could be at the end of the source file.  Don't
       try to cache the end-of-source token. */
    /* Cache the current token and advance past it. */
    cache_curr_token(&decl_state->decl_token_cache);
    (void)get_token();
    /* Initialize a local stop token set. */
    clear_token_set_array(stop_tokens);
    /* Cache all tokens up to the ";" that follows a declaration, the "{" that
       begins a definition, or a ":" that begins a ctor initializer list.
       For static data members, some or all of the initializer will be
       in the cache.  The initializer tokens will be removed from this
       cache later.  The only case in which the entire initializer will not
       be in this cache is in cases where the initializer contains a
       brace enclosed list. */
    incr_token_set_array_element(stop_tokens, tok_lbrace);
    incr_token_set_array_element(stop_tokens, tok_colon);
    incr_token_set_array_element(stop_tokens, tok_semicolon);
    cache_token_stream(&decl_state->decl_token_cache, stop_tokens);
  }  /* if */
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(&decl_state->decl_token_cache);
  /* Rescan a copy of the cached tokens from this cache.  This is done so that
     when the original template declaration is scanned the last token of
     the cache is followed by the token that followed it in the original
     source program with no intervening tok_end_of_source.  This also
     allows the reusable token cache to be discarded if it turns out that
     this is not a function declaration. */
  rescan_copy_of_cache(&decl_state->decl_token_cache);
  if (!skip_params) {
    /* Also rescan the tokens from the template parameter list(s).  This
       is done after the rescan of the template declaration because the
       rescanning is a stack-based processed (i.e., the last tokens added
       the rescan list are fetched first. */
    rescan_copy_of_cache(&decl_state->param_list_cache);
  }  /* if */
  db_exit();
}  /* cache_template_declaration */


static a_template_nesting_depth template_nesting_depth(void)
/*
Computes the nesting depth of the current template declaration scope.
The nesting depth indicates the number of template instantiation scopes
that enclose the current one.  If no template instantiation scopes are
present, the nesting depth "0" is used. 
*/
{
  a_template_nesting_depth	curr_depth = 0;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];

  /* Note that we don't have to check for nested instantiation scopes
     because only active instantiation scopes will be on the linked
     list of previous scopes that are examined. */
  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      curr_depth++;
    }  /* if */
  }  /* for */
  return curr_depth;
}  /* template_nesting_depth */


void prescan_function_template_default_arg_expr(a_param_type_ptr  ptp)
/*
Scan a default argument expression and add it to the list of arguments
pointed to by the template symbol supplement.
*/
{
  a_def_arg_expr_fixup_ptr	*list;
  list = &curr_default_args;
  prescan_default_function_arg_expr(ptp, list);
}  /* prescan_function_template_default_arg_expr */


static void prescan_template_param_decl(a_token_cache	*token_cache)
/*
Place the tokens for a template parameter into a token cache.
*/
{
  a_token_set_array  stop_tokens;

  db_enter(3, "prescan_template_param_decl");
  clear_token_cache(token_cache, /*reusable=*/TRUE);
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* In the normal case we will scan an expression and encounter a comma
     or right parenthesis.  If both of these are omitted, terminate the token
     stream when some likely delimiter is reached. */
  incr_token_set_array_element(stop_tokens, tok_comma);
  incr_token_set_array_element(stop_tokens, tok_gt);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  cache_token_stream(token_cache, stop_tokens);
  /* Note that the terminating token (comma, etc.) is not added to
     the cache. */
  /* Add an end-of-source token to the end of the token cache.  This assures
     that we won't scan past the end of the cache in the actual scan. */
  terminate_token_cache(token_cache);
  /* Rescan a copy of the tokens that were just cached.  Rescanning a copy
     ensures that processing of the remainder of the original line will
     not be affected by the tok_end_of_source that terminates the cache. */
  rescan_copy_of_cache(token_cache);
  db_exit();
}  /* prescan_template_param_decl */


static
void scan_a_template_parameter_declaration(a_symbol_locator *param_locator,
					   a_type_ptr       *param_type_ptr)
/*
Scan the declaration of a single template nontype parameter.
*/
{
  a_decl_flag_set              do_flags;
  a_decl_flag_set              dso_flags;
  a_type_qualifier_set         qualifiers;
  a_decl_modifier              decl_modifiers;
  a_storage_class              param_storage_class;
  a_source_position            param_pos;
  a_source_sequence_entry_ptr  declarator_ssep;
  a_type_ptr                   tp;

  /* Scan the declaration specifiers. */
  param_pos = pos_curr_token;
  (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_IS_TEMPLATE_PARAMETER),
                         &dso_flags, &param_storage_class, param_type_ptr,
                         &qualifiers, &decl_modifiers);
  if (dso_flags & DSO_DEFINES_SOMETHING) {
    pos_error(ec_type_definition_not_allowed, &param_pos);
    *param_type_ptr = error_type();
  }  /* if */
  if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  /* Scan the declarator. */
  declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags,
             *param_type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
             param_locator, param_type_ptr,
             &declarator_ssep, (a_func_info_block_ptr)NULL);
  /* Adjust the type if necessary (for example, "array of x"
     becomes "pointer to x"). */
  adjust_parameter_type(param_type_ptr, /*restrict_qualified=*/FALSE);
  /* Check for illegal nontype parameter types. */
  tp = skip_typerefs(*param_type_ptr);
  if (is_void_type(tp)) {
    /* A parameter type of void is not allowed. */
    error(ec_void_template_parameter);
  } else if (tp->kind == (a_type_kind)tk_float) {
#if ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS
    /* Though no longer permitted by the working paper (as of 3/94) floating 
       point template parameters are allowed for backward compatibility.
       Issue a diagnostic in strict ANSI mode. */
    if (strict_ansi_mode) {
      diagnostic(strict_ansi_error_severity, ec_float_template_parameter);
    }  /* if */
#else /* !ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS */
    /* A floating point parameter type of void is no longer allowed
       as of 3/94. */
    error(ec_float_template_parameter);
#endif /* ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS */
  }  /* if */
}  /* scan_a_template_parameter_declaration */


static
a_template_param_ptr scan_template_param_list(a_decl_state_ptr decl_state)
/*
Scan a comma-separated list of template parameters.  The opening "<" will
already have been scanned, and an empty list will have already been
checked for.  The current token, consequently, is the first token of the
first parameter.  Return a pointer to the linked list that is created
to represent the template parameters.
*/
{
  a_symbol_ptr         		sym;
  a_template_param_ptr 		template_param;
  a_template_param_ptr 		template_param_list = NULL;
  a_template_param_ptr 		end_of_template_param_list = NULL;
  a_type_ptr           		template_param_type;
  a_token_cache        		param_cache;
  a_boolean	       		parameter_cache_used = FALSE;
  a_template_param_list_pos	template_param_list_pos = 0;

  db_enter(3, "scan_template_param_list");
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_gt);
  /* Loop through the comma-separated list of template parameter
     declarations. */
  do {
    a_boolean      has_default_arg = FALSE;
    a_boolean	   const_type_involves_template_param = FALSE;
    a_boolean	   def_arg_involves_template_param = FALSE;
    a_token_cache  def_arg_cache;
    a_boolean	   def_arg_cache_used = FALSE;
    a_constant_ptr default_arg_constant;
    a_type_ptr	   default_arg_type;
    a_token_kind   second_token;

    /* If we've unexpectedly reached the end of the template parameter list,
       issue an error. */
    if (curr_token == tok_gt) {
      error(ec_missing_template_param);
      break;
    }  /* if */
    ++template_param_list_pos;
    /* Cache the tokens that comprise the template parameter declaration.
       If the parameter depends on other template parameters this cache
       will be saved and rescanned to scan template argument lists. */
    prescan_template_param_decl(&param_cache);
    add_stop_token(tok_comma);
    /* Determine whether this is a "type-argument" (a parameter that
       represents a type) or a "arg-declaration" (a parameter that represents
       a constant). */
    if ((curr_token == tok_class || curr_token == tok_typename) &&
        next_two_tokens(tok_identifier, &second_token) == tok_identifier &&
        second_token != tok_colon_colon) {
      /* A type-argument. Note that there is a possible ambiguity here:
         template <class T> vs. template <class T X>, where in the second
         case T is already declared.  One could argue that the second is an
         "arg-declaration" rather than a "type-argument", but the working
         paper (14.1 para 2) appears to resolve the ambiguity in favor of
         always interpreting <class T ... as a type-argument.  Although
         the WP is not clear about the extent to which the tokens that follow
         "class T" are involved in the disambiguation.  A similar ambiguity
         exists when "typename" is used.  If the name that follows "class"
         or "typename" is a simple identifier (i.e., not a qualified name)
         we assume it to be a type parameter. */
      /* Bypass "class" or "typename". */
      (void)get_token();
      /* Enter a type symbol in the symbol table.  It is made (for now) to
         point to an error type, to make everything work smoothly during
         preliminary scanning of the body of the class. */
      sym = enter_symbol((a_symbol_kind)sk_type, &locator_for_curr_id,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      /* Allocate a template-param type.  This type is for front-end use
         only and will not appear in the IL passed on to the back end.  It
         is therefore not added to any scope types list. */
      template_param_type = alloc_type((a_type_kind)tk_template_param);
      template_param_type->variant.template_param.extra_info->
                                coordinates.depth = decl_state->nesting_depth;
      template_param_type->variant.template_param.extra_info->
                               coordinates.position = template_param_list_pos;
      set_type_size(template_param_type);
      set_source_corresp(&template_param_type->source_corresp, sym);
      /* The type symbol for the template parameter points for now to the
         template-param type -- "for now", since it will be replaced with
         an actual type during instantiation of the class or function. */
      sym->variant.type = template_param_type;
      sym->is_template_param = TRUE;
      mark_defined(sym, &sym->decl_position);
      /* Bypass the identifier. */
      (void)get_token();
      if (curr_token == tok_assign) {
        /* Scan the default value for a type argument. */
	has_default_arg = TRUE;
	/* Skip past the equals sign. */
        (void)get_token();
        /* Cache the tokens that make up the default argument expression. */
        prescan_default_arg_expr(&def_arg_cache, /*is_template_param=*/TRUE);
        rescan_copy_of_cache(&def_arg_cache);
        type_name(&default_arg_type);
        if (is_or_contains_template_param(default_arg_type)) {
          def_arg_involves_template_param = TRUE;
        }  /* if */
      }  /* if */
    } else if (curr_token != tok_template) {
      a_type_ptr           param_type_ptr;
      a_symbol_locator     param_locator;
      a_constant_ptr       param_con;
      /* Not a type-argument, so treat it as an arg-declaration.  If this
         template declaration happens to be of a function rather than a class,
         arg-declarations are not allowed.  That will be detected later. */
      scan_a_template_parameter_declaration(&param_locator, &param_type_ptr);
#if 0
      /* Check here for types for which constants cannot be created?  E.g.,
         the program would not be able to declare a constant class object or
         a constant array.  Likewise, should reference types be permitted?
         Should a constant with an error type be created for such cases? */
#endif /* if 0 */
      /* Enter a symbol and bind a template param constant to it. At each
         point of instantiation an actual constant will be substituted. */
      sym = enter_symbol((a_symbol_kind)sk_constant, &param_locator,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      sym->variant.constant = param_con =
                         fs_constant((a_constant_repr_kind)ck_template_param);
      param_con->type = param_type_ptr;
      set_template_param_constant_kind(param_con,
                                   (a_template_param_constant_kind)tpck_param);
      param_con->variant.template_param.
                        variant.coordinates.depth = decl_state->nesting_depth;
      param_con->variant.template_param.
                        variant.coordinates.position = template_param_list_pos;
      set_source_corresp(&param_con->source_corresp, sym);
      const_type_involves_template_param = 
				is_or_contains_template_param(param_type_ptr);
      sym->is_template_param = TRUE;
      mark_defined(sym, &sym->decl_position);
      if (curr_token == tok_assign) {
        /* Scan the default value. */
	has_default_arg = TRUE;
	/* Skip past the equals sign. */
        (void)get_token();
        /* Cache the tokens that make up the default argument expression. */
        prescan_default_arg_expr(&def_arg_cache, /*is_template_param=*/TRUE);
        if (const_type_involves_template_param) {
	  /* The type of the constant parameter involve a template parameter
	     type so we can't scan the expression now.  When the type of the
	     constant involves a template parameter we have to save the
	     constant as a token cache, so we also set the flag that indicates
	     that the default argument contains a template parameter. */
          def_arg_involves_template_param = TRUE;
        } else {
	  /* The type doesn't involve a template parameter type.  Scan the
	     default argument expression.  Rescan a copy of the cache.
             This is done so that when the default argument is scanned, the
             last token of the cache is followed by the token that followed
             it in the original source program with no intervening
             tok_end_of_source. */
          rescan_copy_of_cache(&def_arg_cache);
          default_arg_constant = fs_constant((a_constant_repr_kind)ck_error);
          scan_template_argument_constant_expression(param_type_ptr,
						     default_arg_constant);
          def_arg_involves_template_param = default_arg_constant->kind ==
                                      (a_constant_repr_kind)ck_template_param;
        }  /* if */
      }  /* if */
    } else {
      /* Error case ("template ..."). */
      set_to_error_locator(locator_for_curr_id);
      locator_for_curr_id.source_position = pos_curr_token;
      set_err_pos_to_curr_token();
      syntax_error(ec_template_not_allowed);
      /* Enter a dummy param type. */
      sym = enter_symbol((a_symbol_kind)sk_type, &locator_for_curr_id,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      sym->variant.type = error_type();
    }  /* if */
    /* Allocate a template parameter and set its fields based on sym. */
    template_param = alloc_template_param(sym,
                                          def_arg_involves_template_param);
    if (const_type_involves_template_param) {
      /* For nontype parameters, the type of the parameter needs
         to be saved as a token cache if the type uses template
         parameters. */
      template_param->variant.constant.type_involves_template_param = TRUE;
      set_template_cache_info(&template_param->cache, &param_cache,
                              decl_state->decl_info);
      parameter_cache_used = TRUE;
    }  /* if */
    if (has_default_arg) {
      template_param->has_default_arg = TRUE;
      /* Update the default argument information in the template parameter. */
      if (def_arg_involves_template_param) {
        /* The default argument involves a template parameter.  This means that
	   the default needs to be rescanned for each instantiation, so the
           default is saved as a token cache. */
        template_param->def_arg_involves_template_param = TRUE;
        set_template_cache_info(&template_param->default_arg.cache,
                                &def_arg_cache, decl_state->decl_info);
        def_arg_cache_used = TRUE;
      } else {
        /* The default does not use template parameters.  Simply save the
           type or constant that is the default. */
        if (sym->kind == (a_symbol_kind)sk_constant) {
          template_param->default_arg.constant = default_arg_constant;
        } else {
          template_param->default_arg.type = default_arg_type;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Add the template param to the end of the list. */
    if (template_param_list == NULL) {
      template_param_list = template_param;
    } else {
      end_of_template_param_list->next = template_param;
    }  /* if */
    /* Discard the parameter token cache if it is not needed for later use. */
    if (!parameter_cache_used) discard_token_cache(&param_cache);
    if (has_default_arg && !def_arg_cache_used) {
      /* Discard the default argument token cache if it is not needed for
         later use. */
      discard_token_cache(&def_arg_cache);
    }  /* if */
    end_of_template_param_list = template_param;
    /* Make sure we are at the end of a template parameter. */
    if (curr_token != tok_comma && curr_token != tok_gt) {
      pos_error(ec_exp_comma_or_gt, &pos_curr_token);
      flush_tokens();
    }  /* if */
    remove_stop_token(tok_comma);
    /* Keep looping on a comma. */
  } while (loop_token(tok_comma));
  /* Check for an bypass the ">".  If the closing ">" is missing, an
     error will have already been issued  above. */
  if (curr_token == tok_gt) (void)get_token();
  remove_stop_token(tok_gt);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  db_exit();
  return template_param_list;
}  /* scan_template_param_list */


a_type_ptr rescan_template_constant_parameter
                                     (a_symbol_ptr	   template_sym,
                                      a_symbol_ptr	   param_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list,
                                      a_boolean		   do_default_arg,
                                      a_constant_ptr       *constant)
/*
Rescan the tokens of a template parameter declaration and/or default
argument using the current values of any previous parameters so that
the declaration and/or default argument is processed with the types
with which the class is to be instantiated.  This is used to get the
correct types for template parameters whose types depend on other
template parameters.  If the type of the constant depends on a
template parameter, then the type is rescanned.  Otherwise, the
existing type is simply used.  If do_default_arg is TRUE, then the
default argument constant is processed too.  A pointer to the
resulting constant is stored in the pointer pointed to by "constant".
*/
{
  a_type_ptr				constant_type;
  a_symbol_locator   			param_locator;
  a_source_position  			saved_pos_curr_token;
  a_source_position  			saved_error_position;
  a_boolean				type_involves_template_param;
  a_boolean				constant_involves_template_param;

  type_involves_template_param =
               param_ptr->variant.constant.type_involves_template_param;
  constant_involves_template_param =
            param_ptr->def_arg_involves_template_param;
  saved_pos_curr_token = pos_curr_token;
  saved_error_position = error_position;
  if (type_involves_template_param) {
    /* Push the template instantiation scope.  Note that the instance symbol
       passed to push_scope is NULL because we don't yet know which instance
       is being instantiated.  Also note that a class type is not being
       passed for the same reason. */
    (void)push_template_instantiation_scope(param_ptr->cache.decl_info,
  					    (a_type_ptr)NULL,
					    (a_routine_ptr)NULL,
					    (a_symbol_ptr)NULL,
					    template_sym, arg_list);
    /* Rescan the tokens of the function declaration. */
    rescan_reusable_cache(&param_ptr->cache.tokens);
    /* Scan the declaration specifiers. */
    scan_a_template_parameter_declaration(&param_locator, &constant_type);
    /* Skip past any tokens remaining in the cache.  Extra tokens will
       be present under certain error conditions and when a default argument
       has been supplied. */
    flush_past_token_cache_terminator();
    /* Pop the template instantiation scope. */
    pop_template_instantiation_scope();
  } else {
    constant_type = param_sym->variant.constant->type;
  }  /* if */
  if (do_default_arg) {
    /* This parameter has a default argument whose value is to be used. */
    if (constant_involves_template_param) {
      /* Push the template instantiation scope.  See note above regarding
         the instance symbol and class type. */
      a_template_cache_ptr	tcp = &param_ptr->default_arg.cache;
      (void)push_template_instantiation_scope(tcp->decl_info,
    					      (a_type_ptr)NULL,
					      (a_routine_ptr)NULL,
					      (a_symbol_ptr)NULL,
					      template_sym, arg_list);
      rescan_reusable_cache(&tcp->tokens);
      *constant = fs_constant((a_constant_repr_kind)ck_error);
      delayed_scan_of_template_default_arg_expr(constant_type, *constant);
      /* Pop the template instantiation scope. */
      pop_template_instantiation_scope();
    } else {
      *constant = param_ptr->default_arg.constant;
    }  /* if */
  }  /* if */
  error_position = saved_error_position;
  pos_curr_token = saved_pos_curr_token;
  return constant_type;
}  /* rescan_template_constant_parameter */


a_type_ptr rescan_template_type_default_arg
                                     (a_symbol_ptr	   template_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list)
/*
Rescan the tokens of a template parameter default argument using the
current values of any previous parameters so that the default argument
is processed with the types with which the class is to be instantiated.
This is used to get the correct types for type default arguments that
depend on other template parameters.  If the default depends on
a template parameter then the cache is rescanned, otherwise, the
existing type is simply used. 
*/
{
  a_source_position  			saved_pos_curr_token;
  a_source_position  			saved_error_position;
  a_type_ptr				tp;

  if (param_ptr->def_arg_involves_template_param) {
    /* Push the template instantiation scope.  Note that the instance symbol
       passed to push_scope is NULL because we don't yet know which instance
       is being instantiated.  Also note that a class type is not being
       passed for the same reason. */
    a_template_cache_ptr	tcp = &param_ptr->default_arg.cache;
    (void)push_template_instantiation_scope(tcp->decl_info,
                                            (a_type_ptr)NULL,
					    (a_routine_ptr)NULL,
					    (a_symbol_ptr)NULL,
					    template_sym, arg_list);
    saved_pos_curr_token = pos_curr_token;
    saved_error_position = error_position;
    rescan_reusable_cache(&tcp->tokens);
    tp = delayed_scan_of_template_default_type_arg();
    error_position = saved_error_position;
    pos_curr_token = saved_pos_curr_token;
    /* Pop the template instantiation scope. */
    pop_template_instantiation_scope();
  } else {
    tp = param_ptr->default_arg.type;
  }  /* if */
  return tp;
}  /* rescan_template_type_default_arg */


static a_boolean template_param_used_in_type(a_symbol_ptr param_sym,
                                             a_type_ptr   tp)
/*
Returns TRUE if the template parameter specified by param_sym is used in
the type specified by tp.
*/
{
  a_boolean	result;

  if (param_sym->kind == (a_symbol_kind)sk_type) {
    result =
           is_or_contains_specific_template_param(tp, param_sym->variant.type);
  } else {
    result = type_contains_specific_template_param_constant(
                                              tp, param_sym->variant.constant);
  }  /* if */
  return result;
}  /* template_param_used_in_type */


static a_boolean template_param_appears_in_param_list
				(a_symbol_ptr param_sym,
                                 a_type_ptr   rout_type,
			         a_boolean    *only_used_in_default_args)
/*
tparam_type is a tk_template_parameter type entry used in a template
declaration, and rout_type is a routine type.  Search each of the routine's
parameter types to see if tparam_type appears in it.  If
only_used_in_default_args is not NULL then also determine whether the
template parameter is only used in function parameters with default
arguments.
*/
{
  a_boolean         found = FALSE;
  a_boolean	    only_in_default_args;
  a_param_type_ptr  ptp;

  /* Only do this check if the pointer passed by the caller is non-NULL. */
  only_in_default_args = (only_used_in_default_args != NULL);
  ptp = rout_type->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->type_involves_template_param) {
      if (template_param_used_in_type(param_sym, ptp->type)) {
        found = TRUE;
        if (!ptp->has_default_arg) only_in_default_args = FALSE;
      }  /* if */
      /* If we've found all the information we are looking for then stop. */
      if (found && !only_in_default_args) break;
    }  /* if */
  }  /* for */
  if (only_used_in_default_args != NULL) {
    *only_used_in_default_args = only_in_default_args;
  }  /* if */
  return found;
}  /* template_param_appears_in_param_list */


static void fixup_types_that_refer_to_incomplete_instantiations(
                                      a_symbol_ptr   sym,
				      a_type_ptr     prototype_type)
/*
This is the resolution of a previously incomplete template
declaration; check for incomplete instantiations.  And if there are
any incomplete instantiations that were involved in array type
declarations, the instantiations need to be done and the arrays
fixed up at this time.  For example:
    template <class T> class X;
    typedef X<int> arr[10];
    template <class T> class X { ... };
Now that template X has been defined, X<int> can be instantiated and
the size of arr can be computed.
*/
{
  a_template_symbol_supplement_ptr  tssp;
  a_symbol_ptr                      instance_sym;
  a_type_ptr                        class_type;
  a_dependent_type_fixup_ptr        dtfp;

  /* Loop though all the instantiations of the current class template. */
  tssp = template_supplement_for_symbol(sym);
  for (instance_sym = tssp->variant.class_template.instantiations;
       instance_sym != NULL;
       instance_sym = instance_sym->next) {
    if (instance_sym == tssp->variant.class_template.prototype_instantiation) {
      /* Ignore the prototype instantiation. */
    } else {
      class_type = instance_sym->variant.class_struct_union.type;
      if (class_type->variant.class_struct_union.is_specialized) {
        /* Ignore specific definitions. */
      } else {
        /* Found an incomplete instantiation.  Be sure the type kind matches
           that of the current template definition. */
        if (class_type->kind == prototype_type->kind) {
          /* Okay. */
        } else if (class_type->kind == (a_type_kind)tk_union ||
                   prototype_type->kind == (a_type_kind)tk_union) {
          /* Error, detected elsewhere. */
        } else {
          class_type->kind = prototype_type->kind;
        }  /* if */
        /* See if it has any fixup entries that resulted from uses in array
           declarations. */
        dtfp = instance_sym->variant.class_struct_union.extra_info->
                                                   dependent_type_fixup_list;
        for (; dtfp != NULL; dtfp = dtfp->next) {
          if (dtfp->fixup_kind ==
                        (a_dependent_type_fixup_kind)dtfk_array_type_size) {
            /* This one was used in at least one array declaration; there
               may be others on the list but one is enough to justify
               instantiating the template class.  The call to do the array
               fixup is made from scan_class_defintion. */
            instantiate_template_class(instance_sym->
                                          variant.class_struct_union.type);
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* fixup_types_that_refer_to_incomplete_instantiations */


#if RECORD_TEMPLATES_IN_IL
static
void complete_il_template_entry(a_template_ptr il_template_entry,
                                a_symbol_ptr   sym,
                                a_token_cache  *decl_token_cache,
                                a_token_cache  *template_param_list_cache,
                                a_token_cache  *p_template_body_cache)
/*
Finish up establishing the IL template entry.  (It has already been
added to the templates list, its decl_position has been set, and
its source correspondence entry, if any, has been put out.)
*/
{
  a_boolean  err = FALSE;
  if (il_template_entry != NULL) {
    if (sym != NULL && !sym->is_error) {
      /* Set the template kind. */
      switch (sym->kind) {
        case sk_class_template:
          il_template_entry->kind = (a_template_kind)templk_class;
          break;
        case sk_function_template:
          il_template_entry->kind = (a_template_kind)templk_function;
          break;
        case sk_member_function:
          il_template_entry->kind = (a_template_kind)templk_member_function;
          break;
        case sk_static_data_member:
          il_template_entry->kind = (a_template_kind)templk_static_data_member;
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
          check_assertion(sym->is_class_member);
          il_template_entry->kind = (a_template_kind)templk_member_class;
          break;
        default:
          /* There must have been an error.  Do the check because we don't
             want an incomplete IL entry to be handed to the back end. */
          check_assertion(total_errors > 0);
          err = TRUE;
      }  /* switch */
      if (!err) {
	/* Give it a name, etc. */
	set_source_corresp(&il_template_entry->source_corresp, sym);
        /* Set parent information in the IL entry. */
        if (sym->is_class_member) {
          if (!(symbol_supplement_for_class(sym->parent.class_type))->
                                                         is_nonreal_class) {
            set_class_membership((a_symbol_ptr)NULL,
                                 &il_template_entry->source_corresp,
                                 sym->parent.class_type);
          }  /* if */
        } else if (sym->parent.namespace_ptr != NULL) {
          set_namespace_membership((a_symbol_ptr)NULL,
                                   &il_template_entry->source_corresp,
                                   sym->parent.namespace_ptr);
        }  /* if */
	/* Create the string that represents the template declaration. */
	make_template_string(il_template_entry, template_param_list_cache,
			     decl_token_cache, p_template_body_cache);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* complete_il_template_entry */
#endif /* RECORD_TEMPLATES_IN_IL */


static a_symbol_ptr template_static_data_member_declaration
                    (a_decl_state_ptr                 decl_state,
                     a_symbol_locator                 *locator,
		     a_decl_flag_set                  do_flags,
		     a_type_ptr                       type,
		     a_template_symbol_supplement_ptr *p_tssp)
/*
Scan a template static data member declaration.  locator identifies
the static data member being declared.  do_flags contains the
declaration flags returned by declarator.  type is the type pointer
returned by declarator.  template_param_list points to the parameter
list for this template declaration.  p_tssp points to the location
in which the template symbol supplement for this template should be
returned to the caller.
*/
{
  /* Name is a member of a class template (or a class nested within a class
     template).  It is not a function, so (in a legal program) it must be
     a static data member. */
  /* Special processing for static data member template declarations. */
  a_boolean                        err = FALSE;
  a_token_cache                    local_token_cache;
  a_token_cache                    *p_token_cache = NULL;
  a_symbol_ptr                     sym;
  a_boolean                        has_parenthesized_initializer = FALSE;
  a_template_symbol_supplement_ptr tssp = NULL;

  db_enter(4, "template_static_data_member_declaration");
  sym = locator->specific_symbol;
  has_parenthesized_initializer = 
                              (do_flags & DO_PARENTHESIZED_INITIALIZER) != 0;
  if (is_error_locator(*locator)) {
    /* An error occurred while scanning the declarator of what we assume
       is a static data member.  We make this assumption because the
       declarator is not a function and is followed by an equals sign. */
    err = TRUE;
  } else if (sym->kind != (a_symbol_kind)sk_static_data_member) {
    /* Not a static data member. */
    if (sym->kind == (a_symbol_kind)sk_field) {
      pos_error(ec_nonstatic_member_def_not_allowed,
		&locator->source_position);
    } else if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else {
      pos_sy_error(ec_not_compatible_with_previous_decl,
		   &locator->source_position, sym);
    } /* if */
    err = TRUE;
  } else if (!namespace_is_enclosed_by_scope(
                         sym, &scope_stack[depth_innermost_namespace_scope])) {
    /* Static data member template is being defined in a scope that does not
       enclose the scope in which the parent class was defined. */
    sym_error(ec_bad_scope_for_definition, sym);
  } else if (sym->defined) {
    /* Prior definition. */
    pos_sy_error(ec_already_defined, &locator->source_position, sym);
    err = TRUE;
  } else if (!types_are_redecl_compatible(type,
                                          sym->variant.static_data_member.
                                                            variable->type)) {
    /* The type of the static data member definition does not match
       the declaration in the class. */
    pos_sy_error(ec_not_compatible_with_previous_decl,
		 &locator->source_position, sym);
    err = TRUE;
  } else {
    /* This is a template definition of a static data member of a
       class template. */
#if CHECKING
    if (sym->variant.static_data_member.instance_ptr->template_sym != sym) {
      internal_error("template_declaration: bad instance for static mem");
    } /* if */
#endif /* CHECKING */
    mark_defined(sym, &locator->source_position);
    tssp = sym->variant.static_data_member.instance_ptr->template_info;
    /* Make sure the parameter list matches the class declaration. */
    if (!member_template_param_list_matches_class
                              (decl_state->decl_info, sym, &error_position)) {
      err = TRUE;
    } /* if */
  }  /* if */
  /* Scan the initializer expression, if any, and cache its tokens.
     The initializer may be of the form "= ...;" or "(...);".
     Anything else will not get cached and an error will be generated
     on this declaration. */
  if (curr_token == tok_assign || has_parenthesized_initializer) {
    a_token_sequence_number	split_location;
    a_token_set_array		stop_tokens;
    p_token_cache = &local_token_cache;
    clear_token_cache(p_token_cache, /*reusable=*/TRUE);
    /* Then declaration token cache contains the declaration and the
       initializer.  Split the cache so that the initialization is
       removed from the declaration cache and placed in the initializer
       cache. */
    split_location = curr_token_sequence_number;
    split_token_cache(&decl_state->decl_token_cache,
                      p_token_cache, split_location,
                      /*include_prev_token=*/has_parenthesized_initializer);
    /* Skip over the tokens that are already part of the token cache. */
    clear_token_set_array(stop_tokens);
    incr_token_set_array_element(stop_tokens, tok_lbrace);
    incr_token_set_array_element(stop_tokens, tok_colon);
    incr_token_set_array_element(stop_tokens, tok_semicolon);
    flush_tokens_with_stop_tokens(stop_tokens);
    if (curr_token != tok_semicolon) {
      /* The initializer was not fully cached when the template declaration
         was scanned.  This is usually because of a brace enclosed
         initializer.  Cache the rest of the initializer now. */
      decr_token_set_array_element(stop_tokens, tok_lbrace);
      decr_token_set_array_element(stop_tokens, tok_colon);
      remove_cache_terminator(p_token_cache);
      /* Only semicolon should be left on the list. */
      cache_token_stream(p_token_cache, stop_tokens);
      terminate_token_cache(p_token_cache);
    }  /* if */
    if (err) {
      discard_token_cache(p_token_cache);
      p_token_cache = NULL;
    } /* if */
  } /* if */
  if (tssp != NULL) {
    /* Save the information needed to create an instantiation based
       on the definition of the template. */
    set_template_cache_info(&tssp->cache, p_token_cache,
                            decl_state->decl_info);
  }  /* if */
  *p_tssp = tssp;
  db_exit();
  return sym;
}  /* template_static_data_member_declaration */


static void check_function_template_param_usage
                         (a_symbol_ptr                     sym,
			  a_type_ptr                       type,
			  a_template_param_ptr             template_param_list,
			  a_template_symbol_supplement_ptr tssp)
/*
Make sure that all of the template parameters are used as part of the
signature of the functions that will be generated from this template.
Use in a function parameter with a default argument is not counted as
it would not be possible to deduce the value of a template parameter
when the associated function argument was omitted.
*/
{
  a_template_param_ptr   tpp;
  a_type_ptr	         rout_type = skip_typerefs(type);
  a_boolean		 is_conversion_operator;

  is_conversion_operator = is_conversion_function_symbol(sym);
  for (tpp = template_param_list; tpp != NULL; tpp = tpp->next) {
    a_symbol_ptr param_sym = tpp->param_symbol;
    a_boolean	 only_in_default_args;
    a_boolean	 param_used;
    if (tpp->has_default_arg) {
      pos_error(ec_default_template_arg_not_allowed,
                &param_sym->decl_position);
    }  /* if */
    if (is_conversion_operator) {
      /* For conversion operator functions, the template parameters must be
         used in the return type. */
      param_used =  template_param_used_in_type(
                            param_sym, rout_type->variant.routine.return_type);
      only_in_default_args = FALSE;
    } else {
      /* Make sure that all template parameters are used by
         function parameter types and not just by parameters
         with default arguments.  If an error occurs set the
         cannot_be_called flag to prevent an instantiation from
         being attempted with an incomplete set of template arguments. */
      param_used = template_param_appears_in_param_list
                                 (param_sym, rout_type, &only_in_default_args);
    }  /* if */
    if (!param_used) {
      pos_sy2_error(ec_not_used_in_template_function_params,
                    &param_sym->decl_position, param_sym, sym);
      tssp->variant.function.cannot_be_called = TRUE;
    } else if (only_in_default_args) {
      pos_sy2_error(ec_template_param_only_used_in_default_args,
                    &param_sym->decl_position, param_sym, sym);
        tssp->variant.function.cannot_be_called = TRUE;
    } /* if */
  } /* for */
}  /* check_function_template_param_usage */


static void add_befriending_class_to_function_template
                      (a_template_symbol_supplement_ptr     tssp,
		       a_type_ptr                           class_declared_in)
/*
Indicate that the template designated by tssp is a friend of the class
specified by class_declared_in.  If any instances of the template have already
been instantiated, update the befriending information for the instances.
*/
{
  a_class_list_entry_ptr  clep;
  a_template_instance_ptr tip;

  clep = alloc_list_entry_for_class();
  clep->next = tssp->befriending_classes;
  clep->class_type = class_declared_in;
  tssp->befriending_classes = clep;
  /* Update any instances that have already been created. */
  for (tip = tssp->variant.function.instantiations;
       tip != NULL; tip = tip->next) {
    a_symbol_ptr  instance_sym = tip->instance_sym;
    a_routine_ptr rout_ptr = instance_sym->variant.routine.ptr;
    update_friend_function_info(rout_ptr, class_declared_in,
                                /*is_definition=*/FALSE,
                                /*move_to_front=*/FALSE);
  }  /* for */
  if (tssp->subordinate_templates != NULL) {
    /* This is a member function template declared in a class template.
       We need to visit the template symbols for this template in each
       of the instantiations of the enclosing class template and update
       the befriending information for the instantiations of those
       templates. */
    a_symbol_list_entry_ptr	slep;
    for (slep = tssp->subordinate_templates; slep != NULL; slep = slep->next) {
      a_symbol_ptr			subordinate_sym;
      a_template_symbol_supplement_ptr	subordinate_tssp;
      subordinate_sym = slep->symbol;
      subordinate_tssp = template_supplement_for_symbol(subordinate_sym);
      add_befriending_class_to_function_template(subordinate_tssp,
                                                 class_declared_in);
    }  /* for */
  }  /* if */
}  /* add_befriending_class_to_function_template */


static void complete_function_template_decl(
                     a_decl_state_ptr		      decl_state,
                     a_symbol_ptr                     sym,
                     a_func_info_block                *func_info,
                     a_template_symbol_supplement_ptr *p_tssp,
                     a_source_position		      *decl_pos)
/*
Complete the processing for a function template declaration.  sym is a symbol
indicating the template.  func_info points to the block of information for
the current function declaration.  template_decl_info points to the template
declaration information (parameter list, declaration scope, etc.)  for this
template declaration.  p_tssp points to the location in which the
template symbol supplement for this template should be returned to the
caller.
*/
{
  a_boolean                        err = sym == NULL || sym->is_error;
  a_template_symbol_supplement_ptr tssp = NULL;
  a_template_param_ptr             template_param_list =
                                           decl_state->decl_info->parameters;

  if (!err && !is_function_or_template_symbol(sym)) {
    /* The symbol is something other than a function symbol.  Issue
       an error and set the symbol to NULL.  This error test only applies
       to class member templates. */
    pos_sy_error(ec_bad_member_template_decl, decl_pos, sym);
    err = TRUE;
    sym = NULL;
  }  /* if */
  if (sym != NULL) tssp = template_supplement_for_symbol(sym);
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_function_template &&
      sym->is_class_member && !decl_state->is_template_friend) {
    if (decl_state->in_prototype_instantiation) {
      /* Save the token sequence number associated with this declaration.
         This is done here for function templates that are class members.
         This information is used later to match a template declaration in
         a real instantiation with the corresponding template from the
         prototype instantiation. */
      tssp->token_sequence_number = curr_token_sequence_number;
    } else {
      /* Find the associated template from the prototype instantiation.  This
         can be changed later if a specialization is seen before any
         instantiations are done. */
      if (decl_state->class_declared_in != NULL) {
        /* Only do this for the original declaration inside the class. */
        find_function_template_member(sym, decl_state->class_declared_in);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Make sure that the template parameter list is compatible with
     any previous declaration (i.e., the declaration of the class
     if this is a member function. */
  if (!err && sym->is_class_member &&
      (decl_state->class_declared_in == NULL ||
       decl_state->is_template_friend)) {
    if (!member_template_param_list_matches_class(decl_state->decl_info,
                                                  sym, decl_pos)) {
      err = TRUE;
    } /* if */
  } /* if */
  if (err) {
    a_token_cache  local_token_cache;
    clear_token_cache(&local_token_cache, /*reusable=*/FALSE);
    cache_function_template_body(&local_token_cache, /*is_ctor=*/TRUE,
                                 &decl_state->defines_something, sym);
    discard_token_cache(&local_token_cache);
  } else {
    a_def_arg_expr_fixup_ptr    daefp;
    a_token_cache               local_token_cache;
    a_token_sequence_number     first_token_number;
    a_token_sequence_number     last_token_number;

    clear_token_cache(&local_token_cache, /*reusable=*/TRUE);
    first_token_number = curr_token_sequence_number;
    cache_function_template_body(&local_token_cache,
                                 is_constructor_symbol(sym),
                                 &decl_state->defines_something, sym);
    last_token_number = curr_token_sequence_number;
    if (decl_state->in_prototype_instantiation) {
      if (sym->is_class_member && decl_state->class_declared_in != NULL &&
          sym->kind == (a_symbol_kind)sk_function_template) {
        /* This is a member template function definition.  Create a template
           cache segment entry so that the body of this template can
           be removed from the enclosing template cache. */
        tssp->cache_segment = alloc_template_cache_segment(sym, tssp);
        tssp->cache_segment->first_token_number = first_token_number;
        tssp->cache_segment->last_token_number = last_token_number;
      }  /* if */
    }  /* if */
    if (decl_state->is_specialization && !decl_state->is_template_friend) {
      /* This template is a specialization of a member template.  Update the
         template information to reflect this. */
      record_specialization(decl_state, sym, tssp);
    }  /* if */
    if (tssp->variant.function.decl_cache.tokens.first_token == NULL) {
      /* The decl_token_cache is always saved from the initial declaration
         of the template.  Note that this may be different than the one
         for which the func_info block is saved.  This is done so that
         the return type and declarator will be of an appropriate form
         so that partial instantiations of the function can be done in
         the context of the original declaration. */
      set_template_cache_info(&tssp->variant.function.decl_cache,
                              &decl_state->decl_token_cache,
                              decl_state->decl_info);
      decl_state->decl_token_cache_used = TRUE;
    }  /* if */
    if (decl_state->defines_something || 
        tssp->cache.decl_info == NULL) {
      /* This is either the defining declaration or the initial declaration
         (or both). */
      if (func_info != NULL) {
        /* The func_info block should point to the declaration associated
           with the definition, if a definition is present.  This is done
           elsewhere for class members. */
        tssp->variant.function.func_info = *func_info;
        /* Copy the func_info block and then null out its param-id
           pointer so that it won't be freed. */
        func_info->param_id_list = NULL;
      }  /* if */
      /* Save the token cache and associated template declaration
         information.  This is done for the initial declaration and
         is done again if the function is defined later. */
      set_template_cache_info(&tssp->cache,
                              &local_token_cache,
                              decl_state->decl_info);
    } /* if */
    daefp = curr_default_args;
    /* Update the template declaration information to refer to
       the declaration information of the function template. */
    while (daefp != NULL) {
      daefp->cache.decl_info = decl_state->decl_info;
      daefp = daefp->next;
    }  /* while */
    /* Link the default argument list from the template supplement
       onto the end of the list of current default arguments.  The
       list in the supplement must be for arguments that follow the
       new list (otherwise it would be an error).  Find the end
       of the current list and link the existing list to the end. */
    daefp = curr_default_args;
    if (daefp != NULL) {
      while (daefp->next != NULL) {
        daefp = daefp->next;
      }  /* if */
      daefp->next = tssp->variant.function.def_arg_expr_list;
      tssp->variant.function.def_arg_expr_list = curr_default_args;
    } /* if */
    if (decl_state->is_template_friend &&
       !decl_state->in_prototype_instantiation) {
      /* This is a template friend declaration, add the current class to
         the list of friend classes associated with this template. */
      add_befriending_class_to_function_template(
                                          tssp, decl_state->class_declared_in);
    }  /* if */
  } /* if */
  if (err) {
    /* Avoid spurious errors -- skip the check for template params, since
       this might have been intended to be a member function. */
  } else if (sym->kind != (a_symbol_kind)sk_function_template) {
    /* Out-of-line definition of a member function of a class template.
       Don't impose requirements on the use of template parameters in the
       parameters. */
  } else {
    /* Go back through the template params and make sure that all of the
       template parameters were used in a way that effects the function
       signature. */
    a_type_ptr  type = tssp->variant.function.routine->type;
    check_function_template_param_usage(sym, type, template_param_list, tssp);
  }  /* if */
  *p_tssp = tssp;
}  /* complete_function_template_decl */


static a_symbol_ptr function_template_declaration(
                               a_decl_state_ptr		decl_state,
                               a_symbol_locator         *locator,
                               a_func_info_block        *func_info,
                               a_storage_class          storage_class,
                               a_decl_modifier          decl_modifiers,
                               a_type_ptr               type)
/*
Scan a function template declaration or the declaration of a member function
of a class template.  locator identifies the function template being
declared.  func_info points to the block of information for the current
function declaration.  storage_class, decl_modifiers, and type indicate
information returned from decl_specifiers and declarator.
*/
{
  a_symbol_ptr  sym = NULL;

  db_enter(4, "function_template_declaration");  
  /* Set a flag in each param type entry whose associated type is or
     contains a template parameter. */
  set_type_involves_template_param_flags(type);
  if (curr_token == tok_lbrace && decl_state->is_member_decl &&
      decl_state->is_template_friend) {
    /* A function template defined inside a class or class template is
       implicitly "inline".  Note that the only member declarations
       processed by this routine are friend declarations. */
    func_info->is_inline = TRUE;
  }  /* if */
  /* Process a function template declaration. */
  decl_function_template(locator, type, func_info, &sym, storage_class,
                         decl_modifiers, decl_state->decl_info->parameters,
                         decl_state->effective_decl_level);
  db_exit();
  return sym;
}  /* function_template_declaration */


static a_boolean is_class_template_decl(a_token_cache *token_cache)
/*
Determine whether the template declaration described by token_cache
is a class template declaration of the form

	friend	class-key identifier tok_end_of_source
	      opt

Return TRUE if the declaration is a class template declaration.
Otherwise, return FALSE.  This is done by rescanning the tokens from
the declaration token cache.
*/
{
  a_boolean		result = FALSE;

  rescan_reusable_cache(token_cache);
  if (curr_token == tok_friend) (void)get_token();
  if (curr_token == tok_class || curr_token == tok_struct ||
      curr_token == tok_union) {
    (void)get_token();
    if (is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                        GID_USE_PROTOTYPE_NOT_NONREAL)) {
      (void)get_token();
      if (curr_token == tok_end_of_source) {
        result = TRUE;
      }  /* if */
    } else if (curr_token == tok_end_of_source) {
      /* A class template declaration with a missing identifier.  Return
         TRUE for better error recovery. */
      result = TRUE;
    }  /* if */
  }  /* if */
  /* Flush and remaining tokens from the reusable cache. */
  while (curr_token != tok_end_of_source) (void)get_token();
  /* Skip past the tok_end_of_source. */
  (void)get_token();
  return result;
}  /* is_class_template_decl */


static a_boolean is_template_friend_decl(a_decl_state_ptr decl_state)
/*
Scan the tokens of a template declaration and determine whether
the token "friend" is used in the declaration.
*/
{
  a_boolean	result = FALSE;
 
  rescan_reusable_cache(&decl_state->decl_token_cache);
  /* Go through the tokens of the cache.  We have to scan all the way to
     the end even if the friend token is found so that the token stream
     will be at the right place when we return. */
  while (curr_token != tok_end_of_source) {
    if (curr_token == tok_friend) result = TRUE;
    (void)get_token();
  }  /* if */
  /* Skip past the tok_end_of_source. */
  (void)get_token();
  return result;
}  /* is_template_friend_decl */


static void prescan_nonclass_template_declaration(a_token_cache *token_cache)
/*
This routine is called before scanning a template declaration to determine
whether this is the definition of a member of a class template, and if so,
which class template.  This is needed for cases like

	template <class T> struct B {};
	template <class T> struct A {
          A<T> f();
          B<T> g();
	};
	template <class T> A<T> A<T>::f(){}  // A<T> refers to prototype
	template <class T> B<T> A<T>::f(){}  // B<T> refers to nonreal

The problem is that until you have seen the declarator, you don't know
the class associated with the template (if any), but you need to know
whether template references earlier in the line refer to the prototype
or a nonreal instantiation.  The difference becomes significant when
the name being used is something like A<T>::X.  If A<T> is the
prototype instantiation, then you know what X is, if it is a nonreal
instantiation, then you don't know what X is.
*/
{
  a_type_ptr			tp;
  a_symbol_ptr			sym = NULL;
  a_scope_stack_entry_ptr	ssep;

  db_enter(4, "prescan_nonclass_template_declaration");

#if 0
  /* This will need to be checked when member template classes are
     implemented. */
#endif /* 0 */  
  ssep = &scope_stack[depth_scope_stack];
  check_assertion(ssep->kind == (a_scope_kind)sck_template_declaration);
  tp = prescan_and_find_declarator(token_cache);
  if (tp != NULL) tp = skip_typerefs(tp);
  /* The following is_class_struct_union_type test is needed because in
     certain error cases the type may not be a class type. */
  if (tp != NULL && is_class_struct_union_type(tp)) {
    /* Skip out to the nearest enclosing class that has a template argument
       list. */
    while (tp->source_corresp.is_class_member &&
           tp->variant.class_struct_union.extra_info->
                                                  template_arg_list == NULL) {
      tp = tp->source_corresp.parent.class_type;
    }  /* while */
    /* Make sure that this is a class type.  If it is not, ignore the
       type.  It must be an error and will be diagnosed during the real
       scanning of the template. */
    if (!is_class_struct_union_type(tp)) {
      /* Not a class type -- ignore it. */
    } else {
      sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
      check_assertion(sym != NULL);
      sym = sym->variant.class_struct_union.extra_info->class_template;
    }  /* if */
  }  /* if */
  /* Save the symbol that points to the template whose member is being
     instantiated.  This will be NULL if this is not a member
     declaration. */
  ssep->templ_member_class_sym = sym;
  db_exit();
}  /* prescan_nonclass_template_declaration */


static void scan_template_param_clauses(a_decl_state_ptr	decl_state)
/*
Scan one or more template parameter lists of the form:

	template < param-list    >
                             opt

The parameter list can be empty for a specialization declaration.  Once
a non-empty parameter list has been specified, all subsequent parameter
lists must by non-empty.
*/
{
  a_template_decl_info_ptr	    prev_template_decl_info = NULL;
  a_template_decl_info_ptr	    template_decl_info = NULL;
  a_boolean			    param_list_seen = FALSE;

  /* Loop until there are no more template parameter clauses.  Note that
     this routine is not called for explicit instantiations, in which
     the template keyword is not followed by a parameter clause. */
  while (curr_token == tok_template) {
    decl_state->nesting_depth++;
    decl_state->number_of_template_param_clauses++;
    /* Bypass "template".  The next token should be "<".  This is done
       before the scope is pushed so that any pragma associated with the
       tok_template token will be processed in the current scope. */
    (void)get_token();
    if (curr_token == tok_lt) {
      /* Bypass the "<". */
      (void)get_token();
      if (curr_token != tok_gt) {
        /* Create a template declaration information entry for this
           declaration. A pointer to this entry will be stored in the
           template cache entries that contain tokens from this declaration. */
        template_decl_info = alloc_template_decl_info();
        decl_state->decl_info = template_decl_info;
        template_decl_info->enclosing_scope = decl_state->enclosing_scope;
        /* If there are multiple template parameter clauses in a single
           declaration, create a link to the template parameter list
           declaration that preceded the current one. */
        template_decl_info->enclosing_template_decl = prev_template_decl_info;
        prev_template_decl_info = template_decl_info;
        push_template_declaration_scope(template_decl_info);
        decl_state->number_of_template_decl_scopes++;
        template_decl_info->parameters = scan_template_param_list(decl_state);
        template_decl_info->declaration_scope =
                                         scope_stack[decl_scope_level].number;
        /* Record that a template parameter list has been seen.  A
           subsequent missing parameter list is an error. */
        param_list_seen = TRUE;
      } else {
        /* A specialization declaration.  If a previous "template < >" clause
           contained a template parameter list, all subsequent parameter
           lists must be non-empty. */
        if (param_list_seen) {
          error(ec_specialization_follows_param_list);
          decl_state->decl_scope_err = TRUE;
        }  /* if */
        /* Bypass the ">". */
        (void)get_token();
      }  /* if */
    } else {
      error(ec_missing_template_param_list);
    }  /* if */
  }  /* while */
  decl_state->decl_info = template_decl_info;
  if (curr_token_sequence_number !=
            decl_state->decl_token_cache.first_token->token_sequence_number &&
      curr_token != tok_end_of_source) {
    /* We aren't where we expected to be after scanning the template parameter
       lists.  This should be the result of an error.  Recache the template
       declaration at this point. */
    check_assertion(total_errors != 0);
    discard_token_cache(&decl_state->decl_token_cache);
    clear_token_cache(&decl_state->decl_token_cache, /*reusable=*/TRUE);
    cache_template_declaration(decl_state, /*skip_params=*/TRUE);
  }  /* if */
  if (decl_state->is_member_decl && !decl_state->is_template_friend &&
      decl_state->number_of_template_param_clauses > 1) {
    /* A declaration with more than one template parameter clause is only
       valid in a namespace scope definition of a member template or in
       a friend declaration. */
    error(ec_multiple_template_decls_not_allowed);
    decl_state->decl_scope_err = TRUE;
  }  /* if */
}  /* scan_template_param_clauses */


static
void template_declaration(a_decl_state_ptr	decl_state)
/*
Scan a C++ template declaration.  Syntax:

  template-declaration:

    template < template-argument-list > declaration

  template-argument:

    type-argument
    argument-declaration

  type-argument:

    class identifier

When this routine is called, the template parameter clauses will already
have been scanned and the current token will be the first token of the
declaration that follows the template parameter list.  In addition, the
declaration will be been prescanned to determine whether it is a friend
declaration.  Template declaration scopes will have been pushed for
any non-empty template parameter lists that were scanned.
*/
{
  a_symbol_ptr                      sym = NULL;
  a_template_symbol_supplement_ptr  tssp = NULL;
  a_boolean                         tag_resolution = FALSE;
  a_type_ptr                        prototype_type = NULL;
#if RECORD_TEMPLATES_IN_IL
  a_token_cache                     *p_template_body_cache = NULL;
#endif /* RECORD_TEMPLATES_IN_IL */
  a_boolean                         is_class_template = FALSE;
  a_template_cache_segment_ptr	    cache_segments;

  db_enter(3, "template_declaration");
  /* See if it is a class template declaration.  If it is, scan the tokens
     of the definition (if any) and cache them away of later reference. */
  if (is_class_template_decl(&decl_state->decl_token_cache)) {
    class_template_declaration(decl_state, &sym,
			       &tag_resolution, &prototype_type);
    is_class_template = TRUE;
    tssp = sym != NULL ? template_supplement_for_symbol(sym) : NULL;
#if RECORD_TEMPLATES_IN_IL
    if (decl_state->defines_something && sym != NULL) {
      /* Save a pointer to the token cache for class template body. */
      p_template_body_cache = &tssp->cache.tokens;
    }  /* if */
#endif /* RECORD_TEMPLATES_IN_IL */
  } else {
    /* Not a class template declaration.  Check for a function template
       declaration or a static data member template definition. */
    /* Determine whether the thing being declared is a member of a
       class template.  This is needed to know how references to the
       parent class should be processed.  This must be done before 
       is_decl_start is called, as is_decl_start will cause the initial
       identifier (typically the return type) to be coalesced. */
    prescan_nonclass_template_declaration(&decl_state->decl_token_cache);
    if (!is_decl_start(/*expr_context=*/FALSE,
                       /*real_declarator_allowed=*/TRUE) &&
        !is_declarator_start()) {
      /* Template parameters are declared, but the declaration is missing. */
      pos_error(ec_exp_declaration, &pos_curr_token);
    } else if (decl_state->is_member_decl && !decl_state->is_template_friend) {
      /* A member template declaration. */
      a_source_position	decl_start_pos;
      decl_start_pos = pos_curr_token;
      sym = class_member_template_declaration(decl_state->class_declared_in);
      complete_function_template_decl(decl_state, sym,
                                      (a_func_info_block *)NULL,
                                      &tssp, &decl_start_pos);
#if RECORD_TEMPLATES_IN_IL
      if (decl_state->defines_something) {
        /* Save a pointer to the token cache for function body.  tssp may
           be NULL in error cases. */
        if (tssp != NULL) p_template_body_cache = &tssp->cache.tokens;
      } /* if */
#endif /* RECORD_TEMPLATES_IN_IL */
    } else {
      a_type_ptr         type;
      a_symbol_locator   locator;
      a_decl_flag_set    do_flags;
      a_decl_flag_set    dso_flags;
      a_func_info_block  func_info;
      a_storage_class    storage_class;
      a_decl_modifier    decl_modifiers;

      /* Scan the decl. specifiers and the declaration. */
      clear_func_info(&func_info);
      scan_template_declaration(/*is_initial_decl=*/TRUE,
                                decl_state->is_member_decl,
                                decl_state->class_declared_in,
                                decl_state->decl_scope_err,
                                decl_state->is_specialization,
                                &dso_flags, &do_flags, &locator, &type,
                                &func_info, &storage_class, &decl_modifiers);
      /* If an error occurred scanning the declarator, set the flag to
         suppress subsequent errors. */
      if (is_error_locator(locator)) decl_state->decl_scope_err = TRUE;
      if (!locator.is_qualified_name && !decl_state->decl_scope_err &&
          !decl_state->is_template_friend &&
          decl_state->number_of_template_param_clauses > 1) {
        /* This is a declaration of class template that is not a friend, but
           it has multiple template parameter clauses.  This is an error
           except for member declarations done outside of the class.  We
           know this is not one of those, the identifier is not a qualified
           name. */
        error(ec_multiple_template_decls_not_allowed);
        decl_state->decl_scope_err = TRUE;
      }  /* if */
      if (decl_state->decl_scope_err) {
        set_to_named_error_locator(locator);
      }  /* if */
      if (!is_function_type(type) && 
          (locator.specific_symbol != NULL ||
           (is_error_locator(locator) && curr_token == tok_assign))) {
        sym = template_static_data_member_declaration(
                                 decl_state, &locator, do_flags, type, &tssp);
#if RECORD_TEMPLATES_IN_IL
        /* Save a pointer to the token cache for the initializer.  tssp
           may be NULL in error cases. */
        if (tssp != NULL) p_template_body_cache = &tssp->cache.tokens;
#endif /* RECORD_TEMPLATES_IN_IL */
      } else if (is_function_type(type)) {
        sym = function_template_declaration(decl_state, &locator,
                                            &func_info, storage_class,
                                            decl_modifiers, type);
        complete_function_template_decl(decl_state, sym, &func_info,
                                        &tssp, &locator.source_position);
#if RECORD_TEMPLATES_IN_IL
        if (decl_state->defines_something) {
          /* Save a pointer to the token cache for function body.  tssp may
             be NULL in error cases. */
          if (tssp != NULL) p_template_body_cache = &tssp->cache.tokens;
        } /* if */
#endif /* RECORD_TEMPLATES_IN_IL */
      } else {
        /* Error -- not a class template, a function template, nor a static
           data member template. */
        if (!is_error_locator(locator)) {
          pos_st_error(ec_bad_template_declaration, &locator.source_position,
                       locator.symbol_header->identifier);
        }  /* if */
      }  /* if */
      done_with_func_info(func_info);
    }  /* if */
  }  /* if */
  /* Check and/or advance past the terminating token of the declaration. */
  if (decl_state->defines_something && !is_class_template) {
    /* It's a definition but not a class template definition, so it must be
       a function template definition. */
    if (decl_state->no_advance_past_final_token) {
      /* Leave it to the caller to advance past the closing right brace. */
    } else if (curr_token == tok_rbrace) {
      (void)get_token();
    }  /* if */
  } else {
    /* All template declarations except function template definitions should
       terminate with a semicolon. */
    if (decl_state->no_advance_past_final_token) {
      (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
    } else {
      (void)required_token(tok_semicolon, ec_exp_semicolon);
    }  /* if */
  }  /* if */
  /* Pop all of the template declaration scopes that were pushed earlier.
     Note that this must be done before doing the prototype instantiation. */
  for (; decl_state->number_of_template_decl_scopes != 0;
         decl_state->number_of_template_decl_scopes--) {
    pop_scope();
  }  /* for */
  /* Any pbk_next_construct pragmas will be considered to bind to each of
     the instances generated from the template.  Save the current construct
     pragma list in the template symbol supplement. */
  {
    a_boolean	saved_pragmas = FALSE;
    if (sym != NULL) {
      if (tssp != NULL) {
        /* A null pointer could be returned if the symbol has an invalid
           kind because of an earlier error. */
        tssp->pragmas_bound_to_template =
                                        decl_state->pragmas_bound_to_template;
        saved_pragmas = TRUE;
      }  /* if */
    }  /* if */
    if (!saved_pragmas) {
      /* An error occurred earlier so we can't attach the pragmas to the
         template, so they need to be discarded. */
      free_pending_pragma_list(decl_state->pragmas_bound_to_template);
    }  /* if */
  }
  {
    /* Save and clear the prototype_type.  After the code below is executed
       prototype_type will only be non-NULL for valid cases where the
       prototype instantiation has actually been done. */
    a_type_ptr	saved_prototype_type = prototype_type;
    prototype_type = NULL;
    if (!decl_state->decl_scope_err && saved_prototype_type != NULL) {
#if CHECKING
      if (sym == NULL || tssp == NULL ||
          tssp->variant.class_template.instantiations == NULL ||
          tssp->variant.class_template.instantiations->
                     variant.class_struct_union.type != saved_prototype_type) {
        internal_error(
                     "template_declaration: sym & prototype_type out of sync");
      }  /* if */
#endif /* CHECKING */
      if (!sym->is_error) {
        /* Do a "prototype instantiation" of the class template -- i.e., parse
           the declarative information looking for gross syntax errors. */
        prototype_type = saved_prototype_type;
        instantiate_class_template(sym, prototype_type, &cache_segments);
        if (tag_resolution) {
          /* This is the resolution of a previously incomplete template
             declaration.  If there are any incomplete instantiations that were
             involved in array type declarations, fix them up now. */
          fixup_types_that_refer_to_incomplete_instantiations(sym,
		                                              prototype_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }
  {
    a_boolean	member_bodies_need_extraction = prototype_type != NULL;
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    /* Member function bodies are extracted before the template string is
       constructed when member function instantiations are included in the
       source sequence lists.  In this mode, member function bodies are
       put out as specializations (by the C++ generating back end, and 
       the function bodies cannot be present in the class template body. */
    if (member_bodies_need_extraction) {
      cache_segments = extract_member_bodies(tssp, cache_segments,
                                            /*functions_only=*/TRUE);
    }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#if RECORD_TEMPLATES_IN_IL
    if (!decl_state->in_prototype_instantiation) {
      complete_il_template_entry(decl_state->il_template_entry, sym,
                                 &decl_state->decl_token_cache,
                                 &decl_state->param_list_cache,
                                 p_template_body_cache);
      /* If this is a template definition or the initial declaration, update
         the template symbol supplement to point to the IL entry . */
      if (tssp != NULL &&
          (decl_state->defines_something || tssp->il_template_entry == NULL)) {
        tssp->il_template_entry = decl_state->il_template_entry;
      }  /* if */
    }  /* if */
#endif /* RECORD_TEMPLATES_IN_IL */
    /* The cache for the template parameter list is no longer needed. */
    discard_token_cache(&decl_state->param_list_cache);
    /* When member function bodies are not extract above, they are done now
       that the template string for the class has been created.  Nested class
       bodies are always extracted at this point. */
    if (member_bodies_need_extraction) {
      (void)extract_member_bodies(tssp, cache_segments,
                                  /*functions_only=*/FALSE);
    } /* if */
  }
  /* If the declaration token cache is not needed, discard it. */
  if (!decl_state->decl_token_cache_used) {
    discard_token_cache(&decl_state->decl_token_cache);
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (sym != NULL) db_symbol(sym, "template symbol: ", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* template_declaration */


static
a_symbol_ptr find_matching_template_instance(a_symbol_ptr      sym,
                                             a_type_ptr        type)
/*
sym is some kind of function symbol.  type is the type declared for a
function template instance;  Return in the symbol for the instance, or
NULL if no instance is found.
*/
{
  a_symbol_ptr  orig_sym;
  a_boolean     any_found = FALSE;
  a_symbol_ptr  sym_found = NULL;
  a_symbol_ptr	new_sym = NULL;

  orig_sym = sym;
  if (sym->is_class_member) {
    /* A member function symbol, find the member function that matches
       the specified type. */
    new_sym = member_function_redecl_sym(sym, type);
    if (new_sym != NULL) any_found = TRUE;
  }  /* if */
  if (!any_found) {
    /* A regular function name that is expected to represent one or
       more function templates.  Loop through the function templates
       and find an instance that matches the specified function type.
       If none exists, a new one can is generated, if possible.  If
       more than one exists (or can be generated) an error is issued. */
    a_boolean		is_list;
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      sym = sym->variant.overloaded_function.symbols;
      is_list = TRUE;
    } else {
      is_list = FALSE;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      a_symbol_ptr	lookup_sym = NULL;
      /* If this is a function template symbol, use it to find a function
         that matches the type we are looking for. */
      if (sym->kind != (a_symbol_kind)sk_function_template) continue;
      lookup_sym = sym;
      /* Look for a match on the list of instantiations. */
      if (lookup_sym != NULL) {
        sym_found = matching_template_function(lookup_sym, type,
                                               /*is_decl_context=*/TRUE);
        if (sym_found != NULL) {
          if (any_found) {
            sym_error(ec_ambiguous_overloaded_function, orig_sym);
            new_sym = NULL;
            break;
          }  /* if */
          any_found = TRUE;
          new_sym = sym_found;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!any_found) {
    sym_error(ec_no_match_for_type_of_overloaded_function, orig_sym);
  }  /* if */
  return new_sym;
}  /* find_matching_template_instance */


a_boolean has_matching_template_instance(a_symbol_ptr      sym,
                                         a_type_ptr        type)
/*
sym is some kind of function symbol.  type is the type declared for a
function template instance;  Return TRUE if one or more function template
symbols under sym (assuming it is an overload set) matches the type specified
by type.
*/
{
  a_boolean	found = FALSE;

  /* sym is a regular function name that is expected to represent one or
     more function templates.  Loop through the function templates
     and find an instance that matches the specified function type. */
  a_boolean		is_list;
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    sym = sym->variant.overloaded_function.symbols;
    is_list = TRUE;
  } else {
    is_list = FALSE;
  }  /* if */
  for (; sym != NULL; sym = is_list ? sym->next : NULL) {
    a_symbol_ptr	lookup_sym = NULL;
    /* If this is a function template symbol, use it to find a function
       that matches the type we are looking for.  If it is a member
       function symbol, get the corresponding function template symbol
       from the function instantiation entry. */
    if (sym->kind != (a_symbol_kind)sk_function_template) continue;
    lookup_sym = sym;
    /* Look for a match on the list of instantiations. */
    if (lookup_sym != NULL) {
      found = has_matching_template_function(lookup_sym, type,
                                             /*is_decl_context=*/TRUE);
      if (found) break;
    }  /* for */
  }  /* if */
  return found;
}  /* has_matching_template_instance */


static void check_for_decl_spec_errors(a_decl_flag_set   dso_flags,
					     a_type_ptr	       type,
					     a_symbol_ptr      sym,
                                             a_symbol_locator  *locator,
					     a_source_position *pos)
/*
Check whether a type specifier is required by the current declaration,
and issue an error if a required specifier was omitted.

dso_flags and type are the values returned from decl_specifiers and
declarator.  sym is the symbol associated with the declarator.  pos
is the position to be used if a diagnostic is issued.
*/
{
  if (!is_error_locator(*locator)) {
    if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
      if (is_function_type(type) && sym != NULL &&
          (is_constructor_symbol(sym) || is_destructor_symbol(sym) ||
           is_conversion_function_symbol(sym))) {
        /* No type specifier is required. */
      } else {
        /* Error on omitted type specifier. */
        pos_diagnostic(es_discretionary_error, ec_missing_type_specifier, pos);
      }  /* if */
    }  /* if */
    if (dso_flags & DSO_DEFINES_SOMETHING) {
      /* The type specifiers included a type definition, which is not allowed
         in this context. */
      pos_error(ec_type_definition_not_allowed, pos);
    }  /* if */
  }  /* if */
}  /* check_for_decl_spec_errors */


static void check_template_nesting_depth(a_symbol_ptr		sym,
					 a_source_position	*pos,
					 a_decl_state_ptr	decl_state)
/*
This routine is used to determine whether the number of template clauses
in a full specialization matches the template nesting depth of the
entity being specialized.  If a mismatch is found, a diagnostic is
issued.
*/
{
  a_template_nesting_depth	depth = 0;
  a_template_arg_ptr		arg_list = NULL;
  a_type_ptr			parent_tp;

  /* Get the symbol for the template on which this entity is based. */
  switch (sym->kind) {
    case sk_class_or_struct_tag:
    case sk_union_tag:
    {
      a_type_ptr	tp;
      tp = sym->variant.class_struct_union.type;
      arg_list = tp->variant.class_struct_union.extra_info->template_arg_list;
      break;
    }
    case sk_member_function:
    case sk_routine:
    {
      a_routine_ptr	rp;
      rp = sym->variant.routine.ptr;
      arg_list = rp->template_arg_list;
      break;
    }
    case sk_static_data_member:
      break;
    default:
      unexpected_condition_str("check_template_nesting_depth: bad sym kind");
  }  /* switch */
  /* The presence of a template argument list indicates that this entity is
     an instance of a class or function template.  A member function of
     a class template or a nested class within a class template will not
     have a template argument list. */
  if (arg_list != NULL) depth++;
  /* Check the parent classes.  Stop if we find a parent class that is
     specialized. */
  parent_tp = sym->is_class_member ? sym->parent.class_type : NULL;
  while (parent_tp != NULL) {
    a_class_type_supplement_ptr	ctsp;
    parent_tp = skip_typerefs(parent_tp);
    /* If the parent class is a specialization, don't search any further.
       The nesting depth is relative to the innermost specialization. */
    if (parent_tp->variant.class_struct_union.is_specialized) break;
    ctsp = parent_tp->variant.class_struct_union.extra_info;
    /* If the enclosing class has a template argument list, increment the
       nesting depth of this entity. */
    if (ctsp->template_arg_list != NULL) depth++;
    /* Process the next enclosing class, if any. */
    parent_tp = parent_tp->source_corresp.is_class_member
                                ? parent_tp->source_corresp.parent.class_type
                                : NULL;
  }  /* while */
  if (depth != decl_state->number_of_template_param_clauses) {
    /* The depths do not match, issue a diagnostic. */
    pos_sy_diagnostic(es_discretionary_error,
                      ec_template_depth_mismatch, pos, sym);
  }  /* if */
}  /* check_template_nesting_depth */


static void full_specialization(a_decl_state_ptr decl_state)
/*
One or more empty template parameter clauses ("template <>") have been
scanned, and this routine handles the specialization of the template instance
that follows.
*/
{
  a_storage_class               storage_class;
  a_type_ptr                    type;
  a_symbol_locator              locator;
  a_decl_flag_set               do_flags, dso_flags, di_flags;
  a_type_qualifier_set          qualifiers;
  a_decl_modifier	        decl_modifiers;
  a_source_sequence_entry_ptr   declarator_ssep;
  a_symbol_ptr		        sym;
  a_func_info_block             func_info;
  a_symbol_reference_kind       srk_flags = SRK_DECLARATION;
  a_source_position             decl_start_pos, id_pos;
  a_boolean                     has_parenthesized_initializer;
  a_source_correspondence       *scp;
  a_routine_ptr                 rp;
  a_variable_ptr                vp;
  a_boolean			is_definition;

  db_enter(3, "full_specialization");
  decl_start_pos = pos_curr_token;
  /* First scan the decl-specifiers. */
  (void)decl_specifiers((DSI_IS_SPECIALIZATION |
                         DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                         DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                         DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_INLINE_ALLOWED |
                         (decl_state->is_member_decl
                                  ? DSI_IS_MEMBER_DECLARATION
                                  : DSI_NO_INPUT_FLAGS)),
                        &dso_flags, &storage_class, &type, &qualifiers,
                        &decl_modifiers);
  if (is_error_type(type) && !is_declarator_start()) {
    /* Error of some sort. */
    set_to_error_locator(locator);
  } else if ((dso_flags & (DSO_DEFINES_SOMETHING |
                           DSO_DECLARES_SOMETHING |
                           DSO_ELABORATED_TYPE_SPECIFIER)) &&
             is_immediate_class_type(type) && curr_token == tok_semicolon) {
    /* The argument is something like class A<int>.  Note that this also
       permits the class to be a nested class within a template class.  All
       of the remaining processing is done in class_specifier. */
    sym = (a_symbol_ptr)type->source_corresp.assoc_info;
    check_assertion(sym != NULL);
    if (storage_class != (a_storage_class)sc_unspecified) {
      /* Storage class is not allowed. */
      pos_error(ec_storage_class_not_allowed, &decl_start_pos);
    }  /* if */
    if (!is_template_instance_class_symbol(sym)) {
      /* Not a template instance. */
      sym_error(ec_entity_cannot_be_specialized, sym);
    } else {
      /* Make sure that this declaration has the correct number of
         "template <>" clauses. */
      check_template_nesting_depth(sym, &decl_start_pos, decl_state);
      type->variant.class_struct_union.is_specialized = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      { a_boolean	is_definition;
        /* The specialization should be marked as an autonomous declaration. */
        is_definition = ((dso_flags & DSO_DEFINES_SOMETHING) != 0);
        set_autonomous_tag_decl_flag(type, is_definition);
        if (!is_definition) {
          (void)set_src_seq_secondary_decl_type((char *)type, type,
                                                /*new_style_spec=*/TRUE);
        }  /* if */
      }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  } else {
    /* Assume the template specialization applies to the declarator, which
       should follow. */
    add_stop_token(tok_semicolon);
    clear_func_info(&func_info);
    func_info.is_inline = ((dso_flags & DSO_INLINE) != 0);
    di_flags = DI_REAL_DECLARATOR_ALLOWED |
               DI_QUALIFIED_NAME_ALLOWED |
               DI_IS_SPECIALIZATION |
               DI_PARENTHESIZED_INITIALIZER_ALLOWED |
               DI_OPERATOR_NAME_ALLOWED;
    if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) &&
        qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
    declarator(di_flags, &do_flags, type, (a_type_ptr)NULL, &locator, &type,
               &declarator_ssep, &func_info);
    sym = NULL;
    has_parenthesized_initializer =
                              (do_flags & DO_PARENTHESIZED_INITIALIZER) != 0;
    if (!is_error_locator(locator)) {
      id_pos = locator.source_position;
      sym = locator.specific_symbol;
      if (sym == NULL) {
        sym = normal_id_lookup(&locator, IDL_NO_OPTIONS);
      }  /* if */
    }  /* if */
    check_for_decl_spec_errors(dso_flags, type, sym, &locator,
                               &decl_start_pos);
    if (is_error_locator(locator)) {
      /* Ignore it. */
    } else if (sym == NULL) {
      /* No symbol, which means the lookup failed. */
      pos_st_error(ec_not_a_template_name, &locator.source_position,
                   locator.symbol_header->identifier);
    } else {
      if (sym->is_class_member &&
          sym->kind == (a_symbol_kind)sk_projection) {
        /* Specifying an inherited name in a template specialization
           declaration is disallowed. */
        pos_error(ec_inherited_member_not_allowed, &locator.source_position);
        reduce_projection_symbol_to_fundamental_symbol(sym);
      }  /* if */
      if (is_function_type(type) && is_function_or_template_symbol(sym)) {
        sym = find_matching_template_instance(sym, type);
        if (sym == NULL) {
          /* No match was found and an error was issued. */
        } else if (sym->variant.routine.instance_ptr == NULL) {
          /* Not a template instance. */
          pos_sy_error(ec_entity_cannot_be_specialized,
                       &locator.source_position, sym);
          sym = NULL;
        } else {
          /* Okay. */
        }  /* if */
      } else if (sym->kind == (a_symbol_kind)sk_static_data_member &&
                 sym->variant.static_data_member.instance_ptr != NULL) {
        if (!types_are_redecl_compatible(type,
                                         sym->variant.static_data_member.
                                                            variable->type)) {
          /* The type of the static data member definition does not match
             the declaration in the class. */
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator.source_position, sym);
          sym = NULL;
        }  /* if */
      } else {
        pos_sy_error(ec_entity_cannot_be_specialized,
                     &locator.source_position, sym);
        sym = NULL;
      }  /* if */
    }  /* if */
    if (sym != NULL) {
      /* Specializations of namespace members can only occur within the
         namespace they belong to or a namespace that encloses it. */
      if (sym->decl_scope != scope_stack[depth_scope_stack].number &&
          (sym->parent.namespace_ptr == NULL ||
           !namespace_is_enclosed_by_curr_scope(sym))) {
        if (!decl_state->decl_scope_err) {
          pos_sy_error(ec_bad_scope_for_specialization,
                       &locator.source_position, sym);
          decl_state->decl_scope_err = TRUE;
        }  /* if */
        sym = NULL;
      }  /* if */
    }  /* if */
    vp = NULL;
    rp = NULL;
    scp = NULL;
    if (sym != NULL) {
      /* Determine whether this entity has already been referenced by
         looking at the source correspondence entry.  An entity that
         has already been referenced cannot be specialized. */
      a_boolean	already_specialized;
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        vp = sym->variant.static_data_member.variable;
        scp = &vp->source_corresp;
        already_specialized = vp->is_specialized;
      } else {
        check_assertion(sym->kind == (a_symbol_kind)sk_routine ||
                        sym->kind == (a_symbol_kind)sk_member_function);
        rp = sym->variant.routine.ptr;
        scp = &rp->source_corresp;
        already_specialized = rp->is_specialized;
      }  /* if */
      /* See if this is a declaration or a definition. */
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        is_definition = (curr_token == tok_assign ||
                         has_parenthesized_initializer);
      } else {
        is_definition = (curr_token == tok_lbrace ||
                         (curr_token == tok_colon &&
                          is_constructor_symbol(sym)));
      }  /* if */
      if (scp->referenced && !already_specialized) {
        pos_sy_error(ec_specialization_of_referenced_entity,
                     &locator.source_position, sym);
        sym = NULL;
      } else if (is_definition && sym->defined) {
        /* The entity has already been defined. */
        pos_sy_error(ec_already_defined, &locator.source_position, sym);
        sym = NULL;
      } else if (!already_specialized) {
        scp->decl_position = id_pos;
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      /* Check for the semicolon. */
      if (curr_token == tok_lbrace) {
        /* This may have been intended to be a function definition.  Flush
           tokens to the closing right brace. */
        flush_until_matching_token();
      } else if (curr_token == tok_assign) {
        /* This may have been intended to be a static data member
           initialization.  Flush to a semicolon. */
        add_stop_token(tok_semicolon);
        flush_tokens();
        remove_stop_token(tok_semicolon);
        (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
      } else {
        (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
      }  /* if */
    } else {
      /* The symbol is not NULL. */
      sym->decl_position = id_pos;
      if (is_definition) srk_flags |= SRK_DEFINITION;
      /* Update cross reference info, etc. */
      record_symbol_declaration(srk_flags, sym, &locator.source_position,
                                declarator_ssep);
      /* Make sure that this declaration has the correct number of
         "template <>" clauses. */
      check_template_nesting_depth(sym, &locator.source_position, decl_state);
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Do fixup on the source sequence entry that was just created to
           represent the current declaration. */
        if (!is_definition) {
          (void)set_src_seq_secondary_decl_type((char *)vp, type,
                                                /*new_style_spec=*/TRUE);
        } else {
          /* The defining declaration of the variable.  Record the type.  */
          if (vp->declared_type == NULL) vp->declared_type = type;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        vp->is_specialized = TRUE;
        if (storage_class != (a_storage_class)sc_unspecified) {
          /* Storage class may not be specified on a member template
             specialization. */
          pos_error(ec_storage_class_not_allowed, &decl_start_pos);
          storage_class = (a_storage_class)sc_unspecified;
        } else if (dso_flags & DSO_INLINE) {
          /* Inline may not be specified. */
          pos_error(ec_inline_and_nonfunction, &decl_start_pos);
        }  /* if */
        /* Deal with initializer. */
        if (is_definition) {
          a_boolean  incomplete_type_error_reported = FALSE;

          sym->variant.static_data_member.variable->
                           storage_class = (a_storage_class)sc_unspecified;
          /* Advance past "=". */
          if (curr_token == tok_assign) (void)get_token();
          initializer(sym, &locator.source_position,
                      (an_id_linkage_kind)idl_external,
                      has_parenthesized_initializer,
                      /*is_old_style_param_decl=*/FALSE,
                      &incomplete_type_error_reported);
        }  /* if */
        (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
      } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Do fixup on the source sequence entry that was just created to
           represent the current declaration. */
        if (!is_definition) {
          (void)set_src_seq_secondary_decl_type((char *)rp, type,
                                                /*new_style_spec=*/TRUE);
        } else {
          /* The defining declaration of the routine.  Record the type.  */
          if (rp->declared_type == NULL) rp->declared_type = type;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        rp->is_specialized = TRUE;
        rp->is_inline = func_info.is_inline;
        if (rp->source_corresp.is_class_member &&
            storage_class != (a_storage_class)sc_unspecified) {
          /* Storage class may not be specified on a member template
             specialization. */
          pos_error(ec_storage_class_not_allowed, &decl_start_pos);
          storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (func_info.is_inline ||
            storage_class == (a_storage_class)sc_static ||
            (scp->is_class_member ?
             (scp->parent.class_type->source_corresp.name_linkage ==
                                      (a_name_linkage_kind)nlk_internal) :
             (scp->parent.namespace_ptr != NULL &&
              (symbol_supplement_for_namespace(scp->parent.namespace_ptr)->
                                                within_unnamed_namespace)))) {
          /* Function was declared "inline" or "static" or is a member of
             an unnamed namespace or of a class that belongs to an unnamed
             namespace. */
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_internal;
        } else {
          rp->storage_class = is_definition? (a_storage_class)sc_unspecified :
                                             (a_storage_class)sc_extern;
          rp->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_cplusplus_external;
        }  /* if */
        if (is_definition) {
          /* This is a defining declaration of the function template. */
          func_info.is_definition = TRUE;
          if (func_info.function_type_from_typedef) {
            /* Just as it is an error when a normal function is defined for
               the function type to come from a typedef, so too is that an
               error when a function template is being defined. */
            error(ec_function_type_must_come_from_declarator);
            /* Copy the type entry, since the typedef type may not be
               shared. */
            type = copy_routine_type_with_param_types(skip_typerefs(type));
            sym->variant.routine.ptr->type = type;
          }  /* if */
          /* Scan the function body. */
          scan_function_body(sym->variant.routine.ptr, &func_info,
                             SFB_NO_FLAGS);
        } else {
          /* Update xref info on param ids. */
          record_param_id_list_declarations(func_info.param_id_list);
          /* No function body, so there ought to be a semicolon following the
             declaration. */
          (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
        }  /* if */
      }  /* if */
    }  /* if */
    done_with_func_info(func_info);
    remove_stop_token(tok_semicolon);
  }  /* if */
  db_exit();
}  /* full_specialization */


static void decl_level_of_template(a_decl_state_ptr decl_state)
/*
Determine the effective declaration scope for a template declaration
in this context.  is_template_friend (in decl_state) is TRUE if this is
a friend declaration and is used only for error checking.  The actual scope to
be used for a friend declaration will be determined later because it
differs between function and nonfunction declarations.
*/
{
  a_scope_depth			depth = decl_scope_level;
  a_scope_stack_entry_ptr	ssep;
  a_boolean			err = FALSE;

  ssep = &scope_stack[depth];
  decl_state->enclosing_scope = ssep->il_scope;
  decl_state->is_member_decl =
                           ssep->kind == (a_scope_kind)sck_class_struct_union;
  if (decl_state->is_member_decl) {
    /* If this template declaration is within a class definition,
       save a pointer to the class in which the definition appears. */
    decl_state->class_declared_in = ssep->assoc_type;
    decl_state->access = ssep->current_access;
  }  /* if */
  if (decl_state->is_member_decl) {
    /* A member template cannot be declared in a local class. */
    if (ssep->inside_local_class && !decl_state->is_template_friend) {
      err = TRUE;
    }  /* if */
  } else if (ssep->kind == (a_scope_kind)sck_file) {
    /* File scope is okay. */
  } else if (ssep->kind == (a_scope_kind)sck_namespace ||
             ssep->kind == (a_scope_kind)sck_namespace_extension) {
    /* Namespace scopes are okay. */
  } else {
    /* Any other scope is not allowed. */
    err = TRUE;
  }  /* if */
  if (err) depth = NO_SCOPE_DEPTH;
  decl_state->effective_decl_level = depth;
  /* Determine whether this is a friend declaration.  For declarations
     inside a class this is determine by inspecting the tokens that
     make up the template declaration. */
  decl_state->is_template_friend = decl_state->is_member_decl &&
                                           is_template_friend_decl(decl_state);
  /* Determine the nesting depth of this template declaration.  Templates
     not enclosed within other templates are given a depth of "1".  The
     depth is incremented for each successive template declaration. */
  decl_state->nesting_depth =
                decl_state->is_template_friend ? 0 : template_nesting_depth();
}  /* decl_level_of_template */


static void template_or_specialization_declaration(
                                      a_boolean  no_advance_past_final_token)
/*
Scan a template declaration of a template specialization declaration.

This routine determines whether the entity being scanned is a "full
specialization".  A full specialization is a declaration that declares
a real function or class and not a template.  In a full specialization
all of the template parameter clauses contain empty template parameter
lists (e.g., "template <>").  Declarations that are not full specializations
are either the specialization of a template or a template declaration.
*/
{
  a_decl_state			decl_state;
  a_def_arg_expr_fixup_ptr	saved_curr_default_args;

  check_assertion_str2(curr_token == tok_template,
                       "template__or_specialization_declaration:",
                       "expected tok_template");
  init_templ_decl_state(&decl_state);
  /* Note that select_curr_construct_pragmas is called in the caller.
     extract_curr_construct_pragmas is called to save the list of
     pragmas associated with this template declaration.  This pragma
     list will later be associated with the template and applied to
     each instance generated from the template. */
  decl_state.pragmas_bound_to_template = extract_curr_construct_pragmas();
  saved_curr_default_args = curr_default_args;
  curr_default_args = NULL;
  decl_state.start_pos = pos_curr_token;
  decl_state.in_prototype_instantiation =
                    scope_stack[depth_scope_stack].in_prototype_instantiation;
  decl_state.no_advance_past_final_token = no_advance_past_final_token;
  /* If there are any pk_immediate pragmas associated with the current
     token, process them now, before the current token is cached, instead
     of in get_token, as is usually done. */
  process_curr_token_pragmas();
  /* Cache the tokens for this declaration.  If this turns out to be
     a function the cache will be saved to generates new routine types
     for this function.  If it is not a function the cache will be
     discarded.  The tokens are cached and a temporary copy of the
     cache is made.  The tokens are scanned in a nonreusable manner
     from the temporary cache.  The last cached token is followed
     immediately by the token that followed it in the original source
     program (i.e., the temporary cache does not contain a terminating
     tok_end_of_source).  The cache is also needed for several different
     kinds of prescans that are done to determine the kind of declaration
     being processed. */
  cache_template_declaration(&decl_state, /*skip_params=*/FALSE);
  decl_level_of_template(&decl_state);
  /* Make sure that this template declaration is permitted in the current
     scope. */
  if (decl_state.is_specialization) {
    /* A specialization declaration is only permitted in a namespace scope. */
    a_scope_stack_entry_ptr ssep = scope_stack_entry_for(depth_scope_stack);
    if ((ssep->kind == (a_scope_kind)sck_file ||
        ssep->kind == (a_scope_kind)sck_namespace ||
        ssep->kind == (a_scope_kind)sck_namespace_extension)) {
      /* A valid template specialization scope. */
    } else {
      error(ec_explicit_specialization_not_in_namespace_scope);
      decl_state.decl_scope_err = TRUE;
    }  /* if */
  } else if (decl_state.effective_decl_level == NO_SCOPE_DEPTH) {
    pos_error(ec_bad_template_declaration_scope, &decl_state.start_pos);
    decl_state.decl_scope_err = TRUE;
  }  /* if */
#if RECORD_TEMPLATES_IN_IL
  if (!decl_state.is_full_specialization &&
      !decl_state.in_prototype_instantiation) {
    /* Create an IL template entry for this declaration.  This is only done
       for template declarations and specializations that are still templates.
       IL entries are not created for templates found during the prototype
       instantiation of other templates because they will be included in
       the template string of the enclosing template. */
    decl_state.il_template_entry =
                               make_il_template_entry(&decl_state.start_pos);
  }  /* if */
#endif /* RECORD_TEMPLATES_IN_IL */
  /* Scan one or more template parameter lists.  Each template parameter
     list looks like "template < param-list >".  The param-list is
     optional (but once a parameter list has been specified, all subsequent
     param-lists must be present). */
  scan_template_param_clauses(&decl_state);
  if (decl_state.is_full_specialization) {
    full_specialization(&decl_state);
    /* Advance past the semicolon or closing rbrace if required. */
    if (!no_advance_past_final_token && (curr_token == tok_semicolon ||
                                         curr_token == tok_rbrace)) {
      (void)get_token();
    }  /* if */
  } else {
    /* The entity being declared is a template. */
    template_declaration(&decl_state);
  }  /* if */
  curr_default_args = saved_curr_default_args;
}  /* template_or_specialization_declaration */


static a_can_instantiate_entry_ptr alloc_can_instantiate_entry(void)
/*
Allocate an entry of a can_instantiate list, initialize it, and return
a pointer to it.
*/
{
  a_can_instantiate_entry_ptr	ciep;

  ciep = (a_can_instantiate_entry_ptr)
                                alloc_fe(sizeof(a_can_instantiate_entry));
  ciep->next = NULL;
  ciep->class_type = NULL;
  return ciep;
}  /* alloc_can_instantiate_entry */


static void add_to_can_instantiate_list(a_type_ptr class_type)
/*
Add an entry to the can_instantiate list.
*/
{
  a_can_instantiate_entry_ptr	ciep;

  ciep = alloc_can_instantiate_entry();
  ciep->class_type = class_type;
  ciep->next = can_instantiate_list;
  can_instantiate_list = ciep;
}  /* add_to_can_instantiate_list */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
static void do_implicit_include_if_needed(a_template_instance_ptr tip)
/*
Gets the name of the file in which the template was declared and attempts
to include a source file (e.g., .c file) that corresponds to the header
of the definition.  If an implicit include was already attempted then
we return without doing anything.  If there is no corresponding source
file we simply return.
*/
{
  a_source_position	*decl_position;
  a_line_number		line_number;
  a_boolean		at_end_of_source;
  unsigned long		nesting_depth;
  a_source_file_ptr	sfp;
  char			*full_file_name, *display_name;
  FILE			*f_source;
  a_boolean		is_system_include;
#if DEBUG
  a_boolean		print_debug_info = FALSE;
#endif /* DEBUG */

  db_enter(3, "do_implicit_include_if_needed");
  /* Translate the sequence number into a file name and line number. */
#if DEBUG
  print_debug_info = debug_level >= 3 || db_flag_is_set("implicit_include");
  if (print_debug_info) {
    fprintf(f_debug, "Attempting implicit include to define:\n");
    db_symbol(tip->instance_sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  decl_position = &tip->template_sym->decl_position;
  sfp = source_file_for_seq(decl_position->seq, &line_number,
                            &at_end_of_source, &nesting_depth,
                            /*physical_line=*/FALSE);
  if (sfp != NULL && sfp != il_header.primary_source_file &&
      sfp->name_as_written != NULL) {
    /* A source file was found and it does not refer to the primary source
       file.  sfp->name_as_written will be NULL if the file name came from
       a #line directive.  In which case the implicit inclusion will not
       be attempted. */
    if (!sfp->related_file_implicit_include_done) {
      /* If we haven't already included the corresponding source file then
         do so now. */
#if DEBUG
      if (print_debug_info) {
        fprintf(f_debug, "  Looking for source file related to '%s'\n",
                sfp->file_name);
      }  /* if */
#endif /* DEBUG */
      sfp->related_file_implicit_include_done = TRUE;
      is_system_include = sfp->included_by_system_include;
      /* Call a routine to search for a file with an appropriate suffix. */
      f_source = open_file_for_input(sfp->name_as_written, 
                                     is_system_include ? sys_incl_search_path :
                                                         incl_search_path,
				     /*replace_suffix=*/TRUE,
				     &full_file_name, &display_name);
      if (f_source != NULL) {
        an_include_file_history_ptr	ifhp;
        /* A related source file was found.  Make sure that the name of the
           file found is not the same as the file we started with.  This
           could occur if the user included a .c file that contains a
           template declaration.  Also make sure that this file has not
           previously been included. */
        if (compare_file_names(full_file_name, sfp->full_name) != 0 &&
            !find_include_history(full_file_name, &ifhp, /*create=*/FALSE)) {
#if DEBUG
          if (print_debug_info || db_flag_is_set("show_implicit_include")) {
            fprintf(f_debug, "  Including text from '%s'\n", full_file_name);
          }  /* if */
#endif /* DEBUG */
          /* Push the new file onto the input stack and scan it.  There is
             no "name as written" so a NULL pointer is passed in. */
	  if (suppress_subsequent_include_of_file(full_file_name, &ifhp)) {
	    (void)fclose(f_source);
#if DEBUG
	    if (print_debug_info) {
	      fprintf(f_debug, "%s %s %s\n", "do_implicit_include_if_needed:",
                      "skipping guarded include file", full_file_name);
            }  /* if */
#endif /* DEBUG */
	  } else {
            push_input_stack(f_source, (char *)NULL, display_name,
                             full_file_name, /*is_include_file=*/FALSE,
                             is_system_include, ifhp);
            scan_implicitly_included_template_definition_file();
	  }  /* if */
        } else {
          /* The file name returned by open_file_for_input is the same as
             the file in which the template was declared.  Just close
             the file. */
          (void)fclose(f_source);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* do_implicit_include_if_needed */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


static void add_to_instantiations_required_list(a_template_instance_ptr  tip)
/*
Add a template instance entry to the end of the instantiatiations_required
list.
*/
{
  db_enter(5, "add_to_instantiations_required_list");
  if (tip->next_in_instantiation_list != NULL ||
      tip == instantiations_required_tail) {
    /* Already on the list -- don't try to add it again. */
    if (in_instantiation_wrapup && tip->instantiation_required) {
      /* The instantiation required flag has been set for an entry already
         on the list.  This means that instantiation_wrapup must make another
         pass over the instantiations list. */
      entries_updated_during_instantiation_wrapup = TRUE;
    }  /* if */
  } else {
    /* The entry must be added to the end of the list.  This is because new
       entries may be placed on the list even after processing on the list
       begins (see instantiation_wrapup). */
    if (instantiations_required == NULL) {
      instantiations_required = tip;
    } else {
      instantiations_required_tail->next_in_instantiation_list = tip;
    }  /* if */
    instantiations_required_tail = tip;
  }  /* if */
  db_exit();
}  /* add_to_instantiations_required_list */


static a_boolean is_inline_template_function(a_template_instance_ptr tip)
/*
Determines whether a template instance pointer refers to a function that
is inline.
*/
{
  a_boolean	result = FALSE;
  if (is_function_symbol(tip->instance_sym)) {
    /* If the is_inline flag is set in the routine entry then the routine
       must be inline whether or not an instantiation has been done or
       whether a template definition has been supplied.  If the routine
       is_inline flag is FALSE then we need to look at the is_inline flags
       associated with the template.  For member functions the is_inline
       flag in the template's routine entry reflects the declaration in
       the class template, whereas the flag in the func_info block reflects
       the function template definition, if any. */
    a_routine_ptr	rout = tip->instance_sym->variant.routine.ptr;
    result =  rout->is_inline;
    if (!result) {
      if (rout->assoc_scope == NULL_region_number) {
        a_template_symbol_supplement_ptr	tssp;
        tssp = template_supplement_for_symbol(tip->template_sym);
        result = tssp->variant.function.routine->is_inline ||
                 tssp->variant.function.func_info.is_inline;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_inline_template_function */


static a_boolean is_static_or_inline_template_function
					(a_template_instance_ptr tip)
/*
Determines whether a template instance pointer refers to a function that
is static or inline (i.e., is not an external function).
*/
{
  a_boolean     result = FALSE;

  if (!is_function_symbol(tip->instance_sym)) {
    /* Must be a static data member. */
  } else if (is_inline_template_function(tip)) {
    result = TRUE;
  } else if (tip->instance_sym->kind != (a_symbol_kind)sk_member_function) {
    /* Only check the storage class of nonmember functions.  The linkage
       of member functions has not been determined yet -- and member
       functions are inline or noninline.  There is no such thing as
       a noninline member function with static storage class.  This
       is only important in tim_none mode.  In all other modes any
       function with the instantiation required flag set will be
       instantiated. */
    a_routine_ptr	rout = tip->instance_sym->variant.routine.ptr;
    result = (rout->storage_class == (a_storage_class)sc_static);
  }  /* if */
  return result;
}  /* is_static_or_inline_template_function */

#if !INSTANTIATION_BY_IMPLICIT_INCLUSION
/*ARGSUSED*/ /* <-- implicit_inclusion_ok is not used if no implicit
                 inclusion. */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
static a_boolean should_be_instantiated
				(a_template_instance_ptr tip,
				 a_boolean	         implicit_inclusion_ok)
/*
Determines whether this template instance needs an instantiation and
generates any errors caused by conflicting instantiation information
such as instantiating a template for which no body was supplied.
implicit_inclusion_ok is TRUE if the compiler should attempt to include
a template definition file to provide definitions for externally linked
template entities.
*/
{
  a_boolean	result = TRUE;
  a_boolean	specialized;
  a_boolean	specialization_defined;
  a_boolean	template_def;

  if (tip->explicit_instantiation ||
      ((tip->instantiation_required || instantiation_mode == tim_all) &&
        (instantiation_mode != tim_none ||
         is_static_or_inline_template_function(tip)))) {
    /* For error checking purposes, find out if a specific definition
       exists and whether a body exists for the template definition. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      a_variable_ptr	vp;
      vp = tip->instance_sym->variant.static_data_member.variable;
      specialized = vp->is_specialized;
      specialization_defined = tip->instance_sym->defined;
      template_def = tip->template_sym->defined;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      if (!template_def && !specialized && implicit_inclusion_ok &&
          implicit_template_inclusion_mode) {
        /* If a template definition is not present, attempt to include a
           source file that will provide the definition.  Then check
           again to see if a template definition is present. */
        do_implicit_include_if_needed(tip);
        template_def = tip->template_sym->defined;
      }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    } else {
      a_template_symbol_supplement_ptr  tssp;
      a_symbol_ptr			template_sym;
      a_routine_ptr	rp;
      rp = tip->instance_sym->variant.routine.ptr;
      specialized = rp->is_specialized;
      specialization_defined = specialized && tip->instance_sym->defined;
      template_sym = tip->template_sym;
      tssp = template_supplement_for_symbol(template_sym);
      template_def = cache_for_template(tssp)->tokens.first_token != NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      if (!template_def && !specialized && implicit_inclusion_ok &&
          implicit_template_inclusion_mode) {
        /* If a template definition is not present, attempt to include a
           source file that will provide the definition.  Then check
           again to see if a template definition is present. */
        do_implicit_include_if_needed(tip);
        template_def = cache_for_template(tssp)->tokens.first_token != NULL;
      }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    }  /* if */
    /* The template should be instantiated if the entity is not specialized
       and a template definition is available. */
    result = !specialized && template_def;
    if (!template_def && !specialization_defined) {
      /* A template can be declared and referenced without ever being defined.
         If, however, an instantiation was explicitly requested an error is
         issued.  The error is not issued if the instantiation was requested
         by an instantiation of the entire class (meaning that all members
         should be instantiated). */
      if (tip->explicit_instantiation && !tip->class_explicitly_instantiated) {
        pos_sy_error(ec_instantiation_requested_no_definition_supplied,
  	           &tip->explicit_instantiation_pos,
  		    tip->instance_sym);
      }  /* if */
    } else {
      /* There is a body or a declared specialization. */
      if (specialized) {
        /* A specialization was declared (but not necessarily defined).
           Simply skip the instantiation unless an instantiation was
           explicitly requested. */
        if (tip->explicit_instantiation) {
          pos_sy_error(ec_instantiation_requested_and_specialized,
  	             &tip->explicit_instantiation_pos, tip->instance_sym);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* No instantiation needed. */
    result = FALSE;
  }  /* if */
  return result;
}  /* should_be_instantiated */


static a_boolean too_many_unused_instantiations
                              (a_symbol_ptr                     template_sym,
                               a_template_symbol_supplement_ptr tssp)
/*
When a function is added to the instantiations required list in
tim_all mode but is not actually required, it is not instantiated
until instantiation wrapup is done, even if it is an inline function.
This is done because these functions may be put on the list before
they can actually be instantiated.  Consequently, the runaway
recursive instantiation check will not detect a loop in which new
"unused" entries get added while instantiating earlier "unused"
entries.  To prevent such loops we set an arbitrary limit to the
number of unused instantiations that can be generated for a given
function.
*/
{
  if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
    /* We are instantiating a function that is not needed.  Update the count
       of unused instantiations.  The count is only incremented for entries
       added in the process of generating other instantiations in an
       attempt to only detect truly recursive instantiations.  It is still
       possible to cause this error to occur in nonrecursive contexts but
       it is very unlikely. */
    tssp->variant.function.unused_instantiations++;
    if (tssp->variant.function.unused_instantiations ==
                                        MAX_UNUSED_ALL_MODE_INSTANTIATIONS) {
      sym_error(ec_too_many_unused_instantiations, template_sym);
    }  /* if */
  }  /* if */
  /* Return TRUE if there have been too many instantiations.  This prevents
     additional instantiations from being generated. */
  return tssp->variant.function.unused_instantiations >=
                                           MAX_UNUSED_ALL_MODE_INSTANTIATIONS;
}  /* too_many_unused_instantiations */


#if AUTOMATIC_TEMPLATE_INSTANTIATION
static an_instance_lookup_entry_ptr alloc_instance_lookup_entry(void)
/*
Allocate an instance lookup entry, initialize it, and return a pointer
to it.
*/
{
  an_instance_lookup_entry_ptr	ilp;

  ilp = (an_instance_lookup_entry_ptr)
                                alloc_fe(sizeof(an_instance_lookup_entry));
  ilp->next = NULL;
  ilp->name = NULL;
  return ilp;
}  /* alloc_instance_lookup_entry */


static an_instance_lookup_entry_ptr find_instance(char		*name,
				                  a_boolean	add)
/*
Find a symbol entry with the specified name.  Add the name to the
list if an entry does not already exist.
*/
{
  register unsigned            hash_value = 0;
  register char                *ptr;
  an_instance_lookup_entry_ptr ilp    = NULL;
  int                          bucket_number;
  int			       length;

  length = strlen(name);
  /* Hash the symbol's name.  This involves taking the name's
     first, last, and middle 3 characters.  Of course, if the name has
     fewer than 5 characters, take the entire name. */
  if (length > 5) {
    ptr = name + (length >> 1) - 1;
    hash_value = (int)*name;
    hash_value = (hash_value * HASH_FACTOR) +
                                             (int)*(name + length - 1);
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr;
  } else {
    register int i;
    ptr = name;
    for (i = 0; i < length; i++) {
      hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    }  /* for */
  }  /* if */

  /* Look in the symbol bucket saving the position in case this symbol needs
     to be added. */
  bucket_number = hash_value % INSTANCE_LOOKUP_TABLE_SIZE;
  if ((ilp = instance_lookup_table[bucket_number]) != NULL) {
    do {
      if (strcmp(name, ilp->name) == 0) {
        /* We have a match. */
        goto symbol_found;
      }  /* if */
    } while ((ilp = ilp->next) != NULL);
  }  /* if */

  /* Exiting this loop indicates that the symbol does not exist in the table;
     allocate a symbol header for it. */
  if (add) {
    ilp = alloc_instance_lookup_entry();
    /* Link the new header onto the front of the appropriate bucket of the
       symbol table. */
    ilp->next = instance_lookup_table[bucket_number];
    instance_lookup_table[bucket_number] = ilp;
    /* Allocate space for the name (including a null terminator) and make a
       copy of the name. */
    ilp->name = (char *)alloc_fe((sizeof_t)length + 1);
    strcpy(ilp->name, name);
  }  /* if */

symbol_found:
  return ilp;
}  /* find_instance */


static a_boolean check_if_present_in_info_file(a_template_instance_ptr tip)
/*
See if the specified instantiation is one that is included in the
instantiation information file.  Return TRUE if it is present.
*/
{
  char		*name;
  a_boolean	found = FALSE;

  if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
    a_variable_ptr	variable;
    variable = tip->instance_sym->variant.static_data_member.variable;
    name = get_mangled_static_data_member_name(variable);
  } else {
    a_routine_ptr	routine;
    routine = tip->instance_sym->variant.routine.ptr;
    name = get_mangled_function_name(routine);
  }  /* if */
  if (find_instance(name, /*add=*/FALSE) != NULL) {
    found = TRUE;
  }  /* if  */
  return found;
}  /* check_if_present_in_info_file */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */


void update_instantiation_required_flag(a_template_instance_ptr tip,
                                        a_boolean               value,
					a_boolean		defer_inline)
/*
Updates the instantiation required flag in a template instance entry.  If the
flag is set to TRUE the instance entry is added to a list of entries for which
instantiation is required.  If the flag is set to FALSE the entry is simply
added to the list.  Once set to TRUE, the flag cannot be reset to FALSE.
Inline functions are instantiated as they are added to the list, unless
defer_inline is TRUE.
*/
{
  a_symbol_ptr			   sym;
  a_template_symbol_supplement_ptr tssp;
  a_boolean			   add_to_list = TRUE;

  db_enter(5, "-update_instantiation_required_flag");
  sym = tip->instance_sym;
  tssp = template_supplement_for_symbol(tip->template_sym);
#if DEBUG
  if (debug_level >= 5) {
    a_symbol_ptr sym = tip->instance_sym;
    fprintf(f_debug, "Setting instantiation_required flag to %s for ",
            value ? "TRUE" : "FALSE");
    db_symbol(tip->instance_sym, "", 0);
    fprintf(f_debug, "is_function_symbol=%d\n", is_function_symbol(sym));
    fprintf(f_debug, "defined=%d\n", sym->defined);
    if (is_function_symbol(sym)) {
      fprintf(f_debug, "inline=%d\n", sym->variant.routine.ptr->is_inline);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  if (instantiation_mode == tim_can_instantiate) {
    /* Leave the instantiation_required flag unchanged in this mode. */
  } else if (instantiation_mode == tim_all && !value) {
    /* An "unused" instantiation is being added to the list. */
    if (too_many_unused_instantiations(tip->template_sym, tssp)) {
      /* Don't add this entry to the list.  This prevents infinite
         instantiation loops. */
      add_to_list = FALSE;
    }  /* if */
  } else if (sym == tip->template_sym) {
      /* Somehow a member function of a nonreal class (e.g., a prototype
         instantiation of a class template) has been referenced.  (This
         can occur in a sizeof operation applied to the address of a
         static member function -- anywhere else?).  Do not instantiate
         the function. */
    add_to_list = FALSE;
  } else if (!value) {
    /* When value is FALSE we still add the entry to the instantiations
       required list. */
    tip->instantiation_required = FALSE;
  } else if (pending_class_definitions != 0) {
    /* A class definition is in progress.  Any nonclass instantiations
       must be deferred until all class definitions are complete.
       Add this instantiation request to the list of deferred
       instantiations. */
    a_symbol_list_entry_ptr	slep;
    slep = alloc_symbol_list_entry();
    slep->symbol = sym;
    /* Add this entry to the end of the deferred instantiations list. */
    if (deferred_instantiations == NULL) deferred_instantiations = slep;
    if (deferred_instantiations_tail != NULL) {
      deferred_instantiations_tail->next = slep;
    }  /* if */
    deferred_instantiations_tail = slep;
  } else {
    a_boolean	flag_already_set = tip->instantiation_required;
    tip->instantiation_required = TRUE;
    if (!flag_already_set) {
      /* Record the namespace from which this instantiation is first used. */
      tip->referencing_namespace =
                  scope_stack[depth_innermost_namespace_scope].assoc_namespace;
    }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (automatic_instantiation_mode) {
      /* Set the instantiation required flag in the routine or variable
         entry. */
      if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
        a_variable_ptr	variable;
        variable = sym->variant.static_data_member.variable;
        variable->instance_required = TRUE;
      } else if (!is_static_or_inline_template_function(tip)) {
        /* A noninline function. */
        a_routine_ptr	routine;
        routine = sym->variant.routine.ptr;
        routine->instance_required = TRUE;
      }  /* if */
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
    if (!defer_inline && is_inline_template_function(tip)) {
      if (!tip->already_instantiated &&
          should_be_instantiated(tip, /*implicit_inclusion_ok=*/FALSE)) {
        /* Inline (member or nonmember) functions are instantiated at the
           point of first use, in case the back end requires the function
           body immediately to perform inlining. */
        instantiate_template_function(tip);
      }  /* if */
    } else if (flag_already_set) {
      /* The instantiation required flag is already set to the desired
         value.  This test is used to ensure that an entry that is already
         on the instantiation required list won't be instantiated until
         reached on the list.  This prevents things on the list from being
         instantiated during the instantiation of other functions of entries
         earlier on the list. */
    } else {
      /* If we are in instantiation wrapup then instantiate the function now
	 instead of just adding it to the end of the list.  This makes
	 it possible to detect runaway recursive instantiations that
	 are very difficult to detect when the instantiations are done
	 serially. */
      if (in_instantiation_wrapup) {
        if (!tip->already_instantiated &&
            should_be_instantiated(tip, /*implicit_inclusion_ok=*/FALSE)) {
          /* Implicit inclusion is not done for "on the fly" instantiations
             because the includes cannot be processed in the middle of
	     the instantiation of another function.  The entry will be put
	     on the instantiation required list and instantiated later in
             instantiation_wrapup. */
          if (tip->instance_sym->kind ==
                                        (a_symbol_kind)sk_static_data_member) {
            define_template_static_data_member(tip);
          } else {
            instantiate_template_function(tip);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (add_to_list) {
    /* The entry is added to the instantiations list even if the instantiation
       required flag is FALSE because certain entries for which instantiation
       is not required need to be processed for automatic instantiation
       processing. */
    add_to_instantiations_required_list(tip);
  }  /* if */
  db_exit();
}  /* update_instantiation_required_flag */


void process_deferred_instantiation_requests(void)
/*
When a class definition is pending, any requests to have functions or
static data members instantiated are deferred until all class definitions
have been completed.  This routine is called when the last class definition
has been completed.  It checks for deferred instantiations and calls
update_instantiation_required_flag to do the appropriate processing.
*/
{
  a_symbol_list_entry_ptr	slep;

  for (slep = deferred_instantiations; slep != NULL; slep = slep->next) {
    a_template_instance_ptr	tip;
    a_symbol_ptr		sym = slep->symbol;
    if (is_function_symbol(sym)) {
      tip = sym->variant.routine.instance_ptr;
    } else {
      check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
      tip = sym->variant.static_data_member.instance_ptr;
    }  /* if */
    update_instantiation_required_flag(tip, /*value=*/TRUE,
                                       /*defer_inline=*/FALSE);
  }  /* for */
  /* Free any list entries that were used. */
  free_list_of_symbol_list_entries(deferred_instantiations);
  deferred_instantiations = NULL;
  deferred_instantiations_tail = NULL;
}  /* process_deferred_instantiation_requests */


#if AUTOMATIC_TEMPLATE_INSTANTIATION
static a_boolean open_instantiation_info_file(void)
/*
Open the instantiation information file associated with the primary source
file.  Return TRUE if the file was successfully opened.
*/
{
  f_instantiation_info = NULL;
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    /* Only open the file if the input is coming from a file.  The name of
       the instantiation information file can be specified on the command
       line.  If none is specified, then a default name is generated. */
    if (ii_file_name != NULL) {
      instantiation_info_file_name = ii_file_name;
    } else {
      instantiation_info_file_name =
            derived_name(primary_source_file_name, INSTANTIATION_FILE_SUFFIX);
    }  /* if */
    f_instantiation_info = fopen(instantiation_info_file_name, "r");
  }  /* if */
  return f_instantiation_info != NULL;
}  /* open_instantiation_info_file */


void create_or_remove_instantiation_information_file(void)
/*
If this compilation made use of any entities that could be instantiated,
create an instantiation information file.  If this compilation did not
make use of any entities that could be instantiated, remove the .ii file
if one already exists.
*/
{
  FILE		*f_ii_file;

  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    /* The name of the instantiation information file should have already
       been determined when the file was opened as part of automatic
       instantiation processing for this file. */
    check_assertion_str2(instantiation_info_file_name != NULL,
                         "create_or_remove_instantiation_information_file:",
                         "file name is NULL");
    /* Only create the file if the input is coming from a file.  Note
       that the file will have been closed after all input was read so
       it must be reopened now. */
    f_ii_file = fopen(instantiation_info_file_name, "r");
    if (f_ii_file != NULL) (void)fclose(f_ii_file);
    if (any_instantiations_required) {
      /* If the file does not exist, create it. */
      if (f_ii_file == NULL) {
        f_ii_file = fopen(instantiation_info_file_name, "a");
        if (f_ii_file == NULL) {
          str_catastrophe(ec_cannot_create_instantiation_information_file,
                          instantiation_info_file_name);
        }  /* if */
      }  /* if */
    } else {
      /* No instantiation information needed.  Delete the file if it
         already exits. */
      if (f_ii_file != NULL) {
        delete_file(instantiation_info_file_name);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* create_or_remove_instantiation_information_file */


static char *read_info_file(void)
/*
Reads a line of input from the instantiation list file.  Returns TRUE if
a line of input is being returned.  Returns FALSE at end-of-file.
*/
{
  register char*    buffer_pos;
  register sizeof_t size = 0;
  register int      ch;
  char              *result;
  static char	    *input_line;
  static sizeof_t   info_file_line_size = 0;

  /* Allocate space into which the input line can be read.  Only done
     the first time this routine is called. */
  if (info_file_line_size == 0) {
    input_line =
            (char *)alloc_general(INFO_FILE_LINE_INCREMENTAL_ALLOCATION);
    info_file_line_size = INFO_FILE_LINE_INCREMENTAL_ALLOCATION;
  }  /* if */
  buffer_pos = input_line;

  while (ch = getc(f_instantiation_info), ch != EOF && ch != '\n') {
    if (++size == info_file_line_size) {
      /* The input line needs to be expanded.  This occurs one character
         before the actual end of the buffer to ensure that there will be
         enough room for the null terminator at the end of the string. */
      sizeof_t  curr_offset;
      sizeof_t	new_size;
      new_size = info_file_line_size + INFO_FILE_LINE_INCREMENTAL_ALLOCATION;
      curr_offset = buffer_pos - input_line;
      input_line = realloc_general(input_line, info_file_line_size, new_size);
      info_file_line_size = new_size;
      buffer_pos = input_line + curr_offset;
    }  /* if */
    *buffer_pos++ = ch;
  }  /* while */
  
  /* Terminate string with a null character. */
  *buffer_pos++ = '\0';

  /* Determine whether to return end-of-file (NULL). */
  result = input_line;
  if (ch == EOF && size == 0) result = NULL;

  return (result);
}  /* read_info_file */


static a_boolean read_instantiation_info_file(void)
/*
Read the list of names from the instantiation information file and
enter the names into a hash table.  Returns TRUE if any entries
were entered in the hash table; otherwise returns FALSE.
*/
{
  char				*line;
  a_boolean			result = FALSE;
  int				i;

  if (open_instantiation_info_file()) {
    /* If the file does not exist, the open routine will return FALSE. */
    /* Skip over initial lines of the instantiation information file
       that don't contain instantiation entries. */
    for (i = 1; i <= INSTANTIATION_INFO_LINES_RESERVED; ++i) {
      /* Read and discard the line. */
      (void)read_info_file();
    }  /* if */
    /* The variable do_auto_instantiation indicates that an instantiation
       list file is present. */
    while ((line = read_info_file()) != NULL) {
      (void)find_instance(line, /*add=*/TRUE);
      result = TRUE;
    }  /* while */
    (void)fclose(f_instantiation_info);
  }  /* if */
  return result;
}  /* read_instantiation_info_file */


static a_boolean can_be_instantiated(a_template_instance_ptr tip)
/*
Determines whether this compilation is capable of generating an
instantiation of a given template instance.
*/
{
  a_boolean	result = TRUE;
  a_boolean	template_def;
  a_boolean	specialized;

  /* For error checking purposes, find out if a specialization declaration
     exists and whether a body exists for the template definition. */
  if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
    a_variable_ptr	vp;
    vp = tip->instance_sym->variant.static_data_member.variable;
    specialized = vp->is_specialized;
    template_def = tip->template_sym->defined;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (!template_def && !specialized && implicit_template_inclusion_mode) {
      /* If a template definition is not present, attempt to include a
         source file that will provide the definition.  Then check
         again to see if a template definition is present. */
      do_implicit_include_if_needed(tip);
      template_def = tip->template_sym->defined;
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  } else {
    a_symbol_ptr		      template_sym;
    a_template_symbol_supplement_ptr  tssp;
    a_routine_ptr		      rp;
    rp = tip->instance_sym->variant.routine.ptr;
    template_sym = tip->template_sym;
    tssp = template_supplement_for_symbol(template_sym);
    specialized = rp->is_specialized;
    template_def = cache_for_template(tssp)->tokens.first_token != NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (!template_def && !specialized && implicit_template_inclusion_mode) {
      /* If a template definition is not present, attempt to include a
         source file that will provide the definition.  Then check
         again to see if a template definition is present. */
      do_implicit_include_if_needed(tip);
      template_def = cache_for_template(tssp)->tokens.first_token != NULL;
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  }  /* if */
  result = template_def && !specialized && !tip->already_instantiated;
  return result;
}  /* can_be_instantiated */


static void automatic_instantiation(void)
/*
Go through the instantiations_required list and look for entries that
were in the instantiation information file.  See if the entry can be
instantiated (and has not already been instantiated).  Instantiate any
entities from the info file list that can be instantiated.
*/
{
  a_template_instance_ptr	tip;
  a_template_instantiation_mode	saved_instantiation_mode;
  a_boolean			can_instantiate;
  a_boolean			check_info_file;

  db_enter(3, "automatic_instantiation");
  /* Set the instantiation mode to tim_none.  This is done to ensure that
     only the instantiations explicitly requested in the list file are
     performed.  We don't want a mode like "used" or "all" to cause
     other instantiations to happen as a consequence of the requested
     instantiations that are performed. */
  saved_instantiation_mode = instantiation_mode;
  instantiation_mode = tim_none;
  /* Read the list of things to be instantiated from the instantiation
     information file. */
  check_info_file = read_instantiation_info_file();
  /* Set the flag that indicates that this compilation includes
     external template entities. */
  any_instantiations_required = instantiations_required != NULL;
  for (tip = instantiations_required;
       tip != NULL; tip = tip->next_in_instantiation_list) {
    /* Call can_be_instantiated.  This is done to force any implicit
       inclusions that may be needed. */
    can_instantiate = can_be_instantiated(tip);
    /* Skip entries that do were not included in the instantiation
       information file. */
    if (!check_info_file || !check_if_present_in_info_file(tip)) continue;
    /* Skip non-external function. */
    if (is_static_or_inline_template_function(tip)) continue;
    /* Skip entries that have already been instantiated. */
    if (tip->already_instantiated) continue;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Automatic instantiation processing for:\n");
      db_symbol(tip->instance_sym, "", 0);
    }  /* if */
#endif /* DEBUG */
    if (can_instantiate) {
      if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
        define_template_static_data_member(tip);
      } else {
        instantiate_template_function(tip);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Restore the original instantiation mode.  This is needed because it
     is used later on in the front end wrapup process when assigning
     linkage class members. */
  instantiation_mode = saved_instantiation_mode;
  db_exit();
}  /* automatic_instantiation */


void update_auto_instantiation_flags(void)
/*
Go through the instantiations_required list and set the fields in the
variable and routine entries that are used to pass information to the
to the link-time instantiation processor.  The "can instantiate" and
"do not instantiate" flags are set here.  The "instance required" flag
is set by update_instantiation_required_flag.
*/
{
  a_template_instance_ptr	tip;

  db_enter(3, "update_auto_instantiation_flags");
  /* Make a pass through all of the instantiations to set the
     flags to be passed to the link time instantiation mechanism.
     This needs to be done after all instantiations have been done
     so that the flags are in their final state. */
  tip = instantiations_required;
  for (; tip != NULL; tip = tip->next_in_instantiation_list) {
    a_symbol_ptr			instance_sym = tip->instance_sym;
    a_routine_ptr			routine;
    a_variable_ptr			variable;
    a_boolean				can_instantiate;
    a_boolean				is_static_data_member;

    /* Skip non-external function. */
    if (is_static_or_inline_template_function(tip)) continue;
    /* Get a pointer to the IL entry to be processed. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      is_static_data_member = TRUE;
      variable = instance_sym->variant.static_data_member.variable;
    } else {
      is_static_data_member = FALSE;
      routine = instance_sym->variant.routine.ptr;
    }  /* if */
    can_instantiate = tip->already_instantiated || can_be_instantiated(tip);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, " already_instantiated=%d\n",
              tip->already_instantiated);
      fprintf(f_debug, " instantiation_required=%d\n",
              tip->instantiation_required);
      fprintf(f_debug, " can_instantiate=%d\n", can_instantiate);
    }  /* if */
#endif /* DEBUG */
    if (is_static_data_member) {
      variable->can_be_instantiated = can_instantiate;
      variable->do_not_instantiate = tip->explicit_do_not_instantiate;
    } else {
      routine->can_be_instantiated = can_instantiate;
      routine->do_not_instantiate = tip->explicit_do_not_instantiate;
    }  /* if */
  }  /* for */
  db_exit();
}  /* update_auto_instantiation_flags */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */


static void delayed_processing_of_can_instantiate_class_pragmas(void)
/*
The can_instantiate pragma is used to let the instantiation routines
know that certain functions or static data members can be instantiated
by a given compilation even if no other references to the entities
are seen.  This is designed to be used in cases such as a library
that internally references certain template classes which are not
used in the external interface of the library (and for which the
library does not provide instantiations).  Without the can_instantiate
pragma the instantiator would have no way of knowing how to generate
the instantiations needed to resolve the references from within the
library.
*/
#if 0
/*
There is currently a problem caused by creating a class in
tim_can_instantiate mode and then referencing the class later.

The current workaround to this problem is to instantiate the
class in tim_none mode.  This has the undesired effect that
any static data members or virtual functions will be flagged
as requiring instantiations.
*/
#endif /* 0 */
{
  a_can_instantiate_entry_ptr	ciep;

  db_enter(4, "delayed_processing_of_can_instantiate_class_pragmas");
#if 0
  /* Temporarily disabled until we solve the problem of classes that
     are referenced after the instantiation is done. */
  instantiation_mode = tim_can_instantiate;
#endif /* 0 */
  ciep = can_instantiate_list;
  while (ciep != NULL) {
    a_type_ptr	class_type = ciep->class_type;
    complete_class_type_is_needed(class_type);
    ciep = ciep->next;
  }  /* while */
  db_exit();
}  /* delayed_processing_of_can_instantiate_class_pragmas */


void instantiation_wrapup(void)
/*
Performs end-of-compilation processing for template instantiation.  An
instantiation will be done if an explicit instantiation has been
requested (i.e., via a pragma) or if an instantiation is required because
the function has been referenced and we are not in "instantiate none"
mode.  Note that the pragma overrides the command line option.  Something
can appear on the list with the instantiation required flag FALSE if, for
instance, a reference that forced instantiation was followed by a
specific definition that made it unnecessary.
*/
{
  a_template_instance_ptr           tip;

  db_enter(3, "instantiation_wrapup");
  /* Now that all input has been processed including any instantiations that
     may be done, process the classes that have been put on the can
     instantiate list. */
  delayed_processing_of_can_instantiate_class_pragmas();

  /* The in_instantiation_wrapup flag indicates that we are generating
     instantiations that were requested earlier in the compilation.  When
     this flag is TRUE new instantiations are generated on the fly instead
     of being added to the end of the list.  This makes it possible to
     detect certain types of recursive instantiations that would otherwise
     be difficult to detect. */
  in_instantiation_wrapup = TRUE;
  do {
    entries_updated_during_instantiation_wrapup = FALSE;
    for (tip = instantiations_required;
         tip != NULL;
         tip = tip->next_in_instantiation_list) {
      if ((instantiation_mode == tim_all || tip->instantiation_required) &&
          !tip->already_instantiated) {
        if (should_be_instantiated(tip, /*implicit_inclusion_ok=*/TRUE)) {
          if (tip->instance_sym->kind ==
                                        (a_symbol_kind)sk_static_data_member) {
            /* Static data member definition. */
            define_template_static_data_member(tip);
          } else {
            /* Function instantiation. */
            instantiate_template_function(tip);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  } while (entries_updated_during_instantiation_wrapup);

#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (automatic_instantiation_mode) {
    /* Do processing related to automatic instantiation processing. */
    automatic_instantiation();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

  in_instantiation_wrapup = FALSE;
  db_exit();
}  /* instantiation_wrapup */


static a_boolean sym_can_be_instantiated(a_symbol_ptr	sym,
				         a_boolean	issue_errors,
                                         a_boolean	is_pragma)
/*
Determine whether the template function specified by sym can be instantiated.
Inline functions and compiler generated routines (which also happen to be
inline) cannot be instantiated.  Pure virtual functions cannot be
instantiated.
*/
{
  a_boolean	result = TRUE;
  a_routine_ptr	routine;

  check_assertion(sym->kind == (a_symbol_kind)sk_routine ||
		  sym->kind == (a_symbol_kind)sk_member_function);
  routine = sym->variant.routine.ptr;
  if (routine->compiler_generated) {
    result = FALSE;
    if (issue_errors) {
      sym_diagnostic(is_pragma ? es_error : es_discretionary_error,
                     ec_compiler_generated_function_cannot_be_instantiated,
                     sym);
    }  /* if */
  } else if (sym->variant.routine.instance_ptr == NULL) {
    /* Not a template function. */
    result = FALSE;
    if (issue_errors) {
      sym_error(ec_not_instantiatable_entity, sym);
    }  /* if */
  } else if (sym->variant.routine.ptr->is_specialized) {
    /* A specialization declaration has been supplied. */
    result = FALSE;
    if (issue_errors) {
      sym_error(ec_instantiation_requested_and_specialized, sym);
    }  /* if */
  } else if (routine->is_inline) {
    /* An inline function is allowed in an explicit instantiation, but not
       in a pragma. */
    result = !is_pragma;
    if (issue_errors) {
      sym_diagnostic(is_pragma ? es_error : es_remark,
                     ec_inline_function_cannot_be_instantiated,
                     sym);
    }  /* if */
  } else if (routine->pure_virtual) {
    result = FALSE;
    if (issue_errors) {
      sym_diagnostic(is_pragma ? es_error : es_discretionary_error,
                     ec_pure_virtual_function_cannot_be_instantiated,
                     sym);
    }  /* if */
  }  /* if */
  return result;
}  /* sym_can_be_instantiated */


static void check_instantiation_scope(a_symbol_ptr sym)
/*
An explicit instantiation is permitted where an explicit specialization
of the template would be permitted, which is to say in a namespace that
is or contains the namespace in which the template was declared.
*/
{
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
  if (!namespace_is_enclosed_by_scope(sym, ssep)) {
    sym_diagnostic(es_discretionary_error,
                   ec_bad_scope_for_explicit_instantiation, sym);
  }  /* if */
}  /* check_instantiation_scope */


static
void update_instantiation_flags(a_symbol_ptr	      sym,
 		                a_pragma_kind	      pragma_kind,
				a_source_position     *pos,
                                a_boolean	      is_class_instantiation,
                                a_boolean	      is_pragma)
/*
Given a pointer to either a routine, member function, or static data member
symbol, set either the instantiation required flag (if instantiate is TRUE)
or the specific definition flag (if instantiate is FALSE).
*/
{
  a_template_instance_ptr	tip = NULL;
  db_enter(3, "update_instantiation_flags");
  if (is_function_symbol(sym)) {
    if (sym_can_be_instantiated(sym, /*issue_errors=*/TRUE, is_pragma)) {
      tip = sym->variant.routine.instance_ptr;
    }  /* if */
  } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    tip = sym->variant.static_data_member.instance_ptr;
  } else {
    unexpected_condition();
  }  /* if */
  if (tip != NULL) {
    a_boolean	instantiation_required_flag;
    if (!is_pragma && tip->explicit_instantiation) {
      /* A template cannot be instantiated more than once using an explicit
         instantiation. */
      sym_diagnostic(es_discretionary_error,
                     ec_multiple_explicit_instantiations, sym);
    }  /* if */
    if (pragma_kind == (a_pragma_kind)pk_instantiate) {
      instantiation_required_flag = TRUE;
      tip->explicit_instantiation = TRUE;
      tip->class_explicitly_instantiated = is_class_instantiation;
      tip->explicit_instantiation_pos = *pos;
    } else if (pragma_kind == (a_pragma_kind)pk_do_not_instantiate) {
      instantiation_required_flag = FALSE;
      tip->explicit_instantiation = FALSE;
      tip->explicit_do_not_instantiate = TRUE;
      tip->class_explicitly_instantiated = FALSE;
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        a_variable_ptr vp = sym->variant.static_data_member.variable;
        vp->is_specialized = TRUE;
        if (is_pragma) vp->specialized_with_old_syntax = TRUE;
      } else {
        a_routine_ptr rp = sym->variant.routine.ptr;
        rp->is_specialized = TRUE;
        if (is_pragma) rp->specialized_with_old_syntax = TRUE;
      }  /* if */
    } else { /* pragma_kind == (a_pragma_kind)pk_can_instantiate */
      /* For the can_instantiate pragma set the instantiation required
         flag to its current value.  The purpose of this is to ensure
         that the entry is on the instantiations required list. */
      instantiation_required_flag = tip->instantiation_required;
      tip->explicit_can_instantiate = TRUE;
    }  /* if */
    /* See if this is a valid scope for the explicit instantiation of this
       entity.  This test is only done for explicit instantiation directives,
       not for pragmas.  When the entire class is being instantiated, the
       check is done only once for the class. */
    if (!is_pragma && !is_class_instantiation) check_instantiation_scope(sym);
    update_instantiation_required_flag(tip, instantiation_required_flag,
                                       /*defer_inline=*/FALSE);
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "Updated instantiation flags for:\n");
    if (sym != NULL) {
      db_symbol(sym, "", 2);
    } else {
      fprintf(f_debug, "<NULL>");
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* update_instantiation_flags */


static void update_instantiation_flags_for_class
					(a_symbol_ptr	        sym,
					 a_pragma_kind          pragma_kind,
					 a_source_position      *pos,
                                         a_boolean		is_pragma,
                                         a_boolean		top_level)
/*
Updates the instantiation flags for all of the member functions and static
data members within a given template class.  top_level is TRUE if
this is the call for the template class itself.  It is FALSE if this
is a recursive call for a class nested within the template class.
*/
{
  a_symbol_ptr	mem_sym;
  a_type_ptr	class_type;

  /* See if this is a valid scope for the explicit instantiation of this
     entity.  This test is only done for explicit instantiation directives,
     not for pragmas. */
  if (!is_pragma && top_level) check_instantiation_scope(sym);
  class_type = sym->variant.class_struct_union.type;
  /* The members of the class will be instantiated. Consider this to
     be a reference of this class. */
  class_type->source_corresp.referenced = TRUE;
  if (pragma_kind == (a_pragma_kind)pk_can_instantiate) {
    /* The can_instantiate pragma is a special case.  Instead of
       processing the class now we simply put the class on a list
       of can instantiate pragmas that will be processed during
       instantiation wrapup. */
    add_to_can_instantiate_list(class_type);
  } else {
    /* Instantiate the class, if not already done. */
    complete_class_type_is_needed(class_type);
    if (is_incomplete_type(class_type)) {
      if (top_level) pos_error(ec_incomplete_type_not_allowed, pos);
    } else {
      mem_sym = sym->variant.class_struct_union.extra_info->symbols;
      /* Loop through all the member symbols looking for member functions. */
      for (; mem_sym != NULL; mem_sym = mem_sym->next_in_scope) {
        a_symbol_ptr	list_sym;
        a_boolean		is_list;
       if (is_member_function_symbol(mem_sym)) {
          /* If this is an overloaded function, loop through each of the
             functions underneath it. */
          if (mem_sym->kind == (a_symbol_kind)sk_overloaded_function) {
            list_sym = mem_sym->variant.overloaded_function.symbols;
            is_list = TRUE;
          } else {
            list_sym = mem_sym;
            is_list = FALSE;
          }  /* if */
          for (;
               list_sym != NULL;
               list_sym = is_list ? list_sym->next : NULL) {
            /* Only set the flags for things that can be instantiated.  The
               test of is_function_symbol excludes function templates from
               this processing. */
            if (is_function_symbol(list_sym) &&
                sym_can_be_instantiated(list_sym, /*issue_errors=*/FALSE,
                                        is_pragma)) {
              update_instantiation_flags(list_sym, pragma_kind, pos,
                                         /*is_class_instantiation=*/TRUE,
                                         is_pragma);
           	}  /* if */
          }  /* for */
        } else if (mem_sym->kind == (a_symbol_kind)sk_static_data_member) {
          update_instantiation_flags(mem_sym, pragma_kind, pos,
                                     /*is_class_instantiation=*/TRUE,
                                     is_pragma);
        } else if (mem_sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
                   mem_sym->kind == (a_symbol_kind)sk_union_tag) {
          /* Instantiate the members of any nested classes. */
          update_instantiation_flags_for_class(mem_sym, pragma_kind, pos,
                                               is_pragma, /*top_level=*/FALSE);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* update_instantiation_flags_for_class */


static a_symbol_ptr sym_if_template_class_member_function(a_symbol_ptr sym)
/*
If sym is a nonoverloaded member function symbol it is simply returned.
If it is an overloaded function symbol we determine if only one of the
overloaded functions is not compiler generated.  If so, we return that
symbol, otherwise we return NULL.
*/
{
  a_symbol_ptr	result_sym = NULL;
  a_symbol_ptr	cowam_sym;

  if (is_member_function_symbol(sym)) {
    /* A member function (possibly overloaded) or non-overloaded
       function.  If this is an overloaded member function, determine
       whether only one of the functions is a user declared
       (i.e., not compiler generated) function.  If so, assume that
       the user declared function is the one intended, otherwise issue
       an error. */

    /* Make sure the resulting symbol is a member of a class that is
       a template class and not a specific definition. */
    cowam_sym = (a_symbol_ptr)sym->parent.class_type->
						source_corresp.assoc_info;
    if (!is_template_class_and_not_specific_def_symbol(cowam_sym)) {
      /* Can't be instantiated -- not a template function. */
    } else {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        result_sym = sym;
      } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* It must be an overloaded member function -- we can't get here
  	   for an overloaded nonmember function. */
        a_symbol_ptr	list_sym;
        a_boolean	any_found = FALSE;
        a_symbol_ptr	new_sym = NULL;
        list_sym = sym->variant.overloaded_function.symbols;
        for (; list_sym != NULL; list_sym = list_sym->next) {
          if (!list_sym->variant.routine.ptr->compiler_generated) {
            if (any_found) {
              /* We have found a second match -- return a NULL. */
              new_sym = NULL;
              break;
            }  /* if */
            any_found = TRUE;
            new_sym = list_sym;
            }  /* if */
        }  /* for */
        result_sym = new_sym;
      }  /* if */
    }  /* if */
  }  /* if */
  return result_sym;
} /* sym_if_template_class_member_function */


#if GENERATE_SOURCE_SEQUENCE_LISTS
static void make_instantiation_directive(a_symbol_ptr                 sym,
                                         a_source_sequence_entry_ptr  ssep,
                                         a_source_position            *pos)
/*
Create an IL entry to represent an instantiation directive.  sym identifies
the entity being instantiated, pos is the source position of the template
keyword, and ssep is the empty source sequence entry that should be used.
*/
{
  an_instantiation_directive_ptr  idp;
  an_il_entry_kind                kind;

  if (!source_sequence_entries_disallowed) {
    idp = alloc_instantiation_directive();
    idp->position = *pos;
    idp->entity.ptr = il_entry_for_symbol(sym, &kind);
    idp->entity.kind = (a_byte_il_entry_kind)kind;
    update_source_sequence_list((char *)idp,
                                 (an_il_entry_kind)iek_instantiation_directive,
                                 ssep);
  }  /* if */
}  /* make_instantiation_directive */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static
void instantiation_directive(a_pragma_kind	kind,
 			     a_boolean		is_pragma,
			     a_source_position	*start_pos)
/*
Processes explicit instantiation requests and those instantiation pragmas
that use the same syntax as the explicit instantiation request.

The syntax of an explicit instantiation is
	template declaration
Where the declaration is either a class-specifier such as
	template class A<int>
or a function declaration.

Note that like all other function declarations a return type of int is
assumed if the return type is omitted.

Pragmas of the form
	#pragma instantiate declaration
	#pragma do_not_instantiate declaration
	#pragma can_instantiate declaration
are also processed by this routine.

If a template class name is used (i.e., A<int>) all of the member functions
and static data members will be instantiated.

kind is the pragma kind being processed.  For an explicit instantiation,
the pragma kind of pk_instantiate is passed by the caller.  is_pragma is
TRUE if this is a pragma and FALSE if it is an explicit instantiation.
*/
{
  a_storage_class               storage_class;
  a_type_ptr                    type;
  a_symbol_locator              locator;
  a_decl_flag_set               do_flags, dso_flags, di_flags;
  a_type_qualifier_set          qualifiers;
  a_decl_modifier	        decl_modifiers;
  a_symbol_ptr                  new_sym;
  a_source_sequence_entry_ptr   declarator_ssep;
  a_symbol_ptr		        sym;
  a_token_kind			end_of_statement_token;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr   ssep;
  a_source_position             template_keyword_pos;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "instantiation_directive");
  if (!is_pragma) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    ssep = add_empty_source_sequence_entry();
    template_keyword_pos = *start_pos;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* By pass "template". */
    (void)get_token();
    *start_pos = pos_curr_token;
  }  /* if */    
  /* If this is a pragma it will end with a tok_newline, if not
     it will end with a semicolon. */
  end_of_statement_token = is_pragma ? tok_newline : tok_semicolon;
  (void)decl_specifiers((DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_IS_EXPLICIT_INSTANTIATION),
                        &dso_flags, &storage_class, &type, &qualifiers,
                        &decl_modifiers);
  if (is_error_type(type) && !is_declarator_start()) {
    /* Error of some sort. */
    set_to_error_locator(locator);
  } else if (curr_token == end_of_statement_token &&
             (dso_flags & DSO_ELABORATED_TYPE_SPECIFIER)) {
    /* The argument is something like class A<int> -- instantiate all the
       members of the class.  Note that this also permits the class to
       be a nested class within a template class. */
    sym = (a_symbol_ptr)type->source_corresp.assoc_info;
    check_assertion(sym != NULL);
    if (is_template_instance_class_symbol(sym) &&
        !is_template_instance_specific_def_symbol(sym)) {
      /* Process all member functions and static data members. */
      update_instantiation_flags_for_class(sym, kind, start_pos, is_pragma,
                                           /*top_level=*/TRUE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (!is_pragma) {
        make_instantiation_directive(sym, ssep, &template_keyword_pos);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Something else -- issue an error. */
      sym_error(ec_not_instantiatable_entity, sym);
    }  /* if */
    goto done;
  } else {
    a_func_info_block  	func_info;
    clear_func_info(&func_info);
    di_flags = DI_REAL_DECLARATOR_ALLOWED |
               DI_QUALIFIED_NAME_ALLOWED |
               DI_IS_EXPLICIT_INSTANTIATION |
               DI_OPERATOR_NAME_ALLOWED;
    if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) &&
        qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
    declarator(di_flags, &do_flags, type, (a_type_ptr)NULL, &locator, &type,
               &declarator_ssep, &func_info);
    record_param_id_list_declarations(func_info.param_id_list);
    done_with_func_info(func_info);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (declarator_ssep != NULL) {
      a_src_seq_sublist_ptr  sublist = NULL;
      remove_from_source_sequence_list(declarator_ssep, &sublist);
      declarator_ssep = NULL;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  /* Look up the identifier scanned in the declarator.  If the
     declarator contains a qualified name it will already have
     been looked up. */
  sym = locator.specific_symbol;
  if (sym == NULL) {
    sym = normal_id_lookup(&locator, IDL_NO_OPTIONS);
  }  /* if */
  check_for_decl_spec_errors(dso_flags, type, sym, &locator, start_pos);
  if (sym == NULL) {
    /* No symbol was found.  If the declarator has a function type
       then say that the name is undefined.  If it was not a function
       type then say it is an invalid pragma argument. */
    if (is_error_locator(locator) ||
        (type != NULL && !is_function_type(type))) {
      pos_error(ec_invalid_instantiation_argument, start_pos);
    } else {
      pos_st_error(ec_undefined_identifier, &locator.source_position,
                   locator.symbol_header->identifier);
    }  /* if */
  } else {
    if (sym->is_class_member &&
        sym->kind == (a_symbol_kind)sk_projection) {
      /* Specifying an inherited name in an explicit instantiation
         directive is disallowed. */
      pos_error(ec_inherited_member_not_allowed, &locator.source_position);
      reduce_projection_symbol_to_fundamental_symbol(sym);
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_static_data_member) {
      if (sym->variant.static_data_member.instance_ptr != NULL) {
        /* A static data member -- set the instantiation flags. */
        if (!types_are_redecl_compatible(type,
                                         sym->variant.static_data_member.
                                                            variable->type)) {
          /* The type of the static data member definition does not match
             the declaration in the class. */
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator.source_position, sym);
        } else {
          update_instantiation_flags(sym, kind, start_pos,
                                     /*is_class_instantiation=*/FALSE,
                                     is_pragma);
#if GENERATE_SOURCE_SEQUENCE_LISTS
          if (!is_pragma) {
            make_instantiation_directive(sym, ssep, &template_keyword_pos);
          }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        }  /* if */
      } else {
        /* A static data member, but of a template class. */
        sym_error(ec_not_instantiatable_entity, sym);
      }  /* if */
    } else if (!is_function_or_template_symbol(sym)) {
      /* Not a function symbol -- issue an error. */
      pos_error(ec_invalid_instantiation_argument, start_pos);
    } else if (!is_function_type(type)) {
      /* The symbol represents a function but the type is not a routine
         type.  This can occur if a declaration contains the name of a
         function but the declaration is not a function declarator. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator.source_position, sym);
    } else {
      /* The symbol found is a function, and the type returned from declarator
         is a function type.  Match this declaration with a previous
         declaration or a template instance. */
      new_sym = find_matching_template_instance(sym, type);
      if (new_sym != NULL) {
        /* Update the flags for the symbol found. */
        update_instantiation_flags(new_sym, kind, start_pos,
                                   /*is_class_instantiation=*/FALSE,
                                   is_pragma);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (!is_pragma) {
          make_instantiation_directive(new_sym, ssep, &template_keyword_pos);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
  }  /* if */
done:;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!is_pragma) {
    if (ssep != NULL && ssep->entity.kind == (a_byte_il_entry_kind)iek_none) {
      a_src_seq_sublist_ptr  sublist = NULL;
      remove_from_source_sequence_list(ssep, &sublist);
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
}  /* instantiation_directive */


void instantiation_pragma(a_pending_pragma_ptr	ppp)
/*
Processes pragmas to request that certain template function(s) or
static data member(s) should be or should not be instantiated.
The pragmas are:

	#pragma instantiate <id or function declaration>
	#pragma do_not_instantiate <id or function declaration>
	#pragma can_instantiate <id or function declaration>

The pragma name can be followed by either a qualified name or a 
complete function declaration.  The name can be something like:

	A<int>
	A<int>::f

If a template class name is used (i.e., A<int>) all of the member functions
and static data members will be instantiated.  If a member name (i.e.,
A<int>::f) is used it must refer to a unique (i.e., not overloaded),
user-defined, non-inline member function or static data member.  The
name may, however, refer to an overloaded function where only one of the
functions is user defined.  This will occur in a class with a user defined
constructor and a compiler generated copy constructor.  In this case the
name A<int>::A may still be used to refer to the one user defined constructor.

Instantiations of function templates and of overloaded member functions
(except for the special case described above) must be done using
complete function declarations such as:

	void A::f(int)
	f(int, const float*)

Note that like all other function declarations a return type of int is
assumed if the return type is omitted.
*/
{
  a_boolean		err = FALSE;
  a_symbol_ptr		sym;
  a_symbol_ptr		new_sym;
  a_source_position	start_pos;
  a_template_instantiation_mode
			saved_instantiation_mode = instantiation_mode;
  a_pragma_kind		pragma_kind;
  a_stop_token_array	save_stop_tokens_array;

  /* The instantiation mode is set to "none" while the pragma processing is
     performed to ensure that no other instantiations are implicitly
     requested as a consequence of scanning the pragma. */
  instantiation_mode = tim_none;
  pragma_kind = ppp->descr_ptr->kind;
  if (pragma_kind == (a_pragma_kind)pk_can_instantiate) {
    if (saved_instantiation_mode == tim_all) {
      /* In tim_all mode the can_instantiate pragma is treated as an
         instantiate pragma. */
      pragma_kind = (a_pragma_kind)pk_instantiate;
    } else {
      pragma_kind = (a_pragma_kind)pk_can_instantiate;
    }  /* if */
  } else if (pragma_kind != (a_pragma_kind)pk_instantiate &&
             pragma_kind != (a_pragma_kind)pk_do_not_instantiate) {
    unexpected_condition();
  }  /* if */
  begin_rescan_of_pragma_tokens(ppp, save_stop_tokens_array);
  begin_deferral_of_access_checks();
  start_pos = pos_curr_token;
  if (is_generalized_identifier_start(GID_NO_OPTIONS) &&
      next_token() == tok_newline) {
    /* An identifier followed by a newline -- this is the simple
       identifier case. */
    sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
						     ilm_normal, &err);
    if (!err) {
      if (sym == NULL) {
        /* Not a currently defined symbol. */
        pos_error(ec_invalid_instantiation_argument, &start_pos);
        err = TRUE;
      } else if (is_template_instance_class_symbol(sym) &&
                 !is_template_instance_specific_def_symbol(sym)) {
        /* Process all member functions and static data members.  This
           kind of directive accepts a template class instance or a
           class nested within a template class. */
	update_instantiation_flags_for_class(sym, pragma_kind, &start_pos,
                                             /*is_pragma=*/TRUE,
                                             /*top_level=*/TRUE);
      } else if ((new_sym = sym_if_template_class_member_function(sym))
								 != NULL) {
	sym = new_sym;
	update_instantiation_flags(sym, pragma_kind, &start_pos,
                                   /*is_class_instantiation=*/FALSE,
                                   /*is_pragma=*/TRUE);
      } else if (sym->kind == (a_symbol_kind)sk_static_data_member &&
                 sym->variant.static_data_member.instance_ptr != NULL) {
	/* A static data member -- set the instantiation flags. */
	update_instantiation_flags(sym, pragma_kind, &start_pos,
                                   /*is_class_instantiation=*/FALSE,
                                   /*is_pragma=*/TRUE);
      } else if (sym->kind == (a_symbol_kind)sk_overloaded_function ||
		 sym->kind == (a_symbol_kind)sk_function_template) {
        /* An overloaded function name or a plain function template name.
	   A full type declaration is required for an overloaded function. */
	sym_error(ec_indeterminate_overloaded_function, sym);
	err = TRUE;
      } else {
        /* Something else -- issue an error. */
        sym_error(ec_not_instantiatable_entity, sym);
	err = TRUE;
      }  /* if */
    }  /* if */
    /* Get the token after the identifier -- it should be a newline. */
    (void)get_token();
  } else if (is_decl_start(/*expr_context=*/FALSE,
                    /*real_declarator_allowed=*/TRUE) ||
             is_declarator_start()) {
    /* This is a declaration-style instantiation pragma, the syntax of
       which is the same as the explicit instantiation directive.  Call
       the explicit instantiation routine to do the processing. */
    add_stop_token(tok_newline);
    instantiation_directive(pragma_kind, /*is_pragma=*/TRUE, &start_pos);
    remove_stop_token(tok_newline);
  } else {
    /* Not an identifier or a declaration. */
    error(ec_invalid_instantiation_argument);
    err = TRUE;
  }  /* if */
  discard_deferred_access_checks();
  end_deferral_of_access_checks();
  /* Stop rescanning tokens from the pragma token cache. */
  wrapup_rescan_of_pragma_tokens(err, save_stop_tokens_array);
  instantiation_mode = saved_instantiation_mode;
}  /* instantiation_pragma */


static void explicit_instantiation(void)
/*
Process an explicit instantiation directive.  Most of the processing is
done by instantiation_directive.  This routine makes sure that the current
scope is a valid one for an instantiation directive and disables any
access errors that were detected.
*/
{
  a_source_position		start_pos;
  a_template_instantiation_mode
	 			saved_instantiation_mode = instantiation_mode;
  a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];

  db_enter(3, "explicit_instantiation");
  /* Pragmas cannot bind to explicit instantiations. */
  cannot_bind_to_curr_construct();
  add_stop_token(tok_semicolon);
  if (ssep->kind != (a_scope_kind)sck_file &&
      ssep->kind != (a_scope_kind)sck_namespace &&
      ssep->kind != (a_scope_kind)sck_namespace_extension) {
    error(ec_explicit_instantiation_not_in_namespace_scope);
    flush_tokens();
  } else {
    /* The instantiation mode is set to "none" while the pragma processing is
       performed to ensure that no other instantiations are implicitly
      requested as a consequence of scanning the pragma. */
    instantiation_mode = tim_none;
    /* Note that the "template" keyword is bypassed in the subroutine. */
    start_pos = pos_curr_token;
    begin_deferral_of_access_checks();
    instantiation_directive((a_pragma_kind)pk_instantiate, /*is_pragma=*/FALSE,
                            &start_pos);
    discard_deferred_access_checks();
    end_deferral_of_access_checks();
  }  /* if */
  remove_stop_token(tok_semicolon);
  instantiation_mode = saved_instantiation_mode;
  db_exit();
}  /* explicit_instantiation */


void template_directive_or_declaration(a_boolean  no_advance_past_final_token)
/*
Scan a template declaration of an explicit instantiation.  This routine
is called to decide whether the current statement is a template
declaration or an explicit instantiation.  It then calls the appropriate
routine.  If no_advance_past_final_token is TRUE, the right brace or
semicolon terminating the template declaration is left as the current token;
otherwise, it is consumed.
*/
{
  an_extern_linkage   saved_linkage;

  db_enter(3, "template_directive_or_declaration");
  if (next_token() == tok_lt) {
    /* The template keyword is followed by a template parameter list.
       This is a template declaration or a specialization using the new
       specialization syntax. */
    /* Save the current default linkage. */
    saved_linkage = def_external_linkage;
    /* Issue an error if this is not C++ linkage. */
    if (def_external_linkage.kind !=
                        (a_name_linkage_kind)nlk_cplusplus_external &&
        !scope_stack[depth_innermost_namespace_scope].
                                            within_unnamed_namespace) {
      pos_error(ec_bad_linkage_for_decl, &pos_curr_token);
      def_external_linkage.kind = (a_name_linkage_kind)nlk_cplusplus_external;
      def_external_linkage.is_explicit = FALSE;
    }  /* if */
    /* Scan the declaration. */
    template_or_specialization_declaration(no_advance_past_final_token);
    /* Restore the linkage. */
    def_external_linkage = saved_linkage;
  } else {
    /* There is no template parameter list, this must be an explicit
       instantiation. */
    explicit_instantiation();
    /* The declaration should terminate with a semicolon. */
    if (no_advance_past_final_token) {
      (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
    } else {
      (void)required_token(tok_semicolon, ec_exp_semicolon);
    }  /* if */
  }  /* if */
  db_exit();
}  /* template_directive_or_declaration */


void templates_one_time_init(void)
/*
One-time initialization for templates.c static variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(instantiations_required),
      pch_saved_var_array_elem(instantiations_required_tail),
      pch_saved_var_array_elem(can_instantiate_list),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* templates_one_time_init */


void templates_init(void)
/*
Initializations for template.
*/
{
  curr_default_args = NULL;
  instantiations_required = NULL;
  instantiations_required_tail = NULL;
  in_instantiation_wrapup = FALSE;
  entries_updated_during_instantiation_wrapup = FALSE;
  can_instantiate_list = NULL;
  deferred_instantiations = NULL;
  deferred_instantiations_tail = NULL;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  any_instantiations_required = FALSE;
  instantiation_info_file_name = NULL;
  f_instantiation_info = NULL;
  memzero((char *)instance_lookup_table, sizeof(instance_lookup_table));
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if RECORD_TEMPLATES_IN_IL
  /* Initialize the output control block for the il-to-str routines. */
  clear_il_to_str_output_control_block(&octl);
  octl.output_str = put_str_to_temp_text_buffer;
  octl.gen_compilable_code = TRUE;
#endif /* RECORD_TEMPLATES_IN_IL */
  /* Allocate a type to be used for template parameter constants whose
     real types cannot be known.  This type will be used for all such
     constants that are created. */
  type_of_unknown_templ_param_constant =
                                    alloc_type((a_type_kind)tk_template_param);
  set_type_size(type_of_unknown_templ_param_constant);
  type_of_unknown_templ_param_constant->variant.template_param.kind = 
                     (a_template_param_type_kind)tptk_type_of_unknown_constant;

}  /* templates_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1996 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
