/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

pragma.c -- Routines to support #pragma directives

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
#if USER_CONTROL_OF_STRUCT_PACKING
#include "layout.h"
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

/*
Macro used to get a pointer to the active pointer to the current construct
pragma list.  The list pointer is stored in the scope stack entry.
This macro returns a pointer to the head of the list.  By updating this
pointer the caller may alter the list pointed to by the current pointer.
*/
#define curr_list_of_curr_construct_pragmas()		         	      \
  (&scope_stack[depth_scope_stack].curr_construct_pragmas)

#if DEBUG

void db_pragma_list(a_pragma_ptr pp)
/*
Display a list of pragmas for debugging purposes.
*/
{
  a_source_correspondence *scp;  

  for (; pp != NULL; pp = pp->next) {
    fprintf(f_debug, "  Entity kind: %s, ",
                     il_entry_kind_names[(int)pp->entity.kind]);
    fprintf(f_debug, "entity ptr: %lx", (unsigned long)pp->entity.ptr);
    if (pp->entity.ptr != NULL) {
      scp = source_corresp_for_il_entry(pp->entity.ptr,
                                        (an_il_entry_kind)pp->entity.kind);
      if (scp != NULL) {
        fprintf(f_debug, " (");
        db_name(scp);
        fprintf(f_debug, ")");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* for */
}  /* db_pragma_list */


void db_scope_pragmas(a_scope_ptr scope)
/*
Display the pragma list for a scope for debugging purposes.
*/
{
  fprintf(f_debug, "Pragma list for ");
  db_scope(scope);
  fprintf(f_debug, ":\n");
  db_pragma_list(scope->pragmas);
}  /* db_scope_pragmas */

#endif /* DEBUG */

static a_pragma_kind_description_ptr add_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_pragma_binding_kind binding_kind,
		       a_function_tag_entry  processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     may_bind_to_decl,
		       a_boolean	     may_bind_to_stmt,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     allowed_in_pragma_operator,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
Allocate a pragma description entry, initialize its fields, and add it
to a linked list of pragma descriptions.  is_pseudo_pragma is used for
things like lint comments that are treated like pragmas by the front end
but cannot be referenced by name in a pragma directive.
processing_function_index is a function pointer index that indicates
which function to call to process this pragma.
*/
{
  a_pragma_kind_description_ptr	pkdp;

  /* Make sure this pragma kind is not already on the list. */
  check_assertion_str(pragma_description_for_pragma_kind[kind] == NULL,
                      "add_pragma_kind_description: duplicate pragma kind");
  /* A pbk_next_construct pragma must bind to a declaration and/or
     statement. */
  check_assertion_str2(binding_kind != pbk_next_construct ||
                       (may_bind_to_decl || may_bind_to_stmt),
                       "add_pragma_kind_description:",
		       "bad next_construct binding");
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  /* The back end must be capable of handling any pragmas that are included
     in the IL, and which it is expected to process (i.e., not ignore).
     The C and C++ generating back ends can only handle pragmas that
     are represented as character strings, or those for which the IL
     contains all the information needed for the back end to re-emit the
     pragma (il_info_is_complete), not those represented as token
     caches.  Note: If the C or C++ generating back end is modified to
     do special processing for other kinds of pragmas, this checking code
     will need to be modified accordingly. */
  check_assertion_str2(!automatically_include_in_il ||
                       (record_pragma_text || ignore_in_back_end ||
                        il_info_is_complete),
                       "add_pragma_kind_description:",
		       "pragma flags not valid when using C/C++ gen. BE");
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  /* When fetching pp-tokens, processing_C_code must be FALSE. */
  check_assertion_str2(!p_fetch_pp_tokens || !processing_C_code,
                       "add_pragma_kind_description:",
		       "flags not valid when fetching pp-tokens");
  /* Preprocessing immediate pragmas must have the fetch_pp_tokens flag set. */
  check_assertion_str2(binding_kind != pbk_preproc_immediate ||
                       p_fetch_pp_tokens, "add_pragma_kind_description:",
                       "preproc_immediate pragmas must use fetch_pp_tokens");
  /* Allocate a new entry. */
  pkdp = (a_pragma_kind_description_ptr)
				alloc_fe(sizeof(a_pragma_kind_description));
#if DEBUG
  num_pragma_descriptions_allocated++;
#endif /* DEBUG */
  pkdp->kind = kind;
  pkdp->binding_kind = binding_kind;
  pkdp->processing_function_index = processing_function_index;
  pkdp->may_bind_to_decl = may_bind_to_decl;
  pkdp->may_bind_to_stmt = may_bind_to_stmt;
  pkdp->global = global;
  pkdp->automatically_include_in_il = automatically_include_in_il;
  pkdp->record_pragma_text = record_pragma_text;
  pkdp->expand_macros = p_expand_macros;
  pkdp->processing_C_code = processing_C_code;
  pkdp->fetch_pp_tokens = p_fetch_pp_tokens;
  pkdp->ignore_in_back_end = ignore_in_back_end;
  pkdp->is_pseudo_pragma = is_pseudo_pragma;
  pkdp->il_info_is_complete = il_info_is_complete;
  pkdp->allowed_in_pragma_operator = allowed_in_pragma_operator;
  pkdp->read_string_as_header_name = read_string_as_header_name;
  pkdp->error_severity = error_severity;
  if (is_pseudo_pragma) {
    /* This is a pseudo-pragma (such as a lint comment) that cannot
       be referenced by name.  Don't add it to the linked list. */
    pkdp->next = NULL;
  } else {
    pkdp->next = pragma_kind_descriptions;
    pragma_kind_descriptions = pkdp;
  }  /* if */
  /* Save the pragma description in an array indexed by pragma kind. */
  pragma_description_for_pragma_kind[(int)kind] = pkdp;
  return pkdp;
}  /* add_pragma_kind_description */


static a_pragma_kind_description_ptr add_next_construct_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_tag_entry  processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     may_bind_to_decl,
		       a_boolean	     may_bind_to_stmt,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_next_construct pragmas.
*/
{
  /* A pbk_next_construct pragma must bind to a declaration and/or
     statement. */
  check_assertion_str2(may_bind_to_decl || may_bind_to_stmt,
                       "add_next_construct_pragma_kind_description:",
                       "bad next_construct binding");
  return add_pragma_kind_description
           (kind, pbk_next_construct, processing_function_index,
            is_pseudo_pragma, may_bind_to_decl, may_bind_to_stmt,
	    /*global=*/FALSE, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_next_construct_pragma_kind_description */


static a_pragma_kind_description_ptr add_next_token_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_tag_entry  processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_next_token pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_next_token, processing_function_index,
	    is_pseudo_pragma, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, global, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_next_token_pragma_kind_description */


static a_pragma_kind_description_ptr add_immediate_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_tag_entry  processing_function_index,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_immediate pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_immediate, processing_function_index,
	    /*is_pseudo_pragma=*/FALSE, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, global, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_immediate_pragma_kind_description */

#if INCLUDE_EDG_TEST_PRAGMAS

static a_pragma_kind_description_ptr add_other_pragma_kind_description
                      (a_pragma_kind 	     kind,
		       a_function_tag_entry  processing_function_index,
		       a_boolean	     is_pseudo_pragma,
		       a_boolean	     global,
		       a_boolean	     automatically_include_in_il,
		       a_boolean	     record_pragma_text,
		       a_boolean	     p_expand_macros,
		       a_boolean	     processing_C_code,
		       a_boolean	     p_fetch_pp_tokens,
		       a_boolean	     ignore_in_back_end,
		       a_boolean	     il_info_is_complete,
		       a_boolean	     read_string_as_header_name,
		       an_error_severity     error_severity)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_other pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_other, processing_function_index,
	    is_pseudo_pragma, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, global, automatically_include_in_il,
            record_pragma_text, p_expand_macros, processing_C_code,
            p_fetch_pp_tokens, ignore_in_back_end, il_info_is_complete,
            /*allowed_in_pragma_operator=*/TRUE,
            read_string_as_header_name, error_severity);
}  /* add_other_pragma_kind_description */

#endif /* INCLUDE_EDG_TEST_PRAGMAS */

static
a_pragma_kind_description_ptr add_preproc_immediate_pragma_kind_description(
                       a_pragma_kind 	          kind,
                       a_function_tag_entry	  processing_function_index,
		       a_boolean		  record_pragma_text,
		       a_boolean		  il_info_is_complete,
                       a_boolean                  automatically_include_in_il,
		       a_boolean		  ignore_in_back_end,
		       a_boolean		  allowed_in_pragma_operator,
		       a_boolean		  read_string_as_header_name)
/*
This is an interface to the general add_pragma_kind_description that is
used for creating pbk_preproc_immediate pragmas.
*/
{
  return add_pragma_kind_description
           (kind, pbk_preproc_immediate, processing_function_index,
            /*is_pseudo_pragma=*/FALSE, /*may_bind_to_decl=*/FALSE,
            /*may_bind_to_expr=*/FALSE, /*global=*/FALSE,
            automatically_include_in_il, record_pragma_text,
            /*expand_macros=*/FALSE, /*processing_C_code=*/FALSE,
            /*fetch_pp_tokens=*/TRUE, ignore_in_back_end,
            il_info_is_complete, allowed_in_pragma_operator,
            read_string_as_header_name,
            /*error_severity=*/es_none);
}  /* add_preproc_immediate_pragma_kind_description */


a_pending_pragma_ptr alloc_pending_pragma(a_pragma_kind_description_ptr pkdp)
/*
Allocate and initialize a pending pragma entry.  Reuse a freed entry if
possible.
*/
{
  a_pending_pragma_ptr	ppp;

  if (avail_pending_pragmas != NULL) {
    /* Reuse a freed entry. */
    ppp = avail_pending_pragmas;
    avail_pending_pragmas = avail_pending_pragmas->next;
  } else {
    /* Allocate a new entry. */
    ppp = (a_pending_pragma_ptr)alloc_fe(sizeof(a_pending_pragma));
#if DEBUG
    num_pending_pragmas_allocated++;
#endif /* DEBUG */
  }  /* if */
  ppp->next = NULL;
  /* Initialize the token cache as a reusable token cache. */
  clear_token_cache(&ppp->token_cache, /*reusable=*/TRUE);
  ppp->id_position = null_source_position;
  ppp->pragma_position = null_source_position;
  ppp->descr_ptr = pkdp;
  ppp->discard_cache_when_done = TRUE;
  ppp->is_microsoft_pragma_operator = FALSE;
  ppp->has_been_processed = FALSE;
  ppp->pragma_text = NULL;
  ppp->il_pragma_entry = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  ppp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Initialize any pragma-specific information. */
  switch (pkdp->kind) {
    case pk_lint_varargs_count:
      ppp->variant.lint_varargs_count = 0;
      break;
#if INCLUDE_EDG_TEST_PRAGMAS
    case pk_test_next_statement:
    case pk_test_next_decl:
    case pk_test_immediate:
    case pk_test_immediate_text:
    case pk_test_immediate_pp_text:
    case pk_test_other:
    case pk_test_bind_next_pass:
      break;
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
    case pk_checking_pragma:
      break;
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
    case pk_db_opt:
    case pk_db_name:
      break;
#endif /* DEBUG */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
    case pk_if_exists:
      break;
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
    case pk_unrecognized:
      /* No special initialization is required. */
      break;
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
    /* Pragma kinds that have no information in the variant section. */
    case pk_printf_args:
    case pk_scanf_args:
    case pk_lint_argsused:
    case pk_lint_notreached:
    case pk_instantiate:
    case pk_do_not_instantiate:
    case pk_can_instantiate:
    case pk_inline_template:
    case pk_diag_suppress:
    case pk_diag_remark:
    case pk_diag_warning:
    case pk_diag_error:
    case pk_diag_once:
    case pk_diag_default:
#if USER_CONTROL_OF_STRUCT_PACKING
    case pk_pack:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if IDENT_DIRECTIVE_AND_PRAGMA
    case pk_ident:
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
    case pk_weak:
#endif /* PRAGMA_WEAK_ALLOWED */
    case pk_define_type_info:
    case pk_stdc:
#if UPC_EXTENSIONS_ALLOWED
    case pk_upc:
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
    case pk_redefine_extname:
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
    case pk_enable_ldscope:
    case pk_disable_ldscope:
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case pk_gcc:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case pk_once:
    case pk_hdrstop:
    case pk_no_pch:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case pk_push_macro:
    case pk_pop_macro:
    case pk_start_map_region:
    case pk_stop_map_region:
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    case pk_setlocale:
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    case pk_comment:
    case pk_conform:
    case pk_include_alias:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    default:
      unexpected_condition_str2("alloc_pending_pragma:", "bad pragma kind");
      break;
  }  /* switch */
  return ppp;
}  /* alloc_pending_pragma */


static a_pending_pragma_ptr alloc_copy_of_pending_pragma
                                            (a_pending_pragma_ptr orig_ppp)
/*
Allocate a pending pragma entry and copy an existing pragma entry into
it.  Reuse a freed entry if possible.
*/
{
  a_pending_pragma_ptr	ppp;

  if (avail_pending_pragmas != NULL) {
    /* Reuse a freed entry. */
    ppp = avail_pending_pragmas;
    avail_pending_pragmas = avail_pending_pragmas->next;
  } else {
    /* Allocate a new entry. */
    ppp = (a_pending_pragma_ptr)alloc_fe(sizeof(a_pending_pragma));
#if DEBUG
    num_pending_pragmas_allocated++;
#endif /* DEBUG */
  }  /* if */
  *ppp = *orig_ppp;
  ppp->next = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  check_assertion_str2(ppp->source_sequence_entry == NULL,
                       "alloc_copy_of_pending_pragma:",
		       "copied pragma has source sequence entry");
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return ppp;
}  /* alloc_copy_of_pending_pragma */


a_pending_pragma_ptr make_copy_of_pragma_list(a_pending_pragma_ptr old_list)
/*
Make a copy of a list of pending pragma entries and set the flag in the
entry that indicates that this is a copy.  This routine is used, for
example, when rescanning tokens from a reusable cache.  When a token with
associated pragma entries is rescanned, the pragma entries must be copied
because the original entries will remain attached to the token in the
reusable cache and must not be affected by operations performed on the
copies associated with the token being processed.
*/
{
  a_pending_pragma_ptr	new_list = NULL;
  a_pending_pragma_ptr	new_list_end = NULL;
  a_pending_pragma_ptr	src_ppp;
  a_pending_pragma_ptr	dest_ppp;

  db_enter(4, "make_copy_of_pragma_list");
  src_ppp = old_list;
  while (src_ppp != NULL) {
    dest_ppp = alloc_copy_of_pending_pragma(src_ppp);
    dest_ppp->discard_cache_when_done = FALSE;
    /* Clear the flag that indicates the pragma has been processed so that
       it will be processed again for the cached token. */
    dest_ppp->has_been_processed = FALSE;
    if (new_list == NULL) new_list = dest_ppp;
    if (new_list_end != NULL) new_list_end->next = dest_ppp;
    new_list_end = dest_ppp;
    src_ppp = src_ppp->next;
  }  /* while */
  db_exit();
  return new_list;
}  /* make_copy_of_pragma_list */


void free_pending_pragma(a_pending_pragma_ptr ppp)
/*
Return a pending pragma entry to the available list.
*/
{
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* If this source sequence entry was never bound to another IL entry,
     remove it from the source sequence list. */
  if (ppp->source_sequence_entry != NULL &&
      ppp->source_sequence_entry->entity.kind ==
                                      (a_byte_il_entry_kind)iek_none) {
    remove_from_src_seq_list(ppp->source_sequence_entry);
    ppp->source_sequence_entry = NULL;
  }  /* if */
#endif /* if GENERATE_SOURCE_SEQUENCE_LISTS */
  if (ppp->discard_cache_when_done) {
    /* This pending pragma entry is the primary entry that refers to this
       token cache and so the token cache should be discarded. */
    discard_token_cache(&ppp->token_cache);
  }  /* if */
  ppp->next = avail_pending_pragmas;
  avail_pending_pragmas = ppp;
}  /* free_pending_pragma */


void free_pending_pragma_list(a_pending_pragma_ptr ppp)
/*
Free a list of pending pragma entries.
*/
{
  while (ppp != NULL) {
    a_pending_pragma_ptr	next_ppp = ppp->next;
    free_pending_pragma(ppp);
    ppp = next_ppp;
  }  /* while */
}  /* free_pending_pragma_list */


void add_to_curr_token_pragma_list(a_pending_pragma_ptr ppp)
/*
Add a pragma to the list of pragmas associated with the current token.
Find the end of the current token pragma list.  The cost of this
should be virtually zero because there will virtually never be more
than one pragma on the list at any point.
*/
{
  a_pending_pragma_ptr	ctp_tail;
  ctp_tail = curr_token_pragmas;
  while (ctp_tail != NULL && ctp_tail->next != NULL) {
    ctp_tail = ctp_tail->next;
  }  /* while */
  if (ctp_tail != NULL) ctp_tail->next = ppp;
  /* If the current token pragma list is NULL, set it to point
     to this entry. */
  if (curr_token_pragmas == NULL) curr_token_pragmas = ppp;
  /* Indicate that the special case code at the beginning of get_token
     is needed to do current token pragma processing. */
  any_initial_get_token_tests_needed = TRUE;
}  /* add_to_curr_token_pragma_list */


a_pending_pragma_ptr add_curr_token_pseudo_pragma(a_pragma_kind      kind,
						  a_source_position *pos)
/*
This routine is used to create pragma entries for things like lint comments
that are treated as "pseudo pragmas" by the front end (although this
routine can actually be used to create any kind of pragma entry).  A
pending pragma is created and added to the current token pragma list.
The pragma entry is returned to the caller so that the pragma-specific
information can be updated, if necessary.
*/
{
  a_pending_pragma_ptr		ppp;
  a_pragma_kind_description_ptr	pkdp;

  pkdp = pragma_description_for_pragma_kind[(int)kind];
  ppp = alloc_pending_pragma(pkdp);
  /* We don't have two positions for pseudo pragmas.  Use the same
     position for both the ID and the start of the directive. */
  ppp->id_position = *pos;
  ppp->pragma_position = *pos;
  add_to_curr_token_pragma_list(ppp);
  return ppp;
}  /* add_curr_token_pseudo_pragma */


#if GENERATE_SOURCE_SEQUENCE_LISTS

static a_source_sequence_entry_ptr add_empty_src_seq_entry_for_pragma(
					a_pending_pragma_ptr	ppp)
/*
Create an empty source sequence entry for the pragma specified by ppp
and return a pointer to the newly created entry.
*/
{
  a_memory_region_number	region_to_switch_back_to;
  a_scope_depth			scope_depth_to_switch_to;
  a_source_sequence_entry_ptr	ssep;

  /* If we are inside a function scope, allocate the source sequence entry
     in the function scope so that it will match the IL pragma entry to
     which it gets bound later on.  Global pbk_other pragmas are always
     allocated in the file scope. */
  scope_depth_to_switch_to = scope_stack[depth_scope_stack].
                                               depth_innermost_function_scope;
  if (scope_depth_to_switch_to == NO_SCOPE_DEPTH ||
      (ppp->descr_ptr->binding_kind == (a_pragma_binding_kind)pbk_other &&
       ppp->descr_ptr->global)) {
    scope_depth_to_switch_to = DEPTH_OF_FILE_SCOPE;
  }  /* if */
  switch_to_scope_region(scope_depth_to_switch_to,
                         &region_to_switch_back_to);
  ssep = add_empty_source_sequence_entry();
  switch_back_to_original_region(region_to_switch_back_to);
  return ssep;
}  /* add_empty_src_seq_entry_for_pragma */


static void add_source_sequence_entry_to_curr_token_pragmas(
                                            a_pragma_binding_kind binding_kind)
/*
Loop through the current token pragma list and create an empty source
sequence entry for any pending pragma entry that does not already have
one and whose binding kind matches binding_kind (or all entries if binding_kind
is pbk_none).  This routine is called by routines which may end up removing
entries from the current token pragma list.  The source sequence
entries must be created before any entries are removed to preserve
the original source ordering information.

The empty source sequence entry will be changed to an iek_pragma entry and
completed when the corresponding IL pragma entry is created (or removed
if it turns out that no IL pragma entry is created).
*/
{
  a_pending_pragma_ptr	ppp = curr_token_pragmas;
  db_enter(4, "add_source_sequence_entry_to_curr_token_pragmas");
  if (!is_nonspecialized_instantiation_context() &&
      depth_template_declaration_scope == NO_SCOPE_DEPTH) {
    while (ppp != NULL) {
      if (ppp->source_sequence_entry == NULL &&
          (binding_kind == pbk_none ||
           binding_kind == ppp->descr_ptr->binding_kind)) {
        ppp->source_sequence_entry = add_empty_src_seq_entry_for_pragma(ppp);
      }  /* if */
      ppp = ppp->next;
    }  /* while */
  }  /* if */
  db_exit();
}  /* add_source_sequence_entry_to_curr_token_pragmas */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


a_boolean select_curr_construct_pragmas(a_boolean	add_to_list)
/*
This routine scans the current token pragma list for any pbk_next_construct
pragmas.  If the binding kind matches the flags passed by the caller,
the pragma is copied to the curr_construct_pragmas list.
If add_to_list is TRUE, any new entries are added to the end of
the list.  If it is FALSE, the existing list must be empty.
If binding kind does not match the flags passed by the caller an error
is issued.  After any next construct pragmas have been removed from
the current token pragma list, process_curr_token_pragmas is called to
take the appropriate actions for the remaining pragmas.  If there are
any pragmas on the curr_construct_pragmas (either ones that were
already on the list, or new ones added by this call) return TRUE;
otherwise return FALSE.
*/
{
  a_pending_pragma_ptr		list_start;
  a_pending_pragma_ptr		list_end;
  a_pending_pragma_ptr		ppp;
  a_pending_pragma_ptr		prev_ppp;

  db_enter(4, "select_curr_construct_pragmas");
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Create source sequence entries for any pragmas that don't yet have
     them. */
  add_source_sequence_entry_to_curr_token_pragmas(pbk_next_construct);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  list_start = *curr_list_of_curr_construct_pragmas();
  if (add_to_list) {
    /* Find the end of the current list. */
    list_end = list_start;
    if (list_end != NULL) {
      while (list_end->next != NULL) {
        list_end = list_end->next;
      }  /* while */
    }  /* if */
  } else {
    if (list_start != NULL && total_errors != 0) {
      /* There should be no items remaining on the list.  If any errors
         occurred, the list items may be a result of the errors.  Discard
         the items on the list.  If no errors have been issued, an internal
         error will be issued below. */
      free_pending_pragma_list(list_start);
      list_start = NULL;
    }  /* if */
    check_assertion_str2(list_start == NULL, "select_curr_construct_pragmas:",
                         "previous list not NULL");
    list_start = NULL;
    list_end = NULL;
  }  /* if */
  ppp = curr_token_pragmas;
  prev_ppp = NULL;
  while (ppp != NULL) {
    a_pending_pragma_ptr	next_ppp = ppp->next;
    a_pragma_kind_description_ptr
				pkdp = ppp->descr_ptr;
    a_pragma_binding_kind	binding_kind = pkdp->binding_kind;
    if (binding_kind == pbk_next_construct) {
      /* All pbk_next_construct pragmas will be removed from the list. */
      if (prev_ppp != NULL) {
        /* Make the previous entry on the list point to the entry after this
           one. */
        prev_ppp->next = next_ppp;
      } else {
        /* This is already the head of the list, change the head to point to
           the next element. */
        curr_token_pragmas = next_ppp;
      }  /* if */
      ppp->next = NULL;
      /* Add the entry to the end of the list of pragmas for the current
         declaration or statement. */
      if (list_start == NULL) list_start = ppp;
      if (list_end == NULL) {
        list_end = ppp;
      } else {
        list_end->next = ppp;
        list_end = ppp;
      }  /* if */
    } else {
      /* If this entry will remain on the current list, save the pointer to
         this element as the next "previous" pointer. */
      prev_ppp = ppp;
    }  /* if */
    ppp = next_ppp;
  }  /* while */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
  if (list_start == NULL && !no_checking_pragmas) {
    list_start = alloc_pending_pragma
                 (pragma_description_for_pragma_kind[(int)pk_checking_pragma]);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    list_start->source_sequence_entry = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
  *curr_list_of_curr_construct_pragmas() = list_start;
  /* Call process_curr_token_pragmas to handle other pragma kinds.  This
     ensures that any immediate pragmas will be processed before any
     next construct pragmas found at the same point. */
  if (curr_token_pragmas != NULL) process_curr_token_pragmas();
  db_exit();
  /* Return TRUE if there are any entrys of the list. */
  return list_start != NULL;
}  /* select_curr_construct_pragmas */


void add_pragma_to_il(a_pending_pragma_ptr  ppp,
                      an_il_entry_kind      entity_kind,
                      char                  *entity_ptr,
                      a_boolean             is_global)
/*
ppp points to the front-end representation of a pragma.  When the pragma
binding kind is pbk_next, entity_ptr is a pointer to the IL entry of the
specified entity_kind with which the pragma is associated; otherwise,
entity_ptr is NULL.  is_global, which is used only when entity_ptr is NULL,
indicates whether the unbound entity is global or local in scope.

This routine (1) allocates the IL pragma entry and initializes it, (2)
binds it to the entity it's associated with, if any, and sets the
has_associated_pragma flag in the latter, (3) adds it to the appropriate
scope pragma list, (4) updates the source sequence entry if there is one,
and (5) returns a pointer to the IL pragma entry to the caller, in case
there is additional processing to be done.
*/
{
  a_pragma_ptr             pp;
  a_memory_region_number   region_to_switch_back_to;
  a_scope_depth            scope_depth = depth_scope_stack;
  a_source_correspondence  *scp = NULL;

  db_enter(5, "add_pragma_to_il");
  if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Pragmas are never added to the IL inside a prototype instantiation. */
  } else {
    /* Determine the memory region in which the IL pragma entry should be
       allocated and the scope_depth of the scope entry to which it should
       be attached. */
    if (entity_ptr == NULL) {
      /* The pragma is not associated with any entity.  If it is a global
         pragma, associate it with the file scope; otherwise, it belongs to
         the local context. */
      if (is_global) {
        scope_depth = DEPTH_OF_FILE_SCOPE;
      } else {
        /* Make sure that this scope is one to which a pragma can be
           attached.  If the current scope has no associated IL scope,
           find the nearest enclosing scope with an associated IL scope. */
        a_scope_stack_entry_ptr	ssep = scope_stack_entry_for(scope_depth);
        a_boolean		done = FALSE;
        for (; !done; ssep = previous_scope_of(ssep)) /*lint !e441*/ {
          check_assertion(ssep != NULL);
          switch (ssep->kind) {
            case sck_class_struct_union:
              /* A pragma can only be added to a class scope in C++ mode. */
              if (C_mode()) break;
              /*FALLTHROUGH*/
            /* Scopes for which a pragma entry may be added to the IL. */
            case sck_enum:
            case sck_file:
            case sck_block:
            case sck_namespace:
            case sck_namespace_extension:
            case sck_function:
              done = TRUE;
              /* Convert the scope stack entry back to a scope depth. */
              scope_depth = scope_depth_of(ssep);
              break;
            /* Scopes for which a pragma entry may not be added to the IL.
               Function prototypes do, in a way, have an associated IL scope,
               but pragmas are not expected to be bound to function prototype
               scopes.  Similarly, template declaration scopes have IL scopes
               when prototype instantiations are included in the IL, but
               cannot have pragmas bound to them. */
            case sck_func_prototype:
            case sck_condition:
            case sck_function_access:
            case sck_pragma:
            case sck_template_instantiation:
            case sck_template_declaration:
            case sck_class_reactivation:
            case sck_namespace_reactivation:
              break;
            default:
              unexpected_condition_str("add_pragma_to_il: bad scope kind");
          }  /* switch */
        }  /* for */
      }  /* if */
    } else if (entity_kind == (an_il_entry_kind)iek_statement) {
      /* Pragmas bound to statements are in local memory and are attached to
         the current scope. */
      /* Set the has_associated_pragma field. */
      ((a_statement_ptr)entity_ptr)->has_associated_pragma = TRUE;
    } else {
      /* We are binding to a declarative entity, and the IL pragma entry
         should be allocated in file-scope memory if the entity itself was
         allocated there; otherwise, use the current memory. */
      scp = source_corresp_for_il_entry(entity_ptr, entity_kind);
      check_assertion_str2(scp != NULL, "add_pragma_to_il:",
                           "invalid entity kind (no source corresp)");
      /* One of the goals here is to use NO_SCOPE_DEPTH as often as
         possible, because that works right even when the entity is
         not in the current translation unit.  The low-level routines,
         however, can't determine the scope for function-local entities
         that aren't class or namespace members.  Note that in C mode
         there are certain cases where NO_SCOPE_DEPTH cannot be used,
         but that's okay because is no way to refer to something in
         another translation unit in C. */
      if (scp_is_class_or_namespace_member(scp)) {
        /* For class and namespace members (including local ones),
           let the low-level routines figure out the scope and memory
           region. */
        if (!C_mode()) {
          scope_depth = NO_SCOPE_DEPTH;
        } else {
          /* C doesn't have class or namespace scopes, so C fields
             go into the file scope. */
          scope_depth = DEPTH_OF_FILE_SCOPE;
        }  /* if */
      } else if (!scp->is_local_to_function) {
        /* For entities not local to a function, let the low-level routines
           figure out the scope and memory region. */
        scope_depth = NO_SCOPE_DEPTH;
      } else if (in_file_scope(scp)) {
        /* Pragmas for things like local static variables are placed on
           the file-scope pragmas list, because there are no orphan lists
           for pragmas. */
        scope_depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
      /* Set the has_associated_pragma field. */
      scp->has_associated_pragma = TRUE;
    }  /* if */
    /* Switch to the proper memory region for the scope depth.  When
       scope_depth is NO_SCOPE_DEPTH, let the low-level routines do
       the switch. */
    if (scope_depth != NO_SCOPE_DEPTH) {
      switch_to_scope_region(scope_depth, &region_to_switch_back_to);
    } else {
      check_assertion(scp != NULL);
    }  /* if */
    pp = alloc_pragma(ppp->descr_ptr->kind, scp);
    pp->position = ppp->pragma_position;
    pp->pragma_text = ppp->pragma_text;
    pp->ignore_in_back_end = ppp->descr_ptr->ignore_in_back_end;
#if MICROSOFT_EXTENSIONS_ALLOWED
    pp->is_microsoft_pragma_operator = ppp->is_microsoft_pragma_operator;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (entity_ptr != NULL) {
      pp->entity.kind = (a_byte_il_entry_kind)entity_kind;
      pp->entity.ptr = entity_ptr;
    }  /* if */
    /* coverity[var_deref_model] */
    add_to_pragma_list(pp, scope_depth, scp);
    if (scope_depth != NO_SCOPE_DEPTH) {
      switch_back_to_original_region(region_to_switch_back_to);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    update_source_sequence_list((char *)pp, (an_il_entry_kind)iek_pragma,
                                ppp->source_sequence_entry);
    /* The source sequence entry is now attached to the IL pragma entry.
       Clear the copy of the source_sequence_entry pointer in the pending
       pragma entry because it is now obsolete. */
    ppp->source_sequence_entry = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    ppp->il_pragma_entry = pp;
  }  /* if */
  db_exit();
}  /* add_pragma_to_il */


void create_il_entry_for_pragma(a_pending_pragma_ptr ppp,
                                a_symbol_ptr         sym,
                                a_statement_ptr      sp)
/*
Create an IL pragma entry for a pending pragma entry.  If either
sym or sp is non-NULL, bind the pragma IL entry to the IL entry
indicated by sym or sp.  For pbk_next_construct pragmas, a non-NULL sym
or sp pointer must be supplied.  The IL entry is then added to the IL.
*/
{
  char              		 *entity;
  an_il_entry_kind	 	 entity_kind;
  a_boolean	         	 is_global = FALSE;
  a_pragma_kind_description_ptr	 pkdp;

  db_enter(5, "create_il_entry_for_pragma");
  pkdp = ppp->descr_ptr;
#if CHECKING
  /* Next construct pragmas must be bound to an IL entry.  Other binding
     kinds may optionally be bound to an IL entry. */
  if (pkdp->binding_kind == pbk_next_construct) {
    check_assertion_str2((sym == NULL) != (sp == NULL),
                         "create_il_entry_for_pragma:",
                         "invalid next_construct call");
  }  /* if */
#endif /* CHECKING */
  if (sym != NULL) {
    entity = il_entry_for_symbol(sym, &entity_kind);
  } else if (sp != NULL) {
    entity = (char *)sp;
    entity_kind = (an_il_entry_kind)iek_statement;
  } else {
    entity = NULL;
    entity_kind = (an_il_entry_kind)iek_none;
    is_global = pkdp->global;
  }  /* if */
  add_pragma_to_il(ppp, entity_kind, entity, is_global);
  db_exit();
}  /* create_il_entry_for_pragma */


void process_curr_token_pragmas(void)
/*
Called by get_token to process pragmas that were found before the
token that is about to become the previous token.

Any pragmas that bind to the next statement/declaration should have
already been removed from the list (assuming that the pragmas were
legally placed).  Diagnostics are issued for any such pragmas that
remain on the list.

pbk_other pragmas are moved to the pragma list of the current scope stack
entry.

pbk_immediate pragmas are processed here.
*/
{
  a_pending_pragma_ptr		ppp;
  a_pragma_kind_description_ptr	pkdp;
  a_next_token_pragma_function_ptr
                                ntfp;
  an_immediate_pragma_function_ptr
                                ipfp;

  db_enter(5, "process_curr_token_pragmas");
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Create source sequence entries for any pragmas that don't yet have
     them. */
  add_source_sequence_entry_to_curr_token_pragmas(pbk_none);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  ppp = curr_token_pragmas;
  curr_token_pragmas = NULL;
  while (ppp != NULL) {
    a_pending_pragma_ptr	next_ppp = ppp->next;
    pkdp = ppp->descr_ptr;
    switch (pkdp->binding_kind) {
      case pbk_next_construct:
        if (pkdp->error_severity != es_none) {
          an_error_code	error_code;
          /* Select the appropriate error based on the kinds of constructs
             that this pragma may bind to. */
          if (pkdp->may_bind_to_decl && pkdp->may_bind_to_stmt) {
            error_code = ec_pragma_must_precede_decl_or_stmt;
          } else if (pkdp->may_bind_to_decl) {
            error_code = ec_pragma_must_precede_declaration;
          } else {
            error_code = ec_pragma_must_precede_statement;
          }  /* if */
          if (pkdp->error_severity != es_none) {
            pos_diagnostic(pkdp->error_severity, error_code,
                           &ppp->id_position);
          }  /* if */
        }  /* if */
        free_pending_pragma(ppp);
        break;
      case pbk_next_token:
        /* Next token pragmas are processed when the token they precede is
           discarded. */
        if (pkdp->automatically_include_in_il) {
          /* Create an IL entry for pragmas that should automatically be
             included in the IL. */
          create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL,
                                     (a_statement_ptr)NULL);
        }  /* if */
        ntfp = (a_next_token_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
        if (ntfp != NULL) {
          (*ntfp)(ppp);
        }  /* if */
        free_pending_pragma(ppp);
        break;
      case pbk_immediate:
        if (!ppp->has_been_processed) {
          /* Normally, immediate pragmas are processed just after they are
             scanned.  But when the pragma is put into a token cache it is
             instead processed when the token it precedes is discarded, in
             the same way as pbk_next_token pragmas. */
          if (pkdp->automatically_include_in_il) {
            /* Create an IL entry for pragmas that should automatically be
               included in the IL. */
            create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL,
                                       (a_statement_ptr)NULL);
          }  /* if */
          ipfp = (an_immediate_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
          if (ipfp != NULL) {
            (*ipfp)(ppp);
          }  /* if */
        }  /* if */
        free_pending_pragma(ppp);
        break;
      case pbk_other:
        {
          /* Add this pragma to the pending pragmas list of the current
             scope. */
          a_scope_stack_entry_ptr	ssep;
          a_pending_pragma_ptr		list_end;
          ssep = &scope_stack[depth_scope_stack];
          list_end = ssep->pending_pragmas;
          if (list_end == NULL) {
            /* No entries on the list yet.  Make the head of the list point
               to this entry. */
            ssep->pending_pragmas = ppp;
          } else {
            /* Find the end of the existing list and add the new entry to the
               end. */
            while (list_end->next != NULL) list_end = list_end->next;
            list_end->next = ppp;
          }  /* if */
          /* Clear the next pointer of ppp so that it no longer points into
             the original list. */
          ppp->next = NULL;
        }
        break;
      default:
        unexpected_condition_str
			("process_curr_token_pragmas: bad binding kind");
        break;
    }  /* switch */
    ppp = next_ppp;
  }  /* while */
  db_exit();
}  /* process_curr_token_pragmas */


void process_immediate_pragmas(void)
/*
Go through the current token pragma list, process any immediate pragmas
on the list.  The pragmas remain on the list, but are marked has having
been processed.  They are kept on the list in case they are associated
with a token that is to be cached.
*/
{
  a_pending_pragma_ptr		ppp;
  a_pragma_kind_description_ptr	pkdp;
  an_immediate_pragma_function_ptr
                                ipfp;

#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Create source sequence entries for any pragmas that don't yet have
     them. */
  add_source_sequence_entry_to_curr_token_pragmas(pbk_immediate);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  for (ppp = curr_token_pragmas; ppp != NULL; ppp = ppp->next) {
    pkdp = ppp->descr_ptr;
    if (pkdp->binding_kind == (a_pragma_binding_kind)pbk_immediate) {
      if (!ppp->has_been_processed) {
        /* Unless this token is going into a token cache, mark this pragma
           as having been processed so that it won't be applied again by
           process_curr_token_pragmas. */
        ppp->has_been_processed = TRUE;
        if (pkdp->automatically_include_in_il) {
          /* Create an IL entry for pragmas that should automatically be
             included in the IL. */
          create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL,
                                     (a_statement_ptr)NULL);
        }  /* if */
        ipfp = (an_immediate_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
        if (ipfp != NULL) {
          (*ipfp)(ppp);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* process_immediate_pragmas */


void end_of_scope_pragma_processing(a_pending_pragma_ptr pending_pragmas)
/*
This routine is called by pop_scope to process any pbk_other pragmas
that remain on the pending pragma list of the current scope.  pending_pragmas
is the pending pragma list to be processed.

Go through the list and issue diagnostics that indicate that this
pragma is not valid in this location.
*/
{
  a_pending_pragma_ptr	ppp;

  db_enter(4, "end_of_scope_pragma_processing");
  for (ppp = pending_pragmas; ppp != NULL; ppp = ppp->next) {
    a_pragma_kind_description_ptr	pkdp = ppp->descr_ptr;
    if (pkdp->error_severity != es_none) {
      pos_diagnostic(pkdp->error_severity, ec_pragma_may_not_be_used_here,
                     &ppp->id_position);
    }  /* if */
  }  /* for */
  /* Free the list of pragmas. */
  free_pending_pragma_list(pending_pragmas);
  db_exit();
}  /* end_of_scope_pragma_processing */


a_pending_pragma_ptr extract_specific_pragmas(a_pragma_kind    kind,
                                              a_symbol_ptr     sym,
                                              a_statement_ptr  sp,
					      a_boolean	       curr_scope_only)
/*
Return one or more pending-pragma entries of the specified pragma kind.  If
the pragma binds to the current declaration or statement and the pragma's
automatically_include_in_il flag is TRUE, then the current construct must be
specified: either sym, if this is a declaration, or sp, if it's a statement,
but not both, must be non-NULL.  If automatically_include_in_il is TRUE, the
IL entry is created before the associated pending-pragma entry is returned.
If more than one pending pragma entry of the required kind is found, they
are returned in a linked list.  This is possible, since the entries returned
are first removed from the lists they currently reside on.

The curr_scope_only flag limits the search to pragmas in the current scope
instead of looking through all of the active scope stack entries.
*/
{
  a_pending_pragma_ptr           ppp;
  a_pending_pragma_ptr		 *scope_list_addr;
  a_pragma_kind_description_ptr  pkdp;
  a_pending_pragma_ptr           new_list = NULL;
  a_pending_pragma_ptr           end_of_new_list = NULL;
  a_pending_pragma_ptr           prev_in_scope_list;
  a_pending_pragma_ptr           next_in_scope_list;
  a_boolean                      is_bound_to_curr_construct;
  a_scope_stack_entry_ptr        ssep;

  db_enter(4, "extract_specific_pragmas");
  /* Get the pragma description entry for the specified kind. */
  pkdp = pragma_description_for_pragma_kind[(int)kind];
  if (pkdp->binding_kind == (a_pragma_binding_kind)pbk_next_construct) {
    /* Set up to search for a pragma bound to the current declaration or
       statement. */
    is_bound_to_curr_construct = TRUE;
    ssep = &scope_stack[depth_scope_stack];
    scope_list_addr = curr_list_of_curr_construct_pragmas();
  } else {
    /* Set up to search for a pbk_other pragma. */
    is_bound_to_curr_construct = FALSE;
    ssep = &scope_stack[depth_scope_stack];
    scope_list_addr = &ssep->pending_pragmas;
  }  /* if */
  /* The outer loop examines one or more scope stack entries.  If it's a
     bind-to-next pragma, only the current scope stack list is checked.
     Otherwise, if it's a global pragma, only the file scope list is checked;
     otherwise, all the scope stack lists, from the current scope stack
     out to the file scope, are checked in turn. */
  for (;;) {
    prev_in_scope_list = NULL;
    /* Check the appropriate list of pending-pragma entries. */
    for (ppp = *scope_list_addr; ppp != NULL; ppp = next_in_scope_list) {
      next_in_scope_list = ppp->next;
      if (ppp->descr_ptr == pkdp) {
        /* It's the right kind -- remove it from the scope stack list. */
        if (prev_in_scope_list == NULL) {
          (*scope_list_addr) = ppp->next;
        } else {
          prev_in_scope_list->next = ppp->next;
        }  /* if */
        /* Now add it to the end of the list to return to the caller. */
        if (new_list == NULL) {
          new_list = ppp;
        } else {
          end_of_new_list->next = ppp;
        }  /* if */
        ppp->next = NULL;
        end_of_new_list = ppp;
        /* If an IL pragma should be generated for it, do that now. */
        if (pkdp->automatically_include_in_il) {
          create_il_entry_for_pragma(ppp, sym, sp);
        }  /* if */
      } else {
        /* Not a match.  Save the prev pointer and advance to the next entry
           on the scope's list. */
        prev_in_scope_list = ppp;
      }  /* if */
    }  /* for */
    /* The appropriate list for the scope has been examined.  Move on the
       containing scope if appropriate; otherwise, terminate the loop. */
    if (is_bound_to_curr_construct || curr_scope_only) {
      /* Only one iteration of the loop for bind-to-next pragmas. */
      break;
    } else if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) {
      /* Nothing else on the scope stack. */
      break;
    } else {
      if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
        /* If this is a template instantiation scope then skip directly from
           here to the file scope. */
        ssep = &scope_stack[DEPTH_OF_FILE_SCOPE];
      } else {
        /* Advance on through the scope stack. */
        --ssep;
      }  /* if */
      scope_list_addr = &ssep->pending_pragmas;
    }  /* if */
  }  /* for */
  db_exit();
  return new_list;  
}  /* extract_specific_pragmas */


void process_curr_construct_pragmas(a_symbol_ptr     sym,
                                    a_statement_ptr  sp)
/*
Go through the list of pragmas that are to be bound to the current
declaration or statement and perform any actions required to process
the pragmas.
*/
{
  a_pending_pragma_ptr     	ppp;
  a_pending_pragma_ptr		list_start;
  a_pragma_kind_description_ptr	pkdp;

  db_enter(4, "process_curr_construct_pragmas");
  check_assertion_str((sym == NULL) == (sp != NULL),
                      "process_pragmas_bound...: invalid arguments");
  /* Go though the pragmas that are meant to apply to the current
     declaration or statement. */
  ppp = *curr_list_of_curr_construct_pragmas();
  list_start = ppp;
  /* Clear the list now so that pragmas can be added to this list as
     a consequence of processing the list of pragmas. */
  *curr_list_of_curr_construct_pragmas() = NULL;
  for(; ppp != NULL; ppp = ppp->next) {
    a_next_construct_pragma_function_ptr ncpfp;
    a_boolean				 err = FALSE;
    pkdp = ppp->descr_ptr;
    /* Make sure that the binding information in the pragma description
       is consistent with the argument list.  Issue diagnostics for
       any pragmas that cannot bind to the current construct. */
    if ((pkdp->may_bind_to_decl && sym != NULL) ||
        (pkdp->may_bind_to_stmt && sp != NULL)) {
      /* Pragma kind matches arguments. */
    } else {
      /* The pragma binding does not match the kind of construct being
         processed.  Issue a diagnostic. */
      an_error_code	error_code;
      err = TRUE;
      if (pkdp->error_severity != es_none) {
        if (pkdp->may_bind_to_decl) {
          error_code = ec_pragma_must_precede_declaration;
        } else {
          check_assertion(pkdp->may_bind_to_stmt);
          error_code = ec_pragma_must_precede_statement;
        }  /* if */
        pos_diagnostic(pkdp->error_severity, error_code, &ppp->id_position);
      }  /* if */
    }  /* if */
    if (!err) {
      ncpfp = (a_next_construct_pragma_function_ptr)index_to_function_pointer(
                                              pkdp->processing_function_index);
      if (pkdp->automatically_include_in_il) {
        /* Create an IL entry for pragmas that should automatically be
           included in the IL. */
        create_il_entry_for_pragma(ppp, sym, sp);
      }  /* if */
      if (ncpfp != NULL) {
        /* Call the pragma processing function associated with this pragma. */
        (*ncpfp)(ppp, sym, sp);
      }  /* if */
    }  /* if */
  }  /* for */
  if (list_start != NULL) {
    free_pending_pragma_list(list_start);
  }  /* if */
  db_exit();
}  /* process_curr_construct_pragmas */


void cannot_bind_to_curr_construct(void)
/*
While processing a construct the caller has determined that it is
not possible to bind pragmas to the construct.  This routine
issues diagnostics that indicate the pragma could not be bound and
clears the curr_construct_pragma list.
*/
{
  a_pending_pragma_ptr     	ppp;
  a_pending_pragma_ptr		list_start;
  a_pending_pragma_ptr		*list_ptr;
  a_pragma_kind_description_ptr	pkdp;

  db_enter(4, "cannot_bind_to_curr_construct");
  list_ptr = curr_list_of_curr_construct_pragmas();
  ppp = *list_ptr;
  list_start = ppp;
  for(; ppp != NULL; ppp = ppp->next) {
    pkdp = ppp->descr_ptr;
    if (pkdp->error_severity != es_none) {
      pos_diagnostic(pkdp->error_severity, ec_pragma_may_not_be_used_here,
                     &ppp->id_position);
    }  /* if */
  }  /* for */
  if (list_start != NULL) {
    free_pending_pragma_list(list_start);
  }  /* if */
  /* Clear the curr_construct_pragma list. */
  *list_ptr = NULL;
  db_exit();
}  /* cannot_bind_to_curr_construct */


void discard_curr_construct_pragmas(void)
/*
This routine is called when an error is encountered while processing
a construct, and as a result of that error it is not possible to
do the binding of any current construct pragmas.  The list of
current construct pragmas is simply cleared.
*/
{
  a_pending_pragma_ptr		*list_ptr;
  a_pending_pragma_ptr		list_start;

  db_enter(4, "discard_curr_construct_pragmas");
  list_ptr = curr_list_of_curr_construct_pragmas();
  list_start = *list_ptr;
  if (list_start != NULL) {
    free_pending_pragma_list(list_start);
  }  /* if */
  *list_ptr = NULL;
  db_exit();
}  /* discard_curr_construct_pragmas */


a_pending_pragma_ptr extract_curr_construct_pragmas(void)
/*
This routine gets the pointer to the list of current construct
pragmas, goes through the list and clears any removes any source
sequence entries that may exist, clears the current construct pragma
entry on the scope stack, and returns the pragma list to the caller.
This is used for saving the current construct pragma list so that
the pragmas may be applied to each instance of a template.
*/
{
  a_pending_pragma_ptr	ppp;
  a_pending_pragma_ptr  *scope_list_addr;
  a_pending_pragma_ptr  list_head;

  db_enter(4, "extract_curr_construct_pragmas");
  scope_list_addr = curr_list_of_curr_construct_pragmas();
  /* Get the list head and clear the list pointer in the scope stack. */
  ppp = *scope_list_addr;
  list_head = ppp;
  *scope_list_addr = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  while (ppp != NULL) {
    if (ppp->source_sequence_entry != NULL) {
      /* If this source sequence entry was never bound to another IL entry,
         remove it from the source sequence list. */
      check_assertion_str2(ppp->source_sequence_entry->entity.kind ==
                                             (a_byte_il_entry_kind)iek_none,
                           "extract_curr_construct_pragmas:",
                           "source sequence entry already in use");
      remove_from_src_seq_list(ppp->source_sequence_entry);
      ppp->source_sequence_entry = NULL;
    }  /* if */
    ppp = ppp->next;
  }  /* while */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
  return list_head;
}  /* extract_curr_construct_pragmas */


void reactivate_curr_construct_pragmas(a_pending_pragma_ptr pragma_list)
/*
Restore a list of pragmas as the current token pragmas.
*/
{
  a_pending_pragma_ptr  *scope_list_addr;
  a_pending_pragma_ptr	ppp;

  db_enter(4, "reactivate_curr_construct_pragmas");
  scope_list_addr = curr_list_of_curr_construct_pragmas();
  check_assertion_str2(*scope_list_addr == NULL,
                       "reactivate_curr_construct_pragmas:",
                       "pragma list not already empty");
  /* Make a copy of the list of pragmas associated with this template and
     set this scope's current construct list to point to the new copy. */
  ppp = make_copy_of_pragma_list(pragma_list);
  *scope_list_addr = ppp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* The source sequence entries were cleared when the current construct
     pragmas were extracted.  Create new source sequence entries now. */
  for (; ppp != NULL; ppp = ppp->next) {
    ppp->source_sequence_entry = add_empty_src_seq_entry_for_pragma(ppp);
  }  /* for */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
}  /* reactivate_curr_construct_pragmas */


void process_pragmas_at_end_of_source(void)
/*
Do any pragma processing that is required when the end of the source file
has been reached.
*/
{
  db_enter(4, "process_pragmas_at_end_of_source");
  /* Process any current token pragmas that appeared after all other
     tokens of the source program. */
  process_curr_token_pragmas();
  db_exit();
}  /* process_pragmas_at_end_of_source */

#if DEBUG

void db_opt_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a db_opt pragma is encountered.  Process the
pragma argument as if it were a debug option specified on the command-line.
*/
{
  if (!db_active) {
    /* In order for the db_opt pragma to be used a debug option must have
       been specified on the command-line.  This is needed because
       db_active cannot be set TRUE in the middle of a compilation. */
    pos_error(ec_db_option_required_on_cmd_line, &ppp->pragma_position);
  } else {
    char	*debug_arg = ppp->pragma_text;
    /* Skip past the debug pragma name. */
    debug_arg = strchr(debug_arg, ' ');
    if (debug_arg != NULL) {
      char	*arg_copy;
      /* Skip past the blank. */
      debug_arg++;
      /* Make a copy of the argument. */
      arg_copy = alloc_general((sizeof_t)(strlen(debug_arg) + 1));
      (void)strcpy(arg_copy, debug_arg);
      (void)proc_debug_option(arg_copy);
    }  /* if */
  }  /* if */
}  /* db_opt_pragma */

void db_name_pragma(a_pending_pragma_ptr	ppp)
/*
The routine called when a db_name pragma is encountered.  Process the
pragma argument as if it were a debug option specified on the command-line.
*/
{
  if (!db_active) {
    /* In order for the db_name pragma to be used a debug option must have
       been specified on the command-line.  This is needed because
       db_active cannot be set TRUE in the middle of a compilation. */
    pos_error(ec_db_option_required_on_cmd_line, &ppp->pragma_position);
  } else {
    char	*debug_arg = ppp->pragma_text;
    /* Skip past the debug pragma name. */
    debug_arg = strchr(debug_arg, ' ');
    if (debug_arg != NULL) {
      char	*arg_copy;
      /* Skip past the blank. */
      debug_arg++;
      /* Make a copy of the argument. */
      arg_copy = alloc_general((sizeof_t)(strlen(debug_arg) + 1));
      (void)strcpy(arg_copy, debug_arg);
      (void)proc_debug_name_option(arg_copy);
    }  /* if */
  }  /* if */
}  /* db_name_pragma */

#endif /* DEBUG */

#if INCLUDE_EDG_TEST_PRAGMAS
void test_immediate_pragma(a_pending_pragma_ptr ppp)
/*
Routine called by the "#pragma test_immediate", a pragma included
by EDG for testing purposes.
*/
{
  a_symbol_ptr       sym = NULL;
  a_boolean          err = FALSE;

  begin_rescan_of_pragma_tokens(ppp);
  if (is_generalized_identifier_start(GID_NO_OPTIONS)) {
    sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
						     ilm_normal, &err);
  }  /* if */
  /* Flush to the end of the pragma. */
  while (curr_token != tok_newline && curr_token != tok_end_of_source) {
    (void)get_token();
  }  /* while */
  wrapup_rescan_of_pragma_tokens(err);
  if (sym != NULL) {
    create_il_entry_for_pragma(ppp, sym, (a_statement_ptr)NULL);
  }  /* if */
}  /* test_immediate_pragma */
#endif /* INCLUDE_EDG_TEST_PRAGMAS */

void pragma_one_time_init(void)
/*
Do one-time initialization of variables related to pragma processing.
(Variables that need to be reinitialized with each new translation unit
are handled in pragma_init.)
*/
{
  /* Save variables from pragma.h and pragma.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_token_pragmas),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* pragma_one_time_init */


void pragma_init(void)
/*
Initialize the pragma description table.
*/
{
  int	i;
  db_enter(3, "pragma_init");
  /* Clear the array used to get a pragma description pointer based on a
     pragma kind. */
  for (i = (int)pk_none; i < (int)pk_last; ++i) {
    pragma_description_for_pragma_kind[i] =
					 (a_pragma_kind_description_ptr)NULL;
  }  /* for */
  pragma_kind_descriptions = NULL;
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_printf_args,
		 (a_function_tag_entry)fn_record_arg_pragma,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_scanf_args,
	         (a_function_tag_entry)fn_record_arg_pragma,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_lint_argsused,
		 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/TRUE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_lint_varargs_count,
		 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/TRUE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_lint_notreached,
		 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/TRUE,
		 /*may_bind_to_decl=*/FALSE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
  if (!C_mode()) {
    (void)add_next_token_pragma_kind_description
 		((a_pragma_kind)pk_instantiate,
	         (a_function_tag_entry)fn_instantiation_pragma,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_do_not_instantiate,
		 (a_function_tag_entry)fn_instantiation_pragma,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_can_instantiate,
		 (a_function_tag_entry)fn_instantiation_pragma,
		 /*is_pseudo_pragma=*/FALSE,
		 /*global=*/FALSE,
		 /*automatically_include_in_il=*/FALSE,
		 /*record_pragma_text=*/FALSE,
		 /*expand_macros=*/TRUE,
		 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
		 es_error);
  }  /* if */
#if USER_CONTROL_OF_STRUCT_PACKING
  (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_pack,
                 (a_function_tag_entry)fn_pack_pragma,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/BACK_END_IS_CP_GEN_BE,
                 /*record_pragma_text=*/BACK_END_IS_CP_GEN_BE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if IDENT_DIRECTIVE_AND_PRAGMA
  (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_ident,
                 (a_function_tag_entry)fn_ident_pragma,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/TRUE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
#if PRAGMA_WEAK_ALLOWED
  (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_weak,
		 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* PRAGMA_WEAK_ALLOWED */
  (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_once,
                 (a_function_tag_entry)fn_once_pragma,
                 /*record_pragma_text=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/FALSE);
  (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_hdrstop,
                 (a_function_tag_entry)fn_hdrstop_or_no_pch_pragma,
                 /*record_pragma_text=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*allowed_in_pragma_operator=*/FALSE,
		 /*read_string_as_header_name=*/FALSE);
  (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_no_pch,
                 (a_function_tag_entry)fn_hdrstop_or_no_pch_pragma,
                 /*record_pragma_text=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*allowed_in_pragma_operator=*/FALSE,
		 /*read_string_as_header_name=*/FALSE);
  if (!C_mode()) {
    (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_define_type_info,
		 (a_function_tag_entry)fn_define_type_info_pragma,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/BACK_END_IS_CP_GEN_BE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/!BACK_END_IS_CP_GEN_BE, /*lint !e506*/
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
  if (c99_mode || fixed_point_enabled) {
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_stdc,
                 (a_function_tag_entry)fn_stdc_pragma,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    (void)add_next_token_pragma_kind_description(
                                         (a_pragma_kind)pk_upc,
                                         (a_function_tag_entry)fn_upc_pragma,
                                         /*is_pseudo_pragma=*/FALSE,
                                         /*global=*/TRUE,
                                         /*automatically_include_in_il=*/FALSE,
                                         /*record_pragma_text=*/FALSE,
                                         /*expand_macros=*/FALSE,
                                         /*processing_C_code=*/FALSE,
			                 /*fetch_pp_tokens=*/FALSE,
                                         /*ignore_in_back_end=*/FALSE,
                                         /*il_info_is_complete=*/TRUE,
                                         /*read_string_as_header_name=*/FALSE,
                                         es_error);
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  (void)add_next_token_pragma_kind_description(
                                         (a_pragma_kind)pk_redefine_extname,
                                         (a_function_tag_entry)
                                                    fn_redefine_extname_pragma,
                                         /*is_pseudo_pragma=*/FALSE,
                                         /*global=*/TRUE,
                                         /*automatically_include_in_il=*/FALSE,
                                         /*record_pragma_text=*/FALSE,
                                         /*expand_macros=*/FALSE,
                                         /*processing_C_code=*/FALSE,
			                 /*fetch_pp_tokens=*/FALSE,
                                         /*ignore_in_back_end=*/FALSE,
                                         /*il_info_is_complete=*/TRUE,
                                         /*read_string_as_header_name=*/FALSE,
                                         es_error);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if SUN_EXTENSIONS_ALLOWED
  if (sun_linker_scope_allowed) {
    (void)add_preproc_immediate_pragma_kind_description(
                                        (a_pragma_kind)pk_enable_ldscope,
                                        (a_function_tag_entry)
                                                             fn_ldscope_pragma,
		                        /*record_pragma_text=*/TRUE,
                                        /*il_info_is_complete=*/TRUE,
                                        /*automatically_include_in_il=*/TRUE,
					/*ignore_in_back_end=*/FALSE,
					/*allowed_in_pragma_operator=*/TRUE,
                                        /*read_string_as_header_name=*/FALSE);
    (void)add_preproc_immediate_pragma_kind_description(
                                        (a_pragma_kind)pk_disable_ldscope,
                                        (a_function_tag_entry)
                                                             fn_ldscope_pragma,
		                        /*record_pragma_text=*/TRUE,
                                        /*il_info_is_complete=*/TRUE,
                                        /*automatically_include_in_il=*/TRUE,
					/*ignore_in_back_end=*/FALSE,
					/*allowed_in_pragma_operator=*/TRUE,
                                        /*read_string_as_header_name=*/FALSE);
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && gnu_version >= 40200) {
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_gcc,
                 (a_function_tag_entry)fn_gcc_pragma,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_diag_suppress,
                 (a_function_tag_entry)fn_diag_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_diag_remark,
                 (a_function_tag_entry)fn_diag_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_diag_warning,
                 (a_function_tag_entry)fn_diag_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_diag_error,
                 (a_function_tag_entry)fn_diag_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_diag_once,
                 (a_function_tag_entry)fn_diag_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_diag_default,
                 (a_function_tag_entry)fn_diag_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#if INCLUDE_EDG_TEST_PRAGMAS
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_test_next_decl,
		 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_next_construct_pragma_kind_description
 		((a_pragma_kind)pk_test_next_statement,
		 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/FALSE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_test_immediate,
                 (a_function_tag_entry)fn_test_immediate_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_test_immediate_text,
                 (a_function_tag_entry)fn_null,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_test_immediate_pp_text,
                 (a_function_tag_entry)fn_null,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/TRUE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_other_pragma_kind_description
		((a_pragma_kind)pk_test_other,
                 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
  (void)add_next_construct_pragma_kind_description
 		((a_pragma_kind)pk_test_bind_next_pass,
                 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* INCLUDE_EDG_TEST_PRAGMAS */
#if ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_checking_pragma,
                 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/FALSE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_none);
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */
#if DEBUG
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_db_opt,
                 (a_function_tag_entry)fn_db_opt_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  (void)add_immediate_pragma_kind_description
		((a_pragma_kind)pk_db_name,
                 (a_function_tag_entry)fn_db_name_pragma,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
#endif /* DEBUG */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  if (microsoft_mode) {
    (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_if_exists,
                 (a_function_tag_entry)fn_if_exists_pragma,
		 /*is_pseudo_pragma=*/TRUE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/TRUE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_error);
  }  /* if */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    (void)add_preproc_immediate_pragma_kind_description
		((a_pragma_kind)pk_push_macro,
                 (a_function_tag_entry)fn_push_macro_pragma,
                 /*record_pragma_text=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
		 /*ignore_in_back_end=*/TRUE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/FALSE);
    (void)add_preproc_immediate_pragma_kind_description
		((a_pragma_kind)pk_pop_macro,
                 (a_function_tag_entry)fn_pop_macro_pragma,
                 /*record_pragma_text=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
		 /*ignore_in_back_end=*/TRUE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/FALSE);
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_start_map_region,
                 (a_function_tag_entry)fn_microsoft_start_map_region_pragma,
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/TRUE,
                 es_warning);
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_stop_map_region,
                 (a_function_tag_entry)fn_microsoft_stop_map_region_pragma,
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_setlocale,
                 (a_function_tag_entry)fn_setlocale_pragma,
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
		 /*ignore_in_back_end=*/TRUE,
		 /*il_info_is_complete=*/TRUE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
    /* In the following call, processing_C_code is set to TRUE so that
       concatenation of string literals will occur in the optional second
       argument. */
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_comment,
                 (a_function_tag_entry)fn_microsoft_comment_pragma,
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/TRUE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_warning);
    (void)add_next_token_pragma_kind_description
                ((a_pragma_kind)pk_conform,
                 (a_function_tag_entry)fn_microsoft_conform_pragma,
                 /*is_pseudo_pragma=*/FALSE,
                 /*global=*/FALSE,
                 /*automatically_include_in_il=*/FALSE,
                 /*record_pragma_text=*/FALSE,
                 /*expand_macros=*/TRUE,
                 /*processing_C_code=*/FALSE,
                 /*fetch_pp_tokens=*/FALSE,
                 /*ignore_in_back_end=*/FALSE,
                 /*il_info_is_complete=*/TRUE,
                 /*read_string_as_header_name=*/FALSE,
                 es_warning);
    (void)add_preproc_immediate_pragma_kind_description
                ((a_pragma_kind)pk_include_alias,
                 (a_function_tag_entry)fn_microsoft_include_alias_pragma,
                 /*record_pragma_text=*/FALSE,
		 /*il_info_is_complete=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,
		 /*ignore_in_back_end=*/TRUE,
		 /*allowed_in_pragma_operator=*/TRUE,
		 /*read_string_as_header_name=*/TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
  /* When unrecognized pragmas are being included in the IL, we need a
     pragma description that can be used for the unrecognized pragmas.
     The pragma description for pk_unrecognized is the one that will
     be used.

     This definition may be modified (except as noted below), but some
     description must be provided. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* When source sequence lists are being generated, unrecognized pragmas
     are treated as immediate pragmas.  The source sequence information can
     be used to output the pragmas in the right location (when the C++
     generating back end is used, for example).   Immediate pragmas are
     preferable to next-construct pragmas for representing unrecognized
     pragmas because next-construct pragmas are only valid in certain
     contexts. */
  (void)add_next_token_pragma_kind_description
		((a_pragma_kind)pk_unrecognized,
                 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*global=*/FALSE,
                 /*automatically_include_in_il=*/TRUE,  /* Do not change. */
                 /*record_pragma_text=*/TRUE,         /* Do not change. */
                 /*expand_macros=*/FALSE,		/* Do not change. */
                 /*processing_C_code=*/FALSE, /* Do not change. */
                 /*fetch_pp_tokens=*/TRUE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
  /* When source sequence lists are not being generated, unrecognized pragmas
     are treated as next-construct pragmas.  This allows the C generating
     back end to output the pragma along with the entity to which it is
     bound but also means that unrecognized pragmas may only appear in
     contexts in which next-construct pragmas are allowed (i.e., immediately
     before a declaration or statement).  Furthermore, if the associated
     entity is omitted (because it is unused) the pragma will also be
     omitted.  If you want the pragma to come out even if the associated
     entity is omitted, you can change the pragma to an "immediate" pragma.
     This will, however, have the side-effect that the pragma will no longer
     come out adjacent to the associated declaration.  All of the immediate
     pragmas in a given scope will come out together. */
  (void)add_next_construct_pragma_kind_description
		((a_pragma_kind)pk_unrecognized,
                 (a_function_tag_entry)fn_null,
		 /*is_pseudo_pragma=*/FALSE,
		 /*may_bind_to_decl=*/TRUE,
		 /*may_bind_to_stmt=*/TRUE,
                 /*automatically_include_in_il=*/TRUE,  /* Do not change. */
                 /*record_pragma_text=*/TRUE,         /* Do not change. */
                 /*expand_macros=*/FALSE,		/* Do not change. */
                 /*processing_C_code=*/FALSE, /* Do not change. */
                 /*fetch_pp_tokens=*/TRUE,
		 /*ignore_in_back_end=*/FALSE,
		 /*il_info_is_complete=*/FALSE,
		 /*read_string_as_header_name=*/FALSE,
                 es_warning);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
  db_exit();
}  /* pragma_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
