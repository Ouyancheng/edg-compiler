/*****************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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
#include "folding.h"
#include "trans_corresp.h"
#if AUTOMATIC_TEMPLATE_INSTANTIATION
#include "lower_name.h"
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if USER_CONTROL_OF_STRUCT_PACKING
#include "layout.h"
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if DO_IL_LOWERING && AUTOMATIC_TEMPLATE_INSTANTIATION
#include "lower_il.h"
#endif /* DO_IL_LOWERING && AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATE_EXTERN_INLINE && MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* INSTANTIATE_EXTERN_INLINE && MAINTAIN_NEEDED_FLAGS */
#ifdef lint
/* Include the definition of an_arg_operand to suppress lint errors. */
#include "exprutil.h"
#endif /* ifdef lint */


#if AUTOMATIC_TEMPLATE_INSTANTIATION
typedef struct an_instance_lookup_entry *an_instance_lookup_entry_ptr;
typedef struct an_instance_lookup_entry {
  /* Structure used to represent entries in the hash table of template
     instantiation names.  This is used to match entries from the
     instantiation list file with entries on the compiler's instantiation
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
  a_byte_boolean
		in_request_file;
			/* TRUE if the name is present in the request file. */
  a_byte_boolean
		in_definition_list_file;
			/* TRUE if the name is present in the definition
			   list file. */
} an_instance_lookup_entry;

/*
Enumeration used to specify the kind of template information file line to
be written.
*/
typedef enum /* a_template_info_line_type */ {
  tilt_command_line,		/* Used by driver. */
  tilt_curr_dir,		/* Used by driver. */
  tilt_file_name,		/* Used by driver. */
  tilt_instantiation_flag,
  tilt_instantiation_dir_name,	/* Used by driver. */
  tilt_instantiation_file_name,
  tilt_last
  /* Lint comments to disable warnings that the driver line types are
     not used. */
  /*lint -esym(749,tilt_command_line)*/
  /*lint -esym(749,tilt_curr_dir)*/
  /*lint -esym(749,tilt_file_name)*/
  /*lint -esym(749,tilt_instantiation_dir_name)*/
  /* The Instantiation file name is only used when one instantiation per
     object mode is used. */
  /*lint -esym(749,tilt_instantiation_file_name)*/
} a_template_info_line_type;

/*
The template information line type string to be written to the
file for the various line type kinds.
*/
static char	*template_info_line_type_namess[(int)tilt_last+1] = {
  /* tilt_command_line */		"cmd",
  /* tilt_curr_dir */			"dir",
  /* tilt_file_name */			"fnm",
  /* tilt_instantiation_flag */		"flg",
  /* tilt_instantiation_dir_name */	"idn",
  /* tilt_instantiation_file_name */	"ifn",
  /* tilt_last */			NULL
};

typedef struct a_template_lookup_entry *a_template_lookup_entry_ptr;
typedef struct a_template_lookup_entry {
  /* Structure used to represent entries in the hash table of template
     names.  This is used to record the signatures of template definitions
     specified in a given exported template file so that a definition
     for an exported template can be found when an instantiation must be
     generated. */
  a_template_lookup_entry_ptr
		next;
			/* Pointer to the next template in a given hash
			   table bucket. */
  char		*name;
			/* Name of the template.  This is the name read from
			   the exported template file.  This will typically
			   be the mangled name of the template. */
  an_exported_template_file_ptr
		exported_template_file;
			/* Pointer to the entry that describes the file in
			   which the template was defined. */
} a_template_lookup_entry;

/*
Enumeration used to specify the kind of template information file line to
be written.
*/
typedef enum /* an_exported_template_line_type */ {
  etlt_file_name,
  etlt_template_name,
  etlt_last
} an_exported_template_line_type;

/*
The template information line type string to be written to the
file for the various line type kinds.
*/
static char	*exported_template_line_type_namess[(int)etlt_last+1] = {
  /* etlt_file_name */			"fnm",
  /* etlt_template_name */		"tnm",
  /* etlt_last */			NULL
};

/*
Macro that is TRUE if the instantiation request and/or template information
files should be generated.  This is not done when doing preprocessing
only or when the back end is suppressed.
*/
#define generate_template_files()					\
  (!do_preprocessing_only && !suppress_back_end)

/*
Macro that is TRUE if template instantiation flags should be generated.
*/
#define instantiation_flags_needed()					\
  (automatic_instantiation_mode && !suppress_instantiation_flags)


#define INSTANCE_LOOKUP_TABLE_SIZE 10007
			/* The number of buckets in the instance lookup table.
			   This number should be prime.  The number is large
			   because this table is also used to process the
			   definition list file, which may contain a very
			   large number of symbols. */

static an_instance_lookup_entry_ptr
		instance_lookup_table[INSTANCE_LOOKUP_TABLE_SIZE];
			/* Each element of the array points to a list of
			   entries associated with instantiations that hashed
			   to a given group. */

#define TEMPLATE_LOOKUP_TABLE_SIZE 599
			/* The number of buckets in the template lookup table.
			   This number should be prime. */

static a_template_lookup_entry_ptr
		template_lookup_table[TEMPLATE_LOOKUP_TABLE_SIZE];
			/* Table used to find the definition of exported
			   templates.  Each element of the array points to
			   a list of entries for templates that hash
			   to a given group. */

#define HASH_FACTOR ((unsigned int)73)
			/* The multiplier used in the hash algorithm that
			   generates an index in the hash table from an
                           identifier name string.
			   Do not change without investigating the
			   hash table performance that results.  Prime
			   values are likely to work better than
			   non-prime values. */

static a_boolean
		any_instantiations_required;
			/* TRUE if there are any template instantiations
			   needed for this compilation.  This is TRUE
			   whether or not the instantiations are provided
			   by this file.  This is used to determine whether
			   to create an instantiation request file. */

static char	*instantiation_request_file_name;
                        /* The name of a file containing a list of names
			   of template functions and static data members to
			   be instantiated.  Intended to be used for linker
			   feedback mechanisms to provide automatic
			   instantiation. */

static FILE	*f_instantiation_request;
			/* File from which the instantiation list should be
			   read.  Only valid when do_auto_instantiation is
			   TRUE. */
static a_boolean
		request_file_check_needed;
			/* TRUE if any instantiations have been assigned
			   to this translation unit or if a definition list
			   file is in use.  This means that it
			   is necessary to compare the instances in this
			   translation unit with the list of assigned
			   instantiations and/or with the list of entities
			   in the definition list file.  */

static FILE	*f_template_info;
			/* File variable associated with the template
			   information file. */

static FILE	*f_exported_template;
			/* File variable associated with the exported
			   template file. */

static a_boolean
		any_instantiated_entities_added_to_request_file;
			/* TRUE if any instances have had their add to
			   request flag set and have also been instantiated.
			   This is used to determine whether a list of
			   added entities should be generated at the end of
			   the compilation. */

static a_text_buffer_ptr
		file_read_buffer;
			/* Buffer used when reading from the various template
			   files. */

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

static a_routine_list_entry_ptr
		inline_function_list;
			/* When instantiating extern inline functions,
			   this points to a list of inline functions
			   defined in this translation unit. */

#if CHECKING
static a_boolean
		after_instantiation_wrapup;
			/* TRUE after instantiation wrapup processing has
			   completed. */
#endif /* CHECKING */

static a_boolean
		entries_updated_during_instantiation_wrapup;
			/* TRUE when entries on the instantiation required
			   list have their instantiation required flag
			   set during instantiation wrapup.  This is used to
			   detect situations when an entry that may have
			   already been visited by instantiation wrapup
			   has its instantiation required flag updated
			   while processing an entry later on the list. */

static a_boolean
		implicit_inclusion_done_during_instantiation_wrapup;
			/* TRUE if a file was implicitly included during
			   instantiation wrapup.  An implicit inclusion
			   could make it possible to instantiate some entity
			   that previously could not be instantiated. */

static a_symbol_list_entry_ptr
		exported_templates_list;
			/* List of exported templates whose definitions
			   were provided in this compilation.  This list
			   includes only functions and static data members
			   (i.e., not classes). */

static a_symbol_list_entry_ptr
		exported_templates_tail;
			/* The end of the exported templates list. */

static a_symbol_list_entry_ptr
		deferred_instantiations;
			/* A list of symbol entries for instantiations that
			   were deferred while a class definition was
			   pending.  These entities are instantiated when
			   a class definition is no longer pending. */

static a_symbol_list_entry_ptr
		deferred_instantiations_tail;
			/* The end of the deferred_instantiations list. */

static a_partial_order_candidate_ptr
		avail_partial_order_candidates;
			/* Previously allocated entries available for reuse. */

static a_boolean
		deferred_instantiations_in_process;
			/* Flag used by process_deferred_instantiations to
			   determine whether the routine has already been
			   called and suppress processing during any
			   recursive calls that might occur.  This flag is
			   not a function static so that it can be reset
			   if a compilation is terminated abnormally. */

static unsigned long
		num_total_pending_instantiations;
			/* The number of function instantiations that are
			   in progress at any point in time. */

#if DEBUG
/*
Counters used to track memory usage.
*/
static unsigned long
#if AUTOMATIC_TEMPLATE_INSTANTIATION
		num_template_lookup_entries_allocated,
                num_exported_template_files_allocated,
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
		num_partial_order_candidates_allocated;
#endif /* DEBUG */

#if CHECKING
static a_boolean
		any_friend_state_changed;
			/* TRUE if any template declarations had their friend
			   status changed between the initial scan and the
			   later prescan. */
#endif /* CHECKING */

/*
Structure used to pass information about the current template declaration
between the routines used to implement the processing of template
declarations.
*/
typedef struct a_tmpl_decl_state *a_tmpl_decl_state_ptr;
typedef struct a_tmpl_decl_state {
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
  a_boolean	export_present;
			/* TRUE if the "export" keyword was used on the
			   declaration. */
  a_source_position
		export_position;
			/* If export_present is TRUE, the position of the
			   export keyword. */
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
  a_token_kind	*final_token_ptr;
			/* Pointer to a token kind indicating whether the
			   final token of the declaration is expected to be
			   a semicolon or a right brace. */
  a_template_decl_info_ptr
		decl_info;
			/* Points to the template declaration information
			   associated with the innermost template declaration
			   scope.  Contains NULL for full specializations. */
  a_scope_depth	orig_decl_level;
			/* The scope depth of the scope containing the
			   template declaration. */
  a_scope_depth	effective_decl_level;
			/* The effective declaration scope of the
			   template declaration.  This is normally the
			   to the scope that contains the template
			   declaration, but is the nearest namespace scope
			   for friend declarations. */
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
  a_template_ptr
		il_template_entry;
			/* Pointer to the IL template entry created for this
			   template declaration, or NULL if no entry has been
			   created. */
  a_decl_pos_block
		decl_pos_block;
			/* Source range information for the template
			   declaration. */
  a_symbol_ptr	prototype_scope_symbols;
			/* For a function template declaration, points to the
			   list of prototype scope symbols from the
			   func_info_block. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		definition_range;
			/* Source range information for the template
			   definition (if any). */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_template_decl_ptr
		template_decl;
			/* IL representation of the template parameterization
			   of the entity being declared. */
} a_tmpl_decl_state;

/* Forward declaration. */
static void update_instantiation_required_flag(
					a_template_instance_ptr tip,
                                        a_boolean               value,
					a_boolean		defer_inline);

static void init_templ_decl_state(a_tmpl_decl_state_ptr	tdsp)
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
  tdsp->export_present = FALSE;
  tdsp->export_position = null_source_position;
  tdsp->access = (an_access_specifier)as_public;
  tdsp->nesting_depth = 0;
  tdsp->final_token_ptr = NULL;
  tdsp->decl_info = NULL;
  tdsp->orig_decl_level = NO_SCOPE_DEPTH;
  tdsp->effective_decl_level = NO_SCOPE_DEPTH;
  tdsp->number_of_template_decl_scopes = 0;
  tdsp->number_of_template_param_clauses = 0;
  tdsp->enclosing_scope = NULL;
  tdsp->class_declared_in = FALSE;
  tdsp->start_pos = null_source_position;
  clear_token_cache(&tdsp->param_list_cache, /*reusable=*/TRUE);
  clear_token_cache(&tdsp->decl_token_cache, /*reusable=*/TRUE);
  tdsp->decl_token_cache_used = FALSE;
  tdsp->pragmas_bound_to_template = NULL;
  tdsp->il_template_entry = NULL;
  clear_decl_pos_block(&tdsp->decl_pos_block);
  tdsp->prototype_scope_symbols = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  tdsp->definition_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  tdsp->template_decl = NULL;
}  /* init_templ_decl_state */


static void wrapup_templ_decl_state(a_tmpl_decl_state_ptr decl_state)
/*
Free the token caches that were used while processing a template declaration.
*/
{
  /* If the declaration token cache is not needed, discard it. */
  if (!decl_state->decl_token_cache_used) {
    discard_token_cache(&decl_state->decl_token_cache);
  }  /* if */
  /* Discard the token cache used to store the template parameter list. */
  discard_token_cache(&decl_state->param_list_cache);
}  /* wrapup_templ_decl_state */


/* Forward declaration. */
static void scan_template_param_clauses(
				a_tmpl_decl_state_ptr	decl_state,
				a_boolean		is_template_param);

static a_partial_order_candidate_ptr alloc_partial_order_candidate(void)
/*
Allocate a new partial ordering candidate entry, initialize it,
and return a pointer to it.
*/
{
  a_partial_order_candidate_ptr pscp;

  if (avail_partial_order_candidates != NULL) {
    /* Reuse an existing entry. */
    pscp = avail_partial_order_candidates;
    avail_partial_order_candidates = avail_partial_order_candidates->next;
  } else {
    /* Allocate a new entry. */
    pscp = (a_partial_order_candidate_ptr)
                                   alloc_fe(sizeof(a_partial_order_candidate));
#if DEBUG
   num_partial_order_candidates_allocated++;
#endif /* DEBUG */
  }  /* if */
  pscp->next              = NULL;
  pscp->symbol            = NULL;
  pscp->template_arg_list = NULL;
  
  return pscp;
}  /* alloc_partial_order_candidate */


static void free_partial_order_candidate(a_partial_order_candidate_ptr pscp)
/*
Free a partial ordering candidate entry by returning it to the
list of available entries.
*/
{
  /* Free any template argument list pointed to by this entry. */
  if (pscp->template_arg_list != NULL) {
    free_template_arg_list(pscp->template_arg_list);
  }  /* if */
  pscp->next = avail_partial_order_candidates;
  avail_partial_order_candidates = pscp;
}  /* free_partial_order_candidate */


#if RECORD_TEMPLATE_STRINGS
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
  /* Initialize the buffer that will be used to build the token string.  Use
     the position of the template declaration as the starting position of
     the token string. */
  init_token_string(&template_ptr->source_corresp.decl_position);
  /* The outer loop goes though the three token caches in order, beginning
     with "template < ... >". */
  cache = template_param_list_cache;
  for (;;) {
    check_assertion(cache != NULL);
    /* Add the tokens from this cache to the template string. */
    add_token_cache_to_string(cache);
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

#endif /* RECORD_TEMPLATE_STRINGS */

static a_template_ptr make_il_template_entry(a_tmpl_decl_state_ptr decl_state)
/*  
Allocate an IL template entry.  Don't add the entry to the templates list
of its scope: the appropriate scope is not known for sure yet, since this
may be a friend template.
*/
{
  a_template_ptr  tp;

  db_enter(3, "make_il_template_entry");
  tp = alloc_template();
  tp->source_corresp.decl_position = decl_state->start_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  tp->export_position = decl_state->export_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (depth_scope_stack == depth_innermost_namespace_scope) {
    /* Set the source-sequence insert point for instantiations to NULL -- no
       instantiations should be inserted before it. */
    reset_ss_list_instantiation_insert_point();
  }  /* if */
  /* There's not yet a name or symbol for the template declaration, so call
     update_source_sequence_list directly. */
  add_to_source_sequence_list((char *)tp, (an_il_entry_kind)iek_template);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return tp;
}  /* make_il_template_entry */

#if GENERATE_SOURCE_SEQUENCE_LISTS

a_src_seq_secondary_decl_ptr
                            secondary_src_seq_for_template(a_template_ptr  tp)
/*
Turn the source sequence entry pointing to the given template into a
secondary source sequence entry and return a pointer to that secondary
source sequence entry.
*/
{
  a_source_sequence_entry_ptr  ssep = tp->source_corresp.source_sequence_entry;
  a_src_seq_secondary_decl_ptr sssdp;

  sssdp = alloc_src_seq_secondary_decl();
  sssdp->entity = ssep->entity;
  sssdp->decl_position = tp->source_corresp.decl_position;
  ssep->entity.ptr = (char *)sssdp;
  ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
  return sssdp;
}  /* secondary_src_seq_for_template */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if AUTOMATIC_TEMPLATE_INSTANTIATION

/* Declaration needed because of forward references. */
static void write_to_template_info_file(
				a_template_info_line_type	line_type,
				char				*string,
				char				*flags_string);


static void generate_template_file_names(void)
/*
Generate a name for the template information file and instantiation
request file if names were not provided on the command line.
*/
{
  if (template_info_file_name == NULL) {
    if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
      /* A file name was specified on the command line, but no template
         information file was specified on the command line.  Use a name
         based on the primary source file name. */
      template_info_file_name = 
            derived_name(primary_source_file_name, TEMPLATE_INFO_FILE_SUFFIX);
    } else {
      /* If the input is coming from standard input and no template information
         file name was specified, use a default value.  This should be
         supplied by the driver, so this is only intended for testing
         purposes. */
      template_info_file_name = "default.ti";
    }  /* if */
  }  /* if */
  if (exported_template_file_name == NULL) {
    if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
      /* A file name was specified on the command line, but no exported
         template file was specified on the command line.  Use a name
         based on the primary source file name. */
      exported_template_file_name = 
         derived_name(primary_source_file_name, EXPORTED_TEMPLATE_FILE_SUFFIX);
    } else {
      /* If the input is coming from standard input and no exported template
         file name was specified, use a default value.  This should be
         supplied by the driver, so this is only intended for testing
         purposes. */
      exported_template_file_name = "default.et";
    }  /* if */
  }  /* if */
  /* The name of the instantiation request file can be specified on the
     command line.  If none is specified, then a default name is
     generated.  This file is only used when input is coming from a file. */
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    if (ii_file_name != NULL) {
      instantiation_request_file_name = ii_file_name;
    } else {
      instantiation_request_file_name =
          derived_name(primary_source_file_name, INSTANTIATION_FILE_SUFFIX);
    }  /* if */
  }  /* if */
}  /* generate_template_file_names */


static void open_template_info_file(void)
/*
Open the template information file.
*/
{
  a_boolean	cannot_open;
  a_boolean	bad_name;

  check_assertion_str2(use_template_info_file, "open_template_info_file:",
                      "use_template_info_file is FALSE");
  check_assertion_str2(generate_template_files(), "open_template_info_file:",
                      "generate_template_files() is FALSE");
  /* Open a file in which the list of generated file names will be
     returned. */
  f_template_info = open_output_file(template_info_file_name,
                                     /*binary_file=*/FALSE,
                                     /*update_mode=*/FALSE,
                                     &cannot_open, &bad_name);
  if (bad_name) {
    str_catastrophe(ec_invalid_output_file, template_info_file_name);
  } else if (cannot_open) {
    str_catastrophe(ec_cannot_open_output_file, template_info_file_name);
  }  /* if */
}  /* open_template_info_file */


static void write_to_template_info_file(
				a_template_info_line_type	line_type,
				char				*string,
				char				*flags_string)
/*
Write a line to the template information file.  line_type specifies
the kind of line to be written.  string specifies the value to
be written.  flags_string is either NULL or points to a string of flags
associated with this information file line.
*/
{
  if (f_template_info == NULL) {
    open_template_info_file();
  }  /* if */
  fprintf(f_template_info, "%s:%s",
          template_info_line_type_namess[(int)line_type], string);
  if (flags_string) {
    fprintf(f_template_info, ":%s", flags_string);
  }  /* if */
  fputs("\n", f_template_info);
}  /* write_to_template_info_file */


static void close_or_remove_template_info_file(void)
/*
If this compilation made use of any entities that could be instantiated,
ensure that the template information file has been created, and then
close the file.  If this compilation did not make use of any entities that
could be instantiated, remove the template information  file if one
already exists.
*/
{
  if (f_template_info != NULL) {
    /* Close the file if it is open. */
    if (fclose(f_template_info)) {
      str_catastrophe(ec_file_write_error, "template information file");
    }  /* if */
  }  /* if */
  if (!any_instantiations_required || total_errors != 0) {
    /* If there were no instantiations, delete any old version of the
       template information file.  The file is also deleted if any
       errors occurred during this compilation. */
    if (is_regular_file(template_info_file_name)) {
      delete_file(template_info_file_name);
    }  /* if */
  }  /* if */
  f_template_info = NULL;
}  /* close_or_remove_template_info_file */


static void close_or_remove_exported_template_file(void)
/*
If any entries were written to the exported template definition file,
close it now.  If no entries were written, remove any file that might
have already existed.
*/
{
  if (f_exported_template != NULL) {
    /* Close the file if it is open. */
    if (fclose(f_exported_template)) {
      str_catastrophe(ec_file_write_error, "exported template file");
    }  /* if */
  }  /* if */
  if (f_exported_template == NULL || total_errors != 0) {
    /* If there were no entries written to the exported template file,
       delete any old version of the file.  The file is also deleted if any
       errors occurred during this compilation. */
    if (is_regular_file(exported_template_file_name)) {
      delete_file(exported_template_file_name);
    }  /* if */
  }  /* if */
  f_exported_template = NULL;
}  /* close_or_remove_exported_template_file */


static void open_exported_template_file_for_output(void)
/*
Open the template information file.
*/
{
  a_boolean	cannot_open;
  a_boolean	bad_name;

  check_assertion_str2(generate_template_files(),
                       "open_exported_template_file_for_output:",
                       "generate_template_files() is FALSE");
  /* Open the file into which information about exported template will
     be written. */
  f_exported_template = open_output_file(exported_template_file_name,
                                         /*binary_file=*/FALSE,
                                         /*update_mode=*/FALSE,
                                         &cannot_open, &bad_name);
  if (bad_name) {
    str_catastrophe(ec_invalid_output_file, exported_template_file_name);
  } else if (cannot_open) {
    str_catastrophe(ec_cannot_open_output_file, exported_template_file_name);
  }  /* if */
}  /* open_exported_template_file_for_output */


static void write_to_exported_template_file(
				an_exported_template_line_type	line_type,
				char				*string)
/*
Write a line to the exported template file.  line_type specifies
the kind of line to be written.  string specifies the value to
be written.
*/
{
  if (f_exported_template == NULL) {
    open_exported_template_file_for_output();
  }  /* if */
  fprintf(f_exported_template, "%s:%s",
          exported_template_line_type_namess[(int)line_type],
          string);
  fputs("\n", f_exported_template);
}  /* write_to_exported_template_file */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

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
    /* Function instantiation entries are put on the list, but are not
       marked for actual instantiation (that is, for generation of the
       function body) at this time.  That will happen if/when there
       is an invocation of the function, except if the function is virtual,
       in which case it is marked for instantiation when a constructor
       or destructor is defined for the class, i.e., when it is determined
       that a virtual function table might be put out.  All instances are
       placed on the instantiation list.  In tim_all mode the instantiations
       will be generated even if the instantiation required flag is not set. */
    rout = ctsp->assoc_scope->routines;
    while (rout != NULL) {
      sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
      tip = sym->variant.routine.instance_ptr;
      if (tip == NULL) {
        /* Under certain conditions the instance pointer will be NULL.  This
           occurs for compiler generated routines and under some error
           conditions.  Simply skip this routine. */
      } else if (rout->is_prototype_instantiation) {
        /* Don't add prototype instantiations of member templates to the
           instantiations required list. */
      } else if (!tip->instantiation_required) {
        /* Simply add the function to the instantiation list, without setting
           the flag. */
        a_boolean	flag_value = FALSE;
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
        { a_routine_ptr	templ_rout;
          /* The instantiation required flag is set for virtual functions
             when generating class template instantiation information in the
             source sequence lists.  This is necessary when using the C++
             generating back end in this mode because inline virtual functions
             must have definitions. */
          templ_rout = sym->variant.routine.ptr;
          if (templ_rout->is_virtual && templ_rout->is_inline) {
            flag_value = TRUE;
          }  /* if */
        }
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
        set_instance_required(sym, flag_value, /*defer_inline=*/TRUE);
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      /* Add a secondary source sequence entry to represent the partial
         instantiation -- it will take the form of an explicit
         specialization.  This entry is not needed if the class of which
         this function is a member will itself be put out as a
         specialization, because that definition will include declarations
         of all the member functions. */
      /* Don't do this for things like generated copy constructors. */
      if (!rout->compiler_generated) {
        a_type_ptr  declared_type = rout->declared_type;

        if (declared_type == NULL || declared_type == rout->type) {
          /* Make a copy of the routine type; default_args, if any, will be
             ignored. */
          declared_type = copy_routine_type_with_param_types(
                                                  rout->type,
                                                  /*copy_default_args=*/FALSE);
        } else {
          /* Use the declared_type in the routine entry only if it has no
             default args; otherwise, make a copy. */
          declared_type = routine_type_without_default_args(declared_type);
        }  /* if */
        add_source_sequence_entry_for_partial_instantiation(
                                                (char *)rout,
                                                (an_il_entry_kind)iek_routine,
                                                declared_type);
      }  /* if */
#endif /* !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      rout = rout->next;
    }  /* while */
    
    /* Static data members are eligible for a compiler-generated definition
       only if a template definition appears in the source.  However, it
       still needs to appear on the instantiation-required list (because
       instantiation is required somewhere in the program even if
       not in the current translation unit).  The instantiation of a static
       data member is required only if the static data member is referenced. */
    var = class_type->variant.class_struct_union.extra_info->
							assoc_scope->variables;
    while (var != NULL) {
      sym = (a_symbol_ptr)var->source_corresp.assoc_info;
      tip = sym->variant.static_data_member.instance_ptr;
      /* We makes sure tip is non-NULL to guard against potential error
         cases. */
      if (tip != NULL && !tip->instantiation_required) {
        set_instance_required(sym, /*value=*/FALSE, /*defer_inline=*/TRUE);
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      /* Add a secondary source sequence entry to represent the partial
         instantiation -- it will take the form of an explicit
         specialization.  This entry is not needed if the class of which
         this is a static data member will itself be put out as a
         specialization, because that definition will include declarations
         of all the static data members. */
      { a_type_ptr  declared_type = var->declared_type;

        if (declared_type == NULL) declared_type = var->type;
        add_source_sequence_entry_for_partial_instantiation(
                                           (char *)var,
                                           (an_il_entry_kind)iek_variable,
                                           declared_type);
      }
#endif /* !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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


a_template_arg_ptr templ_arg_list_for_class(a_type_ptr class_type)
/*
Given a class type, return the template argument list to be used when
generating an instantiation.  This is usually the normal template
argument list, but if the class was generated from a partial specialization,
it is the partial specialization template argument list.
*/
{
  a_template_arg_ptr		arg_list;
  a_class_type_supplement_ptr	ctsp;

  ctsp = class_type->variant.class_struct_union.extra_info;
  /* Return the partial specialization template argument list, if one is
     present.  Otherwise return the primary template argument list. */
  arg_list = ctsp->partial_spec_template_arg_list;
  if (arg_list == NULL) arg_list = ctsp->template_arg_list;
  return arg_list;
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


a_symbol_ptr primary_template_of(a_symbol_ptr sym)
/*
If sym is a partial specialization, return the primary template.  Otherwise,
just return sym.  If the symbol provided is NULL, return a NULL symbol
pointer.
*/
{
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				result_sym;

  if (sym != NULL) {
    check_assertion(sym->kind == (a_symbol_kind)sk_class_template);
    tssp = sym->variant.template_info;
    result_sym = tssp->variant.class_template.primary_template_sym != NULL
                     ? tssp->variant.class_template.primary_template_sym
                     : sym;
  } else {
    result_sym = NULL;
  }  /* if */
  return result_sym;
}  /* primary_template_of */


static a_symbol_ptr template_for_instance(a_symbol_ptr sym)
/*
sym is a pointer to an instance of a template.  Return a pointer to the
template symbol from which the instance was generated.
*/
{
  a_symbol_ptr	template_sym;
  template_sym = sym->variant.class_struct_union.extra_info->class_template;
  check_assertion(template_sym != NULL &&
                  template_sym->kind == (a_symbol_kind)sk_class_template);
  return template_sym;
}  /* template_for_instance */


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
    if (sym != NULL) {
      /* Make sure that the tokens sequence number of the template matches
         the one we are looking for.  If not, it is probably a partial
         specialization. */
      tssp = sym->variant.template_info;
      if (tssp->token_sequence_number != curr_token_sequence_number) {
        /* Check each of its partial specializations. */
        for (sym = tssp->variant.class_template.partial_specializations;
             sym != NULL; sym = sym->next) {
          tssp = sym->variant.template_info;
          if (tssp->token_sequence_number == curr_token_sequence_number) break;
        }  /* for */
      }  /* if */
    }  /* if */
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


static a_boolean all_templ_params_have_values(
				a_template_arg_ptr	templ_arg_list,
				a_template_param_ptr	templ_param_list)
/*
This routine is used after doing argument deduction for a template
argument list.  Its purpose is to make sure that a value has been deduced
for each parameter.
*/
{
  a_boolean		result = TRUE;
  a_template_param_ptr	tpp;
  a_template_arg_ptr	tap;

  tpp = templ_param_list;
  tap = templ_arg_list;
  for (; tpp != NULL; tpp = tpp->next, tap = tap->next) {
    a_boolean	arg_okay = FALSE;
    if (tap == NULL) {
      /* This argument is invalid. */
    } else if (is_type_templ_arg(tap)) {
      /* A type argument -- the argument is okay if the type has been
         filled in. */
      arg_okay = tap->variant.type != NULL;
    } else if (is_nontype_templ_arg(tap)) {
      /* A nontype argument -- the argument is okay if the constant has
         been filled in or if it is an array bound of unknown type. */
      arg_okay = (tap->is_array_bound_of_unknown_type ||
                  tap->variant.constant != NULL);
    } else {
      /* A template template argument -- the argument is okay if the template
         has been filled in. */
      arg_okay = tap->variant.templ != NULL;
    }  /* if */
    if (!arg_okay) {
      result = FALSE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* all_templ_params_have_values */


static a_boolean wrapup_template_argument_deduction(
				a_template_arg_ptr   templ_arg_list,
                                a_symbol_ptr         rout_templ_sym,
                                a_template_param_ptr templ_param_list)
/*
This routine is used after doing argument deduction for each argument to
ensure that any nontype parameter whose type depends on a
template parameter is consistent with the deduced value.  Also,
types are supplied for nontype parameters that are deduced entirely
from array bounds.  templ_param_list is the template parameter list to
be used.  If a NULL pointer is provided, the template parameter list
from the template symbol supplement is used.  The parameter is supplied
because some calls of this routine occur before the field in the
template symbol supplement has been set.  The templ_param_list is also
passed explicitly when this routine is used to check the nontype
template arguments of a partial specialization of a class template.  In
such cases, the rout_templ_sym field is NULL.  For partial specializations
the only tests that are needed are the check that all parameters have
values, and the handling of array bounds of unknown type.
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
  match = all_templ_params_have_values(templ_arg_list, templ_param_list);
  if (match) {
    tpp = templ_param_list;
    tap = templ_arg_list;
    for (; tpp != NULL; tpp = tpp->next, tap = tap->next) {
      a_type_ptr	constant_type;
      /* Only nontype parameters need to be processed. */
      if (!is_nontype_templ_arg(tap)) continue;
      if (tpp->variant.constant.type_involves_template_param) {
        /* Rescan the tokens that make up the parameter declaration. */
        check_assertion(rout_templ_sym != NULL);
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
          set_unsigned_integer_constant(
                   constant, (a_host_large_unsigned)tap->variant.integer_value,
                   skip_typerefs(constant_type)->variant.integer.int_kind);
          tap->variant.constant = constant;
          tap->is_array_bound_of_unknown_type = FALSE;
        }  /* if */
      } else {
        /* The template argument has a deduced value with a type.  The
           type must match the declared type.  This test is only needed if
           the type involves a template parameter. */
        check_assertion(tap->variant.constant != NULL);
        if (tpp->variant.constant.type_involves_template_param) {
          check_assertion(rout_templ_sym != NULL);
          match = identical_types(constant_type, tap->variant.constant->type);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return match;
}  /* wrapup_template_argument_deduction */


a_type_ptr wrapup_function_template_argument_deduction(
				a_template_arg_ptr   templ_arg_list,
                                a_symbol_ptr         rout_templ_sym,
                                a_template_param_ptr templ_param_list)
/*
Calls wrapup_template_argument_deduction and then produces a final
routine type by substituting the completed template arguments into the
template routine type.  The new routine type is returned.  If an error
occurred in the substitution process, a NULL pointer is returned.
*/
{
  a_type_ptr	new_type = NULL;

  if (wrapup_template_argument_deduction(templ_arg_list, rout_templ_sym,
                                         templ_param_list)) {
    new_type = substitute_template_arguments(rout_templ_sym, templ_arg_list,
                                             (a_template_arg_ptr*)NULL,
                                             templ_param_list);
  }  /* if */
  return new_type;
}  /* wrapup_function_template_argument_deduction */


static a_boolean parameter_is_more_specialized(
				a_type_ptr		param_type1,
				a_type_ptr		param_type2,
				a_template_arg_ptr	*templ_arg_list,
				a_template_param_ptr	templ_param_list)
/*
This routine is used by function_template_is_more_specialized to call
matches_template_type for each parameter of a function template.  Before
calling matches_template_type some transformations are done to the parameter
types to remove things that are not relevant to the partial ordering
comparison (such as top level references).
*/
{
  a_boolean	result;

  /* Remove any qualifiers that are present. */
  param_type1 = skip_typerefs(param_type1);
  param_type2 = skip_typerefs(param_type2);
  /* Remove any top level references. */
  if (is_reference_type(param_type1)) {
    param_type1 = type_pointed_to(param_type1);
  }  /* if */
  if (is_reference_type(param_type2)) {
    param_type2 = type_pointed_to(param_type2);
  }  /* if */
  result = matches_template_type(param_type1, param_type2, templ_arg_list,
                                 templ_param_list, MTT_NO_FLAGS);
  return result;
}  /* parameter_is_more_specialized */


static a_boolean function_template_is_more_specialized(
				a_symbol_ptr 		templ_sym1,
				a_symbol_ptr		templ_sym2)
/*
templ_sym1 and templ_sym2 are function template symbols.  Return TRUE if
templ_sym1 is more specialized than templ_sym2.  This means that for
an instance that matches both templates, templ_sym1 should be preferred
over templ_sym2.
*/
{
  a_boolean				result = TRUE;
  a_template_symbol_supplement_ptr	tssp1;
  a_template_symbol_supplement_ptr	tssp2;
  a_routine_ptr				rout1;
  a_routine_ptr				rout2;
  a_type_ptr				rout_type1;
  a_type_ptr				rout_type2;
  a_routine_type_supplement_ptr		rtsp1;
  a_routine_type_supplement_ptr		rtsp2;
  a_param_type_ptr			ptp1;
  a_param_type_ptr			ptp2;
  a_template_param_ptr			templ_param_list;
  a_template_arg_ptr			dummy_arg_list = NULL;
  a_boolean				is_conversion_operator;

  check_assertion_str2(
                  templ_sym1->kind == (a_symbol_kind)sk_function_template &&
                  templ_sym2->kind == (a_symbol_kind)sk_function_template,
                  "function_template_is_more_specialized:", "bad symbol kind");
  tssp1 = template_supplement_for_symbol(templ_sym1);
  tssp2 = template_supplement_for_symbol(templ_sym2);
  rout1 = tssp1->variant.function.routine;
  rout2 = tssp2->variant.function.routine;
  rout_type1 = skip_typerefs(rout1->type);
  rout_type2 = skip_typerefs(rout2->type);
  rtsp1 = rout_type1->variant.routine.extra_info;
  rtsp2 = rout_type2->variant.routine.extra_info;
  /* Get the parameter list to be deduced.  This is the one for the second
     template. */
  templ_param_list = tssp2->variant.function.decl_cache.decl_info->parameters;
  is_conversion_operator = is_conversion_function_symbol(templ_sym1);
  if (is_conversion_operator) {
    /* For conversion templates, the processing is only done on the return
       type. */
    result = parameter_is_more_specialized(
                                      rout_type1->variant.routine.return_type,
                                      rout_type2->variant.routine.return_type,
                                      &dummy_arg_list, templ_param_list);
  } else {
    /* For normal functions, the processing is done for each parameter, but
       not for the return type. */
    ptp1 = rtsp1->param_type_list;
    ptp2 = rtsp2->param_type_list;
    /* Do the argument deduction on each function parameter.  The loop will
       terminate when one of the parameter lists has been exhausted.  If
       the two functions differ in the number of parameters, which can
       occur if one of the functions has default arguments, base
       the decision on the common parameters. */
    for (; ptp1 != NULL && ptp2 != NULL;
           ptp1 = ptp1->next, ptp2 = ptp2->next) {
      if (!parameter_is_more_specialized(ptp1->type, ptp2->type,
                                         &dummy_arg_list,
                                         templ_param_list)) {
        /* Stop when a mismatch is found. */
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (result) {
    /* Each of the arguments match.  Now make sure that all arguments were
       deduced, and that nontype arguments have the correct types. */
    result = FALSE;
    if (wrapup_function_template_argument_deduction(
               dummy_arg_list, templ_sym2, templ_param_list) != NULL) {
      result = TRUE;
    }  /* if */
  }  /* if */
  /* Free the template argument list produced by the deduction process. */
  if (dummy_arg_list != NULL) free_template_arg_list(dummy_arg_list);
  return result;
}  /* function_template_is_more_specialized */


int compare_function_templates(a_symbol_ptr 		templ_sym1,
			       a_symbol_ptr		templ_sym2)
/*
templ_sym1 and templ_sym2 are function template symbols.  Return 1 if
templ_sym1 is more specialized than templ_sym2, return -1 if templ_sym2 is
more specialized than templ_sym1, and return 0 if they are unordered.
*/
{
  a_boolean	templ1_is_more_specialized;
  a_boolean	templ2_is_more_specialized;
  int		result;

  templ_sym1 = fundamental_symbol_of(templ_sym1);
  templ_sym2 = fundamental_symbol_of(templ_sym2);
  templ1_is_more_specialized =
                 function_template_is_more_specialized(templ_sym1, templ_sym2);
  templ2_is_more_specialized =
                 function_template_is_more_specialized(templ_sym2, templ_sym1);
  if (templ1_is_more_specialized && !templ2_is_more_specialized) {
    result = 1;
  } else if (templ2_is_more_specialized && !templ1_is_more_specialized) {
    result = -1;
  } else {
    result = 0;
  }  /* if */
  return result;
}  /* compare_function_templates */


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
  } else {
   a_symbol_kind	sym_kind= tpp->param_symbol->kind;
    if (sym_kind == (a_symbol_kind)sk_type) {
      depth = tpp->variant.type->
                         variant.template_param.extra_info->coordinates.depth;
    } else if (sym_kind == (a_symbol_kind)sk_constant) {
      depth = tpp->variant.constant.ptr->
                   variant.template_param.variant.coordinates.depth;
    } else {
      /* A template template parameter. */
      depth = tpp->variant.templ->il_template_entry->coordinates.depth;
    }  /* if */
  }  /* if */
  return depth;
}  /* nesting_depth_of_template_param */


/* Forward declaration. */
static a_boolean matches_template_arg_list(
				a_template_arg_ptr	tap,
				a_template_arg_ptr	templ_tap,
				a_template_arg_ptr	*templ_arg_list,
				a_template_param_ptr	templ_param_list);

static a_boolean matches_partial_specialization(
				a_symbol_ptr		template_sym,
				a_symbol_ptr		instance_sym,
				a_template_arg_ptr	*ps_arg_list)
/*
Determine whether the template instance specified by instance_sym
matches the partial specialization indicated by template_sym.  Return
TRUE if it does; otherwise return FALSE.  If a match is found, return
the template argument list with respect to the partial specialization
in ps_arg_list.
*/
{
  a_boolean				result = FALSE;
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				prototype_sym;
  a_type_ptr				prototype_type;
  a_type_ptr				instance_type;
  a_template_param_ptr			templ_param_list;
  a_template_arg_ptr			local_arg_list;
  a_boolean				local_arg_list_used = FALSE;
  
  /* Get a pointer to the prototype instantiation associated with this
     partial specialization.  Then get the template argument list from
     the prototype instantiation. */
  tssp = template_sym->variant.template_info;
  prototype_sym = tssp->variant.class_template.prototype_instantiation;
  prototype_type = type_symbol_type(prototype_sym);
  instance_type = type_symbol_type(instance_sym);
  /* Get the template parameter list associated with this partial
     specialization. */
  templ_param_list = tssp->cache.decl_info->parameters;
  /* If no template argument list was provided by the caller, use a local
     one.  This is the case when the caller doesn't care about the
     argument list. */
  if (ps_arg_list == NULL) {
    ps_arg_list = &local_arg_list;
    local_arg_list = NULL;
    local_arg_list_used = TRUE;
  }  /* if */
  if (matches_template_type(instance_type, prototype_type, ps_arg_list,
                            templ_param_list, MTT_NO_FLAGS)) {
    if (wrapup_template_argument_deduction(
                        *ps_arg_list, (a_symbol_ptr)NULL, templ_param_list)) {
      a_type_ptr			test_type;
      a_boolean				copy_error = FALSE;
      /* Substitute the template parameters of the template with the deduced
         arguments.  We should end up with the original type.  This main
         purpose of this test is to make sure that template parameters in
         nondeduced contexts yield the expected types once substituted. */
      test_type = copy_type_with_substitution(prototype_type,
                                              *ps_arg_list, templ_param_list,
					      &template_sym->decl_position,
					      CTWS_PROTOTYPE_ALLOWED,
					      &copy_error);
      if (!copy_error && identical_types(instance_type, test_type)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!result || local_arg_list_used) {
    /* If no match was found, free the template argument list that was
       created, if any.  The argument list is also freed if the caller
       does not want one returned. */
    free_template_arg_list(*ps_arg_list);
  }  /* if */
  return result;
}  /* matches_partial_specialization */


static a_boolean is_more_specialized(
				a_symbol_ptr 		template_sym1,
				a_symbol_ptr		template_sym2)
/*
templ_sym1 and templ_sym2 are class template symbols for partial
specializations of a template.  Return TRUE if templ_sym1 is more
specialized than templ_sym2.  This means that, for an instance that
matches both templates, templ_sym1 should be preferred over templ_sym2.
*/
{
  a_boolean				result = FALSE;
  a_symbol_ptr				prototype_sym1;
  a_template_symbol_supplement_ptr	tssp1;
 
  /* Use the argument deduction routines to determine whether the template
     parameters used in template2 can be deduced from the values used in
     template1.  If so, then template1 is more specialized than template2.
     For example:
       1. template <class T> struct A<T**> {};
       2. template <class T> struct A<T*> {};
     In this example, the T in template2 can be deduced from template1.
     The deduced value is T*.  So, template1 is more specialized than
     template1. */
  tssp1 = template_sym1->variant.template_info;
  prototype_sym1 = tssp1->variant.class_template.prototype_instantiation;
  result = matches_partial_specialization(template_sym2, prototype_sym1,
                                          (a_template_arg_ptr*)NULL);
  return result;
}  /* is_more_specialized */


void add_to_partial_order_candidates_list(
			a_partial_order_candidate_ptr	*psc_list,
			a_symbol_ptr			new_sym,
			a_template_arg_ptr		templ_arg_list)
/*
Add the partial specialization or function template specified by
new_sym to the candidates list pointed to by psc_list.  If the new
entry is a poorer match than an entry already on the list, don't add
it.  Go through the existing list and remove any entries that are
poorer candidates than the new entry. templ_arg_list is the template
argument list associated with new_sym, and is only supplied when the
templates being ordered are class template partial specializations.
*/
{
  a_partial_order_candidate_ptr	prev_pscp = NULL;
  a_partial_order_candidate_ptr	next_pscp;
  a_partial_order_candidate_ptr	pscp;
  a_boolean			do_not_add = FALSE;
  a_symbol_ptr			fund_new_sym;
  a_symbol_ptr			fund_curr_sym;

  fund_new_sym = fundamental_symbol_of(new_sym);
  for (pscp = *psc_list; pscp != NULL;  pscp = next_pscp) {
    a_boolean	new_is_more_specialized;
    a_boolean	curr_is_more_specialized;
    next_pscp = pscp->next;
    fund_curr_sym = fundamental_symbol_of(pscp->symbol);
    if (fund_new_sym->kind == (a_symbol_kind)sk_class_template) {
      new_is_more_specialized = is_more_specialized(fund_new_sym,
                                                    fund_curr_sym);
      curr_is_more_specialized = is_more_specialized(fund_curr_sym,
                                                     fund_new_sym);
    } else {
      int	result;
      check_assertion(fund_new_sym->kind ==
                                         (a_symbol_kind)sk_function_template);
      result = compare_function_templates(fund_new_sym, fund_curr_sym);
      new_is_more_specialized = result == 1;
      curr_is_more_specialized = result == -1;
    }  /* if */
    if (new_is_more_specialized && !curr_is_more_specialized) {
      /* The new entry is more specialized than the one already on the
         list.  Remove the entry from the list.
         Remove the entry from the list. */
      if (prev_pscp == NULL) {
        *psc_list = pscp->next;
      } else {
        prev_pscp->next = pscp->next;
      }  /* if */
      /* Free the entry.  This also frees the template argument list. */
      free_partial_order_candidate(pscp);
    } else {
      prev_pscp = pscp;
      if (curr_is_more_specialized && !new_is_more_specialized) {
        /* The new entry is not more specialized than the one on the list.
           Set a flag that indicates that this entry should not be added
           to the list. */
        do_not_add = TRUE;
      }  /* if */
    }  /* if */
  }  /* for */
  if (!do_not_add) {
    /* Add the new entry to the front of the list. */
    a_partial_order_candidate_ptr	new_pscp;
    new_pscp = alloc_partial_order_candidate();
    new_pscp->symbol = new_sym;
    new_pscp->template_arg_list = templ_arg_list;
    new_pscp->next = *psc_list;
    *psc_list = new_pscp;
  } else {
    /* If we are not adding the entry to the list, free the template
       argument list. */
    if (templ_arg_list != NULL) free_template_arg_list(templ_arg_list);
  }  /* if */
}  /* add_to_partial_order_candidates_list */


void select_best_partial_order_candidate(
			a_partial_order_candidate_ptr	psc_list,
			a_symbol_ptr			instance_sym,
			a_symbol_ptr			*best_sym,
			a_template_arg_ptr		*best_arg_list,
			a_boolean			*p_ambiguous)
/*
Return the best partial specialization symbol and its associated
template argument list.  There should only be one entry
left on the list, unless there is an ambiguity.  Return the first
entry on the list.  If there are multiple entries, issue an error
if the candidates are partial specializations, and set *p_ambiguous
to TRUE.
*/
{
  a_partial_order_candidate_ptr	pscp;
  a_partial_order_candidate_ptr	next_pscp;
  a_boolean			ambiguous = FALSE;

  /* Return the information from the first entry on the list. */
  *best_sym = psc_list->symbol;
  *best_arg_list = psc_list->template_arg_list;
  if (psc_list->next != NULL) {
    /* There is more than one entry on the list -- issue an error if the
       entries are partial specializations. */
    ambiguous = TRUE;
    if ((*best_sym)->kind == (a_symbol_kind)sk_class_template) {
      pos_sy_start_error(ec_ambiguous_partial_spec, &error_position,
                         instance_sym);
      for (pscp = psc_list; pscp != NULL; pscp = pscp->next) {
        /* The prototype instantiation for the partial specialization is used
           in the diagnostic because it includes the template argument list
           of the  partial specialization. */
        sym_add_diag_info(ec_ambiguous_partial_spec_add_on,
                          pscp->symbol->variant.template_info->
                               variant.class_template.prototype_instantiation);
      }  /* for */
      end_error();
    }  /* if */
  }  /* if */
  /* Clear the template argument list pointer in the first entry to prevent
     it from being freed below. */
  psc_list->template_arg_list = NULL;
  for (pscp = psc_list; pscp != NULL; pscp = next_pscp) {
    next_pscp = pscp->next;
    /* Free the entry.  This also frees the template argument list. */
    free_partial_order_candidate(pscp);
  }  /* for */
  if (p_ambiguous != NULL) *p_ambiguous = ambiguous;
}  /* select_best_partial_order_candidate */


static a_symbol_ptr check_partial_specializations(
				a_symbol_ptr		instance_sym,
				a_type_ptr		class_type,
				a_symbol_ptr		template_sym)
/*
instance_sym identifies a template class that is about to be instantiated.
template_sym points to the primary template on which the instantiation
will be based.  class_type points to the class associated with instance_sym.
If a matching partial specialization is found, return the symbol associate
with that partial specialization; otherwise return NULL.
*/
{
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				matching_sym = NULL;
  a_symbol_ptr				ps_sym;
  a_class_type_supplement_ptr		ctsp;
  a_partial_order_candidate_ptr		candidate_list = NULL;

  db_enter(3, "check_partial_specializations");
  tssp = template_sym->variant.template_info;
  /* Get the template argument list with respect to the primary template. */
  ctsp = class_type->variant.class_struct_union.extra_info;
  for (ps_sym = tssp->variant.class_template.partial_specializations;
       ps_sym != NULL; ps_sym = ps_sym->next) {
    a_template_arg_ptr	ps_arg_list = NULL;
    if (matches_partial_specialization(ps_sym, instance_sym,
                                       &ps_arg_list)) {
      add_to_partial_order_candidates_list(&candidate_list,
                                           ps_sym, ps_arg_list);
    }  /* if */
  }  /* for */
  if (candidate_list != NULL) {
    /* A partial specialization was found.  Update the instance to record
       the template argument list with respect to the partial
       specialization.  If more than one match was found, this routine
       will report the ambiguity. */
    select_best_partial_order_candidate(candidate_list, instance_sym,
                                        &matching_sym,
                                        &ctsp->partial_spec_template_arg_list,
                                        (a_boolean*)NULL);
  }  /* if */
  db_exit();
  return matching_sym;
}  /* check_partial_specializations */


a_namespace_ptr determine_referencing_namespace(void)
/*
Determine the referencing namespace for a template that is to be
instantiated.  For a template that is not instantiated because of a
reference within another template, this is the nearest enclosing
namespace.  For a template that is instantiated because of a reference
within another template, this is the referencing namespace of the
enclosing template.
*/
{
  a_namespace_ptr	result = NULL;
  a_scope_depth		depth;
  a_symbol_ptr		instance_sym = NULL;

  for (depth = depth_scope_stack; depth >= 0; depth--) {
    a_scope_stack_entry_ptr	ssep = scope_stack_entry_for(depth);
    /* Find a template instantiation scope with an associated instance
       symbol.  Some scopes, such as scopes for the partial instantiation
       of a template function, do not have instance symbols and are ignored. */
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      instance_sym = ssep->instance_sym;
      if (instance_sym != NULL) break;
    }  /* if */
  }  /* for */
  if (instance_sym == NULL) {
    /* Either no instantiation scopes, or only instantiation scopes
       with NULL instance symbols.  Just use the innermost namespace
       scope. */
    result = scope_stack[depth_innermost_namespace_scope].assoc_namespace;
  } else if (is_class_struct_union_symbol(instance_sym)) {
    /* The entity is a class.  Get the referencing namespace from the
       class symbol supplement. */
    result = instance_sym->variant.class_struct_union.extra_info->
                                                        referencing_namespace;
  } else {
    /* The entity is a function or static data member.  Get the referencing
       namespace from the template instance. */
    a_template_instance_ptr	tip;
    if (is_function_symbol(instance_sym)) {
      tip = instance_sym->variant.routine.instance_ptr;
    } else {
      check_assertion(instance_sym->kind ==
                                        (a_symbol_kind)sk_static_data_member);
      tip = instance_sym->variant.static_data_member.instance_ptr;
    }  /* if */
    check_assertion(tip != NULL);
    result = tip->referencing_namespace;
  }  /* if */
  return result;
}  /* determine_referencing_namespace */


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

This routine should be called from macro instantiate_template_class,
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
  cssp->referencing_namespace = determine_referencing_namespace();
  template_sym = template_symbol_for_class_symbol(instance_sym);
  if (template_sym == NULL) {
    /* Not a class based on a class template. */
  } else if (class_type->variant.class_struct_union.is_nonreal_class) {
    /* Don't try to instantiate a template class without real template
       arguments. */
  } else if (class_type->variant.class_struct_union.is_specialized) {
    /* This is an attempt to instantiate an incomplete type that is
       a specific definition.  This can occur in error cases while scanning
       the class definition.  Simply ignore the instantiation request. */
  } else {
    a_template_cache_ptr	body_cache;
    tssp = template_supplement_for_symbol(template_sym);
    /* Check whether this particular instance should be generated from a
       partial specialization.  This is only done for class templates, not
       normal nested classes of class templates. */
    if (template_sym->kind == (a_symbol_kind)sk_class_template &&
        tssp->variant.class_template.partial_specializations != NULL) {
      a_symbol_ptr		partial_spec_sym = NULL;
      partial_spec_sym = check_partial_specializations(
                                       instance_sym, class_type, template_sym);
      if (partial_spec_sym != NULL) {
        template_sym = partial_spec_sym;
        tssp = template_supplement_for_symbol(template_sym);
      }  /* if */
    }  /* if */
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
    body_cache = cache_for_template(tssp_of_prototype);
    /* There is a class template from which to generate this class and it is
       a real instantiation. */
    /* Update the class symbol supplement pointer that points to the
       prototype instantiation.  Instances of a class template can sometimes
       be created before this is known.  Furthermore, even if it was set
       it may need to be revised if the instantiation is generated from
       a partial specialization. */
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
    } else if (tssp->pending_instantiations >= max_pending_instantiations) {
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
      if (template_sym->kind == (a_symbol_kind)sk_class_template) {
        /* If this is an instance of a class template (as opposed to a
           nested class of a class template) update the class_template
           pointer to reflect the template from which the instance was
           generated.  This will be different from the previous value
	   when a partial specialization is used. */
        cssp->class_template = template_sym;
      }  /* if */
      /* Update the type_kind of the instance with the type_kind from the
         template.  Ordinarily, this will have already been done when the
         partial instantiation was done.  But, for partial specializations
         the type kind of the partial specialization may be different than
         that of the primary template. */
      class_type->kind = tssp->variant.class_template.type_kind;
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
      push_template_instantiation_scope(body_cache->decl_info,
					class_type,
					(a_routine_ptr)NULL,
					instance_sym, template_sym,
					template_arg_list,
                                        /*push_stop_tokens=*/TRUE,
                                        PS_NO_OPTIONS);
      /* Reactivate any pragmas that should be bound to the generated
         instance. */
      reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
      /* The tokens of the template definition have been cached away.
         Activate the cache so that they can be rescanned in light of
         the new values associated with the template parameters. */
      rescan_reusable_cache(&body_cache->tokens);
#if CHECKING
      if (curr_token != tok_lbrace && curr_token != tok_colon) {
        internal_error("f_instantiate_template_class: bad 1st token in cache");
      }  /* if */
#endif /* CHECKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("full instantiation of \"", f_debug);
        db_type_name(class_type);
        fputs("\":\n", f_debug);
      }  /* if */
#endif /* DEBUG */
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
                    /*is_template_instantiation=*/TRUE,
                    (a_template_ptr)NULL,
                    (a_decl_pos_block_ptr)NULL);
      pending_class_definitions--;
      set_instantiation_required_for_template_class_members(class_type);
      /* Process any pragmas that are to be bound to this instance. */
      process_curr_construct_pragmas(instance_sym, (a_statement_ptr)NULL);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      /* A template instantiation is considered to always be "autonomous",
         even if its instantiation happens to be triggered by a reference
         in the declaration of another entity. */
      class_type->autonomous_primary_tag_decl = TRUE;
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Pop the template instantiation scope. */
      pop_template_instantiation_scope();
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      check_for_and_remove_redundant_secondary_decl_ss_entry(class_type);
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
    fprintf(f_debug, "Entry %d\n", count);
    fprintf(f_debug, "Symbol: ");
    db_symbol(tcsp->symbol, "", 6);
    fprintf(f_debug, "  first_token_number: %lu\n", tcsp->first_token_number);
    fprintf(f_debug, "  last_token_number: %lu\n", tcsp->last_token_number);
    fprintf(f_debug, "  before_first_token: %p\n",
            (void*)tcsp->before_first_token);
    fprintf(f_debug, "  last_token: %p\n", (void*)tcsp->last_token);
    fprintf(f_debug, "\n");
  }  /* for */
}  /* db_template_cache_segments */
#endif /* DEBUG */


static
a_template_cache_segment_ptr map_token_numbers_to_cache_pointers(
			a_template_cache_ptr			tcp,
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
  a_cached_token_ptr		next_prev_ctp = NULL;
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
      check_assertion(tcsp->first_token_number >= prev_tsn);
      prev_tsn = tcsp->first_token_number;
    }  /* for */
  }
#endif /* CHECKING */

  for (ctp = tcp->tokens.first_token;
       ctp != NULL; prev_ctp = next_prev_ctp, ctp = ctp->next) {
    /* Stop searching if there are no more entries to be processed. */
    if (curr_tcsp == NULL && start_found_list == NULL) break;
    if (ctp->extra_info_kind != (a_token_extra_info_kind)teik_pragma) {
      /* Don't use a pragma entry as a previous token. */
      next_prev_ctp = ctp;
    }  /* if */
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
         (ctp->next != NULL && ctp->next->token_sequence_number >
                                      start_found_list->last_token_number) ||
         start_found_list->last_token_number == NO_TOKEN_SEQUENCE_NUMBER)) {
      /* We've found the last token of the first entry on the "start found"
         list.  In addition to checking whether we've found the token number
         specified by "last_token_number" we also check whether the next
         token's sequence number is greater than the one we are looking for.
         This is used when scanning the tokens of a default argument where the
         last token number is computed, and could represent a preprocessing
         or pragma token that is not in the cache.

         Now that we've found the end of this entry, we move the entry to the
         completed list.  The test for NO_TOKEN_SEQUENCE_NUMBER is present
         for error cases in which the last token of the body was not found. */
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


static void create_extracted_body_entry_for_friend(
					a_template_cache_segment_ptr	tcsp)
/*
Create a special "extracted body" token entry for the first token of the
friend function definition.  This is used by the token string creation
routine to skip the friend function body when creating the template string.
Unlike normal "extracted body" entries, the body is not actually removed
from the cache.  The entry that is created is used to simply skip those
tokens during the token string creation process.
*/
{
  a_cached_token_ptr	first_token = tcsp->before_first_token->next;

  /* Skip over any pragmas that precede the first token of the body. */
  while (first_token != NULL &&
         first_token->extra_info_kind ==
                                        (a_token_extra_info_kind)teik_pragma) {
    first_token = first_token->next;
  }  /* while */
  /* Update the first token of the body with information about the tokens that
     have been skipped when the token string is created. */
  check_assertion(first_token->extra_info_kind ==
                                           (a_token_extra_info_kind)teik_none);
  first_token->extra_info_kind = (a_token_extra_info_kind)teik_extracted_body;
  first_token->variant.extracted_template.symbol = tcsp->symbol;
  first_token->variant.extracted_template.semicolon_inserted = FALSE;
  first_token->variant.extracted_template.next_in_token_string =
                                                              tcsp->last_token;
}  /* create_extracted_body_entry_for_friend */


static void replace_body_with_semicolon(a_template_cache_segment_ptr tcsp)
/*
tcsp points to a template cache entry for a member function or member class.
Remove the body from the cache.  If it was not already followed by a
semicolon, add a semicolon to the cache.
*/
{
  a_boolean		insert_semicolon = FALSE;
  a_cached_token_ptr	before_first_token = tcsp->before_first_token;
  a_cached_token_ptr	first_token = before_first_token->next;
  a_cached_token_ptr	last_token = tcsp->last_token;
  a_cached_token_ptr	ctp;
  a_cached_token_ptr	semicolon_token;

  /* See if the last token in the cache is followed by an optional
     semicolon.  Only insert one if there is not already one there. */
  for (ctp = tcsp->last_token->next; ctp != NULL; ctp = ctp->next) {
    /* Ignore pragma tokens. */
    if (ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
      continue;
    }  /* if */
    if (ctp->token != (a_byte_token_kind)tok_semicolon) {
      insert_semicolon = TRUE;
    } else {
      semicolon_token = ctp;
    }  /* if */
    break;
  }  /* for */
  /* Skip over any pragmas that precede the first token of the body. */
  while (first_token != NULL &&
         first_token->extra_info_kind ==
                                        (a_token_extra_info_kind)teik_pragma) {
    first_token = first_token->next;
  }  /* while */
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
    semicolon_token = replacement_token;
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
  /* Update the semicolon token with information about the tokens that
     have been removed. */
  check_assertion(semicolon_token->extra_info_kind ==
                                           (a_token_extra_info_kind)teik_none);
  semicolon_token->extra_info_kind =
                                  (a_token_extra_info_kind)teik_extracted_body;
  semicolon_token->variant.extracted_template.symbol = tcsp->symbol;
  semicolon_token->variant.extracted_template.semicolon_inserted =
                                                              insert_semicolon;
  semicolon_token->variant.extracted_template.next_in_token_string = NULL;
}  /* replace_body_with_semicolon */


static void remove_default_arg(a_template_cache_segment_ptr tcsp)
/*
Remove a default argument from a token cache.  Replace it with a
special "removed default argument" token.  Then tokens are removed
from the list linked by the "next" pointer in the token cache, but
are still pointed to by the "next_in_token_string" link so that they
can still be put in the token string that is generated.
*/
{
  a_cached_token_ptr	before_first_token = tcsp->before_first_token;
  a_cached_token_ptr	first_token = before_first_token->next;
  a_cached_token_ptr	last_token = tcsp->last_token;
  a_cached_token_ptr	replacement_token;

  /* Make a new cached token entry for a dummy "removed default argument"
     token.  The default argument will be replaced with this token.
     Give it the same token sequence number as the first token of the
     default argument. */
  replacement_token = build_cached_token(tok_removed_default_arg,
                                         tcsp->first_token_number,
                                         &first_token->source_position);
  /* Link the replacement token into the cache in the place of
     the default argument. */
  replacement_token->next = last_token->next;
  before_first_token->next = replacement_token;
  /* Flag the replacement token as representing an extracted body.  This
     is somewhat redundant as in this particular case the token kind
     already indicates that. */
  check_assertion(replacement_token->extra_info_kind ==
                                           (a_token_extra_info_kind)teik_none);
  replacement_token->extra_info_kind =
                                  (a_token_extra_info_kind)teik_extracted_body;
  replacement_token->variant.extracted_template.symbol = NULL;
  replacement_token->variant.extracted_template.semicolon_inserted = FALSE;
  replacement_token->variant.extracted_template.next_in_token_string =
                                                                   first_token;
}  /* remove_default_arg */


static a_template_cache_segment_ptr extract_member_bodies(
		a_template_cache_ptr			tcp,
		a_template_cache_segment_ptr		cache_segments,
		a_boolean				keep_default_args)
/*
Go through the member functions and nested classes of the class template
associated with tcp, and remove the tokens from the token cache.  If
keep_default_args is TRUE, any default argument entries are retained
and a list of the unprocessed entries is returned to the caller.
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
    cache_segments = map_token_numbers_to_cache_pointers(tcp, cache_segments);
  }  /* if */
  for (tcsp = cache_segments; tcsp != NULL; tcsp = next_tcsp) {
    next_tcsp = tcsp->next;
    /* A missing last_token_number indicates that an error occurred
       while scanning the class definition and no ending token was found.
       Don't attempt to remove the body from the template. */ 
    if (tcsp->last_token_number == NO_TOKEN_SEQUENCE_NUMBER) continue;
    if (tcsp->is_friend) {
      /* Friend function bodies are not actually removed from the cache. */
      create_extracted_body_entry_for_friend(tcsp);
    } else if (tcsp->is_default_arg) {
      if (keep_default_args) {
        /* Add this entry to a new list of entries that still need to
           be processed. */
        tcsp->next = new_list;
        new_list = tcsp;
        continue;
      } else {
        /* A default argument.  Remove the default argument and replace it
           with a "removed default argument" token. */
        remove_default_arg(tcsp);
      }  /* if */
    } else {
      switch (tcsp->symbol->kind) {
        case sk_member_function:
        case sk_class_template:
        case sk_function_template:
          /* A separate copy of the token cache is already maintained for
             member functions and member templates.  Just free the
             tokens that were removed from
             the original cache. */
          { a_cached_token_ptr	first_token = tcsp->before_first_token->next;
            replace_body_with_semicolon(tcsp);
            free_tokens_from_reusable_cache(first_token, &tcp->tokens);
          }
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
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
            move_cached_tokens(first_token, &tcp->tokens,
                               &tcsp->template_info->cache.tokens);
          }  /* if */
          break;
        default:
          unexpected_condition();
      }  /* switch */
    }  /* if */
    /* Free the template cache segment for the member just removed. */
    free_template_cache_segment(tcsp);
  }  /* for */
  db_exit();
  return new_list;
}  /* extract_member_bodies */


static
void instantiate_class_template(a_symbol_ptr                 template_sym,
                                a_type_ptr                   prototype_type,
                                a_template_cache_segment_ptr *tcsp,
                                a_tmpl_decl_state_ptr        decl_state)
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
  instance_sym = tssp->variant.class_template.prototype_instantiation;
  instance_sym->defined = TRUE;
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
  /* Update the type_kind of the instance with the type_kind from the
     template.  Ordinarily, this will have already been done when the
     partial instantiation was done.  But, for partial specializations
     the type kind of the partial specialization may be different than
     that of the primary template. */
  prototype_type->kind = tssp->variant.class_template.type_kind;
  template_arg_list = templ_arg_list_for_class(prototype_type);
  cssp->instantiation_in_progress = TRUE;
  /* Record the namespace that is the "referencing context" namespace for
     this instantiation.  For the prototype instantiation this is the
     same as the namespace in which the template was defined. */
  cssp->referencing_namespace = 
                 scope_stack[depth_innermost_namespace_scope].assoc_namespace;
  push_template_instantiation_scope(tssp->cache.decl_info,
				    prototype_type,
				    (a_routine_ptr)NULL, instance_sym,
				    template_sym, template_arg_list,
                                    /*push_stop_tokens=*/TRUE,
                                    PS_PROTOTYPE_INSTANTIATION);
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
                              /*is_template_instantiation=*/TRUE,
                              decl_state->il_template_entry,
                              &decl_state->decl_pos_block);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (prototype_type->source_corresp.decl_pos_info != NULL) {
    /* Record extended position information for the template declaration. */
    a_decl_pos_block_ptr            pos_src = &decl_state->decl_pos_block;
    a_decl_position_supplement_ptr  pos_dst = prototype_type->
                                                 source_corresp.decl_pos_info;
    pos_dst->identifier_range = pos_src->identifier_range;
    pos_dst->specifiers_range = pos_src->specifiers_range;
    pos_dst->variant.declarator_range = pos_src->declarator_range;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  prototype_type->source_corresp.access = access_for_symbol(template_sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  prototype_type->autonomous_primary_tag_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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


void function_prototype_instantiation(
			a_symbol_ptr		template_sym)
/*
This routine is called to do a "prototype instantiation" of a function
template or member function of a class template.

This is done to detect those errors that can be diagnosed at template
definition time and to record information about nondependent calls for
user later during real instantiations.
*/
{
  a_symbol_ptr                      rout_sym;
  a_routine_ptr                     rout_ptr;
  a_template_symbol_supplement_ptr  tssp;
  a_template_cache_ptr		    tcp;
  a_func_info_block		    *func_info_ptr;
  a_template_instance_ptr	    tip;
  a_boolean			    instantiation_scope_needed;

  db_enter(3, "function_prototype_instantiation");
  tssp = template_supplement_for_symbol(template_sym);
  rout_ptr = tssp->variant.function.routine;
  rout_sym = (a_symbol_ptr)rout_ptr->source_corresp.assoc_info;
  check_assertion(rout_sym != NULL);
  /* Set the referencing namespace for the prototype instantiation. */
  tip = rout_sym->variant.routine.instance_ptr;
  check_assertion(tip != NULL);
  tip->referencing_namespace = parent_namespace_for_symbol(rout_sym);
  /* We don't need to push an instantiation scope if we are in the prototype
     instantiation of the enclosing class, and the thing being instantiated
     is a nontemplate member. */
  instantiation_scope_needed =
                    template_sym->kind != (a_symbol_kind)sk_member_function ||
                    !scope_stack[depth_scope_stack].in_prototype_instantiation;
  if (rout_ptr->assoc_scope != NULL_region_number) {
    /* The routine is already defined (a duplicate definition error should
       have already been issued). */
  } else {
    func_info_ptr = func_info_for_template(tssp);
    /* Push the template instantiation scope. */
    tcp = cache_for_template(tssp);
    if (instantiation_scope_needed) {
      /* For member functions that are not member templates the argument
         list comes from the enclosing class that is reactivated by
         push_template_instantiation_scope and the value from the routine
         entry (which should be NULL) is not used. */
      push_template_instantiation_scope(tcp->decl_info,
 				        (a_type_ptr)NULL, rout_ptr,
  				        rout_sym, template_sym,
  				        rout_ptr->template_arg_list,
                                        /*push_stop_tokens=*/TRUE,
                                        PS_PROTOTYPE_INSTANTIATION);
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
    /* Reactivate the tokens comprising the function body and scan them. */
    rescan_reusable_cache(&tcp->tokens);
    scan_function_body(rout_ptr, func_info_ptr,
                       (SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                        SFB_IS_INSTANTIATION |
                        SFB_PRAGMA_PACK_IS_LOCAL));
    /* scan_function_body does not scan past the right brace. */
    if (curr_token == tok_rbrace) (void)get_token();
    /* Process any pragmas that are to be bound to this instance. */
    process_curr_construct_pragmas(rout_sym, (a_statement_ptr)NULL);
    if (instantiation_scope_needed) {
      /* Pop the template instantiation scope. */
      pop_template_instantiation_scope();
    }  /* if */
    /* In the normal case the current token should be end_of_source, which was
       inserted to mark the end of the cached token stream. If necessary, keep
       flushing until end-of-source is found. */
    flush_past_token_cache_terminator();
  }  /* if */
  db_exit();
}  /* function_prototype_instantiation */


#if !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/ /* update_declared_type is not used in this case. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
void default_arg_prototype_instantiation(
	a_symbol_ptr				template_sym,
	a_def_arg_expr_fixup_ptr		def_arg_list,
	a_symbol_ptr				prototype_scope_symbols,
        a_boolean                               update_declared_type)
/*
This routine is called to do a "prototype instantiation" of a default
argument expression of a function template or a member function
of a class template.  If update_declared_type is TRUE, the default
arguments will be recorded in the declared type (for default arguments
instantiated during class fixups this is done elsewhere).

This is done to detect those errors that can be diagnosed at template
definition time and to record information about nondependent calls for
user later during real instantiations.
*/
{
  a_symbol_ptr				rout_sym;
  a_routine_ptr				rout_ptr;
  a_def_arg_expr_fixup_ptr		daefp;
  a_template_symbol_supplement_ptr	tssp;

  db_enter(3, "default_arg_prototype_instantiation");
#if DEBUG
  if (db_flag_is_set("def_arg_proto")) {
    for (daefp = def_arg_list; daefp != NULL; daefp = daefp->next) {
      fprintf(f_debug, "prototype instantiation of default arg:\n");
      db_token_cache(&daefp->cache.tokens, "default arg");
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  tssp = template_supplement_for_symbol(template_sym);
  rout_ptr = tssp->variant.function.routine;
  rout_sym = (a_symbol_ptr)rout_ptr->source_corresp.assoc_info;
  check_assertion(rout_sym != NULL);
  for (daefp = def_arg_list; daefp != NULL; daefp = daefp->next) {
    /* Push the template instantiation scope. */
    /* For member functions that are not member templates the argument
       list comes from the enclosing class that is reactivated by
       push_template_instantiation_scope and the value from the routine
       entry (which should be NULL) is not used. */
    push_template_instantiation_scope(daefp->cache.decl_info,
				      (a_type_ptr)NULL, rout_ptr,
				      rout_sym, template_sym,
				      rout_ptr->template_arg_list,
                                      /*push_stop_tokens=*/TRUE,
                                      PS_PROTOTYPE_INSTANTIATION);
    /* The function prototype scope should be reactivated and its symbols
       reentered because parameter names hide names from enclosing scopes
       and, moreover, may not be used in default argument expressions. */
    (void)push_scope((a_scope_kind)sck_func_prototype,
                     daefp->cache.decl_info->declaration_scope,
                     rout_ptr->type, (a_routine_ptr)NULL);
    if (prototype_scope_symbols != NULL) {
      reactivate_prototype_scope_symbols(prototype_scope_symbols);
    }  /* if */
    /* Reactivate the tokens comprising the function body and scan them. */
    rescan_reusable_cache(&daefp->cache.tokens);
    delayed_scan_of_default_arg_expr(daefp->param_type,
                                     /*check_for_errors=*/FALSE);
    /* Pop the reactivated function prototype scope off the stack. */
    pop_scope();
    /* Pop the template instantiation scope. */
    pop_template_instantiation_scope();
    /* The routine that rescans the default argument ensures that we have
       reached the end of the token cache. */
  }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (update_declared_type && prototype_instantiations_in_il &&
      def_arg_list != NULL) {
    /* The IL representation for the default arguments should be added to
       the declared type too.  The declared type might be in a secondary
       source sequence entry; otherwise, we fix up the one in the template
       symbol supplement. */
    a_source_sequence_entry_ptr  ssep =
                              last_matching_source_sequence_entry(
                                      (char *)tssp->variant.function.routine);
    a_type_ptr  declared_type;
    if (ssep != NULL && ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
      a_src_seq_secondary_decl_ptr  sssdp =
                               (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
      declared_type = sssdp->declared_type;
    } else {
      declared_type = tssp->variant.function.func_info.declared_type;
    }  /* if */
    if (declared_type != NULL) {
      copy_routine_type_default_args(
                         tssp->variant.function.routine->type, declared_type);
    }  /* if */
  }  /* if */
#endif  /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
}  /* default_arg_prototype_instantiation */


static void static_data_member_prototype_instantiation(
					a_symbol_ptr	template_sym)
/*
This routine is called to do a "prototype instantiation" of a template
static data member.

This is done to detect those errors that can be diagnosed at template
definition time and to record information about nondependent calls for
user later during real instantiations.
*/
{
  a_template_symbol_supplement_ptr  tssp;
  a_variable_ptr		    var_ptr;
  a_template_cache_ptr		    tcp;
  a_template_instance_ptr	    tip;
  a_boolean			    instantiation_scope_needed;

  db_enter(3, "static_data_member_prototype_instantiation");
  var_ptr = template_sym->variant.static_data_member.variable;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  var_ptr->declared_type = var_ptr->type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Set the referencing namespace for the prototype instantiation. */
  tip = template_sym->variant.static_data_member.instance_ptr;
  check_assertion(tip != NULL);
  tip->referencing_namespace = parent_namespace_for_symbol(template_sym);
  tssp = template_sym->variant.static_data_member.instance_ptr->template_info;
  /* If the type of the static data member is a template class, make sure
     it is instantiated. */
  complete_type_is_needed(var_ptr->type);
  /* We don't need to push an instantiation scope if we are in the prototype
     instantiation of the enclosing class. */
  instantiation_scope_needed =
                    !scope_stack[depth_scope_stack].in_prototype_instantiation;
  if (instantiation_scope_needed) {
    /* Push a template instantiation scope.  For static data members, the
       argument list comes from the enclosing class that is reactivated by
       push_template_instantiation_scope. */
    tcp = cache_for_template(tssp);
    push_template_instantiation_scope(tcp->decl_info,
                                      (a_type_ptr)NULL,
                                      (a_routine_ptr)NULL,
				      template_sym,
                                      template_sym,
                                      (a_template_arg_ptr)NULL,
                                      /*push_stop_tokens=*/TRUE,
                                      PS_PROTOTYPE_INSTANTIATION);
  }  /* if */
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
    initializer(template_sym, &template_sym->decl_position,
                idl_external, has_parenthesized_initializer,
                /*is_old_style_param_decl=*/FALSE,
                &incomplete_type_error_reported, (a_decl_pos_block_ptr)NULL);
    if (curr_token != tok_end_of_source) {
      pos_error(ec_exp_semicolon, &pos_curr_token);
      while (curr_token != tok_end_of_source) (void)get_token();
    }  /* if */
    /* By pass end-of-source token, which is probably the terminator token
       in the cache. */
    (void)get_token();
  } else {
    /* There's no explicit initializer. */
    (void)def_initializer(template_sym, &template_sym->decl_position);
  }  /* if */
  if (instantiation_scope_needed) {
    pop_template_instantiation_scope();
  }  /* if */
  db_exit();
}  /* static_data_member_prototype_instantiation */


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
void find_function_template_member(a_tmpl_decl_state_ptr	decl_state,
				   a_symbol_ptr			ft_symbol)
/*
ft_symbol is a symbol representing a member function template of a real
instantiation of a class template.  Find the sk_function_template symbol
from the prototype instantiation (it serves as the template for the
real member function template), and record it in the template symbol
supplement already associated with ft_symbol.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_symbol_supplement_ptr  orig_tssp;
  a_symbol_ptr			    parent_class_sym;
  a_symbol_ptr			    corresp_prototype_tag_sym;
  a_symbol_list_entry_ptr	    slep;
  a_type_ptr			    parent_class;

  db_enter(3, "find_function_template_member");
  /* Get the prototype instantiation symbol that corresponds to the parent
     class of this member template. */
  parent_class = decl_state->class_declared_in;
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

    } else if (is_conversion_function_symbol(ft_symbol)) {
      /* Look through the conversion routines of the prototype instantiation.
         The token sequence number associated for the current token is saved
         during the prototype instantiation.  This is used to match this
         declaration with the symbol generated by the prototype
         instantiation. */
      sym = NULL;
      for (slep = corresp_prototype_tag_sym->
               variant.class_struct_union.extra_info->conversion_template_list;
           slep != NULL;
           slep = slep->next) {
        a_template_symbol_supplement_ptr	other_tssp;
        a_symbol_ptr                            other_sym;
        other_sym = slep->symbol;
        other_tssp = template_supplement_for_symbol(other_sym);
        if (other_tssp->token_sequence_number == curr_token_sequence_number) {
          /* slep->symbol is the template function symbol for rout_sym. */
          sym = slep->symbol;
          break;
        }  /* if */
      }  /* for */
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
    if (sym != NULL) {
      a_boolean	is_list = FALSE;
      if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        is_list = TRUE;
        sym = sym->variant.overloaded_function.symbols;
      }  /* if */
      /* If an overloaded function was found, go through the symbols on its
         list and find the function template symbol that corresponds to
         ft_symbol. The token sequence number associated for the current
         token is saved during the prototype instantiation.  This is used
         to match this declaration with the symbol generated by the prototype
         instantiation.  If the symbol is not an overloaded function, make
         sure that it matches the ft_symbol. */
      for (; sym != NULL;
           sym = is_list ? sym->next : NULL) {
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
    /* Copy the default argument information from the prototype template. */
    tssp->variant.function.def_arg_expr_list =
                                orig_tssp->variant.function.def_arg_expr_list;
    /* Get the declaration sequence number from the prototype template. */
    decl_state->decl_info->decl_seq =
                    orig_tssp->variant.function.decl_cache.decl_info->decl_seq;
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
  if (tssp->pending_instantiations >= max_pending_instantiations) {
    /* This function instantiation occurs within the context of other
       instantiations of the same function template.  When the number of
       such instantiations-in-progress exceeds a specified value,
       we assume this to be runaway recursion.

       Note that this can only catch recursive instantiations of inline
       functions, and instantiations done during instantiation wrapup. */
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
    if (!extern_inline_allowed) {
      rout_ptr->storage_class = (a_storage_class)sc_static;
      rout_ptr->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_internal;
    }  /* if */
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
    check_assertion(!is_qualified_type(rout_ptr->type));
    rout_ptr->type =
            copy_routine_type_with_param_types(rout_ptr->type,
                                               /*copy_default_args=*/TRUE);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (tip->declared_type == NULL) {
    /* The declared type info was lost.  This could happen in error recovery
       mode (e.g., because the declaration was thought to be a typedef). 
       Reconstruct the instantiated declared type from that of the template. */
      check_assertion(total_errors != 0);
      tip->declared_type = instantiate_type_for_template_function(
                                      func_info_ptr->declared_type, rout_ptr);
  }  /* if */
  set_routine_declared_type(rout_ptr, tip->declared_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Set the linkage and storage class. */
  if (instantiation_mode == tim_local) {
    /* Put out template function as internally linked. */
    rout_ptr->storage_class = (a_storage_class)sc_static;
    rout_ptr->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_internal;
  } else if (rout_ptr->storage_class != (a_storage_class)sc_static) {
    /* Set the linkage for the definition of an externally linked routine. */
    rout_ptr->storage_class = (a_storage_class)sc_unspecified;
    rout_ptr->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_cplusplus_external;
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object &&
        !rout_ptr->is_inline &&
        !is_member_of_unnamed_namespace(&rout_ptr->source_corresp)) {
      /* Get a "needed bit number" for the routine. */
      rout_ptr->instantiation_needed_bit_number =
                                      assign_instantiation_needed_bit_number();
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  }  /* if */
  ++(tssp->pending_instantiations);
  /* Push the template instantiation scope. */
  tcp = cache_for_template(tssp);
  /* For member functions that are not member templates the argument
     list comes from the enclosing class that is reactivated by
     push_template_instantiation_scope and the value from the routine
     entry (which should be NULL) is not used. */
  push_template_instantiation_scope(tcp->decl_info,
				    (a_type_ptr)NULL, rout_ptr,
				    rout_sym, template_sym,
				    rout_ptr->template_arg_list,
                                    /*push_stop_tokens=*/TRUE, PS_NO_OPTIONS);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    fputs("full instantiation of \"", f_debug);
    db_name(&rout_ptr->source_corresp);
    fputs("\":\n", f_debug);
  }  /* if */
#endif /* DEBUG */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
      add_to_source_sequence_list((char *)rout_ptr,
                                  (an_il_entry_kind)iek_routine);
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
  /* Reactivate the tokens comprising the function body and scan them. */
  rescan_reusable_cache(&tcp->tokens);
  scan_function_body(rout_ptr, func_info_ptr,
                     (SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                      SFB_IS_INSTANTIATION |
                      SFB_PRAGMA_PACK_IS_LOCAL));
  /* scan_function_body does not scan past the right brace. */
  if (curr_token == tok_rbrace) (void)get_token();
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
  if (tssp->befriending_classes != NULL && !rout_sym->is_class_member) {
    /* If this template is a friend of one or more classes, check whether
       the template was defined in a friend declaration.  If so, update
       the friend information accordingly.  This is not done for functions
       that are class members, because they cannot be defined in friend
       declarations. */
    check_for_definition_in_friend_declaration(tssp, rout_ptr);
  }  /* if */
done:;
  /* The already instantiated flag is set even if certain error conditions
     exist (such as runaway instantiation), to prevent the compiler from
     attempting to instantiate this function again. */
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
  if (tssp->pending_instantiations >= max_pending_instantiations) {
    /* This instantiation occurs within the context of other instantiations
       of the same static data member.  When the number of such instantiations
       exceeds a specified limit, we assume this to be a runaway recursion. */
    sym_error(ec_runaway_recursive_instantiation, static_data_member_sym);
    goto done;
  }  /* if */
  /* If the type of the static data member is a template class, make sure
     it is instantiated. */
  complete_type_is_needed(var_ptr->type);
  if (instantiation_mode == tim_local) {
    /* In -tlocal mode, put out the static data member with internal
       linkage. */
    var_ptr->storage_class = (a_storage_class)sc_static;
    var_ptr->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
  }  /* if */
  /* If the storage class is sc_extern, reset it to sc_unspecified (since the
     variable is being defined).  If it is sc_static (e.g., for a static
     data member), leave it alone. */
  if (var_ptr->storage_class == (a_storage_class)sc_extern) {
    var_ptr->storage_class = (a_storage_class)sc_unspecified;
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object &&
        !is_member_of_unnamed_namespace(&var_ptr->source_corresp)) {
      /* Get a "needed bit number" for the routine. */
      var_ptr->instantiation_needed_bit_number =
                                      assign_instantiation_needed_bit_number();
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
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
  push_template_instantiation_scope(tssp->cache.decl_info,
                                    (a_type_ptr)NULL,
                                    (a_routine_ptr)NULL,
                                    static_data_member_sym,
                                    tip->template_sym,
                                    (a_template_arg_ptr)NULL,
                                    /*push_stop_tokens=*/TRUE, PS_NO_OPTIONS);
  /* Reactivate any pragmas that should be bound to the generated
     instance. */
  reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
  ++(tssp->pending_instantiations);
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
                &incomplete_type_error_reported, (a_decl_pos_block_ptr)NULL);
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
  --(tssp->pending_instantiations);
  /* Usually template static data members are instantiated "on demand" and
     so the referenced flag will already have been set.  But if the
     instantiation mode says to instantiate whether or not there is
     a reference, we should set the referenced flag anyway, so that
     the back-end will be sure to generate the function. */ 
  var_ptr->source_corresp.referenced = TRUE;
  var_ptr->is_template_static_data_member = TRUE;
  /* Note that Microsoft decl_modifiers are not processed on static
     data member definitions.  Microsoft does not allow this either. */
  var_ptr->assoc_template = tssp->il_template_entry;
done:
  /* The already instantiated flag is set even if certain error conditions
     exist (such as runaway instantiation), to prevent the compiler from
     attempting to instantiate this static data member again. */
  tip->already_instantiated = TRUE;
  db_exit();
}  /* define_template_static_data_member */


a_boolean equiv_templates_given_supplement(
				a_template_symbol_supplement_ptr	tssp1,
				a_template_symbol_supplement_ptr	tssp2)
/*
Return TRUE if tssp1 and tssp2 are equivalent.  If either of the
templates is a "real" template, the pointers must refer to the same
template.  If they are both "nonreal" (either nonreal members or template
template parameters), the two templates are equivalent if they have
equivalent template parameter lists.
*/
{
  a_boolean	result = FALSE;
  a_boolean	must_be_identical = TRUE;
  a_boolean	okay_so_far = TRUE;
  a_boolean	compare_parameters = TRUE;

  if (tssp1->is_nonreal_member &&
      tssp2->is_nonreal_member) {
    /* Nonreal members have must have the same name and parent class. */
    a_template_ptr	templ1 = tssp1->il_template_entry;
    a_template_ptr	templ2 = tssp2->il_template_entry;
    must_be_identical = FALSE;
    /* Nonreal templates have no parameter lists. */
    compare_parameters = FALSE;
    if (strcmp(templ1->source_corresp.name,
               templ2->source_corresp.name) == 0) {
      /* They have the same names. */
      if (!identical_types(templ1->source_corresp.parent.class_type,
                          templ2->source_corresp.parent.class_type)) {
        /* Their parent types are the different. */
        okay_so_far = FALSE;
      }  /* if */
    }  /* if */
  } else if (tssp1->variant.class_template.template_template_param &&
             tssp2->variant.class_template.template_template_param) {
    /* Template template parameters must be at the same coordinates. */
    a_template_param_coordinate_ptr	coordinates1;
    a_template_param_coordinate_ptr	coordinates2;
    coordinates1 = &tssp1->il_template_entry->coordinates;
    coordinates2 = &tssp2->il_template_entry->coordinates;
    must_be_identical = FALSE;
    if (coordinates1->position != coordinates2->position ||
        !equiv_nesting_depths(coordinates1->depth, coordinates2->depth)) {
      /* The coordinates do not match. */
      okay_so_far = FALSE;
    }  /* if */
  }  /* if */
  if (!okay_so_far) {
    /* No further checking necessary. */
  } else if (must_be_identical) {
    result = tssp1 == tssp2;
  } else {
    result = !compare_parameters ||
             equiv_template_param_lists(tssp1->cache.decl_info->parameters,
                                        tssp2->cache.decl_info->parameters,
				        /*issue_errors=*/FALSE,
				        (a_source_position*)NULL);
  }  /* if */
  return result;
}  /* equiv_templates_given_supplement */


static a_boolean equiv_templates(a_template_ptr	templ1,
				 a_template_ptr	templ2)
/*
Return TRUE if the templates specified by templ1 and templ2 are equivalent.
templ1 and/or templ2 are permitted to be NULL (in which case, they match
nothing).
*/
{
  a_template_symbol_supplement_ptr	tssp1;
  a_template_symbol_supplement_ptr	tssp2;
  a_boolean				result = FALSE;

  if (templ1 != NULL && templ2 != NULL) {
    templ1 = canonical_template_entry_of(templ1);
    templ2 = canonical_template_entry_of(templ2);
    tssp1 = template_supplement_for_template(templ1);
    tssp2 = template_supplement_for_template(templ2);
    result = equiv_templates_given_supplement(tssp1, tssp2);
  }  /* if */
  return result;
}  /* equiv_templates */


static a_boolean equiv_nontype_template_param_names(
						a_constant_ptr	con1,
						a_constant_ptr	con2)
/*
Determine whether con1 and con2 are ck_template_param constants that
refer to equivalent template parameter constants except for the fact
that their types were declared differently.  This is used in Microsoft
mode where usage such as the following is accepted:

	template <int I> struct A { void f(); };
	template <unsigned int I> void A<I>::f(){}

In this case, the template arguments being compared have different types
(int and unsigned int), but we want them to be treated as equivalent.

Return TRUE if the constants should be considered to match.
*/
{
  a_boolean	result = FALSE;

  if (con1->kind == (a_constant_repr_kind)ck_template_param &&
      con1->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_param) {
    /* The first constant is the name of a template parameter. */
    if (con2->kind == (a_constant_repr_kind)ck_template_param &&
        con2->variant.template_param.kind ==
                                   (a_template_param_constant_kind)tpck_cast) {
      /* The second constant is a cast of something.  Remove the cast. */
      con2 = con2->variant.template_param.variant.constant;
    }  /* if */
    if (con2->kind == (a_constant_repr_kind)ck_template_param &&
        con2->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_param) {
      /* The thing being cast is the name of a template parameter.
         Make a copy of the constant under the cast, but use the type of
         the first constant.  Compare the resulting constants. */
      a_constant	copy_of_con2;
      copy_constant(con2, &copy_of_con2);
      copy_of_con2.type = con1->type;
      result = eq_constants(con1, &copy_of_con2);
    }  /* if */
  }  /* if */
  return result;
}  /* equiv_nontype_template_param_names */


a_boolean equiv_template_arg_lists(
				a_template_arg_ptr list1,
				a_template_arg_ptr list2,
				an_equiv_templ_arg_options_set	options)
/*
Return TRUE if the two linked lists of template arguments for a given template
class or template function are equivalent -- that is, if corresponding type
arguments refer to the same type and corresponding constant arguments refer to
the same constant.
*/
{
  a_boolean		equiv;
  a_template_arg_ptr	arg1 = list1, arg2 = list2;
  a_boolean		is_nonreal_member;
  a_boolean		error_matches_anything;
  a_boolean		ignore_unknown_arg_values;
  a_boolean		ignore_qualifiers;
  a_boolean		is_prototype;

  db_enter(4, "equiv_template_arg_lists");
  is_nonreal_member = (options & ETA_IS_NONREAL_MEMBER) != 0;
  error_matches_anything = (options & ETA_ERROR_MATCHES_ANYTHING) != 0;
  ignore_unknown_arg_values = (options & ETA_IGNORE_UNKNOWN_ARG_VALUES) != 0;
  ignore_qualifiers = (options & ETA_MS_IGNORE_QUALIFIERS) != 0;
  is_prototype = (options & ETA_IS_PROTOTYPE) != 0;
  /* There is no way to produce a NULL template argument list, so the real
     code doesn't need to check for that. */
  check_assertion_str2(is_nonreal_member || (list1 != NULL && list2 != NULL),
                       "equiv_template_arg_lists:", " NULL arg list");
  /* Assume they are equivalent, until we find evidence to the contrary. */
  equiv = TRUE;
  /* Loop through both lists in step, comparing arguments. */
  while (arg1 != NULL && arg2 != NULL) {
    /* For a given class, argument lists should always have the same sequence
       of type, constant, and template arguments. */
    if (arg1->kind != arg2->kind) {
      equiv = FALSE;
      check_assertion_str(is_nonreal_member,
                          "equiv_template_arg_lists: arg inconsistency");
      break;
    } else if (is_nontype_templ_arg(arg1)) {
      /* Both are constant arguments.  If they are not identical, this is a
         mismatch. */
      a_constant_ptr con1 = arg1->variant.constant;
      a_constant_ptr con2 = arg2->variant.constant;
      /* Unknown array bounds should not escape the type deduction process. */
      check_assertion(!arg1->is_array_bound_of_unknown_type &&
                      !arg2->is_array_bound_of_unknown_type);
      if (ignore_unknown_arg_values &&
          (con1 == NULL || con2 == NULL)) {
        /* An argument with no specified value.  Treat this as a match. */
      } else if (con1 == NULL && con2 == NULL) {
        /* Both argument values are unspecified.  Treat this as a match. */
      } else if (con1 == NULL || con2 == NULL) {
        /* Only one is unspecified -- this is a mismatch. */
        equiv = FALSE;
      } else if (eq_constants(con1, con2)) {
        /* Okay. */
      } else if (is_prototype &&
                 equiv_nontype_template_param_names(con1, con2)) {
        /* Two template parameter names that have a type mismatch but
           that should be considered equivalent in the current mode. */
        /* Okay. */
      } else if (error_matches_anything &&
                 (is_error_constant(con1) || is_error_constant(con2))) {
        /* Okay. */
      } else {
        equiv = FALSE;
      }  /* if */
      if (!equiv) break;
    } else if (is_type_templ_arg(arg1)) {
      /* Both are type arguments.  If they are not identical, this is a
         mismatch. */
      a_type_ptr type1 = arg1->variant.type;
      a_type_ptr type2 = arg2->variant.type;
      if (ignore_unknown_arg_values &&
          (type1 == NULL || type2 == NULL)) {
        /* An argument with no specified value.  Treat this as a match. */
      } else if (type1 == NULL && type2 == NULL) {
        /* Both argument values are unspecified.  Treat this as a match. */
      } else if (type1 == NULL || type2 == NULL) {
        /* Only one is unspecified -- this is a mismatch. */
        equiv = FALSE;
      } else if (identical_types(type1, type2)) {
        /* Okay. */
      } else if (error_matches_anything &&
                 (is_error_type(type1) || is_error_type(type2))) {
        /* Okay. */
      } else {
        a_type_ptr	tp1 = skip_typerefs(type1);
        a_type_ptr	tp2 = skip_typerefs(type2);
        if (ignore_qualifiers && identical_types(tp1, tp2)) {
          /* In Microsoft bugs mode top level qualifiers are ignored when
             comparing two argument lists. */
          /* Okay. */
        } else {
          equiv = FALSE;
        }  /* if */
      }  /* if */
      if (!equiv) break;
    } else {
      /* A template template argument. */
      if (equiv_templates(arg1->variant.templ, arg2->variant.templ)) {
        /* Okay. */
      } else {
        equiv = FALSE;
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
a template parameter.
*/
{
  a_boolean  template_param_found;

  if (is_type_templ_arg(tap)) {
    template_param_found = is_or_contains_template_param(tap->variant.type);
  } else if (is_nontype_templ_arg(tap)) {
    if (tap->arg_operand != NULL) {
      /* The constant is still in arg_operand form. */
      template_param_found = arg_operand_contains_template_param(
                                                             tap->arg_operand);
    } else if (tap->is_array_bound_of_unknown_type) {
      /* An array bound specified as a integral constant. */
      template_param_found = FALSE;
    } else {
      /* A normal nontype parameter represented as a constant. */
      check_assertion(tap->variant.constant != NULL);
      template_param_found = (tap->variant.constant->kind ==
                                   (a_constant_repr_kind)ck_template_param);
    }  /* if */
  } else {
    /* A template template parameter.  The argument involves a template
       parameter if it is itself a template parameter, or if it is
       is a nonreal class member. */
    a_template_symbol_supplement_ptr	tssp;
    a_template_ptr			templ_ptr;
    a_symbol_ptr			templ_sym;
    templ_ptr = tap->variant.templ;
    /* Look at the argument template, not the original symbol (which,
       unlike other template parameters, always points to the prototype
       argument symbol). */
    templ_sym = symbol_for_template(templ_ptr);
    templ_sym = template_argument_if_template_template_param(templ_sym);
    tssp = templ_sym->variant.template_info;
    template_param_found = tssp->is_nonreal_member ||
                         tssp->variant.class_template.template_template_param;
    if (!template_param_found && templ_sym->is_class_member) {
      /* Check whether the parent type depends on a template parameter. */
      template_param_found =
                   is_or_contains_template_param(templ_sym->parent.class_type);
    }  /* if */
  }  /* if */
  return template_param_found;
}  /* template_arg_involves_template_param */


a_boolean template_arg_list_involves_template_param(a_template_arg_ptr	tap)
/*
Return TRUE if the template argument list pointed to by tap is or
contains a template parameter.
*/
{
  a_boolean	result = FALSE;

  for (; tap != NULL; tap = tap->next) {
    result = template_arg_involves_template_param(tap);
    if (result) break;
  }  /* for */
  return result;
}  /* template_arg_list_involves_template_param */


a_symbol_ptr find_template_class(
			     a_symbol_ptr        class_template_sym,
                             a_template_arg_ptr  *new_list,
			     a_boolean	         any_prototype_allowed,
			     a_symbol_ptr        specific_prototype_allowed)
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

If any_prototype_allowed is TRUE then the prototype instantiations of
the primary template and any partial specializations are checked
before any of the other instantiations.  If it is FALSE the prototype
instantiations will not be included in the search, except that if
specific_prototype_allowed is non-NULL then only the specified
prototype instantiation is considered as a potential match.
*/
{
  a_symbol_ptr                      sym;
  a_symbol_ptr 			    prototype_sym;
  a_template_arg_ptr                old_list;
  a_type_ptr                        class_type;
  a_class_type_supplement_ptr       ctsp;
  a_template_symbol_supplement_ptr  tssp;
  a_template_arg_ptr                tap;
  an_equiv_templ_arg_options_set    eta_options = ETA_NO_OPTIONS;

  db_enter(3, "find_template_class");
  check_assertion(class_template_sym->kind ==
                                            (a_symbol_kind)sk_class_template);
  /* If this is a template template parameter, replace the template symbol
     with the one referred to by the parameter. */
  class_template_sym =
              template_argument_if_template_template_param(class_template_sym);
  tssp = class_template_sym->variant.template_info;
  if (tssp->is_nonreal_member || tssp->is_error) {
    eta_options |= ETA_IS_NONREAL_MEMBER;
  }  /* if */
  if (microsoft_bugs && microsoft_version <= 1100) {
    eta_options |= ETA_MS_IGNORE_QUALIFIERS;
  }  /* if */
  sym = NULL;
  prototype_sym = tssp->variant.class_template.prototype_instantiation;
  if (any_prototype_allowed || specific_prototype_allowed != NULL) {
    if (prototype_sym != NULL &&
        (any_prototype_allowed ||
         specific_prototype_allowed == prototype_sym)) {
      /* Old list is the template argument list from the prototype
         instantiation of the primary template.  See if the list passed
         in matches it. */
      old_list = prototype_sym->variant.class_struct_union.type->
                     variant.class_struct_union.extra_info->template_arg_list;
      if (equiv_template_arg_lists(old_list, *new_list,
                                   eta_options | ETA_IS_PROTOTYPE)) {
        /* A match.  Set sym which will suppress any further search. */
        sym = prototype_sym;
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      /* The list passed in did not match the primary prototype instantiation.
         See if it matches any of the partial specializations. */
      a_symbol_ptr	ps_sym;
      ps_sym = tssp->variant.class_template.partial_specializations;
      for (; ps_sym != NULL; ps_sym = ps_sym->next) {
        /* Get the symbol associated with the prototype instantiation of this
           partial specialization. */
        a_symbol_ptr	ps_prototype_sym;
        ps_prototype_sym = ps_sym->variant.template_info->
                                variant.class_template.prototype_instantiation;
        if (any_prototype_allowed ||
            specific_prototype_allowed == ps_prototype_sym) {
          /* Old list is the template argument list associated with the
             prototype instantiation of the partial specialization.  See if
             the list passed in matches it. */
          old_list = ps_prototype_sym->variant.class_struct_union.type->
                      variant.class_struct_union.extra_info->template_arg_list;
          if (equiv_template_arg_lists(old_list, *new_list,
                                       eta_options | ETA_IS_PROTOTYPE)) {
#if DEBUG
            if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
            sym = ps_prototype_sym;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    /* Make a pass over the symbols representing instantiations of the class
       template. */
    sym = tssp->variant.class_template.instantiations;
    for (; sym != NULL; sym = next_instance_sym(sym)) {
      /* Prototype instantiations should not be checked.  If they are being
         considered, then we would have already checked them in the tests
         above. */
      if (is_prototype_instantiation_symbol(sym)) continue;
      /* Old list is the template argument list from a template class that has
         already been created.  See if the list passed in matches it. */
      old_list = sym->variant.class_struct_union.type->
                     variant.class_struct_union.extra_info->template_arg_list;
      if (equiv_template_arg_lists(old_list, *new_list, eta_options)) {
        /* We've found a match. */
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
       are always looked up through the template). */
    a_symbol_ptr			primary_template_sym;
    a_template_symbol_supplement_ptr	primary_tssp;
    sym = make_template_class_symbol(class_template_sym);
    /* Add the new symbol to the head of the instantiation list.  The
       instantiation list of the primary template is always used (i.e.,
       not the list of a partial specialization). */
    primary_template_sym = primary_template_of(class_template_sym);
    primary_tssp = primary_template_sym->variant.template_info;
    next_instance_sym(sym) =
                           primary_tssp->variant.class_template.instantiations;
    primary_tssp->variant.class_template.instantiations = sym;
    /* Now create a new type entry. */
    class_type = alloc_type(tssp->variant.class_template.type_kind);
    class_type->variant.class_struct_union.is_template_class = TRUE;
    sym->variant.class_struct_union.type = class_type;
    if (tssp->is_nonreal_member ||
        tssp->variant.class_template.template_template_param) {
      /* Instantiations of a nonreal member template (for example,
         T::A<int>) are created as nonreal instantiations.  Likewise,
         instantiations of template template parameters are nonreal. */
      class_type->variant.class_struct_union.is_nonreal_class = TRUE;
    } else if (sym->is_class_member) {
      /* If the enclosing class is nonreal, then any instances of member
         classes must also be nonreal. */
      a_type_ptr			parent_class;
      parent_class = sym->parent.class_type;
      if (parent_class->variant.class_struct_union.is_nonreal_class) {
        class_type->variant.class_struct_union.is_nonreal_class = TRUE;
      }  /* if */
    }  /* if */
    /* If this is a "real instantiation" leave the type incomplete; it will
       become complete when it is instantiated.  However, if it is based on
       template parameters and is therefore a "nonreal" instantiation, give
       it a size and alignment to permit it to pass through subsequent
         processing without causing spurious errors. */
    for (tap = *new_list; tap != NULL; tap = tap->next) {
      if (!class_type->variant.class_struct_union.is_nonreal_class) {
        if (template_arg_involves_template_param(tap)) {
          class_type->variant.class_struct_union.is_nonreal_class = TRUE;
        }  /* if */
      }  /* if */
      if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
        /* Local typedef names (legal if they refer to nonlocal types) should
           not be part of the type signature of the template class itself,
           which is nonlocal.  Strip them off, if there are any. */
        if (is_type_templ_arg(tap)) {
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
    ctsp = class_type->variant.class_struct_union.extra_info;
    ctsp->template_arg_list = *new_list;
    {
      /* For certain classes (like X<int>::Y<T>) the prototype instantiation
         must be fetched from the prototype template (e.g., X<T>::Y).  Hence
         we cannot just use prototype_sym. */
      a_symbol_ptr  proto_template = prototype_template_of(class_template_sym);
      ctsp->assoc_template =
                      proto_template->variant.template_info->il_template_entry;
    }  /* if */
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
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (microsoft_mode or_near_and_far_enabled()) {
      a_symbol_ptr	prototype_template_prototype_sym;
      a_type_ptr	prototype_type;
      /* Update the Microsoft decl modifier information for this class based
         on the information stored in the prototype instantiation.  If this
         is an instance of a subordinate template, use the prototype
         instantiation associated with the prototype template. */
      prototype_template_prototype_sym =
             prototype_template_of(class_template_sym)->variant.template_info->
                                variant.class_template.prototype_instantiation;
      prototype_type = prototype_template_prototype_sym == NULL ? NULL :
                           prototype_template_prototype_sym->
                                               variant.class_struct_union.type;
      if (prototype_type != NULL) {
        a_class_type_supplement_ptr  prototype_ctsp;
        an_extended_decl_info_block  extended_decl_info;
        a_source_position            pos;

        pos = class_template_sym->decl_position;
        clear_extended_decl_info_block(extended_decl_info);
        prototype_ctsp = prototype_type->variant.class_struct_union.extra_info;
#if MICROSOFT_EXTENSIONS_ALLOWED
        extended_decl_info.decl_modifiers.flags =
                                    prototype_ctsp->decl_modifiers;
        extended_decl_info.decl_modifiers.uuid_string =
                                    prototype_ctsp->uuid_string;
        extended_decl_info.inheritance_kind = prototype_ctsp->inheritance_kind;
        extended_decl_info.inheritance_kind_pos = pos;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
        extended_decl_info.qualifiers = prototype_ctsp->qualifiers;
#endif /* NEAR_AND_FAR_ALLOWED */
        update_extended_decl_info_for_class(class_type, &extended_decl_info,
                                            &pos);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    if (class_type->variant.class_struct_union.is_nonreal_class) {
      class_type->size = 1;
      class_type->alignment = 1;
      if (prototype_instantiations_in_il) {
        /* If this is a nonreal member, add the type to the file scope types
           list.  Otherwise, pass in NO_SCOPE_DEPTH so that the add routine
           will figure out the appropriate scope to be used. */
        a_scope_depth	depth_to_add;
        depth_to_add = tssp->is_nonreal_member ? DEPTH_OF_FILE_SCOPE
                                               : NO_SCOPE_DEPTH;
        add_to_types_list(class_type, depth_to_add);
      }  /* if */
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fputs("partial instantiation of \"", f_debug);
        db_type_name(class_type);
        fputs("\":\n", f_debug);
      }  /* if */
#endif /* DEBUG */
      add_source_sequence_entry_for_partial_instantiation(
                                             (char *)class_type,
                                             (an_il_entry_kind)iek_type,
                                             class_type);
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    /* Call a routine that manages the correspondence of entities between
       translation units to notify it of the new instance. */
    record_instantiation(sym, tssp);
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


static a_template_arg_ptr create_initial_template_arg_list(
			a_template_param_ptr		templ_param_list,
			a_template_arg_ptr		partial_arg_list,
			a_source_position		*source_pos)
/*
Create a template argument list that corresponds in kind with the template
parameter list specified by templ_param_list.  Each template argument in the
list that is created initially contains a NULL type or constant pointer.
If partial_arg_list is non-NULL it will point to an explicitly specified
template argument list.  When partial_arg_list is provided, it is used to
supply the values for the specified arguments.  This is only done if the
arguments specified by partial_arg_list match in kind the parameter list
that was specified (i.e., type parameters correspond with type arguments).
If the supplied partial_arg_list does not match the parameter list, no
new argument list is created and a NULL pointer is returned.  source_pos is
only supplied when a partial_arg_list is provided.  source_pos is a
position passed to copy_type_with_substitution, which is called when
an explicitly specified template argument has a type that depends on
another template parameter.
*/
{
  a_template_arg_ptr	tap;
  a_template_param_ptr	tpp;
  a_boolean		arg_kind_mismatch = FALSE;
  a_template_arg_ptr	new_list = NULL;

  if (partial_arg_list != NULL) {
    /* An explicit template argument list was supplied.  Do some initial
       tests to see if this template is a viable match for the specific
       arguments. */
    for (tpp = templ_param_list, tap = partial_arg_list;
         tpp != NULL && tap != NULL; tpp = tpp->next, tap = tap->next) {
      a_symbol_kind		sym_kind = tpp->param_symbol->kind;
      if (templ_arg_kind_for_symbol_kind(sym_kind) != tap->kind) {
        arg_kind_mismatch = TRUE;
        break;
      }  /* if */
    }  /* for */
    if (!arg_kind_mismatch && tap != NULL && tpp == NULL) {
       /* There were more arguments specified than there are parameters.
          This can't be a match. */
       arg_kind_mismatch = TRUE;
    }  /* if */
  }  /* if */
  if (!arg_kind_mismatch) {
    a_template_arg_ptr		prev_tap = NULL;
    a_template_arg_ptr		specified_tap;
    /* Loop through the template parameter list and create a template
       argument entry of the appropriate type for each parameter. */
    for (tpp = templ_param_list, specified_tap = partial_arg_list;
         tpp != NULL;
         tpp = tpp->next,
           specified_tap = specified_tap == NULL
                                              ? NULL : specified_tap->next) {
      a_symbol_kind		sym_kind = tpp->param_symbol->kind;
      a_templ_arg_kind		arg_kind;
      arg_kind = templ_arg_kind_for_symbol_kind(sym_kind);
      tap = alloc_template_arg(arg_kind);
      if (specified_tap != NULL) {
        /* An argument value was supplied.  Copy it to the newly created
           template argument. */
        tap->explicitly_specified = specified_tap->explicitly_specified;
        if (is_type_templ_arg(tap)) {
          tap->variant.type = specified_tap->variant.type;
        } else if (is_template_templ_arg(tap)) {
          /* A template template argument can only be used if its parameter
             list is compatible with that of the template template
             parameter. */
          a_template_symbol_supplement_ptr	arg_template;
          arg_template = template_supplement_for_template(
                                                specified_tap->variant.templ);
          if (equiv_template_param_lists(
                           arg_template->cache.decl_info->parameters,
                           tpp->variant.templ->cache.decl_info->parameters,
                           /*issue_errors=*/FALSE, (a_source_position*)NULL)) {
            tap->variant.templ = specified_tap->variant.templ;
          } else {
            arg_kind_mismatch = TRUE;
            break;
          }  /* if */
        } else {
          /* Convert the constant value to the type of the template
             parameter. */
          a_type_ptr		constant_type;
          a_constant_ptr	constant;
          a_boolean		copy_error = FALSE;
          check_assertion(specified_tap->arg_operand != NULL);
          constant = fs_constant((a_constant_repr_kind)ck_error);
          constant_type = tpp->param_symbol->variant.constant->type;
          constant_type = copy_type_with_substitution(
                                    constant_type, new_list, templ_param_list,
				    source_pos,
                                    CTWS_NO_OPTIONS, &copy_error);
          if (copy_error) {
            /* The substitution of the type of the nontype parameter
               resulted in an invalid type. */
            arg_kind_mismatch = TRUE;
            break;
          }  /* if */
          /* Verify that the constant value can be converted to the type of the
             corresponding template parameter. */
          if (!nontype_template_arg_is_compatible_with_param_type(
                                  specified_tap->arg_operand, constant_type)) {
            arg_kind_mismatch = TRUE;
            break;
          }  /* if */
          conv_nontype_template_arg_to_param_type(
                          specified_tap->arg_operand, constant_type, constant);
          tap->arg_operand = NULL;
          tap->variant.constant = constant;
        }  /* if */
      }  /* if */
      if (prev_tap == NULL) {
        /* First iteration -- the start of the list. */
        new_list = tap;
      } else {
        /* Add to the end of the list. */
        prev_tap->next = tap;
      }  /* if */
      prev_tap = tap;
    }  /* for */
  }  /* if */
  if (arg_kind_mismatch && new_list != NULL) {
    /* A mismatch was found after part of the list was created.  Free the
       list and set the new list pointer to NULL. */
    free_template_arg_list(new_list);
    new_list = NULL;
  }  /* if */
  return new_list;
}  /* create_initial_template_arg_list */


a_template_arg_ptr get_template_arg_by_list_pos(
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
    *templ_arg_list = create_initial_template_arg_list(
				templ_param_list, (a_template_arg_ptr)NULL,
                                (a_source_position*)NULL);
  }  /* if */
  /* For the nth template parameter find the nth template argument. */
  for (tap = *templ_arg_list; pos > 1; pos--) tap = tap->next;
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
  for (; pos > 1; pos--) tpp = tpp->next;
  return tpp;
}  /* get_template_param_by_list_pos */


static a_boolean matches_template_template_param(
		a_template_ptr				templ,
		a_template_ptr				templ_templ,
		a_template_arg_ptr			*templ_arg_list,
		a_template_param_ptr			templ_param_list)
/*
Determine whether the template specified by "templ" matches the template
template parameter specified by "templ_templ".  Return TRUE if a
match is found.
*/
{
  a_template_param_ptr			param_list_for_templ;
  a_template_param_ptr			param_list;
  a_boolean				match = FALSE;
  a_template_symbol_supplement_ptr	templ_tssp;
  a_template_symbol_supplement_ptr	tssp;
  a_symbol_ptr				sym;
  a_symbol_ptr				templ_sym;

  /* Get the template parameter list associated with the template. */
  sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
  templ_sym = (a_symbol_ptr)templ_templ->source_corresp.assoc_info;
  if (sym->is_error || templ_sym->is_error) {
    /* If either symbol is an error symbol, there is no match. */
  } else {
    tssp = sym->variant.template_info;
    templ_tssp = templ_sym->variant.template_info;
    if (templ_tssp->variant.class_template.template_template_param) {
      param_list_for_templ = templ_tssp->cache.decl_info->parameters;
      param_list = tssp->cache.decl_info->parameters;
      if (equiv_template_param_lists(param_list_for_templ, param_list,
                                     /*issue_errors=*/FALSE,
                                     (a_source_position*)NULL)) {
        /* The actual template is compatible with the template template
           parameter.  See if it is compatible with any previously deduced
           value. */
        /* Get the template nesting depth as indicated by the first template
           parameter.  Any template parameters found in templ_type must be at
           the same level to participate in deduction. */
        a_template_nesting_depth	depth_of_template;
        depth_of_template = nesting_depth_of_template_param(templ_param_list);
        if (depth_of_template ==
                        templ_tssp->il_template_entry->coordinates.depth) {
          /* The depths match. */
          a_template_param_list_pos	list_pos;
          a_template_ptr		templ_ptr;
          a_template_arg_ptr		tap;
          /* Get the template argument that corresponds with this parameter. */
          list_pos = templ_tssp->il_template_entry->coordinates.position;
          tap = get_template_arg_by_list_pos(templ_param_list, templ_arg_list,
                                             list_pos);
          check_assertion(tap->kind == (a_templ_arg_kind)tak_template);
          templ_ptr = tssp->il_template_entry;
          if (tap->variant.templ == NULL) {
            /* No template has been bound to this template argument yet, so
               just the current template. */
            tap->variant.templ = templ_ptr;
            match = TRUE;
          } else {
            /* A template was already bound to this template argument.  We
               have a match if and only if the new one is the same as the
               old one. */
            if (tap->variant.templ == templ_ptr) {
              /* Okay. */
              match = TRUE;
            } else {
              /* Not a match.  Return FALSE. */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (tssp->is_nonreal_member || templ_tssp->is_nonreal_member) {
      /* Nonreal members have must have the same name and parent class. */
      a_template_ptr	tp = tssp->il_template_entry;
      a_template_ptr	templ_tp = templ_tssp->il_template_entry;
      if (strcmp(tp->source_corresp.name,
                 templ_tp->source_corresp.name) == 0) {
        /* They have the same names. */
        if (matches_template_type(tp->source_corresp.parent.class_type,
                                  templ_tp->
                                           source_corresp.parent.class_type,
                                  templ_arg_list, templ_param_list,
                                  MTT_NO_FLAGS)) {
          match = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* The template template is not a template template parameter.  Just make
         sure the templates match. */
      match = equiv_templates_given_supplement(tssp, templ_tssp);
    }  /* if */
  }  /* if */
  return match;
}  /* matches_template_template_param */


static a_boolean class_matches_template_template_param(
		a_type_ptr				type,
		a_symbol_ptr				sym_for_templ,
		a_template_arg_ptr			*templ_arg_list,
		a_template_param_ptr			templ_param_list)
/*
Determine whether the type specified by "type" is based on a template
that matches the template template parameter specified by sym_for_templ.
*/
{
  a_class_symbol_supplement_ptr		cssp;
  a_symbol_ptr				templ_for_type;
  a_boolean				match = FALSE;

  /* Get the template entry for the template from which "type" was
     generated (if any). */
  cssp = symbol_supplement_for_class(type);
  templ_for_type = cssp->class_template;
  /* If there is no template, the type is not template based so this is
     not a match. */
  if (templ_for_type != NULL) {
    a_template_ptr	templ;
    a_template_ptr	templ_templ;
    /* "type" is template based. */
    templ = templ_for_type->variant.template_info->il_template_entry;
    templ_templ = sym_for_templ->variant.template_info->il_template_entry;
    match = matches_template_template_param(templ, templ_templ, templ_arg_list,
                                            templ_param_list);
  }  /* if */
  return match;
}  /* class_matches_template_template_param */


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
 
  /* Get the template nesting depth as indicated by the first template
     parameter.  If templ_constant is a template parameter constant, it
     must be at the same level to participate in deduction.  A template
     parameters from a different nesting depth can be present when
     templ_constant is used in the parent class of a type that is passed
     to matches_template_type.  Nesting depths are only checked for
     declared template parameters (i.e., of kind tpck_param).  Other
     template parameters do not have nesting depths. */
  if (templ_constant->kind == (a_constant_repr_kind)ck_template_param &&
      (templ_constant->variant.template_param.kind != 
                             (a_template_param_constant_kind)tpck_param ||
        nesting_depth_of_template_param(templ_param_list) ==
           templ_constant->variant.template_param.variant.coordinates.depth)) {
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
                       (a_host_large_unsigned)tap->variant.integer_value) == 0;
          if (match) {
            /* The values match.  Use the constant value instead of the
               integer array bound as the new value of the argument. */
            tap->is_array_bound_of_unknown_type = FALSE;
            tap->variant.constant = constant;
          }  /* if */
        }  /* if */
      } else {
        check_assertion(tap->arg_operand == NULL);
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
    } else {
      /* A template parameter constant in an expression context.  Check
         for the special case of a constant cast to a template parameter
         type.  This is needed, for examples such as this:

		template <class T, T t> struct A { };
		template <class T> void f(A<T,1>);
		void (*fp)(A<int, 1>) = f;
 
         In this case, the "1" in A<T,1> in the template declaration is
         cast to type T, so we have to be able to match up a "1" of type
	 "int" with a (T)1.  This is done by calling matches_template_type
         so ensure that T has type "int".  After this is done, a copy of the
         constant is made, and the constant is converted to the type of
         the constant (int in this case).  The conversion is needed for
         cases where the constant in the template declaration may have a
         different intrinsic type (e.g., if it were specified as '\001').
         Finally, matches_template_constant is called on the converted
         constant. */
      if (templ_constant->variant.template_param.kind ==
                             (a_template_param_constant_kind)tpck_cast) {
        if (matches_template_type(constant->type, templ_constant->type,
                                  templ_arg_list, templ_param_list,
                                  MTT_NO_FLAGS)) {
          a_constant_ptr	tcp; 
          tcp = templ_constant->variant.template_param.variant.constant;
          /* Make sure the constant under the cast is not a ck_template_param
             constant.  Such constants cannot be converted. */
          if (tcp->kind != (a_constant_repr_kind)ck_template_param) {
            a_constant	new_templ_constant;
            a_boolean	did_not_fold;
            copy_constant(tcp, &new_templ_constant);
            type_change_constant(&new_templ_constant, constant->type,
                                 /*is_implicit_cast=*/FALSE,
                                 /*constant_context=*/TRUE,
                                 /*evaluated_context=*/TRUE,
                                 /*fold_constant_addr_exprs=*/FALSE,
                                 /*is_reinterpret_cast=*/FALSE,
                                 /*maintain_expression=*/FALSE,
                                 &did_not_fold, &error_position);
            match = !did_not_fold &&
                    matches_template_constant(constant, &new_templ_constant,
                                              templ_arg_list,
                                              templ_param_list);
          }  /* if */
        }  /* if */
      }  /* if */
      /* A expression involving nontype parameters is a "nondeduced" context.
         This means that template parameters do not have their values deduced
         from this context, but instead use values deduced elsewhere.  The
         code above is still necessary though, because it permits a template
         parameter to be deduced from that context (which is important if
         that is the only reference to the template parameter from which
         it can be deduced).  Whether or not that succeeded, consider this
         a match for the time being. */
      match = TRUE;
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
       but is not a simple template parameter.  This is a nondeduced context.
       Consider this a match for now.  This expression will be evaluated in
       the deduction wrapup process and compared with the actual value. */
    match = TRUE;
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
      if (!tap->is_array_bound_of_unknown_type) {
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
      check_assertion(cp != NULL);
      if (is_integral_type(cp->type)) {
        /* An array bound can only match an integral value.  We have
           a match if the number of elements matches the previously
           deduced constant. */
        match = cmpulit_integer_constant(cp,
                                         (a_host_large_unsigned)elements) == 0;
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* matches_template_array_bound */


static a_boolean matches_template_arg_list(
				a_template_arg_ptr	tap,
				a_template_arg_ptr	templ_tap,
				a_template_arg_ptr	*templ_arg_list,
				a_template_param_ptr	templ_param_list)
/* This routine has a forward declaration earlier in this file. */
/*
Called by matches_template_type_for_class to determine whether a given
template argument list matches a template argument list from the parameter
list of a template function.  Also used to compare a template argument list
from a template class reference with a template argument list of a
partial specialization.
*/
{
  a_boolean	match = FALSE;

  do {
    if (tap->kind != templ_tap->kind) {
      /* The argument kinds do not match */
      match = FALSE;
    } else if (is_type_templ_arg(tap)) {
      /* A type template parameter.  See if the types match. */
      match = matches_template_type(tap->variant.type,
                                    templ_tap->variant.type,
                                    templ_arg_list,
                                    templ_param_list,
                                    MTT_NO_FLAGS);
    } else if (is_nontype_templ_arg(tap)) {
      /* A nontype template parameter. */
      match = matches_template_constant(tap->variant.constant,
                                        templ_tap->variant.constant,
                                        templ_arg_list,
                                        templ_param_list);
    } else {
      /* A template template argument. */
      match = matches_template_template_param(tap->variant.templ,
                                              templ_tap->variant.templ,
					      templ_arg_list,
					      templ_param_list);
    }  /* if */
    tap = tap->next;
    templ_tap = templ_tap->next;
  } while (match && tap != NULL && templ_tap != NULL);
  /* If either list has arguments remaining, this is not a match. */
  if ((tap == NULL) != (templ_tap == NULL)) match = FALSE;
  return match;
}  /* matches_template_arg_list */


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
  a_class_symbol_supplement_ptr cssp;
  a_symbol_ptr			primary_template;
  a_symbol_ptr			templ_primary_template;
  a_symbol_ptr			type_sym;

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
  check_assertion(is_immediate_class_type(type));
  type_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  cssp = type_sym->variant.class_struct_union.extra_info;
  primary_template = primary_template_of(cssp->class_template);
  templ_primary_template = primary_template_of(templ_cssp->class_template);
  if (templ_cssp->class_template != NULL &&
      primary_template == templ_primary_template &&
      templ_type->variant.class_struct_union.is_nonreal_class) {
    /* The two classes refer to the same template, but templ_type
       is a nonreal instantiation -- i.e., one based on template
       parameter types instead of real types. */
    a_template_arg_ptr  tap, templ_tap;
    tap = type->variant.class_struct_union.extra_info->
                                                   template_arg_list;
    templ_tap = templ_type->variant.class_struct_union.
                                       extra_info->template_arg_list;
    if (matches_template_arg_list(tap, templ_tap, templ_arg_list,
                                  templ_param_list)) {
      match = TRUE;
    }  /* if */
  } else if (identical_types(type, templ_type)) {
    /* If the two classes are not instances of the same template, check to
       see if they are the same types.  This may seem backward, but it
       is important that if type and templ_type are both A<T>, that the
       template argument lists are processed by the code above.  This is
       needed for binding template parameter values when doing partial
       ordering comparisons. */
    match = TRUE;
  } else if (templ_primary_template != NULL &&
             templ_primary_template->variant.template_info->
                             variant.class_template.template_template_param) {
    /* A class based on a template template parameter. */
    if (class_matches_template_template_param(type, templ_primary_template,
                                              templ_arg_list,
                                              templ_param_list)) {
      a_template_arg_ptr  tap, templ_tap;
      tap = type->variant.class_struct_union.extra_info->
                                                     template_arg_list;
      templ_tap = templ_type->variant.class_struct_union.
                                         extra_info->template_arg_list;
      if (matches_template_arg_list(tap, templ_tap, templ_arg_list,
                                    templ_param_list)) {
        match = TRUE;
      }  /* if */
    }  /* if */
  } else if (templ_type->source_corresp.is_class_member) {
    if (nonstandard_qualifier_deduction) {
      /* The parameter type is a class member -- a nested class or enum.  Be
         sure the parent classes match and that the members correspond
         (i.e., have the same name).  This occurs when the parameter has
         a type like "T::X<int>", and the actual argument is something
         like "Z::X<int>".  The WP does not permit this kind of deduction,
         but it is optionally supported for compatibility reasons. */
      if (!type->source_corresp.is_class_member) {
        /* No match. */
      } else {
        a_symbol_ptr	sym;
        a_symbol_ptr	templ_sym;
        sym = (a_symbol_ptr)type->source_corresp.assoc_info;
        templ_sym = (a_symbol_ptr)templ_type->source_corresp.assoc_info;
        if (sym->header != templ_sym->header) {
          /* Members have different names -- no match. */
        } else {
          a_class_symbol_supplement_ptr	ttp_cssp;
          a_type_ptr			tp;
          a_type_ptr			ttp;
          tp = type->source_corresp.parent.class_type;
          ttp = templ_type->source_corresp.parent.class_type;
          ttp_cssp = symbol_supplement_for_class(ttp);
          if (ttp_cssp->template_param_for_proxy_class != NULL) {
            /* The type being matches is a member of a proxy class.
               Substitute the original template parameter for the proxy
               class in the matching process. */
            ttp = ttp_cssp->template_param_for_proxy_class;
          }  /* if */
          if (matches_template_type(tp, ttp, templ_arg_list,
                                    templ_param_list,
                                    MTT_NO_FLAGS)) {
            /* Members have the same names and the parent classes
               "match". */
            match = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* The two classes do not otherwise match.  Determine whether the
         qualifier depends on a template parameter.  If the qualifier
         depends on a template parameter, then this is a "nondeduced"
         context in which the template argument values deduced elsewhere
         should be used to determine the type.  Consider this type to
         match for now.  The type that results from the substitution of
         the template argument values will be checked later. */
      a_class_symbol_supplement_ptr	ttp_cssp;
      a_type_ptr			ttp;
      ttp = templ_type->source_corresp.parent.class_type;
      ttp_cssp = symbol_supplement_for_class(ttp);
      if (ttp_cssp->template_param_for_proxy_class != NULL) {
        /* The type is a member of a proxy class.  Substitute the original
           template parameter for the proxy class in the matching process. */
        ttp = ttp_cssp->template_param_for_proxy_class;
      }  /* if */
      match = is_or_contains_template_param(ttp);
    }  /* if */
  }  /* if */
  return match;
}  /* matches_template_type_for_class_type */


a_boolean matches_template_type(a_type_ptr           type,
                                a_type_ptr           templ_type,
                                a_template_arg_ptr   *templ_arg_list,
				a_template_param_ptr templ_param_list,
				an_mtt_flag_set      flags)
/*
Compare type and templ_type.  The latter is from a parameter list of a
function template (function params, not template params).  If the types are
identical, return TRUE.  If they are identical but for a template parameter,
return TRUE if the type is consistent with other uses of that template
parameter, as represented in the template argument list.  Otherwise, return
FALSE.  When for the nth template parameter, the nth template arg has not
yet been created, extend the template argument list to include n entries.
flags specifies a set of options used to control how the type matching
is done.  See the MTT flag definitions in templates.h.  templ_param_list
points to the template parameter list.
*/
{
  a_boolean                      match = FALSE;
  a_type_ptr                     tp, ttp;
  a_param_type_ptr               ptp, tptp;
  a_template_arg_ptr             tap;
  a_symbol_ptr                   sym, templ_sym;
  an_mtt_flag_set		 new_flags;
#if DEBUG
  a_type_ptr			 orig_type = type;
  a_type_ptr			 orig_templ_type = templ_type;

  db_enter(5, "matches_template_type");
  if (db_flag_is_set("mtt")) {
    fprintf(f_debug, "matches_template_type starting evaluation of type: ");
    db_type(orig_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* When this routine calls itself recursively, the recursive calls
     should not allow conversions or the special unknown this class
     type checks. */
  new_flags = MTT_NO_FLAGS;
  templ_type = skip_typedefs(templ_type);
  if (is_immediate_class_type(templ_type)) {
    /* If the template type is a proxy class for a template parameter,
       substitute the underlying template parameter for the deduction
       process. */
    a_class_symbol_supplement_ptr	cssp;
    cssp = symbol_supplement_for_class(templ_type);
    if (cssp->template_param_for_proxy_class != NULL) {
      templ_type = cssp->template_param_for_proxy_class;
    }  /* if */
  }  /* if */
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
      a_template_nesting_depth	depth_of_template;
      /* Get the template nesting depth as indicated by the first template
         parameter.  Any template parameters found in templ_type must be at
         the same level to participate in deduction. */
      depth_of_template = nesting_depth_of_template_param(templ_param_list);
      if (templ_type->variant.template_param.kind ==
                             (a_template_param_type_kind)tptk_param) {
        if (depth_of_template !=
            templ_type->variant.template_param.extra_info->coordinates.depth) {
          /* Template parameters from a different nesting depth.  This should
             only happen if templ_type is a type from a prototype instantiation
             that includes a template parameter type in the parent class. */
          match = identical_types(type, templ_type);
        } else {
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
            /* No type has been bound to this template argument yet, so just
               use "type".  This counts as a match. */
            tap->variant.type = type;
            match = TRUE;
          } else {
            /* A type was already bound to this template argument.  We have a
               match if and only if the new type is the same as the one
               already there. */
            if (identical_types(type, tap->variant.type)) {
              /* Okay. */
              match = TRUE;
            } else if (microsoft_bugs &&
                       f_identical_types(f_skip_typerefs(type),
                                         f_skip_typerefs(tap->variant.type),
                                         ITF_NO_FLAGS)) {
              /* The Microsoft compiler has a bug that ignores qualifiers
                 when comparing the two deduced values of a given template
                 argument.  Consider the deduction to match if the types are
                 the same after stripping qualifiers. */
              match = TRUE;
            } else {
              /* Not a match.  Return FALSE. */
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Skip typedefs on the real type. */
        type = skip_typedefs(type);
        if (templ_type->source_corresp.is_class_member) {
          /* This is a template parameter associated with a member of a
             proxy class (e.g., X in a type like T::X).  The members must have
             the same name (e.g., T::X matches A::X) and the parent classes
             must match.  The WP does not permit this kind of deduction, but
             it is optionally supported for compatibility reasons. */
          if (nonstandard_qualifier_deduction) {
            if (!type->source_corresp.is_class_member) {
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
                                            new_flags)) {
                    /* Members have the same names and the parent classes
                       "match".  This will handle cases like T::B. */
                    match = TRUE;
                  }  /* if */
                }  /* if */
                if (!match) {
                  /* Attempt to match on the class of which this is a
                     member. */
                  ttp = templ_type->source_corresp.parent.class_type;
                  if (matches_template_type(tp, ttp, templ_arg_list,
    				            templ_param_list,
                                            new_flags)) {
                    /* Members have the same names and the parent classes
                       "match".  This will handle cases like A<T>::B. */
                    match = TRUE;
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* if */
          } else {
            /* Determine whether the qualifier of the template type
               depends on a template parameter.  If it does, this is a
               "nondeduced" context in which the template argument
               values deduced elsewhere should be used to determine the
               type.  Consider this type to match for now.  The type
               that results from the substitution of the template
               argument values will be checked later. */
            a_class_symbol_supplement_ptr	ttp_cssp;
            ttp = templ_type->source_corresp.parent.class_type;
            ttp_cssp = symbol_supplement_for_class(ttp);
            if (ttp_cssp->template_param_for_proxy_class != NULL) {
              /* The type is a member of a proxy class.  Substitute the
                 original template parameter for the proxy class in
                 the matching process. */
              ttp = ttp_cssp->template_param_for_proxy_class;
            }  /* if */
            match = is_or_contains_template_param(ttp);
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
    if (templ_type_kind != type_kind) {
      /* No match. */
    } else {
      switch (type->kind) {
        case tk_class:
        case tk_struct:
        case tk_union:
          match = matches_template_type_for_class_type(type, templ_type,
                                                       templ_arg_list,
                                                       templ_param_list);
          if (!match && (flags & MTT_ALLOW_CONVERSION) != 0) {
            a_base_class_ptr	bcp;
            /* See if the type matches a base class type of actual argument
               type.  This is allows a Derived<T> to be passed to a function
               expecting a Base<T> as an argument. */
            complete_class_type_is_needed(type);
            bcp = type->variant.class_struct_union.extra_info->base_classes;
            while (bcp != NULL) {
              match = matches_template_type_for_class_type(bcp->type,
                                                           templ_type,
                                                           templ_arg_list,
                                                           templ_param_list);
              if (match) break;
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
                                          new_flags);
          }  /* if */
          break;
        case tk_array:
          /* Array types match if their element types match and the number of
             elements is the same. */
          check_assertion(!type->variant.array.is_variable_size_array &&
                          !templ_type->variant.array.is_variable_size_array);
          if (type->variant.array.is_template_dependent_size_array &&
              templ_type->variant.array.is_template_dependent_size_array) {
            /* Both the type and the template type are dependent size
               arrays.  This should only occur when comparing two
               types that are actually template types during partial
               ordering comparisons. */
            a_constant_ptr cp =
                           type->variant.array.variant.element_count_constant;
            a_constant_ptr templ_cp =
                     templ_type->variant.array.variant.element_count_constant;
            match = matches_template_constant(cp, templ_cp,
                                              templ_arg_list,
                                              templ_param_list);
          } else if (
                 templ_type->variant.array.is_template_dependent_size_array) {
            /* The type from the template has a variable size.  If the
               variable size is a constant that refers to a template
               parameter, then this could be a match. */
            a_constant_ptr cp =
                     templ_type->variant.array.variant.element_count_constant;
            a_targ_size_t  elements;
            elements = type->variant.array.variant.number_of_elements;
            match = matches_template_array_bound(elements, cp,
                                                 templ_arg_list,
                                                 templ_param_list);
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
                                          new_flags);
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
                                          new_flags);
          }  /* if */
          break;
        case tk_ptr_to_member:
          /* For ptr-to-member types, there needs to be a match on both the
             member types and the class-of-which-a-member. */
          tp = type->variant.ptr_to_member.type;
          ttp = templ_type->variant.ptr_to_member.type;
          if (matches_template_type(tp, ttp, templ_arg_list,
                                    templ_param_list,
                                    new_flags)) {
            tp = type->variant.ptr_to_member.class_of_which_a_member;
            ttp = templ_type->variant.ptr_to_member.class_of_which_a_member;
            match = (matches_template_type(tp, ttp, templ_arg_list,
                                           templ_param_list,
                                           new_flags));
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
                                    new_flags) &&
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
                                         new_flags)) {
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
                 this classes, if present, match. */
              tp =  type->variant.routine.extra_info->this_class;
              ttp =  templ_type->variant.routine.extra_info->this_class;
              if (tp == NULL || ttp == NULL) {
                /* One or both of the types does not have an implicit
                   this class.  This is okay if they are both NULL. 
                   It is also okay if the type has no this class type,
                   the unknown this class type flag was passed in, and
                   the other this class type has no qualifiers. */
                if (tp == ttp) {
                  /* They are both NULL, this is a match. */
                  match = TRUE;
                } else if (ttp == NULL) {
                  /* The template type is NULL and the other type is not.
                     This is not a match. */
                  match = FALSE;
                } else { /* tp == NULL */
                  /* The template type is not NULL.  This is a match when
                     the unknown this class flag is set and the this
                     parameter from the template has no qualifiers. */
                  match = FALSE;
                  if ((flags & MTT_UNKNOWN_THIS_CLASS_TYPE) != 0) {
                    match = templ_type->variant.routine.extra_info->qualifiers
                                              == (a_type_qualifier_set)TQ_NONE;
                  }  /* if */
                }  /* if */
              } else {
                /* They both have this class types, make sure the
                   types match.  Construct an implicit this type so that
                   the qualifiers will be processed too. */
                tp =  implicit_this_param_type_of(type);
                ttp =  implicit_this_param_type_of(templ_type);
                match = matches_template_type(tp, ttp, templ_arg_list,
                                              templ_param_list,
                                              new_flags);
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
#if DEBUG
  if (db_flag_is_set("mtt")) {
    fprintf(f_debug, "matches_template_type type: ");
    db_type(orig_type);
    fprintf(f_debug, ", templ_type; ");
    db_type(orig_templ_type);
    fprintf(f_debug, ", match=%s\n", match ? "TRUE" : "FALSE");
  }  /* if */
#endif /* DEBUG */
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
                                 templ_param_list, MTT_NO_FLAGS);
  if (templ_arg_list != NULL) free_template_arg_list(templ_arg_list);
  db_exit();
  return result;
}  /* tentatively_matches_template_type */


a_boolean is_template_param_from_list(
		        a_template_param_coordinate_ptr	coordinates,
			a_template_param_ptr		templ_param_list)
/*
Return TRUE if the template parameter described by coordinates is
from the list specified by templ_param_list.

This is done by comparing the nesting depth specifies by coordinates with
that of a parameter from templ_param_list.
*/
{
  a_boolean	result;

  result = coordinates->depth ==
                             nesting_depth_of_template_param(templ_param_list);
  return result;
}  /* is_template_param_from_list */


static a_template_ptr copy_template_with_substitution(
			a_template_ptr			templ,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
If "templ" is a template associated with a template template parameter
return the corresponding actual template template argument (if any).
Otherwise, return the original template.
*/
{
  a_template_ptr			result = templ;
  a_template_symbol_supplement_ptr	tssp;

  if (templ->source_corresp.is_class_member) {
    a_symbol_ptr	sym;
    a_type_ptr		parent_type;
    sym = (a_symbol_ptr)templ->source_corresp.assoc_info;
    parent_type = templ->source_corresp.parent.class_type;
    check_assertion(sym != NULL);
    sym = copy_parent_type_with_substitution(sym, parent_type,
                                             templ_arg_list, templ_param_list,
                                             source_pos,
                                             /*is_type=*/FALSE,
                                             options,
                                             copy_error);
    if (sym == NULL || !is_class_template_symbol(sym)) {
      /* The type was specified as something like A<T>::B, but the
         substituted "A<T>" does not contain a B, or the B found is not
         a template. */
      *copy_error = TRUE;
      sym = error_class_template();
    }  /* if */
    templ = sym->variant.template_info->il_template_entry;
    result = templ;
  }  /* if */
  /* Get the associated template symbol supplement. */
  tssp = template_supplement_for_template(templ);
  if (tssp->variant.class_template.template_template_param) {
    /* If this template parameter entry corresponds to the nth
       parameter, the real template to substitute for it is given in the nth
       template argument.  Find the template argument that matches this
       template parameter use it. */
    a_template_param_coordinate_ptr	coordinates;
    coordinates = &templ->coordinates;
    if (!is_template_param_from_list(coordinates,
                                     templ_param_list)) {
      /* A template parameter from a different nesting depth or from a
         different template parameter list.  Leave this template
         unsubstituted. */
    } else {
      a_template_arg_ptr	tap;
      tap = get_template_arg_by_list_pos((a_template_param_ptr)NULL,
                                         &templ_arg_list,
                                         coordinates->position);
      if (tap->variant.templ == NULL) {
        /* No value has been provided for this template parameter yet.
           Don't do the substitution, but don't consider this to be
           a copy error either. */
      } else {
        /* Use the template specified by this template argument. */
        result = tap->variant.templ;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* copy_template_with_substitution */


a_template_arg_ptr copy_template_arg_list_with_substitution(
			a_template_arg_ptr		arg_list_to_copy,
			a_template_param_ptr		param_list_for_copy,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
Copy the template argument list arg_list_to_copy, and return a pointer
to the copy.  In the process of copying, replace any template parameters at
with the corresponding values from the template argument list templ_arg_list.
templ_param_list is the template parameter list for which templ_arg_list
is an argument list.

param_list_for_copy gives the corresponding template parameter list,
or is NULL if the parameter list is not known (e.g., for a nonreal
instantiation).  source_pos indicates the source position of the
argument list.  options is a set of bit flags used to control how
names are looked up, if needed.  If there is an error in the copying,
set *copy_error to TRUE.
*/
{
  a_template_arg_ptr	tap;
  a_template_arg_ptr	new_list;
  a_template_arg_ptr	new_tap;
  a_template_arg_ptr	prev_new_tap;
  a_template_param_ptr	tpp;
  a_boolean		have_params = (param_list_for_copy != NULL);

  prev_new_tap = new_list = NULL;
  for (tap = arg_list_to_copy, tpp = param_list_for_copy;
       tap != NULL;
       tap = tap->next, tpp = have_params ? tpp->next : NULL) {
    new_tap = alloc_template_arg(tap->kind);
    /* If there are too few parameters, the copy should fail. */
    if (have_params && tpp == NULL) {
      *copy_error = TRUE;
      break;
    }  /* if */
    /* Make sure that the template argument kind matches the parameter
       kind. */
    if (have_params) {
      a_symbol_kind	param_sym_kind;
      param_sym_kind = tpp->param_symbol->kind;
      if (templ_arg_kind_for_symbol_kind(param_sym_kind) != tap->kind) {
        /* The argument kinds do not match. */
        *copy_error = TRUE;
        break;
      }  /* if */
    }  /* if */
    if (is_type_templ_arg(tap)) {
      new_tap->variant.type =
               copy_type_with_substitution(tap->variant.type,
                                           templ_arg_list, templ_param_list,
					   source_pos, options, copy_error);
    } else if (is_nontype_templ_arg(tap)) {
      /* Perform the substitution on the type of the constant. */
      a_type_ptr	const_type;
      a_type_ptr	new_const_type;
      /* Pass in the expected type of the constant, i.e., the type of
         the template parameter after substitution.  A NULL pointer is
         passed if we do not know the parameter type. */
      new_const_type = NULL;
      if (have_params) {
        const_type = tpp->param_symbol->variant.constant->type;
        new_const_type = copy_type_with_substitution(const_type,
                                                     templ_arg_list,
                                                     templ_param_list,
                                                     source_pos, options,
                                                     copy_error);
      }  /* if */
      new_tap->variant.constant =
         copy_template_param_con_with_substitution(tap->variant.constant,
                                                   templ_arg_list,
                                                   templ_param_list,
						   new_const_type,
                                                   source_pos,
                                                   options, copy_error);
    } else {
      /* A template template argument. */
      new_tap->variant.templ = copy_template_with_substitution(
                                    tap->variant.templ, templ_arg_list,
                                    templ_param_list,
                                    source_pos, options, copy_error);
    }  /* if */
    if (new_list == NULL) {
      new_list = new_tap;
    } else {
      prev_new_tap->next = new_tap;
    }  /* if */
    prev_new_tap = new_tap;
  }  /* for */
  /* If there are too many parameters, the copy should fail. */
  if (have_params && tpp != NULL) {
    *copy_error = TRUE;
  }  /* if */
  return new_list;
}  /* copy_template_arg_list_with_substitution */


static a_symbol_ptr copy_template_class_reference_with_substitution(
			a_symbol_ptr			template_sym,
			a_type_ptr			orig_type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
Copy, with substitution, the template argument list from orig_type and
find the corresponding instance of the template indicated by
template_sym.  options is a set of bit flags used to control how names
are looked up, if needed.  The symbol of the new instance is returned.
*/
{
  a_template_arg_ptr			new_list;
  a_symbol_ptr				new_sym;
  a_template_arg_ptr			tap;
  a_template_param_ptr			tpp = NULL;
  a_template_symbol_supplement_ptr	tssp;
  a_boolean				is_nonreal_template;
  a_boolean				orig_is_prototype;
  
  template_sym = primary_template_of(template_sym);
  /* If the template symbol refers to a template template parameter, get
     the actual template to use from the template argument list. */
  if (template_sym->is_template_param) {
    a_template_ptr	new_templ;
    new_templ = template_sym->variant.template_info->il_template_entry;
    new_templ = copy_template_with_substitution(new_templ, templ_arg_list,
                                                templ_param_list, source_pos,
                                                options, copy_error);
    template_sym = (a_symbol_ptr)new_templ->source_corresp.assoc_info;
  }  /* if */
  tssp = template_sym->variant.template_info;
  tap = orig_type->variant.class_struct_union.extra_info->template_arg_list;
  orig_is_prototype = orig_type->
                        variant.class_struct_union.is_prototype_instantiation;
  is_nonreal_template = tssp->is_nonreal_member;
  if (!is_nonreal_template) {
    /* Except for nonreal templates, get the corresponding template parameter
       list. */
    tpp = tssp->cache.decl_info->parameters;
  }  /* if */
  /* Make a copy of the template argument list, doing substitution. */
  new_list = copy_template_arg_list_with_substitution(
                                           tap, tpp, templ_arg_list,
                                           templ_param_list, 
                                           source_pos, options, copy_error);
  if (*copy_error) {
    /* If an error occurred earlier, and in particular while creating one
       of the template arguments, don't try to find a matching template
       class. */
    new_sym = NULL;
  } else {
    a_boolean	prototype_allowed;
    prototype_allowed = orig_is_prototype ||
                        (options & CTWS_PROTOTYPE_ALLOWED) != 0;
    new_sym = find_template_class(template_sym, &new_list, prototype_allowed,
                                  (a_symbol_ptr)NULL);
  }  /* if */
  return new_sym;
}  /* copy_template_class_reference_with_substitution */


static a_type_ptr copy_array_type_with_substitution(
			a_type_ptr			type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
type points to an array type.  Copy, with substitution, the element type.
If the array type has a variable array dimension, do the substitution
on the ck_template_param constant pointed to by the expression.
*/
{
  a_constant_ptr	orig_cp = NULL;
  a_constant_ptr	new_cp = NULL;
  a_type_ptr		new_type;
  a_type_ptr		tp;
  a_type_ptr		new_array_type;

  tp = copy_type_with_substitution(type->variant.array.element_type,
                                   templ_arg_list, templ_param_list,
                                   source_pos,
                                   options, copy_error);
  /* Determine whether the number of elements is fixed, or whether
     it requires substitution. */
  if (type->variant.array.is_template_dependent_size_array) {
    /* The array size points to a template-dependent constant. */
    orig_cp = type->variant.array.variant.element_count_constant;
    new_cp = copy_template_param_con_with_substitution(
                      orig_cp, templ_arg_list, templ_param_list,
                      (a_type_ptr)NULL,
                      source_pos, options, copy_error);
  }  /* if */
  if (tp == type->variant.array.element_type &&
      orig_cp == new_cp) {
    /* Reuse the current type. */
    new_type = type;
  } else {
    if (is_function_type(tp) ||
        is_void_type(tp) ||
        is_reference_type(tp) ||
        (tp->kind == (a_type_kind)tk_array &&
         !has_unknown_specified_bound(tp) &&
         tp->variant.array.variant.number_of_elements == 0)) {
      /* The element type is invalid. */
      *copy_error = TRUE;
      new_type = NULL;
    } else {
      /* Create a new array type. */
      new_array_type = alloc_type((a_type_kind)tk_array);
      copy_type(type, new_array_type);
      new_array_type->variant.array.element_type = tp;
      if (orig_cp != new_cp) {
        if (new_cp->kind == (a_constant_repr_kind)ck_integer) {
          /* The substituted value is no longer template-dependent.  Extract
             that value and use it as a constant bound.  Note that
             overflow is ignored at this point. */
          a_boolean	overflow;

          new_array_type->
                       variant.array.is_template_dependent_size_array = FALSE;
          new_array_type->variant.array.variant.number_of_elements =
                 unsigned_value_of_integer_constant(new_cp, &overflow);
          if (overflow ||
              new_array_type->variant.array.variant.number_of_elements == 0) {
            /* If the array size is negative or zero, indicate a type error. */
            *copy_error = TRUE;
          }  /* if */
        } else if (is_error_constant(new_cp)) {
          *copy_error = TRUE;
        } else {
          /* The substituted value is still a ck_template_param
             constant. */
          check_assertion(new_cp->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
                          new_array_type->
                               variant.array.is_template_dependent_size_array);
          new_array_type->variant.array.variant.element_count_constant=new_cp;
        }  /* if */
      }  /* if */
      new_type = new_array_type;
      /* Compute the array size based on the substituted element type and
         number of elements. */
      if (!set_array_type_size(new_type, /*suppress_error=*/TRUE)) {
        /* The resulting array size is too large. */
        *copy_error = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return new_type;
}  /* copy_array_type_with_substitution */


a_type_ptr type_if_unknown_conversion_function_symbol(a_symbol_ptr	sym)
/*
If "sym" is a ck_template_parameter constant of kind tpck_unknown_function
that represents an unknown conversion function, return the conversion type,
otherwise return NULL;
*/
{
  a_type_ptr	result = NULL;

  if (sym->kind == (a_symbol_kind)sk_constant) {
    a_constant_ptr	cp;
    cp = sym->variant.constant;
    if (cp->kind == (a_constant_repr_kind)ck_template_param) {
      if (cp->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_unknown_function) {
        result =
           cp->variant.template_param.variant.unknown_function.conversion_type;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* type_if_unknown_conversion_function_symbol */


static a_symbol_ptr look_up_member_in_substituted_parent(
			a_symbol_ptr			orig_sym,
			a_type_ptr			parent_type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_boolean			is_type,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
parent_type is a class type that has been substituted.  orig_sym is the
symbol from the original parent type.  is_type is TRUE if the entity
being looked up is known to be a type.
*/
{
  a_type_ptr			conv_type;
  a_symbol_ptr			new_sym = NULL;

  complete_class_type_is_needed(parent_type);
  /* Determine whether orig_sym is an unknown conversion function symbol.
     If so, get its type. */
  conv_type = type_if_unknown_conversion_function_symbol(orig_sym);
  if (conv_type != NULL) {
    /* Substitute the any template parameters in the conversion type. */
    conv_type = copy_type_with_substitution(conv_type, templ_arg_list,
                                            templ_param_list,
                                            source_pos, options, copy_error);
    /* Look for a conversion function that converts to the new type. */
    new_sym = look_up_conversion_function(parent_type, conv_type, source_pos);
  } else {
    a_symbol_locator		locator;
    an_id_lookup_options_set	lookup_options;
    clear_locator(&locator, source_pos);
    locator.symbol_header = orig_sym->header;
    /* If the entity being looked up is known the be the parent of another
       entity, then it must be a class or a namespace.  Otherwise, use the
       is_type parameter to determine whether a typename lookup is needed. */
    if (options & CTWS_IS_PARENT) {
      lookup_options = IDL_MUST_BE_CLASS_OR_NAMESPACE;
    } else {
      lookup_options = is_type ? IDL_TYPENAME_LOOKUP : IDL_NO_OPTIONS;
    }  /* if */
    new_sym = class_qualified_id_lookup(&locator, parent_type, lookup_options);
  }  /* if */
  return new_sym;
}  /* look_up_member_in_substituted_parent */



a_symbol_ptr copy_parent_type_with_substitution(
			a_symbol_ptr			sym,
			a_type_ptr			parent_type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_boolean			is_type,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
sym points to a member symbol.  parent_type points to the parent type
of sym.  The parent type is copied using copy_type_with_substitution,
and the corresponding member is looked up in the updated parent type.
The symbol associated with the corresponding member is returned.  A
NULL symbol is returned if the updated parent type does not contain
the specified member.  If it involves no template-parameter type,
simply return "type".  options is a set of bit flags used to control
how names are looked up, if needed.  is_type is TRUE if the child
entity is known to be a type.
*/
{
  a_type_ptr			orig_parent_type;
  a_symbol_ptr			new_sym = NULL;
  a_class_symbol_supplement_ptr	parent_cssp;

  check_assertion(parent_type != NULL);
  /* Nested type case -- e.g., A<T>::B, where B names a nested class or
     enumeration.  The substitution is performed on the class-of-which-member
     rather than on the nested type itself.  Note that the algorithm deals
     with any nesting depth. */
  parent_cssp = symbol_supplement_for_class(parent_type);
  if (parent_cssp->template_param_for_proxy_class) {
    /* The parent type is a proxy class for a template parameter.  Substitute
       the original template parameter for the proxy class. */
    parent_type = parent_cssp->template_param_for_proxy_class;
  }  /* if */
  orig_parent_type = parent_type;
  /* Copy the parent type.  Pass in the CTWS_IS_PARENT flag so that only
     classes and namespace are considered for the lookup. */
  parent_type = copy_type_with_substitution(parent_type, templ_arg_list,
                                            templ_param_list, source_pos,
                                            options | CTWS_IS_PARENT,
                                            copy_error);
  if (*copy_error) goto done;
  if (parent_type == orig_parent_type) {
    /* No change to the parent class, so this is simply a case of A::B --
       i.e., the parent class is not a template reference.  Just return the
       original symbol. */
    new_sym = sym;
  } else if (!is_class_struct_union_type(parent_type)) {
    /* The new type is not a class type, and so cannot be a parent. */
    *copy_error = TRUE;
    goto done;
  } else {
    /* If the original parent type of "type" was A<T>, parent_type now
       represents a class with the substitution performed on the template
       parameter, e.g., A<int>.  If "type" was A<T>::B, we want to return as
       new_sym the corresponding member of the new type, e.g., A<int>::B. */
    new_sym = look_up_member_in_substituted_parent(
                             sym, parent_type, templ_arg_list,
                             templ_param_list, source_pos, is_type,
                             options, copy_error);
    if (new_sym != NULL && is_class_template_symbol(new_sym) &&
        !is_class_template_symbol(sym)) {
      /* The symbol found is a class template symbol but the original symbol
         was just a class.  Get the corresponding instance using the template
         argument list from the original parent class. */
      check_assertion(is_any_template_instance_class_symbol(sym));
      new_sym = copy_template_class_reference_with_substitution(
                                 new_sym, sym->variant.class_struct_union.type,
                                 templ_arg_list, templ_param_list, source_pos,
                                 options, copy_error);
    }  /* if */
  }  /* if */
done:
  return new_sym;
}  /* copy_parent_type_with_substitution */


a_type_ptr copy_type_with_substitution(
			a_type_ptr			type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error)
/*
If "type", a pointer to a type entry, is a template-parameter type, return
the corresponding real type, based on the template argument list.  If "type"
contains a template-parameter type, return a copy with the substitution made.
templ_param_list is the template parameter list for which templ_arg_list
is an argument list.

If the type involves no template-parameter type, simply return "type".
options is a set of bit flags used to control how names are looked up,
if needed.  *copy_error is set to TRUE if the substitution would have
created an invalid type.  A NULL type is also returned in such cases.
An invalid type can result from a type such as A<T>::B, if, for a
given T, A<T> contains no member named B.  Other cases include putting
a pointer over a reference type or creating an array of references.
*/
{
  a_type_ptr			new_type;
  a_type_ptr			tp;
  a_type_ptr			tp2;
  int				reusable_param_types;
  a_template_arg_ptr		tap;
  a_type_ptr			new_return_type;
  a_type_ptr			this_class;
  a_type_ptr			new_this_class;
  a_type_ptr			first_new_type_for_param_types_list;
  a_param_type_ptr		ptp;
  a_param_type_ptr		new_ptp;
  a_param_type_ptr		prev_ptp;
  a_class_symbol_supplement_ptr	cssp;

  db_enter(5, "copy_type_with_substitution");
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("ctws")) {
    fputs("in:  ", f_debug);
    db_type(type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  if (type->source_corresp.is_class_member) {
    a_symbol_ptr	sym;
    a_type_ptr		parent_type;
    sym = (a_symbol_ptr)type->source_corresp.assoc_info;
    parent_type = type->source_corresp.parent.class_type;
    check_assertion(sym != NULL);
    sym = copy_parent_type_with_substitution(sym, parent_type,
                                             templ_arg_list, templ_param_list,
                                             source_pos,
                                             /*is_type=*/TRUE,
                                             options,
                                             copy_error);
    if (sym == NULL || !is_type_symbol(sym)) {
      /* The type was specified as something like A<T>::B, but the
         substituted "A<T>" does not contain a B, or the B found is not
         a type. */
      *copy_error = TRUE;
      type = error_type();
    } else {
      type = type_symbol_type(sym);
    }  /* if */
  }  /* if */
  /* The CTWS_IS_PARENT flag should only influence the lookup of the parent
     type, if necessary, by copy_parent_type_with_substitution.  Reset the
     flag now. */
  options = options & ~CTWS_IS_PARENT;
  {
    switch (type->kind) {
      case tk_template_param:
        /* If this template parameter type entry corresponds to the nth
           parameter, the real type to substitute for it is given in the nth
           template argument.  Find the template argument that matches this
           template parameter and return it to the caller. */
        { a_template_param_coordinate_ptr	coordinates;
          coordinates = &type->variant.template_param.extra_info->coordinates;
          if (!is_template_param_from_list(coordinates,
                                           templ_param_list)) {
            /* A template parameter from a different nesting depth or from a
               different template parameter list.  Leave this template
               unsubstituted. */
            new_type = type;
          } else {
            tap = get_template_arg_by_list_pos((a_template_param_ptr)NULL,
                                               &templ_arg_list,
                                               coordinates->position);
            if (tap->variant.type == NULL) {
              /* No value has been provided for this template parameter yet.
                 Don't do the substitution, but don't consider this to be
                 a copy error either. */
              new_type = type;
            } else {
              new_type = tap->variant.type;
            }  /* if */
          }  /* if */
        }
        break;
      case tk_pointer:
        /* Make a pointer type based on a copy (or reuse, if copying is not
           required) of the type pointed to. */
        tp = type->variant.pointer.type;
        tp = copy_type_with_substitution(tp, templ_arg_list, templ_param_list,
                                         source_pos, options, copy_error);
        if (type->variant.pointer.is_reference) {
          if (!is_reference_type(tp) && !is_void_type(tp)) {
            new_type = make_reference_type(tp);
          } else {
            /* A reference to reference or reference to void would be
               invalid. */
            *copy_error = TRUE;
          }  /* if */
        } else {
          if (!is_reference_type(tp)) {
            new_type = make_pointer_type(tp);
          } else {
            /* A pointer to reference would be invalid. */
            *copy_error = TRUE;
          }  /* if */
        }  /* if */
        break;
      case tk_typeref:
        /* Make an identically qualified type of a copy (or reuse) of the type
           that underlies the typeref. */
        { a_type_qualifier_set	qualifiers;
          a_type_ptr		type_without_typerefs;
          type_without_typerefs = skip_typerefs(type);
          tp = copy_type_with_substitution(type_without_typerefs,
                                           templ_arg_list,
                                           templ_param_list, source_pos,
                                           options, copy_error);
          qualifiers = get_type_qualifiers(type);
          if (qualifiers != TQ_NONE && is_function_type(tp)) {
            /* An attempt to place a qualifier on top of a function type.
               This is not allowed. */
            *copy_error = TRUE;
          } else {
            new_type = make_qualified_type(tp, qualifiers);
          }  /* if */
        }
        break;
      case tk_ptr_to_member:
        /* Make a pointer to member type.  The current pointer to member type
           points to two types, so the new type is based on copies (or reuses)
           of each. */
        tp = copy_type_with_substitution(type->variant.ptr_to_member.type,
                                         templ_arg_list, templ_param_list,
                                         source_pos, options, copy_error);
        tp2 = copy_type_with_substitution(
                          type->variant.ptr_to_member.class_of_which_a_member,
                          templ_arg_list, templ_param_list, source_pos,
                          options, copy_error);
        if (tp == type->variant.ptr_to_member.type &&
            tp2 == type->variant.ptr_to_member.class_of_which_a_member) {
          /* There was no change -- use the original type. */
          new_type = type;
        } else {
          /* Construct a new type.  The new class of which a member type must
             be a class type or a template parameter type. */
          if (!is_class_struct_union_type(tp2) &&
              !is_template_param_type(tp2)) {
            /* The new type would be invalid. */
            *copy_error = TRUE;
            new_type = NULL;
          } else {
            new_type = ptr_to_member_type(tp, tp2);
          }  /* if */
        }  /* if */
        break;
      case tk_routine:
        /* We can reuse "type" as long as we can reuse the return type and all
           its param types.  Otherwise we will need to allocate a new type
           entry. Go through "type" until we find that a new type was returned
           from copy_type_with_substitution. */
        reusable_param_types = 0;
        first_new_type_for_param_types_list = NULL;
        new_return_type = copy_type_with_substitution(
                                        type->variant.routine.return_type,
                                        templ_arg_list, templ_param_list,
                                        source_pos, options, copy_error);
        this_class = type->variant.routine.extra_info->this_class;
        if (this_class == NULL) {
          new_this_class = NULL;
        } else {
          new_this_class = copy_type_with_substitution(
                                        this_class, templ_arg_list,
                                        templ_param_list, source_pos, options,
                                        copy_error);
          /* Drop any typedefs on the class, but not any qualifiers. */
          new_this_class = skip_typedefs(new_this_class);
          if (!is_immediate_class_type(new_this_class)) {
            /* The this class type must be a class type. */
            *copy_error = TRUE;
          }  /* if */
        }  /* if */
        if (new_return_type != type->variant.routine.return_type ||
            new_this_class != this_class) {
          /* A substitution was made on the return type or the this-param
             type, so a new routine type will be required. */
          goto make_new_type;
        }  /* if */
        /* Now examine each of the parameters. */
        for (ptp = type->variant.routine.extra_info->param_type_list;
             ptp != NULL;
             ptp = ptp->next) {
          tp = copy_type_with_substitution(ptp->type, templ_arg_list,
                                           templ_param_list, source_pos,
                                           options, copy_error);
          if (tp != ptp->type) {
            /* A substitution was made, so a new routine type will be required.
               Remember tp so we can avoid calling copy_type_with_substituion
               again for this param type entry. */
            first_new_type_for_param_types_list = tp;
            goto make_new_type;
          }  /* if */
          /* Keep track of the number of param type entries for which reuse of
             the existing type is okay. */
          ++reusable_param_types;
        }  /* for */
        /* Falling through to here means that no substitutions are required
           for this type.  Therefore it can simply be reused. */
        new_type = type;
        break;
make_new_type:
        /* Make a routine type based on "type".  Checking for reusable types
           has already been done for the return type and possibly for some of
           the parameter types. */
        new_type = alloc_type((a_type_kind)tk_routine);
        /* Fill in the return type.  It has already been determined. */
        new_type->variant.routine.return_type = new_return_type;
        /* Clone the routine type supplement, except for the pointers. */
        *(new_type->variant.routine.extra_info) =
                                         *(type->variant.routine.extra_info);
        new_type->variant.routine.extra_info->assoc_routine = NULL;
        new_type->variant.routine.extra_info->this_class = new_this_class;
        /* Make copies of the entries on type's param types list, making the
           appropriate substitutions for template parameter type entries. */
        prev_ptp = NULL;
        for (ptp = type->variant.routine.extra_info->param_type_list;
             ptp != NULL;
             ptp = ptp->next) {
          if (reusable_param_types > 0) {
            /* We have already called copy_type_with_substitution for this
               parameter and we know we can reuse the existing type. */
            tp = ptp->type;
            --reusable_param_types;
          } else if (first_new_type_for_param_types_list != NULL) {
            /* We have already called copy_type_with_substitution for this
               parameter and the type returned contained a substitution; we can
               use that type. */
            tp = first_new_type_for_param_types_list;
            first_new_type_for_param_types_list = NULL;
          } else {
            /* copy_type_with_substitution has not been called yet. */
            tp = copy_type_with_substitution(ptp->type, templ_arg_list,
                                             templ_param_list, source_pos,
                                             options, copy_error);
          }  /* if */
          if (tp != ptp->type) {
            /* The type is not the one originally pointed to.  Adjust
               the parameter type, if needed. */
            adjust_parameter_type(&tp, /*array_qualifiers=*/TQ_NONE);
            if (remove_qualifiers_from_param_types) { /* Strip off
                 top-level type qualifiers.  They are not part of the
                 type signature of a C++ function -- see 8.3.5 para 3.
                 However, because they do belong to the type of the
                 parameter variable, they were not removed before
                 add_to_param_id_list was called. */
               tp = make_unqualified_type(tp);
            }  /* if */
          }  /* if */
          /* Allocate the param type entry and copy default arg info. */
          new_ptp = make_param_type(tp, &null_source_position);
          if (ptp->has_default_arg) {
            new_ptp->has_default_arg = TRUE;
          }  /* if */
          if (ptp->has_unevaluated_template_default) {
            new_ptp->has_unevaluated_template_default = TRUE;
          }  /* if */
          /* Add the new param type entry to the param types list. */
          if (prev_ptp == NULL) {
            new_type->variant.routine.extra_info->param_type_list = new_ptp;
          } else {
            prev_ptp->next = new_ptp;
          }  /* if */
          prev_ptp = new_ptp;
        }  /* for */
        set_routine_calling_method_flag(new_type, &null_source_position);
        break;
      case tk_array:
        new_type = copy_array_type_with_substitution(
                                      type, templ_arg_list, templ_param_list,
                                      source_pos, options, copy_error);
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        cssp = symbol_supplement_for_class(type);
        if (!type->variant.class_struct_union.is_nonreal_class) {
          /* Reuse the current type. */
          new_type = type;
        } else if (cssp->template_param_for_proxy_class != NULL) {
          /* The proxy class for a template parameter.  Use the substituted
             template parameter type. */
          a_type_ptr	templ_param_for_type;
          templ_param_for_type = cssp->template_param_for_proxy_class;
          new_type = copy_type_with_substitution(templ_param_for_type,
                                                 templ_arg_list,
                                                 templ_param_list,
                                                 source_pos, options,
                                                 copy_error);
          /* If the template parameter is not substituted, retain the
             original proxy class type. */
          if (new_type == templ_param_for_type) new_type = type;
        } else {
          a_symbol_ptr		new_sym;
          if (cssp->class_template == NULL) {
            /* A nonreal class, but not a template class.  Retain the current
               type. */
            new_type = type;
          } else {
            /* Substitute the template arguments. */
            tap = type->variant.class_struct_union.extra_info->
                                                             template_arg_list;
            new_sym = copy_template_class_reference_with_substitution(
                            cssp->class_template, type, templ_arg_list,
                            templ_param_list, source_pos, options, copy_error);
            if (new_sym == NULL || !is_type_symbol(new_sym)) {
              /* The type was specified as something like A<T>::B, but the
                 substituted "A<T>" does not contain a B, or the B found is not
                 a type. */
              *copy_error = TRUE;
              new_type = error_type();
            } else {
              new_type = type_symbol_type(new_sym);
            }  /* if */
          }  /* if */
        }  /* if */
        break;
      default:;
        /* No modification required. */
        new_type = type;
    }  /* switch */
  }
  /* Return an error type pointer if a copy error occurred. */
  if (*copy_error) new_type = error_type();
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("ctws")) {
    fputs("out: ", f_debug);
    if (*copy_error) {
      fprintf(f_debug, "<copy error>");
    } else {
      db_type(new_type);
    }  /* if */
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return new_type;
}  /* copy_type_with_substitution */


#if GENERATE_SOURCE_SEQUENCE_LISTS
a_type_ptr instantiate_type_for_template_function(a_type_ptr     type,
                                                  a_routine_ptr  routine)
/*
Given a parameterized type, return the instantiation of that type for the
template arguments with which the template function routine was instantiated.
*/
{
  a_symbol_ptr  rout_sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
  a_template_instance_ptr
                tip = rout_sym->variant.routine.instance_ptr;
  a_template_symbol_supplement_ptr
                tssp = tip->template_sym->variant.template_info;
  a_template_param_ptr
                templ_param_list = tssp->cache.decl_info->parameters;
  a_boolean     copy_error = FALSE;

  return copy_type_with_substitution(
                             type,
                             routine->template_arg_list,
                             templ_param_list,
                             &tip->template_sym->decl_position,
                             CTWS_NO_OPTIONS, &copy_error);
}  /* instantiate_type_for_template_function */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


static void add_to_substituted_types_list(
			a_template_symbol_supplement_ptr	tssp,
			a_template_arg_ptr			templ_arg_list,
			a_type_ptr				type)
/*
Create a new substituted types list entry and add it to the list of
substituted types associated with tssp.  A copy of the template
argument list is created so that the original list can be freed by
the caller.
*/
{
  a_substituted_type_list_entry_ptr	stlep;

  stlep = alloc_substituted_type_list_entry();
  stlep->templ_arg_list = copy_template_arg_list(templ_arg_list);
  stlep->type = type;
  stlep->next = tssp->variant.function.substituted_types;
  tssp->variant.function.substituted_types = stlep;
}  /* add_to_substituted_types_list */


static a_type_ptr find_substituted_type(
			a_template_symbol_supplement_ptr	tssp,
			a_template_arg_ptr			templ_arg_list)
/*
Determine whether a type has already been created for a given set of template
arguments (specified by templ_arg_list).  tssp points to the template symbol
supplement associated with the function template being used.
*/
{
  a_substituted_type_list_entry_ptr	stlep;
  a_type_ptr				result_type = NULL;

  for (stlep = tssp->variant.function.substituted_types;
       stlep != NULL; stlep = stlep->next) {
    if (equiv_template_arg_lists(templ_arg_list, stlep->templ_arg_list,
                                 ETA_NO_OPTIONS)) {
      result_type = stlep->type;
      break;
    }  /* if */
  }  /* for */
  return result_type;
}  /* find_substituted_type */


a_type_ptr substitute_template_arguments(
				a_symbol_ptr		templ_sym,
				a_template_arg_ptr	templ_arg_list,
				a_template_arg_ptr	*new_arg_list,
				a_template_param_ptr	templ_param_list)
/*
In the function template specified by templ_sym, replace the template
parameters in the function type with the values specified by
templ_arg_list and return the resulting type.  If the substitution
would result in an invalid type, a NULL type is returned.  If
new_arg_list is non-NULL, templ_arg_list is an explicitly specified
template argument list that must be converted into an argument list
appropriate for the specified template.  The new argument list is
returned in *new_arg_list.  templ_param_list is the template parameter
list to be used.  If a NULL pointer is provided, the template
parameter list from the template symbol supplement is used.  The
parameter is supplied because some calls of this routine occur before
the field in the template symbol supplement has been set.
*/
{
  a_boolean				copy_error = FALSE;
  a_template_symbol_supplement_ptr	tssp;
  a_type_ptr				templ_rout_type = NULL;

  tssp = template_supplement_for_symbol(templ_sym);
  if (templ_param_list == NULL) {
    /* Get the template parameter list, if one was not passed in. */
    templ_param_list = tssp->variant.function.decl_cache.decl_info->parameters;
  }  /* if */
  if (new_arg_list != NULL) {
    /* An explicit template argument list was specified, initialize the
       new template argument list with the specified list.  If the new
       list that is returned is NULL, the explicit argument list didn't
       match the template parameter list, so no further processing of this
       template should be done. */
    templ_arg_list = create_initial_template_arg_list(
                                           templ_param_list, templ_arg_list,
                                           &templ_sym->decl_position);
    *new_arg_list = templ_arg_list;
  }  /* if */
  if (templ_arg_list != NULL) {
    /* See whether copy_type_with_substitution has already been done for
       this template argument list.  If so, simply return the type
       already created. */
    templ_rout_type = find_substituted_type(tssp, templ_arg_list);
    if (templ_rout_type == NULL) {
      /* This is the first time this routine has been called for this
         template argument list.  Create a new type. */
      templ_rout_type = skip_typerefs(tssp->variant.function.routine->type);
      templ_rout_type = copy_type_with_substitution(templ_rout_type,
                                                    templ_arg_list,
                                                    templ_param_list,
	       					    &templ_sym->decl_position,
						    CTWS_NO_OPTIONS,
						    &copy_error);
      if (copy_error) templ_rout_type = NULL;
      if (templ_rout_type != NULL) {
        /* Reset the flags in the param type entry to reflect whether the
           parameter contains any template parameters that participate in
           template argument deduction. */
        set_type_involves_deduced_template_param(templ_rout_type);
      }  /* if */
      /* Add the new type to the list of substituted types. */
      add_to_substituted_types_list(tssp, templ_arg_list, templ_rout_type);
    }  /* if */
  }  /* if */
  return templ_rout_type;
}  /* substitute_template_arguments */


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
    check_assertion_str2((is_type_templ_arg(tap) &&
                          tap->variant.type != NULL) ||
                         (is_nontype_templ_arg(tap) &&
                          tap->variant.constant != NULL) ||
                         (is_template_templ_arg(tap) &&
                          tap->variant.templ != NULL),
                         "check_template_arg_list:",
                         "missing type, constant, or template  pointer");
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


void instantiate_default_argument(a_symbol_ptr		rout_sym,
				  a_param_type_ptr	param)
/*
Rescan a default argument of a template function, or member function
of a template class.  rout_sym points to the symbol of the function
instance with which the parameter is associated.  param points to the
parameter list entry for the parameter whose default argument is to be
instantiated.
*/
{
  a_def_arg_expr_fixup_ptr		daefp;
  a_param_type_ptr			templ_ptp;
  a_param_type_ptr			ptp;
  a_routine_ptr				templ_rout;
  a_routine_ptr				rout_ptr;
  a_type_ptr				templ_rout_type;
  a_type_ptr				rout_type;
  a_template_instance_ptr		tip;
  a_symbol_ptr				template_sym;
  int					arg_num, i;
  a_template_symbol_supplement_ptr	tssp;

  check_assertion(rout_sym->kind == (a_symbol_kind)sk_routine ||
                  rout_sym->kind == (a_symbol_kind)sk_member_function);
  rout_ptr = rout_sym->variant.routine.ptr;
  rout_type = skip_typerefs(rout_ptr->type);
  ptp = rout_type->variant.routine.extra_info->param_type_list;
  /* Determine the argument number that "param" represents. */
  for (arg_num = 1; ptp != NULL; ptp = ptp->next, arg_num++) {
    if (ptp == param) break;
  }  /* for */
  if (param->default_being_instantiated) {
    /* This default argument (for this instance) is already being instantiated.
       Don't attempt another instantiation. */
    error(ec_recursive_def_arg_instantiation);
    goto done;
  }  /* if */
  /* Indicate that an instantiation of this default argument is pending. */
  param->default_being_instantiated = TRUE;
  tip = rout_sym->variant.routine.instance_ptr;
  check_assertion(tip != NULL);
  template_sym = tip->template_sym;
  tssp = template_supplement_for_symbol(template_sym);
  templ_rout = tssp->variant.function.routine;
  templ_rout_type = skip_typerefs(templ_rout->type);
  daefp = tssp->variant.function.def_arg_expr_list;
  /* Find the param type entry for the "prototype" template routine that
     corresponds to the argument number determined above. */
  templ_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
  for (i = arg_num; i > 1; i--, templ_ptp = templ_ptp->next) {
    if (daefp == NULL || templ_ptp == NULL) {
      daefp = NULL;
      break;
    }  /* if */
    /* Only skip to the next default argument fixup entry when we encounter
       a parameter with a default argument. */
    if (templ_ptp->has_default_arg) daefp = daefp->next;
  }  /* for */
  /* We should always find the corresponding parameter of the template,
     unless some error occurred earlier. */
  check_assertion(daefp != NULL || total_errors != 0);
  /* Now that we've found the corresponding parameter of the template,
     instantiate that default argument value. */
  if (daefp != NULL) {
    /* Push the template instantiation scope for the context in which the
       default argument is to be evaluated. */
    push_template_instantiation_scope(daefp->cache.decl_info,
                                      (a_type_ptr)NULL, rout_ptr,
                                      tip->instance_sym,
                                      tip->template_sym,
                                      rout_ptr->template_arg_list,
                                      /*push_stop_tokens=*/TRUE,
				      PS_NO_OPTIONS);
    /* The function prototype scope should be reactivated and its symbols
       reentered because parameter names hide names from enclosing scopes
       and, moreover, may not be used in default argument expressions
       (3.4.1p11). */
    (void)push_scope((a_scope_kind)sck_func_prototype,
                     daefp->cache.decl_info->declaration_scope,
                     rout_ptr->type, (a_routine_ptr)NULL);
    if (tip->prototype_scope_symbols != NULL) {
      reactivate_prototype_scope_symbols(tip->prototype_scope_symbols);
    }  /* if */
    /* Update the default argument expression entry to point to the
       current param type entry. */
    daefp->param_type = ptp;
    /* Rescan the default argument tokens from the cache. */
    rescan_reusable_cache(&daefp->cache.tokens);
    delayed_scan_of_default_arg_expr(daefp->param_type,
                                     /*check_for_errors=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Copy the default argument expression into the corresponding param
       type entry of the declared type, if any. */
    if (tip->declared_type_for_default_arg_fixup != NULL) {
      ptp = tip->declared_type_for_default_arg_fixup->
                       variant.routine.extra_info->param_type_list;
      for (i = arg_num; i > 1; i--, ptp = ptp->next) {
        check_assertion(ptp != NULL);
      }  /* if */
      if (ptp->default_arg_expr == NULL) {
        ptp->has_default_arg = TRUE;
        ptp->default_arg_expr =
              duplicate_default_arg_expr(daefp->param_type->default_arg_expr);
      }  /* if */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Pop the reactivated function prototype scope off the stack. */
    pop_scope();
    /* Pop the template instantiation scope. */
    pop_template_instantiation_scope();
  }  /* if */
done:
  /* Reset the flag that indicates that this default value has not yet
     been evaluated. */
  param->has_unevaluated_template_default = FALSE;
  param->default_being_instantiated = FALSE;
}  /* instantiate_default_argument */


void check_for_function_template_default_args(
		    a_routine_ptr		     templ_rout,
		    a_routine_ptr		     rout_ptr,
                    a_template_symbol_supplement_ptr tssp)
/*
Determine which of the parameters of the template instance specified by
rout_ptr have default arguments.  Note that the defaults are not actually
scanned here.  They will be scanned later, only if the value of the default
is needed for a call.
*/
{
  a_def_arg_expr_fixup_ptr	daefp;
  a_param_type_ptr		templ_ptp;
  a_param_type_ptr		ptp;
  a_type_ptr			templ_rout_type;
  a_type_ptr			rout_type;

  templ_rout_type = skip_typerefs(templ_rout->type);
  rout_type = skip_typerefs(rout_ptr->type);
  daefp = tssp->variant.function.def_arg_expr_list;
  if (daefp != NULL) {
    templ_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    /* Loop through the two linked lists of param_type entries and the
       default argument expression fixup entries, and update the default
       argument flags in the corresponding param_type entries. */
    for (; ptp != NULL; ptp = ptp->next, templ_ptp = templ_ptp->next) {
      if (templ_ptp == NULL) {
        /* There is a mismatch in the number of default arguments between
           the template and the instance.  This should only occur as the
           result of some other error. */
        check_assertion(total_errors != 0);
        break;
      }  /* if */
      if (templ_ptp->has_default_arg) {
	check_assertion(daefp != NULL);
        /* Update the default argument expression entry to point to the
           current param type entry. */
        ptp->has_default_arg = TRUE;
        ptp->has_unevaluated_template_default = TRUE;
        daefp = daefp->next;
      }  /* if */
    }  /* for */
    check_assertion(daefp == NULL || total_errors != 0);
  }  /* if */
}  /* check_for_function_template_default_args */


static a_type_ptr create_error_routine_type(a_routine_ptr	templ_rout,
					    a_type_ptr		parent_class)
/*
Given a routine pointer (templ_rout) create a new routine type entry
with the same number of parameters as templ_rout, but all of whose
parameter types are error types, and whose return type is also an error
type.  parent_class points to the class type of which the routine with
the error type is a member, or is NULL for a nonmember.
*/
{
  a_type_ptr			rout_type;
  a_routine_type_supplement_ptr	rtsp;
  a_type_ptr			templ_rout_type;
  a_routine_type_supplement_ptr	templ_rtsp;
  a_param_type_ptr		ptp;
  a_param_type_ptr		last_ptp = NULL;
  a_param_type_ptr		templ_ptp;
  a_type_ptr			error_type_ptr;

  error_type_ptr = error_type();
  templ_rout_type = templ_rout->type;
  templ_rtsp = templ_rout_type->variant.routine.extra_info;
  rout_type = alloc_type((a_type_kind)tk_routine);
  rtsp = rout_type->variant.routine.extra_info;
  rtsp->prototyped = TRUE;
  rout_type->variant.routine.return_type = error_type_ptr;
  /* Create a list of parameters of error type.  The number of parameters
     should match the parameter list of the original template. */
  for (templ_ptp = templ_rtsp->param_type_list; templ_ptp != NULL;
       templ_ptp = templ_ptp->next) {
    ptp = alloc_param_type(error_type_ptr);
    if (last_ptp == NULL) {
      rtsp->param_type_list = ptp;
    } else {
      last_ptp->next = ptp;
    }  /* if */
    last_ptp = ptp;
  }  /* for */
  if (templ_rtsp->this_class != NULL) {
    /* If this is a member function, set the this class type. */
    rtsp->this_class = parent_class;
  }  /* if */
  return rout_type;
}  /* create_error_routine_type */


static void check_for_invalid_instantiation(
				a_type_ptr		*type,
				a_routine_ptr		templ_rout,
				a_boolean		suppress_diagnostic,
				a_type_ptr		parent_class,
				a_template_instance_ptr	tip)
/*
This routine is called after a declaration of a function has been rescanned
to create a partial instantiation.  It determines whether all of the tokens
of the declaration have been scanned, and whether the type created is
a function type.  Errors are issued if either of these conditions is
not satisfied unless suppress_diagnostic is TRUE.  If an error is
detected a special error routine type is created.  This is done even if
the diagnostic is suppressed.
*/
{
  /* The rescan of the declaration should have produced a routine
     type.  If not all of the tokens were used, or if the type created
     is not a function type, issue a diagnostic.  We permit a tok_colon
     and tok_try to be present because the ctor-initializers or the start
     of a function try block may be part of the declaration cache. */
  if ((curr_token != tok_end_of_source &&
       curr_token != tok_colon && curr_token != tok_try) ||
      *type == NULL ||
      !is_function_type(*type)) {
    if (!suppress_diagnostic) {
      pos_error(ec_invalid_declaration, &pos_curr_token);
    }  /* if */
    /* The scanning of the declaration must produce a suitable function
       type.  Create a function type with a suitable number of parameters
       whose types are error types. */
    *type = create_error_routine_type(templ_rout, parent_class);
    tip->suppress_instantiation = TRUE;
  }  /* if */
}  /* check_for_invalid_instantiation */


static a_decl_flag_set merge_declarator_flags(a_decl_flag_set dso_flags,
                                              a_decl_flag_set do_flags)
/*
Some flags normally set by decl_specifiers cannot be determined until
declarator has run.  For example, a parenthesized constructor declarator is
not recognized as such until the declarator has been scanned.

dso_flags are the flags produced by decl_specifier and do_flags those
produced by declarator. The relevant bits of the latter are merged into the
former and the resulting value is returned.
*/
{
  a_decl_flag_set result = dso_flags;

  if (do_flags & DO_IS_CONSTRUCTOR) {
    result |= DSO_CONSTRUCTOR;
  }  /* if */
  if (do_flags & DO_IS_DESTRUCTOR) {
    result |= DSO_DESTRUCTOR;
  }  /* if */
  return result;
}  /* merge_declarator_flags */


static void check_for_declaration_errors(a_decl_flag_set   dso_flags,
                                         a_type_ptr        type,
                                         a_symbol_locator  *locator,
                                         a_source_position *pos)
/*
This routine is used to detect certain kinds of errors related to
the processing of a declaration in a function template declaration,
explicit instantiation or specialization.  

dso_flags is a set of flags produced by decl_specifiers; type is produced by
declarator. pos is the position to be used if a diagnostic is issued.
*/
{
  /* Make sure the lookup was not ambiguous. */
  check_for_ambiguity(locator);
  if (!is_error_locator(*locator)) {
    a_boolean	is_function;
    is_function = is_function_type(type);
    if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
      if (is_function_type(type) &&
          (((dso_flags & (DSO_CONSTRUCTOR | DSO_DESTRUCTOR)) != 0) ||
           locator->is_conversion_name)) {
        /* No type specifier is required. */
      } else {
        /* Error on omitted type specifier. */
        a_boolean  any_decl_specifiers =
                                 (dso_flags & DSO_NO_DECL_SPECIFIERS) == 0;
        report_missing_type_specifier(pos, is_function,
                                      /*is_function_def=*/FALSE,
                                      /*is_main_function=*/FALSE,
                                      any_decl_specifiers);
      }  /* if */
    }  /* if */
    if (dso_flags & DSO_DEFINES_SOMETHING) {
      /* The type specifiers included a type definition, which is not allowed
         in this context. */
      pos_error(ec_type_definition_not_allowed, pos);
    }  /* if */
  }  /* if */
}  /* check_for_declaration_errors */


static void verify_routine_type_matches_template(
					a_symbol_ptr		templ_sym,
					a_routine_ptr		rout,
					a_routine_ptr		templ_rout,
					a_type_ptr		parent_class,
					a_template_arg_ptr	templ_arg_list,
					a_template_instance_ptr	tip)
/*
Verify that the routine type associated with rout is one that can be
generated from the template represented by templ_sym.  The purpose of
this test is to determine whether the partial instantiation of a function
has been affected by declarations that appeared after the template was
declared and before the partial instantiation of the function was done.
*/
{
  a_type_ptr	substituted_type;
  a_type_ptr	type = rout->type;

  substituted_type = substitute_template_arguments(
                                  templ_sym, templ_arg_list,
                                  (a_template_arg_ptr*)NULL,
                                  (a_template_param_ptr)NULL);
  if (substituted_type == NULL ||
      !types_are_compatible(substituted_type, type)) {
    if (!is_or_contains_error_type(type) &&
        !is_or_contains_error_type(templ_rout->type)) {
      /* If the type contains an error type it is likely that the current
         routine type is already an error routine type produced earlier.
         Don't issue a diagnostic in this case, but still create an
         error routine type in case some of the parameter types were not
         already error types. */
      pos_ty2_error(ec_bad_type_from_instantiation, &pos_curr_token,
                    type, templ_rout->type);
    }  /* if */
    type = create_error_routine_type(templ_rout, parent_class);
    rout->type = type;
    tip->suppress_instantiation = TRUE;
  }  /* if */
}  /* verify_routine_type_matches_template */


static void scan_template_declaration(
				a_boolean         	   is_initial_decl,
                                a_boolean         	   is_member_decl,
                                a_type_ptr		   parent_class,
				a_boolean         	   decl_scope_err,
				a_boolean		   is_specialization,
                                a_decl_flag_set   	   *dso_flags,
                                a_decl_flag_set   	   *do_flags,
                                a_symbol_locator  	   *locator,
                                a_type_ptr        	   *type,
                                a_func_info_block	   *func_info,
                                a_storage_class		   *storage_class,
                                a_decl_modifiers_block_ptr decl_modifiers,
                                a_routine_ptr		   templ_rout,
				a_template_instance_ptr	   tip,
                                a_decl_pos_block_ptr       decl_pos_block)
/*
Calls decl_specifiers and declarator to scan a template declaration of
a function or static data member.  is_initial_decl is TRUE if this
is being called to scan the original declaration and is FALSE when
rescanning the tokens to generate a type for a specific instance
of a function template.  templ_rout points to the routine associated
with the original declaration of a template and is only present
(non-NULL) when is_initial_decl is FALSE.  tip points to the template
instance and is also only present when is_initial_decl is FALSE.
decl_pos_block points to entry used to record detailed source position
information.
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
                        &qualifiers, decl_modifiers, decl_pos_block);
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
         add a this class to the type. */
      di_flags |= DI_NONSTATIC_MEMBER;
    }  /* if */
    if (is_member_decl && (*dso_flags & DSO_CONSTRUCTOR) != 0) {
      di_flags |= DI_IS_CONSTRUCTOR;
    }  /* if */
    if (!(*dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) &&
        qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
    declarator(di_flags, do_flags, (a_type_qualifier_set *)NULL, *type,
               !friend_specified ? parent_class : (a_type_ptr)NULL,
               locator, type,
               &declarator_ssep, func_info, decl_pos_block);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (declarator_ssep != NULL) {
      remove_from_src_seq_list(declarator_ssep);
      declarator_ssep = NULL;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (decl_scope_err) {
      /* Just to be sure a template symbol doesn't get added to a scope that
         is not equipped to handle it, create an error locator based on the
         previously reported error. */
      set_to_named_error_locator(*locator);
    }  /* if */
    check_for_declaration_errors(
                             merge_declarator_flags(*dso_flags, *do_flags),
                             *type, locator, &decl_start_pos);
    func_info->is_inline = ((*dso_flags & DSO_INLINE) != 0);
    /* Note whether this is a function type that comes from a typedef.  The
       setting is checked later if this turns out to be a function template
       definition. */
    if (is_function_type(*type)) {
      if ((*type)->kind == (a_type_kind)tk_typeref) {
        func_info->function_type_from_typedef = TRUE;
      }  /* if */
      if (parent_class == NULL && locator->is_class_member) {
        /* This is a member template declaration outside the class definition,
           so a storage class may not be specified (as in the nontemplate
           case). */
        if (*storage_class != (a_storage_class)sc_unspecified) {
          pos_error(ec_storage_class_not_allowed, &decl_start_pos);
          *storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
      /* Issue diagnostic on an incomplete-type in an exception
         specification. */
      if (func_info->exception_spec_errors != NULL) {
        /* If this is an instantiation of the function template and the
           template has been defined, not just declared, treat this as a
           definition as far as diagnostic severity is concerned.  Otherwise,
           even if this is a template definition, let the diagnostics come
           out with relaxed severity. */
        if (!is_initial_decl && tip->template_sym->defined) {
          func_info->is_definition = TRUE;
        }  /* if */
        report_exception_spec_errors(func_info);
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
    /* The rescan of the declaration should have produced a routine
       type.  If not all of the tokens were used, or if the type created
       is not a function type, issue a diagnostic. */
    check_for_invalid_instantiation(type, templ_rout,
                                    (a_boolean)is_error_locator(*locator),
                                    (a_type_ptr)NULL, tip);
    flush_past_token_cache_terminator();
  }  /* if */
}  /* scan_template_declaration */


static a_type_ptr scan_member_declaration(
				a_type_ptr		parent_class,
                                a_routine_ptr 		templ_rout,
				a_template_instance_ptr	tip)
/*
Calls rescan_member_template_declaration to rescan the tokens of a
member function template to produce the type for the instance and to
detect any errors that should be diagnosed.  templ_rout points to the
routine associated with the original declaration of a template.  tip
is the template instance record associated with the instance.
*/
{
  a_type_ptr		instance_type;

  add_stop_token(tok_end_of_source);
  instance_type = rescan_member_template_declaration(parent_class, tip);
  remove_stop_token(tok_end_of_source);
  /* The rescan of the declaration should have produced a routine
     type.  If not all of the tokens were used, or if the type created
     is not a function type, issue a diagnostic. */
  check_for_invalid_instantiation(&instance_type, templ_rout,
                                  /*suppress_diagnostic=*/FALSE,
                                  parent_class, tip);
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
  check_assertion(templ_sym->kind == (a_symbol_kind)sk_function_template);
  tssp = templ_sym->variant.template_info;
  /* Create the associated function instantiation entry and link it
     onto the front of the instantiation list for the template. */
  tip = alloc_template_instance();
  tip->template_sym = templ_sym;
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position           saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if DECL_MODIFIERS_IN_USE
    a_source_position	 locator_position;
#endif /* DECL_MODIFIERS_IN_USE */
    a_template_cache_ptr tcp;

    /* Push the template instantiation scope.  Note that the instance symbol
       passed to push_template_instantiation_scope is NULL.  This is done
       because the type associated with the symbol is not yet complete
       (it has no routine type).  Using a partially constructed symbol could
       cause problems if errors occur while rescanning the declaration. */
    tcp = &tssp->variant.function.decl_cache;
    /* Increment the count of pending instantiations of this template. */
    ++(tssp->variant.function.pending_partial_instantiations);
    push_template_instantiation_scope(tcp->decl_info,
				      (a_type_ptr)NULL,
				      (a_routine_ptr)NULL,
				      (a_symbol_ptr)NULL, templ_sym,
				      templ_arg_list,
                                      /*push_stop_tokens=*/TRUE,
				      PS_NO_OPTIONS);
    /* Reactivate any pragmas that should be bound to the generated
       instance. */
    reactivate_curr_construct_pragmas(tssp->pragmas_bound_to_template);
    /* Rescan the tokens of the function declaration. */
    saved_pos_curr_token = pos_curr_token;
    saved_error_position = error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    saved_curr_construct_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
#if DECL_MODIFIERS_IN_USE
    locator_position = pos_curr_token;
#endif /* DECL_MODIFIERS_IN_USE */
    if (tssp->variant.function.pending_partial_instantiations >=
                                                  max_pending_instantiations) {
      sym_error(ec_runaway_recursive_instantiation, templ_sym);
      rout_type = create_error_routine_type(templ_rout, parent_class);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      tip->declared_type = rout_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Flush to the end of the declaration cache. */
      while (curr_token != tok_end_of_source) (void)get_token();
      /* Skip past the tok_end_of_source. */
      (void)get_token();
    } else if (parent_class != NULL) {
      rout_type = scan_member_declaration(parent_class, templ_rout, tip);
#if 0
      /* We should get the locator position returned. */
#endif /* 0 */
    } else {
      a_decl_flag_set	      do_flags;
      a_func_info_block	      func_info;
      a_storage_class         storage_class;
      a_symbol_locator	      locator;
      a_decl_modifiers_block  decl_modifiers;
      a_decl_pos_block        decl_pos_block;

      clear_func_info(&func_info);
      clear_decl_pos_block(&decl_pos_block);
      scan_template_declaration(/*is_initial_decl=*/FALSE,
                                is_member_decl, parent_class,
  			        /*decl_scope_err=*/FALSE,
				/*is_specialization=*/FALSE,
                                &dso_flags, &do_flags, &locator,
                                &rout_type, &func_info, &storage_class,
                                &decl_modifiers, templ_rout, tip,
                                &decl_pos_block);
      /* Save the prototype scope symbols in the instance pointer. */
      tip->prototype_scope_symbols = func_info.prototype_scope_symbols;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Set the declared type immediately, before the func_info block is
         discarded. */
      tip->declared_type = form_declared_type(rout_type, &func_info);
      /* Also save the parameter-id list to later reconstruct the declared
         types of parameters for the associated parameter variables. */
      tip->param_id_list = func_info.param_id_list;
      /* Clear the func_info field to prevent deallocation: */
      func_info.param_id_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      done_with_func_info(func_info);
#if DECL_MODIFIERS_IN_USE
      locator_position = locator.source_position;
#endif /* DECL_MODIFIERS_IN_USE */
    }  /* if */
    error_position = saved_error_position;
    pos_curr_token = saved_pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
    rp->assoc_template = tssp->il_template_entry;
#if DECL_MODIFIERS_IN_USE
    {
    a_decl_modifiers_block  decl_modifiers;

    clear_decl_modifiers_block(&decl_modifiers);
    decl_modifiers.flags = templ_rout->decl_modifiers;
    update_routine_decl_modifiers(rp, &decl_modifiers, &locator_position,
                                  /*is_redecl=*/FALSE, /*is_definition=*/TRUE,
                                  (a_boolean)rp->is_inline);
    }
#endif /* DECL_MODIFIERS_IN_USE */
    /* Add it to the routines list of the appropriate scope; NO_SCOPE_DEPTH
       is passed in to cause the scope to be computed. */
    add_to_routines_list(rp, NO_SCOPE_DEPTH);
    update_befriending_classes_for_function(tssp, rp);
    perform_deferred_access_checks_for_function(rp);
    end_deferral_of_access_checks();
  }
  /* Check that the routine type created by this instance matches the
     template.  This is used to make sure the binding of names used in
     the declarations hasn't changed. */
  verify_routine_type_matches_template(templ_sym, rp, templ_rout,
                                       parent_class, templ_arg_list, tip);
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  tip->instance_sym = sym;
  sym->variant.routine.instance_ptr = tip;
  tip->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = tip;
  /* Process any pragmas that are to be bound to this instance. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  {
    a_symbol_locator	locator;
    /* Set the default argument information in the newly created routine
       based on the information for the template.  Note that the default
       argument values are not actually scanned at this point.  They will
       be scanned later, only if their value(s) are needed. */
    if (tssp->variant.function.def_arg_expr_list != NULL) {
      check_for_function_template_default_args(templ_rout, rp, tssp);
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (parent_class != NULL &&
      parent_class->variant.class_struct_union.is_prototype_instantiation) {
    /* In Microsoft mode a member template may be specialized within the
       definition of the parent class.  Don't generate the source sequence
       entry when the parent class is a prototype instantiation. */
  } else {
    /* Add a secondary source sequence entry to represent the partial
       instantiation -- it will take the form of an explicit specialization. */
    a_type_ptr  declared_type = tip->declared_type;

    check_assertion(declared_type != NULL);
    if (declared_type == rp->type &&
        declared_type->kind != (a_type_kind)tk_typeref) {
      /* Make a copy of the routine type; default_args, if any, will be
         ignored. */
      declared_type = copy_routine_type_with_param_types(
                                                rp->type,
                                                /*copy_default_args=*/FALSE);
    } else {
      /* Use the declared_type in the routine entry only if it has no
         default args; otherwise, make a copy. */
      check_assertion(!is_qualified_type(declared_type));
      declared_type = routine_type_without_default_args(declared_type);
    }  /* if */
#if DEBUG
    if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
      fputs("creating new template function:\n", f_debug);
    }  /* if */
#endif /* DEBUG */
    add_source_sequence_entry_for_partial_instantiation(
                                               (char *)rp,
                                               (an_il_entry_kind)iek_routine,
                                               declared_type);
    /* Reset the insert point so that instantiations triggered will follow
       the entry representing the partial instantiation, not precede it. */
    reset_ss_list_instantiation_insert_point();
  }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Pop the template instantiation scope. */
  pop_template_instantiation_scope();
  /* Decrement the count of pending instantiations of this template. */
  --(tssp->variant.function.pending_partial_instantiations);
  switch_back_to_original_region(region_to_switch_back_to);
  /* Call a routine that manages the correspondence of entities between
     translation units to notify it of the new instance. */
  record_instantiation(sym, tssp);
  /* Function instantiation entries are not marked for actual instantiation
     (that is, for generation of the function body) until there is an
     invocation of the function.  In tim_all mode the instantiations
     will be generated even if the instantiation required flag is not
     set. */
  if (!tip->instantiation_required) {
    /* If the flag is set then the entry is already on the list and the flag
       should not be reset. */
    set_instance_required(sym, /*value=*/FALSE, /*defer_inline=*/FALSE);
  }  /* if */
  db_exit();
  return sym;
}  /* make_template_function */


static void update_template_arg_usage_info(
			a_symbol_ptr		rout_sym,
			a_template_arg_ptr	templ_arg_list,
			a_boolean		explicit_arg_list_present)
/*
Go through the template argument list specified by templ_arg_list and update
the corresponding template argument list entry in the template argument
list associated with rout_sym to indicate whether any of the arguments
in templ_arg_list were explicitly specified.  Update the flag in the
routine entry associated with rout_sym if any explicitly specified template
argument was found or if the explicit_arg_list_present flag is TRUE.
explicit_arg_list_present can be TRUE even when none of the template
arguments is explicitly specified when an empty template argument list
is present.
*/
{
  a_template_arg_ptr	rout_tap;
  a_template_arg_ptr	new_tap;
  a_routine_ptr		rout;
  rout = rout_sym->variant.routine.ptr;
  rout_tap = rout->template_arg_list;
  for (new_tap = templ_arg_list; new_tap != NULL;
       rout_tap = rout_tap->next, new_tap = new_tap->next) {
    if (new_tap->explicitly_specified) {
      rout_tap->explicitly_specified = TRUE;
    }  /* if */
  }  /* for */
  if (explicit_arg_list_present ||
      rout->template_arg_list->explicitly_specified) {
    /* If any of the templates arguments were explicitly specified, set
       the flag in the routine entry.  We only need to check the first flag
       because the explicit arguments are always specified starting with
       the first argument. */
   rout->expl_template_arg_list_used = TRUE;
  }  /* if */
}  /* update_template_arg_usage_info */


a_boolean is_match_for_function_template(
				a_symbol_ptr		templ_sym,
				a_type_ptr		curr_type,
				a_template_arg_ptr	*templ_arg_list,
				a_symbol_ptr		*instance_sym,
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	explicit_arg_list,
				a_boolean		is_decl_context)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If such a template
function exists, return its symbol.  Otherwise, try to generate a template
arg list to serve as the basis for creating one.  If either a symbol can
be found or a template arg list can be created, return TRUE; otherwise,
return FALSE.  explicit_arg_list is non-NULL if an explicitly specified
template argument list was provided.

is_decl_context is TRUE if this routine is called to match a declaration with
a template instance.  In such cases it is not known whether or not the
function has a this class type, so the this class should not be used in the
matching process.
*/
{
  a_boolean                         match = FALSE;
  a_type_ptr                        templ_rout_type;
  a_template_symbol_supplement_ptr  tssp;
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
  *instance_sym = NULL;
  /* sym is the symbol for a template function to be returned.  Returning NULL
     means no template function could be found or created. */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  *templ_arg_list = NULL;
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
  if (explicit_arg_list != NULL) {
    /* Substitute the explicitly specified template arguments and
       produce an updated template routine type. */
    a_template_arg_ptr	new_arg_list;
    templ_rout_type = substitute_template_arguments(
                                  templ_sym, explicit_arg_list, &new_arg_list,
                                  (a_template_param_ptr)NULL);
    *templ_arg_list = new_arg_list;
    /* A NULL type will be returned if the copy could not be done because
       the substitution of the template arguments would result in an invalid
       type. */
    if (templ_rout_type == NULL) goto done;
  }  /* if */
  /* Falling through to here means the type signature passed in does not
     match any existing template function based on the function template in
     question, but that it is not disqualified on other grounds.  Try to match
     the type signature to the template's type signature.  If successful, a
     template arg list is returned; otherwise, NULL is returned. */
  if (matches_template_type(curr_type, templ_rout_type, 
                            templ_arg_list, templ_param_list,
                            (an_mtt_flag_set)
                            (is_decl_context ? MTT_UNKNOWN_THIS_CLASS_TYPE
                                            : MTT_NO_FLAGS))) {
    match = TRUE;
  }  /* if */
  /* Make sure that the types of nontype template parameters that depend
     on other template parameters agree with the types of the deduced
     values. */
  if (match) {
    a_type_ptr	new_type;
    /* Make sure the final type, after substitution of nondeduced contexts,
       is correct. */
    new_type = wrapup_function_template_argument_deduction(
                                *templ_arg_list, templ_sym, templ_param_list);
    match = FALSE;
    if (new_type != NULL) {
      if (is_decl_context) {
        /* In declaration contexts we do not yet know whether the type
           has a this class type.  Consequently, a NULL this class
           type should be considered a match for a non-NULL one in the
           routine we are matching with. */
        match = unknown_this_class_identical_types(curr_type, new_type);
      } else {
        /* In nondeclarative contexts, the this class parameter types must
           match exactly. */
        match = identical_types(curr_type, new_type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (match) {
    /* Look for a previously created instance with a matching set of template
       arguments. */
    a_template_instance_ptr           tip;
    a_symbol_ptr                      sym;
    for (tip = tssp->variant.function.instantiations;
         tip != NULL;
         tip = tip->next) {
      a_routine_ptr	rout;
      /* We used to skip entries that represent specific declarations.
         This is no longer done because these entries must be examined this
         routine is called during instantiation pragma processing. */
      sym = tip->instance_sym;
      /* Ignore symbols for which instance_sym has not yet been set.  This
         happens when verify_routine_type_matches_template is called. */
      if (sym == NULL) continue;
      rout = sym->variant.routine.ptr;
      if (equiv_template_arg_lists(*templ_arg_list, rout->template_arg_list,
                                   ETA_NO_OPTIONS)) {
        /* The template argument lists match.  Return the symbol for this
           template. */
        *instance_sym = sym;
        break;
      }  /* if */
    }  /* for */
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


a_symbol_ptr matching_template_function(
				a_symbol_ptr        templ_sym,
                                a_type_ptr          curr_type,
				a_template_arg_ptr  explicit_arg_list,
				a_boolean	    explicit_arg_list_present,
				a_boolean	    is_decl_context,
				a_boolean	    *is_new_template_instance)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If no such template
function exists, try to create one.  If the search/creation is successful
return a pointer to the symbol; otherwise, return NULL.

is_decl_context is TRUE if this routine is called to match a declaration with
a template instance.  In such cases it is not known whether or not the
function has a this class type, so the this class type should not be
used in the matching process.  explicit_arg_list is non-NULL if an explicitly
specified template argument list was provided.  *is_new_template_instance is
returned TRUE if a new template instance is created with this call.
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
  templ_param_list = tssp->variant.function.decl_cache.decl_info->parameters;
  *is_new_template_instance = FALSE;
  if (is_match_for_function_template(templ_sym, curr_type,
                                     &templ_arg_list, &sym,
                                     templ_param_list,
				     explicit_arg_list,
                                     is_decl_context)) {
    if (sym != NULL) {
      /* A match has been found -- just return a pointer to it. */
    } else {
      /* Use the template arg list to create a new symbol. */
      sym = make_template_function(templ_sym, templ_arg_list);
      *is_new_template_instance = TRUE;
    }  /* if */
  }  /* if */
  /* Update the flags that indicate whether any explicitly specified template
     arguments were used. */
  update_template_arg_usage_info(sym, templ_arg_list,
                                 explicit_arg_list_present);
  db_exit();
  return sym;
}  /* matching_template_function */


a_boolean has_matching_template_function(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
					 a_template_arg_ptr explicit_arg_list,
		  		         a_boolean	    is_decl_context)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  Return TRUE if a
match is found.  This routine is like matching_template_function except
that an actual instance is not generated if one does not already exist.

is_decl_context is TRUE if this routine is called to match a declaration with
a template instance.  In such cases it is not known whether or not the
function has a this class type, so the this class type should not be used
in the matching process.  explicit_arg_list is non-NULL if an explicitly
specified template argument list was provided.
*/
{
  a_symbol_ptr          		sym;
  a_template_arg_ptr    		templ_arg_list = NULL;
  a_template_symbol_supplement_ptr	tssp;
  a_template_param_ptr			templ_param_list;
  a_boolean				result;

#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("has_matching_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  curr_type = skip_typerefs(curr_type);
  tssp = template_supplement_for_symbol(templ_sym);
  templ_param_list = tssp->variant.function.decl_cache.decl_info->parameters;
  result = is_match_for_function_template(templ_sym, curr_type,
                                          &templ_arg_list, &sym,
                                          templ_param_list,
					  explicit_arg_list,
					  is_decl_context);
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
                                       (a_template_arg_ptr)NULL,
				       /*is_decl_context=*/TRUE)) {
      /* A match has been found. */
#if CHECKING
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
      /* Mark this function as a "guiding declaration". */
      tip->is_guiding_decl = TRUE;
      tssp = templ_sym->variant.template_info;
      tip->next = tssp->variant.function.instantiations;
      tssp->variant.function.instantiations = tip;
      /* Make the function instantiation entry and its associated symbol
         point at each other. */
      tip->instance_sym = rout_sym;
      rout_sym->variant.routine.instance_ptr = tip;
      rout_sym->variant.routine.ptr->is_template_function = TRUE;
      rout_sym->variant.routine.ptr->template_arg_list = templ_arg_list;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      tip->declared_type = form_declared_type(tp,
                                              func_info_for_template(tssp));
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  }  /* if */
  if (tssp != NULL) {
    if (rout_sym->defined
#if MICROSOFT_EXTENSIONS_ALLOWED
        || (microsoft_mode &&
            !rout_sym->variant.routine.ptr->declared_only_as_friend)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                    ) {
      /* When the function has already been defined, no instantiation is
         required.  Otherwise, in Microsoft mode this is still regarded as
         a specialization, provided at least one of the declarations was
         not a friend declaration.
           void f(int);                 // A specialization in MS mode
           class A {
             friend void f(short);      // Not a specialization is MS mode
           };
           template <class T> void f(T) { ... }                     */
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
        }  /* if */
      } if (rp->storage_class == (a_storage_class)sc_static) {
        sym_warning(ec_template_and_instance_linkage_conflict, rout_sym);
        rp->storage_class = (a_storage_class)sc_unspecified;
        rp->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_cplusplus_external;
      }  /* if */
      if (templ_rp->is_inline) {
        if (rp->called) {
          sym_remark(ec_called_function_redeclared_inline, rout_sym);
        }  /* if */
        rp->is_inline = TRUE;
      } else {
        if (rp->is_inline) {
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
  if (sym != NULL) {
    a_boolean	is_list = FALSE;
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    /* If an overloaded function was found, go through the symbols on its
       list and find the function template symbol that corresponds to
       rout_sym. The token sequence number associated for the current
       token is saved during the prototype instantiation.  This is used
       to match this declaration with the symbol generated by the prototype
       instantiation.  If the symbol is not an overloaded function, make
       sure that it matches the rout_sym. */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
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
  { a_template_symbol_supplement_ptr  tssp;
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
    /* A placeholder a_template entry was created in the prototype
       instantiation.  It serves as the associated "template". */
    rout_sym->variant.routine.ptr->assoc_template =
                                     sym->variant.routine.ptr->assoc_template;
  }
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
        if (sym->variant.static_data_member.instance_ptr == NULL) {
          /* Something went very wrong with this symbol, discard it. */
          check_assertion(sym->is_error);
          sym = NULL;
        } else {
          a_template_symbol_supplement_ptr	tssp;
          tssp = sym->variant.static_data_member.instance_ptr->template_info;
          check_assertion(tssp != NULL);
          if (tssp->token_sequence_number == curr_token_sequence_number) {
            break;
          } else {
            sym = NULL;
          }  /* if */
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
    a_template_instance_ptr		tip = alloc_template_instance();
    a_template_symbol_supplement_ptr	tssp;
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
    /* A placeholder a_template entry was created in the prototype
       instantiation.  It serves as the associated "template". */
    vp->assoc_template =
                     sym->variant.static_data_member.variable->assoc_template;
  }  /* if */
  db_exit();
}  /* find_static_data_member_template */


a_symbol_ptr find_template_function(
			a_symbol_ptr		templ_sym,
                        a_template_arg_ptr	*new_list,
			a_boolean		explicit_arg_list_present,
                        a_source_position	*source_pos)
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
  templ_sym = fundamental_symbol_of(templ_sym);
  /* Make a pass over the entries representing instantiations of the function
     template. */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    check_assertion(templ_sym->kind == (a_symbol_kind)sk_function_template);
    tssp = templ_sym->variant.template_info;
  }  /* if */
  /* Check for invalid type arguments.  Local types may not be used as
     arguments nor may unnamed types.  Issue an error if any are found.
     Unnamed types are permitted as template arguments in Microsoft mode. */
  while (tap != NULL) {
    if (is_type_templ_arg(tap)) {
      a_type_ptr	type = tap->variant.type;
      a_boolean		is_unnamed;
      a_boolean		is_local;
      if (is_or_contains_unnamed_or_local_type(type, &is_unnamed, &is_local)) {
        if (is_local) {
          pos_error(ec_local_type_in_template_arg, source_pos);
          tap->variant.type = error_type();
        } else if (is_unnamed && !microsoft_mode) {
          pos_error(ec_unnamed_type_in_template_arg, source_pos);
        }  /* if */
      }  /* if */
      /* Local typedef names (legal if they refer to nonlocal types) should
         not be part of the type signature of the template itself,
         which is nonlocal.  Strip them off, if there are any. */
      tap->variant.type = strip_local_and_nonreal_typedefs(tap->variant.type);
    } else if (is_nontype_templ_arg(tap)) {
      if (constant_references_non_external_entity(tap->variant.constant)) {
        pos_error(ec_nonexternal_entity_in_template_arg, source_pos);
        set_error_constant(tap->variant.constant);
      }  /* if */
    }  /* if */
    tap = tap->next;
  }  /* while */
  tip = tssp->variant.function.instantiations;
  prev_tip = NULL;
  for (; tip != NULL; tip = tip->next) {
    a_template_arg_ptr	arg_list;
    arg_list = tip->instance_sym->variant.routine.ptr->template_arg_list;
    if (equiv_template_arg_lists(arg_list, *new_list, ETA_NO_OPTIONS)) {
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
  } else {
    sym = tip->instance_sym;
  }  /* if */
  /* Update the flags that indicate whether any explicitly specified template
     arguments were used. */
  update_template_arg_usage_info(sym, *new_list, explicit_arg_list_present);
  if (tip != NULL) {
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


a_boolean equiv_template_param_lists(
				a_template_param_ptr	old_list,
				a_template_param_ptr	new_list,
				a_boolean		issue_errors,
				a_source_position	*error_pos)
/*
Compare the template parameter list pointed to by old_list with the
one pointed to by new_list.  To be equivalent, the parameter lists must
have the same number of parameters, be of the same kind (type vs. nontype),
and nontype parameters must be of the same type.  Return TRUE if the
lists are equivalent.  If issue_errors is TRUE, errors are issued
describing any incompatibilities.
*/
{
  a_template_param_ptr		new_tpp;
  a_template_param_ptr		old_tpp;
  a_boolean			any_errors = FALSE;
  a_template_param_ptr		prev_new_tpp = NULL;
  a_template_nesting_depth	old_depth;
  a_template_nesting_depth	new_depth;

  old_depth = nesting_depth_of_template_param(old_list);
  new_depth = nesting_depth_of_template_param(new_list);
  if (!equiv_nesting_depths(old_depth, new_depth)) {
    /* The nesting depths do not match -- don't check any further. */
    any_errors = TRUE;
    goto done;
  }  /* if */
  old_tpp = old_list;
  new_tpp = new_list;
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
      a_type_ptr        old_type = old_tpp->variant.type;
      a_type_ptr        new_type = new_tpp->variant.type;
      err = !identical_types(old_type, new_type);
    } else if (old_sym->kind == (a_symbol_kind)sk_constant) {
      /* Both are constants.  Make sure the values are the same. */
      err = !eq_constants(old_tpp->variant.constant.ptr,
                          new_tpp->variant.constant.ptr);
      if (err && microsoft_bugs) {
        /* In Microsoft bugs mode, a member of a class template can be
           declared using a template parameter with a type that is different
           than that of the associated class template. */
        err = !equiv_nontype_template_param_names(
                 old_tpp->variant.constant.ptr, new_tpp->variant.constant.ptr);
        if (!err) {
          /* An incompatible redeclaration of the parameter that is accepted
             in Microsoft bugs mode.  Issue a warning. */
          pos_sy_warning(ec_not_compatible_with_previous_decl,
                         &new_sym->decl_position, old_sym);
        }  /* if */
      }  /* if */
    } else {
      /* Template template parameters.  Compare the two templates. */
      check_assertion(old_sym->kind == (a_symbol_kind)sk_class_template);
      err = !equiv_templates_given_supplement(old_tpp->variant.templ,
                                              new_tpp->variant.templ);
    }  /* if */
    if (err) {
      if (issue_errors) {
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &new_sym->decl_position, old_sym);
      }  /* if */
      any_errors = TRUE;
    }  /* if */
    old_tpp = old_tpp->next;
    prev_new_tpp = new_tpp;
    new_tpp = new_tpp->next;
  }  /* while */
  if (old_tpp != NULL || new_tpp != NULL) {
    /* The lists differ in the number of parameters. */
    any_errors = TRUE;
    if (issue_errors) {
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
    }  /* if */
  }  /* if */
done:
  return !any_errors;
}  /* equiv_template_param_lists */


static a_boolean reconcile_template_param_lists
					(a_template_param_ptr param_list,
                                         a_symbol_ptr         class_sym,
					 a_source_position    *error_pos,
					 a_boolean	      default_allowed)
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

default_allowed is TRUE if a default argument is permitted in the new argument
list (the one specified by param_list).
*/
{
  a_template_param_ptr	new_tpp;
  a_template_param_ptr	old_tpp;
  a_boolean		any_errors;

  new_tpp = param_list;
  old_tpp = class_sym->variant.template_info->cache.decl_info->parameters;
  /* Compare the two template parameter lists. */
  any_errors = !equiv_template_param_lists(old_tpp, new_tpp,
                                           /*issue_errors=*/TRUE, error_pos);
  if (!any_errors) {
    /* Update type parameters so that they point to the same template
       parameter type supplement. */
    new_tpp = param_list;
    old_tpp = class_sym->variant.template_info->cache.decl_info->parameters;
    while (new_tpp != NULL && old_tpp != NULL) {
      if (old_tpp->param_symbol->kind == (a_symbol_kind)sk_type) {
        a_template_param_type_supplement_ptr old_tptsp;
        a_type_ptr        old_type = old_tpp->variant.type;
        a_type_ptr        new_type = new_tpp->variant.type;
        /* Update both type entries to point to the same description entry. */
        old_tptsp = old_type->variant.template_param.extra_info;
        old_type->variant.template_param.extra_info = old_tptsp;
        new_type->variant.template_param.extra_info = old_tptsp;
      }  /* if */
      old_tpp = old_tpp->next;
      new_tpp = new_tpp->next;
    }  /* while */
    /* Merge the default argument information from the two parameter lists.
       This is only done if there were no errors in the previous tests so
       we know that the parameter lists match. */
    new_tpp = param_list;
    old_tpp = class_sym->variant.template_info->cache.decl_info->parameters;
    while (new_tpp != NULL && old_tpp != NULL) {
      a_boolean def_arg_involves_template_param;
      a_boolean old_has_default;
      a_boolean new_has_default;
      old_has_default = old_tpp->has_default_arg;
      new_has_default = new_tpp->has_default_arg;
      if (old_has_default && new_has_default && !microsoft_bugs) {
        /* This parameter already has a default argument.  The
           Microsoft compiler permits this, and uses the new value. */
        pos_error(ec_default_arg_already_defined,
                  &new_tpp->param_symbol->decl_position);
      } else if (new_has_default && !default_allowed) {
        /* A default argument was specified on a member of a class template.
           This is not permitted. */
        pos_diagnostic(microsoft_mode ? es_warning : es_error,
                       ec_default_arg_on_member_decl,
                       &new_tpp->param_symbol->decl_position);
      } else if (old_has_default || new_has_default) {
        /* One or the other has a default argument, or we are in Microsoft
           mode and both have default arguments. */
        a_template_param_ptr	from_tpp;
        a_template_param_ptr	to_tpp;
        /* Copy the default information into the other parameter.  We
           end up with two argument lists with complete parameter
           information. This is done because we don't know which parameter
           list is going to end up being the one actually used. */
        if (new_has_default) {
          /* If the new declaration has a default, use it.  It must be done
             in this order because in Microsoft mode a declaration may have
             both an old and new default value. */
          from_tpp = new_tpp;
          to_tpp = old_tpp;
        } else {
          from_tpp = old_tpp;
          to_tpp = new_tpp;
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
          to_tpp->default_arg_cache = from_tpp->default_arg_cache;
        }  /* if */
        { a_symbol_kind	new_sym_kind = new_tpp->param_symbol->kind;
          if (new_sym_kind == (a_symbol_kind)sk_constant) {
            to_tpp->default_arg.constant = from_tpp->default_arg.constant;
          } else if (new_sym_kind == (a_symbol_kind)sk_type) {
            to_tpp->default_arg.type = from_tpp->default_arg.type;
          } else {
            check_assertion(new_sym_kind == (a_symbol_kind)sk_class_template);
            to_tpp->default_arg.templ = from_tpp->default_arg.templ;
          }  /* if */
        }
      }  /* if */
      old_tpp = old_tpp->next;
      new_tpp = new_tpp->next;
    }  /* while */
  }  /* if */
  return !any_errors;
}  /* reconcile_template_param_lists */


/* Forward declaration. */
static a_boolean check_template_nesting_depth(
					a_symbol_ptr		sym,
					a_source_position	*pos,
					a_tmpl_decl_state_ptr	decl_state);


static a_boolean member_template_param_list_matches_class(
	                         a_tmpl_decl_state_ptr	decl_state,
                                 a_symbol_ptr		member_sym,
				 a_source_position	*error_pos)
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
  decl_info = decl_state->decl_info;
  if (member_sym->kind == (a_symbol_kind)sk_function_template ||
      member_sym->kind == (a_symbol_kind)sk_class_template) {
    decl_info = decl_info->enclosing_template_decl;
  }  /* if */
  type = member_sym->parent.class_type;
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
      if (!decl_state->decl_scope_err) {
        /* Suppress the error if we have already issued an error. */
        pos_sy_error(ec_template_depth_mismatch, error_pos, member_sym);
      }  /* if */
      any_mismatches = TRUE;
      break;
    }  /* if */
    if (!reconcile_template_param_lists(decl_info->parameters,
                                        template_sym, error_pos,
                                        /*default_allowed=*/FALSE)) {
      any_mismatches = TRUE;
    }  /* if */
    /* Skip out to the enclosing class type. */
    type = type->source_corresp.is_class_member ?
                               type->source_corresp.parent.class_type : NULL;
    if (decl_info != NULL) decl_info = decl_info->enclosing_template_decl;
  }  /* for */
  if (!decl_state->decl_scope_err && !any_mismatches) {
    /* Make sure the nesting depth of the declaration matches the entity
       being declared. */
    if (check_template_nesting_depth(member_sym, error_pos, decl_state)) {
      any_mismatches = TRUE;
    }  /* if */
  }  /* if */
  return !any_mismatches;
}  /* member_template_param_list_matches_class */


static void check_template_param_default_args(
			a_template_param_ptr	param_list,
			a_boolean		is_partial_specialization)

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
    if (has_default && is_partial_specialization) {
      /* If this is a partial specialization, make sure that it does not
         have a default template argument. */
      pos_error(ec_default_not_allowed_on_partial_spec, 
                &last_tpp_with_default->param_symbol->decl_position);
    }  /* if */
    /* If there have been parameters with default and this one doesn't have
       a default then issue an error and exit the loop. */
    if (any_defaults && !has_default) {
      pos_error(ec_default_arg_not_at_end,
                &last_tpp_with_default->param_symbol->decl_position);
    }  /* if */
    tpp = tpp->next;
  }  /* while */
}  /* check_template_param_default_args */


#if MICROSOFT_EXTENSIONS_ALLOWED
static void update_extended_decl_info_for_class_template(
                         a_template_symbol_supplement_ptr tssp,
                         an_extended_decl_info_block      *extended_decl_info,
                         a_source_position                *err_pos)
/*
Update the Microsoft decl modifiers information for the specified class
template.  Also update any instances that have already been generated.
*/
{
  a_symbol_ptr  instance_sym;
  a_type_ptr	prototype_type;
  a_symbol_ptr	prototype_sym;

  /* Update the prototype instantiation. */
  prototype_sym = tssp->variant.class_template.prototype_instantiation;
  prototype_type = type_symbol_type(prototype_sym);
  update_extended_decl_info_for_class(prototype_type, extended_decl_info,
                                      err_pos);
  /* Update any instances that have already been created. */
  for (instance_sym = tssp->variant.class_template.instantiations;
       instance_sym != NULL; instance_sym = next_instance_sym(instance_sym)) {
    a_type_ptr  tp = instance_sym->variant.class_struct_union.type;
    if (is_real_class_symbol(instance_sym) &&
        !tp->variant.class_struct_union.is_specialized) {
      update_extended_decl_info_for_class(tp, extended_decl_info, err_pos);
    }  /* if */
  }  /* for */
  if (tssp->subordinate_templates != NULL) {
    /* This is a member class template declared in another class template.
       We need to visit the template symbols for this template in each
       of the instantiations of the enclosing class template and update
       the instantiations of those templates. */
    a_symbol_list_entry_ptr	slep;
    for (slep = tssp->subordinate_templates; slep != NULL; slep = slep->next) {
      a_symbol_ptr			subordinate_sym;
      a_template_symbol_supplement_ptr	subordinate_tssp;
      subordinate_sym = slep->symbol;
      subordinate_tssp = template_supplement_for_symbol(subordinate_sym);
      update_extended_decl_info_for_class_template(subordinate_tssp,
                                                   extended_decl_info,
                                                   err_pos);
    }  /* for */
  }  /* if */
}  /* update_extended_decl_info_for_class_template */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


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
       instance_sym != NULL; instance_sym = next_instance_sym(instance_sym)) {
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


static void make_nested_class_template_supplement(a_symbol_ptr   sym,
                                                  a_type_kind	 type_kind)
/*
Given a class symbol for a nested class within a class template,
and create the template symbol supplement for the class.
*/
{
  a_template_symbol_supplement_ptr	tssp = NULL;
  a_type_ptr				parent_type = sym->parent.class_type;
  a_class_symbol_supplement_ptr		parent_cssp;
  a_template_symbol_supplement_ptr	parent_tssp = NULL;
  a_class_symbol_supplement_ptr		cssp;
  a_type_ptr				class_type;

  check_assertion(is_class_struct_union_symbol(sym));
  check_assertion(sym->is_class_member);
  /* The symbol must be for a nested class.  Make sure that the parent class
     is a prototype instantiation.  Note that the immediate parent can
     be used, we don't have to go all the way out to the outermost class
     because all enclosing classes must have been defined before the
     nested class can be defined. */
  parent_cssp = symbol_supplement_for_class(parent_type);
  parent_tssp = parent_cssp->template_info;
  class_type = sym->variant.class_struct_union.type;
  cssp = sym->variant.class_struct_union.extra_info;
  if (!parent_type->variant.class_struct_union.is_prototype_instantiation) {
    /* Under certain error cases, it is possible to have a real class
       created within a prototype instantiation.  Don't mark such symbols
       as prototype instantiations. */
  } else {
    class_type->variant.class_struct_union.is_prototype_instantiation = TRUE;
    class_type->variant.class_struct_union.is_nonreal_class =
                      parent_type->variant.class_struct_union.is_nonreal_class;
    class_type->variant.class_struct_union.is_template_class = TRUE;
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
    cssp->corresp_prototype_sym = sym;
  }  /* if */
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
  if (sym->is_class_member) {
    a_type_ptr   class_type = sym->variant.class_struct_union.type;
    a_scope_stack_entry_ptr
                 ssep = &scope_stack[depth_innermost_instantiation_scope];
    if (!ssep->in_prototype_instantiation) {
      /* Look for the prototype symbol that corresponds to this nested class
         symbol. */
      a_symbol_ptr  ct_symbol = find_corresp_prototype_tag_sym(sym);
      if (ct_symbol != NULL) {
        /* Set the pointer that points back to the original class template
           symbol. */
        a_class_symbol_supplement_ptr		cssp;
        a_template_symbol_supplement_ptr	tssp;
        cssp = sym->variant.class_struct_union.extra_info;
        tssp = template_supplement_for_symbol(ct_symbol);
        next_instance_sym(sym) = tssp->variant.class_template.instantiations;
        tssp->variant.class_template.instantiations = sym;
        cssp->corresp_prototype_sym = ct_symbol;
        class_type->variant.class_struct_union.is_template_class = TRUE;
        /* Update the friend information associated with this template.
           These are the classes that declared this template as a friend. */
        update_befriending_classes_for_class(tssp, class_type);
        /* A placeholder a_template entry was created in the prototype
           instantiation.  It serves as the associated "template". */
        class_type->variant.class_struct_union.extra_info->assoc_template =
             ct_symbol->variant.class_struct_union.type
                      ->variant.class_struct_union.extra_info->assoc_template;
      } /* if */
    } else {
      /* A nested class within a prototype instantiation. */
      /* Although this is not a template, it is an instantiatable class and
         hence we create a placeholder a_template entry for it. */
      a_template_symbol_supplement_ptr	tssp;
      a_type_ptr			parent_class;
      a_template_ptr			templ = alloc_template();
      parent_class = class_type->source_corresp.parent.class_type;
      templ->kind = (a_template_kind)templk_member_class;
      set_source_corresp(&templ->source_corresp, sym);
      set_class_membership_for_template((a_symbol_ptr)NULL, templ,
                                        parent_class);
      templ->source_corresp.access = class_type->source_corresp.access;
      templ->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
      templ->is_exported = class_is_exported(parent_class);
      add_to_templates_list(templ, depth_scope_stack);
      if (prototype_instantiations_in_il) {
        templ->prototype_instantiation.type = class_type;
      }  /* if */
      templ->canonical_template = templ;
      if (curr_token == tok_lbrace || curr_token == tok_colon) {
        templ->definition_template = templ;
      }  /* if */
      class_type->variant.class_struct_union.extra_info->assoc_template =
                                                                        templ;
      /* A nested class within a prototype instantiation.  Create the
         template symbol supplement for this class. */
      make_nested_class_template_supplement(sym, type_kind);
      tssp = template_supplement_for_symbol(sym);
      /* A NULL template supplement can be returned in certain error cases. */
      if (tssp != NULL) tssp->il_template_entry = templ;
    }  /* if */
  }  /* if */
}  /* set_nested_template_class_symbol_info */


void update_nested_template_class_symbol_info(a_symbol_ptr       sym,
                                              a_type_kind	 type_kind)
/*
This routine is like set_nested_template_class_symbol_info, but is called
in the case where a nested class in a class template is declared and then
later defined.  This routine updates the type kind information of the
template.  sym is the symbol of the nested class being defined.  type_kind
is the type kind associated with this declaration.
*/
{
  a_scope_stack_entry_ptr	ssep;

  ssep = &scope_stack[depth_innermost_instantiation_scope];
  if (sym->is_class_member) {
    /* This processing is only done for prototype instantiations.  The
       template instance test below excludes local classes. */
    if (ssep->in_prototype_instantiation &&
        is_any_template_instance_class_symbol(sym)) {
      /* Set the pointer that points back to the original class template
         symbol. */
      a_template_symbol_supplement_ptr	tssp;
      tssp = template_supplement_for_symbol(sym);
      check_assertion(tssp != NULL);
      tssp->variant.class_template.type_kind = type_kind;
    }  /* if */
  }  /* if */
}  /* update_nested_template_class_symbol_info */


static
void record_specialization(a_tmpl_decl_state_ptr		decl_state,
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
      /* When a member class template is specialized, the list of partial
         specializations should be cleared because those partial
         specializations were associated with the prototype template. */
      tssp->variant.class_template.partial_specializations = NULL;
      for (sym = tssp->variant.class_template.instantiations; sym != NULL;
           sym = next_instance_sym(sym)) {
        /* It is only an error if the class type is complete and is not a
           itself a specialization. */
        if (!is_nonreal_instance_class_symbol(sym) &&
            !is_incomplete_type(type_symbol_type(sym)) &&
            !is_template_instance_specific_def_symbol(sym)) {
          pos_sy2_error(ec_specialization_of_referenced_template,
                        &decl_state->start_pos, template_sym, sym);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* record_specialization */


static
a_boolean same_name_as_template_param(
                        a_template_decl_info_ptr template_decl_info,
                        a_symbol_locator	 *locator,
			a_boolean		 is_template_template_param)
/*
See if the class being declared has the same name as one of its
template parameters.  Is so, issue an error.  Return TRUE if an
error was diagnosed.  is_template_template_param is used to determine
the text of the message to be issued.
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
    an_error_code	code;
    code = is_template_template_param
                      ?  ec_template_template_param_same_name_as_templ_param
                      : ec_class_template_same_name_as_templ_param;
    pos_error(code, &locator->source_position);
  }  /* if */
  return err;
}  /* same_name_as_template_param */


static a_symbol_ptr add_partial_specialization(
			a_tmpl_decl_state_ptr	decl_state,
			a_symbol_ptr		partial_spec_nonreal_sym,
			a_symbol_locator	*locator,
			a_type_kind		type_kind)
/*
Create a symbol for a class template partial specialization and add it
to the list of partial specializations associated with the primary template.
partial_spec_nonreal_sym points to the symbol associated with a nonreal
class created by the initial scan of the partial specialization class name.
For example, if the primary template is A<T1,T2>, a partial specialization
declaration might be A<T1,int>.  When A<T1,int> is first scanned a nonreal
class will be created.  As a consequence of the partial specialization
declaration a new prototype instantiation for the partial specialization
will be created (this is done later).

type_kind is the type_kind used to declare the partial specialization.  Make
sure it matches the primary template.
*/
{
  a_symbol_ptr				primary_sym;
  a_template_symbol_supplement_ptr	primary_tssp;
  a_symbol_ptr				sym;
  a_template_symbol_supplement_ptr	tssp;

  primary_sym = partial_spec_nonreal_sym->
                         variant.class_struct_union.extra_info->class_template;
  check_assertion(primary_sym != NULL &&
                  primary_sym->kind == (a_symbol_kind)sk_class_template);
  primary_sym = primary_template_of(primary_sym);
  primary_tssp = primary_sym->variant.template_info;
  sym = alloc_symbol((a_symbol_kind)sk_class_template, primary_sym->header,
                     &locator->source_position);
  sym->decl_scope = primary_sym->decl_scope;
  tssp = sym->variant.template_info;
  if (primary_sym->is_class_member) {
    sym->parent.class_type = primary_sym->parent.class_type;
    sym->is_class_member = TRUE;
    tssp->variant.class_template.access = decl_state->access; 
  } else if (primary_sym->parent.namespace_ptr != NULL) {
    sym->parent.namespace_ptr = primary_sym->parent.namespace_ptr;
  }  /* if */
  tssp->variant.class_template.primary_template_sym = primary_sym;
  if (!decl_state->decl_scope_err && !is_error_locator(*locator)) {
    /* Only link the symbol to the primary template if some error has not
       already occurred. */
    sym->next = primary_tssp->variant.class_template.partial_specializations;
    primary_tssp->variant.class_template.partial_specializations = sym;
    /* Make sure that the type kind of the partial specialization matches the
       type kind of the primary template. */
    if ((type_kind == (a_type_kind)tk_union) !=
          (primary_tssp->variant.class_template.type_kind ==
                                                  (a_type_kind)tk_union)) {
      char	*type_kind_name;
      switch (type_kind) {
        case tk_struct: type_kind_name = "struct"; break;
        case tk_class:  type_kind_name = "class";  break;
        case tk_union:  type_kind_name = "union";  break;
        default: unexpected_condition();
      }  /* switch */
      /* Cannot mix union and nonunion declarations. */
      pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                     &locator->source_position,
                     type_kind_name, primary_sym);
    }  /* if */
  }  /* if */
  return sym;
}  /* add_partial_specialization */


a_template_arg_ptr create_prototype_arg_list(
			a_template_param_ptr	templ_param_list)
/*
Build the template argument list for the prototype instantiation
of this template.  Loop through the template parameters and
create a corresponding template argument for each.  Return a pointer
to the newly created list.
*/
{
  a_template_arg_ptr                tap;
  a_template_arg_ptr                list_head = NULL;
  a_template_arg_ptr                list_tail = NULL;
  a_template_param_ptr              tpp;
  a_symbol_ptr                      param_sym;

  for (tpp = templ_param_list; tpp != NULL; tpp = tpp->next) {
    param_sym = tpp->param_symbol;
    if (param_sym->kind == (a_symbol_kind)sk_type) {
      tap = alloc_template_arg((a_templ_arg_kind)tak_type);
      tap->variant.type = param_sym->variant.type.ptr;
    } else if (param_sym->kind == (a_symbol_kind)sk_constant) {
      tap = alloc_template_arg((a_templ_arg_kind)tak_nontype);
      tap->variant.constant = param_sym->variant.constant;
    } else {
      /* A template template parameter. */
      check_assertion(param_sym->kind == (a_symbol_kind)sk_class_template);
      tap = alloc_template_arg((a_templ_arg_kind)tak_template);
      tap->variant.templ = param_sym->variant.template_info->il_template_entry;
    }  /* if */
    if (list_head == NULL) list_head = tap;
    if (list_tail != NULL) list_tail->next = tap;
    list_tail = tap;
  }  /* for */
  return list_head;
} /* create_prototype_arg_list */


static void rename_prototype_arg_list(
		a_template_symbol_supplement_ptr	tssp,
		a_template_param_ptr			templ_param_list)
/*
Rename the template argument list for the prototype instantiation with
the names of the template parameters specified by templ_param_list.
*/
{
  a_template_arg_ptr		tap;
  a_template_param_ptr		tpp;
  a_symbol_ptr			param_sym;
  a_type_ptr			prototype_type;
  a_symbol_ptr			prototype_sym;
  a_class_type_supplement_ptr	ctsp;

  prototype_sym = tssp->variant.class_template.prototype_instantiation;
  prototype_type = prototype_sym->variant.class_struct_union.type;
  ctsp = prototype_type->variant.class_struct_union.extra_info;
  /* If this is a partial specialization, update the partial specialization
     argument list. */
  tap = ctsp->partial_spec_template_arg_list;
  if (tap == NULL) tap = ctsp->template_arg_list;
  for (tpp = templ_param_list; tpp != NULL; tpp = tpp->next, tap = tap->next) {
    check_assertion(tap != NULL);
    param_sym = tpp->param_symbol;
    if (param_sym->kind == (a_symbol_kind)sk_type) {
      tap->variant.type = param_sym->variant.type.ptr;
    } else if (param_sym->kind == (a_symbol_kind)sk_constant) {
      tap->variant.constant = param_sym->variant.constant;
    } else {
      check_assertion(param_sym->kind == (a_symbol_kind)sk_class_template);
      tap->variant.templ = param_sym->variant.template_info->il_template_entry;
    }  /* if */
  }  /* for */
} /* rename_prototype_arg_list */


static void create_prototype_type(
        a_tmpl_decl_state_ptr			decl_state,
	a_symbol_ptr				sym,
	a_template_symbol_supplement_ptr	tssp,
        a_symbol_ptr				partial_spec_nonreal_sym,
	a_boolean				is_partial_specialization)
/*
Create the type and symbol for the prototype instantiation of the
template specified by sym.  tssp points to the symbol supplement of sym.
partial_spec_nonreal_sym points to the symbol for the nonreal type
initially used when processing the declaration of a partial specialization.
*/
{
  a_symbol_ptr			prototype_sym;
  a_type_ptr			prototype_type;
  a_class_symbol_supplement_ptr	prototype_cssp;

 if (sym->kind == (a_symbol_kind)sk_class_template) {
    a_template_param_ptr	templ_param_list;
    a_class_type_supplement_ptr	prototype_ctsp;
    a_template_arg_ptr		templ_arg_list;
    /* This is a class template declaration, not a declaration for
       a normal class nested within a template. */
    prototype_sym = make_template_class_symbol(sym);
    /* Now create a new type entry. */
    prototype_type = alloc_type(tssp->variant.class_template.type_kind);
    prototype_type->source_corresp.access = access_for_symbol(sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    prototype_type->autonomous_primary_tag_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    prototype_type->variant.class_struct_union.is_template_class = TRUE;
    prototype_sym->variant.class_struct_union.type = prototype_type;
    set_source_corresp(&(prototype_type->source_corresp), prototype_sym);
    set_membership_in_source_corresp(&(prototype_type->source_corresp),
                                     prototype_sym);
    prototype_ctsp = prototype_type->variant.class_struct_union.extra_info;
    if (prototype_instantiations_in_il) {
      add_to_types_list(prototype_type, NO_SCOPE_DEPTH);
    }  /* if */
    prototype_ctsp->assoc_template = decl_state->il_template_entry;
    /* Use the name linkage saved at the point of the original template
       declaration. */
    prototype_type->source_corresp.name_linkage =
                                 tssp->variant.class_template.name_linkage;
    templ_param_list = decl_state->decl_info->parameters;
    /* Create a template argument list that corresponds to the template
       parameter list. */
    templ_arg_list = create_prototype_arg_list(templ_param_list);
    if (is_partial_specialization) {
      /* This is the initial declaration of a partial specialization.
         The template argument list for the partial specialization should
         be taken from the nonreal type created when the declaration was
         scanned.  For example, the declaration may have been
           template <class T1, class T2> struct A<T1*, T2*, int> { ... };
         The template argument list for the prototype instantiation should
          be "T1*, T2*, int". */
      a_class_type_supplement_ptr	partial_spec_nonreal_ctsp;
      partial_spec_nonreal_ctsp = partial_spec_nonreal_sym->variant.
                class_struct_union.type->variant.class_struct_union.extra_info;
      prototype_ctsp->template_arg_list = copy_template_arg_list(
                                 partial_spec_nonreal_ctsp->template_arg_list);
      /* Just as with a normal instance, in the prototype instantiation of a
         partial specialization the template_arg_list is with respect to the
         primary template while the partial_spec_template_arg_list is with
         respect to the partial specialization. */
      prototype_ctsp->partial_spec_template_arg_list = templ_arg_list;
    } else {
      /* A normal prototype (not a partial specialization). */
      prototype_ctsp->template_arg_list = templ_arg_list;
    }  /* if */
  } else {
    /* For a class nested within a class template, the member class
       symbol of the prototype instantiation is used. */
    prototype_sym = sym;
    prototype_type = sym->variant.class_struct_union.type;
  }  /* if */
  /* The prototype_instantiation field is set in the template supplement
     of what may be a partial specialization, not in the primary template. */
  tssp->variant.class_template.prototype_instantiation = prototype_sym;
  prototype_cssp = prototype_sym->variant.class_struct_union.extra_info;
  prototype_type->variant.class_struct_union.is_prototype_instantiation = TRUE;
  prototype_type->variant.class_struct_union.is_nonreal_class = TRUE;
  prototype_cssp->template_info = tssp;
  /* Call a routine that manages the correspondence of entities between
     translation units to notify it of the new instance. */
  record_instantiation(prototype_sym, tssp);
}  /* create_prototype_type */


static void check_local_class_template_friend(
				a_tmpl_decl_state_ptr	decl_state,
				a_symbol_locator	*locator)
/*
Check whether the current template is a friend declaration in a local
class.  If so, issue an error.
*/
{
  if (decl_state->is_template_friend) {
    a_scope_stack_entry_ptr	ssep;
    ssep = &scope_stack[depth_scope_stack];
    if (ssep->inside_local_class) {
      decl_state->decl_scope_err = TRUE;
      pos_error(ec_friend_template_in_local_class, &locator->source_position);
    }  /* if */
  }  /* if */
}  /* check_local_class_template_friend */


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
       is_or_contains_specific_template_param(tp, param_sym->variant.type.ptr);
  } else if (param_sym->kind == (a_symbol_kind)sk_constant) {
    result = type_contains_specific_template_param_constant(
                                              tp, param_sym->variant.constant);
  } else {
    /* A template template argument. */
    result = type_contains_specific_template_template_param(
                      tp, param_sym->variant.template_info->il_template_entry);
  }  /* if */
  return result;
}  /* template_param_used_in_type */


static void check_partial_spec_template_param_usage
                         (a_tmpl_decl_state_ptr	decl_state,
                          a_symbol_ptr		sym)
/*
This routine performs various checks to ensure that the template parameter
list and template argument list of a partial specialization are valid.
*/
{
  a_template_param_ptr	templ_param_list;
  a_template_param_ptr	tpp;
  a_symbol_ptr		prototype_sym;
  a_type_ptr		prototype_type;
  a_boolean		any_errors = FALSE;

  templ_param_list = decl_state->decl_info->parameters;
  /* Get the primary template argument list from the prototype instantiation
     associated with this partial specialization. */
  prototype_sym = sym->variant.template_info->
                              variant.class_template.prototype_instantiation;
  prototype_type = prototype_sym->variant.class_struct_union.type;
  /* Make sure that all of the template parameters are used as part of the
     template argument list of a partial specialization.  This is done
     by checking whether each of the template parameters is used somewhere
     by the prototype instantiation associated with the partial
     specialization. */
  for (tpp = templ_param_list; tpp != NULL; tpp = tpp->next) {
    a_symbol_ptr	param_sym = tpp->param_symbol;
    a_boolean		param_used = FALSE;
    a_boolean		error_on_this_param = FALSE;
    if (param_sym->kind != (a_symbol_kind)sk_type) {
      /* The type of a nontype parameter is not allowed to reference another
         template parameter. */
      if (tpp->variant.constant.type_involves_template_param) {
        pos_sy_error(ec_partial_spec_param_depends_on_templ_param,
                     &param_sym->decl_position, param_sym);
        any_errors = TRUE;
        error_on_this_param = TRUE;
        /* Reset the flag that indicates that this parameter has a template
           dependent type.  This is done because the routines that handle
           partial specialization later on are not prepared to handle such
           cases (because they are errors). */
        tpp->variant.constant.type_involves_template_param = FALSE;
      }  /* if */
    }  /* if */
    if (!error_on_this_param) {
      if (template_param_used_in_type(param_sym, prototype_type)) {
       param_used = TRUE;
      }  /* for */
      if (!param_used) {
        pos_sy2_error(ec_not_used_in_partial_spec_arg_list,
                      &param_sym->decl_position, param_sym, prototype_sym);
        any_errors = TRUE;
      } /* if */
    }  /* if */
  } /* for */
  if (!any_errors && !decl_state->in_prototype_instantiation) {
    /* If no errors were detected above, check each of the template arguments
       to make sure that its type is not dependent on a template parameter.
       This can happen when a value is used as a template argument (of the
       primary template) whose type depends on another template parameter. */
    a_template_arg_ptr	templ_arg_list;
    a_template_arg_ptr	tap;
    templ_arg_list = prototype_type->
                     variant.class_struct_union.extra_info->template_arg_list;
    for (tap = templ_arg_list; tap != NULL; tap = tap->next) {
      if (is_nontype_templ_arg(tap)) {
        a_constant_ptr	cp = tap->variant.constant;
        if (is_or_contains_template_param(cp->type)) {
          error(ec_partial_spec_arg_depends_on_templ_param);
          tap->variant.constant = alloc_error_constant();
        } else if (cp->kind == (a_constant_repr_kind)ck_template_param &&
                   cp->variant.template_param.kind !=
                                 (a_template_param_constant_kind)tpck_param) {
          /* A nontype argument that involves a template parameter is
             only supposed to be a single nontype parameter.  If we have
             something other than a tpck_param, issue an error. */
          error(ec_partial_spec_nontype_expr);
          tap->variant.constant = alloc_error_constant();
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* check_partial_spec_template_param_usage */


static void check_for_prior_use_of_partial_spec(a_symbol_ptr	ps_sym,
						a_symbol_ptr	primary_sym)
/*
ps_sym is a pointer to a class template symbol for a partial instantiation.
Check the existing instantiations of the primary template to determine
whether the new partial specialization would be preferred over the
template actually used to generate the instance.  primary_sym is a pointer
to the primary template whose list of instantiations is to be checked.
This is NULL when called from class_template_declaration, but is non-NULL
when this routine calls itself recursively.  This is done to for member
class templates of class templates to check the instantiations of
subordinate templates.
*/
{
  a_template_symbol_supplement_ptr	ps_tssp;
  a_template_symbol_supplement_ptr	primary_tssp;
  a_symbol_ptr				sym;

  ps_tssp = ps_sym->variant.template_info;
  if (primary_sym == NULL) {
    /* When no primary template symbol is passed in, use the one pointed
       to by this partial specialization. */
    primary_sym = ps_tssp->variant.class_template.primary_template_sym;
  }  /* if */
  primary_tssp = primary_sym->variant.template_info;
  for (sym = primary_tssp->variant.class_template.instantiations;
       sym != NULL; sym = next_instance_sym(sym)) {
    a_type_ptr				instance_type;
    instance_type = sym->variant.class_struct_union.type;
    /* Skip nonreal classes.  This includes prototype instantiations. */
    if (instance_type->variant.class_struct_union.is_nonreal_class) continue;
    /* Skip specialized classes. */
    if (instance_type->variant.class_struct_union.is_specialized) continue;
    /* Skip the instance if a full instantiation has not yet been done. */
    if (is_incomplete_type(instance_type)) continue;
    if (matches_partial_specialization(ps_sym, sym,
                                       (a_template_arg_ptr*)NULL)) {
      /* It does match the partial specialization.  Now see whether the
         existing instantiation came from the primary template or another
         partial specialization.  If it came from the primary, it is always
         an error.  If it came from another partial specialization we must
         see which of the specializations is a better match. */
      a_symbol_ptr			instance_ct_sym;
      a_template_symbol_supplement_ptr	instance_tssp;
      /* Get the class template symbol that was used to generate this
         instance. */
      instance_ct_sym = sym->
                         variant.class_struct_union.extra_info->class_template;
      instance_tssp = instance_ct_sym->variant.template_info;
      if (instance_tssp->variant.class_template.primary_template_sym == NULL) {
        /* The instance was generated from the primary template. */
        pos_sy_error(ec_partial_spec_after_instantiation,
                     &ps_sym->decl_position, sym);
      } else {
        /* The instance was generated by another partial specialization.
           See which is a better match. */
        a_boolean	new_is_more_specialized;
        a_boolean	curr_is_more_specialized;
        new_is_more_specialized = is_more_specialized(ps_sym, instance_ct_sym);
        curr_is_more_specialized = is_more_specialized(instance_ct_sym,
                                                       ps_sym);
        if (new_is_more_specialized && !curr_is_more_specialized) {
          /* The new instance is better.  Issue an error. */
          pos_sy_error(ec_partial_spec_after_instantiation,
                       &ps_sym->decl_position, sym);
        } else if (curr_is_more_specialized && !new_is_more_specialized) {
          /* The template used for the instantiation is a better match than
             this one.  This is okay. */
        } else {
          /* They are unordered.  This renders the instantiation ambiguous. */
          pos_sy_error(ec_partial_spec_after_instantiation_ambiguous,
                       &ps_sym->decl_position, sym);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (primary_tssp->subordinate_templates != NULL) {
    /* This is a member function template declared in a class template.
       We need to visit the template symbols for this template in each
       of the instantiations of the enclosing class template process
       the instantiations list of those templates. */
    a_symbol_list_entry_ptr	slep;
    for (slep = primary_tssp->subordinate_templates;
         slep != NULL; slep = slep->next) {
      a_symbol_ptr			subordinate_sym;
      subordinate_sym = slep->symbol;
      check_for_prior_use_of_partial_spec(ps_sym, subordinate_sym);
    }  /* for */
  }  /* if */
}  /* check_for_prior_use_of_partial_spec */


static a_boolean check_unqualified_template_redecl_scope(
					a_tmpl_decl_state_ptr	decl_state,
					a_symbol_ptr		sym,
					a_symbol_locator	*locator)
/*
Make sure the current scope is a valid scope for sym to be redeclared.
Return TRUE if an error was detected.
*/
{
  a_scope_number	decl_scope_number;
  a_boolean		result = FALSE;

  decl_scope_number = scope_stack[decl_state->effective_decl_level].number;
  if (sym->decl_scope != decl_scope_number) {
    if (decl_state->is_template_friend) {
      /* A scope mismatch is okay in a friend declaration. */
    } else if (sym->is_error) {
      /* Some other error occurred. */
    } else {
      pos_sy_error(ec_bad_scope_for_redeclaration,
                   &locator->source_position, sym);
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* check_unqualified_template_redecl_scope */


static a_boolean check_qualified_template_redecl_scope(
					a_tmpl_decl_state_ptr	decl_state,
					a_symbol_ptr		sym,
					a_symbol_locator	*locator,
					a_boolean		is_definition)
/*
Make sure the current scope is a valid scope for sym to be redeclared
using a qualified name.  Return TRUE if an error was detected.
*/
{
  a_scope_stack_entry_ptr	ssep =
                                &scope_stack[decl_state->effective_decl_level];
  a_namespace_ptr		nsp;
  a_namespace_ptr		curr_nsp;
  a_boolean			result = FALSE;

  /* Get the namespace that is currently being defined. */
  curr_nsp = scope_stack[depth_innermost_namespace_scope].assoc_namespace;
  nsp = parent_namespace_for_symbol(sym);
  if (!locator->is_class_member && nsp == curr_nsp && nsp != NULL
      && !decl_state->is_template_friend) {
    /* The namespace is the same as the one currently being defined.
       This is an error. */
    pos_error(ec_qualifier_in_namespace_member_decl,
              &locator->source_position);
    result = TRUE;
  } else if (!is_definition) {
    /* A declaration using a qualified name.  This is only allowed if it
       is a friend declaration. */
    if (!decl_state->is_template_friend) {
      pos_sy_error(ec_bad_scope_for_redeclaration,
                   &locator->source_position, sym);
      result = TRUE;
    }  /* if */
  } else if (decl_state->class_declared_in != NULL) {
    /* A definition using a qualified name in a class scope.  This is
       not allowed. */
    pos_error(ec_qualifier_in_member_declaration, &locator->source_position);
    result = TRUE;
  } else if (!namespace_is_enclosed_by_scope(sym, ssep)) {
    /* This definition appears within a namespace scope in which the name
       cannot be defined -- it is a member (directly or indirectly) of a
       namespace that is not enclosed by the current namespace scope
       (see WP 7.3.1.4). */
    pos_sy_error(ec_bad_scope_for_definition,
                 &locator->source_position, sym);
    result = TRUE;
  } else {
    /* Check for the definition of a nonreal member. */
    a_template_symbol_supplement_ptr	tssp;
    tssp = template_supplement_for_symbol(sym);
    if (tssp != NULL && tssp->is_nonreal_member) {
      /* An attempt to define a nonreal member. */
      pos_sy_error(ec_bad_template_name, &locator->source_position, sym);
      result = TRUE;
    }  /* if */
  }  /* if */
  if (result) decl_state->decl_scope_err = TRUE;
  return result;
}  /* check_qualified_template_redecl_scope */


static void skip_illegal_class_template_decl_specifiers(a_boolean  diagnose)
/*
A class template declaration cannot have tokens like "volatile" precede the
class key.  E.g., "template<class T> const struct X;" is not legal.  This
routine is used to enhance diagnostics on such cases.  While exploring the
token stream to see if the upcoming construct is a class template declaration,
diagnostics can be inhibited by setting diagnose to FALSE.
*/
{
  a_boolean  error_issued = FALSE;
  for (;;) {
    switch (curr_token) {
      case tok_class:
      case tok_struct:
      case tok_union:
        /* These are the possible valid tokens: continue normal parsing. */
        goto done;
      case tok_const:
      case tok_volatile:
      case tok_inline:
      case tok_typedef:
        /* Known illegal tokens: issue an error message if requested and skip
           the token. */
        if (diagnose && !error_issued) {
          error(ec_bad_class_template_decl);
        }  /* if */
        break;
      default:
        /* Unexpected illegal tokens: continue normal parsing and any errors
           will be caught downstream. */
        goto done;
    }  /* switch */
    (void)get_token();
  }  /* for */
done:;
}  /* skip_illegal_class_template_decl_specifiers */


static void set_il_template_entry(
			a_tmpl_decl_state_ptr			decl_state,
			a_symbol_ptr				sym,
			a_template_symbol_supplement_ptr	tssp)
/*
If this is the initial declaration, save a pointer to the IL template
entry in the template symbol supplement of sym.
*/
{

  if (sym != NULL) {
    check_assertion(decl_state->il_template_entry != NULL);
    if (decl_state->il_template_entry->source_corresp.assoc_info == NULL) {
      /* Set the source correspondence if it has not already been set. */
      set_source_corresp(&decl_state->il_template_entry->source_corresp, sym);
    }  /* if */
    /* If this is initial declaration, update the template symbol supplement
       to point to the IL entry . */
    if (tssp->il_template_entry == NULL) {
      tssp->il_template_entry = decl_state->il_template_entry;
    }  /* if */
  }  /* if */
}  /* set_il_template_entry */


static void update_export_flag_for_class(
			a_tmpl_decl_state_ptr			decl_state,
			a_template_symbol_supplement_ptr	tssp)
/*
tssp is the template symbol supplement for a class template or a nested
class of a class template.  Its is_exported flag may or may not have
been set by a previous declaration.  Update it to reflect an export
keyword present on the current declaration or an export keyword that
may have been present when the enclosing class was declared.
*/
{
  if (tssp != NULL && !tssp->il_template_entry->is_exported) {
    /* A class template or nested class is exported if declared as
       exported or if the enclosing class was declared as exported. */
    a_boolean	new_value = FALSE;
    if (decl_state->class_declared_in != NULL) {
      if (class_is_exported(decl_state->class_declared_in)) {
        new_value = TRUE;
      }  /* if */
    }  /* if */
    if (decl_state->export_present) new_value = TRUE;
    if (new_value && !tssp->il_template_entry->is_exported &&
        (tssp->cache.tokens.first_token != NULL &&
         !decl_state->defines_something)) {
      /* The export keyword appeared on a declaration after the definition.
         This is not allowed. */
      pos_error(ec_export_after_definition, &decl_state->export_position);
    }  /* if */
    tssp->il_template_entry->is_exported = new_value;
  }  /* if */
}  /* update_export_flag_for_class */


static void class_template_declaration(
                         a_tmpl_decl_state_ptr decl_state,
		         a_symbol_ptr          *p_sym_ptr,
		         a_boolean             *resolution)

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
  a_symbol_ptr                      sym = NULL;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     local_token_cache;
  a_type_kind                       type_kind;
  a_token_set_array                 stop_tokens;
  a_source_position                 friend_pos;
  a_boolean			    friend_token_seen = FALSE;
  a_boolean			    is_nested_class_definition = FALSE;
  a_template_param_ptr		    templ_params =
                                             decl_state->decl_info->parameters;
  a_token_cache_ptr		    definition_token_cache = NULL;
  a_token_kind			    next_tok;
  a_boolean			    is_partial_specialization = FALSE;
  a_symbol_ptr			    partial_spec_nonreal_sym = sym;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_extended_decl_info_block       extended_decl_info;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                         saved_sses_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "class_template_declaration");
  if (curr_token == tok_typedef || curr_token == tok_auto ||
      curr_token == tok_register) {
    error(curr_token == tok_typedef ?
            ec_typedef_not_allowed : ec_bad_storage_class_on_template_decl);
    (void)get_token();
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  decl_state->decl_pos_block.specifiers_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (curr_token == tok_friend) {
    /* The is_template_friend flag should already be set.  The exception
       is an error case in which "friend" appears outside of a class. */
    check_assertion(!decl_state->is_member_decl ||
                    decl_state->is_template_friend ||
                    decl_state->decl_scope_err);
    /* Set it, just in case is wasn't already set because of use outside
       of a class.  This permits the error to be diagnosed below. */
    decl_state->is_template_friend = TRUE;
    friend_pos = pos_curr_token;
    friend_token_seen = TRUE;
    (void)get_token();
  }  /* if */
  skip_illegal_class_template_decl_specifiers(/*diagnose=*/TRUE);
  switch (curr_token) {
    case tok_class:  type_kind = (a_type_kind)tk_class;  break;
    case tok_struct: type_kind = (a_type_kind)tk_struct; break;
    case tok_union:  type_kind = (a_type_kind)tk_union;  break;
    default:	     unexpected_condition();
  }  /* switch */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Set the specifiers end position here; it will be overwritten later unless
     there is an error in scanning the identifier. */
  decl_state->decl_pos_block.specifiers_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Bypass "class", "struct", or "union". */
  (void)get_token();
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Scan any Microsoft extended decl modifiers that may be present
       such as __single_inheritance. */
    a_boolean	err = FALSE;
    clear_extended_decl_info_block(extended_decl_info);
    scan_extended_decl_modifiers(/*is_class_decl=*/TRUE,
                                 decl_state->is_member_decl,
                                 &extended_decl_info, &err);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Next should be the class name. */
  if (!is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                       GID_USE_PROTOTYPE_NOT_NONREAL |
				       GID_IS_CLASS_TEMPLATE_DECL)) {

    /* Not an identifier. */
    error(ec_exp_identifier);
    set_to_error_locator(locator);
    /* Probably a missing class name -- use the current token as the next
       token for lookahead purposes. */
    next_tok = curr_token;
  } else if (is_error_locator(locator_for_curr_id)) {
    /* An incorrectly formed identifier. */
    set_to_error_locator(locator);
    next_tok = next_token();
  } else if (locator_for_curr_id.is_operator_name ||
             locator_for_curr_id.is_conversion_name) {
    /* Issue an error for something like "class operator+" or
       "class operator int". */
    pos_error(ec_operator_name_not_allowed,
              &locator_for_curr_id.source_position);
    set_to_error_locator(locator);
    next_tok = next_token();
  } else {
    /* Look up the identifier.  If it's a qualified name there will be an
       error down the line.  The options used when coalescing the 
       identifier are specified above. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_state->decl_pos_block.identifier_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* For friend declarations, or declarations in which the template name
       is a qualified name, and for cases where the template name is a
       template ID (i.e., for partial specializations) do a normal lookup.
       For unqualified references that are not in friend declarations, just
       look in the current scope. */
    if (decl_state->is_template_friend ||
        locator_for_curr_id.is_qualified_name ||
        locator_for_curr_id.is_template_id) {
      a_boolean	err = FALSE;
      sym = coalesce_and_lookup_generalized_identifier
                             (GID_CLASS_TEMPLATE_REQUIRED,
                              ilm_template_linkage, &err);
      /* If the class name is a template ID, then this is probably a
         declaration of a partial specialization. */
      if (sym != NULL && is_template_class_symbol(sym) &&
          locator_for_curr_id.is_template_id) {
        is_partial_specialization = TRUE;
      }  /* if */
      /* If the symbol found is an injected template symbol, replace it with
         the template that it represents. */
      if (sym != NULL && is_injected_template_symbol(sym)) {
        sym = class_template_for_injected_template_symbol(sym);
      }  /* if */
    } else {
      /* Look up the symbol in the current scope.  To do this we must
         temporarily change the decl. scope level to the effective
         level for this declaration because decl_scope_level currently
         points to the template declaration scope. */
      a_scope_depth	saved_decl_scope_level = decl_scope_level;
      decl_scope_level = decl_state->orig_decl_level;
      sym = curr_scope_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
      decl_scope_level = saved_decl_scope_level;
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_state->decl_pos_block.identifier_range.end = end_pos_curr_token;
    decl_state->decl_pos_block.specifiers_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
      /* If the lookup found a class template that is actually a template
         template parameter, ignore it.  This will result in a redeclaration
         error later. */
      if (sym != NULL && sym->is_template_param &&
          sym->kind == (a_symbol_kind)sk_class_template) {
        sym = NULL;
      } else if (sym != NULL && !decl_state->in_prototype_instantiation &&
                 sym->kind == (a_symbol_kind)sk_class_template &&
                 sym->variant.template_info->is_nonreal_member) {
        /* A template friend declaration that refers to a nonreal template
           is not allowed. */
        pos_error(ec_friend_is_nonreal_template, &locator.source_position);
      }  /* if */
    } else if (friend_token_seen) {
      /* A friend declaration in a nonclass scope.  Only issue the error
         if we actually scanned the friend token in this routine.  If
         the is_template_friend flag was set elsewhere, this must be
         a badly formed declarations -- assume an error has been issued. */
      pos_error(ec_bad_specifier_outside_class_decl, &friend_pos);
      decl_state->decl_scope_err = TRUE;
    }  /* if */
    /* Make sure the friend is not in a local class. */
    check_local_class_template_friend(decl_state, &locator);
  }  /* if */
  if (is_partial_specialization) {
    a_boolean	err = FALSE;
    /* If this is a partial specialization, the symbol that was returned
       by the lookup will be the prototype instantiation associated with
       the partial specialization.  If this is the case, reset the symbol
       to point to the class template symbol associated with the partial
       specialization.  If instead the symbol points to a nonreal class,
       then this is probably the initial declaration of the partial
       specialization in which case we save a pointer to the nonreal class
       so that we retain the information about the template argument list
       that was used and we reset the symbol pointer to NULL so that a new
       class template symbol will be created below. */
    if (is_prototype_instantiation_symbol(sym)) {
      sym = sym->variant.class_struct_union.extra_info->class_template;
      check_assertion(sym != NULL);
      if (sym->variant.template_info->
                         variant.class_template.primary_template_sym == NULL) {
        /* The template found is the prototype instantiation of the primary
           template.  This occurs if the primary template was named in the
           template argument list of a partial specialization.  This is
           not permitted. */
        pos_error(ec_partial_spec_is_primary_template,
                  &locator.source_position);
        err = TRUE;
      }  /* if */
    } else if (is_nonreal_instance_class_symbol(sym)) {
      a_scope_stack_entry_ptr	ssep =
                                &scope_stack[decl_state->effective_decl_level];
      if (sym->is_class_member && decl_state->class_declared_in == NULL &&
          !template_for_instance(sym)->
                               variant.template_info->is_specific_definition) {
        /* A partial specialization must be declared in the class of which it
           is a member.  An exception is made for partial specializations
           of a template that is itself a specialization of a member class
           template. */
        pos_error(ec_member_partial_spec_not_in_class,
                  &locator.source_position);
        err = TRUE;
      } else if (!decl_state->is_template_friend &&
                 decl_state->class_declared_in != NULL &&
                 (!sym->is_class_member ||
                   sym->parent.class_type != decl_state->class_declared_in)) {
        /* A partial specialization in a class, but the entity found is from
           a different scope. */
        pos_sy_error(ec_cannot_be_declared_in_scope, &locator.source_position,
                     sym);
        err = TRUE;
      } else if (!sym->is_class_member &&
                 ssep->assoc_namespace != sym->parent.namespace_ptr) {
        pos_error(ec_member_partial_spec_not_in_namespace,
                  &locator.source_position);
        err = TRUE;
      } else {
        partial_spec_nonreal_sym = sym;
        sym = NULL;
      }  /* if */
    } else {
      /* The symbol found is a real class.  This is an invalid partial
         specialization. */
      pos_sy_error(ec_bad_partial_specialization, &locator.source_position,
                   sym);
      err = TRUE;
    }  /* if */
    if (decl_state->is_template_friend &&
        !decl_state->decl_scope_err && !err) {
      /* A partial specialization is not permitted in a friend declaration. */
      pos_error(ec_friend_partial_specialization, &locator.source_position);
      err = TRUE;
    }  /* if */
    if (err) {
      is_partial_specialization = FALSE;
      decl_state->decl_scope_err = TRUE;
      sym = NULL;
    }  /* if */
  }  /* if */
  if (sym != NULL && !decl_state->decl_scope_err && !sym->is_error) {
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
                                 !locator.is_template_id &&
                                 tssp != NULL;
  }  /* if */
  /* See if the class being declared has the same name as one of its
     template parameters. */
  if (same_name_as_template_param(decl_state->decl_info, &locator,
                                  /*is_template_template_param=*/FALSE)) {
    sym = NULL;
    suppress_redecl_error = TRUE;
  }  /* if */
  if (!decl_state->decl_scope_err) {
    if (!locator.is_qualified_name) {
      if (sym != NULL) {
        /* Unless this is a friend declaration, an unqualified name must refer
           to a name from the current scope. */
        suppress_redecl_error = check_unqualified_template_redecl_scope(
                                                   decl_state, sym, &locator);
      } else if (is_partial_specialization &&
                 partial_spec_nonreal_sym != NULL) {
        suppress_redecl_error = check_unqualified_template_redecl_scope(
                               decl_state, partial_spec_nonreal_sym, &locator);
      }  /* if */
    } else if (locator.is_qualified_name) {
      if (sym != NULL) {
        suppress_redecl_error = check_qualified_template_redecl_scope(
                                     decl_state, sym, &locator, is_definition);
      } else if (is_partial_specialization &&
                 partial_spec_nonreal_sym != NULL) {
        suppress_redecl_error = check_qualified_template_redecl_scope(
                                          decl_state, partial_spec_nonreal_sym,
                                          &locator, is_definition);
      }  /* if */
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
      } else if (is_definition) {
        /* Attempting to redefine a class template. */
        pos_sy_error(ec_already_defined, &locator.source_position, sym);
        err = TRUE;
      }  /* if */
      if (decl_state->decl_scope_err) err = TRUE;
      if ((is_definition || is_redecl) && sym != NULL) {
        /* Either a definition or a redeclaration.  Make sure the template
           parameters are compatible with the previous declaration. */
        if (sym->is_class_member &&
            !decl_state->in_prototype_instantiation &&
            (decl_state->class_declared_in == NULL ||
             decl_state->is_template_friend)) {
          /* If this is a class member defined outside of its class or a friend
             function declaration in a class.  Make sure that the template
             parameters match those of the original class definition. */
          if (!member_template_param_list_matches_class(decl_state,
                                                        sym,
                                                        &error_position)) {
            err = TRUE;
          } /* if */
        }  /* if */
        if (!err && sym->kind == (a_symbol_kind)sk_class_template &&
            !tssp->is_nonreal_member) {
          /* If this is a class template, make sure the template parameters
             match a previous declaration of the class.  This test is not
             done if the template found is a nonreal template (that has no
             template parameter list). */
          if (microsoft_bugs && sym->defined) {
            /* The Microsoft compiler does not check the parameter list
               of a template that is redeclared after it has been defined. */
          } else if (!reconcile_template_param_lists(
                                templ_params, sym, &locator.source_position,
                                /*default_allowed=*/TRUE)) {
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
  check_template_param_default_args(templ_params, is_partial_specialization);
  if (sym == NULL) {
    /* Enter the symbol at the scope indicated by effective_decl_level. */
    a_scope_stack_entry_ptr	ssep =
                                &scope_stack[decl_state->effective_decl_level];
    if (is_partial_specialization && partial_spec_nonreal_sym == NULL) {
      /* A partial specialization cannot be entered if no partial spec.
         nonreal symbol is available.  This situation can occur in certain
         error cases.  Clear the is_partial_specialization flag and continue
         with this declaration as a normal template. */
      check_assertion(is_error_locator(locator));
      is_partial_specialization = FALSE;
    }  /* if */
    if (is_partial_specialization) {
      /* The symbol being created is for a partial specialization.  Create
         the symbol. */
      sym = add_partial_specialization(decl_state, partial_spec_nonreal_sym,
                                       &locator, type_kind);
      tssp = sym->variant.template_info;
    } else {
      sym = enter_symbol((a_symbol_kind)sk_class_template, &locator,
                         decl_state->effective_decl_level,
                         suppress_redecl_error);
      if (!friend_injection_enabled) {
        /* If the class template is initially declared in a friend declaration,
           mark it as invisible. */
        sym->is_invisible = decl_state->is_template_friend;
      }  /* if */
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
    }  /* if */
    /* Save the type kind on the initial declaration.  This may be modified
       later on a definition. */
    tssp->variant.class_template.type_kind = type_kind;
    /* Set the name-linkage for this template -- it will be propagated
       into the instances. */
    /* Normally, a template has C++ linkage. */
    tssp->variant.class_template.name_linkage =
                            (a_name_linkage_kind)nlk_cplusplus_external;
    /* Save the IL template entry pointer for this symbol. */
    set_il_template_entry(decl_state, sym, tssp);
    is_redecl = FALSE;
  }  /* if */
  /* Make sure the is_exported flag is set properly. */
  update_export_flag_for_class(decl_state, tssp);
  if (is_definition) {
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
  if (sym->is_class_member && sym->kind == (a_symbol_kind)sk_class_template &&
      !is_redecl) {
    /* This is a member class template declaration.  See if the enclosing
       class was also generated from a template.  If so, find the
       corresponding class template symbol from the prototype instantiation. */
    if (decl_state->in_prototype_instantiation) {
      /* Save the token sequence number associated with this declaration.
         This is done here for function templates that are class members.
         This information is used later to match a template declaration in
         a real instantiation with the corresponding template from the
         prototype instantiation. */
      tssp->token_sequence_number = curr_token_sequence_number;
    } else {
      if (decl_state->class_declared_in != NULL) {
        /* Only do this for the original declaration inside the class. */
        find_class_template_member(sym, sym->parent.class_type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (decl_state->is_specialization && !decl_state->is_template_friend) {
    /* This template is a specialization of a member template.  Update the
       template information to reflect this. */
    record_specialization(decl_state, sym, tssp);
  }  /* if */
  if (tssp->variant.class_template.prototype_instantiation == NULL &&
      (tssp->prototype_template == NULL || is_partial_specialization ||
       tssp->is_specific_definition || is_definition)) {
    /* Create the symbol for the prototype instantiation (but don't do
       the instantiation yet).  The prototype instantiation type is
       not created for subordinate templates unless they are have been
       specialized.  The "is_definition" test is there for error cases.
       Subordinate templates should have had their bodies removed already,
       but may still appear to be defined if the actual definition is
       improperly formed.  Prototype types are needed for partial
       specializations, because the template argument list of the
       prototype instantiation must be recorded. */
    create_prototype_type(decl_state, sym, tssp, partial_spec_nonreal_sym,
                          is_partial_specialization);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (tssp->prototype_template == NULL || tssp->is_specific_definition) {
      /* Update any decl modifiers that may have been specified.  Don't
         do this for subordinate templates -- the prototype of the prototype
         template is used. */
      update_extended_decl_info_for_class_template(tssp, &extended_decl_info,
                                                   &locator.source_position);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (is_partial_specialization && !is_redecl) {
    /* Make sure that the template parameters are used correctly in the
       partial specialization template argument list. */
    check_partial_spec_template_param_usage(decl_state, sym);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (prototype_instantiations_in_il) {
    /* Prevent the generation of a source sequence entry for the a_template
       entry since we have one for the recorded prototype instantiation. */
    saved_sses_disallowed = source_sequence_entries_disallowed;
    source_sequence_entries_disallowed = TRUE;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
      decl_state->definition_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Swallow the "{" and then cache everything through to the "}". */
      cache_curr_token(definition_token_cache);
      (void)get_token();
      incr_token_set_array_element(stop_tokens, tok_rbrace);
      cache_token_stream(definition_token_cache, stop_tokens);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      decl_state->definition_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
#if RECORD_HIDDEN_NAMES_IN_IL
    } else if (decl_state->is_template_friend) {
      /* Set the flag directly, since record_symbol_declaration is not
         called. */
      sym->header->any_decl_in_file_or_namespace_scope = TRUE;
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    }  /* if */
    /* This is not a class template definition, so we have no need to
       cache the tokens. */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (prototype_instantiations_in_il) {
    /* Restore the previous state wrt. the generation of source sequence
       entries. */
    source_sequence_entries_disallowed = saved_sses_disallowed;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_class_template &&
        is_definition && tssp->cache.decl_info != NULL) {
      /* This is a definition of a previously declared template.  Update
         the names of the prototype instantiation arguments to reflect
         the template parameter names used on the definition. */
      rename_prototype_arg_list(tssp, decl_state->decl_info->parameters);
    }  /* if */
    if (is_definition || tssp->cache.decl_info == NULL) {
      /* Save the information needed to create an instantiation based
         on the definition of the template.  This information is saved
         for the definition and also for the initial declaration. */
     set_template_cache_info(&tssp->cache, definition_token_cache,
                              decl_state->decl_info);
    }  /* if */
    if (is_partial_specialization && !is_redecl) {
      /* Check any existing instances to see if the new partial specialization
         would have been a better match. */
      check_for_prior_use_of_partial_spec(sym, (a_symbol_ptr)NULL);
    }  /* if */
  }  /* if */
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_class_template) {
    if (!friend_injection_enabled) {
      /* If this is not a friend declaration, mark the symbol as visible.
         A class template declared only in template friend declarations is
         not otherwise visible. */
      if (!decl_state->is_template_friend) sym->is_invisible = FALSE;
    }  /* if */
  }  /* if */
  *p_sym_ptr = sym;
  db_exit();
}  /* class_template_declaration */


static void cache_function_template_body(a_tmpl_decl_state_ptr decl_state,
                                         a_token_cache         *p_token_cache,
                                         a_boolean             is_constructor,
                                         a_source_position     *decl_pos)
/*
Scan a function template body and cache the tokens (in *p_token_cache) so
that they can be rescanned for the instantiation.  is_constructor is
TRUE if the function is a constructor.  The current source position is
immediately after the function declarator.  decl_pos is the position of the
function declarator.
*/
{
  a_source_position	start_pos;
  a_source_position	end_pos;
  a_boolean		missing_end;

  db_enter(3, "cache_function_template_body");
  if (cache_function_body(p_token_cache, is_constructor, &missing_end,
                          (a_token_sequence_number*)NULL,
                          (a_token_sequence_number*)NULL,
                          &start_pos, &end_pos) || missing_end) {
    /* Even a partial definition is considered to define something. */
    decl_state->defines_something = TRUE;
  }  /* if */
  if (missing_end) {
    /* The ending brace of the function template was not found.  This is
       usually the result of a mismatched delimiter. */
    pos_error(ec_template_missing_closing_brace, decl_pos);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  decl_state->definition_range.start = start_pos;
  decl_state->definition_range.end = end_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* cache_function_template_body */


static void prescan_template_declaration(a_tmpl_decl_state_ptr decl_state,
					 a_boolean	       skip_params)
/*
Scan the tokens of a template declaration and determine whether
it is full specialization, and whether the token "friend" is used in
the declaration.

skip_params is TRUE if this routine is being called a second time to
when recaching the template declaration, but not the template parameter list.
This is done in certain error cases when the initial caching did not
cache the expected tokens.
*/
{
  a_boolean		is_template_friend = FALSE;
  a_boolean		is_full_specialization = TRUE;
  a_token_cache_ptr	p_cache;

  if (skip_params) {
    p_cache = &decl_state->decl_token_cache;
  } else {
    p_cache = &decl_state->param_list_cache;
  }  /* if */
  rescan_reusable_cache(p_cache);
  if (!skip_params) {
    /* See if the beginning of the declaration consists of template
       parameter clauses that are all of the form "template <>". */
    while (curr_token == tok_template) {
      (void)get_token();
      if (curr_token != tok_lt) continue;
      (void)get_token();
      if (curr_token != tok_gt) {
        is_full_specialization = FALSE;
        continue;
      }  /* if */
      (void)get_token();
    }  /* while */
    decl_state->is_full_specialization = is_full_specialization;
  }  /* if */
  /* Go through the remaining tokens of the cache.  We have to scan all
     the way to the end even if the friend token is found so that the
     token stream will be at the right place when we return. */
  while (curr_token != tok_end_of_source) {
    if (curr_token == tok_friend) is_template_friend = TRUE;
    (void)get_token();
  }  /* if */
  /* Skip past the tok_end_of_source. */
  (void)get_token();
  decl_state->is_template_friend = is_template_friend;
}  /* prescan_template_declaration */


static void cache_template_declaration(a_tmpl_decl_state_ptr decl_state,
				       a_boolean	     skip_params)
/*
Scan one or more template parameter clauses and the declaration that
follows, and cache the tokens so that they can be rescanned for the
instantiation.  The declarations for functions must be saved
so that they may be rescanned with the appropriate values substituted
for the template parameters.  The parameter clauses and the actual
declaration are scanned into the parameter list cache at this point.
The template declaration will be split into a separate cache later
later once the template parameter clauses have been scanned.

An initial pass is made through the cache to determine if the declaration
is a full specialization, and whether the declaration is a friend
declaration.  A full specialization is one in which all of the template
clauses contain empty template parameter lists.

skip_params is TRUE if this routine is being called a second time to
recache the template declaration, but not the template parameter list.
This is done in certain error cases when the initial caching did not
cache the expected tokens.
*/
{
  a_token_set_array  stop_tokens;
  a_token_cache_ptr  p_cache;

  db_enter(3, "cache_template_declaration");
  if (skip_params) {
    p_cache = &decl_state->decl_token_cache;
  } else {
    p_cache = &decl_state->param_list_cache;
  }  /* if */
  /* Cache the current token and advance past it. */
  cache_curr_token(p_cache);
  (void)get_token();
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* Cache all tokens up to the ";" that follows a declaration or the "{" that
     begins a definition.  We don't stop on the ":" that begins a ctor
     initializer list because there are other contexts in which a ":"
     could occur that cannot be detected during the caching process
     (for example, a ? : operator in a default template argument).
     For static data members, some or all of the initializer will be
     in the cache.  The initializer tokens will be removed from this
     cache later.  The only case in which the entire initializer will not
     be in this cache is in cases where the initializer contains a
     brace enclosed list. */
  incr_token_set_array_element(stop_tokens, tok_lbrace);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  cache_token_stream(p_cache, stop_tokens);
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(p_cache);
  /* Do an initial scan of the template declaration to determine whether
     it is a full specialization and/or a friend declaration. */
  prescan_template_declaration(decl_state, skip_params);
  /* Rescan a copy of the cached tokens from this cache.  This is done so that
     when the original template declaration is scanned the last token of
     the cache is followed by the token that followed it in the original
     source program with no intervening tok_end_of_source.  This also
     allows the reusable token cache to be discarded if it turns out that
     this is not a function declaration. */
  rescan_copy_of_cache(p_cache);
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
     list of previous scopes that are examined.  Microsoft specialization
     scopes are ignored because they represent specializations and not
     actual instantiations. */
  for (; ssep != NULL; ssep = previous_scope_of(ssep)) {
    if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      if (!ssep->microsoft_specialization_instantiation_scope) {
        curr_depth++;
      }  /* if */
    }  /* if */
  }  /* for */
  return curr_depth;
}  /* template_nesting_depth */


void prescan_function_template_default_arg_expr(a_param_type_ptr  ptp)
/*
Scan a default argument expression and add it to the list of arguments
pointed to by the template symbol supplement.  "ptp" can be NULL if
the tokens should be scanned and discarded.
*/
{
  a_def_arg_expr_fixup_ptr	*list;
  a_scope_stack_entry_ptr	ssep;
  a_token_cache_ptr		decl_cache;

  if (curr_token == tok_removed_default_arg) {
    /* If we are scanning a removed default argument, just bypass the token. */
    (void)get_token();
  } else {
    /* The current scope stack entry is expected to be a function prototype
       scope.  The enclosing scope is expected to be either the template
       declaration scope for the current function template or the instantiation
       scope for the partial instantiation of a template function declaration.
       In the latter case, the tokens that are cached are simply discarded. */
    ssep = scope_stack_entry_for(depth_scope_stack-1);
    if (ssep->kind == (a_scope_kind)sck_template_declaration) {
      /* Get a pointer to the declaration token cache for the function
         template. */
      decl_cache = &ssep->tmpl_decl_state->decl_token_cache;
    } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
      a_symbol_ptr			template_sym = ssep->template_sym;
      a_template_symbol_supplement_ptr	tssp;
      /* Get a pointer to the decl_cache associated with the function template
         whose declaration is being instantiated. */
      check_assertion(template_sym->kind ==
                                          (a_symbol_kind)sk_function_template);
      tssp = template_supplement_for_symbol(template_sym);
      decl_cache = &tssp->variant.function.decl_cache.tokens;
    }  /* if */
    list = &curr_default_args;
    prescan_default_function_arg_expr(ptp, list, decl_cache,
                                      /*is_function_template=*/TRUE,
				      /*is_friend_decl=*/FALSE);
  }  /* if */
  /* Indicate that this default argument is a template default argument
     whose expression has not yet been evaluated. */
  if (ptp != NULL) ptp->has_unevaluated_template_default = TRUE;
}  /* prescan_function_template_default_arg_expr */


static void prescan_template_param_decl(a_token_cache	      *token_cache,
                                        a_tmpl_decl_state_ptr decl_state)
/*
Place the tokens for a template parameter into a token cache.
*/
{
  a_token_set_array  stop_tokens;

  db_enter(3, "prescan_template_param_decl");
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* In the normal case we will scan an expression and encounter a comma
     or right parenthesis.  If both of these are omitted, terminate the token
     stream when some likely delimiter is reached. */
  incr_token_set_array_element(stop_tokens, tok_comma);
  incr_token_set_array_element(stop_tokens, tok_gt);
  incr_token_set_array_element(stop_tokens, tok_semicolon);
  clear_token_cache(token_cache, /*reusable=*/TRUE);
  cache_token_stream_coalesce_identifiers(token_cache, stop_tokens,
                                          &decl_state->param_list_cache);
  /* Note that the terminating token (comma, etc.) is not added to
     the cache. */
  terminate_token_cache(token_cache);
  /* Rescan a copy of the tokens that were just cached.  Rescanning a copy
     ensures that processing of the remainder of the original line will
     not be affected by the tok_end_of_source that terminates the cache. */
  rescan_copy_of_cache(token_cache);
  db_exit();
}  /* prescan_template_param_decl */


static void scan_a_template_parameter_declaration(
				a_symbol_locator	*param_locator,
				a_type_ptr		*param_type_ptr,
				a_boolean		*is_unnamed,
				a_boolean		*template_dependent)
/*
Scan the declaration of a single template nontype parameter.  If the
parameter is unnamed, and is_unnamed is not NULL, return a flag indicating
whether the nontype parameter is unnamed.  If the parameter type
depends on a template parameter type, return TRUE in *template_dependent
(if it is not NULL).
*/
{
  a_decl_flag_set              do_flags;
  a_decl_flag_set              dso_flags;
  a_type_qualifier_set         qualifiers;
  a_decl_modifiers_block       decl_modifiers;
  a_storage_class              param_storage_class;
  a_source_position            param_pos;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;
  a_type_ptr                   tp;
  a_decl_pos_block             decl_pos_block;
  a_type_qualifier_set         array_qualifiers;

  /* Scan the declaration specifiers. */
  param_pos = pos_curr_token;
  clear_decl_pos_block(&decl_pos_block);
  (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_IS_TEMPLATE_PARAMETER),
                         &dso_flags, &param_storage_class, param_type_ptr,
                         &qualifiers, &decl_modifiers, &decl_pos_block);
  if (dso_flags & DSO_DEFINES_SOMETHING) {
    pos_error(ec_type_definition_not_allowed, &param_pos);
    *param_type_ptr = error_type();
  }  /* if */
  if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  /* Scan the declarator. */
  declarator((DI_REAL_DECLARATOR_ALLOWED |
              DI_ABSTRACT_DECLARATOR_ALLOWED |
              DI_IS_TEMPLATE_PARAM_DECL),
             &do_flags, &array_qualifiers, *param_type_ptr,
             /*member_parent_type=*/(a_type_ptr)NULL, param_locator,
             param_type_ptr, &declarator_ssep,
             (a_func_info_block_ptr)NULL, &decl_pos_block);
  if (is_unnamed != NULL) {
    /* Return a flag indicating whether the parameter is unnamed. */
    *is_unnamed = (do_flags & DO_REAL_DECLARATOR_SCANNED) == 0;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (declarator_ssep != NULL) {
    /* Declarators appearing in template parameter list need not be recorded
       in the source sequence entry lists. */
    remove_from_src_seq_list(declarator_ssep);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (template_dependent != NULL) {
    /* Check whether the type depends on a template parameter.  This is
       done before the parameter type is adjusted below because certain
       dependencies could be eliminated. */
    *template_dependent = is_or_contains_template_param(*param_type_ptr);
  }  /* if */
  /* Adjust the type if necessary (for example, "array of x"
     becomes "pointer to x"). */
  adjust_parameter_type(param_type_ptr, array_qualifiers);
  /* Check for illegal nontype parameter types.  Template parameters of
     void type, class type, and floating point type are not permitted
     by the standard.  Floating point template parameters are still
     accepted when floating_point_template_parameters_allowed is TRUE.
     Template parameters of array type are permitted even though there
     is no way to make use of them. */
  tp = skip_typerefs(*param_type_ptr);
  if (is_void_type(tp)) {
    /* A parameter type of void is not allowed. */
    pos_error(ec_void_template_parameter, &param_pos);
    *param_type_ptr = error_type();
    /* Change the parameter type to an error type.  This is done to prevent
       template parameters from having unexpected types. */
    *param_type_ptr = error_type();
  } else if (is_class_struct_union_type(tp)) {
    /* A template parameter cannot have class type. */
    pos_error(ec_template_parameter_has_class_type, &param_pos);
    /* Change the parameter type to an error type.  This is done to prevent
       template parameters from having unexpected types.  In particular,
       nontype parameters with incomplete class types are problematic. */
    *param_type_ptr = error_type();
  } else if (tp->kind == (a_type_kind)tk_float) {
    if (!floating_point_template_parameters_allowed) {
      /* A floating-point template parameter type is no longer allowed
         as of 3/94. */
      pos_error(ec_float_template_parameter, &param_pos);
    }  /* if */
  }  /* if */
}  /* scan_a_template_parameter_declaration */


static a_symbol_kind determine_template_param_kind(void)
/*
Determine the kind of template parameter that is being scanned.
Return the symbol kind for the parameter symbol to be created for
this parameter.
*/
{
  a_symbol_kind	result;
  a_token_kind	next_tok;
  a_token_kind	second_token;
  a_token_kind	token_after_id;
  a_boolean	is_end_of_param;

  /* Determine whether this is a "type-argument" (a parameter that
     represents a type) or a "parameter-declaration" (a parameter that
     represents a constant).  A type argument may be specified as "class T"
     or "typename T", or if the parameter is unnamed, simply "class" or
     "typename".  "class" and "typename" may also be used at the beginning
     of the declaration of a nontype parameter.  The parameter is considered
     to be a type parameter if it is "class" or "typename" followed by an
     optional simple (i.e., nonqualified) identifier.  A template template
     parameter begins with they keyword "template".  All other cases are
     considered to be nontype parameters. */
  next_tok = next_two_tokens(tok_identifier, &second_token);
  token_after_id = next_tok == tok_identifier ? second_token : next_tok;
  is_end_of_param = token_after_id == tok_comma ||
                    token_after_id == tok_gt ||
                    token_after_id == tok_assign;
  if ((curr_token == tok_class || curr_token == tok_typename) &&
      is_end_of_param) {
    /* A type parameter. */
    result = (a_symbol_kind)sk_type;
  } else if (curr_token == tok_template) {
    /* A template template parameter. */
    result = (a_symbol_kind)sk_class_template;
  } else {
    /* A nontype parameter. */
    result = (a_symbol_kind)sk_constant;
  }  /* if */
  return result;
}  /* determine_template_param_kind */


static a_symbol_ptr create_template_param_symbol(
					a_symbol_kind		kind,
					a_symbol_locator	*locator,
					a_boolean		is_unnamed)
/*
Create the symbol for a template parameter.  Return the symbol.
*/
{
  a_symbol_ptr	sym;

  if (!is_unnamed) {
    /* Create a symbol of the appropriate name and kind. */
    sym = enter_symbol(kind, locator, decl_scope_level,
                       /*suppress_redecl_error=*/FALSE);
  } else {
    sym = make_unnamed_template_param_symbol(kind, &pos_curr_token);
  }  /* if */
  sym->is_template_param = TRUE;
  mark_defined(sym, &sym->decl_position);
  return sym;
}  /* create_template_param_symbol */


static a_template_param_ptr scan_type_template_param(
		a_tmpl_decl_state_ptr decl_state,
		a_template_param_list_pos	template_param_list_pos)
/*
Scan the declaration of a type template parameter.  Return the template
parameter entry for the parameter.
*/
{
  a_boolean		is_named;
  a_symbol_ptr		sym;
  a_type_ptr		template_param_type;
  a_template_param_ptr	template_param;

  /* Bypass "class" or "typename". */
  (void)get_token();
  is_named = curr_token == tok_identifier;
  /* Create an sk_type symbol for the parameter. */
  sym = create_template_param_symbol((a_symbol_kind)sk_type,
                                     &locator_for_curr_id, !is_named);
  /* Bypass the identifier. */
  if (is_named) (void)get_token();
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
  if (!is_named) {
    /* Reset the name in the source correspondence entry.  An unnamed
       type is represented by NULL, not "<unnamed>" as indicated by the
       symbol header. */
    template_param_type->source_corresp.name = NULL;
  }  /* if */
  /* The type symbol for the template parameter points for now to the
     template-param type -- "for now", since it will be replaced with
     an actual type during instantiation of the class or function. */
  sym->variant.type.ptr = template_param_type;
  /* Allocate a template parameter and set its fields based on sym. */
  template_param = alloc_template_param(sym);
  if (curr_token == tok_assign) {
    a_token_cache  def_arg_cache;
    a_boolean	   def_arg_involves_template_param = FALSE;
    a_type_ptr	   default_arg_type;
    /* Scan the default value for a type argument. */
    /* Skip past the equals sign. */
    (void)get_token();
    /* Cache the tokens that make up the default argument expression. */
    prescan_default_arg_expr(&def_arg_cache, /*is_template_param=*/TRUE,
                             /*is_function_template=*/FALSE,
			     /*is_friend_decl=*/FALSE,
                             &decl_state->param_list_cache);
    if (microsoft_mode) {
      /* The Microsoft compiler doesn't check default arguments until
         an instantiation is done. */
      def_arg_involves_template_param = TRUE;
    } else {
      rescan_copy_of_cache(&def_arg_cache);
      type_name(&default_arg_type);
      if (is_or_contains_template_param(default_arg_type)) {
        def_arg_involves_template_param = TRUE;
      }  /* if */
      /* Save the scanned value of the default argument.  This is saved even
         if we also decide to save the cache.  This value will be used if
         the default is needed, but the parameters on which it depends
         are still template dependent. */
      template_param->default_arg.type = default_arg_type;
    }  /* if */
    template_param->has_default_arg = TRUE;
    /* Update the default argument information in the template parameter. */
    if (def_arg_involves_template_param) {
      /* The default argument involves a template parameter.  This means that
         the default needs to be rescanned for each instantiation, so the
         default is saved as a token cache. */
      template_param->def_arg_involves_template_param = TRUE;
      set_template_cache_info(&template_param->default_arg_cache,
                              &def_arg_cache, decl_state->decl_info);
    } else {
      /* Discard the default argument token cache if it is not needed for
         later use. */
      discard_token_cache(&def_arg_cache);
    }  /* if */
  }  /* if */
  return template_param;
}  /* scan_type_template_param */


static a_template_param_ptr scan_nontype_template_param(
		a_tmpl_decl_state_ptr		decl_state,
		a_template_param_list_pos	template_param_list_pos,
		a_token_cache			*param_cache,
		a_boolean			*param_cache_used,
		a_boolean			is_template_param)
/*
Scan the declaration of a nontype template parameter.  Return the template
parameter entry for the parameter.  param_cache is the cache containing
the template parameter declaration.  param_cache_used is set to TRUE if
a that cache has been saved for rescanning when the type of the nontype
parameter depends on a template parameter.  is_template_param is TRUE if
this is the template parameter list of a template template parameter.
*/
{
  a_type_ptr		param_type_ptr;
  a_symbol_locator	param_locator;
  a_constant_ptr	param_con;
  a_boolean		is_unnamed;
  a_template_param_ptr	template_param;
  a_symbol_ptr         	sym;
  a_boolean		const_type_involves_template_param = FALSE;
  a_constant_ptr	default_arg_constant = NULL;
  a_boolean		def_arg_involves_template_param = FALSE;

  /* Scan the declaration of the type of the nontype parameter. */
  scan_a_template_parameter_declaration(&param_locator, &param_type_ptr,
                                        &is_unnamed,
                                        &const_type_involves_template_param);
  /* Create a symbol and bind a template param constant to it. At each
      point of instantiation an actual constant will be substituted. */
  sym = create_template_param_symbol((a_symbol_kind)sk_constant,
                                     &param_locator, is_unnamed);
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
  if (is_unnamed) {
    /* Reset the name in the source correspondence entry.  An unnamed
       type is represented by NULL, not "<unnamed>" as indicated by the
       symbol header. */
    param_con->source_corresp.name = NULL;
  }  /* if */
  /* Allocate a template parameter and set its fields based on sym. */
  template_param = alloc_template_param(sym);
  if (const_type_involves_template_param) {
    /* For nontype parameters, the type of the parameter needs
       to be saved as a token cache if the type uses template
       parameters. */
    template_param->variant.constant.type_involves_template_param = TRUE;
    set_template_cache_info(&template_param->cache, param_cache,
                            decl_state->decl_info);
    *param_cache_used = TRUE;
    if (is_template_param) {
      error(ec_dependent_type_in_templ_templ_param);
    }  /* if */
  }  /* if */
  if (curr_token == tok_assign) {
    /* Scan the default value. */
    a_token_cache  def_arg_cache;
    template_param->has_default_arg = TRUE;
    /* Skip past the equals sign. */
    (void)get_token();
    /* Cache the tokens that make up the default argument expression. */
    prescan_default_arg_expr(&def_arg_cache, /*is_template_param=*/TRUE,
                             /*is_function_template=*/FALSE,
			     /*is_friend_decl=*/FALSE,
			     &decl_state->param_list_cache);
    if (const_type_involves_template_param) {
      /* The type of the constant parameter involves a template parameter.
         When the type of the constant involves a template parameter we have
         to save the constant as a token cache, so we also set the flag that
         indicates that the default argument contains a template parameter. */
     def_arg_involves_template_param = TRUE;
    }  /* if */
    if (!const_type_involves_template_param ||
        nonclass_prototype_instantiations) {
      /* Scan the default argument expression.  Rescan a copy of the cache.
         This is done so that when the default argument is scanned, the
         last token of the cache is followed by the token that followed
         it in the original source program with no intervening
         tok_end_of_source.  Note that this is also done for defaults whose
         type is not template dependent.  This is done because, prior to
         nonclass prototype instantiations, such default arguments were
         scanned in all cases. */
      rescan_copy_of_cache(&def_arg_cache);
      default_arg_constant = fs_constant((a_constant_repr_kind)ck_error);
      scan_template_argument_constant_expression(param_type_ptr,
  					         default_arg_constant);
      if (default_arg_constant->kind ==
                                     (a_constant_repr_kind)ck_template_param) {
        def_arg_involves_template_param = TRUE;
      } else {
        /* Make sure the constant does not use a local or nonexternal
           variable, etc. */
        if (constant_references_non_external_entity(default_arg_constant)) {
          error(ec_nonexternal_entity_in_template_arg);
          set_error_constant(default_arg_constant);
        }  /* if */
      }  /* if */
      /* Save the scanned value of the default argument.  This is saved even
         if we also decide to save the cache.  This value will be used if
         the default is needed, but the parameters on which it depends
         are still template dependent. */
      template_param->default_arg.constant = default_arg_constant;
    }  /* if */
    /* Update the default argument information in the template parameter. */
    if (def_arg_involves_template_param) {
      /* The default argument involves a template parameter.  This means that
         the default needs to be rescanned for each instantiation, so the
         default is saved as a token cache. */
      template_param->def_arg_involves_template_param = TRUE;
      set_template_cache_info(&template_param->default_arg_cache,
                              &def_arg_cache, decl_state->decl_info);
    } else {
      /* Discard the default argument token cache. */
      discard_token_cache(&def_arg_cache);
    }  /* if */
  }  /* if */
  return template_param;
}  /* scan_nontype_template_param */


static void set_decl_state_for_template_param(
				a_tmpl_decl_state_ptr	curr_state,
				a_tmpl_decl_state_ptr	new_state)
/*
Create a new template declaration state for scanning a template template
parameter based on the current state.
*/
{
  init_templ_decl_state(new_state);
  new_state->in_prototype_instantiation =
                                        curr_state->in_prototype_instantiation;
  new_state->nesting_depth = 0;
  new_state->decl_info = curr_state->decl_info;
  new_state->orig_decl_level = curr_state->orig_decl_level;
  new_state->effective_decl_level = curr_state->effective_decl_level;
  new_state->enclosing_scope = curr_state->enclosing_scope;
  new_state->param_list_cache = curr_state->param_list_cache;
}  /* set_decl_state_for_template_param */


static a_template_param_ptr scan_template_template_param(
		a_tmpl_decl_state_ptr		parent_decl_state,
		a_template_param_list_pos	template_param_list_pos)
/*
Scan the declaration of a template template parameter.  Return the template
parameter entry for the parameter.
*/
{
  a_template_param_ptr			template_param;
  a_symbol_ptr				sym;
  a_template_ptr			templ_ptr;
  a_template_symbol_supplement_ptr	tssp;
  a_boolean				is_named;
  a_tmpl_decl_state			local_decl_state;

  /* Create a new set of declaration state information to be used while
     scanning the template template parameter. */
  set_decl_state_for_template_param(parent_decl_state, &local_decl_state);
  scan_template_param_clauses(&local_decl_state, /*is_template_param=*/TRUE);
  /* Pop all of the template declaration scopes that were pushed earlier. */
  for (; local_decl_state.number_of_template_decl_scopes != 0;
         local_decl_state.number_of_template_decl_scopes--) {
    pop_scope();
  }  /* for */
  if (local_decl_state.decl_info == NULL) {
    /* An error must have occurred while scanning the template parameter list.
       Create a template declaration information structure for error
       recovery purposes. */
    a_template_decl_info_ptr	    template_decl_info;
    template_decl_info = alloc_template_decl_info();
    local_decl_state.decl_info = template_decl_info;
    template_decl_info->enclosing_scope = local_decl_state.enclosing_scope;
  }  /* if */
  /* The current keyword must be "class" followed by an optional identifier.
     If it is "struct", give an error, but treat it like "class". */
  if (curr_token != tok_class && curr_token != tok_struct) {
    error(ec_exp_class);
  } else {
     if (curr_token == tok_struct) {
       error(ec_struct_not_allowed);
     }  /* if */
     /* Bypass the "class" or "struct" keyword. */
     (void)get_token();
  }  /* if */
  is_named = curr_token == tok_identifier;
  /* Create a class template symbol for this template template parameter. */
  sym = create_template_param_symbol((a_symbol_kind)sk_class_template,
                                     &locator_for_curr_id,
                                     !is_named);
  /* See if the parameter being declared has the same name as one of its
     template parameters. */
  if (is_named) {
    (void)(same_name_as_template_param(local_decl_state.decl_info,
                                       &locator_for_curr_id,
                                       /*is_template_template_param=*/TRUE));
  }  /* if */
  /* Bypass the identifier. */
  if (is_named) (void)get_token();
  tssp = sym->variant.template_info;
  templ_ptr = alloc_template();
  set_source_corresp(&templ_ptr->source_corresp, sym);
  templ_ptr->kind = (a_template_kind)templk_template_template_param;
  if (prototype_instantiations_in_il) {
    /* Keep a record of the parameterization structure.  (Needed, e.g., in the
       C++-generating back end.) */
    templ_ptr->template_decl = local_decl_state.template_decl;
  }  /* if */
  if (!is_named) {
    /* Reset the name in the source correspondence entry.  An unnamed
       parameter is represented by NULL, not "<unnamed>" as indicated
       by the symbol header. */
    templ_ptr->source_corresp.name = NULL;
  }  /* if */
  /* The templates associated with template parameters and nonreal classes
     have a template_info pointer that points back to the front end
     information. */
  templ_ptr->template_info = tssp;
  tssp->variant.class_template.template_template_param = TRUE;
  tssp->variant.class_template.type_kind = (a_type_kind)tk_class;
  templ_ptr->coordinates.depth = parent_decl_state->nesting_depth;
  templ_ptr->coordinates.position = template_param_list_pos;
  tssp->il_template_entry = templ_ptr;
  tssp->variant.class_template.argument_template = sym;
  set_template_cache_info(&tssp->cache,
                          (a_token_cache_ptr)NULL,
                          local_decl_state.decl_info);
  /* Allocate a template parameter and set its fields based on sym. */
  template_param = alloc_template_param(sym);
  /* Check the default arguments of the parameter list of the template
     template parameter. */
  check_template_param_default_args(local_decl_state.decl_info->parameters,
                                    /*is_partial_specialization=*/FALSE);
  if (curr_token == tok_assign) {
    a_token_cache			def_arg_cache;
    a_template_ptr			def_arg_templ;
    a_template_symbol_supplement_ptr	def_arg_tssp;
    /* Scan the default value for a type argument. */
    template_param->has_default_arg = TRUE;
    /* Skip past the equals sign. */
    (void)get_token();
    /* Cache the tokens that make up the default argument expression. */
    prescan_default_arg_expr(&def_arg_cache, /*is_template_param=*/TRUE,
                             /*is_function_template=*/FALSE,
			     /*is_friend_decl=*/FALSE,
                             &parent_decl_state->param_list_cache);
    rescan_copy_of_cache(&def_arg_cache);
    def_arg_templ = scan_template_template_argument(templ_ptr,
                                                    &pos_curr_token);
    def_arg_tssp = template_supplement_for_template(def_arg_templ);
    /* Save the scanned value of the default argument.  This is saved even
       if we also decide to save the cache.  This value will be used if
       the default is needed, but the parameters on which it depends
       are still template dependent. */
    template_param->default_arg.templ = def_arg_templ;
    /* Update the default argument information in the template parameter. */
    if (def_arg_tssp->is_nonreal_member ||
        def_arg_tssp->variant.class_template.template_template_param) {
      /* If the template that is returned is marked as a nonreal member
         or a template template parameter, the qualifier must depend on a
         template parameter.  This means that the default needs to be
         rescanned for each instantiation, so the default is saved as a
         token cache. */
      template_param->def_arg_involves_template_param = TRUE;
      set_template_cache_info(&template_param->default_arg_cache,
                              &def_arg_cache, parent_decl_state->decl_info);
    } else {
      /* Discard the default argument token cache if it is not needed for
         later use. */
      discard_token_cache(&def_arg_cache);
    }  /* if */
  }  /* if */
  return template_param;
}  /* scan_template_template_param */


static void scan_template_param_list(a_tmpl_decl_state_ptr decl_state,
				     a_boolean		   is_template_param)
/*
Scan a comma-separated list of template parameters.  The opening "<" will
already have been scanned, and an empty list will have already been
checked for.  The current token, consequently, is the first token of the
first parameter.  Return a pointer to the linked list that is created
to represent the template parameters.  is_template_param is TRUE if
this is the template parameter list of a template template parameter.
*/
{
  a_template_param_ptr 		template_param;
  a_template_param_ptr 		template_param_list = NULL;
  a_template_param_ptr 		end_of_template_param_list = NULL;
  a_token_cache        		param_cache;
  a_boolean			param_cache_used = FALSE;
  a_template_param_list_pos	template_param_list_pos = 0;

  db_enter(3, "scan_template_param_list");
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_gt);
  /* Loop through the comma-separated list of template parameter
     declarations. */
  do {
    a_symbol_kind  param_kind;
    /* If we've unexpectedly reached the end of the template parameter list,
       issue an error. */
    if (curr_token == tok_gt || curr_token == tok_end_of_source) {
      error(ec_missing_template_param);
      break;
    }  /* if */
    ++template_param_list_pos;
    /* Cache the tokens that comprise the template parameter declaration.
       If the parameter depends on other template parameters this cache
       will be saved and rescanned to scan template argument lists. */
    prescan_template_param_decl(&param_cache, decl_state);
    add_stop_token(tok_comma);
    /* Determine the kind of template parameter to be scanned. */
    param_kind = determine_template_param_kind();
    if (param_kind == (a_symbol_kind)sk_type) {
      /* A type template parameter. */
      template_param = scan_type_template_param(decl_state,
                                                template_param_list_pos);
    } else if (param_kind == (a_symbol_kind)sk_constant) {
      template_param = scan_nontype_template_param(
                             decl_state, template_param_list_pos, &param_cache,
                             &param_cache_used, is_template_param);
    } else {
      /* A template template parameter. */
      template_param = scan_template_template_param(decl_state,
                                                    template_param_list_pos);
    }  /* if */
    /* Add the template param to the end of the list. */
    if (template_param_list == NULL) {
      template_param_list = template_param;
      /* Update the parameters list of the template_decl_info for this
         declaration scope. */
      decl_state->decl_info->parameters = template_param_list;
    } else {
      end_of_template_param_list->next = template_param;
    }  /* if */
    /* Discard the parameter token cache if it is not needed for later use. */
    if (!param_cache_used) discard_token_cache(&param_cache);
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position           saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean				type_involves_template_param;
  a_boolean				constant_involves_template_param;
  static unsigned int			pending_instantiations = 0;
  a_boolean				dependent_arg_list;
  a_push_scope_options_set		ps_options = PS_NO_OPTIONS;

  type_involves_template_param =
               param_ptr->variant.constant.type_involves_template_param;
  constant_involves_template_param =
            param_ptr->def_arg_involves_template_param;
  saved_pos_curr_token = pos_curr_token;
  saved_error_position = error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  saved_curr_construct_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  dependent_arg_list = is_template_dependent_context() ||
                       template_arg_list_involves_template_param(arg_list);
  /* If the argument list is dependent, flag this as a nonreal
     instantiation. */
  if (dependent_arg_list) ps_options |= PS_NONREAL_INSTANTIATION;
  if (type_involves_template_param) {
    if (pending_instantiations == max_pending_instantiations) {
      error(ec_recursive_inst_of_templ_default_arg);
      constant_type = error_type();
    } else {
      /* Increment the count of pending default argument instantiations.
         This is used to detect infinite recursion. */
      ++pending_instantiations;
      /* Push the template instantiation scope.  Note that the instance symbol
         passed to push_scope is NULL because we don't yet know which instance
         is being instantiated.  Also note that a class type is not being
         passed for the same reason. */
      push_template_instantiation_scope(param_ptr->cache.decl_info,
  				        (a_type_ptr)NULL,
				        (a_routine_ptr)NULL,
				        (a_symbol_ptr)NULL,
				        template_sym, arg_list,
                                        /*push_stop_tokens=*/TRUE,
				        ps_options);
      /* Rescan the tokens of the function declaration. */
      rescan_reusable_cache(&param_ptr->cache.tokens);
      /* Scan the declaration specifiers. */
      scan_a_template_parameter_declaration(&param_locator, &constant_type,
                                            (a_boolean*)NULL,
                                            (a_boolean*)NULL);
      /* Skip past any tokens remaining in the cache.  Extra tokens will
         be present under certain error conditions and when a default argument
         has been supplied. */
      flush_past_token_cache_terminator();
      /* Pop the template instantiation scope. */
      pop_template_instantiation_scope();
      --pending_instantiations;
    }  /* if */
  } else {
    constant_type = param_sym->variant.constant->type;
  }  /* if */
  if (do_default_arg) {
    /* This parameter has a default argument whose value is to be used. */
    /* Determine whether the template argument list depends on a template
       parameter type.  */
    if (constant_involves_template_param) {
      if (pending_instantiations == max_pending_instantiations) {
        error(ec_recursive_inst_of_templ_default_arg);
        *constant = alloc_error_constant();
      } else {
        a_template_cache_ptr		tcp;
        a_source_position		arg_pos;
        /* Increment the count of pending default argument instantiations.
           This is used to detect infinite recursion. */
        ++pending_instantiations;
        /* Push the template instantiation scope.  See note above regarding
           the instance symbol and class type. */
        tcp = &param_ptr->default_arg_cache;
        push_template_instantiation_scope(tcp->decl_info,
					  (a_type_ptr)NULL,
				  	  (a_routine_ptr)NULL,
				 	  (a_symbol_ptr)NULL,
					  template_sym, arg_list,
                                          /*push_stop_tokens=*/TRUE,
					  ps_options);
        rescan_reusable_cache(&tcp->tokens);
        arg_pos = pos_curr_token;
        *constant = fs_constant((a_constant_repr_kind)ck_error);
        delayed_scan_of_template_default_arg_expr(constant_type, *constant);
        /* Make sure the constant does not use a local or nonexternal
           variable, etc. */
        if (constant_references_non_external_entity(*constant)) {
          pos_error(ec_nonexternal_entity_in_template_arg, &arg_pos);
          set_error_constant(*constant);
        }  /* if */
        /* Pop the template instantiation scope. */
        pop_template_instantiation_scope();
        --pending_instantiations;
      }  /* if */
    } else {
      *constant = param_ptr->default_arg.constant;
    }  /* if */
  }  /* if */
  error_position = saved_error_position;
  pos_curr_token = saved_pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position           saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_type_ptr				tp;
  static unsigned int			pending_instantiations = 0;
  a_boolean				dependent_arg_list;

  /* Determine whether the template argument list depends on a template
     parameter type. */
  dependent_arg_list = is_template_dependent_context() ||
                       template_arg_list_involves_template_param(arg_list);
  if (param_ptr->def_arg_involves_template_param) {
    if (pending_instantiations == max_pending_instantiations) {
      error(ec_recursive_inst_of_templ_default_arg);
      tp = error_type();
    } else {
      a_template_cache_ptr	tcp;
      a_push_scope_options_set	ps_options = PS_NO_OPTIONS;
      /* Increment the count of pending default argument instantiations.
         This is used to detect infinite recursion. */
      ++pending_instantiations;
      /* If the argument list is dependent, flag this as a nonreal
         instantiation. */
      if (dependent_arg_list) ps_options |= PS_NONREAL_INSTANTIATION;
      /* Push the template instantiation scope.  Note that the instance symbol
         passed to push_scope is NULL because we don't yet know which instance
         is being instantiated.  Also note that a class type is not being
         passed for the same reason. */
      tcp = &param_ptr->default_arg_cache;
      push_template_instantiation_scope(tcp->decl_info,
                                        (a_type_ptr)NULL,
				        (a_routine_ptr)NULL,
				        (a_symbol_ptr)NULL,
				        template_sym, arg_list,
                                        /*push_stop_tokens=*/TRUE,
					ps_options);
      saved_pos_curr_token = pos_curr_token;
      saved_error_position = error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      saved_curr_construct_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      rescan_reusable_cache(&tcp->tokens);
      tp = delayed_scan_of_template_default_type_arg();
      error_position = saved_error_position;
      pos_curr_token = saved_pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Pop the template instantiation scope. */
      pop_template_instantiation_scope();
      --pending_instantiations;
    }  /* if */
  } else {
    tp = param_ptr->default_arg.type;
  }  /* if */
  return tp;
}  /* rescan_template_type_default_arg */


a_template_ptr rescan_template_template_default_arg
                                     (a_symbol_ptr	   template_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list)
/*
Rescan the tokens of a template template parameter default argument using
the current values of any previous parameters so that the default argument
is processed with the types with which the class is to be instantiated.
This is used to get the correct template for template template default
arguments that depend on other template parameters.  If the default depends
on a template parameter then the cache is rescanned, otherwise, the
existing type is simply used. 
*/
{
  a_source_position  			saved_pos_curr_token;
  a_source_position  			saved_error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position           saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_template_ptr			templ;
  a_boolean				dependent_arg_list;

  /* Determine whether the template argument list depends on a template
     parameter type. */
  dependent_arg_list = is_template_dependent_context() ||
                       template_arg_list_involves_template_param(arg_list);
  if (param_ptr->def_arg_involves_template_param) {
    /* Push the template instantiation scope.  Note that the instance symbol
       passed to push_scope is NULL because we don't yet know which instance
       is being instantiated.  Also note that a class type is not being
       passed for the same reason. */
    a_template_cache_ptr	tcp = &param_ptr->default_arg_cache;
    a_push_scope_options_set	ps_options = PS_NO_OPTIONS;
    /* If the argument list is dependent, flag this as a nonreal
       instantiation. */
    if (dependent_arg_list) ps_options |= PS_NONREAL_INSTANTIATION;
    push_template_instantiation_scope(tcp->decl_info,
                                      (a_type_ptr)NULL,
				      (a_routine_ptr)NULL,
				      (a_symbol_ptr)NULL,
				      template_sym, arg_list,
                                      /*push_stop_tokens=*/TRUE,
				      ps_options);
    saved_pos_curr_token = pos_curr_token;
    saved_error_position = error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    saved_curr_construct_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    rescan_reusable_cache(&tcp->tokens);
    templ = delayed_scan_of_template_default_template_arg(
                 param_ptr->variant.templ->il_template_entry, &pos_curr_token);
    error_position = saved_error_position;
    pos_curr_token = saved_pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Pop the template instantiation scope. */
    pop_template_instantiation_scope();
  } else {
    templ = param_ptr->default_arg.templ;
  }  /* if */
  return templ;
}  /* rescan_template_template_default_arg */


static a_boolean template_param_appears_in_param_list
				(a_symbol_ptr param_sym,
                                 a_type_ptr   rout_type)
/*
tparam_type is a tk_template_parameter type entry used in a template
declaration, and rout_type is a routine type.  Search each of the routine's
parameter types to see if tparam_type appears in it.  If
*/
{
  a_boolean         found = FALSE;
  a_param_type_ptr  ptp;

  ptp = rout_type->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    /* Inspect all template parameters, not just those that involve
       deduced template parameters.  A template parameter can affect the
       type even in a nondeduced location. */
    if (template_param_used_in_type(param_sym, ptp->type)) {
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
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
       instance_sym = next_instance_sym(instance_sym)) {
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
               fixup is made from scan_class_definition. */
            instantiate_template_class(instance_sym->
                                          variant.class_struct_union.type);
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* fixup_types_that_refer_to_incomplete_instantiations */


static void add_to_exported_templates_list(a_symbol_ptr	sym)
/*
sym is the symbol of a template function, member function of a class
template, or static data member of a class template that is an exported
template defined in the current translation unit.  Add it to the list
of exported templates for this translation unit.
*/
{
  a_symbol_list_entry_ptr	slep;

  slep = alloc_symbol_list_entry();
  slep->symbol = sym;
  /* Add it to the end of the list of exported templates. */
  if (exported_templates_list == NULL) exported_templates_list = slep;
  if (exported_templates_tail != NULL) {
    exported_templates_tail->next = slep;
  }  /* if */
  exported_templates_tail = slep;
}  /* add_to_exported_templates_list */


#if RECORD_TEMPLATE_STRINGS

static void select_caches_and_make_template_string(
				a_tmpl_decl_state_ptr	decl_state,
				a_symbol_ptr		sym,
                                a_token_cache		*p_template_body_cache)
/*
Create a template string for the current template.  Usually the token
caches used to build the token string are the ones from the declaration
just scanned, but for member template declarations of class templates and
friend declarations of class templates, the declaration cache information
is taken from the declaration scanned during the prototype instantiation
of the enclosing class template.  This is done because the default argument
information is removed from the token cache after the prototype instantiation
is done.  sym is the symbol of the template for which the template string
is being created.
*/
{
  a_token_cache				*decl_cache = NULL;

  /* Determine whether there is a corresponding declaration in a prototype
     instantiation that should be used. */
  if (sym->kind == (a_symbol_kind)sk_function_template) {
    a_template_symbol_supplement_ptr	proto_tssp = NULL;
    a_template_symbol_supplement_ptr	tssp;
    tssp = template_supplement_for_symbol(sym);
    if (tssp->prototype_template != NULL && !tssp->is_specific_definition) {
      /* A member template of a class template.  Get the template supplement
         for the prototype template. */
      proto_tssp = template_supplement_for_symbol(tssp->prototype_template);
    } else if (decl_state->class_declared_in != NULL &&
               tssp->variant.function.prototype_friend_symbol != NULL) {
      /* A friend of a class template.  Get the template supplement for the
         friend of the prototype instantiation. */
      a_symbol_ptr	friend_sym;
      friend_sym = tssp->variant.function.prototype_friend_symbol;
      proto_tssp = template_supplement_for_symbol(friend_sym);
    }  /* if */
    if (proto_tssp != NULL) {
      decl_cache = &proto_tssp->variant.function.decl_cache.tokens;
    }  /* if */
  }  /* if */
  if (decl_cache == NULL) decl_cache = &decl_state->decl_token_cache;
  /* Create a new template string from the tokens. */
  make_template_string(decl_state->il_template_entry,
                       &decl_state->param_list_cache,
                       decl_cache,
                       p_template_body_cache);
}  /* select_caches_and_make_template_string */

#endif /* RECORD_TEMPLATE_STRINGS */

#if !RECORD_TEMPLATE_STRINGS
/*ARGSUSED*/ /* <-- p_template_body_cache is not used when not recording
                template strings. */
#endif /* RECORD_TEMPLATE_STRINGS */
static
void complete_il_template_entry(a_tmpl_decl_state_ptr  decl_state,
                                a_symbol_ptr           sym,
                                a_token_cache          *p_template_body_cache)
/*
Finish up establishing the IL template entry.  (Its decl_position has been
set, and its source sequence entry, if any, has been put out.)
*/
{
  a_boolean       err = FALSE;
  a_template_ptr  il_template_entry = decl_state->il_template_entry;
  a_symbol_ptr                      proto_sym = NULL;
  a_template_symbol_supplement_ptr  tssp = NULL;

  if (il_template_entry != NULL) {
    if (sym != NULL) {
      /* Set the name linkage.  This may be updated below for a static
         function template. */
      il_template_entry->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
      /* Set the template kind. */
      switch (sym->kind) {
        case sk_class_template:
          il_template_entry->kind = (a_template_kind)templk_class;
          proto_sym = prototype_template_of(sym);
          tssp = template_supplement_for_symbol(proto_sym);
          if (prototype_instantiations_in_il) {
            proto_sym = tssp->variant.class_template.prototype_instantiation;
            il_template_entry->prototype_instantiation.type =
                                                  type_symbol_type(proto_sym);
          } else {
            il_template_entry->prototype_instantiation.type = NULL;
          }  /* if */
          if (tssp->is_nonreal_member) {
            /* The class referenced is a nonreal member.  Don't consider it
               tobe the canonical template. */
            il_template_entry->canonical_template = il_template_entry;
          } else {
            il_template_entry->canonical_template = tssp->il_template_entry;
            if (decl_state->defines_something &&
                tssp->il_template_entry->definition_template == NULL) {
              /* With nested class templates, multiple definitions may be seen
                 but only the first one is a true prototype instantiation. */
              tssp->il_template_entry->definition_template = il_template_entry;
            }  /* if */
          }  /* if */
          break;
        case sk_function_template:
          {
            a_routine_ptr	rout;
            il_template_entry->kind = (a_template_kind)templk_function;
            proto_sym = prototype_template_of(sym);
            tssp = template_supplement_for_symbol(proto_sym);
            rout = tssp->variant.function.routine;
            if (prototype_instantiations_in_il) {
              il_template_entry->prototype_instantiation.routine = rout;
            } else {
              il_template_entry->prototype_instantiation.routine = NULL;
            }  /* if */
            il_template_entry->canonical_template = tssp->il_template_entry;
            if (decl_state->defines_something) {
              tssp->il_template_entry->definition_template = il_template_entry;
            }  /* if */
            /* Function templates have C++ linkage unless they are static. */
            if (rout->storage_class == (a_storage_class)sc_static) {
              il_template_entry->source_corresp.name_linkage =
                                            (a_name_linkage_kind)nlk_internal;
            }  /* if */
          }
          break;
        case sk_member_function:
          il_template_entry->kind = (a_template_kind)templk_member_function;
          if (prototype_instantiations_in_il) {
            il_template_entry->prototype_instantiation.routine =
                                                     sym->variant.routine.ptr;
          } else {
            il_template_entry->prototype_instantiation.routine = NULL;
          }  /* if */
          il_template_entry->canonical_template =
                                     sym->variant.routine.ptr->assoc_template;
          if (decl_state->defines_something) {
            il_template_entry->canonical_template->definition_template =
                                                            il_template_entry;
          }  /* if */
          break;
        case sk_static_data_member:
          il_template_entry->kind = (a_template_kind)templk_static_data_member;
          if (prototype_instantiations_in_il) {
            il_template_entry->prototype_instantiation.variable =
                                     sym->variant.static_data_member.variable;
          } else {
            il_template_entry->prototype_instantiation.variable = NULL;
          }  /* if */
          /* An out-of-class static data member declaration is always a
             definition. */
          il_template_entry->canonical_template =
                     sym->variant.static_data_member.variable->assoc_template;
          il_template_entry->canonical_template->definition_template =
                                                            il_template_entry;
          break;
        case sk_class_or_struct_tag:
        case sk_union_tag:
          check_assertion(sym->is_class_member);
          il_template_entry->kind = (a_template_kind)templk_member_class;
          if (prototype_instantiations_in_il) {
            il_template_entry->prototype_instantiation.type =
                                                        type_symbol_type(sym);
          } else {
            il_template_entry->prototype_instantiation.type = NULL;
          }  /* if */
          il_template_entry->canonical_template =
                   sym->variant.class_struct_union.type
                      ->variant.class_struct_union.extra_info->assoc_template;
          if (decl_state->defines_something) {
            il_template_entry->canonical_template->definition_template =
                                                            il_template_entry;
          }  /* if */
          break;
        default:
          /* There must have been an error.  Do the check because we don't
             want an incomplete IL entry to be handed to the back end. */
          check_assertion(total_errors > 0);
          err = TRUE;
      }  /* switch */
      if (!err || sym->is_error) {
        /* Set parent information in the IL entry. */
        if (sym->is_class_member) {
          set_class_membership_for_template((a_symbol_ptr)NULL,
                                            il_template_entry,
                                            sym->parent.class_type);
        } else if (sym->parent.namespace_ptr != NULL) {
          set_namespace_membership((a_symbol_ptr)NULL,
                                   &il_template_entry->source_corresp,
                                   sym->parent.namespace_ptr);
        }  /* if */
        if (sym->is_class_member && decl_state->class_declared_in != NULL) {
          /* If this is the declaration of a member inside the class,
             record the access. */
          il_template_entry->source_corresp.access = decl_state->access;
        }  /* if */
#if RECORD_TEMPLATE_STRINGS
        if (p_template_body_cache != NULL) {
          a_cached_token_ptr	first_token;
          first_token = p_template_body_cache->first_token;
          /* Skip over any pragmas that precede the first token of the body. */
          while (first_token != NULL &&
                 first_token->extra_info_kind ==
                                       (a_token_extra_info_kind)teik_pragma) {
            first_token = first_token->next;
          }  /* while */
          if (first_token != NULL &&
              ((a_token_kind)first_token->token == tok_colon ||
               (a_token_kind)first_token->token == tok_try)) {
            /* There can sometimes be an overlap between the template
               declaration cache and the template body cache.  Such an
               overlap does not cause problems for the normal
               processing, but must be eliminated when template
               strings are created.  Split the declaration cache at
               the first token of the body cache and discard the
               duplicated tokens. */
            a_token_cache		dummy_cache;
            a_token_sequence_number	tsn_to_split;
            tsn_to_split = first_token->token_sequence_number;
            clear_token_cache(&dummy_cache, /*reusable=*/TRUE);
            split_token_cache(&decl_state->decl_token_cache, &dummy_cache,
                              tsn_to_split,
                              /*include_prev_token=*/FALSE,
                             /*okay_if_not_found=*/FALSE);
            discard_token_cache(&dummy_cache);
          } /* if */
        }  /* if */
	/* Create the string that represents the template declaration. */
        select_caches_and_make_template_string(decl_state, sym,
                                               p_template_body_cache);
#endif /* RECORD_TEMPLATE_STRINGS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        /* Record source range information for this template declaration. */
        il_template_entry->source_corresp.decl_pos_info =
                        make_decl_pos_supplement(/*at_file_scope=*/TRUE,
                                                 &decl_state->decl_pos_block);
        switch (sym->kind) {
          case sk_static_data_member:
            il_template_entry->definition_range =
                                   decl_state->decl_pos_block.var_init_range;
            break;
          case sk_function_template:
          case sk_member_function:
          case sk_class_template:
          case sk_class_or_struct_tag:
          case sk_union_tag:
            il_template_entry->definition_range = decl_state->definition_range;
            break;
          default:;
        }  /* switch */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Add the IL template entry to the templates list of the
           appropriate scope. */
        add_to_templates_list(il_template_entry,
                              decl_state->effective_decl_level);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* complete_il_template_entry */


static a_symbol_ptr template_static_data_member_declaration
                    (a_tmpl_decl_state_ptr            decl_state,
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
    err = TRUE;
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
    tssp = sym->variant.static_data_member.instance_ptr->template_info;
    /* Make sure the parameter list matches the class declaration. */
    if (!member_template_param_list_matches_class
                              (decl_state, sym, &error_position)) {
      err = TRUE;
    } else if ((is_ptr_or_ref_type(type) &&
                is_function_type(type_pointed_to(type))) ||
               (is_ptr_to_member_type(type) &&
                is_function_type(pm_member_type(type)))) {
      /* Check that any exception specifications match with those declared
         in the class. */
      check_exception_specification(type, sym, &locator->source_position,
                                    /*is_redecl=*/TRUE);
    }  /* if */
  }  /* if */
  /* Scan the initializer expression, if any, and cache its tokens.
     The initializer may be of the form "= ...;" or "(...);".
     Anything else will not get cached and an error will be generated
     on this declaration. */
  if (curr_token != tok_end_of_source &&
      (curr_token == tok_assign || has_parenthesized_initializer)) {
    a_token_sequence_number	split_location;
    a_token_set_array		stop_tokens;
    p_token_cache = &local_token_cache;

#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_state->decl_pos_block.var_init_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    clear_token_cache(p_token_cache, /*reusable=*/TRUE);
    /* The declaration token cache contains the declaration and the
       initializer.  Split the cache so that the initialization is
       removed from the declaration cache and placed in the initializer
       cache. */
    split_location = curr_token_sequence_number;
    split_token_cache(&decl_state->decl_token_cache,
                      p_token_cache, split_location,
                      /*include_prev_token=*/has_parenthesized_initializer,
                      /*okay_if_not_found=*/FALSE);
    /* Skip over the tokens that are already part of the token cache. */
    clear_token_set_array(stop_tokens);
    incr_token_set_array_element(stop_tokens, tok_lbrace);
    incr_token_set_array_element(stop_tokens, tok_colon);
    incr_token_set_array_element(stop_tokens, tok_semicolon);
    /* The normal flush_tokens_with_stop_tokens sometimes issues a warning
       based on the number of tokens skipped.  This should not be done
       in this case because the flush is not being done for error recovery. */
    flush_tokens_with_stop_tokens_and_warning_flag(stop_tokens,
						   /*suppress_warning=*/TRUE);
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* To find the end position, scan the cache to find the token preceding
       the semicolon (or the end-of-source, if a semicolon was omitted). */
    { a_cached_token_ptr  ctp = p_token_cache->first_token;
      a_token_kind        next_tok;

      for (;;) {
        check_assertion(ctp->next != NULL);
        next_tok = (a_token_kind)ctp->next->token;
        if (next_tok == tok_semicolon || next_tok == tok_end_of_source) {
          decl_state->decl_pos_block.var_init_range.end =
                                                ctp->end_source_position;
          break;
        }  /* if */
        ctp = ctp->next;
      }  /* for */
    }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (err) {
      discard_token_cache(p_token_cache);
      p_token_cache = NULL;
    } /* if */
  } /* if */
  if (err) {
    /* If an error occurred earlier, return a NULL symbol. */
    sym = NULL;
    tssp = NULL;
  } /* if */
  if (tssp != NULL) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Prevent the generation of a source sequence entry for the a_template
       entry since we already did so elsewhere. */
    a_boolean  saved_sses_disallowed = source_sequence_entries_disallowed;
    source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Save the information needed to create an instantiation based
       on the definition of the template. */
    set_template_cache_info(&tssp->cache, p_token_cache,
                            decl_state->decl_info);
    mark_defined(sym, &locator->source_position);
    check_assertion(tssp->il_template_entry != NULL);
    if (decl_state->export_present) {
      tssp->il_template_entry->is_exported = TRUE;
      add_to_exported_templates_list(sym);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Restore the previous state wrt. the generation of source sequence
       entries. */
    source_sequence_entries_disallowed = saved_sses_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  *p_tssp = tssp;
  db_exit();
  return sym;
}  /* template_static_data_member_declaration */


static void check_function_template_param_usage
                         (a_symbol_ptr                     sym,
			  a_type_ptr                       type,
			  a_template_param_ptr             template_param_list)
/*
Perform certain error tests on a function template parameter list.
When not using distinct template name mangling, all of the template
parameters must be used as part of the signature of the functions that
will be generated from this template.  Function templates are not
permitted to have default template argument values.  This test is also
done here.
*/
{
  a_template_param_ptr  tpp;
  a_type_ptr	        rout_type = skip_typerefs(type);
  a_boolean		is_conversion_operator;
  a_boolean		is_constructor;
  an_error_severity	severity;

  is_conversion_operator = is_conversion_function_symbol(sym);
  is_constructor = is_constructor_symbol(sym);
  if (distinct_template_signatures) {
    /* When distinct template signatures are used there is no requirement
       for template parameters to be used in the function signature.  We
       still perform the test to set template_param_not_in_function_type
       appropriately.  This is used in determining how to display diagnostics
       involving the template.  A remark is issued because, in most cases,
       this will indicate the use of the wrong name in the function template
       declaration.  A warning is issued for constructors and conversion
       operators because there is no way to call such functions (because
       explicit argument lists can't be supplied). */
    if (is_conversion_operator || is_constructor) {
      severity = es_warning;
    } else {
      severity = es_remark;
    }  /* if */
  } else {
    /* All templates must use their template parameters in the function
       signature when old template mangling is used.  Otherwise, duplicate
       mangled named would be generated. */
    severity = es_error;
  }  /* if */
  /* Go through the loop anyway, to check for function template parameters
     with default arguments. */
  for (tpp = template_param_list; tpp != NULL; tpp = tpp->next) {
    a_symbol_ptr param_sym = tpp->param_symbol;
    a_boolean	 param_used;
    if (tpp->has_default_arg) {
      pos_diagnostic(microsoft_mode ? es_warning : es_error,
                     ec_default_template_arg_not_allowed,
                     &param_sym->decl_position);
    }  /* if */
    if (is_conversion_operator) {
      /* For conversion operator functions, the template parameters must be
         used in the return type. */
      param_used = template_param_used_in_type(
                          param_sym, rout_type->variant.routine.return_type);
    } else {
      /* Determine whether all template parameters are used by
         function parameter types. */
      param_used = template_param_appears_in_param_list(param_sym,
                                                        rout_type);
    }  /* if */
    if (!param_used) {
      a_template_symbol_supplement_ptr	tssp;
      tssp = template_supplement_for_symbol(sym);
      tssp->variant.function.template_param_not_in_function_type = TRUE;
      pos_sy2_diagnostic(severity, ec_not_used_in_template_function_params,
                         &param_sym->decl_position, param_sym, sym);
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


static a_symbol_ptr find_friend_info_from_prototype(
			a_symbol_ptr			proto_sym)
/*
Get information about this friend declaration that was saved during
the prototype information of the class.  This is only used for friends
declared within class templates.  Return a pointer to the function
template symbol from the prototype instantiation.
*/
{
  a_templ_friend_info_ptr		tfip;
  a_template_symbol_supplement_ptr	tssp;

  tssp = template_supplement_for_symbol(proto_sym);
  /* Find the default argument entry that corresponds to the current
     token sequence number. */
  for (tfip = tssp->variant.class_template.friend_info;
       tfip != NULL; tfip = tfip->next) {
    if (tfip->token_number == curr_token_sequence_number) break;
  }  /* for */
  return tfip != NULL ? tfip->symbol : NULL;
}  /* find_friend_info_from_prototype */


static void set_friend_info_for_prototype(
			a_symbol_ptr			proto_sym,
			a_symbol_ptr			friend_sym)
/*
Save information about this friend declaration in the enclosing class
template.  proto_sym is the symbol for the enclosing class template.
friend_sym is the symbol for the friend declaration.
*/
{
  a_template_symbol_supplement_ptr	tssp;
  a_templ_friend_info_ptr		tfip;

  tssp = template_supplement_for_symbol(proto_sym);
  tfip = alloc_templ_friend_info();
  tfip->symbol = friend_sym;
  tfip->token_number = curr_token_sequence_number;
  /* Add this entry to the front of a list of default argument entries
     associated with enclosing class template. */
  tfip->next = tssp->variant.class_template.friend_info;
  tssp->variant.class_template.friend_info = tfip;
}  /* set_friend_info_for_prototype */


static void set_or_find_prototype_friend_info(
			a_tmpl_decl_state_ptr			decl_state,
			a_symbol_ptr				sym,
			a_template_symbol_supplement_ptr	tssp)
/*
When a function template is declared as a friend of a class template,
certain information is saved during the prototype instantiation and
a pointer back to the prototype information is established during
a real instantiation.
*/
{
  a_type_ptr			encl_class;
  a_symbol_ptr			encl_class_sym;
  a_symbol_ptr			proto_sym;

  encl_class = decl_state->class_declared_in;
  encl_class_sym = (a_symbol_ptr)encl_class->source_corresp.assoc_info;
  if (is_prototype_instantiation_symbol(encl_class_sym)) {
    proto_sym = encl_class_sym;
    set_friend_info_for_prototype(proto_sym, sym);
  } else {
    proto_sym = corresp_prototype_for_class_symbol(encl_class_sym);
    if (proto_sym == NULL) {
      /* Not a template-based class. */
    } else {
      /* Find the symbol of the corresponding friend declaration from the
         prototype instantiation. */
      a_symbol_ptr			proto_friend_sym;
      a_template_symbol_supplement_ptr	proto_friend_tssp;
      proto_friend_sym = find_friend_info_from_prototype(proto_sym);
      if (proto_friend_sym != NULL) {
        tssp->variant.function.prototype_friend_symbol = proto_friend_sym;
        /* Set the declaration sequence number of this declaration based on
           the value from the prototype friend declaration. */
        proto_friend_tssp = template_supplement_for_symbol(proto_friend_sym);
        decl_state->decl_info->decl_seq =
            proto_friend_tssp->variant.function.decl_cache.decl_info->decl_seq;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_or_find_prototype_friend_info */


static a_def_arg_expr_fixup_ptr copy_def_args_and_update_decl_info(
				a_tmpl_decl_state_ptr		decl_state,
				a_def_arg_expr_fixup_ptr	orig_list)
/*
Make a copy of the default argument list specified by orig_list.  Update
the copied entries to refer to the declaration information indicated in
decl_state.
*/
{
  a_def_arg_expr_fixup_ptr	new_list;
  a_def_arg_expr_fixup_ptr	daefp;

  /* Make a copy of the list. */
  new_list = copy_def_arg_expr_fixup_list(orig_list);
  /* Go through the list and update the declaration information. */
  for (daefp = new_list; daefp != NULL; daefp = daefp->next) {
    daefp->cache.decl_info = decl_state->decl_info;
  }  /* for */
  return new_list;
}  /* copy_def_args_and_update_decl_info */


static void update_function_template_default_args(
			a_tmpl_decl_state_ptr			decl_state,
			a_symbol_ptr				template_sym,
			a_template_symbol_supplement_ptr	tssp)
/*
sym is a function template that is currently being declared.  Update
the default_arg_expr_list based on the setting of curr_default_args.
If this is a friend template declared in a class template, get the
default argument information that was saved during the prototype
instantiation.
*/
{
  a_def_arg_expr_fixup_ptr	daefp;
  a_symbol_ptr			proto_sym;

  /* Update the template declaration information to refer to
     the declaration information of the function template. */
  daefp = curr_default_args;
  for (daefp = curr_default_args; daefp != NULL; daefp = daefp->next) {
    daefp->cache.decl_info = decl_state->decl_info;
  }  /* for */
  proto_sym = tssp->variant.function.prototype_friend_symbol;
  if (proto_sym != NULL) {
    /* Use the default argument information from the friend declaration from
       the prototype instantiation. */
    a_template_symbol_supplement_ptr	proto_tssp;
    proto_tssp = template_supplement_for_symbol(proto_sym);
    /* Free the existing original set of default arguments. */
    free_def_arg_expr_fixup(curr_default_args);
    /* Make a copy of the default argument list for the template.  Update the
       declaration information to reflect the current instantiation of the
       class. */
    curr_default_args = copy_def_args_and_update_decl_info(
                               decl_state,
                               proto_tssp->variant.function.def_arg_expr_list);
  }  /* if */
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
  if (proto_sym == NULL) {
    /* We are using the newly specified default arguments.  Do a prototype
       instantiation of the new defaults.  For declarations within classes
       this is done in class fixup processing. */
    if (nonclass_prototype_instantiations) {
      if (decl_state->class_declared_in == NULL) {
        /* Record the declaration sequence number for the default argument.
           This is done here because the value for the containing declaration
           has not been set yet. */
        decl_state->decl_info->decl_seq = ++decl_seq_counter;
        default_arg_prototype_instantiation(
                                          template_sym, curr_default_args,
                                          decl_state->prototype_scope_symbols,
                                          /*update_declared_type=*/TRUE);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* update_function_template_default_args */


static void update_export_flag_for_function(
			a_tmpl_decl_state_ptr			decl_state,
			a_routine_ptr				rout_ptr,
			a_symbol_ptr				sym,
			a_template_symbol_supplement_ptr	tssp)
/*
sym is the symbol for a function template or a member function of a class
template, tssp is its template symbol supplement.  Its is_exported flag
may or may not have been set by a previous declaration.  Update it to
reflect an export keyword present on the current declaration.
*/
{
  a_boolean	is_defined;

  is_defined = tssp->cache.tokens.first_token != NULL;
  if (rout_ptr->is_inline) {
    /* An inline function cannot be exported.  Clear the flag if it was
       set earlier. */
    tssp->il_template_entry->is_exported = FALSE;
  } else if (decl_state->export_present) {
    /* Export was specified on this declaration.  Set the flag. */
    if (!tssp->il_template_entry->is_exported &&
        (is_defined && !decl_state->defines_something)) {
      /* The export keyword appeared on a declaration after the definition.
         This is not allowed. */
      pos_error(ec_export_after_definition, &decl_state->export_position);
    }  /* if */
    tssp->il_template_entry->is_exported = TRUE;
  }  /* if */
  if (tssp->il_template_entry->is_exported && is_defined) {
    /* Add the template to the list of exported templates. */
    add_to_exported_templates_list(sym);
  }  /* if */
}  /* update_export_flag_for_function */


static void complete_function_template_decl(
                     a_tmpl_decl_state_ptr	      decl_state,
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
  a_routine_ptr			   rout_ptr = NULL;

  if (!err && !is_function_or_template_symbol(sym)) {
    /* The symbol is something other than a function symbol.  Issue
       an error and set the symbol to NULL.  This error test only applies
       to class member templates. */
    pos_sy_error(ec_bad_member_template_decl, decl_pos, sym);
    err = TRUE;
    sym = NULL;
  }  /* if */
  /* If some kind of error has occurred, set the decl_scope_err flag
     to suppress subsequent errors. */
  if (err) decl_state->decl_scope_err = TRUE;
  if (sym != NULL) {
    tssp = template_supplement_for_symbol(sym);
    rout_ptr = tssp->variant.function.routine;
  }  /* if */
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_function_template) {
    if (sym->is_class_member && !decl_state->is_template_friend) {
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
          find_function_template_member(decl_state, sym);
        }  /* if */
      }  /* if */
    } else if (decl_state->is_template_friend &&
               decl_state->class_declared_in != NULL) {
      /* Information about friend declarations is saved during the prototype
         instantiation of a class and reused during real instantiations. */
      set_or_find_prototype_friend_info(decl_state, sym, tssp);
    }  /* if */
  }  /* if */
  /* Make sure that the template parameter list is compatible with
     any previous declaration (i.e., the declaration of the class
     if this is a member function. */
  if (!err && sym->is_class_member &&
      !decl_state->in_prototype_instantiation &&
      (decl_state->class_declared_in == NULL ||
       decl_state->is_template_friend)) {
    if (!member_template_param_list_matches_class(decl_state,
                                                  sym, decl_pos)) {
      err = TRUE;
    } /* if */
  } /* if */
  if (err) {
    a_token_cache  local_token_cache;
    clear_token_cache(&local_token_cache, /*reusable=*/FALSE);
    cache_function_template_body(decl_state, &local_token_cache,
                                 /*is_ctor=*/TRUE, decl_pos);
    discard_token_cache(&local_token_cache);
    decl_state->decl_scope_err = TRUE;
  } else {
    a_token_cache               local_token_cache;
    a_token_sequence_number     first_token_number;
    a_token_sequence_number     last_token_number;

    clear_token_cache(&local_token_cache, /*reusable=*/TRUE);
    first_token_number = curr_token_sequence_number;
    cache_function_template_body(decl_state, &local_token_cache,
                                 is_constructor_symbol(sym), decl_pos);
    last_token_number = curr_token_sequence_number;
    if (decl_state->in_prototype_instantiation) {
      if (sym->is_class_member && decl_state->class_declared_in != NULL &&
          decl_state->defines_something &&
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
      /* Set the assoc_template field of the prototype instantiation routine
         entry. */
      tssp->variant.function.routine->assoc_template = tssp->il_template_entry;
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
      /* Record the source position of the definition in case it is
         different from the declaration. */
      tssp->variant.function.routine->source_corresp.decl_position = *decl_pos;
      /* Save the token cache and associated template declaration
         information.  This is done for the initial declaration and
         is done again if the function is defined later. */
      set_template_cache_info(&tssp->cache,
                              &local_token_cache,
                              decl_state->decl_info);
    } /* if */
    if (decl_state->class_declared_in != NULL &&
        nonclass_prototype_instantiations) {
      /* Create a routine fixup entry so that the body of this template
         (if present) and any default arguments will have their prototype
         instantiations done at the completion of the prototype instantiation
         of the enclosing class. */
      add_routine_fixup_for_template_decl(sym,
                                          decl_state->prototype_scope_symbols,
                                          decl_state->class_declared_in,
					  curr_default_args);
    }  /* if */
    /* Update the default argument information for this template from
       either curr_default_args or from the corresponding declaration
       from the prototype instantiation of the enclosing class. */
    update_function_template_default_args(decl_state, sym, tssp);
    if (decl_state->is_template_friend &&
        !decl_state->in_prototype_instantiation) {
      /* This is a template friend declaration, add the current class to
         the list of friend classes associated with this template. */
      add_befriending_class_to_function_template(
                                          tssp, decl_state->class_declared_in);
    }  /* if */
  } /* if */
  if (sym != NULL) {
    /* Save the IL template entry pointer for this symbol. */
    set_il_template_entry(decl_state, sym, tssp);
    /* Update the exported flag, if necessary. */
    update_export_flag_for_function(decl_state, rout_ptr, sym, tssp);
  }  /* if */
  if (decl_state->defines_something) {
    /* A function template definition -- leave it to the caller to advance
       past the closing right brace. */
    *(decl_state->final_token_ptr) = tok_rbrace;
#if USER_CONTROL_OF_STRUCT_PACKING
    if (!err) {
      /* Record the current setting of the maximum alignment for local class
         members (an adjustment may be required for packing). */
      tssp->variant.function.func_info.max_member_alignment =
                             current_max_alignment_for_class_members();
    }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  }  /* if */
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
       signature, and other tests. */
    a_type_ptr  type = tssp->variant.function.routine->type;
    check_function_template_param_usage(sym, type, template_param_list);
  }  /* if */
  *p_tssp = tssp;
}  /* complete_function_template_decl */


static a_symbol_ptr function_template_declaration(
                               a_tmpl_decl_state_ptr	   decl_state,
                               a_symbol_locator            *locator,
                               a_func_info_block           *func_info,
                               a_storage_class             storage_class,
                               a_decl_modifiers_block_ptr  decl_modifiers,
                               a_type_ptr                  type)
/*
Scan a function template declaration or the declaration of a member function
of a class template.  locator identifies the function template being
declared.  func_info points to the block of information for the current
function declaration.  storage_class, decl_modifiers, and type indicate
information returned from decl_specifiers and declarator.
*/
{
  a_symbol_ptr         sym = NULL;

  db_enter(4, "function_template_declaration");  
  /* Set a flag in each param type entry whose associated type is or
     contains a template parameter. */
  set_type_involves_deduced_template_param(type);
  if (curr_token == tok_lbrace && decl_state->is_member_decl &&
      decl_state->is_template_friend) {
    /* A function template defined inside a class or class template is
       implicitly "inline".  Note that the only member declarations
       processed by this routine are friend declarations. */
    func_info->is_inline = TRUE;
  }  /* if */
  decl_state->prototype_scope_symbols = func_info->prototype_scope_symbols;
  /* Process a function template declaration. */
  decl_function_template(locator, type, func_info, &sym, storage_class,
                         decl_modifiers, decl_state->decl_info,
                         decl_state->orig_decl_level,
                         decl_state->is_specialization);
  if (func_info->is_definition) {
    
#if GENERATE_SOURCE_SEQUENCE_LISTS
  } else if (!source_sequence_entries_disallowed) {
    /* Turn the source sequence entry for the a_template entry into a
       secondary source sequence entry. */
    a_src_seq_secondary_decl_ptr sssdp = secondary_src_seq_for_template(
                                               decl_state->il_template_entry);
    sssdp->declared_type = func_info->declared_type;
    sssdp->friend_decl = decl_state->is_template_friend;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_member_function &&
      decl_state->is_specialization) {
    /* Earlier, we thought this was a specialization but it turned out to
       be a definition of a member of a class specialization. Reset
       specialization flag now. */
    decl_state->is_specialization = FALSE;
  }  /* if */
  if (decl_state->is_template_friend) {
    /* Make sure the friend is not in a local class. */
    check_local_class_template_friend(decl_state, locator);
  }  /* if */
  db_exit();
  return sym;
}  /* function_template_declaration */


static a_boolean is_class_template_decl(a_token_cache *token_cache)
/*
Determine whether the template declaration described by token_cache
is a class template declaration of the form

	friend	class-key identifier tok_colon
	      opt
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
  skip_illegal_class_template_decl_specifiers(/*diagnose=*/FALSE);
  if (curr_token == tok_class || curr_token == tok_struct ||
      curr_token == tok_union) {
    (void)get_token();
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (microsoft_mode or_near_and_far_enabled()) {
      /* Skip over any extended decl modifiers that may be present such as
         near/far or __single_inheritance. */
      prescan_decl_modifiers();
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    if (is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                        GID_USE_PROTOTYPE_NOT_NONREAL |
                                        GID_IS_TEMPLATE_PRESCAN |
					GID_IMPLICIT_TYPE_CONTEXT)) {
      (void)get_token();
      if (curr_token == tok_colon || curr_token == tok_end_of_source) {
        result = TRUE;
      }  /* if */
    } else if (curr_token == tok_colon || curr_token == tok_end_of_source) {
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


static void prescan_nonclass_template_declaration(
                               a_tmpl_decl_state_ptr	decl_state)
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
  a_boolean			is_friend;

  db_enter(4, "prescan_nonclass_template_declaration");

  ssep = &scope_stack[depth_scope_stack];
  check_assertion(ssep->kind == (a_scope_kind)sck_template_declaration);
  tp = prescan_and_find_declarator(&decl_state->decl_token_cache, &is_friend);
  /* Flush and remaining tokens from the reusable cache. */
  while (curr_token != tok_end_of_source) {
    if (curr_token == tok_friend && total_errors == 0) {
      /* Issue an error on any misplaced friend tokens in the cache.  The
         main purpose of this is to make sure than at least one error is
         issued on a misplaced friend keyword that could have affected the
         prescan of a template declaration.  An error might not have been
         issued if the friend keyword appeared in the ctor-initializer
         section of a constructor body. */
      pos_error(ec_bad_friend_decl, &pos_curr_token);
    }  /* if */
    (void)get_token();
  }  /* while */
  /* Skip past the tok_end_of_source. */
  (void)get_token();
  if ((is_friend != decl_state->is_template_friend) &&
      decl_state->is_member_decl) {
    /* The initial prescan found a tok_friend, but this prescan did not
       find it among the decl-specifiers of the declaration.  This can
       occur if "friend" appears later in some invalid position.  Update
       the flag to reflect the newly discovered state.  Note that we
       only do this if is_member_decl is TRUE (a declaration outside of
       a class should never be considered a friend). */
    decl_state->is_template_friend = is_friend;
#if CHECKING
    any_friend_state_changed = TRUE;
#endif /* CHECKING */   
  }  /* if */
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


static a_template_decl_ptr make_template_decl(a_template_param_ptr tp_list)
/*
Create an IL structure describing the parameterization of a template using the
information gathered in the front end structures.
*/
{
  a_template_decl_ptr       result = alloc_template_decl();
  a_template_parameter_ptr  il_tpp = NULL;
  a_template_param_ptr      sym_tpp;

  /* Copy the template parameter list into the IL: */
  for (sym_tpp = tp_list; sym_tpp != NULL; sym_tpp = sym_tpp->next) {
    a_template_parameter_ptr  new_tpp = alloc_template_parameter();
    switch (sym_tpp->param_symbol->kind) {
      case sk_type:
        new_tpp->kind = (a_template_parameter_kind)tpk_type;
        new_tpp->variant.type.ptr = sym_tpp->variant.type;
        new_tpp->variant.type.default_arg_type = sym_tpp->default_arg.type;
        new_tpp->source_corresp = *source_corresp_for_il_entry(
                                      (char*)sym_tpp->variant.type, iek_type);
        break;
      case sk_constant:
        new_tpp->kind = (a_template_parameter_kind)tpk_nontype;
        new_tpp->variant.nontype.constant = sym_tpp->variant.constant.ptr;
        new_tpp->variant.nontype.default_arg_constant =
                                                sym_tpp->default_arg.constant;
        new_tpp->source_corresp = *source_corresp_for_il_entry(
                          (char*)sym_tpp->variant.constant.ptr, iek_constant);
        break;
      case sk_class_template:
        new_tpp->kind = (a_template_parameter_kind)tpk_template;
        new_tpp->variant.templ.class_template = 
                                    sym_tpp->variant.templ->il_template_entry;
        new_tpp->variant.templ.default_arg_template =
                                                   sym_tpp->default_arg.templ;
        new_tpp->source_corresp = *source_corresp_for_il_entry(
                  (char*)new_tpp->variant.templ.class_template, iek_template);
        break;
      default:
        unexpected_condition_str("make_template_decl: unexpected symbol kind");
    }  /* switch */
    if (il_tpp == NULL) {
      result->param_list = new_tpp;
    } else {
      il_tpp->next = new_tpp;
    }  /* if */
    il_tpp = new_tpp;
  }  /* for */
  return result;
}  /* make_template_decl */


static void scan_template_param_clauses(
				a_tmpl_decl_state_ptr	decl_state,
				a_boolean		is_template_param)
/*
Note that this routine has a forward declaration.

Scan one or more template parameter lists of the form:

	template < param-list    >
                             opt

The parameter list can be empty for a specialization declaration.  Once
a non-empty parameter list has been specified, all subsequent parameter
lists must by non-empty.

This is used to scan the initial portion of template declarations and
also for template template parameters (when is_template_param is TRUE).

The parameter decl_state points to information describing the general state
of the parsing of the template clause so far (and this routine adds to that
information).  See the definition of a_tmpl_decl_state for details.
*/
{
  a_template_decl_info_ptr	    prev_template_decl_info = NULL;
  a_template_decl_info_ptr	    template_decl_info = NULL;
  a_boolean                    param_list_seen = FALSE;
  a_source_position            template_pos;
  a_template_decl_ptr          template_decl;

  /* Loop until there are no more template parameter clauses.  Note that
     this routine is not called for explicit instantiations, in which
     the template keyword is not followed by a parameter clause. */
  while (curr_token == tok_template) {
    /* The template parameter lists of template template parameters do not
       have nesting depths. */
    if (!is_template_param) decl_state->nesting_depth++;
    decl_state->number_of_template_param_clauses++;
    /* Bypass "template".  The next token should be "<".  This is done
       before the scope is pushed so that any pragma associated with the
       tok_template token will be processed in the current scope. */
    template_pos = pos_curr_token;
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
        /* Record the default name linkage at the point of declaration. */
        template_decl_info->name_linkage =
                         scope_stack[depth_scope_stack].default_name_linkage;
        push_template_declaration_scope(template_decl_info);
        check_assertion(!decl_state->is_full_specialization);
        decl_state->number_of_template_decl_scopes++;
        /* Save a pointer to the template declaration information in the
           scope stack entry. */
        scope_stack[depth_scope_stack].tmpl_decl_state = decl_state;
        scan_template_param_list(decl_state, is_template_param);
        template_decl_info->declaration_scope =
                                         scope_stack[decl_scope_level].number;
        /* Record that a template parameter list has been seen.  A
           subsequent missing parameter list is an error. */
        param_list_seen = TRUE;
        if (prototype_instantiations_in_il) {
          template_decl =
                        make_template_decl(decl_state->decl_info->parameters);
          template_decl->template_pos = template_pos;
          if (decl_state->il_template_entry != NULL) {
            template_decl->parent =
                                 decl_state->il_template_entry->template_decl;
            decl_state->il_template_entry->template_decl = template_decl;
          }  /* if */
          decl_state->template_decl = template_decl;
        }  /* if */
      } else if (is_template_param) {
        /* A template parameter declaration with a missing template
           parameter list. */
        error(ec_empty_template_param_list);
        /* Bypass the ">". */
        (void)get_token();
      } else {
        /* A specialization declaration.  If a previous "template < >" clause
           contained a template parameter list, all subsequent parameter
           lists must be non-empty. */
        decl_state->is_specialization = TRUE;
        if (param_list_seen) {
          error(ec_specialization_follows_param_list);
          decl_state->decl_scope_err = TRUE;
        }  /* if */
        /* Bypass the ">". */
        (void)get_token();
        if (prototype_instantiations_in_il) {
          template_decl = make_template_decl((a_template_param_ptr)NULL);
          template_decl->template_pos = template_pos;
          if (decl_state->il_template_entry != NULL) {
            template_decl->parent =
                                 decl_state->il_template_entry->template_decl;
            decl_state->il_template_entry->template_decl = template_decl;
          }  /* if */
          decl_state->template_decl = template_decl;
        }  /* if */
      }  /* if */
    } else {
      error(ec_missing_template_param_list);
    }  /* if */
  }  /* while */
  decl_state->decl_info = template_decl_info;
  if ((is_template_param ||
       (decl_state->is_member_decl && !decl_state->is_template_friend)) &&
      decl_state->number_of_template_param_clauses > 1) {
    /* A declaration with more than one template parameter clause is only
       valid in a namespace scope definition of a member template or in
       a friend declaration. */
    error(ec_multiple_template_decls_not_allowed);
    decl_state->decl_scope_err = TRUE;
  }  /* if */
}  /* scan_template_param_clauses */


static
void template_declaration(a_tmpl_decl_state_ptr	decl_state)
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
  a_token_cache                     *p_template_body_cache = NULL;
  a_template_cache_segment_ptr	    class_templ_cache_segments = NULL;
  a_template_cache_segment_ptr	    function_templ_cache_segments = NULL;
  a_boolean			    prototype_okay = FALSE;
  a_boolean			    is_class_template = FALSE;
  a_cached_token_ptr		    ctp;
  a_boolean			    invalid_decl = FALSE;

  db_enter(3, "template_declaration");
  /* Now that we know where the template declaration begins (and the template
     parameter list ends), break the original token cache at this point. */
  split_token_cache(&decl_state->param_list_cache,
                    &decl_state->decl_token_cache,
                    curr_token_sequence_number,
                    /*include_prev_token=*/FALSE,
                    /*okay_if_not_found=*/TRUE);
  /* Skip over any pragma entries for purposes of the following test. */
  ctp = decl_state->decl_token_cache.first_token;
  while (ctp != NULL &&
         ctp->extra_info_kind == (a_token_extra_info_kind)teik_pragma) {
    ctp = ctp->next;
  }  /* while */
  if (ctp == NULL ||
      ctp->token_sequence_number != curr_token_sequence_number) {
    /* We are not where we expected to be after scanning the template parameter
       lists.  Recache the template declaration now for better error
       recovery.  This should only happen in error cases. */
    check_assertion(total_errors != 0 || curr_token == tok_end_of_source ||
                    curr_token == tok_colon || curr_token == tok_lbrace ||
                    curr_token == tok_try || curr_token == tok_semicolon);
    cache_template_declaration(decl_state, /*skip_params=*/TRUE);
    /* Set the error flag to indicate that we know that some kind of error
       has occurred. */
    decl_state->decl_scope_err = TRUE;
  }  /* if */
  /* See if it is a class template declaration.  If it is, scan the tokens
     of the definition (if any) and cache them away of later reference. */
  if (is_class_template_decl(&decl_state->decl_token_cache)) {
    class_template_declaration(decl_state, &sym,
			       &tag_resolution);
    tssp = sym != NULL ? template_supplement_for_symbol(sym) : NULL;
    is_class_template = TRUE;
    if (decl_state->defines_something && sym != NULL) {
      /* Save a pointer to the token cache for class template body. */
      p_template_body_cache = &tssp->cache.tokens;
    }  /* if */
  } else {
    /* Not a class template declaration.  Check for a function template
       declaration or a static data member template definition. */
    /* Determine whether the thing being declared is a member of a
       class template.  This is needed to know how references to the
       parent class should be processed.  This must be done before 
       is_decl_start is called, as is_decl_start will cause the initial
       identifier (typically the return type) to be coalesced. */
    prescan_nonclass_template_declaration(decl_state);
    if (!is_decl_start(/*expr_context=*/FALSE,
                       /*real_declarator_allowed=*/TRUE) &&
        !is_declarator_start()) {
      /* Template parameters are declared, but the declaration is missing. */
      pos_error(ec_exp_declaration, &pos_curr_token);
    } else if (decl_state->is_member_decl && !decl_state->is_template_friend) {
      /* A member template declaration. */
      a_source_position	   decl_start_pos;
      decl_start_pos = pos_curr_token;
      sym = class_member_template_declaration(decl_state->class_declared_in,
                                              decl_state->
                                                     decl_info->parameters,
                                              decl_state->il_template_entry,
                                              &decl_state->decl_pos_block);
      complete_function_template_decl(decl_state, sym,
                                      (a_func_info_block *)NULL,
                                      &tssp, &decl_start_pos);
      if (decl_state->defines_something) {
        /* Save a pointer to the token cache for function body.  tssp may
           be NULL in error cases. */
        if (tssp != NULL) p_template_body_cache = &tssp->cache.tokens;
      } /* if */
    } else {
      a_type_ptr              type;
      a_symbol_locator        locator;
      a_decl_flag_set         do_flags;
      a_decl_flag_set         dso_flags;
      a_func_info_block       func_info;
      a_storage_class         storage_class;
      a_decl_modifiers_block  decl_modifiers;

      /* Scan the decl. specifiers and the declaration. */
      clear_func_info(&func_info);
      scan_template_declaration(/*is_initial_decl=*/TRUE,
                                decl_state->is_member_decl,
                                decl_state->class_declared_in,
                                decl_state->decl_scope_err,
                                decl_state->is_specialization,
                                &dso_flags, &do_flags, &locator, &type,
                                &func_info, &storage_class, &decl_modifiers,
                                (a_routine_ptr)NULL,
			        (a_template_instance_ptr)NULL,
                                &decl_state->decl_pos_block);
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
          locator.specific_symbol != NULL) {
        sym = template_static_data_member_declaration(
                                 decl_state, &locator, do_flags, type, &tssp);
        /* Save a pointer to the token cache for the initializer.  tssp
           may be NULL in error cases. */
        if (tssp != NULL) p_template_body_cache = &tssp->cache.tokens;
      } else if (is_function_type(type)) {
        sym = function_template_declaration(
                 decl_state, &locator, &func_info, storage_class,
                 &decl_modifiers, type);
        complete_function_template_decl(decl_state, sym, &func_info,
                                        &tssp, &locator.source_position);
        if (decl_state->defines_something) {
          /* Save a pointer to the token cache for function body.  tssp may
             be NULL in error cases. */
          if (tssp != NULL) p_template_body_cache = &tssp->cache.tokens;
        } /* if */
      } else {
        /* Error -- not a class template, a function template, nor a static
           data member template. */
        if (!is_error_locator(locator)) {
          pos_st_error(ec_bad_template_declaration, &locator.source_position,
                       locator.symbol_header->identifier);
        } else {
          /* Record that this declaration was invalid so that we can flush
             to the end of the declaration, if necessary. */
          invalid_decl = TRUE;
        }  /* if */
      }  /* if */
      done_with_func_info(func_info);
    }  /* if */
  }  /* if */
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_function_template &&
      !sym->is_class_member) {
    /* For function templates that are not class members, remove any
       default arguments that may have been specified.  At this point,
       just fetch the list of cache segments from the scope stack entry. */
    if (decl_state->decl_token_cache_used) {
      /* The default arguments only need to be removed from the function
         template declaration stored in the decl_cache.  If this is not
         the initial declaration, this process need not be done. */
      a_scope_stack_entry_ptr	ssep;
      ssep = &scope_stack[depth_scope_stack];
      function_templ_cache_segments = ssep->first_template_cache_segment;
    }  /* if */
  }  /* if */
  /* Pop all of the template declaration scopes that were pushed earlier.
     Note that this must be done before doing the prototype instantiation. */
  for (; decl_state->number_of_template_decl_scopes != 0;
         decl_state->number_of_template_decl_scopes--) {
    pop_scope();
  }  /* for */
  /* Save the declaration sequence number at the end of this template
     declaration. */
  if (decl_state->decl_info != NULL) {
    /* Record the current declaration sequence number.  This is used
       to restrict name visibility during template instantiation. */
    if (decl_state->decl_info->decl_seq == NO_DECL_SEQUENCE_NUMBER) {
      /* Only set it if it was not already set.  For subordinate function
         templates, it will have been set based on the prototype (but note
         that this is only used for the decl_cache, the body cache is
         actually used directly from the prototype template). */
      decl_state->decl_info->decl_seq = ++decl_seq_counter;
    }  /* if */
  }  /* if */
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
  check_assertion_str2(tssp == NULL || tssp->il_template_entry != NULL,
                       "template_declaration:", "il_template_entry not set");
  /* The IL template entry should already be set to point to the initial
     declaration.  In case this is a redeclaration, set the source
     correspondence for this template entry. */
  set_il_template_entry(decl_state, sym, tssp);
  if (is_class_template) {
    if (!decl_state->decl_scope_err && decl_state->defines_something) {
      a_type_ptr	prototype_type;
      a_symbol_ptr	prototype_sym;
      check_assertion_str2(sym != NULL && tssp != NULL,
                           "template_declaration:", "sym or tssp NULL");
      prototype_sym = tssp->variant.class_template.prototype_instantiation;
      prototype_type = type_symbol_type(prototype_sym);
      check_assertion_str2(is_class_struct_union_symbol(prototype_sym),
                           "template_declaration:", "prototype_sym invalid");
      if (!sym->is_error) {
        /* Do a "prototype instantiation" of the class template -- i.e., parse
           the declarative information looking for gross syntax errors. */
        prototype_okay = TRUE;
        assoc_template_of(prototype_type) = tssp->il_template_entry;
        instantiate_class_template(sym, prototype_type,
                                   &class_templ_cache_segments, decl_state);
        prototype_type->source_corresp.decl_position = sym->decl_position;
        if (tag_resolution) {
          /* This is the resolution of a previously incomplete template
             declaration.  If there are any incomplete instantiations that were
             involved in array type declarations, fix them up now. */
          fixup_types_that_refer_to_incomplete_instantiations(sym,
		                                              prototype_type);
        }  /* if */
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    } else if (!decl_state->defines_something &&
               !source_sequence_entries_disallowed) {
      /* Turn the source sequence entry for the a_template entry into a
         secondary source sequence entry. */
      a_src_seq_secondary_decl_ptr sssdp = secondary_src_seq_for_template(
                                               decl_state->il_template_entry);
      sssdp->friend_decl = decl_state->is_template_friend;
      sssdp->autonomous_tag_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  } else if (nonclass_prototype_instantiations && sym != NULL) {
    if (is_function_or_template_symbol(sym)) {
      /* Do the prototype instantiation of the function. */
      if (!decl_state->decl_scope_err && decl_state->defines_something) {
        if (decl_state->class_declared_in == NULL) {
          /* Prototype instantiations for templates declared within classes
             are handled elsewhere. */
          function_prototype_instantiation(sym);
        }  /* if */
      }  /* if */
    } else {
      check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
      static_data_member_prototype_instantiation(sym);
    }  /* if */
  }  /* if */
  /* Extract the bodies of any member functions, nested classes, or
     member templates that were defined within this class template. */
  if (prototype_okay) {
    class_templ_cache_segments = extract_member_bodies(
                                                &tssp->cache,
                                                class_templ_cache_segments,
                                                /*keep_default_args=*/TRUE);
  } /* if */
  /* Complete the a_template entry and link it into the list of templates
     for the appropriate scope. */
  complete_il_template_entry(decl_state, sym, p_template_body_cache);
  if (class_templ_cache_segments != NULL) {
    /* Remove any default arguments that may remain in the cache. */
    (void)extract_member_bodies(&tssp->cache, class_templ_cache_segments,
                                /*keep_default_args=*/FALSE);
  } /* if */
  if (function_templ_cache_segments != NULL) {
    /* For function templates that are not class members, remove any
       default arguments that may have been specified. */
    (void)extract_member_bodies(&tssp->variant.function.decl_cache,
                                function_templ_cache_segments,
                                /*keep_default_args=*/FALSE);
  }  /* if */
  if (invalid_decl) {
    /* The declaration was invalid -- flush to the end of the declaration
       if necessary. */
    add_stop_token(tok_semicolon);
    add_stop_token(tok_rbrace);
    flush_tokens();
    remove_stop_token(tok_semicolon);
    remove_stop_token(tok_rbrace);
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (sym != NULL) db_symbol(sym, "template symbol: ", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* template_declaration */


a_symbol_ptr find_matching_template_instance(
			a_symbol_ptr		sym,
			a_type_ptr		type,
			a_template_arg_ptr	explicit_arg_list,
			a_boolean		explicit_arg_list_present,
			an_error_severity	severity_if_not_found)
/*
sym is some kind of function symbol.  type is the type declared for a
function template instance.  explicit_arg_list is an explicitly specified
template argument list, which may be NULL.  explicit_arg_list_present
is TRUE if an explicit argument list was provided, even an empty one
(in which case explicit_arg_list would be NULL).  severity_if_not_found
is the severity of the diagnostic to be issued if no matching instance
is found.  Return the symbol for the instance, or NULL if no instance is
found.
*/
{
  a_symbol_ptr  		orig_sym;
  a_boolean     		any_found = FALSE;
  a_symbol_ptr			new_sym = NULL;
  a_boolean			any_templates = FALSE;
  a_partial_order_candidate_ptr	candidates_list = NULL;

  orig_sym = sym;
  if (sym->is_class_member && !explicit_arg_list_present) {
    /* A member function symbol, find the member function that matches
       the specified type.  This is used to find a normal member function
       of a template class.  Skip this step when an explicit template
       argument list has been specified, as this implies that the entity
       to be found must be a template. */
    new_sym = member_function_redecl_sym(sym, type,
                                         (a_template_param_ptr)NULL);
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
      /* If this is a function template symbol, use it to find a function
         that matches the type we are looking for. */
      if (sym->kind != (a_symbol_kind)sk_function_template) continue;
      any_templates = TRUE;
      if (has_matching_template_function(sym, type, explicit_arg_list,
                                         /*is_decl_context=*/TRUE)) {
        /* This template can generate an instance of the appropriate
           type.  Add the matching template to a list of matching
           candidates. */
        add_to_partial_order_candidates_list(&candidates_list, sym,
                                             (a_template_arg_ptr)NULL);
      }  /* if */
    }  /* for */
    if (candidates_list != NULL) {
      /* If any of the templates matched, select the best one using
         the partial ordering rules.  If a best match cannot be selected,
         an arbitrary member of the unordered set of templates will be
         returned and the ambiguous flag will be set. */
      a_template_arg_ptr	templ_arg_list;
      a_boolean			ambiguous;
      any_found = TRUE;
      select_best_partial_order_candidate(candidates_list, (a_symbol_ptr)NULL,
					  &sym, &templ_arg_list,
					  &ambiguous);
      /* Look for a match on the list of instantiations. */
      if (ambiguous) {
        sym_error(ec_ambiguous_overloaded_function, orig_sym);
        new_sym = NULL;
      } else {
        a_boolean  is_new_template_instance;
        new_sym = matching_template_function(sym, type, explicit_arg_list,
					     explicit_arg_list_present,
                                             /*is_decl_context=*/TRUE,
                                             &is_new_template_instance);
      }  /* if */
    }  /* for */
  }  /* if */
  if (!any_found) {
    /* Issue an error.  One message is used for overloaded functions (which
       includes all template cases).  Another message is used if the symbol
       refers to a single nontemplate. */
    an_error_code	err_code;
    if (any_templates ||
        orig_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      err_code = ec_no_match_for_type_of_overloaded_function;
    } else {
      err_code = ec_not_compatible_with_previous_decl;
    }  /* if */
    sym_diagnostic(severity_if_not_found, err_code, orig_sym);
  }  /* if */
  return new_sym;
}  /* find_matching_template_instance */


a_boolean has_matching_template_instance(
				a_symbol_ptr		sym,
                                a_type_ptr		type,
				a_template_arg_ptr	explicit_arg_list)
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
                                             explicit_arg_list,
                                             /*is_decl_context=*/TRUE);
      if (found) break;
    }  /* for */
  }  /* if */
  return found;
}  /* has_matching_template_instance */


static a_boolean check_template_nesting_depth(
					a_symbol_ptr		sym,
					a_source_position	*pos,
					a_tmpl_decl_state_ptr	decl_state)
/*
This routine is used to determine whether the number of template clauses
in a specialization matches the template nesting depth of the
entity being specialized.  If a mismatch is found, a diagnostic is
issued, and TRUE is returned.
*/
{
  a_template_nesting_depth	depth = 0;
  a_template_arg_ptr		arg_list;
  a_boolean			is_template = FALSE;
  a_type_ptr			parent_tp;
  a_boolean			result = FALSE;

  /* The presence of a template argument list indicates that this entity is
     an instance of a class or function template.  A member function of
     a class template or a nested class within a class template will not
     have a template argument list. */
  switch (sym->kind) {
    case sk_class_or_struct_tag:
    case sk_union_tag:
    {
      a_type_ptr	tp;
      tp = sym->variant.class_struct_union.type;
      arg_list = tp->variant.class_struct_union.extra_info->template_arg_list;
      is_template = arg_list != NULL;
      break;
    }
    case sk_member_function:
    case sk_routine:
    {
      a_routine_ptr	rp;
      rp = sym->variant.routine.ptr;
      arg_list = rp->template_arg_list;
      is_template = arg_list != NULL;
      break;
    }
    case sk_class_template:
    case sk_function_template:
      is_template = TRUE;
      break;
    case sk_static_data_member:
      break;
    default:
      unexpected_condition_str("check_template_nesting_depth: bad sym kind");
  }  /* switch */
  /* If the entity being declared is a template or a template instance
     (but not a nested class or member function of a class template),
     there must be a template parameter clause for the entity. */
  if (is_template) depth++;
  if (decl_state->is_member_decl && !decl_state->is_template_friend) {
    /* If this declaration appears within a class, don't count the enclosing
       classes.  This should only occur in Microsoft mode when an explicit
       specialization appears within a class definition. */
  } else {
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
  }  /* if */
  if (depth != decl_state->number_of_template_param_clauses &&
      !decl_state->decl_scope_err) {
    /* The depths do not match, issue a diagnostic. */
    pos_sy_diagnostic(es_discretionary_error,
                      ec_template_depth_mismatch, pos, sym);
    decl_state->decl_scope_err = TRUE;
    result = TRUE;
  }  /* if */
  return result;
}  /* check_template_nesting_depth */


static void full_specialization(a_tmpl_decl_state_ptr decl_state)
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
  a_decl_modifiers_block        decl_modifiers;
  a_source_sequence_entry_ptr   declarator_ssep = NULL;
  a_symbol_ptr		        sym;
  a_func_info_block             func_info;
  a_boolean			keep_func_info = FALSE;
  a_symbol_reference_kind       srk_flags = SRK_DECLARATION;
  a_source_position             decl_start_pos, id_pos;
  a_boolean                     has_parenthesized_initializer;
  a_source_correspondence       *scp;
  a_routine_ptr                 rp;
  a_variable_ptr                vp;
  a_boolean			is_definition;
  a_boolean			is_constructor = FALSE;
  a_decl_pos_block              decl_pos_block;
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
  a_boolean                     first_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */

  db_enter(3, "full_specialization");
  decl_start_pos = pos_curr_token;
  clear_decl_pos_block(&decl_pos_block);
  /* First scan the decl-specifiers. */
  (void)decl_specifiers((DSI_IS_SPECIALIZATION |
                         DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                         DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_INLINE_ALLOWED |
                         (decl_state->is_member_decl
                                  ? DSI_IS_MEMBER_DECLARATION
                                  : DSI_NO_INPUT_FLAGS)),
                        &dso_flags, &storage_class, &type, &qualifiers,
                        &decl_modifiers, &decl_pos_block);
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
    if (!is_any_template_instance_class_symbol(sym)) {
      /* Not a template instance. */
      sym_error(ec_entity_cannot_be_specialized, sym);
    } else {
      /* Make sure that this declaration has the correct number of
         "template <>" clauses. */
      (void)check_template_nesting_depth(sym, &decl_start_pos, decl_state);
      type->variant.class_struct_union.is_specialized = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      { a_boolean	decl_is_definition;
        /* The specialization should be marked as an autonomous declaration. */
        decl_is_definition = ((dso_flags & DSO_DEFINES_SOMETHING) != 0);
        if (decl_is_definition) {
          type->autonomous_primary_tag_decl = TRUE;
        } else {
          an_sssd_flag_set  flags = SSSD_AUTONOMOUS_TAG_DECL |
                                    SSSD_SPECIALIZED_WITH_NEW_SYNTAX;
          (void)update_src_seq_secondary_decl((char *)type, type,
                                              flags, &decl_pos_block);
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
    if (decl_state->is_member_decl && (dso_flags & DSO_CONSTRUCTOR)) {
      /* If this is a Microsoft mode specialization in a class context, and
         decl_specifiers returned a constructor flag, pass the constructor
         flag into declarator.  This flag can only be set when a parent class
         type is provided to declarator. */
      di_flags |= DI_IS_CONSTRUCTOR;
    }  /* if */
    if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) &&
        qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
    declarator(di_flags, &do_flags, (a_type_qualifier_set *)NULL, type,
               decl_state->class_declared_in,
               &locator, &type, &declarator_ssep, &func_info, &decl_pos_block);
    sym = NULL;
    has_parenthesized_initializer =
                              (do_flags & DO_PARENTHESIZED_INITIALIZER) != 0;
    if (!is_error_locator(locator)) {
      id_pos = locator.source_position;
      sym = locator.specific_symbol;
      if (sym == NULL) {
        sym = normal_id_lookup(&locator, IDL_LINKAGE_LOOKUP);
      }  /* if */
    }  /* if */
    check_for_declaration_errors(merge_declarator_flags(dso_flags, do_flags),
                                 type, &locator, &decl_start_pos);
    /* Issue diagnostic on an incomplete-type in an exception specification. */
    report_exception_spec_errors(&func_info);
    if (is_error_locator(locator)) {
      /* Ignore it. */
      sym = NULL;
    } else if (decl_state->in_prototype_instantiation &&
               !microsoft_mode) {
      /* A specialization in a prototype instantiation scope. */
      sym = NULL;
      set_to_error_locator(locator);
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
        sym = find_matching_template_instance(
                                        sym, type, locator.template_arg_list,
                                        (a_boolean)locator.is_template_id,
					es_error);
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
          ((!sym->is_class_member && sym->parent.namespace_ptr == NULL) ||
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
        is_constructor = is_constructor_symbol(sym);
        is_definition = (curr_token == tok_lbrace ||
                         curr_token == tok_try ||
                         (curr_token == tok_colon &&
                          is_constructor));
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
      if (already_specialized) first_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */
      if (scp->referenced && !already_specialized) {
        /* The entity has already been referenced and cannot be specialized.
           This is accepted for class members in Microsoft bugs mode. */
        an_error_severity	severity;
        severity = microsoft_bugs && sym->is_class_member ? es_warning
                                                          : es_error;
        pos_sy_diagnostic(severity, ec_specialization_of_referenced_entity,
                          &locator.source_position, sym);
        if (severity == es_error) sym = NULL;
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
           tokens to the closing right brace.  Leave it to the caller to
           advance past the closing right brace. */
        flush_until_matching_token();
        *(decl_state->final_token_ptr) = tok_rbrace;
      } else if (curr_token == tok_assign) {
        /* This may have been intended to be a static data member
           initialization.  Flush to a semicolon. */
        add_stop_token(tok_semicolon);
        flush_tokens();
        remove_stop_token(tok_semicolon);
      }  /* if */
    } else {
      /* The symbol is not NULL. */
      sym->decl_position = id_pos;
      if (is_definition) {
        srk_flags |= SRK_DEFINITION;
        if (sym->kind == (a_symbol_kind)sk_static_data_member) {
          srk_flags |= SRK_INITIALIZATION;
          /* Set the IL referenced flag since, as an externally visible
             variable, it could be referenced from another translation unit. */
          scp->referenced = TRUE;
        } else {
          /* The IL referenced flag for defined functions is updated later. */
        }  /* if */
      }  /* if */
      /* Update cross reference info, etc. */
      record_symbol_declaration(srk_flags, sym, &locator.source_position,
                                declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (is_definition || first_decl) {
        update_decl_pos_info(scp, &decl_pos_block);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Make sure that this declaration has the correct number of
         "template <>" clauses. */
      (void)check_template_nesting_depth(sym, &locator.source_position,
                                         decl_state);
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Do fixup on the source sequence entry that was just created to
           represent the current declaration. */
        if (!is_definition) {
          an_sssd_flag_set  flags = SSSD_SPECIALIZED_WITH_NEW_SYNTAX;

          if (first_decl) flags |= SSSD_FIRST_DECLARATION;
          (void)update_src_seq_secondary_decl((char *)vp, type, flags, 
                                              &decl_pos_block);
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
          /* Make sure that the type of the static data member is complete.
             If the type cannot be completed, an error will be issued by
             initializer.  Note that a static data member specialization
             that is a definition always has an initializer (such a
             declaration without an initializer is just a declaration, not
             a definition). */
          complete_class_type_is_needed(vp->type);
          initializer(sym, &locator.source_position,
                      (an_id_linkage_kind)idl_external,
                      has_parenthesized_initializer,
                      /*is_old_style_param_decl=*/FALSE,
                      &incomplete_type_error_reported,
                      &decl_pos_block);
        }  /* if */
      } else {
        /* Issue an error if the exception specification on the instance does
           not match that of the template. */
        check_exception_specification(type, sym, &func_info.throw_position,
                                      /*is_redecl=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Do fixup on the source sequence entry that was just created to
           represent the current declaration. */
        { a_type_ptr        declared_type;
          an_sssd_flag_set  flags = SSSD_SPECIALIZED_WITH_NEW_SYNTAX;

          declared_type = form_declared_type(type, &func_info);
          if (first_decl) flags |= SSSD_FIRST_DECLARATION;
          if (is_definition) {
            /* The defining declaration of the routine.  Record the declared
               type. */
            check_assertion(rp->declared_type == NULL);
            set_routine_declared_type(rp, declared_type);
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
            if (rp->source_corresp.source_sequence_entry != NULL &&
                (an_il_entry_kind)rp->source_corresp.source_sequence_entry->
                                                                 entity.kind ==
                             (an_il_entry_kind)iek_src_seq_secondary_decl) {
              /* This must be a member or friend function definition inside
                 the definition of a nonlocal class.  A source sequence
                 entry representing the function definition will be inserted
                 following the class definition and a secondary source
                 sequence entry has been put out here. */
              (void)update_src_seq_secondary_decl((char *)rp, declared_type,
                                                  flags, &decl_pos_block);
            }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
          } else {
            (void)update_src_seq_secondary_decl((char *)rp, declared_type,
                                                flags, &decl_pos_block);
          }  /* if */
        }
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
        if ((func_info.is_inline && !extern_inline_allowed) ||
            storage_class == (a_storage_class)sc_static) {
          /* Function was declared "static" or it was declared "inline" and
             inline functions have internal linkage by default. */
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_internal;
        } else {
          rp->storage_class = is_definition? (a_storage_class)sc_unspecified :
                                             (a_storage_class)sc_extern;
          rp->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_cplusplus_external;
        }  /* if */
        if (type->kind == (a_type_kind)tk_typeref) {
          func_info.function_type_from_typedef = TRUE;
          type = skip_typerefs(type);
        }  /* if */
        if (is_definition) {
          /* This is a defining declaration of the function template. */
          func_info.is_definition = TRUE;
          if (func_info.function_type_from_typedef) {
            /* Just as it is an error when a normal function is defined for
               the function type to come from a typedef, so too is that an
               error when a function template is being defined. */
            error(ec_function_type_must_come_from_declarator);
            /* No need to copy the type, since it will not actually be
               pointed to by the routine entry. */
          }  /* if */
          if (remove_qualifiers_from_param_types) {
            /* The parameter type top-level cv-qualifiers that appeared on
               the specialization declaration should be preserved (in
               preference to those from on the template declaration), since
               they belong to the definition -- i.e., are used to form the
               the parameter variable types.  For instance:
                 template <class T> int f(T);
                 template<> int f<const int>(int x) {
                   return ++x;        // no error
                 }
               Go through the param-type lists of the two routine types and
               update the type generated from the template with qualifiers
               from the type as actually declared. */
            a_param_type_ptr  ptp, decl_ptp;

            for (ptp = rp->type->variant.routine.extra_info->param_type_list,
                 decl_ptp = type->variant.routine.extra_info->param_type_list;
                 ptp != NULL && decl_ptp != NULL;
                 ptp = ptp->next, decl_ptp = decl_ptp->next) {
              ptp->qualifiers = decl_ptp->qualifiers;
            }  /* for */
          }  /* if */
          /* Scan the function body. */
          if (decl_state->is_member_decl) {
            /* A Microsoft mode specialization that appears in a class context.
               Cache the function body now and scan it later during the class
               fixup process. */
            a_token_cache	body_cache;

            clear_token_cache(&body_cache, /*reusable=*/TRUE);
            cache_function_template_body(decl_state, &body_cache,
                                         is_constructor,
                                         &locator.source_position);
            /* Add the specialization to the routine fixup list for the class
               being defined.  This routine makes a copy of the body cache
               so body_cache should not be discarded here. */
            add_routine_fixup_for_specialization(decl_state->class_declared_in,
                                                 sym, &func_info, &body_cache);
            /* The param_id_list is needed because the func_info information
               is on the routine fixup list.  Don't discard it below. */
            keep_func_info = TRUE;
            /* Determine if this is a copy constructor.  If so, set the class
               symbol supplement flags appropriately.  Note that in-class
               specializations are only permitted in Microsoft mode, so
               a specialization can only be considered a copy constructor
               in Microsoft mode too. */
            if (is_constructor_symbol(sym)) {
              check_member_decl_is_copy_constructor(
                                         rp, decl_state->class_declared_in,
                                         /*compiler_generated=*/FALSE);
            }  /* if */
          } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
            if (rp->source_corresp.is_class_member) {
              rp->defined_outside_of_parent = TRUE;
            }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
            scan_function_body(rp, &func_info,
                               SFB_NEW_STRUCT_STMT_STACK_REQUIRED);
          }  /* if */
          /* Leave it to the caller to advance past the closing right brace. */
          *(decl_state->final_token_ptr) = tok_rbrace;
        } else {
          /* Update xref info on param ids. */
          record_param_id_list_declarations(&func_info);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!keep_func_info) done_with_func_info(func_info);
    remove_stop_token(tok_semicolon);
  }  /* if */
  db_exit();
}  /* full_specialization */


static void decl_level_of_template(a_tmpl_decl_state_ptr decl_state)
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
  decl_state->orig_decl_level = depth;
  if (!err && decl_state->is_template_friend &&
      !decl_state->in_prototype_instantiation) {
    /* For friend declarations (that are not in a prototyep instantiation),
       the effective declaration level is the nearest namespace scope. */
    depth = depth_innermost_namespace_scope;
  }  /* if */
  decl_state->effective_decl_level = depth;
  /* Determine whether this is a friend declaration.  For declarations
     inside a class this is determine by inspecting the tokens that
     make up the template declaration. */
  decl_state->is_template_friend = decl_state->is_member_decl &&
                                           decl_state->is_template_friend;
  /* Determine the nesting depth of this template declaration.  Templates
     not enclosed within other templates are given a depth of "1".  The
     depth is incremented for each successive template declaration.  Friend
     declarations are normally restarted at a depth of "1" (the depth set here
     is incremented before used).  This is suppressed inside prototype
     instantiations because the outer template parameters must be distinct
     from those of the template when prototype instantiations are put in
     the IL.  This special processing is okay because a friend in a prototype
     instantiation is never matched up with an existing declaration of a 
     template. */
  if (decl_state->is_template_friend &&
      !decl_state->in_prototype_instantiation) {
    decl_state->nesting_depth = 0;
  } else {
    decl_state->nesting_depth = template_nesting_depth();
  }  /* if */
}  /* decl_level_of_template */


static void template_or_specialization_declaration(
				a_token_kind		*final_token,
				a_boolean		export_present,
				a_source_position	*export_pos)
/*
Scan a template declaration of a template specialization declaration.

This routine determines whether the entity being scanned is a "full
specialization".  A full specialization is a declaration that declares
a real function or class and not a template.  In a full specialization
all of the template parameter clauses contain empty template parameter
lists (e.g., "template <>").  Declarations that are not full specializations
are either the specialization of a template or a template declaration.

export_present is TRUE if the template keyword was preceded by "export".
If export_present is TRUE, export_pos is the position of the export
keyword.
*/
{
  a_tmpl_decl_state		decl_state;
  a_def_arg_expr_fixup_ptr	saved_curr_default_args;
  a_scope_depth			orig_depth = depth_scope_stack;

  check_assertion_str2(curr_token == tok_template,
                       "template_or_specialization_declaration:",
                       "expected tok_template");
  init_templ_decl_state(&decl_state);
  /* Note that select_curr_construct_pragmas is called in the caller.
     extract_curr_construct_pragmas is called to save the list of
     pragmas associated with this template declaration.  This pragma
     list will later be associated with the template and applied to
     each instance generated from the template. */
  decl_state.pragmas_bound_to_template = extract_curr_construct_pragmas();
  decl_state.export_present = export_present;
  decl_state.export_position = *export_pos;
  saved_curr_default_args = curr_default_args;
  curr_default_args = NULL;
  decl_state.start_pos = pos_curr_token;
  decl_state.in_prototype_instantiation =
                    scope_stack[depth_scope_stack].in_prototype_instantiation;
  decl_state.final_token_ptr = final_token;
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
  if (decl_state.effective_decl_level == NO_SCOPE_DEPTH) {
    pos_error(ec_bad_template_declaration_scope, &decl_state.start_pos);
    decl_state.decl_scope_err = TRUE;
    /* Set the effective declaration level to a valid value for the remainder
       of the processing. */
    decl_state.effective_decl_level = depth_scope_stack;
    decl_state.orig_decl_level = depth_scope_stack;
  }  /* if */
  if (export_present) {
    if (decl_state.is_member_decl) {
      /* Member declarations cannot be declared export. */
      pos_error(ec_exported_member_decl, export_pos);
    } else if (scope_stack[depth_scope_stack].within_unnamed_namespace) {
      /* A template in an unnamed namespace cannot be declared export. */
      pos_error(ec_exported_in_unnamed_namespace, export_pos);
    }  /* if */
  }  /* if */
  if (decl_state.is_full_specialization) {
    /* No IL template entry required. */
  } else {
    /* Create an IL template entry for this declaration.  This is only done
       for template declarations and specializations that are still templates.
       IL entries are usually not created for templates found during prototype
       instantiation of other templates because they will be included in
       the template string of the enclosing template. */
    decl_state.il_template_entry = make_il_template_entry(&decl_state);
  }  /* if */
  /* Scan one or more template parameter lists.  Each template parameter
     list looks like "template < param-list >".  The param-list is
     optional (but once a parameter list has been specified, all subsequent
     param-lists must be present). */
  scan_template_param_clauses(&decl_state, /*is_template_param=*/FALSE);
  if (decl_state.is_specialization) {
    /* A specialization declaration is only permitted in a namespace scope. */
    a_scope_stack_entry_ptr ssep = scope_stack_entry_for(orig_depth);
    if ((ssep->kind == (a_scope_kind)sck_file ||
        ssep->kind == (a_scope_kind)sck_namespace ||
        ssep->kind == (a_scope_kind)sck_namespace_extension)) {
      /* A valid template specialization scope. */
    } else if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
               microsoft_mode) {
      /* Microsoft permits specializations to appear in class scopes. */
    } else {
      if (!decl_state.decl_scope_err) {
        pos_error(ec_explicit_specialization_not_in_namespace_scope,
                  &decl_state.start_pos);
        decl_state.decl_scope_err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (decl_state.is_full_specialization) {
    /* The entity being declared is a full specialization. */
    if (export_present) {
      /* A full specialization cannot be exported. */
      pos_error(ec_bad_decl_for_export, export_pos);
    }  /* if */
    full_specialization(&decl_state);
  } else {
    /* The entity being declared is a template. */
    template_declaration(&decl_state);
  }  /* if */
  wrapup_templ_decl_state(&decl_state);
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
  a_directory_name_entry_ptr
                        dir_entry;
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
  if (sfp != NULL && !sfp->top_level_file &&
      sfp->name_as_written != NULL) {
    /* A source file was found and it does not refer to the primary source
       file.  sfp->name_as_written will be NULL if the file name came from
       a #line directive, in which case the implicit inclusion will not
       be attempted. */
    if (!sfp->related_file_implicit_include_done) {
      /* If we haven't already included the corresponding source file then
         do so now. */
      check_assertion(!after_instantiation_wrapup);
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
                                     /*use_search_path=*/TRUE,
                                     is_system_include,
                                     /*is_include_next=*/FALSE,
				     /*replace_suffix=*/TRUE,
				     &full_file_name, &display_name,
				     &dir_entry);
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
                             is_system_include, /*is_preinclude=*/FALSE,
                             /*is_implicit_include=*/TRUE,
                             dir_entry, ifhp);
            scan_implicitly_included_template_definition_file();
            if (in_instantiation_wrapup ) {
              /* Set a flag if this implicit inclusion was done during
                 instantiation wrapup. */
              implicit_inclusion_done_during_instantiation_wrapup = TRUE;
            }  /* if */
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

#if EXPENSIVE_CHECKING

static void check_for_nonreal_instance(a_template_instance_ptr	tip)
/*
Make sure that none of the template parameters of the template instance
specified by "tip" depend on a template parameter.
*/
{
  a_symbol_ptr		sym;
  a_template_arg_ptr	arg_list;

  sym = tip->instance_sym;
  /* Get the template argument list for the routine or static data member. */
  if (is_function_symbol(sym)) {
    a_routine_ptr	rp;
    rp = sym->variant.routine.ptr;
    arg_list = rp->template_arg_list;
  } else {
    /* A static data member -- always a member of a class template. */
    arg_list = NULL;
  }  /* if */
  if (arg_list == NULL) {
    /* If the argument list is NULL this must be a member of a template
       class.  Get the argument list from the parent class.  Find the
       nearest enclosing class template (class with a template
       argument list. */
    a_type_ptr	type;
    check_assertion(sym->is_class_member);
    type = sym->parent.class_type;
    while (type != NULL && type->source_corresp.is_class_member &&
           type->variant.class_struct_union.extra_info->
                                                  template_arg_list == NULL) {
      type = type->source_corresp.parent.class_type;
    }  /* while */
    check_assertion(type != NULL);
    arg_list = type->variant.class_struct_union.extra_info->template_arg_list;
  }  /* if */
  check_assertion_str2(!template_arg_list_involves_template_param(arg_list),
                       "check_for_nonreal_instance:",
                       "nonreal instance on instantiation required list");
}  /* check_for_nonreal_instance */

#endif /* EXPENSIVE_CHECKING */


static void add_to_instantiations_required_list(a_template_instance_ptr  tip)
/*
Add a template instance entry to the end of the instantiations_required
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
#if EXPENSIVE_CHECKING
    /* Make sure none of the template arguments depend on template
       parameters. */
    check_for_nonreal_instance(tip);
#endif /* EXPENSIVE_CHECKING */
  }  /* if */
  db_exit();
}  /* add_to_instantiations_required_list */


static a_boolean is_inline_template_function(a_template_instance_ptr tip)
/*
Determines whether a template instance pointer refers to a function that
is inline.  This involves more than simply checking the is_inline flag
in the routine entry because, if the function has not yet been instantiated,
the template from which the routine would be generated must be checked.
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
    if (!result && !rout->is_specialized) {
      if (rout->assoc_scope == NULL_region_number) {
        a_template_symbol_supplement_ptr	tssp;
        tssp = template_supplement_for_symbol(tip->template_sym);
        result = tssp->variant.function.routine->is_inline ||
                 func_info_for_template(tssp)->is_inline;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_inline_template_function */


a_boolean rout_is_inline_template_function(a_routine_ptr	rout)
/*
Interface to is_inline_template_function that takes a routine pointer
as its argument.  rout must be a pointer to an instance of a function
template or a member function of a template class.
*/
{
  a_symbol_ptr			sym;
  a_template_instance_ptr	tip;

  sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
  check_assertion(sym != NULL && is_function_symbol(sym));
  tip = sym->variant.routine.instance_ptr;
  check_assertion(tip != NULL);
  return is_inline_template_function(tip);
}  /* rout_is_inline_template_function */


static a_boolean f_is_static_or_inline_template_entity
					(a_template_instance_ptr tip)
/*
Determines whether a template instance pointer refers to a function that
is static or inline (i.e., is not an external function).  Functions
within unnamed namespaces are treated as having internal linkage for
purposes of this test.  Also determines whether a template static
data member is a member of an unnamed namespace.
*/
{
  a_boolean     result = FALSE;
  a_symbol_ptr	sym = tip->instance_sym;

  if (is_inline_template_function(tip)) {
    result = TRUE;
  } else if (sym->kind != (a_symbol_kind)sk_static_data_member &&
             (sym->variant.routine.ptr->storage_class ==
                                                 (a_storage_class)sc_static ||
              is_or_contains_unnamed_namespace_type(
                                            sym->variant.routine.ptr->type))) {
    /* Return TRUE if the function is marked as static, or if the routine type
       contains a type from an unnamed namespace. */
    result = TRUE;
  } else {
    /* A function or static data member -- check if it is a member of an
       unnamed namespace. */
    a_namespace_ptr			parent_nsp;
    a_namespace_symbol_supplement_ptr	parent_nssp;
    parent_nsp = parent_namespace_for_symbol(sym);
    if (parent_nsp != NULL) {
      parent_nssp = symbol_supplement_for_namespace(parent_nsp);
      if (parent_nssp->within_unnamed_namespace) {
        /* A member of an unnamed namespace (or a class or namespace within
           an unnamed namespace).  Treat this as if it had internal linkage. */
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Save the result.  If this flag is set, its value is used instead of
     calling this routine again. */
  tip->is_static_or_inline = result;
  return result;
}  /* f_is_static_or_inline_template_entity */


/*
Macro that calls f_is_static_or_inline_template_entity.  If we have already
determined that the entity is static or inline, the call is suppressed and the
previously computed value is returned.

Note that the routine can be called more than once if the flag is FALSE.
This is needed because a routine could be declared and later declared inline.
*/
#define is_static_or_inline_template_entity(tip)			\
  ((tip)->is_static_or_inline						\
		? (tip)->is_static_or_inline				\
		: f_is_static_or_inline_template_entity(tip))


/* Forward declaration */
static a_boolean exported_definition_is_available(
						a_template_instance_ptr	tip);


#if !INSTANTIATION_BY_IMPLICIT_INCLUSION
/*ARGSUSED*/ /* <-- implicit_inclusion_okay is not used if no implicit
                 inclusion. */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
static a_boolean should_be_instantiated(
			a_template_instance_ptr tip,
			a_boolean		implicit_inclusion_okay)
/*
Determines whether this template instance needs an instantiation and
generates any errors caused by conflicting instantiation information
such as instantiating a template for which no body was supplied.
implicit_inclusion_okay is TRUE if the compiler should attempt to include
a template definition file to provide definitions for externally linked
template entities.
*/
{
  a_boolean	result = TRUE;
  a_boolean	specialized;
  a_boolean	specialization_defined;
  a_boolean	template_def;

  if (!tip->already_instantiated && !tip->explicit_do_not_instantiate &&
      (tip->explicit_instantiation ||
       ((tip->instantiation_required || instantiation_mode == tim_all) &&
        (instantiation_mode != tim_none ||
         is_static_or_inline_template_entity(tip))))) {
    /* For error checking purposes, find out if a specific definition
       exists and whether a body exists for the template definition. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      a_variable_ptr	vp;
      vp = tip->instance_sym->variant.static_data_member.variable;
      specialized = vp->is_specialized;
      specialization_defined = tip->instance_sym->defined;
      template_def = tip->template_sym->defined ||
                     exported_definition_is_available(tip);
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      if (!template_def && !specialized && !tip->suppress_instantiation &&
          implicit_inclusion_okay && implicit_template_inclusion_mode) {
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
      template_def = cache_for_template(tssp)->tokens.first_token != NULL ||
                     exported_definition_is_available(tip);
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      if (!template_def && !specialized && !tip->suppress_instantiation &&
          implicit_inclusion_okay && implicit_template_inclusion_mode) {
        /* If a template definition is not present, attempt to include a
           source file that will provide the definition.  Then check
           again to see if a template definition is present. */
        do_implicit_include_if_needed(tip);
        template_def = cache_for_template(tssp)->tokens.first_token != NULL;
      }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    }  /* if */
    /* The template should be instantiated if the entity is not specialized
       and a template definition is available (and the suppress instantiation
       flag is not set). */
    result = !specialized && !tip->suppress_instantiation && template_def;
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


static a_boolean f_entity_can_be_instantiated(
			a_template_instance_ptr	tip,
			a_boolean		implicit_inclusion_okay)
/*
Determines whether this compilation is capable of generating an
instantiation of a given template instance.

implicit_inclusion_okay is TRUE if the compiler should attempt to include
a template definition file to provide definitions for externally linked
template entities.
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
    if (!template_def && !specialized && export_template_allowed) {
      /* When exported templates are being used, look for an exported
         definition of this template */
      template_def = exported_definition_is_available(tip);
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (!template_def && !specialized && !tip->suppress_instantiation &&
        !tip->explicit_do_not_instantiate &&
        !tip->already_instantiated && implicit_template_inclusion_mode &&
        implicit_inclusion_okay) {
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
    if (!template_def && !specialized && export_template_allowed) {
      /* When exported templates are being used, look for an exported
         definition of this template */
      template_def = exported_definition_is_available(tip);
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (!template_def && !specialized && !tip->suppress_instantiation &&
        !tip->already_instantiated && implicit_template_inclusion_mode &&
        implicit_inclusion_okay) {
      /* If a template definition is not present, attempt to include a
         source file that will provide the definition.  Then check
         again to see if a template definition is present. */
      do_implicit_include_if_needed(tip);
      template_def = cache_for_template(tssp)->tokens.first_token != NULL;
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  }  /* if */
  result = template_def && !specialized && !tip->already_instantiated &&
           !tip->suppress_instantiation && !tip->explicit_do_not_instantiate;
  tip->can_be_instantiated = result;
  return result;
}  /* f_entity_can_be_instantiated */


/*
Macro that calls f_entity_can_be_instantiated.  If we have already determined
that the entity can be instantiated, the call is suppressed and the
previously computed value is returned.
*/
#define entity_can_be_instantiated(tip, implicit_inclusion_okay)	\
  ((tip)->can_be_instantiated						\
		? (tip)->can_be_instantiated				\
		: f_entity_can_be_instantiated(tip, implicit_inclusion_okay))


static void load_exported_template_file(an_exported_template_file_ptr	etfp)
/*
Compile the translation unit described by etfp for the purpose of defining
the exported templates in that file.
*/
{
  a_translation_unit_ptr	saved_tup = curr_translation_unit;

  /* Compile the specified translation unit. */
  /* FIXME - need to handle directory name, include search paths, etc. */
  /* Pop the file scope of the current translation unit. */
  pop_scope();
  process_translation_unit(etfp->source_file_name, /*is_primary=*/FALSE);
  /* Switch back to the previous translation unit. */
  switch_translation_unit(saved_tup);
  /* Reactivate the file scope of the original translation unit. */
  push_file_scope(/*is_reactivation=*/TRUE);
}  /* load_exported_template_file */


static void ensure_exported_template_file_is_loaded(
						a_template_instance_ptr	tip)
/*
We are about to instantiate the template specified by tip, which is an
instance of an exported template.  If the translation unit containing that
template has not yet been loaded, load it now.
*/
{
  an_exported_template_file_ptr	etfp;

  etfp = tip->exported_template_file;
  check_assertion(etfp != NULL);
  if (etfp->translation_unit == NULL) {
    /* The translation unit has not been loaded yet.  Load it now. */
    load_exported_template_file(etfp);
  }  /* if */
}  /* ensure_exported_template_file_is_loaded */


static void instantiate_entity(a_template_instance_ptr tip)
/*
Call the appropriate routine to instantiate the function or static
data member specified by tip.
*/
{
  /* The instantiation process may rescan various things and invalidate the
     current token positions as a result.  Save these positions so that they
     may be restored when we are done. */
  a_source_position saved_pos_curr_token, saved_error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  saved_pos_curr_token = pos_curr_token;
  saved_error_position = error_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  saved_curr_construct_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (tip->exported_template_file != NULL) {
    /* If the template was defined in an exported template file, make sure
       that file is loaded as a translation unit. */
    ensure_exported_template_file_is_loaded(tip);
  }  /* if */
  if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
    /* Static data member definition. */
    define_template_static_data_member(tip);
  } else {
    /* Function instantiation.  The number of simultaneous function
       instantiations is limited to limit the amount of memory used by
       memory regions for functions that are in the process of being
       defined.  This is done because, while a function is being instantiated,
       it generally consumes HOST_ALLOCATION_INCREMENT bytes of storage. */
    if (num_total_pending_instantiations < MAX_TOTAL_PENDING_INSTANTIATIONS) {
      num_total_pending_instantiations++;
      instantiate_template_function(tip);
      num_total_pending_instantiations--;
    }  /* if */
  }  /* if */
  error_position = saved_error_position;
  pos_curr_token = saved_pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* instantiate_entity */


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
  ilp->in_request_file = FALSE;
  ilp->in_definition_list_file = FALSE;
  return ilp;
}  /* alloc_instance_lookup_entry */


static an_instance_lookup_entry_ptr find_instance(char		*name,
				                  a_boolean	add)
/*
Find an entry in the instance lookup table with the specified name.  If "add"
is TRUE, add the name to the list if an entry does not already exist.  This
is used to build a list of instances found in the instantiation request file
and later check whether a specified name was included in that list.
*/
{
  register unsigned            hash_value = 0;
  register unsigned char       *ptr;
  an_instance_lookup_entry_ptr ilp    = NULL;
  int                          bucket_number;
  int			       length;

  length = strlen(name);
  /* Hash the symbol's identifier.  This involves taking the identifier's
     first 3, last 3, and middle 3 characters.  Of course, if the identifier
     has 9 or fewer characters, take the entire identifier. */
  ptr = (unsigned char *)name;
  if (length > 9) {
    hash_value = *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + (length >> 1) - 1;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + length - 3;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
  } else {
    register int a;
    for (a = 0; a < length; a++) {
      hash_value = (hash_value * HASH_FACTOR) + *ptr++;
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
     allocate a template instance entry header for it. */
  if (add) {
    ilp = alloc_instance_lookup_entry();
    /* Link the new entry onto the front of the appropriate bucket of the
       table. */
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


static char *read_line_from_file(FILE *f_file)
/*
Reads a line of input from the file specified by f_file.  Returns a pointer
to a buffer containing the line read, or NULL at end-of-file.  The pointer
returned points to a static buffer that is reused for each call.
*/
{
  int      ch;
  char     *result;

  reset_text_buffer(file_read_buffer);
  while (ch = getc(f_file), ch != EOF && ch != '\n') {
    add_char_to_text_buffer(file_read_buffer, (char)ch);
  }  /* while */
  /* Determine whether to return end-of-file (NULL). */
  result = file_read_buffer->buffer;
  if (ch == EOF && file_read_buffer->size == 0) result = NULL;
  /* Terminate string with a null character. */
  add_char_to_text_buffer(file_read_buffer, '\0');
  return (result);
}  /* read_line_from_file */


static a_boolean open_instantiation_request_file(void)
/*
Open the instantiation request file associated with the primary source
file.  Return TRUE if the file was successfully opened.
*/
{
  f_instantiation_request = NULL;
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    /* Only open the file if the input is coming from a file. */
    check_assertion(instantiation_request_file_name != NULL);
    f_instantiation_request = fopen(instantiation_request_file_name, "r");
  }  /* if */
  return f_instantiation_request != NULL;
}  /* open_instantiation_request_file */


static a_boolean read_instantiation_request_file(void)
/*
Read the list of names from the instantiation request file and
enter the names into a hash table.  Returns TRUE if any entries
were entered in the hash table; otherwise returns FALSE.
*/
{
  char				*line;
  a_boolean			result = FALSE;
  int				i;

  if (open_instantiation_request_file()) {
    /* If the file does not exist, the open routine will return FALSE. */
    /* Skip over initial lines of the instantiation request file
       that don't contain instantiation entries. */
    for (i = 1; i <= INSTANTIATION_REQUEST_LINES_RESERVED; ++i) {
      /* Read and discard the line. */
      (void)read_line_from_file(f_instantiation_request);
    }  /* if */
    while ((line = read_line_from_file(f_instantiation_request)) != NULL) {
      an_instance_lookup_entry_ptr	ilp;
      ilp = find_instance(line, /*add=*/TRUE);
      ilp->in_request_file = TRUE;
      result = TRUE;
    }  /* while */
    (void)fclose(f_instantiation_request);
  }  /* if */
  return result;
}  /* read_instantiation_request_file */


static a_boolean read_definition_list_file(void)
/*
If a definition list file was specified, read the entries from the
definition list file and enter them into the instance table.
Return TRUE if any entries were read.
*/
{
  FILE		*f_definition_list;
  char		*line;
  a_boolean	result = FALSE;

  if (definition_list_file_name != NULL && generate_template_files()) {
    /* Open the definition list file. */
    f_definition_list = fopen(definition_list_file_name, "r");
    if (f_definition_list == NULL) {
      str_catastrophe(ec_cannot_open_definition_list_file,
                      definition_list_file_name);
    }  /* if */
    /* Read the list of instances from the definition list file and
       create an entry in the instance lookup table that is flagged
       as being in the definition list file. */
    while ((line = read_line_from_file(f_definition_list)) != NULL) {
      an_instance_lookup_entry_ptr	ilp;
      ilp = find_instance(line, /*add=*/TRUE);
      ilp->in_definition_list_file = TRUE;
      result = TRUE;
    }  /* while */
    (void)fclose(f_definition_list);
  }  /* if */
  return result;
}  /* read_definition_list_file */


static a_boolean init_auto_instantiation_information(void)
/*
Determine whether there are any instantiations that should be performed
as part of the processing associated with this translation unit.  Return
a flag that is TRUE if there are any such instantiations.  The default
version of this routine simply opens and reads the instantiation 
request file.
*/
{
  a_boolean	result;

  result = read_instantiation_request_file();
  return result;
}  /* init_auto_instantiation_information */


static char *get_mangled_name_for_symbol(a_symbol_ptr	sym)
/*
Return the mangled name of the variable, routine, or template specified
by "sym".
*/
{
  char	*name;

  if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    a_variable_ptr	variable;
    variable = sym->variant.static_data_member.variable;
    name = get_mangled_static_data_member_name(variable);
  } else if (is_function_symbol(sym)) {
    a_routine_ptr	routine;
    routine = sym->variant.routine.ptr;
    name = get_mangled_function_name(routine);
  } else if (sym->kind == (a_symbol_kind)sk_function_template) {
    a_routine_ptr			routine;
    a_template_symbol_supplement_ptr	tssp;
    tssp = sym->variant.template_info;
    routine = tssp->variant.function.routine;
    name = get_mangled_function_name(routine);
  } else {
    unexpected_condition_str("get_mangled_name_for_symbol: bad kind");
  }  /* if */
  return name;
}  /* get_mangled_name_for_symbol */


static void check_if_present_in_request_file(
			a_template_instance_ptr		tip,
			a_boolean			*instantiate,
			a_boolean			*not_defined_elsewhere)
/*
See if the specified instantiation is one that is included in the
instantiation request file.  Return TRUE in instantiated if it is present
Return TRUE in not_defined_elsewhere if the entity is known to to
be defined in some other part of the program (e.g. it is not in an
object or library with which this file is being linked).
*/
{
  char				*name;
  an_instance_lookup_entry_ptr	ilp;

  *not_defined_elsewhere = FALSE;
  *instantiate = FALSE;
  name = get_mangled_name_for_symbol(tip->instance_sym);
  ilp = find_instance(name, /*add=*/FALSE);
  if (ilp != NULL && ilp->in_request_file) {
    /* The entity was named in the instantiation request file. */
    *instantiate = TRUE;
  } else if (definition_list_file_name != NULL &&
             tip->instantiation_required && !tip->already_instantiated &&
             (ilp == NULL || !ilp->in_definition_list_file)) {
    /* The entity was not in the instantiation list file, but neither is
       it in the list of already-defined entities.  Instantiate it here. */
    *not_defined_elsewhere = TRUE;
  }  /* if  */
}  /* check_if_present_in_request_file */


static void create_or_remove_instantiation_request_file(void)
/*
If this compilation made use of any entities that could be instantiated,
create an instantiation request file.  If this compilation did not
make use of any entities that could be instantiated, remove the .ii file
if one already exists.
*/
{
  FILE		*f_ii_file = NULL;

  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    /* The name of the instantiation request file should have already
       been determined when the file was opened as part of automatic
       instantiation processing for this file. */
    check_assertion_str2(instantiation_request_file_name != NULL,
                         "create_or_remove_instantiation_request_file:",
                         "file name is NULL");
    /* Only create the file if the input is coming from a file.  Note
       that the file will have been closed after all input was read so
       it must be reopened now. */
    f_ii_file = fopen(instantiation_request_file_name, "r");
    if (any_instantiations_required) {
      if (!use_template_info_file) {
        /* If the file does not exist, create it.  The file is only
           created when not using a template information file, because
           the existence of the file is used as a signal to the driver. */
        if (f_ii_file == NULL) {
          f_ii_file = fopen(instantiation_request_file_name, "a");
          if (f_ii_file == NULL) {
            str_catastrophe(ec_cannot_create_instantiation_request_file,
                            instantiation_request_file_name);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* No instantiation request needed.  Delete the file if it
         already exists.  This is done even when using a template
         information file so that unused .ii files will be cleaned up. */
      if (f_ii_file != NULL) {
        if (fclose(f_ii_file)) {
          /* Close the file before removing it.  This is necessary on
             some operating systems. */
          str_catastrophe(ec_file_write_error, "instantiation request file");
        }  /* if */
        delete_file(instantiation_request_file_name);
        f_ii_file = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  if (f_ii_file != NULL) {
    if (fclose(f_ii_file)) {
      str_catastrophe(ec_file_write_error, "instantiation request file");
    }  /* if */
  }  /* if */
}  /* create_or_remove_instantiation_request_file */


static void check_if_entity_should_be_automatically_instantiated(
					a_template_instance_ptr tip)
/*
Determine whether the entity specified by tip should be instantiated as part
of the processing of this translation unit.  The default version of this
routine simply checks whether the specified entity was named in the
instantiation request file.
*/
{
  if (request_file_check_needed) {
    a_boolean	instantiate;
    a_boolean	not_defined_elsewhere;
    a_boolean	add_to_request_file = FALSE;
    check_if_present_in_request_file(tip, &instantiate,
                                     &not_defined_elsewhere);
    if (not_defined_elsewhere) {
      /* If the entity is known not to be defined elsewhere, see if an
         instantiation should be performed here.  Only external entities
         can be added to a request file. */
      if (tip->instantiation_required && !tip->already_instantiated &&
          !is_static_or_inline_template_entity(tip)) {
        add_to_request_file = TRUE;
      }  /* if */
    }  /* if */
    if (instantiate || add_to_request_file) {
      /* An instantiation should be done here. */
      tip->automatically_instantiated = TRUE;
    }  /* if */
    if (add_to_request_file) {
      /* The instantiation was not requested by the prelinker, but was
         "adopted" by this translation unit because it is known not to
         be defined anywhere else. */
      tip->add_to_request_file = TRUE;
    }  /* if */
  }  /* if */
}  /* check_if_entity_should_be_automatically_instantiated */


void wrapup_auto_instantiation_information(void)
/*
Do any processing that is needed to finalize the mechanism used to
handle tracking of automatic instantiation information.  The default
version of this routine creates or removes the instantiation
request file, and closes or removed the template information file.
If the translation unit contains templates the files are created,
otherwise they are removed.
*/
{
  /* Create or remove the instantiation request file if necessary. */
  if (generate_template_files()) {
    /* The instantiation request file is not affected when doing
       preprocessing only, or not running the back end.  By not calling
       this routine we keep the old version if one was present and don't
       create one if one did not already exist.  This preserves the
       instantiation list if, for example, there were errors during the
       compilation. */
    if (total_errors == 0) {
      create_or_remove_instantiation_request_file();
    }  /* if */
    /* The template information file (if being used) and the exported
       template file, are closed or removed even in the presence of errors,
       but not if doing preprocessing only or suppressing the back end. */
    if (use_template_info_file) {
      close_or_remove_template_info_file();
    }  /* if */
    close_or_remove_exported_template_file();
  }  /* if */
  check_assertion_str2(f_template_info == NULL,
                       "wrapup_auto_instantiation_information:",
                       "template info file not closed");
}  /* wrapup_auto_instantiation_information */


static void do_automatic_instantiation_of_entity(a_template_instance_ptr tip)
/*
Do the automatic instantiation of the function or static data member
specified by tip.
*/
{
  a_template_instantiation_mode	saved_instantiation_mode;
  /* Set the instantiation mode to tim_none.  This is done to ensure that
     only the instantiations explicitly requested in the list file are
     performed.  We don't want a mode like "used" or "all" to cause
     other instantiations to happen as a consequence of the requested
     instantiations that are performed. */
  saved_instantiation_mode = instantiation_mode;
  instantiation_mode = tim_none;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Automatic instantiation processing for:\n");
    db_symbol(tip->instance_sym, "", 0);
  }  /* if */
#endif /* DEBUG */
  /* Do the instantiation. */
  instantiate_entity(tip);
  if (tip->add_to_request_file) {
    /* This flag is set once we know we have instantiated something that
       is to be added to the request file.  This triggers the generation
       of a list of added entities at the end of the compilation. */
    any_instantiated_entities_added_to_request_file = TRUE;
  }  /* if */
  /* Restore the original instantiation mode.  This is needed because it
     is used later on in the front end wrapup process when assigning
     linkage class members. */
  instantiation_mode = saved_instantiation_mode;
}  /* do_automatic_instantiation_of_entity */


static an_exported_template_file_ptr alloc_exported_template_file(void)
/*
Allocate an exported template file entry, initialize it, and return a pointer
to it.
*/
{
  an_exported_template_file_ptr	etfp;

  etfp = alloc_fe_of_type(an_exported_template_file);
#if DEBUG
  num_exported_template_files_allocated = 0;
#endif /* DEBUG */
  etfp->directory_name = NULL;
  etfp->source_file_name = NULL;
  etfp->translation_unit = NULL;
  return etfp;
}  /* alloc_exported_template_file */


static a_template_lookup_entry_ptr alloc_template_lookup_entry(void)
/*
Allocate a template lookup entry, initialize it, and return a pointer
to it.
*/
{
  a_template_lookup_entry_ptr	tlp;

  tlp = alloc_fe_of_type(a_template_lookup_entry);
#if DEBUG
  num_template_lookup_entries_allocated = 0;
#endif /* DEBUG */
  tlp->next = NULL;
  tlp->name = NULL;
  tlp->exported_template_file = NULL;
  return tlp;
}  /* alloc_template_lookup_entry */


static a_template_lookup_entry_ptr find_exported_template(char		*name,
					                  a_boolean	add)
/*
Find an entry in the template lookup table with the specified name.
If "add" is TRUE add the name to the list if an entry does not already
exist.  This is used to find the definition of an exported template.
*/
{
  register unsigned            hash_value = 0;
  register unsigned char       *ptr;
  a_template_lookup_entry_ptr  tlp    = NULL;
  int                          bucket_number;
  int			       length;

  length = strlen(name);
  /* Hash the symbol's identifier.  This involves taking the identifier's
     first 3, last 3, and middle 3 characters.  Of course, if the identifier
     has 9 or fewer characters, take the entire identifier. */
  ptr = (unsigned char *)name;
  if (length > 9) {
    hash_value = *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + (length >> 1) - 1;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + length - 3;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
  } else {
    register int a;
    for (a = 0; a < length; a++) {
      hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    }  /* for */
  }  /* if */
  /* Look in the symbol bucket saving the position in case this symbol needs
     to be added. */
  bucket_number = hash_value % TEMPLATE_LOOKUP_TABLE_SIZE;
  if ((tlp = template_lookup_table[bucket_number]) != NULL) {
    do {
      if (strcmp(name, tlp->name) == 0) {
        /* We have a match. */
        goto symbol_found;
      }  /* if */
    } while ((tlp = tlp->next) != NULL);
  }  /* if */

  /* Exiting this loop indicates that the symbol does not exist in the table;
     allocate a template lookup entry. */
  if (add) {
    tlp = alloc_template_lookup_entry();
    /* Link the new header onto the front of the appropriate bucket of the
       table. */
    tlp->next = template_lookup_table[bucket_number];
    template_lookup_table[bucket_number] = tlp;
    /* Allocate space for the name (including a null terminator) and make a
       copy of the name. */
    tlp->name = (char *)alloc_fe((sizeof_t)length + 1);
    strcpy(tlp->name, name);
  }  /* if */

symbol_found:
  return tlp;
}  /* find_exported_template */


static a_boolean exported_definition_is_available(
						a_template_instance_ptr	tip)
/*
Determine whether the template, of which tip is an instance, is an
exported template whose definition is available (i.e., that can be
instantiated).  If so, make a record of the file in which the exported
definition was found.  Return TRUE if an exported definition was found,
FALSE otherwise.

The caller is responsible for making sure that this is not called for
a specialized instance.
*/
{
  a_boolean				result = FALSE;
  a_template_symbol_supplement_ptr	tssp;

  tssp = template_supplement_for_symbol(tip->template_sym);
  if (!export_template_allowed) {
    /* We are not doing export processing. */
    result = FALSE;
  } else if (tip->exported_template_file != NULL) {
    /* If we already found the exported template file, skip the remaining
       processing. */
    result = TRUE;
  } else if (tssp->il_template_entry->is_exported) {
    /* The template is exported.  See if a definition is available. */
    char			*name;
    a_template_lookup_entry_ptr	tlp;
    /* Look up the mangled name of the template to see if a definition was
       found. */
    name = get_mangled_name_for_symbol(tip->template_sym);
    tlp = find_exported_template(name, /*add=*/FALSE);
    if (tlp != NULL) {
      /* An exported definition was found.  Record information about the file
         where it was found in the template instance entry. */
      result = TRUE;
      tip->exported_template_file = tlp->exported_template_file;
    }  /* if */
  }  /* if */
  return result;
}  /* exported_definition_is_available */


static FILE *open_exported_template_file_for_input(
				char				*file_name,
				a_directory_name_entry_ptr	dnep)
/*
Open the specified exported template file to be read.  The file is in the
directory specified by dnep.  This is used when reading exported template
files to find template definitions, not when generating a file from this
compilation.

Returns a pointer to the FILE structure for the file.
*/
{
  FILE			*f_file;
  a_text_buffer_ptr	file_name_buffer;
  char			*full_name;

  file_name_buffer = combine_dir_and_file_name(dnep->dir_name, file_name,
                                               (a_text_buffer_ptr)NULL);
  full_name = file_name_buffer->buffer;
#if DEBUG
  if (db_flag_is_set("export")) {
    fprintf(f_debug, "Opening export template file: %s\n", full_name);
  }  /* if */
#endif /* DEBUG */
  f_file = fopen(full_name, "r");
  if (f_file == NULL) {
    str_catastrophe(ec_cannot_open_exported_template_file, full_name);
  }  /* if */
  return f_file;
}  /* open_exported_template_file_for_input */


static void read_exported_template_file(
				char				*file_name,
				a_directory_name_entry_ptr	dnep)
/*
Read the exported template file specified by file_name, found in the
directory indicated by dnep.  Create lookup table entries for the
templates defined in the file.
*/
{
  FILE				*f_file;
  char				*line;
  an_exported_template_file_ptr	etfp;

  /* Create an entry that describes this exported template file. */
  etfp = alloc_exported_template_file();
  /* Make a copy of the directory name in the front end memory region. */
  etfp->directory_name = copy_string_to_region(FRONT_END_REGION_NUMBER,
                                               dnep->dir_name);
  f_file = open_exported_template_file_for_input(file_name, dnep);
  while ((line = read_line_from_file(f_file)) != NULL) {
    if (strncmp(line, "fnm:", 4) == 0) {
      char	*name = &line[4];
      etfp->source_file_name = copy_string_to_region(
                                           FRONT_END_REGION_NUMBER, name);
    } else if (strncmp(line, "tnm:", 4) == 0) {
      a_template_lookup_entry_ptr	tlp;
      char				*name = &line[4];
      tlp = find_exported_template(name, /*add=*/TRUE);
      if (tlp->exported_template_file == NULL) {
        /* Only record the first file that is found that defines the
           template. */
        tlp->exported_template_file = etfp;

      }  /* if */
    } else {
      unexpected_condition_str("read_exported_template_file: bad line kind");
    }  /* if */
  }  /* while */
  /* Close the file. */
  (void)fclose(f_file);
}  /* read_exported_template_file */


static void find_exported_template_files(void)
/*
Go through the template search path and read the exported template files
from each directory.  Build a lookup table so that templates from this
compilation can be looked up to find the corresponding definition.
*/
{
  a_directory_name_entry_ptr	dnep;
  a_boolean			first;

  db_enter(2, "find_exported_template_files");
  for (dnep = template_search_path; dnep != NULL; dnep = dnep->next) {
    for (first = TRUE;;first = FALSE) {
      char	*file_name;
      file_name = get_file_name_from_dir(first, dnep->dir_name,
                                         EXPORTED_TEMPLATE_FILE_SUFFIX,
                                         current_directory_name);
      /* A NULL pointer indicates there are no more matching file names. */
      if (file_name == NULL) break;
      /* Read the contents of the file. */
      read_exported_template_file(file_name, dnep);
    }  /* for */
  }  /* for */
  db_exit();
}  /* find_exported_template_files */


#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */


static void update_instantiation_required_flag(
					a_template_instance_ptr tip,
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

  db_enter(5, "update_instantiation_required_flag");
  /* Inline functions are not treated differently for instantiation purposes
     in Microsoft mode. */
  if (microsoft_bugs) defer_inline = TRUE;
  sym = tip->instance_sym;
  tssp = template_supplement_for_symbol(tip->template_sym);
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("uirf")) {
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
    if (is_function_symbol(sym) &&
        too_many_unused_instantiations(tip->template_sym, tssp)) {
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
  } else if (pending_class_definitions != 0 ||
             defer_inline_function_fixup_and_instantiations != 0) {
    /* A class definition is in progress, or if only
       defer_inline_function_fixup_and_instantiations is set, the default
       argument fixup after a class definition.  Any nonclass instantiations
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
      tip->referencing_namespace = determine_referencing_namespace();
    }  /* if */
    if (!defer_inline && is_inline_template_function(tip)) {
      if (!tip->already_instantiated &&
          should_be_instantiated(tip, /*implicit_inclusion_okay=*/FALSE)) {
        /* Inline (member or nonmember) functions are instantiated at the
           point of first use, in case the back end requires the function
           body immediately to perform inlining. */
        instantiate_entity(tip);
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
            should_be_instantiated(tip, /*implicit_inclusion_okay=*/FALSE)) {
          /* Implicit inclusion is not done for "on the fly" instantiations
             because the includes cannot be processed in the middle of
	     the instantiation of another function.  The entry will be put
	     on the instantiation required list and instantiated later in
             instantiation_wrapup. */
          instantiate_entity(tip);
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
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (in_instantiation_wrapup) {
    /* This entity is being added after we've already gone through the
       instantiations list to look for entities that must be instantiated.
       Do the check for this entity now. */
    check_if_entity_should_be_automatically_instantiated(tip);
    /* See if the entity should be instantiated as a result of an
       assignment by the automatic instantiation mechanism. */
    if (value &&
        entity_can_be_instantiated(tip, /*implicit_inclusion_okay=*/FALSE) &&
        tip->automatically_instantiated && !tip->already_instantiated) {
      /* Implicit inclusion is not done for "on the fly" instantiations
         because the includes cannot be processed in the middle of
         the instantiation of another function.  The entry will be put
	 on the instantiation required list and instantiated later in
         instantiation_wrapup. */
      do_automatic_instantiation_of_entity(tip);
    }  /* if */
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  db_exit();
}  /* update_instantiation_required_flag */


void set_instance_required(a_symbol_ptr	sym,
			   a_boolean	value,
			   a_boolean	defer_inline)
/*
Updates the instantiation required flag in the template instance and/or
the inline_instance_required field in the routine entry associated with sym.
The symbol passed in must be for a function or a static data member,
but it need not be for a template entity or an inline function.
"value" is the value to which the field(s) are to be set.  defer_inline
is passed to update_instantiation_required_flag.  Does nothing if called
in C mode.
*/
{
  /* Do nothing in C mode. */
  if (!C_mode()) {
    a_template_instance_ptr	tip;

#if DEBUG
    if (db_flag_is_set("set_instance_required")) {
      fprintf(f_debug, "Setting instance required for ");
      db_symbol_name(sym);
      fprintf(f_debug, " to %s\n", value ? "true" : "false");
    }  /* if */
#endif /* DEBUG */
    if (sym->kind == (a_symbol_kind)sk_static_data_member) {
      tip = sym->variant.static_data_member.instance_ptr;
    } else {
      check_assertion(sym->kind == (a_symbol_kind)sk_member_function ||
                      sym->kind == (a_symbol_kind)sk_routine);
      tip = sym->variant.routine.instance_ptr;
    }  /* if */
    if (tip != NULL) {
      update_instantiation_required_flag(tip, value, defer_inline);
    }  /* if */
#if INSTANTIATE_EXTERN_INLINE
    /* The inline instance required flag is set for all functions (even
       those that are not inline).  A function can be declared inline after
       it has been called. */
    if (sym->kind == (a_symbol_kind)sk_member_function ||
        sym->kind == (a_symbol_kind)sk_routine) {
      a_routine_ptr	rp;
      rp = sym->variant.routine.ptr;
      rp->inline_instance_required = value;
    }  /* if */
#endif /* INSTANTIATE_EXTERN_INLINE */
  }  /* if */
}  /* set_instance_required */


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

  /* The processing of this list may result in additional deferred
     instantiations that will get added to the end of the list.  A
     flag is used to make sure that the list is not processed during
     potential recursive calls of this routine. */
  if (!deferred_instantiations_in_process) {
    deferred_instantiations_in_process = TRUE;
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
    deferred_instantiations_in_process = FALSE;
  }  /* if */
}  /* process_deferred_instantiation_requests */


#if AUTOMATIC_TEMPLATE_INSTANTIATION

#if DO_IL_LOWERING

static void create_instantiation_flag_variables(
				a_source_correspondence *scp,
				a_boolean		instance_required,
				a_boolean		do_not_instantiate,
				a_boolean		can_be_instantiated)
/*
For automatic instantiation, generate a variable or variables with
names that encode instantiation information.  Note that this is done
only if IL lowering is done.
*/
{
  if (instance_required) {
    /* This routine or variable is template-based. */
    make_instantiation_info_var("__TIR__", scp);
  }  /* if */
  if (do_not_instantiate) {
    /* This routine or variable cannot be instantiated. */
    make_instantiation_info_var("__DNI__", scp);
  }  /* if */
  if (can_be_instantiated) {
    /* This routine or variable can be instantiated. */
    make_instantiation_info_var("__CBI__", scp);
  }  /* if */
}  /* create_instantiation_flag_variables */

#endif /* DO_IL_LOWERING */


static void write_instantiation_flags_to_template_info_file(
				char			*name,
				a_boolean		instance_required,
				a_boolean		do_not_instantiate,
				a_boolean		can_be_instantiated)
/*
For automatic instantiation, write an entry to the template information
file that specifies the instantiation flags associated with the entity.
*/
{
  char	flags[4];
  char	*flag_ptr = flags;

  if (instance_required) {
    /* This routine or variable is template-based. */
    *flag_ptr++ = 'T';
  }  /* if */
  if (do_not_instantiate) {
    /* This routine or variable cannot be instantiated. */
    *flag_ptr++ = 'D';
  }  /* if */
  if (can_be_instantiated) {
    /* This routine or variable can be instantiated. */
    *flag_ptr++ = 'C';
  }  /* if */
  *flag_ptr = '\0';
  if (flags[0] != '\0') {
    /* Only write the line if at least one flag is set. */
    write_to_template_info_file(tilt_instantiation_flag, name, flags);
  }  /* if */
}  /* write_instantiation_flags_to_template_info_file */


static void add_entities_to_request_file(void)
/*
Go through the instantiations required list to find any entities
that should be added to the request file.  The list is written
to the definition list file.  The prelinker is responsible
for adding the entries to the actual instantiation request file.
*/
{
  a_template_instance_ptr	tip;
  FILE				*f_definition_list;

  check_assertion(definition_list_file_name != NULL);
  f_definition_list = fopen(definition_list_file_name, "w");
  if (f_definition_list == NULL) {
    str_catastrophe(ec_cannot_open_definition_list_file,
                    definition_list_file_name);
  }  /* if */
  /* Write a special string to the start of the list of entities to
     be added to the request file.  This is used by the prelinker to
     verify that the file was created by the front end, and is not
     a leftover definition list file created by the prelinker. */
  fputs(":add:\n", f_definition_list);
  for (tip = instantiations_required; tip != NULL;
       tip = tip->next_in_instantiation_list) {
    /* Make sure the entity was actually instantiated before adding it to
       the request file.  It is possible for the add_to_request_file
       flag to be set for entities that cannot be instantiated. */
    if (tip->add_to_request_file && tip->already_instantiated) {
      char	*name;
      if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
        a_variable_ptr	vp;
        vp = tip->instance_sym->variant.static_data_member.variable;
        name = vp->source_corresp.name;
      } else {
        a_routine_ptr	rp;
        rp = tip->instance_sym->variant.routine.ptr;
        name = rp->source_corresp.name;
      }  /* if */
      fputs(name, f_definition_list);
      fputs("\n", f_definition_list);
    }  /* if */
  }  /* for */
  if (fclose(f_definition_list)) {
    str_catastrophe(ec_file_write_error, "definition list file");
  }  /* if */
}  /* add_entities_to_request_file */

#if ONE_INSTANTIATION_PER_OBJECT

static void write_instantiation_file_name_to_template_info_file(
					a_source_correspondence	*scp)
/*
Write the instantiation file name for "scp" to the template information
file.
*/
{
  char			*file_name;
  /* Generate a file name based on the mangled name of the entity. */
  file_name = generate_instantiation_output_file_name(scp->name);
  /* Write the generated file name to the template info file. */
  write_to_template_info_file(tilt_instantiation_file_name,
                              file_name, (char*)NULL);
}  /* write_instantiation_file_name_to_template_info_file */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

void update_auto_instantiation_flags(void)
/*
Go through the instantiations_required list and set the fields in the
variable and routine entries that are used to pass information to the
link-time instantiation processor.  The "instance required", "can instantiate"
and "do not instantiate" flags are set here.
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
    a_boolean				can_be_instantiated;
    a_boolean				do_not_instantiate;
    a_boolean				instance_required;
    a_boolean				is_static_data_member;

    /* Skip non-external functions.  Note that this tests the flag in
       the instance entry instead of calling the function
       is_static_or_inline_emplate_entity.  This is necessary because that
       function cannot be called successfully after the file scope has
       been lowered. */
    if (tip->is_static_or_inline) continue;
    /* Get a pointer to the IL entry to be processed. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      is_static_data_member = TRUE;
      variable = instance_sym->variant.static_data_member.variable;
    } else {
      is_static_data_member = FALSE;
      routine = instance_sym->variant.routine.ptr;
    }  /* if */
    can_be_instantiated = tip->already_instantiated ||
                  entity_can_be_instantiated(tip,
                                             /*implicit_inclusion_okay=*/TRUE);
    if (is_static_data_member) {
      variable->can_be_instantiated = can_be_instantiated;
      do_not_instantiate = variable->do_not_instantiate
                         = tip->explicit_do_not_instantiate;
      instance_required = variable->instance_required
                        = (tip->instantiation_required &&
                           !variable->is_specialized);
    } else {
      routine->can_be_instantiated = can_be_instantiated;
      do_not_instantiate = routine->do_not_instantiate
                         = tip->explicit_do_not_instantiate;
      instance_required = routine->instance_required
                        = (tip->instantiation_required &&
                           !routine->is_specialized);
    }  /* if */
#if DEBUG
    if (debug_level >= 4) {
      db_name(is_static_data_member ?
                 &variable->source_corresp : &routine->source_corresp);
      fputs(":\n", f_debug);
      fprintf(f_debug, " already_instantiated=%d\n",
              tip->already_instantiated);
      fprintf(f_debug, " instance_required=%d\n", instance_required);
      fprintf(f_debug, " can_be_instantiated=%d\n", can_be_instantiated);
    }  /* if */
#endif /* DEBUG */
    if (instantiation_flags_needed()) {
      /* For automatic instantiation, generate the instantiation flags
         used by the prelinker.  These flags are placed in either the
         template information file or in the IL as variables.  If the
         flags are placed in the IL, this is only done if IL lowering is
         being done. */
      if (instantiation_flags_in_template_info_file &&
          generate_template_files()) {
        /* The flags are to be placed in the template information file. */
        char	*name;
        name = get_mangled_name_for_symbol(instance_sym);
        write_instantiation_flags_to_template_info_file(
             name, instance_required, do_not_instantiate, can_be_instantiated);
#if DO_IL_LOWERING
      } else {
        /* The flags are to be placed in the IL as special variables. */
        if (il_lowering_needed()) {
          a_source_correspondence	*scp;
          scp = is_static_data_member ?
                          &variable->source_corresp : &routine->source_corresp;
          create_instantiation_flag_variables(
             scp, instance_required, do_not_instantiate, can_be_instantiated);
        }  /* if */
#endif /* DO_IL_LOWERING */
      }  /* if */
    }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
    /* In one instantiation per object mode, check whether an instantiation
       for this entity was generated in a separate file.  This is determined
       by checking whether an instantiation needed bit number was assigned
       to the entity.  If a file was generated, write the name of the
       generated file to the template information file. */
    if (one_instantiation_per_object && generate_template_files()) {
      a_boolean		instantiation_file_generated;
      instantiation_file_generated = is_static_data_member
                                   ? variable->instantiation_needed_bit_number
                                   : routine->instantiation_needed_bit_number;
#if MAINTAIN_NEEDED_FLAGS
      if (instantiation_file_generated) {
        /* Don't generate an instantiation file for the entity unless the
           needed flag is also set. */
        instantiation_file_generated = is_static_data_member
                                   ? variable->source_corresp.needed
                                   : routine->definition_needed;
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
      if (instantiation_file_generated) {
        a_source_correspondence	*scp;
        scp = is_static_data_member ?
                  &variable->source_corresp : &routine->source_corresp;
        write_instantiation_file_name_to_template_info_file(scp);
      }  /* if */
    }  /* if */
#endif /*  ONE_INSTANTIATION_PER_OBJECT */
  }  /* for */
  if (any_instantiated_entities_added_to_request_file && total_errors == 0) {
    /* This translation unit "adopted" some instantiations that were known
       not to be defined elsewhere.  Don't write the updated file if any
       errors occurred. */
    add_entities_to_request_file();
  }  /* if */
  db_exit();
}  /* update_auto_instantiation_flags */


static void generate_exported_template_file(void)
/*
Create the file containing information about exported templates.
*/
{
  a_symbol_list_entry_ptr	slep;

  for (slep = exported_templates_list; slep != NULL; slep = slep->next) {
    a_symbol_ptr	sym;
    char		*mangled_name;
    sym = slep->symbol;
    /* Get the signature for the template that is defined. */
    mangled_name = get_mangled_name_for_symbol(sym);
    /* Write an entry to the exported template file. */
    write_to_exported_template_file(etlt_template_name, mangled_name);
  }  /* for */
  /* Only write the other information to the exported template file if
     some template names were written above. */
  if (f_exported_template != NULL) {
    /* Output the file name. */
    write_to_exported_template_file(etlt_file_name, primary_source_file_name);
  }  /* if */
}  /* generate_exported_template_file */


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


static void do_any_needed_instantiations(void)
/*
Go through the instantiations required list and do any instantiations
that might be required.
*/
{
  a_template_instance_ptr	tip;

  do {
    entries_updated_during_instantiation_wrapup = FALSE;
    implicit_inclusion_done_during_instantiation_wrapup = FALSE;
    for (tip = instantiations_required;
         tip != NULL;
         tip = tip->next_in_instantiation_list) {
      /* Make sure the is_static_or_inline flag is set (if needed)
         for this entity. */
      (void)is_static_or_inline_template_entity(tip);
      /* Skip entries that have already been instantiated. */
      if (tip->already_instantiated) continue;
      /* See if the entity should be instantiated.  Note that the value
         returned by can_be_instantiated is not used to determine whether
         should_be_instantiated is called because the tests done by
         should_be_instantiated can result the generation of diagnostics
         that are required even if the entity can't be instantiated. */
      (void)entity_can_be_instantiated(tip, /*implicit_inclusion_okay=*/TRUE);
      if ((instantiation_mode == tim_all || tip->instantiation_required) &&
          !tip->already_instantiated) {
        if (should_be_instantiated(tip, /*implicit_inclusion_okay=*/TRUE)) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
          /* Reset the insert point for instantiations to NULL.  This assures
             that the source sequence entry for the instantiation will be
             added to the end of the source-sequence list. */
          reset_ss_list_instantiation_insert_point();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          instantiate_entity(tip);
        }  /* if */
      }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
      /* See if the entity should be instantiated as a result of an
         assignment by the automatic instantiation mechanism. */
      if (entity_can_be_instantiated(tip, /*implicit_inclusion_okay=*/TRUE) &&
          tip->automatically_instantiated && !tip->already_instantiated) {
        do_automatic_instantiation_of_entity(tip);
      }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
    }  /* for */
  } while (entries_updated_during_instantiation_wrapup ||
           implicit_inclusion_done_during_instantiation_wrapup);
}  /* do_any_needed_instantiations */


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
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  a_template_instance_ptr           tip;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

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
     be difficult to detect.  The flag remains set even after this routine
     has exited so that any additional instantiations that may be put on the
     list will have their instantiations generated immediately.  This happens,
     for example, for templates that may be called by virtual destructors
     generated by generate_required_virtual_destructor_bodies, which is
     called by fe_wrapup after instantiation_wrapup has completed. */
  in_instantiation_wrapup = TRUE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  /* Create the file name of the template information file and template
     request file.  This may be used to create the files or to remove them
     if no template entities exist. */
  generate_template_file_names();
  if (automatic_instantiation_mode) {
    /* Set the flag that indicates that this compilation includes
       external template entities. */
    if (instantiations_required != NULL) any_instantiations_required = TRUE;
    /* Read in the list of entities to be automatically instantiated.  We
       also need to do automatic instantiation checking when a definition
       list was supplied.  In automatic instantiation mode, when a definition
       list file is in use, we do the instantiation unless the entity is
       in the definition list file. */
    if (init_auto_instantiation_information()) {
      request_file_check_needed = TRUE;
    }  /* if */
    if (read_definition_list_file()) request_file_check_needed = TRUE;
    /* Go through the instantiations list and determine whether a given entity
       is flagged for automatic instantiation.  This must be done before
       instantiating things in -tused mode because a -tused function that
       is only referenced by unneeded code (e.g., an uncalled static function)
       will be eliminated unless it is flagged for automatic instantiation or
       was explicitly instantiated. */
    for (tip = instantiations_required;
         tip != NULL;
         tip = tip->next_in_instantiation_list) {
      check_if_entity_should_be_automatically_instantiated(tip);
    }  /* for */
  }  /* if */
  if (any_instantiations_required && use_template_info_file &&
      generate_template_files()) {
    /* Make sure the template information file has been created. */
    if (f_template_info == NULL) open_template_info_file();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  /* Go through the instantiations required list and generated any
     instantiations that are needed or were assigned to this file by
     the automatic instantiation mechanism. */
  do_any_needed_instantiations();
  /* If any friend state changed between the initial prescan and the later one,
     an error should have been issued somewhere. */
  check_assertion_str2(!any_friend_state_changed || total_errors != 0,
                       "instantiation_wrapup:",
                       "silent change in friend state");
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  /* Output information about exported templates defined in this
     translation unit. */
  if (export_template_allowed) {
    generate_exported_template_file();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if CHECKING
  after_instantiation_wrapup = TRUE;
#endif /* CHECKING */
  db_exit();
}  /* instantiation_wrapup */

#if INSTANTIATE_EXTERN_INLINE 
#if AUTOMATIC_TEMPLATE_INSTANTIATION

static a_boolean inline_function_in_request_file(a_routine_ptr	rout_ptr)
/*
Return TRUE if rout_ptr is named in the template instantiation request
file.
*/
{
  char				*name;
  an_instance_lookup_entry_ptr	ilp;
  a_boolean			result = FALSE;

  name = get_mangled_function_name(rout_ptr);
  ilp = find_instance(name, /*add=*/FALSE);
  if (ilp != NULL && ilp->in_request_file) {
    /* The routine was named in the instantiation request file. */
    result = TRUE;
  }  /* if */
  return result;
}  /* inline_function_in_request_file */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

static a_boolean inline_function_should_be_emitted(
					a_routine_ptr	rout_ptr)
/*
Return TRUE if the routine specified by "rout_ptr" should have its body
emitted in this translation unit.
*/
{
  a_boolean	result = FALSE;
  a_boolean	body_can_be_generated = FALSE;

  check_assertion(!C_mode());
  if (rout_ptr->assoc_scope != NULL_region_number) {
    /* The routine has a body. */
    body_can_be_generated = TRUE;
  } else if (rout_ptr->compiler_generated &&
#if MICROSOFT_EXTENSIONS_ALLOWED
             (rout_ptr->decl_modifiers & DM_DLLIMPORT) == 0 &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
             !rout_ptr->is_trivial_default_constructor) {
    /* A compiler generated routine, but not a trivial default constructor. */
    body_can_be_generated = TRUE;
  }  /* if */
  if (!body_can_be_generated) {
    /* We can't emit the body if one can't be generated. */
  } else if (instantiation_mode == tim_used ||
             instantiation_mode == tim_all) {
    /* In -tused mode, emit the function if it was referenced.  Note that
       -tall mode is actually treated like -tused mode with respect to
       inline functions.  This is done to prevent the instantiation of
       certain compiler generated functions that might result in errors. */
    result = rout_ptr->inline_instance_required;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  } else if (inline_function_in_request_file(rout_ptr)) {
    result = TRUE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
  return result;
}  /* inline_function_should_be_emitted */


static void set_body_needed_flag_for_inline_function(
					a_routine_ptr	rout_ptr)
/*
Update the inline function specified by rout_ptr to indicate whether or not
the body should be emitted by the back end.
*/
{
  a_boolean	emit_function;

  check_assertion(!C_mode());
  emit_function = inline_function_should_be_emitted(rout_ptr);
  rout_ptr->suppress_inline_body = !emit_function;
  if (emit_function) {
    rout_ptr->source_corresp.referenced = TRUE;
#if MAINTAIN_NEEDED_FLAGS
    mark_as_needed((char*)rout_ptr, (an_il_entry_kind)iek_routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
  if (emit_function && rout_ptr->compiler_generated) {
    /* If this is a compiler generated routine, make sure it has a body. */
    force_definition_of_compiler_generated_routine(rout_ptr);
    check_assertion(rout_ptr->assoc_scope != NULL_region_number);
  }  /* if */
}  /* set_body_needed_flag_for_inline_function */

#endif /* INSTANTIATE_EXTERN_INLINE */

void inline_function_wrapup(void)
/*
Determine which extern inline functions should have bodies emitted in the
current translation unit.  This routine is used when extern inline functions
are instantiated using a mechanism like the template instantiation mechanism.
*/
{
#if INSTANTIATE_EXTERN_INLINE
  if (instantiate_extern_inline) {
    a_routine_list_entry_ptr	rlep;

#if AUTOMATIC_TEMPLATE_INSTANTIATION
    /* Set the flag that indicates that this translation unit contains
       instantiatable entities. */
    if (inline_function_list != NULL) any_instantiations_required = TRUE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
    for (rlep = inline_function_list; rlep != NULL; rlep = rlep->next) {
      set_body_needed_flag_for_inline_function(rlep->routine);
    }  /* for */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (any_instantiations_required && use_template_info_file &&
        generate_template_files()) {
      /* Make sure the template information file has been created. */
      if (f_template_info == NULL) open_template_info_file();
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
#endif /* INSTANTIATE_EXTERN_INLINE */
}  /* inline_function_wrapup */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
#if INSTANTIATE_EXTERN_INLINE

static void create_instantiation_flags_for_inline_function(
					a_routine_ptr	rout_ptr)
/*
Create instantiation flags for the inline function specified by rout_ptr.
This is used when the template instantiation mechanism is used to provide
a body (if needed) for extern inline functions.
*/
{
  if (instantiation_flags_needed()
#if MAINTAIN_NEEDED_FLAGS
      /* Only generate the flags if the definition_needed flag is set.
         This is done to avoid problems caused by the removal from the IL
         of the routine entry or an enclosing class entry. */
      && rout_ptr->definition_needed
#endif /* MAINTAIN_NEEDED_FLAGS */
                                    ) {
    /* Generate the instantiation flags used by the prelinker.  These flags
       are placed in either the template information file or in the IL as
       variables.  If the flags are placed in the IL, this is only done if
       IL lowering is being done. */
    a_boolean	instance_required = FALSE;
    a_boolean	do_not_instantiate = FALSE;
    a_boolean	can_be_instantiated = TRUE;
    /* The instance required flag is set only when we have already
       decided to generate the body of the inline function.  The
       instance required flag is needed when a definition is present so
       that the prelinker knows that the generating translation unit is
       still referencing the entity. */
    if (!rout_ptr->suppress_inline_body &&
        rout_ptr->assoc_scope != NULL_region_number) {
      instance_required = rout_ptr->inline_instance_required;
    }  /* if */
    if (instantiation_flags_in_template_info_file &&
        generate_template_files()) {
      /* The flags are to be placed in the template information file. */
      char	*name;
      name = get_mangled_function_name(rout_ptr);
      write_instantiation_flags_to_template_info_file(
           name, instance_required, do_not_instantiate, can_be_instantiated);
#if DO_IL_LOWERING
    } else {
      /* The flags are to be placed in the IL as special variables. */
      if (il_lowering_needed()) {
        a_source_correspondence	*scp;
        scp = &rout_ptr->source_corresp;
        create_instantiation_flag_variables(
           scp, instance_required, do_not_instantiate, can_be_instantiated);
      }  /* if */
#endif /* DO_IL_LOWERING */
    }  /* if */
  }  /* if */
}  /* create_instantiation_flags_for_inline_function */

#if ONE_INSTANTIATION_PER_OBJECT

static void write_instantiation_file_name_for_inline_function(
						a_routine_ptr	rout_ptr)
/*
If we are using one instantiation per object mode, determine whether the
routine specified by rout_ptr should be defined in its own file.  If so,
write an instantiation file name entry to the template information file.
*/
{
  /* In one instantiation per object mode, check whether an instantiation
     for this entity was generated in a separate file.  This is determined
     by checking whether an instantiation needed bit number was assigned
     to the entity.  If a file was generated, write the name of the
     generated file to the template information file. */
  if (one_instantiation_per_object && generate_template_files()) {
    a_boolean		instantiation_file_generated;
    instantiation_file_generated =
                                rout_ptr->instantiation_needed_bit_number != 0;
#if MAINTAIN_NEEDED_FLAGS
    /* Don't generate an instantiation file for the entity unless the
       needed flag is also set. */
    instantiation_file_generated = instantiation_file_generated &&
                                                   rout_ptr->definition_needed;
#endif /* MAINTAIN_NEEDED_FLAGS */
    /* Don't generate an instantiation file if the inline body is to be
       suppressed. */
    instantiation_file_generated = instantiation_file_generated &&
                                               !rout_ptr->suppress_inline_body;
    if (instantiation_file_generated) {
      a_source_correspondence	*scp;
      scp = &rout_ptr->source_corresp;
      write_instantiation_file_name_to_template_info_file(scp);
    }  /* if */
  }  /* if */
}  /* write_instantiation_file_name_for_inline_function */

#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* INSTANTIATE_EXTERN_INLINE */

void update_inline_function_flags(void)
/*
Create the automatic instantiation flags for inline functions defined in
this translation unit.  This routine is used when extern inline functions
are instantiated using a mechanism like the template instantiation mechanism.
*/
{
#if INSTANTIATE_EXTERN_INLINE
  if (instantiate_extern_inline) {
    a_routine_list_entry_ptr	rlep;

    for (rlep = inline_function_list; rlep != NULL; rlep = rlep->next) {
      create_instantiation_flags_for_inline_function(rlep->routine);
#if ONE_INSTANTIATION_PER_OBJECT
      /* If we are using one instantiation per object mode, write the
         name of the instantiation object file to the template information
         file. */
      write_instantiation_file_name_for_inline_function(rlep->routine);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    }  /* for */
  }  /* if */
#endif /* INSTANTIATE_EXTERN_INLINE */
}  /* update_inline_function_flags */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

static a_boolean sym_can_be_instantiated(a_symbol_ptr	sym,
				         a_boolean	issue_errors,
                                         a_boolean	is_pragma,
					 a_pragma_kind	pragma_kind)
/*
Determine whether the template function or static data member
specified by sym can be instantiated.  Inline functions and compiler
generated routines (which also happen to be inline) cannot be
instantiated.  Pure virtual functions cannot be instantiated.
*/
{
  a_boolean			result = TRUE;
  a_routine_ptr			routine;
  a_template_instance_ptr	tip = NULL;

  if (sym->kind == (a_symbol_kind)sk_routine ||
      sym->kind == (a_symbol_kind)sk_member_function) {
    routine = sym->variant.routine.ptr;
    tip = sym->variant.routine.instance_ptr;
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
    } else if (sym->variant.routine.ptr->is_specialized &&
               pragma_kind != (a_pragma_kind)pk_do_not_instantiate) {
      /* A specialization declaration has been supplied. */
      result = FALSE;
      if (issue_errors) {
        sym_error(ec_instantiation_requested_and_specialized, sym);
      }  /* if */
    } else if (is_inline_template_function(tip)) {
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
  } else {
    check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
    if (sym->variant.static_data_member.variable->is_specialized &&
        pragma_kind != (a_pragma_kind)pk_do_not_instantiate) {
      /* A specialization declaration has been supplied. */
      result = FALSE;
      if (issue_errors) {
        sym_error(ec_instantiation_requested_and_specialized, sym);
      }  /* if */
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
    if (sym_can_be_instantiated(sym, /*issue_errors=*/TRUE,
                                is_pragma, pragma_kind)) {
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
      /* We can get here from either a do_not_instantiate pragma or a
         Microsoft "extern template" explicit instantiation directive.
         A do_not_instantiate pragma is assumed to be used in cases where
         an old-style specialization is present in some other translation unit.
         Consequently, the is_specialized and specialized_with_old_syntax
         flags are set so that the mangled name used here will match the
         mangled name of the old-style specialization in the other unit. */
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        if (is_pragma) {
          a_variable_ptr vp = sym->variant.static_data_member.variable;
          vp->is_specialized = TRUE;
          vp->specialized_with_old_syntax = TRUE;
        }  /* if */
      } else {
        if (is_pragma) {
          a_routine_ptr rp = sym->variant.routine.ptr;
          rp->is_specialized = TRUE;
          rp->specialized_with_old_syntax = TRUE;
        }  /* if */
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
      if (top_level) {
        an_error_severity	severity;
        /* An incomplete type cannot be specified, but a class with
           an incomplete member class is okay.  An incomplete class type
           can be specified in Microsoft mode.  In a Microsoft "extern
           template" directive, the directive should take effect when the
           class is completed, but we don't implement this at this point. */
        severity = microsoft_mode ? es_warning : es_error;
        pos_diagnostic(severity, ec_incomplete_type_not_allowed, pos);
      }  /* if */
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
                                        is_pragma, pragma_kind)) {
              update_instantiation_flags(list_sym, pragma_kind, pos,
                                         /*is_class_instantiation=*/TRUE,
                                         is_pragma);
            }  /* if */
          }  /* for */
        } else if (mem_sym->kind == (a_symbol_kind)sk_static_data_member) {
          if (sym_can_be_instantiated(mem_sym, /*issue_errors=*/FALSE,
                                      is_pragma, pragma_kind)) {
            update_instantiation_flags(mem_sym, pragma_kind, pos,
                                       /*is_class_instantiation=*/TRUE,
                                       is_pragma);
          }  /* if */
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
#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is used only when extra source positions are
                put out in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static
void make_instantiation_directive(a_pragma_kind		       pragma_kind,
                                  a_symbol_ptr                 sym,
                                  a_source_sequence_entry_ptr  ssep,
                                  a_source_position            *pos,
                                  a_decl_pos_block_ptr         decl_pos_block)
/*
Create an IL entry to represent an instantiation directive.  progma_kind is
used to distinguish an instantiation directive from a "do not instantiate"
directive (which is specified as "extern template" in Microsoft mode).
sym identifies the entity being instantiated, pos is the source position
of the template keyword, and ssep is the empty source sequence entry that
should be used.  When EXTRA_SOURCE_POSITIONS_IN_IL is set to TRUE,
*decl_pos_block contains source positions collected during declaration
processing.
*/
{
  an_instantiation_directive_ptr  idp;
  an_il_entry_kind                kind;

  if (!source_sequence_entries_disallowed) /*lint !e506*/ {
    idp = alloc_instantiation_directive();
    idp->position = *pos;
    idp->entity.ptr = il_entry_for_symbol(sym, &kind);
    idp->entity.kind = (a_byte_il_entry_kind)kind;
    if (pragma_kind == (a_pragma_kind)pk_do_not_instantiate) {
      idp->do_not_instantiate = TRUE;
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    idp->decl_pos_info = make_decl_pos_supplement(/*at_file_scope=*/TRUE,
                                                  decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
the pragma kind of pk_instantiate is passed by the caller.  In Microsoft
mode pk_do_not_instantiate may be passed by the caller if the explicit
instantiation directive began with the "extern" keyword.  is_pragma is
TRUE if this is a pragma and FALSE if it is an explicit instantiation.
*/
{
  a_storage_class               storage_class;
  a_type_ptr                    type;
  a_symbol_locator              locator;
  a_decl_flag_set               do_flags, dso_flags, di_flags;
  a_type_qualifier_set          qualifiers;
  a_decl_modifiers_block        decl_modifiers;
  a_symbol_ptr                  new_sym;
  a_source_sequence_entry_ptr   declarator_ssep;
  a_symbol_ptr		        sym;
  a_token_kind			end_of_statement_token;
  a_func_info_block             func_info;
  a_decl_pos_block              decl_pos_block;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr   ssep;
  a_source_position             template_keyword_pos;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  an_error_severity		severity_if_not_found = es_error;

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
  clear_decl_pos_block(&decl_pos_block);
  /* If this is a pragma it will end with a tok_end_of_source, if not
     it will end with a semicolon. */
  end_of_statement_token = is_pragma ? tok_end_of_source : tok_semicolon;
  if (microsoft_mode && is_generalized_identifier_start(GID_NO_OPTIONS) &&
      next_token() == end_of_statement_token) {
    /* The Microsoft compiler accepts a class instantiation without the
       elaborated type specifier. */
    a_boolean	err = FALSE;
    sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
						     ilm_normal, &err);
    if (!err && sym != NULL && is_template_instance_class_symbol(sym) &&
        !is_template_instance_specific_def_symbol(sym)) {
      /* Process all member functions and static data members. */
      update_instantiation_flags_for_class(sym, kind, start_pos, is_pragma,
                                           /*top_level=*/TRUE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (!is_pragma) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
        decl_pos_block.identifier_range.start = *start_pos;
        decl_pos_block.identifier_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        make_instantiation_directive(kind, sym, ssep, &template_keyword_pos,
                                     &decl_pos_block);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (sym != NULL && !sym->is_error) {
      /* A symbol that refers to an entity that is not a template class. */
      sym_error(ec_not_instantiatable_entity, sym);
    } else {
      /* A NULL symbol was returned. */
      error(ec_invalid_instantiation_argument);
    }  /* if */
    /* Bypass the end of statement token. */
    (void)get_token();
    goto done;
  }  /* if */
  clear_decl_pos_block(&decl_pos_block);
  (void)decl_specifiers((DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_IS_EXPLICIT_INSTANTIATION),
                        &dso_flags, &storage_class, &type, &qualifiers,
                        &decl_modifiers, &decl_pos_block);
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
        make_instantiation_directive(kind, sym, ssep, &template_keyword_pos,
                                     &decl_pos_block);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Something else -- issue an error. */
      sym_error(ec_not_instantiatable_entity, sym);
    }  /* if */
    goto done;
  } else {
    clear_func_info(&func_info);
    di_flags = DI_REAL_DECLARATOR_ALLOWED |
               DI_QUALIFIED_NAME_ALLOWED |
               DI_IS_EXPLICIT_INSTANTIATION |
               DI_OPERATOR_NAME_ALLOWED;
    if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) &&
        qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
    }  /* if */
    declarator(di_flags, &do_flags, (a_type_qualifier_set *)NULL,
               type, (a_type_ptr)NULL, &locator, &type,
               &declarator_ssep, &func_info, &decl_pos_block);
    record_param_id_list_declarations(&func_info);
    /* Issue diagnostic on an incomplete-type in an exception specification. */
    report_exception_spec_errors(&func_info);
    done_with_func_info(func_info);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (declarator_ssep != NULL) {
      remove_from_src_seq_list(declarator_ssep);
      declarator_ssep = NULL;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
     /* The Microsoft compiler silently ignores cases in which no matching
        template is found for an explicit instantiation or an "extern
        template" directive.  In Microsoft bugs mode we issue a warning
        for an explicit instantiation and a remark for an "extern template". */
  if (microsoft_bugs && !is_pragma) {
    severity_if_not_found = kind == (a_pragma_kind)pk_do_not_instantiate
                                               ? (an_error_severity)es_remark
                                               : (an_error_severity)es_warning;
  }  /* if */
  /* Look up the identifier scanned in the declarator.  If the
     declarator contains a qualified name it will already have
     been looked up. */
  sym = locator.specific_symbol;
  if (sym == NULL) {
    sym = normal_id_lookup(&locator, IDL_LINKAGE_LOOKUP);
  }  /* if */
  check_for_declaration_errors(merge_declarator_flags(dso_flags, do_flags),
                               type, &locator, start_pos);
  if (sym == NULL) {
    /* No symbol was found.  If the declarator has a function type
       then say that the name is undefined.  If it was not a function
       type then say it is an invalid pragma argument. */
    if (is_error_locator(locator) ||
        (type != NULL && !is_function_type(type))) {
      pos_error(ec_invalid_instantiation_argument, start_pos);
    } else {
      pos_st_diagnostic(severity_if_not_found, ec_not_a_template_name,
                        &locator.source_position,
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
            make_instantiation_directive(kind, sym, ssep,
                                         &template_keyword_pos,
                                         &decl_pos_block);
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
         declaration or a template instance.  Normally, a failure to find
         a match is an error.  The Microsoft compiler silently ignores
         such failures.  Even so, issue a warning for an explicit
         instantiation and a remark for an "extern template". */
      severity_if_not_found = es_error;
      if (microsoft_bugs && !is_pragma) {
        severity_if_not_found = kind == (a_pragma_kind)pk_do_not_instantiate
                                              ? (an_error_severity)es_remark
                                              : (an_error_severity)es_warning;
      }  /* if */
      new_sym = find_matching_template_instance(
                                          sym, type, locator.template_arg_list,
                                          (a_boolean)locator.is_template_id,
                                          severity_if_not_found);
      if (new_sym != NULL) {
        /* Update the flags for the symbol found. */
        update_instantiation_flags(new_sym, kind, start_pos,
                                   /*is_class_instantiation=*/FALSE,
                                   is_pragma);
        /* If a throw specification was mentioned in the instantiation
           directive, check that it matches up with that of the instantiated
           routine. */
        if (type->variant.routine.extra_info->exception_specification !=
                                                                       NULL) {
          check_exception_specification(type, new_sym,
                                        &func_info.throw_position,
                                        /*is_redecl=*/TRUE);
        }  /* if */
#if DECL_MODIFIERS_IN_USE
        /* In Microsoft mode __declspec(...) modifiers are accepted -- e.g.,
           dllimport on an "extern template" declaration. */
        update_routine_decl_modifiers(
                             new_sym->variant.routine.ptr, &decl_modifiers,
                             &locator.source_position, /*is_redecl=*/FALSE,
                             (kind != (a_pragma_kind)pk_do_not_instantiate),
                             (a_boolean)new_sym->
                                          variant.routine.ptr->is_inline);
#endif /* DECL_MODIFIERS_IN_USE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (!is_pragma) {
          make_instantiation_directive(kind, new_sym, ssep,
                                       &template_keyword_pos, &decl_pos_block);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
  }  /* if */
done:;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!is_pragma) {
    if (ssep != NULL && ssep->entity.kind == (a_byte_il_entry_kind)iek_none) {
      remove_from_src_seq_list(ssep);
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
  /* Templates are outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_templates_in_embedded_cplusplus);
  begin_rescan_of_pragma_tokens(ppp);
  begin_deferral_of_access_checks();
  start_pos = pos_curr_token;
  if (is_generalized_identifier_start(GID_NO_OPTIONS) &&
      next_token() == tok_end_of_source) {
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
    instantiation_directive(pragma_kind, /*is_pragma=*/TRUE, &start_pos);
  } else {
    /* Not an identifier or a declaration. */
    error(ec_invalid_instantiation_argument);
    err = TRUE;
  }  /* if */
  discard_deferred_access_checks();
  end_deferral_of_access_checks();
  /* Stop rescanning tokens from the pragma token cache. */
  wrapup_rescan_of_pragma_tokens(err);
  instantiation_mode = saved_instantiation_mode;
}  /* instantiation_pragma */


static void explicit_instantiation(a_template_decl_options_set options)
/*
Process an explicit instantiation directive.  Most of the processing is
done by instantiation_directive.  This routine makes sure that the current
scope is a valid one for an instantiation directive and disables any
access errors that were detected.  options is a bit set of option flags.
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
    a_pragma_kind	pragma_kind;
    instantiation_mode = tim_none;
    /* In Microsoft mode the "extern" keyword may be used in an explicit
       instantiation directive to indicate that an entity should not be
       instantiated. */
    if ((options & TDO_EXTERN) != 0) {
      pragma_kind = (a_pragma_kind)pk_do_not_instantiate;
    } else {
      pragma_kind = (a_pragma_kind)pk_instantiate;
    }  /* if */
    /* Note that the "template" keyword is bypassed in the subroutine. */
    start_pos = pos_curr_token;
    begin_deferral_of_access_checks();
    instantiation_directive(pragma_kind, /*is_pragma=*/FALSE, &start_pos);
    discard_deferred_access_checks();
    end_deferral_of_access_checks();
  }  /* if */
  remove_stop_token(tok_semicolon);
  instantiation_mode = saved_instantiation_mode;
  db_exit();
}  /* explicit_instantiation */


void template_directive_or_declaration(
			a_token_kind			*final_token,
			a_template_decl_options_set	options)
/*
Scan a template declaration of an explicit instantiation.  This routine
is called to decide whether the current statement is a template
declaration or an explicit instantiation.  It then calls the appropriate
routine.  Note that the final token is not consumed -- that is left to the
caller.  For diagnostics, the kind of token expected (semicolon or right
brace) is returned in *final_token.  options is a bit set of option flags.
*/
{
  a_boolean		export_present = FALSE;
  a_source_position	export_pos;

  db_enter(3, "template_directive_or_declaration");
  export_pos = null_source_position;
  check_assertion(curr_token == tok_template || curr_token == tok_export);
  /* Templates are outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_templates_in_embedded_cplusplus);
  /* Caller should have initialized *final_token; it is changed to tok_rbrace
     if appropriate. */
  check_assertion(*final_token == tok_semicolon);
  /* Check for the presence of the "export" keyword. */
  if (curr_token == tok_export) {
    if (!export_template_allowed) {
      /* Export processing is disabled.  Issue a diagnostic. */
      pos_diagnostic(es_discretionary_error, ec_no_export_support,
                     &pos_curr_token);
    } else {
      export_present = TRUE;
      export_pos = pos_curr_token;
    }  /* if */
    (void)get_token();
  }  /* if */
  if (curr_token != tok_template) {
    /* An export keyword not followed by "template". */
    add_stop_token(tok_semicolon);
    add_stop_token(tok_rbrace);
    syntax_error(ec_exp_template);
    remove_stop_token(tok_rbrace);
    remove_stop_token(tok_semicolon);
    /* If we stopped on a right brace, but it is followed by a semicolon,
       advance to the semicolon. */
    if (curr_token == tok_rbrace && next_token() == tok_semicolon) {
      (void)get_token();
    }  /* if */
    *final_token = curr_token;
  } else if (next_token() == tok_lt) {
    /* The template keyword is followed by a template parameter list.
       This is a template declaration or a specialization using the new
       specialization syntax. */
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
    a_name_linkage_kind      saved_name_linkage;
    a_boolean                err = FALSE, saved_name_linkage_is_explicit;

    if ((options & TDO_EXTERN) != 0) {
      /* An "extern" storage class is only permitted on an explicit
         instantiation directive in Microsoft mode. */
      error(ec_bad_storage_class_on_template_decl);
    }  /* if */
    /* Issue an error if this declaration has C linkage. */
    if (ssep->default_name_linkage == (a_name_linkage_kind)nlk_external) {
      pos_error(ec_bad_linkage_for_decl, &pos_curr_token);
      err = TRUE;
      /* Save the current default linkage. */
      saved_name_linkage = ssep->default_name_linkage;
      saved_name_linkage_is_explicit = ssep->name_linkage_is_explicit;
      ssep->default_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
      ssep->name_linkage_is_explicit = FALSE;
    }  /* if */
    /* Scan the declaration. */
    template_or_specialization_declaration(final_token, export_present,
                                           &export_pos);
    if (err) {
      /* Restore the linkage. */
      ssep->default_name_linkage = saved_name_linkage;
      ssep->name_linkage_is_explicit = saved_name_linkage_is_explicit;
    }  /* if */
  } else {
    /* There is no template parameter list, this must be an explicit
       instantiation. */
    if (export_present) {
      /* An explicit instantiation cannot be exported. */
      pos_error(ec_export_on_instantiation, &export_pos);
    }  /* if */
    explicit_instantiation(options);
  }  /* if */
  db_exit();
}  /* template_directive_or_declaration */


void add_to_inline_function_list(a_routine_ptr	rout_ptr)
/*
rout_ptr points to an inline function that is about to be defined.
Add the routine to a list of inline functions and, if one instantiation
per object mode is used, assign a needed bit number to this routine.
*/
{
  a_routine_list_entry_ptr	rlep;

  /* Only add extern inline functions. */
  if (rout_ptr->storage_class != (a_storage_class)sc_static) {
    rlep = alloc_list_entry_for_routine();
    rlep->routine = rout_ptr;
    rlep->next = inline_function_list;
    inline_function_list = rlep;
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object) {
      /* Get a "needed bit number" for the routine. */
      rout_ptr->instantiation_needed_bit_number =
                                      assign_instantiation_needed_bit_number();
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  }  /* if */
}  /* add_to_inline_function_list */


#if DEBUG
unsigned long db_show_template_space_used(unsigned long grand_total)
/*
Show space used by the template routines.  This is called by
the symbol table space used routine.  The space used by the template
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used_lost("partial spec candidates", avail_partial_order_candidates,
                     num_partial_order_candidates_allocated,
                     a_partial_order_candidate);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  db_space_used("template lookup entries", 
                 num_template_lookup_entries_allocated,
                 a_template_lookup_entry);
  db_space_used("exported template files", 
                 num_exported_template_files_allocated,
                 an_exported_template_file);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  return grand_total;
}  /* db_show_template_space_used */
#endif /* DEBUG */


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
      pch_saved_var_array_elem(exported_templates_list),
      pch_saved_var_array_elem(exported_templates_tail),

      pch_saved_var_array_elem(can_instantiate_list),
      pch_saved_var_array_elem(inline_function_list),
      pch_saved_var_array_elem(avail_partial_order_candidates),
      pch_saved_var_array_elem(type_of_unknown_templ_param_nontype),
#if DEBUG
      pch_saved_var_array_elem(num_partial_order_candidates_allocated),
#if AUTOMATIC_TEMPLATE_INSTANTIATION
      pch_saved_var_array_elem(num_template_lookup_entries_allocated),
      pch_saved_var_array_elem(num_exported_template_files_allocated),
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(type_of_unknown_templ_param_nontype);
  register_trans_unit_variable(instantiations_required);
  register_trans_unit_variable(instantiations_required_tail);
  register_trans_unit_variable(exported_templates_list);
  register_trans_unit_variable(exported_templates_tail);
  register_trans_unit_variable(inline_function_list);
  register_trans_unit_variable(entries_updated_during_instantiation_wrapup);
  register_trans_unit_variable(can_instantiate_list);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  register_trans_unit_variable(any_instantiations_required);
  register_trans_unit_variable(request_file_check_needed);
  register_trans_unit_variable(instantiation_request_file_name);
  register_trans_unit_variable(f_instantiation_request);
  register_trans_unit_variable(f_template_info);
  register_trans_unit_array(instance_lookup_table);
  register_trans_unit_variable(
                              any_instantiated_entities_added_to_request_file);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if CHECKING
  register_trans_unit_variable(any_friend_state_changed);
#endif /* CHECKING */
}  /* templates_one_time_init */


void templates_trans_unit_init(void)
/*
Initialize variables related to template processing that are specific to a
given translation unit.
*/
{
  in_instantiation_wrapup = FALSE;
  implicit_inclusion_done_during_instantiation_wrapup = FALSE;
  instantiations_required = NULL;
  instantiations_required_tail = NULL;
  exported_templates_list = NULL;
  exported_templates_tail = NULL;
  inline_function_list = NULL;
  entries_updated_during_instantiation_wrapup = FALSE;
  can_instantiate_list = NULL;
#if CHECKING
  any_friend_state_changed = FALSE;
  after_instantiation_wrapup = FALSE;
#endif /* CHECKING */
  /* Allocate a type to be used for template parameter constants whose
     real types cannot be known.  This type will be used for all such
     constants that are created. */
  type_of_unknown_templ_param_nontype =
                                    alloc_type((a_type_kind)tk_template_param);
  set_type_size(type_of_unknown_templ_param_nontype);
  type_of_unknown_templ_param_nontype->variant.template_param.kind = 
                                      (a_template_param_type_kind)tptk_unknown;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  any_instantiations_required = FALSE;
  request_file_check_needed = FALSE;
  instantiation_request_file_name = NULL;
  f_instantiation_request = NULL;
  f_template_info = NULL;
  memzero((char *)instance_lookup_table, sizeof(instance_lookup_table));
  any_instantiated_entities_added_to_request_file = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
}  /* templates_trans_unit_init */


void templates_init(void)
/*
Initializations for template.
*/
{
  curr_default_args = NULL;
  defer_inline_function_fixup_and_instantiations = 0;
  deferred_instantiations = NULL;
  deferred_instantiations_tail = NULL;
  avail_partial_order_candidates = NULL;
  deferred_instantiations_in_process = FALSE;
  num_total_pending_instantiations = 0;
#if DEBUG
  num_partial_order_candidates_allocated = 0;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  num_template_lookup_entries_allocated = 0;
  num_exported_template_files_allocated = 0;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#endif /* DEBUG */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  /* Allocate a buffer used to read the various template files. */
  file_read_buffer = alloc_text_buffer(1024);
  memzero((char *)template_lookup_table, sizeof(template_lookup_table));
  /* FIXME - temporary */
  find_exported_template_files();
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
}  /* templates_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
