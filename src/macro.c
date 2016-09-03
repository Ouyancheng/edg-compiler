/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

macro.c -- Macro definition and expansion routines.

*/


/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "decls.h"
#include "macro.h"
#include "pch.h"
#include "preproc.h"
#include "symbol_ref.h"
#include "sys_predef.h"
#if DO_IL_LOWERING && GENERATE_EH_TABLES
#include "lower_eh.h"
#endif /* DO_IL_LOWERING && GENERATE_EH_TABLES */

/*
Buffer used to contain the characters of a macro being defined, and the
characters of macro expansions.
*/
static char	*macro_buffer;
			/* Contains characters of macro expansions and of
			   macro definitions.  Dynamically allocated,
			   expanded as needed. */
#define MACRO_BUFFER_INITIAL_ALLOCATION 4000
			/* Initial allocation size for macro_buffer.  The
			   initial allocation should be such that almost all
			   cases can be accepted (so that the realloc is
			   hardly ever needed).  Subsequent reallocations will
			   double the amount previously allocated. */
static char	*after_end_of_macro_buffer;
			/* The address just past the last element of
			   macro_buffer. */
static char	*next_avail_in_macro_buffer;
			/* Next character position in macro_buffer available
			   for allocation.  Reset at the start of a macro
			   definition or a top-level macro expansion.  Not
			   set outside of macro processing. */
static sizeof_t num_compacted_macro_buffer_chars;
			/* Number of characters in macro_buffer that have
			   already been compacted as a result of previous
			   reallocations.  Except for cases where source line
			   modifications are inserted in this region because
			   of rescanning, these characters cannot be compacted
			   further and, as an optimization, can thus just be
			   memcpy'ed during further macro_buffer
			   reallocations. */
static sizeof_t num_chars_deleted_in_macro_buffer;
			/* Number of characters in the uncompacted portion of
			   macro_buffer that have been logically deleted via
			   source line modifications. */
static char	*macro_buffer_region_in_progress;
			/* If non-NULL, points to the start of a section at
			   the end of macro_buffer that is in the process of
			   being built incrementally (e.g., by proc_define).
			   Set by begin_macro_buffer_region() and by
			   release_macro_buffer_region().  This allows the
			   region to be copied by expand_macro_buffer, even
			   though the region is not associated with a source
			   line modification at the time that macro_buffer is
			   reallocated. */
static a_const_char
		*discarded_pcc_top_level_begin,
		*discarded_pcc_top_level_end;
			/* Delimit the region of the macro buffer
			   containing the top-level expansion being
			   replaced by expand_top_level_pcc_macro.  Valid
			   only during the period when the macro buffer may
			   be expanded for copying the contents of the
			   auxiliary buffer.  Allows expand_macro_buffer to
			   verify that any orphaned source line
			   modifications are being legitimately
			   abandoned. */
#if FULLY_RESOLVED_MACRO_POSITIONS
static a_macro_text_map
		macro_text_map;
			/* Map from text in macro_buffer to the original
			   source locations from which the text came, i.e.,
			   from the macro definition or macro argument.  Like
			   macro_buffer, this is cumulative for an entire
			   logical source line; each source line modification
			   will refer to a subset of the map entries to
			   describe the offsets in its inserted_text portion
			   of macro_buffer.  (Note that the offsets in entries
			   in macro_text_map are relative to the beginning of
			   the inserted text of the associated source line
			   modification, not to the beginning of
			   macro_buffer.)  Also like macro_buffer, this data
			   structure is used for both macro definitions and
			   expansions. */
#define MACRO_TEXT_MAP_INITIAL_COUNT 500
			/* Initial number of map entries for macro_text_map.
			   The initial allocation should be such that almost
			   all cases can be accepted (so that the realloc is
			   hardly ever needed).  Subsequent reallocations will
			   double the number of entries previously
			   allocated. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

#if RECORD_MACRO_INVOCATIONS
static a_macro_invocation_record_block_ptr
		last_macro_invocation_record_block;
			/* Pointer to the macro invocation record block
			   containing the record with the largest index so
			   far. */
static a_macro_invocation_record_block_ptr
		ckpt_last_macro_invocation_record_block;
			/* Shadow copy of preceding for checkpoint/revert. */
static a_macro_invocation_record_block_ptr
		saved_next_macro_invocation_record_block;
			/* If reverting a macro invocation moves
			   last_macro_invocation_record_block to the
			   preceding block, the newly-emptied block is
			   pointed to by this so it can be reused for
			   succeeding macro invocation records. */
static a_macro_invocation_record_index
		num_macro_invocation_records;
			/* The total number of macro invocation records used
			   so far. */
static a_macro_invocation_record_index
		ckpt_num_macro_invocation_records;
			/* Shadow copy of preceding for checkpoint/revert. */
static unsigned long
		max_macro_invocation_depth;
			/* The largest number of levels that have been pushed
			   onto the macro invocation stack. */
static unsigned long
		ckpt_max_macro_invocation_depth;
			/* Shadow copy of preceding for checkpoint/revert. */
static unsigned long
		depth_of_curr_macro_invocation_record;
			/* The macro invocation stack depth of the current
			   macro invocation record.  Note that this differs
			   from macro_depth: while macro_depth refers to the
			   number of active (recursive) calls to the
			   macro_invocation function,
			   depth_of_curr_macro_invocation_record takes into
			   account macro invocations occurring during the
			   rescan of macro expansions. */
static unsigned long
		ckpt_depth_of_curr_macro_invocation_record;
			/* Shadow copy of preceding for checkpoint/revert. */
#endif /* RECORD_MACRO_INVOCATIONS */
static char	*aux_buffer_for_pcc_macros;
			/* Auxiliary buffer allocated in pcc mode only and
			   used to construct the full text of a first-level
			   macro expansion so that the token pasting can
			   match pcc's. */
#define AUX_BUFFER_FOR_PCC_MACROS_INITIAL_ALLOCATION 1000
			/* Initial allocation size for
			   aux_buffer_for_pcc_macros.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed).  Subsequent reallocations will double the
			   amount previously allocated. */
static char	*after_end_of_aux_buffer_for_pcc_macros;
			/* Pointer to just after the end of
			   aux_buffer_for_pcc_macros. */
#if FULLY_RESOLVED_MACRO_POSITIONS
static a_macro_text_map
		aux_text_map_for_pcc_macros;
			/* Map from offsets into aux_buffer_for_pcc_macros to
			   the original source locations from which the text
			   came, i.e., from the macro definition or macro
			   argument.  Its allocation and use parallel those
			   of aux_buffer_for_pcc_macros. */
#define AUX_TEXT_MAP_FOR_PCC_MACROS_INITIAL_COUNT 500
			/* Initial number of map entries for
			   aux_text_map_for_pcc_macros.  The initial
			   allocation should be such that almost all cases can
			   be accepted (so that the realloc is hardly ever
			   needed).  Subsequent reallocations will double the
			   number of entries previously allocated. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

static a_symbol_ptr
		Pragma_macro_symbol;
			/* Pointer to the symbol entry for the special
			   macro "_Pragma", which is used in modes where
			   the C99-style pragma operator is allowed. */

static a_symbol_ptr
		microsoft_pragma_macro_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__pragma", which is used in Microsoft
			   mode. */

static a_symbol_ptr
		timestamp_macro_symbol;
			/* Pointer to the symbol entry for the Microsoft
			   and GNU __TIMESTAMP__ macro. */
static a_symbol_ptr
		counter_macro_symbol;
			/* Pointer to the symbol entry for the Microsoft
			   and GNU __COUNTER__ macro. */
static unsigned long
		counter_macro_number;
			/* The current value for the Microsoft/GNU
			   __COUNTER__ macro. */

static a_symbol_ptr
		stdc_macro_symbol;
			/* Pointer to the symbol entry for the __STDC__
			   macro.  Only non-NULL when
			   stdc_zero_in_system_headers is TRUE. */

static a_boolean
		stdc_value;
			/* The value to be used for the __STDC__ macro,
			   except when stdc_zero_in_system_headers is TRUE
			   and we are in a system header. */

static a_boolean
		newline_ungotten;
			/* TRUE if a "defined(" operator ends abruptly so
			   that the terminating newline is queued onto a
			   token cache.  Only used by
			   expand_top_level_pcc_macro to avoid skipping
			   white space onto a new source line. */

static a_symbol_ptr
		clang_has_feature_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__has_feature", which is used in clang
			   mode. */

static a_symbol_ptr
		clang_has_extension_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__has_extension", which is used in clang
			   mode. */

static a_symbol_ptr
		has_include_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__has_include", which is used in clang
			   mode and when the portable feature test macros
			   are enabled. */

static a_symbol_ptr
		clang_has_include_next_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__has_include_next", which is used in
			   clang mode. */

static a_symbol_ptr
		clang_has_attribute_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__has_attribute", which is used in clang
			   mode. */

static a_symbol_ptr
		clang_has_builtin_symbol;
			/* Pointer to the symbol entry for the special
			   macro "__has_builtin", which is used in clang
			   mode. */

static a_boolean
		use_raw_version_of_arg;
			/* TRUE if the raw version of a macro argument
			   should be used, FALSE if the expanded version
			   should be used.  Used to support a Microsoft
			   preprocessor idiosyncrasy.  See
			   choose_raw_or_expanded_arg for details. */

static a_source_line_modif_ptr
		top_microsoft_slmp;
			/* In Microsoft mode, points to the top-level
			   source line modification passed to
			   expand_top_level_pcc_macro when that routine is
			   active; NULL otherwise. */

/*
Maximum nesting depth of calls of a single macro in pcc mode.  Used to
catch recursion, but crudely, because a general recursion check is
probably NP-complete.  The test will generate an error in some cases
that involve deep nesting but no recursion, as for example in
  #define x(a) a
  x(x(x(x(x(x(x(x(x(x(x(x  ... etc ... (1))))))))))))
*/
#define MAX_PCC_RECURSIVE_MACRO_DEPTH 300

/*
Declaration for the data structure used to hold values of
arguments to macro calls.
*/
typedef struct a_macro_arg *a_macro_arg_ptr;

typedef struct a_macro_arg {
  a_macro_arg_ptr
		next;
			/* Pointer used when this macro arg is freed
			   and placed on an avail list. */
  sizeof_t	raw_len;
			/* Length of the raw version of the argument, in
			   raw_text, not counting the final LE_END_OF_INSERTION
			   lexical escape. */
  char		*raw_text;
			/* The raw version of the argument text.  Dynamically
			   allocated, expanded as needed.  Contains various
			   escapes (e.g., end-of-token) in addition to the
			   raw text of the tokens. */
#define ARG_RAW_TEXT_INITIAL_ALLOCATION 400
			/* Initial allocation size for raw_text.  The initial
			   allocation should be such that almost all cases
			   can be accepted (so that the realloc is hardly ever
			   needed).  Subsequent reallocations will double the
			   amount previously allocated. */
  sizeof_t	raw_alloc_len;
			/* Allocated size of the raw_text array. */
  char		*initial_raw_text_not_in_primary_source_line;
			/* If non-NULL, points to the first character of the
			   source for the argument when it comes from a
			   macro expansion rather than the primary source line.
			   This is useful to have because it preserves the
			   context needed to determine macro inertness. */
  a_source_line_modif_ptr
		final_modif_for_initial_text;
			/* If initial_raw_text_not_in_primary_source_line is
			   non-NULL, this points to the source line
			   modification from which the rescan returns to the
			   primary source line, if any, or NULL if the scan
			   never returns to the primary source line. */
  sizeof_t	offset_in_raw_text_of_primary_source_line_text;
			/* Offset of the first character in raw_text that
			   comes from the primary source line.  Non-zero
			   only if initial_raw_text_not_in_primary_source_line
			   is non-NULL, and indicates the point at which we
			   return to the primary line.  If we never return
			   to the primary source line, this is the offset
			   of the LE_END_OF_INSERTION escape at the end
			   of the raw_text. */
  sizeof_t	expanded_len;
			/* Length of the expanded version of the argument, in
			   expanded_text, not counting the final
			   LE_END_OF_INSERTION lexical escape. */
  char		*expanded_text;
			/* The macro-expanded version of the argument text.
			   Dynamically allocated, expanded as needed.
			   Contains various escapes (e.g., end-of-token) in
			   addition to the raw text of the tokens. */
#define ARG_EXPANDED_TEXT_INITIAL_ALLOCATION 800
			/* Initial allocation size for expanded_text.  The
			   initial allocation should be such that almost all
			   cases can be accepted (so that the realloc is
			   hardly ever needed).  Subsequent reallocations will
			   double the amount previously allocated. */
  sizeof_t	expanded_alloc_len;
			/* Allocated size of the expanded_text array. */
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_macro_text_map
		raw_text_map;
			/* A map of offsets within raw_text to the original
			   positions (macro definition and arguments) from
			   which they came. */
  a_macro_text_map
		exp_text_map;
			/* A map of offsets within expanded_text to the
			   original positions (macro definition and arguments)
			   from which they came. */
#define MACRO_ARGUMENT_TEXT_MAP_INITIAL_COUNT 10
			/* Initial number of map entries for raw_text_map and
			   exp_text_map.  The initial allocation is kept low
			   to reduce overall memory usage: most macro
			   invocations tend to have only a few tokens per
			   argument, and deeply-nested invocations can result
			   in the allocation of many a_macro_arg entries, so
			   it's best to start small and extend only those maps
			   where the extra entries are actually needed. */
  a_source_position
		comma_pos;
			/* Position of the terminating comma;
			   null_source_position if this is the last
			   argument. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  a_byte_boolean
		is_empty_arg;
			/* The Microsoft preprocessor distinguishes between
			   omitted and empty arguments.  Both have a
			   raw_len of 0, but an empty argument is preceded
			   or followed by a comma -- e.g., for a
			   two-argument macro, M(a) has an omitted second
			   argument, while M(,) has two empty arguments.
			   This affects how stringizing works: a stringized
			   empty argument produces "", while a stringized
			   omitted argument produces nothing. */
} a_macro_arg;

static a_macro_arg_ptr
		avail_macro_args;
			/* List of freed macro arguments available for
			   reuse. */
static a_macro_arg_ptr
		macro_arg_list,
		end_of_macro_arg_list;
			/* All the a_macro_arg entries currently being used. */

static a_const_char
		*arg_get_token_start_of_curr_token;
			/* Set by arg_get_token to point to the position
			   after the white-space skip.  This differs from
			   the value returned in start_of_curr_token when
			   the token is preceded by an inert-macro escape. */

static unsigned long
		macro_name_modif_seq;
			/* During the execution of macro_invocation, this
			   is the sequence id of the source line
			   modification containing the name of the macro
			   being expanded, or 0 if the macro name appears
			   directly in the source code.  This is used by
			   arg_get_token to distinguish between commas that
			   appear directly in the macro argument list and
			   those appearing in expansions of macro
			   arguments. */

static a_boolean
		single_param_macro;
			/* During the execution of macro_invocation, this
			   will be TRUE if and only if the macro being
			   expanded is a function-style macro with exactly
			   one parameter.  This is used by arg_get_token to
			   determine whether a Microsoft-mode "magic" comma
			   should be suppressed or included. */

#if DEBUG
static unsigned long
		num_macro_params_allocated,
		num_macro_defs_allocated,
		num_macro_args_allocated,
		macro_arg_text_space,
		param_name_string_space,
		macro_definition_space;
			/* Used to track space use. */
#endif /* DEBUG */

static a_const_char
		*end_of_cpp_string;
			/* When preprocessing in cpp-compatibility mode,
			   the insides of character constants and string
			   literals in macro definitions are examined for
			   parameter names.  This is implemented here by
			   actually tokenizing the insides of strings.
			   When this flag is non-NULL, we are inside a
			   string, and it points to the closing quote
			   character. */
static a_const_char
		*start_of_white_space_in_cpp_string;
			/* When end_of_cpp_string is non-NULL, this
			   is set by mdefn_get_token to the start of any
			   white space skipped before the current token,
			   or to NULL on the pseudo-token for the opening
			   quote of the string. */


static a_symbol_ptr
	       	date_macro_symbol,
		time_macro_symbol;
			/* Pointers to the symbol entries for the special
			   macros "__DATE__" and "__TIME__". */


#if FULLY_RESOLVED_MACRO_POSITIONS
/*
Declaration for the data structure used to track the mapping from source
positions to target offsets in a buffer that is built incrementally by
copying one token at a time.  When active, these are kept in a singly-linked
list to allow updates to the offsets being tracked if macro_buffer reallocation
results in compaction of a source line modification that is being scanned.
*/
typedef struct a_text_map_position_tracker *a_text_map_position_tracker_ptr;

typedef struct a_text_map_position_tracker {
  a_text_map_position_tracker_ptr
		next_active_tracker;
			/* Pointer to the next active position tracker, or NULL
			   if none. */
  sizeof_t	src_region_starting_offset;
			/* The offset in the source (input line or source
			   line modification inserted text) at which the
			   current region begins. */
  sizeof_t	targ_region_starting_offset;
			/* The offset in the buffer to which token text is
			   being copied corresponding to
			   src_region_starting_offset. */
  sizeof_t	src_region_len;
			/* The length of the source region so far. */
  a_source_line_modif_ptr
		src_slmp;
			/* The source line modification from whose inserted
			   text tokens are being extracted, or NULL if the
			   input is coming from the current source line. */
  a_simple_source_position
		starting_pos;
			/* The source position corresponding to
			   src_region_starting_offset, if tokens are coming
			   from the current source line, or (the
			   simple-source-position part of) null_source_position
			   if input is coming from a source line
			   modification. */
  a_macro_text_map_ptr
		text_map;
			/* The macro text map in which source positions are
			   being accumulated. */
  a_macro_invocation_record_index
		macro_context;
			/* The macro context to set for map entries that are
			   added. */
} a_text_map_position_tracker;

static a_text_map_position_tracker_ptr
		active_text_map_position_trackers;
			/* Pointer to a list of all active position trackers,
			   so they can be updated appropriately after a
                           macro_buffer reallocation. */


void init_macro_text_map(sizeof_t             num_entries,
                         a_macro_text_map_ptr mtmp,
                         a_boolean            resizable)
/*
Initialize a macro text map object with room for num_entries macro text map
entries.  If resizable is TRUE, the space will be allocated so that the array
can be extended to accommodate more entries; otherwise, it will be allocated
in front-end memory so it can be saved in a precompiled header.
*/
{
  if (num_entries != 0) {
    if (resizable) {
      /* The map can grow -- use alloc resizable_buffer. */
      mtmp->entries = (a_macro_text_map_entry_ptr)alloc_resizable_buffer(
                       (sizeof_t)(num_entries*sizeof(a_macro_text_map_entry)));
    } else {
      /* The map is fixed size -- use alloc_fe. */
      mtmp->entries = (a_macro_text_map_entry_ptr)alloc_fe(
                       (sizeof_t)(num_entries*sizeof(a_macro_text_map_entry)));
      }  /* if */
  } else {
    /* No entries for now (i.e., will probably be just a reference to entries
       that are part of another text map). */
    mtmp->max_entries = 0;
    mtmp->entries = NULL;
  }  /* if */
  mtmp->max_entries = num_entries;
  mtmp->num_entries = 0;
  mtmp->resizable = resizable;
}  /* init_macro_text_map */


static void ensure_avail_text_map_entries(a_macro_text_map_ptr mtmp,
                                          sizeof_t             num_entries)
/*
Make sure that the text map designated by mtmp has at least num_entries free
map entries, extending the array of entries if necessary.
*/
{
  if (mtmp->num_entries + num_entries > mtmp->max_entries) {
    /* The entry array is full; double (at least) the size (implicitly copying
       the previous contents). */
    a_macro_text_map_entry_ptr old_first_entry = mtmp->entries;
    a_macro_text_map_entry_ptr after_old_last_entry =
                                             &mtmp->entries[mtmp->num_entries];
    a_boolean                  source_line_modifs_need_adjustment =
                                                     (mtmp == &macro_text_map);
    sizeof_t                   new_max_entries = mtmp->max_entries * 2;
    /* Make sure this map can be extended. */
    check_assertion(mtmp->resizable);
    if (mtmp->num_entries + num_entries > new_max_entries) {
      /* Doubling wasn't enough for the requested number.  This only happens
         when a block of entries is desired (e.g., for copying the
         aux_text_map_for_pcc_macros to macro_text_map); because that is for
         the exact number that will be needed, there is no need to provide a
         "cushion" in the allocation. */
      new_max_entries = mtmp->num_entries + num_entries;
    }  /* if */
    if (((sizeof_t)-1) / sizeof(a_macro_text_map_entry) <= new_max_entries) {
      /* Calculation of the new requested size would overflow, resulting in
         a too-short buffer and overwriting memory. */
      catastrophe(ec_requested_size_too_large);
    }  /* if */
    mtmp->entries = (a_macro_text_map_entry_ptr)realloc_buffer(
                                   (char *)mtmp->entries,
                                   (sizeof_t)(mtmp->max_entries*
                                              sizeof(a_macro_text_map_entry)),
                                   (sizeof_t)(new_max_entries*
                                              sizeof(a_macro_text_map_entry)));
    if (source_line_modifs_need_adjustment) {
      /* We reallocated the entries for macro_text_map.  The text maps
         in source line modifications do not have their own entries
         but point to subranges of the entries in macro_text_map, so
         those pointers must be adjusted to point into the reallocated
         array. */
      a_source_line_modif_ptr slmp;
      for (slmp = source_line_modif_list; slmp != NULL; slmp = slmp->next) {
        if (ptr_in_range(slmp->text_map.entries, old_first_entry,
                         after_old_last_entry)) {
          slmp->text_map.entries = &mtmp->entries[slmp->text_map.entries -
                                                  old_first_entry];
        }  /* if */
      }  /* for */
    }  /* if */
    mtmp->max_entries = new_max_entries;
  }  /* if */
}  /* ensure_avail_text_map_entries */


static a_macro_text_map_entry_ptr next_macro_text_map_entry(
                                                     a_macro_text_map_ptr mtmp)
/*
Return a pointer to the next free macro text map entry in the specified map.
*/
{
  ensure_avail_text_map_entries(mtmp, 1);
  return &mtmp->entries[mtmp->num_entries++];
}  /* next_macro_text_map_entry */


#if !RECORD_MACRO_INVOCATIONS
/*ARGSUSED*/  /* <-- macro_context is not used in that case. */
#endif /* RECORD_MACRO_INVOCATIONS */
static void add_entry_to_macro_text_map(
                               a_macro_text_map_ptr            mtmp,
                               sizeof_t                        start_of_region,
                               a_seq_number                    seq,
                               a_column_number                 column,
                               a_macro_invocation_record_index macro_context)
/*
Add a new entry to the specified macro text map with the specified offset,
sequence number, column, and macro context.
*/
{
  a_macro_text_map_entry_ptr mtmep = next_macro_text_map_entry(mtmp);

  mtmep->start_of_region = start_of_region;
  mtmep->corresponding_source_pos.seq = seq;
  mtmep->corresponding_source_pos.column = column;
#if RECORD_MACRO_INVOCATIONS
  mtmep->macro_context = macro_context;
#endif /* RECORD_MACRO_INVOCATIONS */
}  /* add_entry_to_macro_text_map */


#if !RECORD_MACRO_INVOCATIONS
/*ARGSUSED*/  /* <-- macro_context is not used in that case. */
#endif /* !RECORD_MACRO_INVOCATIONS */
static void clone_macro_text_map_entries(
                          a_macro_text_map_ptr            src_map,
                          sizeof_t                        starting_src_offset,
                          sizeof_t                        src_region_len,
                          a_macro_text_map_ptr            targ_map,
                          sizeof_t                        starting_targ_offset,
                          a_macro_invocation_record_index macro_context)
/*
Copy the range of macro text map entries in the region designated by
starting_src_offset and src_region_len from src_map to targ_map, adjusting
the offsets appropriately.  If RECORD_MACRO_INVOCATIONS is TRUE, the
macro_context of the positions in the new text map entries will be set to
the value specified by macro_context unless it has the value
NO_PARENT_MACRO_INVOCATION; in that case, the macro_context value from the
source entry will be preserved.
*/
{
  a_macro_text_map_entry_ptr      mtmep;
  a_macro_invocation_record_index ctx = macro_context;
  a_column_number                 adjusted_column;

  /* Call bsearch to find the macro text map entry that covers the specified
     source buffer starting offset. */
  mtmep = (a_macro_text_map_entry_ptr)bsearch(
                                     (a_bsearch_arg_type)&starting_src_offset,
                                     (a_bsearch_arg_type)src_map->entries,
                                     size_t_arg(src_map->num_entries-1),
                                     sizeof(a_macro_text_map_entry),
                                     compare_macro_text_map_entry_with_offset);
  check_assertion_str2(mtmep != NULL, "clone_macro_text_map_entries",
                       "offset not found");
#if RECORD_MACRO_INVOCATIONS
  if (macro_context == NO_PARENT_MACRO_INVOCATION) {
    /* Copy the context from the source entry. */
    ctx = mtmep->macro_context;
  }  /* if */
#endif /* RECORD_MACRO_INVOCATIONS */
  if (mtmep->corresponding_source_pos.seq == 0) {
    /* Don't change the column number -- it's a special flag, not an actual
       column number. */
    adjusted_column = mtmep->corresponding_source_pos.column;
  } else {
    /* The first map entry requires special treatment because we may only be
       cloning a part of its region, i.e., the source buffer starting offset
       might not correspond to the starting location of the map entry,
       requiring the column of the corresponding source position to be adjusted
       accordingly. */
    adjusted_column = mtmep->corresponding_source_pos.column +
                                (starting_src_offset - mtmep->start_of_region);
  }  /* if */
  add_entry_to_macro_text_map(targ_map, starting_targ_offset,
                              mtmep->corresponding_source_pos.seq,
                              adjusted_column, ctx);
  /* Now just loop through the source map entries.  The start_of_region for
     each target map entry will be at the same relative offset to the starting
     target offset as the source entry's start of region is to the starting
     source offset.  The corresponding source position will be identical. */
  ++mtmep;
  check_assertion_str2(mtmep < src_map->entries + src_map->num_entries,
                       "clone_macro_text_map_entries",
                       "map entry pointer past end of entries array");
  while (mtmep->start_of_region < starting_src_offset + src_region_len) {
#if RECORD_MACRO_INVOCATIONS
    if (macro_context == NO_PARENT_MACRO_INVOCATION) {
      /* Copy the context from the source entry. */
      ctx = mtmep->macro_context;
    }  /* if */
#endif /* RECORD_MACRO_INVOCATIONS */
    add_entry_to_macro_text_map(targ_map, starting_targ_offset +
                                (mtmep->start_of_region - starting_src_offset),
                                mtmep->corresponding_source_pos.seq,
                                mtmep->corresponding_source_pos.column, ctx);
    ++mtmep;
    check_assertion_str2(mtmep < src_map->entries + src_map->num_entries,
                         "clone_macro_text_map_entries",
                         "map entry pointer past end of entries array");
  }  /* while */
}  /* clone_macro_text_map_entries */


static void init_text_map_position_tracker(
                                a_text_map_position_tracker_ptr  tmpt,
                                a_macro_text_map_ptr             text_map,
                                a_macro_invocation_record_index  macro_context)
/*
Add the specified position tracker to the list of active trackers and
initialize it with the offset of the current token as target offset 0 and
referring to the specified macro text map.  If macro_context is
NO_PARENT_MACRO_INVOCATION, the context from any cloned entries will be
preserved; otherwise, the specified context will be saved in newly-created
text map entries.
*/
{
  a_const_char *adj_start_of_curr_token = start_of_curr_token;

  tmpt->next_active_tracker = active_text_map_position_trackers;
  active_text_map_position_trackers = tmpt;
  if (!within_curr_source_line(start_of_curr_token)) {
    a_source_line_modif_ptr slmp =
                                  assoc_source_line_modif(start_of_curr_token);
    if (slmp->is_whitespace_kwd) {
      /* This is the canonical representation of a whitespace keyword.  Use
         its parent or the source line, as appropriate, for tracking. */
      adj_start_of_curr_token = loc_of_insert(slmp);
    }  /* if */
  }  /* if */
  if (within_curr_source_line(adj_start_of_curr_token)) {
    /* Tokens are coming from the current source line. */
    tmpt->src_region_starting_offset = adj_start_of_curr_token -
                                                              curr_source_line;
    tmpt->src_slmp = NULL;
    tmpt->starting_pos.seq = pos_curr_token.seq;
    tmpt->starting_pos.column = pos_curr_token.column;
  } else {
    /* Tokens are coming from a source line modification. */
    tmpt->src_slmp = assoc_source_line_modif(adj_start_of_curr_token);
    ++tmpt->src_slmp->num_active_position_trackers;
    tmpt->src_region_starting_offset = adj_start_of_curr_token -
                                                 tmpt->src_slmp->inserted_text;
    tmpt->starting_pos.seq = 0;
    tmpt->starting_pos.column = SP_COL_UNKNOWN;
  }  /* if */
  tmpt->targ_region_starting_offset = 0;
  tmpt->src_region_len = 0;
  tmpt->text_map = text_map;
  tmpt->macro_context = macro_context;
}  /* init_text_map_position_tracker */


static void add_token_part_to_macro_text_map(
                             a_text_map_position_tracker_ptr tmpt,
                             a_const_char                    *token_part_start,
                             a_source_position               *token_part_pos,
                             a_boolean                       force_new_region,
                             sizeof_t                        next_targ_offset)
/*
This routine is called for each token or portion of a token that is to
be added to a buffer with which a macro text map is associated.  It updates
the macro text map pointed to by tmpt->text_map to reflect copying the
current token into the target buffer at next_targ_offset and makes any
necessary adjustments to the state information in *tmpt.  token_part_start
points to the start of the token or portion thereof.  token_part_pos is
the source position of token_part_start.  force_new_region is TRUE if a new
text map entry must be created.
*/
{
  sizeof_t     rel_src_offset = 0;
  sizeof_t     rel_targ_offset;
  a_boolean    new_region_required = FALSE;
  a_const_char *adj_start_of_curr_token = start_of_curr_token;

  if (tmpt->src_slmp != NULL) {
    /* Previous tokens were from the inserted text of a source line
       modification. */
    a_boolean in_curr_slmp = ptr_in_range(token_part_start,
                                          tmpt->src_slmp->inserted_text,
                                          tmpt->src_slmp->end_inserted_text);
    if (force_new_region) {
      new_region_required = TRUE;
    } else if (!in_curr_slmp) {
      /* If the new token is not part of the inserted_text of the source line
         modification from which the previous tokens came, we need a new
         region. */
      new_region_required = TRUE;
    } else {
      /* Check to see if the new token will be at the same relative offset in
         the target as it was in the source; if not, we need a new region. */
      rel_src_offset = token_part_start -
              tmpt->src_slmp->inserted_text - tmpt->src_region_starting_offset;
      rel_targ_offset = next_targ_offset - tmpt->targ_region_starting_offset;
      new_region_required = (rel_src_offset != rel_targ_offset);
    }  /* if */
    if (new_region_required) {
      /* The new token will not be covered by the range of the current map
         entry -- clone the src_slmp map entries and set up for the new
         token. */
      clone_macro_text_map_entries(&tmpt->src_slmp->text_map,
                                   tmpt->src_region_starting_offset,
                                   tmpt->src_region_len, tmpt->text_map,
                                   tmpt->targ_region_starting_offset,
                                   tmpt->macro_context);
      if (!in_curr_slmp) {
        /* Release the associated source line modification and switch to
           the new one, if any. */
        --tmpt->src_slmp->num_active_position_trackers;
        if (!within_curr_source_line(token_part_start)) {
          tmpt->src_slmp = assoc_source_line_modif(token_part_start);
          if (tmpt->src_slmp->is_whitespace_kwd) {
            /* This is the canonical representation of a whitespace keyword,
               which has no map.  Use the location of the original text of
               the keyword. */
            if (token_part_start == tmpt->src_slmp->inserted_text) {
              token_part_start = loc_of_insert(tmpt->src_slmp);
            } else {
              check_assertion(token_part_start ==
                                        tmpt->src_slmp->end_inserted_text - 1);
              token_part_start = loc_of_insert(tmpt->src_slmp) +
                                       tmpt->src_slmp->num_chars_to_delete - 1;
            }  /* if */
            adj_start_of_curr_token = loc_of_insert(tmpt->src_slmp);
            tmpt->src_slmp = parent_source_line_modif(tmpt->src_slmp);
          }  /* if */
        }  /* if */
      }  /* if */
      if (within_curr_source_line(token_part_start)) {
        /* We fell out of the source line modification back to the original
           source line. */
        tmpt->src_region_starting_offset = token_part_start - curr_source_line;
        tmpt->src_slmp = NULL;
        tmpt->starting_pos.seq = token_part_pos->seq;
        tmpt->starting_pos.column = token_part_pos->column;
      } else {
        /* We're still in a source line modification. */
        if (!in_curr_slmp) {
          /* We entered or re-entered a different source line modification
             from the one we were in. */
          ++tmpt->src_slmp->num_active_position_trackers;
        }  /* if */
        tmpt->src_region_starting_offset = token_part_start -
                                                 tmpt->src_slmp->inserted_text;
      }  /* if */
      tmpt->targ_region_starting_offset = next_targ_offset;
      rel_src_offset = 0;
    }  /* if */
  } else {
    /* Previous tokens were from the current source line. */
    a_boolean in_same_source_line =
                               within_curr_source_line(token_part_start) &&
                               tmpt->starting_pos.seq == token_part_pos->seq;
    if (force_new_region) {
      new_region_required = TRUE;
    } else if (!in_same_source_line) {
      /* If the new token is not part of the same source line from which the
         previous tokens came, we need a new region. */
      new_region_required = TRUE;
    } else {
      /* Check to see if the new token will be at the same relative offset in
         the target as it was in the source; if not, we need a new region. */
      rel_src_offset = token_part_start - curr_source_line -
                                              tmpt->src_region_starting_offset;
      rel_targ_offset = next_targ_offset - tmpt->targ_region_starting_offset;
      new_region_required = (rel_src_offset != rel_targ_offset);
    }  /* if */
    if (new_region_required) {
      /* The new token will not be covered by the range of the current map
         entry -- step to a new entry. */
      add_entry_to_macro_text_map(tmpt->text_map,
                                  tmpt->targ_region_starting_offset,
                                  tmpt->starting_pos.seq,
                                  tmpt->starting_pos.column,
                                  tmpt->macro_context);
      if (within_curr_source_line(token_part_start)) {
        /* The new token is still in the current source line. */
        tmpt->src_region_starting_offset = token_part_start - curr_source_line;
        tmpt->starting_pos.seq = token_part_pos->seq;
        tmpt->starting_pos.column = token_part_pos->column;
      } else {
        /* We've entered a source line modification. */
        tmpt->src_slmp = assoc_source_line_modif(token_part_start);
        ++tmpt->src_slmp->num_active_position_trackers;
        tmpt->starting_pos.seq = 0;
        tmpt->starting_pos.column = SP_COL_UNKNOWN;
        tmpt->src_region_starting_offset = token_part_start -
                                                 tmpt->src_slmp->inserted_text;
      }  /* if */
      tmpt->targ_region_starting_offset = next_targ_offset;
      rel_src_offset = 0;
    }  /* if */
  }  /* if */
  tmpt->src_region_len = rel_src_offset + len_of_curr_token -
                                  (token_part_start - adj_start_of_curr_token);
  check_assertion(!(tmpt->src_slmp != NULL &&
                    tmpt->src_slmp->inserted_text + tmpt->src_region_len >
                    tmpt->src_slmp->end_inserted_text));
}  /* add_token_part_to_macro_text_map */


static void add_token_to_macro_text_map(
                              a_text_map_position_tracker_ptr tmpt,
                              sizeof_t                        next_targ_offset)
/*
This routine is called for each token to be added to a buffer with which a
macro text map is associated.  It calls add_token_part_to_macro_text_map
for the start of the token and again for the character following any
multibyte character in the token.
*/
{
  add_token_part_to_macro_text_map(tmpt, start_of_curr_token, &pos_curr_token,
                                   /*force_new_region=*/FALSE,
                                   next_targ_offset);
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  /* Check if this token is in the source line and contains multibyte
     characters.  (The offset calculations for multibyte characters
     appearing in source line modifications have already been reflected in
     the source text map entries that will be cloned for this token and
     should not be repeated.)  The only token shorter than three characters
     that can contain a multibyte sequence is an identifier. */
  if (multibyte_chars_in_source_enabled &&
      within_curr_source_line(start_of_curr_token) &&
      (end_of_curr_token - start_of_curr_token > 2 ||
       curr_token == tok_identifier)) {
    a_source_position     token_part_pos = pos_curr_token;
    a_const_char          *ptr = start_of_curr_token;
    /* Step through the characters of the token. */
    for (;;) {
      int numch = mbc_length_simple(ptr);
      ptr += numch;
      /* Stop when we reach the end of the token.  If the last character of
         the token is a multibyte character a new region will be forced
         elsewhere for the token that follows (if any). */
      if (ptr > end_of_curr_token) break;
      next_targ_offset += numch;
      /* The column number is only incremented for each logical character. */
      token_part_pos.column++;
      if (numch > 1) {
        /* A multibyte character was found.  Start a new text map entry for
           the character that follows. */
        add_token_part_to_macro_text_map(tmpt, ptr, &token_part_pos,
                                         /*force_new_region=*/TRUE,
                                         next_targ_offset);
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
}  /* add_token_to_macro_text_map */


static void terminate_macro_text_map(
                              a_text_map_position_tracker_ptr tmpt,
                              sizeof_t                        after_end_offset)
/*
Complete the macro_text_map pointed to by tmpt->text_map by adding the final
region that was in process and a terminating region at the after_end_offset,
then remove it from the list of active trackers.
*/
{
  if (tmpt->src_slmp != NULL) {
    /* The mapped text is from a source line modification -- clone the
       regions from its text map. */
    clone_macro_text_map_entries(&tmpt->src_slmp->text_map,
                                 tmpt->src_region_starting_offset,
                                 tmpt->src_region_len,
                                 tmpt->text_map,
                                 tmpt->targ_region_starting_offset,
                                 tmpt->macro_context);
    /* Release the associated source line modification. */
    --tmpt->src_slmp->num_active_position_trackers;
  } else {
    /* The mapped text is from the current source line -- add a single
       region for the last token(s). */
    add_entry_to_macro_text_map(tmpt->text_map,
                                tmpt->targ_region_starting_offset,
                                tmpt->starting_pos.seq,
                                tmpt->starting_pos.column,
                                tmpt->macro_context);
  }  /* if */
  /* Add a terminating region to allow the bsearch routine to access the
     "next" region when searching for the last token. */
  add_entry_to_macro_text_map(tmpt->text_map, after_end_offset,
                              (a_seq_number)0, SP_COL_UNKNOWN,
                              NO_PARENT_MACRO_INVOCATION);
  /* This tracker should be at the top of the stack. */
  check_assertion(tmpt == active_text_map_position_trackers);
  active_text_map_position_trackers = tmpt->next_active_tracker;
}  /* terminate_macro_text_map */


static void adjust_macro_text_map_after_compaction(
                                     a_source_line_modif_ptr slmp,
                                     sizeof_t                orig_deletion_len,
                                     sizeof_t                new_deletion_len,
                                     sizeof_t                deletion_offset)
/*
Adjust the offsets in a source line modification's macro text map to reflect
the removal of deleted characters resulting from reallocating macro_buffer.
*/
{
  sizeof_t i;
  sizeof_t num_compacted_characters = orig_deletion_len - new_deletion_len;

  for (i = 0; i < slmp->text_map.num_entries; ++i) {
    if (slmp->text_map.entries[i].start_of_region < deletion_offset) {
      /* Do nothing -- this entry refers to text before the deletion and thus
         is not affected by the compaction. */
    } else if (slmp->text_map.entries[i].start_of_region >=
               deletion_offset + orig_deletion_len) {
      /* This entry refers to text after the deletion: adjust the offset by
         the number of compacted characters. */
      slmp->text_map.entries[i].start_of_region -= num_compacted_characters;
    } else {
      /* This entry refers to text in the deletion and thus would never be
         used: move the offset to the beginning of the deletion to keep it
         out of consideration. */
      slmp->text_map.entries[i].start_of_region = deletion_offset;
    }  /* if */
  }  /* for */
  if (slmp->num_active_position_trackers > 0) {
    /* There are active text map position trackers referring to this
       source line modification.  Adjust their current source offset if it
       is past the location of the compacted deletion. */
    a_text_map_position_tracker_ptr tmpt;
    for (tmpt = active_text_map_position_trackers; tmpt != NULL;
         tmpt = tmpt->next_active_tracker) {
      if (tmpt->src_slmp == slmp &&
          tmpt->src_region_starting_offset >=
                                         deletion_offset + orig_deletion_len) {
        /* The tracker source starting offset must be adjusted to allow for
           the compaction. */
        tmpt->src_region_starting_offset -= num_compacted_characters;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* adjust_macro_text_map_after_compaction */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */


#if RECORD_MACRO_INVOCATIONS
a_macro_invocation_record_ptr macro_invocation_record_at_index(
                                         a_macro_invocation_record_index index)
/*
Return a pointer to the macro invocation record with the specified index
(NULL if the index is out of the range of existing macro invocation records).

At the end of front-end processing, when the number of entries is known, the
list of macro invocation record blocks is transformed into a binary tree to
allow for relatively-efficient random access.  Until that point, however, the
blocks are simply maintained on a doubly-linked list, and only a pointer to
the tail of the list is kept.  This is acceptable because the only indexed
access to the list is in the case of diagnostic output, which does not need
to be especially efficient, and even then the messages will usually refer to
the most recent macro invocations, near the tail of the list.  In cases where
no diagnostic output is needed, balancing the tree once at the end of
processing is more efficient than maintaining a balanced tree throughout the
growth of the list.
*/
{
  a_macro_invocation_record_block_ptr mirbp;
  a_macro_invocation_record_ptr       mirp = NULL;
  int                                 index_in_block;

  if (index >= 0 && index < num_macro_invocation_records) {
    for (mirbp = last_macro_invocation_record_block;
         mirbp != NULL && mirbp->first_record_in_block > index;
         mirbp = mirbp->prev) {}
    index_in_block = index - mirbp->first_record_in_block;
    mirp = mirbp->records + index_in_block;
  }  /* if */
  return mirp;
}  /* macro_invocation_record_at_index */


static void checkpoint_macro_invocation_record(void)
/*
Save state variables to allow reestablishing the current macro invocation
record, in case a macro invocation in progress needs to be canceled.  See
revert_macro_invocation_record.
*/
{
  ckpt_last_macro_invocation_record_block = last_macro_invocation_record_block;
  ckpt_num_macro_invocation_records = num_macro_invocation_records;
  ckpt_depth_of_curr_macro_invocation_record =
                                         depth_of_curr_macro_invocation_record;
  ckpt_max_macro_invocation_depth = max_macro_invocation_depth;
}  /* checkpoint_macro_invocation_record */


static void revert_macro_invocation_record(void)
/*
Restore the list of macro invocation records to its state before the current
invocation was registered, reflecting the cancellation of the most recent
macro invocation.
*/
{
  /* Make sure a previous state was checkpointed. */
  check_assertion(ckpt_last_macro_invocation_record_block != NULL);
  if (ckpt_last_macro_invocation_record_block !=
                                          last_macro_invocation_record_block) {
    /* Registering the most recent invocation overflowed into a new block.
       Roll back to the previous one, saving the current block for later
       reuse. */
    saved_next_macro_invocation_record_block =
                                            last_macro_invocation_record_block;
    last_macro_invocation_record_block =
                                       ckpt_last_macro_invocation_record_block;
  }  /* if */
  num_macro_invocation_records = ckpt_num_macro_invocation_records;
  depth_of_curr_macro_invocation_record =
                                    ckpt_depth_of_curr_macro_invocation_record;
  max_macro_invocation_depth = ckpt_max_macro_invocation_depth;
  /* Ensure that this function cannot be called twice without an intervening
     call to register_macro_invocation. */
  ckpt_last_macro_invocation_record_block = NULL;
}  /* revert_macro_invocation_record */


static a_macro_invocation_record_ptr next_macro_invocation_record(void)
/*
Return a pointer to the next available macro invocation record, allocating a
new macro invocation record block if necessary.
*/
{
  int index_in_block;

  if (last_macro_invocation_record_block == NULL ||
      num_macro_invocation_records -
                   last_macro_invocation_record_block->first_record_in_block ==
                   MACRO_INVOCATION_RECORDS_PER_BLOCK) {
    /* Need a new block for the next record. */
    a_macro_invocation_record_block_ptr new_block;
    if (saved_next_macro_invocation_record_block != NULL) {
      new_block = saved_next_macro_invocation_record_block;
      saved_next_macro_invocation_record_block = NULL;
    } else {
      new_block = alloc_macro_invocation_record_block();
    }  /* if */
    if (last_macro_invocation_record_block != NULL) {
      last_macro_invocation_record_block->next = new_block;
    }  /* if */
    new_block->prev = last_macro_invocation_record_block;
    last_macro_invocation_record_block = new_block;
    new_block->first_record_in_block = num_macro_invocation_records;
  }  /* if */
  index_in_block = num_macro_invocation_records -
                     last_macro_invocation_record_block->first_record_in_block;
  if (num_macro_invocation_records++ == 0) {
    /* Skip the zeroth element so NO_PARENT_MACRO_INVOCATION can have the
       value 0. */
    ++index_in_block;
    ++num_macro_invocation_records;
  }  /* if */
  return last_macro_invocation_record_block->records + index_in_block;
}  /* next_macro_invocation_record */


static a_macro_invocation_record_index register_macro_invocation(
                                a_macro_invocation_record_index parent_index,
                                unsigned long                   stack_depth,
                                a_macro_def_ptr                 mdp,
                                a_source_position_ptr           macro_name_pos,
                                a_macro_invocation_record_ptr   *mirpp)
/*
Add an entry to the list of macro invocation records, reflecting the invocation
of the macro indicated by mdp.  parent_index gives the index of the macro
invocation record in whose expansion the current invocation begins or
NO_PARENT_MACRO_INVOCATION if the invocation occurs directly in source text
(invocations in an argument to a given macro are treated as if they occurred in
the expansion of that macro).  stack_depth is the depth of the invocation
stack after pushing the current invocation (i.e., it will always be at least
1), and macro_name_pos gives the position of the macro name in this macro
invocation.  The return value is the index of the newly-added record, and
*mirpp is set to point to that record, as well.
*/
{
  a_macro_invocation_record_ptr mirp;

  /* Save the current state to allow reverting in case the current invocation
     is canceled. */
  checkpoint_macro_invocation_record();
  mirp = next_macro_invocation_record();
  if (depth_of_curr_macro_invocation_record > 0 &&
      stack_depth < depth_of_curr_macro_invocation_record - 1) {
    /* In order to make a tree-like traversal of the invocation records easier,
       whenever a transition reflects popping more than one stack level, we
       must add a placeholder record reflecting the number of levels popped in
       the transition. */
    mirp->parent_macro_index = stack_depth -
                                         depth_of_curr_macro_invocation_record;
    mirp->assoc_macro = NULL;
    mirp = next_macro_invocation_record();
  }  /* if */
  mirp->parent_macro_index = parent_index;
  mirp->assoc_macro = mdp->macro;
  /* Record the original location of the macro name in this invocation. */
  mirp->start.seq = macro_name_pos->orig_seq;
  mirp->start.column = macro_name_pos->orig_column;
  if (stack_depth > max_macro_invocation_depth) {
    max_macro_invocation_depth = stack_depth;
  }  /* if */
  depth_of_curr_macro_invocation_record = stack_depth;
  *mirpp = mirp;
  return num_macro_invocation_records - 1;
}  /* register_macro_invocation */

#if MACRO_INVOCATION_TREE_IN_IL

static a_macro_invocation_record_block_ptr create_macro_inv_record_tree(
                         a_macro_invocation_record_block_ptr mirbp,
                         a_macro_invocation_record_index     first_record,
                         a_macro_invocation_record_index     after_last_record)
/*
Recursively arrange the list of macro invocation record blocks containing the
indices first_record through after_last_record-1 into a binary tree and return
the address of the root block.  mirbp is a pointer to an arbitrary macro
invocation record block.
*/
{
  unsigned long                   num_blocks_in_tree;
  unsigned long                   num_blocks_in_left_subtree;
  a_macro_invocation_record_index first_record_in_root_block;

  if (mirbp != NULL) {
    num_blocks_in_tree = (after_last_record - first_record + 
                          MACRO_INVOCATION_RECORDS_PER_BLOCK - 1)
                         / MACRO_INVOCATION_RECORDS_PER_BLOCK;
    num_blocks_in_left_subtree = num_blocks_in_tree/2;
    first_record_in_root_block = first_record +
               num_blocks_in_left_subtree * MACRO_INVOCATION_RECORDS_PER_BLOCK;
    /* mirbp might point either before or after the root block, so we need both
       of the following loops to find it. */
    while (mirbp->first_record_in_block < first_record_in_root_block) {
      mirbp = mirbp->next;
    }  /* while */
    while (mirbp->first_record_in_block > first_record_in_root_block) {
      mirbp = mirbp->prev;
    }  /* while */
    /* Recursively create subtrees. */
    if (num_blocks_in_left_subtree != 0) {
      mirbp->left_subtree =
                      create_macro_inv_record_tree(mirbp, first_record,
                                                   first_record_in_root_block);
    }  /* if */
    if (num_blocks_in_tree > num_blocks_in_left_subtree + 1) {
      mirbp->right_subtree =
               create_macro_inv_record_tree(mirbp,
                                            mirbp->next->first_record_in_block,
                                            after_last_record);
    }  /* if */
  }  /* if */
  return mirbp;
}  /* create_macro_inv_record_tree */


void copy_macro_invocation_tree_to_il(void)
/*
Create the binary tree of macro invocation record blocks, copying the
macro_invocation_records list, and set the appropriate fields in il_header.
*/
{
  il_header.num_macro_invocation_records = num_macro_invocation_records;
  il_header.max_macro_invocation_depth = max_macro_invocation_depth;
  il_header.root_macro_invocation_record_block =
               create_macro_inv_record_tree(last_macro_invocation_record_block,
                                            /*first_record=*/
                                            (a_macro_invocation_record_index)0,
                                            num_macro_invocation_records);
}  /* copy_macro_invocation_tree_to_il */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#endif /* RECORD_MACRO_INVOCATIONS */


void adjust_curr_source_line_structure_after_realloc(
                                        a_const_char *old_ptr,
                                        a_const_char *old_after_end_ptr,
                                        a_const_char *new_ptr,
                                        a_boolean    adjust_source_line_modifs)
/*
Walk the data structure associated with curr_source_line, and change any
pointers that point in the range old_ptr..old_after_end_ptr (the latter
pointer pointing to just after the last byte) to point instead to the
area following new_ptr.  This is necessary because the area that was
at old_ptr has been realloc'd (to change its size), and new_ptr is the
new address for the area.  A caller that needs special processing of source
line modifications can pass adjust_source_line_modifs as FALSE; otherwise,
the pointers in all source line modifications will be adjusted as needed.
*/
{
  an_orig_line_modif_ptr       olmp;
  a_source_line_modif_ptr      slmp;
  a_macro_arg_ptr              map;
  a_pointer_registration_ptr   prp;
  a_const_char                 *old_after_end_plus_1;

/* Macro to adjust a single pointer if it needs it.  Include the address
   just past the end of the area moved, since a pointer to there should be
   adjusted.  Recall that an extra byte is allocated at the end of each
   area so that that address will not be the same as the start address of
   the area following it in memory. */
#define fix_ptr(ptr)                                                         \
{ /* Suppress the warning on use of the expired pointer value in CodeCenter. \
     Version 3.1.1 warning number. */                                        \
  /*SUPPRESS 29*/                                                            \
  if (ptr != NULL && ptr_in_range(ptr, old_ptr, old_after_end_plus_1)) {     \
    *(a_const_char **)&ptr = ptr - old_ptr + new_ptr;                        \
  }  /* if */                                                                \
}  /* fix_ptr */

  db_enter(4, "adjust_curr_source_line_structure_after_realloc");

  check_assertion(old_ptr != NULL);  
  /* If the area didn't move, it's not necessary to walk the structure. */
  /* Suppress the warning on use of the expired pointer value in CodeCenter.
     Version 3.1.1 warning number. */
  /*SUPPRESS 29*/
  if (old_ptr != new_ptr) {
    old_after_end_plus_1 = old_after_end_ptr + 1;
    /* Walk the original line modif list (which represents trigraphs and
       line splices).  This is only necessary if it's curr_source_line
       that has been relocated, but it doesn't cost much to do it in all
       cases. */
    for (olmp = orig_line_modif_list; olmp != NULL; olmp = olmp->next) {
      fix_ptr(olmp->line_loc);
    }  /* for */
    if (adjust_source_line_modifs) {
      /* Walk the source line modif list (which represents macro expansions
         and comment deletions). */
      for (slmp = source_line_modif_list; slmp != NULL; slmp = slmp->next) {
        a_concatenation_record_ptr crp;
        if (slmp->line_loc != NULL &&
            ptr_in_range(slmp->line_loc, old_ptr, old_after_end_plus_1)) {
          rem_source_line_modif_from_hash_table(slmp);
          fix_ptr(slmp->line_loc);
          add_source_line_modif_to_hash_table(slmp);
        }  /* if */
        fix_ptr(slmp->inserted_text);
        fix_ptr(slmp->end_inserted_text);
        for (crp = slmp->concatenations; crp != NULL; crp = crp->next) {
          fix_ptr(crp->line_loc);
        }  /* for */
      }  /* for */
    }  /* if */
    /* Fix pointers in the macro argument entries. */
    for (map = macro_arg_list; map != NULL; map = map->next) {
      fix_ptr(map->initial_raw_text_not_in_primary_source_line);
    }  /* for */
    /* Adjust global variables that point into the curr_source_line
       structure. */
    fix_ptr(curr_char_loc);
    fix_ptr(delete_source_from_loc);
    fix_ptr(start_of_curr_token);
    fix_ptr(end_of_curr_token);
    fix_ptr(arg_get_token_start_of_curr_token);
#if ASM_SUPPORT_NEEDED
    fix_ptr(prev_asm_stop_char);
#endif /* ASM_SUPPORT_NEEDED */
    /* Adjust local variables that point into the curr_source_line structure.
       Such variables are registered by calling register_pointer_variable. */
    for (prp = registered_pointers; prp != NULL; prp = prp->next) {
      char **ptr_ptr = prp->ptr_variable;
      fix_ptr(*ptr_ptr);
    }  /* for */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
    /* If we are reallocating the curr_source_line array, adjust the
       logical_char_info array, which contains pointers into
       curr_source_line. */
    if (old_ptr == curr_source_line) {
      int	idx;
      for (idx = 0; idx < logical_char_info_entries_used; idx++) {
        fix_ptr(logical_char_info[idx]);
      }  /* for */
    }  /* if */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  }  /* if */
  db_exit();
}  /* adjust_curr_source_line_structure_after_realloc */


static char* copy_attention_markers(a_source_line_modif_ptr slmp,
                                    char                    *next_avail)
/*
Scan the deleted text of the specified source line modification for
ATTENTION_MARKERs and copy them into the buffer designated by next_avail.
(The one at slmp->line_loc has already been copied into the destination
buffer.)  If a nested ATTENTION_MARKER is found, this routine copies it into
the destination buffer and then calls itself recursively for the corresponding
source line modification.  Finally, the supplied source line modification's
line_loc is relocated to point to the copy and slmp->num_chars_to_delete is
adjusted to be the number of ATTENTION_MARKERs copied (directly or indirectly)
to the destination buffer, effectively compacting this source line
modification's deleted text to the minimum necessary to traverse nested
source line modifications (e.g., to detect inert macros).  The return value
is the address of the character in the destination buffer immediately
following the last copied ATTENTION_MARKER.
*/
{
  a_const_char *src = slmp->line_loc + 1;
  char         *this_target = next_avail - 1;

  while (src < slmp->line_loc + slmp->num_chars_to_delete) {
    if (*src == ATTENTION_MARKER) {
      a_source_line_modif_ptr slmp2 = nested_source_line_modif(src);
      src += slmp2->num_chars_to_delete;
      *next_avail++ = ATTENTION_MARKER;
      next_avail = copy_attention_markers(slmp2, next_avail);
    } else {
      ++src;
    }  /* if */
  }  /* while */
  rem_source_line_modif_from_hash_table(slmp);
  slmp->line_loc = this_target;
  add_source_line_modif_to_hash_table(slmp);
  slmp->num_chars_to_delete = next_avail - this_target;
  return next_avail;
}  /* copy_attention_markers */


static void expand_macro_buffer(sizeof_t needed)
/*
Expand the macro_buffer by reallocating it.  As a side effect, to reduce
memory overhead, remove all deleted text (except ATTENTION_MARKERs) while
copying from the old buffer to the new one.  Called by
ensure_macro_buffer_space.
*/
{
  sizeof_t                total_needed, old_size, old_len, new_size, increment;
  char                    *new_macro_buffer;
  register a_const_char   *src;
  register char           *dst;
  register char           ch;
  a_const_char            *old_start_for_remapping;
  char                    *new_start_for_remapping;
  a_source_line_modif_ptr slmp;
  a_source_line_modif_ptr nested_slmp;
  a_source_line_modif_ptr next_slmp;
  char                    *old_start_of_uncompacted;
  sizeof_t                num_chars_to_copy;

  db_enter(4, "expand_macro_buffer");
  old_size = after_end_of_macro_buffer - macro_buffer;
  old_len = next_avail_in_macro_buffer - macro_buffer;
  if (needed >= ((sizeof_t)-1) - old_len) {
    /* The following calculation of total_needed would overflow, which
       could cause use of a too-short buffer and result in overwriting
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
  total_needed = old_len + needed;
  /* Make sure we ask for enough to satisfy the current request and a
     little bit more. */
  increment = needed + needed/10 -
              (after_end_of_macro_buffer - next_avail_in_macro_buffer);
  if (num_chars_deleted_in_macro_buffer > increment &&
      num_chars_deleted_in_macro_buffer > old_size/4) {
    /* Avoid memory growth by using same size buffer -- there's plenty of
       room after compaction. */
    increment = 0;
  } else if (increment < old_size) {
    /* At least double the current allocation. */
    increment = old_size;
  }  /* if */
  new_size = old_size + increment;
  if (new_size+1 < total_needed && increment != 0) {
    /* There is an overflow somewhere in the calculation of the new request
       size.  The resulting buffer would be too short and cause overwritten
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_macro_buffer = alloc_general((sizeof_t)(new_size+1));
  if (num_compacted_macro_buffer_chars > 0) {
    /* Text that has already been compacted in a previous reallocation can
       simply be memcpy'ed and pointers into it adjusted directly, rather
       than going through the compaction process again.  This can leave some
       uncompacted deletions (when a source line modification is added inside
       the already-compacted space as a result of rescanning), but the time
       saved makes this a good tradeoff. */
    (void)memcpy(new_macro_buffer, macro_buffer,
                 size_t_arg(num_compacted_macro_buffer_chars));
    adjust_curr_source_line_structure_after_realloc(macro_buffer,
                           macro_buffer + num_compacted_macro_buffer_chars - 1,
                           new_macro_buffer,
                           /*adjust_source_line_modifs=*/FALSE);
  }  /* if */
  old_start_of_uncompacted = macro_buffer + num_compacted_macro_buffer_chars;
  dst = new_macro_buffer + num_compacted_macro_buffer_chars;
  /* Scan through all source line modifications.  For each modification
     whose line_loc is in the already-compacted (and already-copied) part of
     the buffer, relocate it appropriately; the same for modifications whose
     inserted_text is in that region.  Modifications whose inserted text is
     in the new (uncompacted) region are copied to the new buffer, scanning
     for and compacting deleted text.  The compaction process relocates the
     line_loc for the associated source line modification of each deletion,
     and adjust_curr_source_line_structure_after_realloc is called for each
     block of text copied into the new buffer.  Note also that text in the
     macro_buffer whose associated source line modification has been removed
     will not be copied, further reducing memory usage. */
  for (slmp = source_line_modif_list; slmp != NULL; slmp = slmp->next) {
    a_concatenation_record_ptr crp;
    if (slmp->line_loc != NULL &&
        ptr_in_range(slmp->line_loc, macro_buffer, old_start_of_uncompacted)) {
      /* slmp->line_loc has already been copied in the compacted portion of
         the buffer; just relocate the pointer. */
      rem_source_line_modif_from_hash_table(slmp);
      slmp->line_loc = slmp->line_loc - macro_buffer + new_macro_buffer;
      add_source_line_modif_to_hash_table(slmp);
    }  /* if */
    if (ptr_in_range(slmp->inserted_text, macro_buffer,
                     old_start_of_uncompacted)) {
      /* The inserted text has already been copied in the compacted portion
         of the buffer; just relocate the start and end pointers and all
         concatenation records. */
      slmp->inserted_text =
                         slmp->inserted_text - macro_buffer + new_macro_buffer;
      slmp->end_inserted_text =
                     slmp->end_inserted_text - macro_buffer + new_macro_buffer;
      for (crp = slmp->concatenations; crp != NULL; crp = crp->next) {
        crp->line_loc = crp->line_loc - macro_buffer + new_macro_buffer;
      }  /* for */
    } else if (ptr_in_range(slmp->inserted_text, old_start_of_uncompacted,
                            next_avail_in_macro_buffer)) {
      /* The inserted text is in the portion of the buffer to be compacted.
         Scan through it, copying undeleted text and only the
         ATTENTION_MARKERs of deleted parts. */
      src = slmp->inserted_text;
      slmp->inserted_text = dst;
      crp = slmp->concatenations;
      for (;;) {
        a_boolean is_lexical_escape;
        old_start_for_remapping = src;
        new_start_for_remapping = dst;
        while ((ch = (*dst++ = *src++)) != ATTENTION_MARKER &&
               ch != LE_END_OF_INSERTION) {}
        /* Having copied some number of characters from the old buffer to the
           new one, update any pointers into that region to reflect the
           movement. */
        is_lexical_escape = (src > old_start_for_remapping + 1 &&
                             src[-LE_ESCAPE_LEN] == LE_ESCAPE);
        adjust_curr_source_line_structure_after_realloc(
                                          old_start_for_remapping, src,
                                          new_start_for_remapping,
                                          /*adjust_source_line_modifs=*/FALSE);
        for (; crp != NULL && crp->line_loc < src; crp = crp->next) {
          crp->line_loc =
             crp->line_loc - old_start_for_remapping + new_start_for_remapping;
        }  /* for */
        if (ch == ATTENTION_MARKER && !is_lexical_escape) {
          /* This is the location of a macro replacement or deleted text.
             If it is a macro replacement, copy only the ATTENTION_MARKERs
             to the new buffer and adjust the source pointer appropriately.
             Deleted text (which occurs when the closing parenthesis of a
             macro invocation is not in the source line modification
             containing the macro name, a relatively rare situation) can
             include character positions that must be relocated, so it must
             be copied in full. */
          sizeof_t orig_deletion_len;
          nested_slmp = nested_source_line_modif(src - 1);
          if (nested_slmp->inserted_text == nested_slmp->inserted_chars) {
            /* This is a deletion source line modification.  Just update
               the location of the deletion. */
            rem_source_line_modif_from_hash_table(nested_slmp);
            nested_slmp->line_loc = dst - 1;
            add_source_line_modif_to_hash_table(nested_slmp);
          } else {
            /* This deletion is for a macro replacement: skip over the
               replaced characters, copying only any attention markers
               contained therein. */
            orig_deletion_len = nested_slmp->num_chars_to_delete;
            src += orig_deletion_len - 1;
            dst = copy_attention_markers(nested_slmp, dst);
#if FULLY_RESOLVED_MACRO_POSITIONS
            adjust_macro_text_map_after_compaction(
                            slmp, orig_deletion_len,
                            nested_slmp->num_chars_to_delete,
                            (sizeof_t)(dst - nested_slmp->num_chars_to_delete -
                                       slmp->inserted_text));
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
          }  /* if */
        } else if (ch == LE_END_OF_INSERTION && is_lexical_escape) {
          /* This is an end-of-insertion marker (and not just a stray
             LE_END_OF_INSERTION character, hence the retroactive check for a
             preceding LE_ESCAPE character -- there will typically be lots of
             LE_ESCAPEs that are not end-of-insertions (because of token-ends),
             so it's much faster to scan for LE_END_OF_INSERTION in the
             character-copying loop and then check for the preceding
             character). */
          check_assertion(slmp->end_inserted_text == src - LE_ESCAPE_LEN);
          slmp->end_inserted_text = dst - LE_ESCAPE_LEN;
          check_assertion(crp == NULL);
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* for */
  num_compacted_macro_buffer_chars = dst - new_macro_buffer;
  if (macro_buffer_region_in_progress != NULL) {
    /* The end of the old buffer contains data that is not referred to by a
       source line modification and thus must be copied specially into the
       new buffer. */
    check_assertion(ptr_in_range(macro_buffer_region_in_progress, macro_buffer,
                                 next_avail_in_macro_buffer + 1));
    num_chars_to_copy =
                  next_avail_in_macro_buffer - macro_buffer_region_in_progress;
    (void)memcpy(dst, macro_buffer_region_in_progress,
                 size_t_arg(num_chars_to_copy));
    adjust_curr_source_line_structure_after_realloc(
                                          macro_buffer_region_in_progress,
                                          next_avail_in_macro_buffer, dst,
                                          /*adjust_source_line_modifs=*/FALSE);
    macro_buffer_region_in_progress = dst;
    dst += num_chars_to_copy;
  }  /* if */
  /* Check to see if any of the source line modifications are replacements
     for text in the old buffer that was not copied into the new buffer; if
     so, break that association or remove the modification, as appropriate.
     One way this occurs is when read_logical_source_line is called during
     a macro invocation.  In that case, all source line modifications
     except those holding saved text for scanned macro arguments are
     removed, and the line_loc for those modifications can refer to text
     associated with modifications that were removed.  Another case is when
     expand_top_level_pcc_macro is in the process of replacing the
     top-level expansion and the text being replaced has leftover deletions
     from LE_RAW_OR_EXPANDED_ARGUMENT sequences. */
  for (slmp = source_line_modif_list; slmp != NULL; slmp = next_slmp) {
    next_slmp = slmp->next;
    if (ptr_in_range(slmp->line_loc, macro_buffer,
                     next_avail_in_macro_buffer)) {
      check_assertion(slmp->contains_saved_macro_argument_text ||
                      ptr_in_range(slmp->line_loc,
                                   discarded_pcc_top_level_begin,
                                   discarded_pcc_top_level_end));
      if (slmp->contains_saved_macro_argument_text) {
        /* Break the modification's association with the buffer text. */
        rem_source_line_modif_from_hash_table(slmp);
        slmp->line_loc = NULL;
        slmp->num_chars_to_delete = 0;
      } else {
        /* Remove the leftover raw/expanded text deletion. */
        a_source_line_modif_ptr slmp2 = slmp;
        rem_source_line_modif(slmp2);
        free_source_line_modif(&slmp2);
      }  /* if */
    }  /* if */
  }  /* for */
  free_general((a_void_ptr)macro_buffer, (sizeof_t)(old_size+1));
  macro_buffer = new_macro_buffer;
  after_end_of_macro_buffer = macro_buffer + new_size;
  next_avail_in_macro_buffer = dst;
  num_chars_deleted_in_macro_buffer = 0;
  check_assertion((sizeof_t)(after_end_of_macro_buffer -
                             next_avail_in_macro_buffer) > needed);
  db_exit();
}  /* expand_macro_buffer */


/*
Ensure that at least "needed" bytes of space remain in macro_buffer.
If not, expand macro_buffer by reallocating it.
*/
#define ensure_macro_buffer_space(needed)                             \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (sizeof_t)(after_end_of_macro_buffer -            \
                               next_avail_in_macro_buffer)) {         \
    expand_macro_buffer(temp_needed);                                 \
  }  /* if */                                                         \
}  /* ensure_macro_buffer_space */


static char* begin_macro_buffer_region(void)
/*
Mark the existence and beginning location of a region at the tail of the
macro_buffer that must be copied specially upon macro_buffer reallocation.
This routine must be called whenever a data structure will be built in the
macro_buffer incrementally, i.e., with the possibility of a reallocation
occurring before the structure is complete.  For convenience, it returns
next_avail_in_macro_buffer, where the structure being built will begin.
*/
{
  check_assertion(macro_buffer_region_in_progress == NULL);
  macro_buffer_region_in_progress = next_avail_in_macro_buffer;
  return next_avail_in_macro_buffer;
}  /* begin_macro_buffer_region */


static void release_macro_buffer_region(void)
/*
Unmark the region at the end of the macro_buffer for special handling upon
buffer reallocation.  This routine must be called once the data structure in
the space marked by begin_macro_buffer_region() is complete and either copied
elsewhere or registered as the inserted_text of a source line modification.
*/
{
  macro_buffer_region_in_progress = NULL;
}  /* release_macro_buffer_region */


static void expand_aux_buffer_for_pcc_macros(sizeof_t needed,
                                             char     *pos_in_aux_buffer)
/*
Expand aux_buffer_for_pcc_macros by reallocating it.  Called by
ensure_aux_buffer_for_pcc_macros_space.  pos_in_aux_buffer points to
the pointer to the next available position in that buffer.
*/
{
  sizeof_t total_needed, old_size, old_len, new_size, increment;
  char     *new_aux_buffer_for_pcc_macros;

  db_enter(4, "expand_aux_buffer_for_pcc_macros");
  old_size = after_end_of_aux_buffer_for_pcc_macros -
             aux_buffer_for_pcc_macros;
  old_len = pos_in_aux_buffer - aux_buffer_for_pcc_macros;
  if (needed >= ((sizeof_t)-1) - old_len) {
    /* The following calculation of total_needed would overflow, which
       could cause use of a too-short buffer and result in overwriting
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
  total_needed = old_len + needed;
  /* Not enough space; need to expand.  Make sure we ask for enough
     to satisfy the current request and a little bit more. */
  increment = needed + needed/10 -
              (after_end_of_aux_buffer_for_pcc_macros - pos_in_aux_buffer);
  if (increment < old_size) {
    /* At least double the current allocation. */
    increment = old_size;
  }  /* if */
  new_size = old_size + increment;
  if (new_size+1 < total_needed) {
    /* There is an overflow somewhere in the calculation of the new request
       size.  The resulting buffer would be too short and cause overwritten
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_aux_buffer_for_pcc_macros = realloc_buffer(aux_buffer_for_pcc_macros,
                                                  (sizeof_t)(old_size+1),
                                                  (sizeof_t)(new_size+1));
  /* Update any pointers to the old aux_buffer_for_pcc_macros in the
     curr_source_line data structure.  This is only needed for any registered
     local pointers that might point into the aux. buffer. */
  adjust_curr_source_line_structure_after_realloc(aux_buffer_for_pcc_macros,
                                        after_end_of_aux_buffer_for_pcc_macros,
                                        new_aux_buffer_for_pcc_macros,
                                        /*adjust_source_line_modifs=*/TRUE);
  /* Note that pos_in_aux_buffer is now unusable, since it has not been
     updated here.  However, the caller has the variable it passed registered
     as a local pointer, and that will have been updated. */
  aux_buffer_for_pcc_macros = new_aux_buffer_for_pcc_macros;
  after_end_of_aux_buffer_for_pcc_macros = aux_buffer_for_pcc_macros +
                                           new_size;
  db_exit();
}  /* expand_aux_buffer_for_pcc_macros */


/*
Ensure that at least "needed" bytes of space remain following
pos_in_aux_buffer in aux_buffer_for_pcc_macros.  If not, expand
aux_buffer_for_pcc_macros by reallocating it.
*/
#define ensure_aux_buffer_for_pcc_macros_space(needed, pos_in_aux_buffer) \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (sizeof_t)(after_end_of_aux_buffer_for_pcc_macros -    \
                               pos_in_aux_buffer)) {                       \
    expand_aux_buffer_for_pcc_macros(temp_needed, pos_in_aux_buffer); \
  }  /* if */                                                         \
}  /* ensure_aux_buffer_for_pcc_macros_space */


static void expand_arg_raw_text(sizeof_t        needed,
                                a_macro_arg_ptr map)
/*
Expand the raw_text of a macro arg by reallocating it.  Called by
ensure_arg_raw_text_space.
*/
{
  a_macro_arg_ptr avail_map;
  sizeof_t        total_needed, old_size, new_size, increment;
  char            *new_raw_text;

  db_enter(4, "expand_arg_raw_text");
  old_size = map->raw_alloc_len;
  if (needed >= ((sizeof_t)-1) - map->raw_len) {
    /* The following calculation of total_needed would overflow, which
       could cause use of a too-short buffer and result in overwriting
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
  total_needed = map->raw_len + needed;
  /* Take a look to see if a freed entry has a large enough raw_text area,
     in which case the two raw_text allocations can be swapped. */
  for (avail_map = avail_macro_args;
       avail_map != NULL;
       avail_map = avail_map->next) {
    if (avail_map->raw_alloc_len >= total_needed) {
      /* This raw_text entry is big enough.  Note this is "first fit", not
         "best fit".  It shouldn't matter.  Swap the raw_text areas, and
         copy the data.  Note that the entries themselves are not swapped,
         and the expanded_text areas are left alone. */
      new_raw_text = avail_map->raw_text;
      new_size = avail_map->raw_alloc_len;
      avail_map->raw_text = map->raw_text;
      avail_map->raw_alloc_len = map->raw_alloc_len;
      (void)memcpy(new_raw_text, map->raw_text, size_t_arg(map->raw_len));
      goto have_space;
    }  /* if */
  }  /* for */
  /* Need to expand.  Make sure we ask for enough to satisfy the current
     request and a little bit more. */
  increment = needed + needed/10 - (old_size - map->raw_len);
  if (increment < old_size) {
    /* At least double the current allocation. */
    increment = old_size;
  }  /* if */
  new_size = old_size + increment;
  if (new_size+1 < total_needed) {
    /* There is an overflow somewhere in the calculation of the new request
       size.  The resulting buffer would be too short and cause overwritten
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
#if DEBUG
  macro_arg_text_space += (unsigned long)increment;
#endif /* DEBUG */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_raw_text = realloc_buffer(map->raw_text, (sizeof_t)(old_size+1),
                                                (sizeof_t)(new_size+1));
have_space:
  /* Update any pointers to the old raw_text in the curr_source_line
     data structure. */
  adjust_curr_source_line_structure_after_realloc(map->raw_text,
                                           map->raw_text+old_size,
                                           new_raw_text,
                                           /*adjust_source_line_modifs=*/TRUE);
  map->raw_text = new_raw_text;
  map->raw_alloc_len = new_size;
  db_exit();
}  /* expand_arg_raw_text */


/*
Ensure that at least "needed" bytes of space remain in the raw_text of
the given macro argument entry.  If not, expand raw_text by reallocating it.
*/
#define ensure_arg_raw_text_space(needed, map)                        \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (map->raw_alloc_len - map->raw_len)) {            \
    expand_arg_raw_text(temp_needed, map);                            \
  }  /* if */                                                         \
}  /* ensure_arg_raw_text_space */


static void expand_arg_expanded_text(sizeof_t        needed,
                                     a_macro_arg_ptr map)
/*
Expand the expanded_text of a macro arg by reallocating it.  Called by
ensure_arg_expanded_text_space.
*/
{
  a_macro_arg_ptr avail_map;
  sizeof_t        total_needed, old_size, new_size, increment;
  char            *new_expanded_text;

  db_enter(4, "expand_arg_expanded_text");
  old_size = map->expanded_alloc_len;
  if (needed >= ((sizeof_t)-1) - map->expanded_len) {
    /* The following calculation of total_needed would overflow, which
       could cause use of a too-short buffer and result in overwriting
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
  total_needed = map->expanded_len + needed;
  /* Take a look to see if a freed entry has a large enough expanded_text area,
     in which case the two expanded_text allocations can be swapped. */
  for (avail_map = avail_macro_args;
       avail_map != NULL;
       avail_map = avail_map->next) {
    if (avail_map->expanded_alloc_len >= total_needed) {
      /* This expanded_text entry is big enough.  Note this is "first fit",
         not "best fit".  It shouldn't matter.  Swap the expanded_text areas,
         and copy the data.  Note that the entries themselves are not swapped,
         and the raw_text areas are left alone. */
      new_expanded_text = avail_map->expanded_text;
      new_size = avail_map->expanded_alloc_len;
      avail_map->expanded_text = map->expanded_text;
      avail_map->expanded_alloc_len = map->expanded_alloc_len;
      (void)memcpy(new_expanded_text, map->expanded_text,
                   size_t_arg(map->expanded_len));
      goto have_space;
    }  /* if */
  }  /* for */
  /* Need to expand.  Make sure we ask for enough to satisfy the current
     request and a little bit more. */
  increment = needed + needed/10 - (old_size - map->expanded_len);
  if (increment < old_size) {
    /* At least double the current allocation. */
    increment = old_size;
  }  /* if */
  new_size = old_size + increment;
  if (new_size+1 < total_needed) {
    /* There is an overflow somewhere in the calculation of the new request
       size.  The resulting buffer would be too short and cause overwritten
       memory. */
    catastrophe(ec_requested_size_too_large);
  }  /* if */
#if DEBUG
  macro_arg_text_space += (unsigned long)increment;
#endif /* DEBUG */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_expanded_text = realloc_buffer(map->expanded_text,
                                      (sizeof_t)(old_size+1),
                                      (sizeof_t)(new_size+1));
have_space:
  /* Update any pointers to the old expanded_text in the curr_source_line
     data structure. */
  adjust_curr_source_line_structure_after_realloc(map->expanded_text,
                                           map->expanded_text+old_size,
                                           new_expanded_text,
                                           /*adjust_source_line_modifs=*/TRUE);
  map->expanded_text = new_expanded_text;
  map->expanded_alloc_len = new_size;
  db_exit();
}  /* expand_arg_expanded_text */


/*
Ensure that at least "needed" bytes of space remain in the expanded_text
of the given macro argument entry.  If not, expand expanded_text by
reallocating it.
*/
#define ensure_arg_expanded_text_space(needed, map)                   \
{ sizeof_t temp_needed = (needed);                                    \
  if (temp_needed > (map->expanded_alloc_len - map->expanded_len)) {  \
    expand_arg_expanded_text(temp_needed, map);                       \
  }  /* if */                                                         \
}  /* ensure_arg_expanded_text_space */


static a_macro_param_ptr alloc_macro_param(void)
/*
Allocate a macro parameter entry (used for the formal parameters of
preprocessor macros), clear it to default values, and return a pointer
to it.
*/
{
  a_macro_param_ptr mpp;

  mpp = (a_macro_param_ptr)alloc_fe(sizeof(a_macro_param));
#if DEBUG
  num_macro_params_allocated++;
#endif /* DEBUG */
  mpp->name = NULL;
  mpp->next = NULL;
  mpp->need_expanded_form = FALSE;
  mpp->is_operand_of_paste = FALSE;
  return (mpp);
}  /* alloc_macro_param */


void clear_macro_def(a_macro_def_ptr mdp)
/*
Clear a macro definition entry to default values.
*/
{
  mdp->object_like                         = TRUE;
  mdp->cannot_be_redefined                 = FALSE;
  mdp->ref_suppresses_pch_file             = FALSE;
  mdp->variadic                            = FALSE;
  mdp->param_list                          = NULL;
  mdp->repl_text                           = NULL;
  mdp->is_predefined                       = FALSE;
#if RECORD_MACROS_IN_IL
  mdp->macro                               = NULL;
#endif /* RECORD_MACROS_IN_IL */
#if FULLY_RESOLVED_MACRO_POSITIONS
  init_macro_text_map(/*num_entries=*/0, &mdp->text_map, /*resizable=*/FALSE);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
}  /* clear_macro_def */


static a_macro_def_ptr alloc_macro_def(void)
/*
Allocate a macro definition entry (used for preprocessor macros), clear it
to default values, and return a pointer to it.
*/
{
  a_macro_def_ptr mdp;

  mdp = (a_macro_def_ptr)alloc_fe(sizeof(a_macro_def));
#if DEBUG
  num_macro_defs_allocated++;
#endif /* DEBUG */
  clear_macro_def(mdp);
  return (mdp);
}  /* alloc_macro_def */


static a_macro_arg_ptr alloc_macro_arg(void)
/*
Allocate a macro argument description, set its fields to default values,
and return a pointer to it.
*/
{
  a_macro_arg_ptr map;

  db_enter(5, "alloc_macro_arg");
  if (avail_macro_args != NULL) {
    /* Reuse a freed entry. */
    map = avail_macro_args;
    avail_macro_args = avail_macro_args->next;
#if FULLY_RESOLVED_MACRO_POSITIONS
    map->raw_text_map.num_entries = 0;
    map->exp_text_map.num_entries = 0;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  } else {
    /* Allocate a new entry.  Note the use of alloc_general rather than
       alloc_fe, so that the raw_text storage can be kept around. */
    map = (a_macro_arg_ptr)alloc_general(sizeof(a_macro_arg));
#if DEBUG
    num_macro_args_allocated++;
#endif /* DEBUG */
    /* Allocate an initial raw text array.  This can be expanded later
       if necessary, but the size here should be able to cover most
       needs.  Note the use of alloc_resizable_buffer here, since only space
       allocated via that routine can be realloced. */
    map->raw_alloc_len = ARG_RAW_TEXT_INITIAL_ALLOCATION;
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    map->raw_text = alloc_resizable_buffer((sizeof_t)(map->raw_alloc_len+1));
#if DEBUG
    macro_arg_text_space += (unsigned long)(map->raw_alloc_len);
#endif /* DEBUG */
    /* Likewise for the macro-expanded text array. */
    map->expanded_alloc_len = ARG_EXPANDED_TEXT_INITIAL_ALLOCATION;
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    map->expanded_text = alloc_resizable_buffer(
                                        (sizeof_t)(map->expanded_alloc_len+1));
#if DEBUG
    macro_arg_text_space += (unsigned long)(map->expanded_alloc_len);
#endif /* DEBUG */
#if FULLY_RESOLVED_MACRO_POSITIONS
    init_macro_text_map(MACRO_ARGUMENT_TEXT_MAP_INITIAL_COUNT,
                        &map->raw_text_map, /*resizable=*/TRUE);
    init_macro_text_map(MACRO_ARGUMENT_TEXT_MAP_INITIAL_COUNT,
                        &map->exp_text_map, /*resizable=*/TRUE);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  } /* if */
  map->next         = NULL;
  map->raw_len      = 0;
  map->initial_raw_text_not_in_primary_source_line = NULL;
  map->final_modif_for_initial_text = NULL;
  map->offset_in_raw_text_of_primary_source_line_text = 0;
  map->expanded_len = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
  map->comma_pos = null_source_position;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  map->is_empty_arg = FALSE;
  db_exit();
  return map;
}  /* alloc_macro_arg */


static void free_macro_arg(a_macro_arg_ptr *map)
/*
Free the macro argument description pointed to by *map, and set *map to NULL.
*/
{
  db_enter(5, "free_macro_arg");
  /* Put the macro buffer on a list of macro buffers freed and available
     to be reused. */
  (*map)->next = avail_macro_args;
  avail_macro_args = *map;
  *map = NULL;
  db_exit();
}  /* free_macro_arg */


#if DEBUG
static void print_markered_text(a_const_char *str,
                                sizeof_t     len,
                                a_boolean    go_to_end_of_line)
/*
Print the indicated string, interpreting any marker characters therein
(attention characters and lexical escapes).  Printing stops after
"len" characters or when an end-of-line or end-of-insertion of the same level
as the start is reached (if go_to_end_of_line is TRUE, only stop for
the end of line or the end of a macro argument).  len < 0 can be used
to disable the character-counting feature.  This routine is used to
print the replacement text and expansions of macros.
*/
{
  a_const_char            *p;
  sizeof_t                n_printed;
  char                    ch;
  a_source_line_modif_ptr slmp;
  int                     level = 0;

  /*lint --e{850} n_printed modified in loop */
  for (p = str, n_printed = 0;
                n_printed != len;
                n_printed++) {
    ch = *p;
    if (ch == LE_ESCAPE) {
      /* Lexical escape. */
      ch = p[1];
      if (ch == LE_END_OF_LINE) {
        /* End of entire source line.  Stop. */
        break;
      } else if (ch == LE_NEWLINE) {
        /* Newline.  Print newline character, stop. */
        fputc('\n', f_debug);
        break;
      } else if (ch == LE_END_OF_INSERTION) {
        /* End of macro modification. */
        if (!go_to_end_of_line && level == 0) {
          /* End of insertion at same level as start of text.  Stop. */
          break;
        } else {
          /* Find character location after modification. */
          ch = '$';
          n_printed++;
          slmp = assoc_source_line_modif(p);
          /* If this is the end of a macro argument, stop. */
          if (slmp->is_isolated_text) break;
          level--;
          leave_insertion(slmp, p);
        }  /* if */
      } else if (ch == LE_END_OF_TOKEN) {
        /* End of token marker. */
        ch = '`';
        n_printed++;
        p += LE_ESCAPE_LEN;
      } else if (ch == LE_INERT_MACRO || ch == LE_TEMPORARILY_INERT_MACRO) {
        /* Marker to suppress expansion of following macro name. */
        ch = '#';
        n_printed++;
        p += LE_ESCAPE_LEN;
      } else if (ch == LE_NULL) {
        /* Marker indicating a null (zero) character. */
        ch = '0';
        n_printed++;
        p += LE_ESCAPE_LEN;
      } else if (ch == LE_COMMA_FROM_ARGUMENT) {
        /* Marker indicating a comma from a macro argument (that will not
           act as a macro argument delimiter when rescanned). */
        ch = '\\';
        p += LE_ESCAPE_LEN;
#if !FULLY_RESOLVED_MACRO_POSITIONS
      } else if (ch == LE_END_OF_TOP_LEVEL_EXPANSION) {
        /* Marker indicating the end of the expansion of a macro invoked
           directly within a source line. */
        ch = '!';
        p += LE_ESCAPE_LEN;
#endif /* !FULLY_RESOLVED_MACRO_POSITIONS */
      } else if (ch == LE_RAW_OR_EXPANDED_ARGUMENT) {
        /* Marker introducing both the raw and expanded versions of a
           macro argument. */
        ch = '~';
        p += LE_ESCAPE_LEN;
      } else if (ch == LE_MICROSOFT_MAGIC_COMMA) {
        /* Marks the following character (a comma) as potentially
           suppressed because of preceding an empty __VA_ARGS__
           expansion. */
        ch = '?';
        p += LE_ESCAPE_LEN;
      } else {
        (void)fprintf(f_debug, "**BAD LEXICAL ESCAPE**");
        break;
      }  /* if */
    } else if (ch == ATTENTION_MARKER) {
      /* Modification begins here.  Go into it.  Print a deletion as "%"
         instead. */
      go_into_insertion(slmp, p);
      if (slmp->inserted_text == slmp->end_inserted_text) {
        /* Deletion, no inserted text. */
        ch = '%';
      } else {
        /* Insertion. */
        level++;
        ch = '@';
      }  /* if */
    } else {
      /* Normal character. */
      if (!isprint((unsigned char)ch)) ch = '?';
      p++;
    }  /* if */
    fputc(ch, f_debug);
  }  /* for */

}  /* print_markered_text */
#endif /* DEBUG */
  

a_symbol_ptr find_defined_macro(a_symbol_header_ptr sym_hdr)
/*
See if there is a macro on the list of symbols pointed to by sym_hdr.
If so, return a pointer to it.  If not, return NULL.  This routine exists
so that "defined" will not be found as a defined macro.
*/
{
  a_symbol_ptr	assoc_symbol;

  assoc_symbol = find_macro_symbol(sym_hdr);
  /* If the macro found is the pseudo-macro "defined" (which is used as
     an operator in #if statements), or "_Pragma" (which is used for the
     C99-style _Pragma operator), or "__pragma" (which is used for the
     Microsoft __pragma operator) pretend it was not found. */
  if (assoc_symbol == defined_macro_symbol ||
      assoc_symbol == Pragma_macro_symbol ||
      assoc_symbol == microsoft_pragma_macro_symbol) {
    assoc_symbol = NULL;
  }  /* if */
  return (assoc_symbol);
}  /* find_defined_macro */


a_token_kind make_pp_int_constant(long value)
/*
Make a constant entry with the given integer value in const_for_curr_token.
This is being created as the value for some preprocessor operation.
The type will be long int (intmax_t in C99), since that is what the
preprocessor uses.  Return tok_int_constant.
*/
{
  set_integer_constant(&const_for_curr_token, (a_host_large_integer)value,
                       (an_integer_kind)(c99_mode ? targ_intmax_kind :
                                                    (an_integer_kind)ik_long));
  return tok_int_constant;
}  /* make_pp_int_constant */


static void check_for_following_parenthesis(a_boolean    *paren_found,
                                            a_boolean    allow_id)
/*
The current token is an identifier, probably the macro name at the
beginning of a macro invocation.  Skip white space and look to see if the
next token is a "(", and return *paren_found == TRUE if it is.  If allow_id
is TRUE, also return *paren_found == TRUE if the next token is an identifier.
If a "(" (or identifier) is not found, return *paren_found == FALSE and
re-insert the identifier if necessary (delete_source_from_loc is non-NULL,
so a hanging delete is in effect).
*/
{
  a_seq_number  old_seq_number;
  a_const_char  *orig_loc;
  char          *ins_loc;
  a_source_line_modif_ptr
                slmp,
                slmp2;
  unsigned long sequence_id;
  a_boolean     saved_do_not_advance_past_end_of_file = FALSE;

  old_seq_number = curr_seq_number;
  orig_loc = start_of_curr_token;
  /* Don't go into another file looking for a "(". */
  if (curr_ise != NULL) {
    saved_do_not_advance_past_end_of_file =
                                     curr_ise->do_not_advance_past_end_of_file;
    curr_ise->do_not_advance_past_end_of_file = TRUE;
  }  /* if */
  skip_white_space();
  if (curr_ise != NULL) {
    curr_ise->do_not_advance_past_end_of_file =
                                         saved_do_not_advance_past_end_of_file;
  }  /* if */
  if (*curr_char_loc == '(') {
    /* Left parenthesis found. */
    *paren_found = TRUE;
  } else if (allow_id &&
             is_identifier_char(curr_char_loc, (int *)NULL,
                                /*is_identifier_start=*/TRUE) &&
             /* Watch out for wide character constants and string literals. */
             (*curr_char_loc != 'L' || (*(curr_char_loc+1) != '"' &&
                                        *(curr_char_loc+1) != '\''))) {
    /* Identifier found when allowed. */
    *paren_found = TRUE;
  } else {
    /* Not a macro because not followed by "(", so return as an
       identifier.  If we have not gone onto another line with
       the skip_white_space call, we can undo the logical deletion.
       If we have left the original line, we have to re-insert the
       identifier at the beginning of the current line, followed by
       a newline (because there was white space skipped).  We insert
       a newline instead of a blank because the next line might be
       a #pragma being passed through and it must remain in column
       one.  Note one strange case: we may have reached end of file,
       and the re-insertion must therefore be done in the empty
       end-of-file line. */
    *paren_found = FALSE;
    delete_source_from_loc = NULL;
    len_of_curr_token = locator_for_curr_id.symbol_header->identifier_length;
    if (curr_seq_number == old_seq_number &&
        !at_end_of_source_file) {
      /* We are still on the same line, so re-insertion is not necessary.
         However, if in getting from the end of the identifier to the
         current position we entered or left a source modification, some
         deletions have already been put out.  We must find them and 
         remove them.  This is a rare case. */
      if (*orig_loc == ATTENTION_MARKER) {
        /* Find the entry that deletes the identifier, and remove it. */
        slmp = nested_source_line_modif(orig_loc);
        sequence_id = slmp->sequence_id;
        rem_source_line_modif(slmp);
        free_source_line_modif(&slmp);
        /* There might be some deletions following that one, to
           delete white space.  We can identify those because they have
           a sequence_id larger than the entry that deleted the
           identifier.  Remove them.  If any of them are deletions of
           comments, the comments will be re-examined and deleted again
           later. */
        if (sequence_id != sequence_id_for_source_line_modifs) {
          for (slmp = source_line_modif_list; slmp != NULL;) {
            slmp2 = slmp;
            slmp = slmp->next;
            if (slmp2->sequence_id > sequence_id) {
              rem_source_line_modif(slmp2);
              free_source_line_modif(&slmp2);
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      start_of_curr_token = orig_loc;
    } else {
      /* We are on a new line, so re-insertion is necessary. */
      /* Make enough room for the insertion text.  "+2*LE_ESCAPE_LEN"
         covers the LE_NEWLINE and LE_END_OF_INSERTION lexical escapes. */
      ensure_macro_buffer_space(len_of_curr_token+2*LE_ESCAPE_LEN);
      /* Insert the identifier name. */
      ins_loc = next_avail_in_macro_buffer;
      (void)memcpy(ins_loc,
                   locator_for_curr_id.symbol_header->identifier,
                   size_t_arg(len_of_curr_token));
      next_avail_in_macro_buffer += len_of_curr_token;
      *next_avail_in_macro_buffer++ = LE_ESCAPE;
      *next_avail_in_macro_buffer++ = LE_NEWLINE;
      *next_avail_in_macro_buffer++ = LE_ESCAPE;
      *next_avail_in_macro_buffer++ = LE_END_OF_INSERTION;
      /* Add a source line modification entry to do the insert.  This is
         a strange kind of entry: line_loc == NULL indicates that
         the insertion is to be done preceding the first character
         of curr_source_line. */
      /* coverity[returned_pointer] -- slmp unused in some configurations. */
      slmp = add_source_line_modif((char *)NULL, 0, ins_loc,
                                   ins_loc+len_of_curr_token+LE_ESCAPE_LEN);
      start_of_curr_token = ins_loc;
#if FULLY_RESOLVED_MACRO_POSITIONS
      /* Add text map entries to describe the original position of the
         re-inserted token. */
      add_entry_to_macro_text_map(&macro_text_map, /*start_of_region=*/0,
                                  pos_curr_token.seq, pos_curr_token.column,
                                  NO_PARENT_MACRO_INVOCATION);
      add_entry_to_macro_text_map(&macro_text_map,
                                  len_of_curr_token+2*LE_ESCAPE_LEN,
                                  (a_seq_number)0, SP_COL_UNKNOWN,
                                  NO_PARENT_MACRO_INVOCATION);
      slmp->text_map.num_entries = 2;
      slmp->text_map.entries =
                       &macro_text_map.entries[macro_text_map.num_entries - 2];
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    }  /* if */
    end_of_curr_token = start_of_curr_token + len_of_curr_token - 1;
  }  /* if */
}  /* check_for_following_parenthesis */


static a_token_kind scan_defined_operator(void)
/*
Scan an instance of the "defined" operator in a preprocessor expression.
It has the form

  defined identifier

or

  defined ( identifier )

(See standard, 3.8.1).  If the "defined" identifier is not an operator
in this case, return tok_identifier.  Otherwise (including in error
cases), set const_for_curr_token to a value of 0L (not defined) or 1L
(defined), and return tok_int_constant.  On return, the first token
beyond the operator has not yet been fetched.
*/
{
  a_symbol_ptr  assoc_symbol = NULL;
  a_symbol_header_ptr
		sym_hdr = NULL;
  a_source_position
		start_position;
  a_token_kind	ctoken;
  a_boolean     save_expand_macros = expand_macros;
  a_boolean     paren_or_id_found;
  a_boolean     in_macro_expansion =
                                 !within_curr_source_line(start_of_curr_token);
  a_boolean     parenthesized_form;

  db_enter(4, "scan_defined_operator");
  copy_source_position(pos_curr_token, start_position);
  if (!in_pp_if_expression) {
    /* If not inside a #if expression, "defined" is just an identifier. */
    ctoken = tok_identifier;
  } else {
    /* Within an #if expression, defined is an operator with a value
       of 0L or 1L (undefined or defined).  Look for a left parenthesis
       or identifier following it. */
    check_for_following_parenthesis(&paren_or_id_found, /*allow_id=*/TRUE);
    if (!paren_or_id_found) {
      /* "defined" is not followed by an identifier or left parenthesis;
         therefore, it should be left as an identifier. */
      ctoken = tok_identifier;
    } else {
      /* "defined" is followed by a left parenthesis or identifier.
         Get it as a token. */
      a_boolean paren_followed_by_whitespace = TRUE;
      /* Turn off macro expansion for the get_token calls that follow. */
      expand_macros = FALSE;
      if (get_token() == tok_identifier) {
        /* First form -- "defined identifier". */
        parenthesized_form = FALSE;
        /* The identifier __VA_ARGS__ is not allowed if variadic macros are
           accepted. */
        check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
        sym_hdr = find_symbol_header(start_of_curr_token, len_of_curr_token,
                                     &locator_for_curr_id);
      } else {
        /* Second form -- "defined ( identifier )". */
#if CHECKING
        if (curr_token != tok_lparen) {
          internal_error("scan_defined_operator: next is not id or \"(\"");
        }  /* if */
#endif /* CHECKING */
        parenthesized_form = TRUE;
        /* Check to see if the left parenthesis is followed by white space,
           which is significant in emulating a Microsoft bug below. */
        skip_white_space();
        paren_followed_by_whitespace = (kind_of_white_space_skipped != 0);
        if (get_token() != tok_identifier) {
          /* Error -- Expected an identifier. */
          pos_error(ec_exp_identifier, &error_position);
          /* The token will be consumed by the expression routines in
             non-pp-token mode, so push it back in that mode as well. */
          if (curr_token == tok_newline) {
            newline_ungotten = TRUE;
          }  /* if */
          fetch_pp_tokens = FALSE;
          unget_token();
          fetch_pp_tokens = TRUE;
        } else {
          /* The identifier __VA_ARGS__ is not allowed if variadic macros are
             accepted. */
          check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
          sym_hdr = find_symbol_header(start_of_curr_token, len_of_curr_token,
                                       &locator_for_curr_id);
          if (get_token() != tok_rparen) {
            /* Error -- Expected a right parenthesis. */
            if (microsoft_mode && microsoft_version <= 1300 &&
                curr_token == tok_newline) {
              /* Versions of the Microsoft compiler before 7.1 give no
                 error on the missing right parenthesis at end of line, and
                 the corresponding Microsoft headers unfortunately use
                 this. */
              pos_remark(ec_exp_rparen, &error_position);
            } else {
              pos_error(ec_exp_rparen, &error_position);
            }  /* if */
            /* The token will be consumed by the expression routines in
               non-pp-token mode, so push it back in that mode as well. */
            if (curr_token == tok_newline) {
              newline_ungotten = TRUE;
            }  /* if */
            fetch_pp_tokens = FALSE;
            unget_token();
            fetch_pp_tokens = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Make a 0 or 1 constant depending or whether the symbol is undefined
         or defined.  Note that for error cases sym_hdr is NULL and
         that will produce a value of 0. */
      if (sym_hdr != NULL) {
        assoc_symbol = find_defined_macro(sym_hdr);
        if (assoc_symbol != NULL) {
          mark_referenced(assoc_symbol, &locator_for_curr_id.source_position);
        }  /* if */
        if (microsoft_bugs && in_macro_expansion && parenthesized_form &&
            !paren_followed_by_whitespace) {
          /* The Microsoft preprocessor has a bug that results in the
             parenthesized form of "defined" unconditionally having the
             value 0 if it appears in a macro expansion and the left
             parenthesis is not followed by white space; some system
             headers depend on this behavior.  (The unparenthesized form is
             processed correctly, as is the parenthesized form when the
             operand is separated from the left parenthesis by a space or
             comment.) */
          pos_warning(ec_defined_always_false, &start_position);
          assoc_symbol = NULL;
        }  /* if */
      }  /* if */
      ctoken = make_pp_int_constant((long)(assoc_symbol != NULL));
      /* Set the token position to the start of the keyword "defined". */
      copy_source_position(start_position, pos_curr_token);
    }  /* if */
  }  /* if */
  expand_macros = save_expand_macros;
  db_exit();
  return (ctoken);
}  /* scan_defined_operator */


static a_macro_arg_ptr copy_pragma_string(void)
/*
The current token is a tok_string_literal scanned in fetch-pp-tokens mode.
Make a copy of the string to a macro argument, replacing \" with " and \\
with \.  Return the macro argument created.
*/
{
  a_macro_arg_ptr	map;
  sizeof_t		length;
  a_const_char		*end_of_string;
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_source_position     curr_pos;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

  /* Compute the length of the string.  Note that this will include the
     quotes on either end.  The size may be slightly larger than what is
     needed if the string includes escapes that are removed.  Note that
     the space counted for the quotes will be used to hold the LE_ESCAPE
     that must be added at the end of the string. */
#if LE_ESCAPE_LEN != 2
 #error -- LE_ESCAPE_LEN expected to be 2
#endif /* LE_ESCAPE_LEN != 2 */
  length = curr_char_loc - start_of_curr_token;
  map = alloc_macro_arg();
  ensure_arg_raw_text_space(length, map);
  end_of_string = end_of_curr_token;
  /* Copy the characters to the macro argument. */
  { a_const_char *src = start_of_curr_token;
    char         *dest = map->raw_text;
    /* For a wide string literal, skip the leading "L". */
    if (*src == 'L') src++;
    /* Skip over the opening quote. */
    check_assertion(*src == '"');
    src++;
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Record initial offset in text map. */
    conv_line_loc_to_source_pos(src, &curr_pos);
    add_entry_to_macro_text_map(&map->raw_text_map, /*start_of_region=*/0,
                                curr_pos.seq, curr_pos.column,
                                NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    for (; src < end_of_string;) {
      /* If this is a \" or \\, ignore the initial character. */
      if (*src == '\\') {
        char	next = *(src+1);
        if (next == '"' || next == '\\') {
          ++src;
#if FULLY_RESOLVED_MACRO_POSITIONS
          /* Reflect change in buffer and source-position column offsets. */
          conv_line_loc_to_source_pos(src, &curr_pos);
          add_entry_to_macro_text_map(&map->raw_text_map,
                                      (sizeof_t)(dest - map->raw_text),
                                      curr_pos.seq, curr_pos.column,
                                      NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
        }  /* if */
      }  /* if */
      *dest++ = *src++;
    }  /* for */
    /* Compute the length of the string, not counting the final escape
       sequence. */
    map->raw_len = dest - map->raw_text;
    /* Append an end-of-buffer escape. */
    *dest++ = LE_ESCAPE;
    *dest++ = LE_END_OF_INSERTION;
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Add the terminating map entry. */
    add_entry_to_macro_text_map(&map->raw_text_map,
                                (sizeof_t)(dest - map->raw_text),
                                (a_seq_number)0, SP_COL_UNKNOWN,
                                NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }
  return map;
}  /* copy_pragma_string */


static void scan_pragma_string(
			a_macro_arg_ptr			map,
			a_source_position		*start_of_dir_position,
			a_pragma_kind_description_ptr	*pragma_descr)
/*
The replacement text for "map" points to the string of a _Pragma operator.
Scan the contents of that string as tokens.  start_of_dir_position
is the source position of the _Pragma token.  Information about the pragma
that was found is returned in *pragma_descr.  Note that the value returned
through *pragma_descr can be NULL.
*/
{
  a_source_line_modif_ptr	slmp;
  a_const_char			*save_curr_char_loc;
  a_pointer_registration_ptr	save_registered_pointers = registered_pointers;
  a_pointer_registration	save_curr_char_loc_reg;

  /* Register the pointer variables used by the routine in case any of
     the structures get reallocated during this processing. */
  register_pointer_variable(save_curr_char_loc, save_curr_char_loc_reg);
  /* Save the state of the lexical variables used by the source line
     modification process. */
  save_curr_char_loc = curr_char_loc;
  check_assertion(delete_source_from_loc == NULL);
  /* Insert a modification for the copied contents of the string at the
     current token.  It doesn't really matter what is replaced; we're
     going to remove the modification later.  It just needs to be linked
     into the lexical data structure temporarily. */
  slmp = add_source_line_modif(start_of_curr_token,
                               len_of_curr_token,
                               &map->raw_text[0],
                               &map->raw_text[map->raw_len]);
  slmp->is_isolated_text = TRUE;
  curr_char_loc = map->raw_text;
#if FULLY_RESOLVED_MACRO_POSITIONS
  /* Use the raw text map entries from the macro argument. */
  slmp->text_map.num_entries = map->raw_text_map.num_entries;
  slmp->text_map.entries = map->raw_text_map.entries;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* Actually scan the tokens that make up the pragma. */
  { a_pragma_kind_description_ptr	pkdp = NULL;
    a_source_position			id_position;
    /* Get the pragma identifier. */
    pkdp = look_up_pragma_id(&id_position);
    *pragma_descr = pkdp;
    if (pkdp != NULL &&
        pkdp->binding_kind == pbk_preproc_immediate &&
        !pkdp->allowed_in_pragma_operator) {
      /* Certain preprocessing pragmas cannot be used in _Pragma operators. */
      pos_error(ec_invalid_pragma_operator, &id_position);
      flush_to_newline();
    }  /* if */
    record_pragma(pkdp, start_of_dir_position, &id_position,
                  /*is_microsoft_pragma_operator=*/FALSE);
  }
  rem_source_line_modif(slmp);
  /* Restore the saved lexical state variables. */
  curr_char_loc = save_curr_char_loc;
  /* Unlink the registered pointers for this function from the list. */
  registered_pointers = save_registered_pointers;
}  /* scan_pragma_string */


static void scan_pragma_operator(
		a_boolean			*got_proper_closing_token,
		a_pragma_kind_description_ptr	*pragma_descr)
/*
Process a C99-style _Pragma operator.  The current token is the _Pragma
identifier token.  The form of a _Pragma invocation is:

	_Pragma("string")

The first component of "string" is the pragma identifier, which may be followed
by pragma arguments.

If the pragma operator is badly formed and we don't successfully find its
end, got_proper_closing_token is set to FALSE, otherwise it is unchanged.
Information about the pragma that was found is returned in *pragma_descr.
Note that the value returned through *pragma_descr can be NULL.
*/
{
  a_boolean		save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean		save_expand_macros = expand_macros;
  a_source_position	start_of_dir_position;
  a_boolean		found_end_of_operator = FALSE;
  a_boolean		err = FALSE;

  /* The inside of the _Pragma directive should be processed as pp-tokens. */
  fetch_pp_tokens = TRUE;
  /* Record the position of the start of the pragma. */
  start_of_dir_position = pos_curr_token;
  /* Bypass the _Pragma token. */
  (void)get_token();
  *pragma_descr = NULL;
  if (curr_token != tok_lparen) {
    pos_error(ec_exp_lparen, &error_position);
    err = TRUE;
  } else if (get_token() != tok_string_literal) {
    pos_error(ec_exp_string_literal, &error_position);
    err = TRUE;
  } else {
    a_macro_arg_ptr	map;
    /* We've scanned a string literal.  Make a copy of the string
       literal replacing \" with " and \\ with \. */
    map = copy_pragma_string();
    /* Scan the tokens from the pragma string.  Don't expand macros inside the
       string. */
    expand_macros = FALSE;
    scan_pragma_string(map, &start_of_dir_position, pragma_descr);
    free_macro_arg(&map);
  }  /* if */
  /* Restore the previous state for expanding macros so that the closing
     parenthesis can be supplied by a macro. */
  expand_macros = save_expand_macros;
  if (!err) {
    /* Bypass the scanned string and check for the closing parenthesis. */
    (void)get_token();
    if (curr_token == tok_rparen) {
      found_end_of_operator = TRUE;
    } else {
      pos_error(ec_exp_rparen, &error_position);
      curr_char_loc = start_of_curr_token;
    }  /* if */
  }  /* if */
  /* Restore the previous state for fetching pp-tokens. */
  fetch_pp_tokens = save_fetch_pp_tokens;
  /* If we didn't find the end of the operator, clear the flag passed
     by the caller. */
  if (!found_end_of_operator) {
    *got_proper_closing_token = FALSE;
    /* The main purpose of the following is to insure that we don't return
       a tok_identifier when we might have an invalid locator (e.g., the
       symbol header could be unset). */
    if (curr_token != tok_end_of_source) curr_token = tok_error;
  }  /* if */
  /* If the pragma was an immediate pragma, process it now. */
  process_immediate_pragmas();
}  /* scan_pragma_operator */


static void process_microsoft_pragma_operator(
			a_source_position		*start_of_dir_position,
			a_pragma_kind_description_ptr	*pragma_descr)
/*
The current token is the pragma identifier of a Microsoft __pragma operator.
Call record_pragma to scan the pragma body and create the pragma entry.
Information about the pragma that was found is returned in *pragma_descr.
Note that the value returned through *pragma_descr can be NULL.
*/
{
  a_pragma_kind_description_ptr	pkdp = NULL;
  a_source_position			id_position;
  /* Get the pragma identifier. */
  pkdp = look_up_pragma_id(&id_position);
  *pragma_descr = pkdp;
  if (pkdp != NULL &&
      pkdp->binding_kind == pbk_preproc_immediate &&
      !pkdp->allowed_in_pragma_operator) {
    /* Certain preprocessing pragmas cannot be used in __pragma operators. */
    pos_error(ec_invalid_microsoft_pragma_operator, &id_position);
    flush_to_closing_paren();
  } else {
    record_pragma(pkdp, start_of_dir_position, &id_position,
                 /*is_microsoft_pragma_operator=*/TRUE);
  }  /* if */
}  /* process_microsoft_pragma_operator */


static void scan_microsoft_pragma_operator(
		a_boolean			*got_proper_closing_token,
		a_pragma_kind_description_ptr	*pragma_descr)
/*
Process a Microsoft __pragma operator.  The current token is the
__pragma identifier token.  The form of a __pragma invocation is:

	__pragma(tokens)

The first component of "tokens" is the pragma identifier, which may be
followed by pragma arguments.

If the pragma operator is badly formed and we don't successfully find its
end, got_proper_closing_token is set to FALSE, otherwise it is unchanged.
Information about the pragma that was found is returned in *pragma_descr.
Note that the value returned through *pragma_descr can be NULL.
*/
{
  a_boolean		save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean		save_expand_macros = expand_macros;
  a_source_position	start_of_dir_position;
  a_boolean		found_end_of_operator = FALSE;

  /* The inside of the _pragma directive should be processed as pp-tokens. */
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
  *pragma_descr = NULL;
  /* Record the position of the start of the pragma. */
  start_of_dir_position = pos_curr_token;
  /* Bypass the __pragma token. */
  (void)get_token();
  if (curr_token != tok_lparen) {
    pos_error(ec_exp_lparen, &error_position);
  } else {
    /* Scan the tokens of the pragma and create the pragma entry. */
    process_microsoft_pragma_operator(&start_of_dir_position, pragma_descr);
    /* Check for the closing parenthesis. */
    if (curr_token == tok_rparen) {
      found_end_of_operator = TRUE;
    } else {
      pos_error(ec_exp_rparen, &error_position);
      curr_char_loc = start_of_curr_token;
    }  /* if */
  }  /* if */
  /* Restore the previous state for fetching pp-tokens, and expanding
     macros. */
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  /* If we didn't find the end of the operator, clear the flag passed
     by the caller. */
  if (!found_end_of_operator) {
    *got_proper_closing_token = FALSE;
    /* The main purpose of the following is to insure that we don't return
       a tok_identifier when we might have an invalid locator (e.g., the
       symbol header could be unset). */
    if (curr_token != tok_end_of_source) curr_token = tok_error;
  }  /* if */
  /* If the pragma was an immediate pragma, process it now. */
  process_immediate_pragmas();
}  /* scan_microsoft_pragma_operator */


/*
Skip white space and set a flag indicating whether or not any was
skipped.  Used within macro invocations.  Under pcc compatibility mode,
comment-only white space is ignored.
*/
#define macro_skip_white_space(any_skipped) \
{ skip_white_space(); \
  any_skipped = FALSE; \
  if (kind_of_white_space_skipped != 0) { \
    if (kind_of_white_space_skipped != WHITE_SPACE_COMMENTS || \
        !pcc_preprocessing_mode) { \
      any_skipped = TRUE; \
    }  /* if */ \
  }  /* if */ \
}  /* macro_skip_white_space */


void choose_raw_or_expanded_arg(void)
/*
curr_char_loc points to an LE_RAW_OR_EXPANDED_ARGUMENT lexical escape.
Immediately following is the ATTENTION_MARKER of a deletion source line
modification whose deleted text is the raw version of a macro argument.
Immediately following the deleted text is an ATTENTION_MARKER of a deletion
source line modification whose deleted text is the expanded version of the
macro argument.  Depending on the value of use_raw_version_of_arg, remove
one or the other of those source line modifications and change the
LE_RAW_OR_EXPANDED_ARGUMENT lexical escape to an LE_END_OF_TOKEN so this
routine will not be invoked twice for the same argument.

This is used to support the following Microsoft idiosyncrasy.  Given the
macro definitions

    #define M1(x) M2(##x)
    #define M3 xyz

and an invocation

    M1(M3)

whether the argument to M1 is expanded to xyz or left as M3 depends on the
characteristics of M2.  If M2 is a macro that uses its argument as an
operand of a paste (##) operator, the raw version of M3 is used; otherwise,
the expanded version is used.  For example, with

    #define M2(x) abc ## x

the result will be abcM3, while with

    #define M2(x) abc x

the result will be abc xyz.  To support deferring the determination of
whether to use the raw or expanded version of the argument until its
eventual use is known, both versions are copied into the expanded text with
their text deleted, and the construct preceded by an
LE_RAW_OR_EXPANDED_ARGUMENT lexical escape.  When macro_invocation is about
to read the first token of a macro argument, it sets use_raw_version_of_arg
according to whether the corresponding parameter is used in a paste
operation, and skip_white_space calls this routine whenever it encounters
an LE_RAW_OR_EXPANDED_ARGUMENT lexical escape.
*/
{
  a_const_char *raw_loc = curr_char_loc + LE_ESCAPE_LEN;
  a_source_line_modif_ptr raw_slmp = nested_source_line_modif(raw_loc);
  a_source_line_modif_ptr exp_slmp =
             nested_source_line_modif(raw_loc + raw_slmp->num_chars_to_delete);

  if (use_raw_version_of_arg) {
    /* Remove the deletion source line modification from the raw version of
       the argument, leaving the expanded one deleted, and update the
       deletion count in macro_buffer accordingly. */
    rem_source_line_modif(raw_slmp);
    free_source_line_modif(&raw_slmp);
    num_chars_deleted_in_macro_buffer += exp_slmp->num_chars_to_delete - 1;
  } else {
    /* Remove the deletion source line modification from the expanded
       version of the argument, leaving the raw one deleted, and update the
       delete count in macro_buffer accordingly. */
    rem_source_line_modif(exp_slmp);
    free_source_line_modif(&exp_slmp);
    num_chars_deleted_in_macro_buffer += raw_slmp->num_chars_to_delete - 1;
  }  /* if */
  /* Ensure that this routine is not called again for this argument. */
  *(char *)(curr_char_loc + 1) = LE_END_OF_TOKEN;
}  /* choose_raw_or_expanded_arg */


static a_token_kind arg_get_token(a_boolean     *any_white_space_skipped)
/*
Fetch and return a token as part of scanning a macro argument.  Return
*any_white_space_skipped == TRUE if any white space was skipped (the white
space will also be deleted).  The global variable
arg_get_token_start_of_curr_token is set to the character position after
the white-space skip, which differs from start_of_curr_token when the token
is preceded by an inert-macro escape.  The global variable
comma_is_from_argument will be TRUE after the call if and only if the call
to skip_white_space encountered an LE_COMMA_FROM_ARGUMENT marker.  The
global variable comma_is_magic will be TRUE after the call if and only if
the call to skip_white_space encountered an LE_MICROSOFT_MAGIC_COMMA
followed by a comma.  In that case, whether the comma is skipped or
included depends on the context.  If the comma appears directly in the
argument to a macro that takes exactly one parameter, it is included; in
all other cases, it is skipped.
*/
{
  a_token_kind tok;
  a_boolean    skip_comma;

  do {
    comma_is_from_argument = FALSE;
    comma_is_magic = FALSE;
    macro_skip_white_space(*any_white_space_skipped);
    arg_get_token_start_of_curr_token = curr_char_loc;
    if (comma_is_magic && !within_curr_source_line(curr_char_loc)) {
      /* The skip traversed an LE_MICROSOFT_MAGIC_COMMA lexical escape, so
         curr_char_loc now points to a comma that preceded an empty
         __VA_ARGS__ expansion.  Such commas are suppressed unless they
         appear directly in a macro argument list and that macro has
         exactly one parameter.  Check to see if the comma at curr_char_loc
         is embedded in the expansion of one of this macro's arguments,
         i.e., if its source modification is newer than the one containing
         the name of the macro.  If so, or if the macro whose arguments are
         being fetched has more than one parameter, it's an ordinary magic
         comma that needs to be skipped; otherwise, it should not be
         suppressed. */
      a_source_line_modif_ptr slmp = assoc_source_line_modif(curr_char_loc);
      skip_comma = (slmp->sequence_id > macro_name_modif_seq ||
                    !single_param_macro);
    } else {
      skip_comma = FALSE;
    }  /* if */
    if (skip_comma) {
      /* Step over the comma and loop again to skip any following white
         space. */
      ++curr_char_loc;
    } else {
      /* Get the next token and exit the loop. */
      tok = get_token();
    }  /* if */
  } while (skip_comma);
  return tok;
}  /* arg_get_token */


static sizeof_t length_for_curr_token_save(a_boolean need_end_of_token_marker,
                                           a_boolean any_white_space_skipped)
/*
Return the number of characters needed to save the current token
as text in a buffer.  need_end_of_token_marker is TRUE if an end-of-
token marker escape is needed.  any_white_space_skipped is TRUE if
there was any white space preceding the token.
*/
{
  sizeof_t len = len_of_curr_token;

  if (any_white_space_skipped) len++;
  if (need_end_of_token_marker) len += LE_ESCAPE_LEN;
  if (curr_token_is_inert_macro) len += LE_ESCAPE_LEN;
  return len;
}  /* length_for_curr_token_save */


static void add_curr_token_text_to_buffer(a_boolean need_end_of_token_marker,
                                          a_boolean any_white_space_skipped,
                                          char      *buffer)
/*
Add a textual version of the current token to the indicated buffer.
If need_end_of_token_marker is TRUE, put an end-of-token marker escape
out first.  If any_white_space_skipped is TRUE, a blank is put out
before the token (and after the end-of-token marker, if any).
The current token must have been scanned as a pp-token.
*/
{
  if (need_end_of_token_marker) {
    *buffer++ = LE_ESCAPE;
    *buffer++ = LE_END_OF_TOKEN;
  }  /* if */
  if (any_white_space_skipped) {
    *buffer++ = ' ';
  }  /* if */
  if (curr_token_is_temporarily_inert_macro) {
    /* This token is the name of a function-style macro that could not be
       expanded when it was first encountered because the closing right
       parenthesis was not found.  We do not mark it as such in this buffer
       in order to allow a right parenthesis at this level to close the
       invocation (a Microsoft quirk). */
    if (!need_end_of_token_marker) {
      /* The LE_TEMPORARILY_INERT_MACRO lexical escape would have
         terminated the preceding token.  If we did not previously add an
         end-of-token marker, do so now. */
      *buffer++ = LE_ESCAPE;
      *buffer++ = LE_END_OF_TOKEN;
    }  /* if */
  } else if (curr_token_is_inert_macro) {
    /* Prefix for a macro identifier name that indicates that the name came
       from its own expansion and should not be expanded further. */
    *buffer++ = LE_ESCAPE;
    *buffer++ = LE_INERT_MACRO;
  }  /* if */
  (void)memcpy((char *)buffer, (char *)start_of_curr_token,
               size_t_arg(len_of_curr_token));
}  /* add_curr_token_text_to_buffer */


static sizeof_t stringized_arg(a_macro_arg_ptr map,
                               char            **src_loc,
                               a_boolean       charize)
/*
Generate the "stringized" version of the macro argument indicated by map,
store it into the current source line at *src_loc, and increment
*src_loc appropriately.  See standard, 3.8.3.2.  The "#" operator produces
the stringized version of an argument.  Return the length of the
stringized version.  If src_loc is NULL, the output is not stored, so
the overall function of this routine is just to compute and return the
length of the stringized version.
This function also supports a Microsoft extension that makes the "#@" operator
produce a "charized" version of the argument (i.e., a character literal).
In such cases, charize is TRUE.
*/
{
  register sizeof_t len = 0;
  register char     *p;
  register char     ch;
  a_boolean         within_char_literal = FALSE;
  a_boolean         start_of_token = TRUE;
  char              quote_char = (charize ? '\'' : '"');

  /* Put out initial quote. */
  len++;
  if (src_loc != NULL) *(*src_loc)++ = quote_char;
  /* Scan through the raw text of the argument, stopping at the end.
     Delete end of token markers.  Keep track of when we are inside of
     a character constant or string literal, and put out a "\" in front
     of each " or \ within those. */
  /*lint --e{850} p modified in loop */
  for (p = map->raw_text; ; p++) {
    ch = *p;
    if (ch == LE_ESCAPE) {
      if (p[1] == LE_END_OF_TOKEN || p[1] == LE_INERT_MACRO ||
          p[1] == LE_TEMPORARILY_INERT_MACRO ||
          p[1] == LE_COMMA_FROM_ARGUMENT ||
          p[1] == LE_RAW_OR_EXPANDED_ARGUMENT) {
        /* End of token marker, also indicates end of character constant or
           string literal, and start of another token soon.  The end of
           token marker itself is not put out.  The inert-macro, comma, and
           argument markers are handled the same way. */
        within_char_literal = FALSE;
        start_of_token = TRUE;
        p += LE_ESCAPE_LEN-1;
      } else if (p[1] == LE_END_OF_INSERTION) {
        /* End of argument. */
        break;
      } else if (p[1] == LE_NULL) {
        /* A null is passed through as \000 if inside a string.  Otherwise,
           it's discarded. */
        if (within_char_literal) {
          len += 4;
          if (src_loc != NULL) {
            *(*src_loc)++ = '\\';
            *(*src_loc)++ = '0';
            *(*src_loc)++ = '0';
            *(*src_loc)++ = '0';
          }  /* if */
        }  /* if */
        p += LE_ESCAPE_LEN-1;
#if !FULLY_RESOLVED_MACRO_POSITIONS
      /* Note: LE_END_OF_TOP_LEVEL_EXPANSION should never occur in a macro
         argument, so it is not handled directly and just falls through to
         the following unexpected condition clause. */
#endif /* !FULLY_RESOLVED_MACRO_POSITIONS */
      } else {
        unexpected_condition_str("stringized_arg: bad lexical escape");
      }  /* if */
    } else {
      /* If the current character is a " or ' at the start of a token,
         then this token is a character constant or string literal. */
      if (start_of_token &&
          (ch == '"' || ch == '\'' ||
           ((ch == 'u' || ch == 'U' || ch == 'L' || ch == 'R') &&
            scan_encoding_prefix(p) != SCLK_NOT_A_LITERAL))) {
        /* Start of character constant or string literal. */
        within_char_literal = TRUE;
      }  /* if */
      /* Reset the start of token flag on the first actual character of
         a token.  Note that all white space has already been standardized
         to a single blank so that is all we have to check for. */
      if (ch != ' ') start_of_token = FALSE;
      if (within_char_literal &&
          (ch == quote_char || ch == '\\')) {
        /* Escape " and \ within a character constant or string literal.
           Note that the quotes delimiting string literals are
           replaced too.  (When charizing, the single quote rather than the
           double quote needs escaping.) */
        len++;
        if (src_loc != NULL) *(*src_loc)++ = '\\';
      }  /* if */
      /* Put out the character itself. */
      len++;
      if (src_loc != NULL) *(*src_loc)++ = ch;
      if (!within_char_literal && microsoft_mode && ch == ')') {
        /* In Microsoft mode we suppress the end-of-token indicator
           following a right parenthesis to allow token concatenation
           between the last fragment of a macro expansion and the
           immediately-following text.  As a result we need special
           handling to recognize that anything following a right
           parenthesis is the start of a new token; otherwise, we will
           fail to escape the quotes in a literal that immediately follows
           a cast, e.g., (T)"x" or (T)L"y". */
        start_of_token = TRUE;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Put out final quote. */
  len++;
  if (src_loc != NULL) *(*src_loc)++ = quote_char;

  return (len);
}  /* stringized_arg */


static void expand_top_level_pcc_macro(a_source_line_modif_ptr main_slmp)
/*
We are in pcc mode, and a top-level macro has just been expanded.  main_slmp
points to the source modification that inserts the body of the macro
into the primary source line.  In order to more closely approximate the
token-pasting behavior of pcc, macro-expand the text in the body of
the macro, then make a copy of the macro-expanded version as one long
string.  If some of the rest of the primary source line looks like
it could be token-pasted with the last token in the macro expansion, add
it to the end of the macro expansion (and effectively remove it from the
primary source line).  Note that after the first call of this routine
that pastes on the end of the line, any subsequent calls of this routine
for that line will see the tacked-on text rather than the primary source
line (e.g., main_slmp will be a modification of that text).  Also used
in Microsoft mode; in that case, token pasting off the end is not allowed.
*/
{
  a_boolean     save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean	save_treat_newline_as_token = treat_newline_as_token;
  a_boolean     any_white_space_skipped = FALSE;
  unsigned long sequence_id;
  a_source_line_modif_ptr
		slmp,
                slmp2;
  sizeof_t      len_new;
  a_token_kind  last_token_of_expansion;
  a_boolean     aux_buffer_modified = FALSE;
  sizeof_t      num_chars_added_from_source_line = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_text_map_position_tracker
                tracker;
  a_boolean     tracker_inited = FALSE;
  a_source_position
                pos_of_rest_of_text;
  sizeof_t      next_targ_offset;
  sizeof_t      save_num_macro_text_map_entries = macro_text_map.num_entries;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char         *pos_in_aux_buffer;
  a_const_char *save_curr_char_loc;
  a_const_char *loc_following_insertion;
  char         *inert_macro_escape;
  a_pointer_registration
               pos_in_aux_buffer_reg, save_curr_char_loc_reg,
               loc_following_insertion_reg, inert_macro_escape_reg;
  a_pointer_registration_ptr
               save_registered_pointers = registered_pointers;

  register_pointer_variable(pos_in_aux_buffer,   pos_in_aux_buffer_reg);
  register_pointer_variable(save_curr_char_loc,  save_curr_char_loc_reg);
  register_pointer_variable(loc_following_insertion,
                                                 loc_following_insertion_reg);
  register_pointer_variable(inert_macro_escape,  inert_macro_escape_reg);

  db_enter(4, "expand_top_level_pcc_macro");
  /* The macro body is at main_slmp->inserted_text.  It's a single piece
     of text because in pcc mode macro arguments are not macro-expanded
     before being inserted into the macro body.  Tokenize the body
     of the macro and save the text of the tokens and white-space scanned
     in an auxiliary buffer.  At the end, copy the text in the auxiliary
     buffer back into macro_buffer, replacing the original (now macro-expanded)
     body of the macro. */
  /* Fetch the tokens as pp tokens. */
  save_fetch_pp_tokens = fetch_pp_tokens;
  fetch_pp_tokens = TRUE;
  /* Push a new lexical state so that tokens fetched by this process
     will not be eligible for caching in the current state. */
  push_lexical_state_stack();
  save_curr_char_loc = curr_char_loc;
  curr_char_loc = main_slmp->inserted_text;
  delete_source_from_loc = NULL;
  expand_macros = TRUE;
  /* Turn on some special processing at the end of the macro insertion
     to decide whether we need to continue into the primary line. */
  main_slmp->being_rescanned_for_token_pasting = TRUE;
  /* Make sure we don't run off the current line if we do need to run
     off the end of the macro. */
  treat_newline_as_token = TRUE;
  sequence_id = main_slmp->sequence_id;
  check_assertion(aux_buffer_for_pcc_macros != NULL);
  pos_in_aux_buffer = aux_buffer_for_pcc_macros;
  last_token_of_expansion = tok_end_of_source;
  inert_macro_escape = NULL;
  /* Set up check for whether a newline token is pushed onto a queue for
     rescanning. */
  newline_ungotten = FALSE;
  /* Fetch pp-tokens while doing macro expansion, and store the token
     text in the aux_buffer_for_pcc_macros.  Stop at the end of the
     top-level modification (usually). */
  for (;;) {
    a_boolean need_inert_macro_indication;
    if (!newline_ungotten) {
      /* If a newline was read during the processing and pushed onto a
         queue for rescanning, don't skip white space -- it would read a
         new source line and free all the source line modifications before
         we finish with them. */
      macro_skip_white_space(any_white_space_skipped);
    }  /* if */
    if (!main_slmp->being_rescanned_for_token_pasting) {
      /* We've run off the end of the top-level macro, because a macro call
         begins in the top-level modification and continues into the text
         in the primary source line.  (The lexical routines turn off the
         flag to indicate this case.) Once we reach the text following the
         call, exit the loop. */
      if (macro_depth == 1) {
        if (within_curr_source_line(curr_char_loc)) goto end_loop;
        /* Check for the case where the main modification does not return
           directly to the primary source line because it modifies the
           result of a previous invocation of this top-level-expansion
           routine. */
        slmp = assoc_source_line_modif(curr_char_loc);
        if (slmp == parent_source_line_modif(main_slmp)) goto end_loop;
      }  /* if */
    }  /* if */        
    /* We get back tok_end_of_source at the end of the top-level macro, or
       tok_newline if we ran to the end of the primary source line
       because we were in the parentheses of a macro invocation when
       we ran off the modification (an error would have been issued already
       in that case). */
    if (get_token() == tok_end_of_source || curr_token == tok_newline) break;
    if (inert_macro_escape != NULL) {
      /* We flagged the preceding identifier as an inert macro. */
      if (!any_white_space_skipped &&
          (curr_token == tok_identifier || curr_token == tok_pp_number)) {
        /* We're adding identifier characters with no white space
           separating them from the preceding identifier.  The
           combination will no longer name an inert macro, so change
           the escape from LE_INERT_MACRO to LE_END_OF_TOKEN. */
        *inert_macro_escape = LE_END_OF_TOKEN;
      }  /* if */
      inert_macro_escape = NULL;
    }  /* if */
    need_inert_macro_indication = (!pcc_preprocessing_mode &&
                                   curr_token_is_inert_macro &&
                                   !curr_token_is_temporarily_inert_macro);
    /* Make enough room in the aux. buffer for the token text. */
    ensure_aux_buffer_for_pcc_macros_space(len_of_curr_token +
                                           any_white_space_skipped +
                                           (need_inert_macro_indication ?
                                                            LE_ESCAPE_LEN : 0),
                                           pos_in_aux_buffer);
    /* If the token was preceded by white-space, put a blank in the
       auxiliary buffer. */
    if (any_white_space_skipped) *pos_in_aux_buffer++ = ' ';
    if (need_inert_macro_indication &&
        (any_white_space_skipped ||
         last_token_of_expansion != tok_identifier)) {
      /* The current token is the name of an inert macro that is not being
         concatenated with a preceding identifier (i.e., will persist as a
         distinct identifier during the rescan); keep the inert macro
         indication (e.g., for Microsoft mode). */
      *pos_in_aux_buffer++ = LE_ESCAPE;
      inert_macro_escape = pos_in_aux_buffer;
      *pos_in_aux_buffer++ = LE_INERT_MACRO;
    } else if (curr_token_is_temporarily_inert_macro) {
      /* The auxiliary buffer won't have the LE_TEMPORARILY_INERT_MACRO
         escape; mark it as modified so we'll use it instead of the
         original, even if there are no further macro expansions. */
      aux_buffer_modified = TRUE;
    }  /* if */
    /* Copy the text of the token to the auxiliary buffer. */
#if FULLY_RESOLVED_MACRO_POSITIONS
    if (!tracker_inited) {
      /* Initialize the position tracker to reflect the start of the text
         being scanned and the position in the auxiliary buffer.  (We must do
         it this way, with an "inited" flag, rather than before the loop
         because the first get_token() is done as part of the loop, and the
         tracker initialization depends on information about the current
         token.)  Use NO_PARENT_MACRO_INVOCATION to preserve the context
         from cloned text map entries. */
      init_text_map_position_tracker(&tracker, &aux_text_map_for_pcc_macros,
                                     NO_PARENT_MACRO_INVOCATION);
      tracker_inited = TRUE;
    }  /* if */
    add_token_to_macro_text_map(&tracker,
                                (sizeof_t)(pos_in_aux_buffer -
                                           aux_buffer_for_pcc_macros));
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    (void)memcpy(pos_in_aux_buffer, start_of_curr_token,
                 size_t_arg(len_of_curr_token));
    pos_in_aux_buffer += len_of_curr_token;
    last_token_of_expansion = curr_token;
  }  /* for */
end_loop:
#if FULLY_RESOLVED_MACRO_POSITIONS
  if (tracker_inited) {
    terminate_macro_text_map(&tracker, (sizeof_t)(pos_in_aux_buffer -
                                                  aux_buffer_for_pcc_macros));
  }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* Determine the location in the primary source line that immediately
     follows the end of the accumulated text. */
  if (curr_token == tok_end_of_source) {
    leave_insertion(main_slmp, curr_char_loc);
  } else if (curr_token == tok_newline && curr_char_loc[-2] == LE_ESCAPE &&
             curr_char_loc[-1] == LE_NEWLINE) {
    /* Back up to keep the newline escape. */
    curr_char_loc -= LE_ESCAPE_LEN;
  }  /* if */
  loc_following_insertion = curr_char_loc;
  if (!main_slmp->being_rescanned_for_token_pasting) {
    a_boolean deletion_can_be_extended = FALSE;
    if (within_curr_source_line(main_slmp->line_loc) &&
        within_curr_source_line(loc_following_insertion)) {
      /* Both the insertion point and the location following the insertion
         are in the primary source line, so the deletion can be extended. */
      deletion_can_be_extended = TRUE;
    } else {
      /* Otherwise, the deletion can only be extended if both the insertion
         point and the location following the insertion are in the same
         source line modification.  (This will not be true only in some
         obscure error cases involving malformed "defined(" operators.) */
      a_source_line_modif_ptr line_loc_slmp =
                        assoc_source_line_modif_full(main_slmp->line_loc,
                                                     /*failure_allowed=*/TRUE);
      a_source_line_modif_ptr after_ins_slmp =
                        assoc_source_line_modif_full(loc_following_insertion,
                                                     /*failure_allowed=*/TRUE);
      deletion_can_be_extended = (line_loc_slmp == after_ins_slmp);
    }  /* if */
    if (deletion_can_be_extended) {
      /* We went off the end of the modification because of an open
         macro argument list.  Adjust the line modification so that the
         additional text is also deleted. */
      main_slmp->num_chars_to_delete = loc_following_insertion -
                                       main_slmp->line_loc;
    }  /* if */
  }  /* if */
  if (pcc_preprocessing_mode) {
    /* Special trick to deal with cases like
         #define x(a) "a
         char *y = x(1)23";
       i.e., token pasting across the end of a top-level macro call.
       If the macro expansion ends with tok_error (which may indicate an
       unclosed string), or if the final character of the expansion looks like
       it could be pasted with the first character following the expansion,
       tack the rest of the primary source line onto the end of the aux.
       buffer. */
    /* Use pp_lexical_category to see if the last character and next character
       could appear together in a token.  If they're singletons, they always
       stand alone and therefore could not appear next to one another.
       Otherwise, if they have the same category, they might appear next to one
       another in a token. */
    char   last_char_of_expansion;
    a_byte cat_last_char, cat_next_char;
    if (pos_in_aux_buffer != aux_buffer_for_pcc_macros) {
      last_char_of_expansion = pos_in_aux_buffer[-1];
    } else {
      last_char_of_expansion = '\n';
    }  /* if */
    cat_last_char = pp_lexical_category[last_char_of_expansion-CHAR_MIN];
    cat_next_char = pp_lexical_category[*loc_following_insertion-CHAR_MIN];
    if (last_token_of_expansion == tok_error ||
        (cat_last_char != PLC_SINGLETON && cat_last_char == cat_next_char)) {
      /* The categories indicate that token pasting might be possible.
         Tack the rest of the primary source line onto the end of the expansion
         buffer so that the macro and what follows have a chance to be pasted
         together. */
      /* Find the end of the primary source line. */
      { a_const_char *temp;  /* Not registered, not kept long. */
        for (num_chars_added_from_source_line = 0,
               temp = loc_following_insertion;
             ;
             num_chars_added_from_source_line++,
               temp++) {
          if (*temp == LE_ESCAPE) {
            if (temp[1] == LE_END_OF_LINE ||
                temp[1] == LE_END_OF_INSERTION) break;
            num_chars_added_from_source_line++;
            temp++;
          }  /* if */
        }  /* for */
        aux_buffer_modified = TRUE;
        /* Do not take the newline from the primary source line. */
        if (num_chars_added_from_source_line >= LE_ESCAPE_LEN &&
            temp[-LE_ESCAPE_LEN] == LE_ESCAPE &&
            temp[-1]             == LE_NEWLINE) {
          num_chars_added_from_source_line -= LE_ESCAPE_LEN;
        }  /* if */
      }
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug,
              "Tacking rest of containing line onto macro expansion:\n%.*s\n",
              (int)num_chars_added_from_source_line, loc_following_insertion);
      }  /* if */
#endif /* DEBUG */
      ensure_aux_buffer_for_pcc_macros_space(num_chars_added_from_source_line,
                                             pos_in_aux_buffer);
#if FULLY_RESOLVED_MACRO_POSITIONS
      /* We're adding text after the supposed end of the mapped regions in
         the buffer (as reflected in the preceding call to
         terminate_macro_text_map): back up over the final entry and add a
         new region and a new termination directly.  (We can't use the
         tracker mechanism because that depends on information about the
         current token, and this additional text has not yet been
         tokenized.) */
      --aux_text_map_for_pcc_macros.num_entries;
      conv_line_loc_to_source_pos(loc_following_insertion,
                                  &pos_of_rest_of_text);
      next_targ_offset = pos_in_aux_buffer - aux_buffer_for_pcc_macros;
      add_entry_to_macro_text_map(&aux_text_map_for_pcc_macros,
                                  next_targ_offset,
                                  pos_of_rest_of_text.seq,
                                  pos_of_rest_of_text.column,
                                  NO_PARENT_MACRO_INVOCATION);
      next_targ_offset += num_chars_added_from_source_line + LE_ESCAPE_LEN;
      add_entry_to_macro_text_map(&aux_text_map_for_pcc_macros,
                                  next_targ_offset, (a_seq_number)0,
                                  SP_COL_UNKNOWN, NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
      (void)memcpy(pos_in_aux_buffer, loc_following_insertion,
                   size_t_arg(num_chars_added_from_source_line));
      pos_in_aux_buffer += num_chars_added_from_source_line;
      /* Adjust the line modification so that the additional text in the
         primary source line is also deleted. */
      main_slmp->num_chars_to_delete += num_chars_added_from_source_line;
    }  /* if */
  }  /* if */
  /* Put an end-of-insertion lexical escape at the end of the aux. buffer. */
  ensure_aux_buffer_for_pcc_macros_space(LE_ESCAPE_LEN, pos_in_aux_buffer);
  *pos_in_aux_buffer++ = LE_ESCAPE;
  *pos_in_aux_buffer++ = LE_END_OF_INSERTION;
  /* Restore the flags that were changed before the scan. */
  main_slmp->being_rescanned_for_token_pasting = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  pop_lexical_state_stack();
  curr_char_loc = save_curr_char_loc;
  treat_newline_as_token = save_treat_newline_as_token;
  /* The body of the top-level macro has been expanded.  macro_buffer
     contains the expansion represented by source line modifications,
     and aux_buffer_for_pcc_macros contains the expansion in raw-text form. */
  /* See if there are any source line modifications made since the one
     to insert the macro body.  If so, some macro expansion was done;
     we free the entries and go on to do the copy.  If not, no macro
     expansion was done. */
  if (sequence_id != sequence_id_for_source_line_modifs) {
    /* There was at least one internal macro expansion, so the aux. buffer
       will be copied into macro_buffer. */
    aux_buffer_modified = TRUE;
    /* Remove the source modifications for the internal macro expansion.
       We don't need the information in them, since we have the full text
       we want in the aux. buffer. */
    for (slmp = source_line_modif_list; slmp != NULL;) {
      slmp2 = slmp;
      slmp = slmp->next;
      if (slmp2->sequence_id > sequence_id) {
        rem_source_line_modif(slmp2);
        free_source_line_modif(&slmp2);
      }  /* if */
    }  /* for */
  }  /* if */
  if (aux_buffer_modified) {
    /* Copy the aux. buffer text into macro_buffer.  We will just abandon the
       previous contents of the main_slmp's inserted text; it will be
       reclaimed during the next macro_buffer reallocation or truncation.  For
       now, just make the insertion appear empty, in case this call to
       ensure_macro_buffer_space() causes reallocation. */
    discarded_pcc_top_level_begin = main_slmp->inserted_text;
    discarded_pcc_top_level_end = main_slmp->end_inserted_text;
    main_slmp->inserted_text = main_slmp->end_inserted_text;
    len_new = pos_in_aux_buffer - aux_buffer_for_pcc_macros;
    ensure_macro_buffer_space(len_new);
    discarded_pcc_top_level_begin = NULL;
    discarded_pcc_top_level_end = NULL;
    /* Copy the new text into macro_buffer.  This will copy up to and
       including the final lexical escape. */
    (void)memcpy(next_avail_in_macro_buffer, aux_buffer_for_pcc_macros,
                 size_t_arg(len_new));
    /* Adjust the main_slmp insertion and macro_buffer pointers to reflect
       the new text. */
    main_slmp->inserted_text = next_avail_in_macro_buffer;
    next_avail_in_macro_buffer += len_new;
    main_slmp->end_inserted_text = next_avail_in_macro_buffer-LE_ESCAPE_LEN;
    if (num_chars_added_from_source_line != 0) {
      /* Some characters from the primary source line were tacked onto
         the expansion, so remember where that text starts. */
      main_slmp->text_from_primary_source_line =
             next_avail_in_macro_buffer - num_chars_added_from_source_line - 1;
    }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Copy the entries from the auxiliary text map to the macro text map
       (after resetting the count to ignore ones that were associated with
       now-discarded text in macro_buffer) and point the text_map of the
       main_slmp at the copied entries. */
    macro_text_map.num_entries = save_num_macro_text_map_entries;
    ensure_avail_text_map_entries(&macro_text_map,
                                  aux_text_map_for_pcc_macros.num_entries);
    (void)memcpy((char *)&macro_text_map.entries[macro_text_map.num_entries],
                 (char *)&aux_text_map_for_pcc_macros.entries[0],
                 size_t_arg(sizeof(a_macro_text_map_entry)*
                            aux_text_map_for_pcc_macros.num_entries));
    main_slmp->text_map.num_entries = aux_text_map_for_pcc_macros.num_entries;
    main_slmp->text_map.entries =
                           &macro_text_map.entries[macro_text_map.num_entries];
    macro_text_map.num_entries += aux_text_map_for_pcc_macros.num_entries;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* if */
  /* Drop any local pointer registrations. */
  registered_pointers = save_registered_pointers;
#if FULLY_RESOLVED_MACRO_POSITIONS
  /* Empty the map in preparation for next time. */
  aux_text_map_for_pcc_macros.num_entries = 0;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  db_exit();
}  /* expand_top_level_pcc_macro */


/*
Add an entry to the list of a_macro_arg entries in use.
*/
#define add_to_macro_arg_list(map)                                    \
{ if (macro_arg_list == NULL) {                                       \
    macro_arg_list = map;                                             \
  } else {                                                            \
    end_of_macro_arg_list->next = map;                                \
  }  /* if */                                                         \
  end_of_macro_arg_list = map;                                        \
}  /* add_to_macro_arg_list */

/* The first ARG_VALUES_SIZE macro argument values will be stored in a random
   access array (which should be called "arg_values"): */
#define ARG_VALUES_SIZE 50

/*
Add an a_macro_arg entry to the end of the current list of argument values.
The first ARG_VALUES_SIZE arguments are indexed in arg_values.  All entries
are linked together, so the later ones can be found, albeit slowly.
*/
#define add_to_arg_values(map)                                        \
{ if (param_num < ARG_VALUES_SIZE) arg_values[param_num] = map;       \
  param_num++;                                                        \
  add_to_macro_arg_list(map);                                         \
}  /* add_to_arg_values */


/*
Fetch the pointer to the a_macro_arg entry for argument "number" (first
is 1), and return it in map.  For the first ARG_VALUES_SIZE entries,
that's an easy look-up in arg_values.  After that, a linear search is needed.
*/
#define get_arg_value(number, map)                                    \
{ if (number <= ARG_VALUES_SIZE) {                                    \
    map = arg_values[number-1];                                       \
  } else {                                                            \
    sizeof_t n = ARG_VALUES_SIZE;                                     \
    map = arg_values[ARG_VALUES_SIZE-1];                              \
    do { n++; map = map->next; } while (n < number);                  \
  }  /* if */                                                         \
}  /* get_arg_value */ 


static void free_macro_arg_entries(a_macro_arg_ptr prev_end_of_macro_arg_list)
/*
Free the macro arg entries following "prev_end_of_macro_list" in the
global list of macro args.  Note that it must be possible to call this
routine more than once with the same pointer; the second call should do
nothing.
*/
{
  a_macro_arg_ptr map, next_map;

  end_of_macro_arg_list = prev_end_of_macro_arg_list;
  if (end_of_macro_arg_list == NULL) {
    /* Free everything on the list. */
    map = macro_arg_list;
    macro_arg_list = NULL;
  } else {
    /* Free everything following the given entry; clip the list off at that
       point, so it does not point to the released entries. */
    map = end_of_macro_arg_list->next;
    end_of_macro_arg_list->next = NULL;
  }  /* if */
  while (map != NULL) {
    next_map = map->next;
    free_macro_arg(&map);
    map = next_map;
  }  /* while */
}  /* free_macro_arg_entries */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_token_kind func_name_token[] = {tok_function_name,
                                         tok_decorated_function_name,
                                         tok_pretty_function_name};

static a_boolean is_microsoft_function_name_paste(a_macro_arg_ptr map,
                                                  a_const_char    *prev_text,
                                                  sizeof_t        prev_len,
                                                  a_const_char    **post_end)
/*
We are in Microsoft mode and we are doing a token paste in a macro
expansion.  Return TRUE if the paste operation is pasting "L" to one
of the Microsoft function-name keywords like __FUNCTION__.  
The raw value of the macro argument map is the text following
the "##", and prev_text (of length prev_len) is the text preceding
the ##.  If TRUE is returned, *post_end is set to the character
position after the end of the function-name keyword.
*/
{
  a_boolean result = FALSE;

  check_assertion(ms_extensions);
  /* MSVC++ 7.0 and 7.1 do this special pasting. */
  if (microsoft_version >= 1300 &&
      prev_len >= 1 && prev_text[prev_len-1] == 'L') {
    if (prev_len == 1 ||
        (prev_len >= LE_ESCAPE_LEN+1 &&
         prev_text[prev_len-LE_ESCAPE_LEN-1] == LE_ESCAPE &&
         prev_text[prev_len-LE_ESCAPE_LEN  ] == LE_END_OF_TOKEN)) {
      /* The preceding text ends with a token that is "L". */
      if (map->raw_len >= 3 &&
          map->raw_text[0] == '_' &&
          map->raw_text[1] == '_') {
        /* Compare the raw_text of map against the function-name tokens. */
        unsigned int i;
        for (i = 0; i < sizeof(func_name_token)/sizeof(a_token_kind); i++) {
          a_const_char *tok =
                          spelling_for_function_name_token(func_name_token[i]);
          sizeof_t     tok_len = strlen(tok);
          if (map->raw_len >= tok_len &&
              strncmp(map->raw_text, tok, size_t_arg(tok_len)) == 0) {
            /* The beginning of the raw text matches the token.  See if the
               text ends at that point or there is an end-of-token marker. */
            if (map->raw_len == tok_len ||
                (map->raw_len >= tok_len + LE_ESCAPE_LEN &&
                 map->raw_text[tok_len  ] == LE_ESCAPE &&
                 map->raw_text[tok_len+1] == LE_END_OF_TOKEN)) {
              /* Yes, everything is as required. */
              result = TRUE;
              *post_end = map->raw_text + tok_len;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_microsoft_function_name_paste */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static char *find_final_inert_escape(char     *text_loc,
                                     sizeof_t sect_len)
/*
text_loc points to a string of replacement text for an rt_raw_argument
insertion, of length sect_len.  If there is an inert-macro escape at the
end of the string (followed by an identifier, but no other escapes), return
a pointer to it.  Otherwise, return NULL.  (Note that this processing does
not apply to temporarily-inert macro escapes, which will be removed in the
expansion of the top-level macro invocation.)
*/
{
  char     *final_inert_escape = NULL;
  sizeof_t len;

  for (len = sect_len; len > 0; len--) {
    if (text_loc[len-1] == LE_ESCAPE) {
      if (text_loc[len] == LE_INERT_MACRO) {
        final_inert_escape = text_loc+len-1;
        break;
      } else if (text_loc[len] == LE_END_OF_TOKEN) {
        /* There's no inert-macro escape on the last token. */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return final_inert_escape;
}  /* find_final_inert_escape */


static void adjust_length_for_magic_arg(a_repl_text_seq_kind kind,
                                        char                 *rtp,
                                        sizeof_t             n_params,
                                        a_macro_arg_ptr      *arg_values,
                                        sizeof_t             *length,
                                        a_boolean            *add_escape)
/*
Check if the operator to which rtp points (an rt_paste or
rt_microsoft_magic_arg_marker) is followed by an empty substitution of the
variadic macro parameter.  If so, and if it is immediately preceded by a
comma (optionally followed by white space), the comma is generally removed
(along with any white space).  (The removal does not occur in Microsoft
mode when the comma appears inside a macro argument list.)  This strange
behavior is emulated for Microsoft variadic macros and when extended
variadic macros are enabled.  Some preprocessors (notably from the GNU
project) implement this to work around the following problem:

	#define M(fmt, args) printf(fmt , ## args)
	void f() { M("Hello.\n"); }

Without the "deletion effect", the macro would generate an extraneous
comma.  (The Microsoft variety of variadic macros does this even without
the "##" operator.)  kind describes what kind of section preceded the
operator.  rtp points to the replacement text sections starting at the
rt_paste or rt_microsoft_magic_arg_marker.  n_params is the number of
parameters in the macro.  arg_values is a pointer to an array of
a_macro_arg_ptr elements: it is referred to by the get_arg_value macro and
hence its name should not be changed.  *length is the value to be adjusted.
In non-Microsoft mode, *length is set to exclude the comma.  In Microsoft
mode, we don't know yet whether the comma will appear in a macro argument
list or not, so *length is set so that an LE_MICROSOFT_MAGIC_COMMA lexical
escape and a comma can be inserted and *add_escape is set to TRUE,
indicating to the caller the need to do so.  This lexical escape allows
skip_white_space to determine when the text is rescanned whether the comma
will be suppressed or not in the expanded text.  *add_escape is set to
FALSE in all other cases.
*/
{
  sizeof_t             arg_number;
  /* Move to the next section, skipping the rt_paste or
     rt_microsoft_magic_arg_marker placeholder. */
  char                 *ahead = rtp+1;
  a_repl_text_seq_kind rtp_op = (a_repl_text_seq_kind)*rtp;
  a_repl_text_seq_kind next_op;

  *add_escape = FALSE;
  get_macro_repl_text_number(arg_number, ahead);
  next_op = (a_repl_text_seq_kind)*(ahead++);
  if (next_op == rt_raw_argument ||
      next_op == rt_microsoft_maybe_raw_argument ||
      (ms_compat && next_op == rt_argument)) {
    /* A macro argument follows the concatenation.  (In Microsoft mode,
       the argument is expanded, whether or not preceded by "##".) */
    a_macro_arg_ptr map;

    get_macro_repl_text_number(arg_number, ahead);
    get_arg_value(arg_number, map);
    if (arg_number == n_params &&
        (map->raw_len == 0 ||
         (ms_compat && map->expanded_len == 0))) {
      /* The last macro parameter (presumably variadic) is empty or missing.
         So we adjust the section length to not include the last chunk of
         white space characters preceded by a comma: */
      if (kind == rt_text) {
        char *back = rtp-1;
        /* Skip preceding white space. */
        while (*back == ' ' || *back == '\t') { --back; }
        if (rtp_op == rt_microsoft_magic_arg_marker &&
            back[-1] == LE_ESCAPE && back[0] == LE_END_OF_TOKEN) {
          /* Unlike rt_paste, rt_microsoft_magic_arg_marker does not
             suppress a preceding end-of-token marker. */
          back -= LE_ESCAPE_LEN;
        }  /* if */
        if (*back == ',') {
          *length -= (rtp-back);
          if (microsoft_mode) {
            /* Leave space for an LE_MICROSOFT_MAGIC_COMMA escape and a
               comma to be inserted and indicate the need to do so. */
            *length += LE_ESCAPE_LEN + 1;
            *add_escape = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_length_for_magic_arg */


static sizeof_t length_of_replacement_text(char            *rtp,
                                           sizeof_t        n_params,
                                           a_macro_def_ptr mdp,
                                           a_macro_arg_ptr *arg_values)
/*
Compute the length (in bytes/characters) of the replacement text described by
the sequence of sections pointed to by rtp.  n_params is the number of macro
parameters of the macro described by mdp. arg_values is a pointer to an array
of a_macro_arg_ptr elements: it is referred to by the get_arg_value macro and
hence its name should not be changed.
*/
{
  sizeof_t  result = 0;
  a_boolean prev_section_is_paste = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  char      *prev_text = NULL;
  sizeof_t  prev_len = 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_boolean add_escape;

  for (; *rtp != (int)rt_null;) {
    sizeof_t             sect_len, rts_number;
    a_repl_text_seq_kind rts_kind = (a_repl_text_seq_kind)*(rtp++);
    /* Extract the section length or argument number. */
    get_macro_repl_text_number(rts_number, rtp);
    if (rts_kind == rt_text) {
      sect_len = rts_number;
#if MICROSOFT_EXTENSIONS_ALLOWED
      prev_text = rtp;
      prev_len = sect_len;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      rtp += sect_len;
    } else if (rts_kind == rt_paste ||
               rts_kind == rt_microsoft_magic_arg_marker) {
      /* Just a placeholder; it will not take up space in the expansion. */
      sect_len = 0;
    } else {
      a_macro_arg_ptr map;
      /* Other section kinds have an associated parameter number. */
      get_arg_value(rts_number, map);
      switch (rts_kind) {
        case rt_raw_argument:
          sect_len = map->raw_len;
          /* Don't count an LE_INERT_MACRO escape at the beginning if present,
             since it will be removed. */
          if (prev_section_is_paste &&
              map->raw_text[0] == LE_ESCAPE &&
              map->raw_text[1] == LE_INERT_MACRO) sect_len -= LE_ESCAPE_LEN;
#if MICROSOFT_EXTENSIONS_ALLOWED
          { a_const_char *post_end;
            /* coverity[var_deref_model] */
            if (ms_extensions && prev_section_is_paste &&
                is_microsoft_function_name_paste(map,
                                                 prev_text,
                                                 prev_len,
                                                 &post_end)) {
              /* This is token pasting of L##__FUNCTION__ or the like, which
                 will be replaced by __LPREFIX(__FUNCTION__).  The "L" is
                 also removed. */
              result += strlen(token_names[(int)tok_microsoft_lprefix])+2-1;
            }  /* if */
          }
          prev_text = map->raw_text;
          prev_len = map->raw_len;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          break;
        case rt_stringized_raw_argument:
        case rt_charized_raw_argument:
          /* Determine the length of the stringized version of the argument
             (or the charized version in some Microsoft macros). */
          if (map->raw_len == 0 && ms_compat && !map->is_empty_arg) {
            /* The Microsoft preprocessor suppresses all output for
               omitted (as opposed to empty) arguments.  That is, given

                 #define M(a,b) #b

               M(1) expands to nothing, while M(1,) expands to "". */
            sect_len = 0;
          } else {
            sect_len = stringized_arg(map, (char **)NULL,
                                      rts_kind == rt_charized_raw_argument);
          }  /* if */
          break;
        case rt_argument:
          sect_len = map->expanded_len;
          break;
        case rt_microsoft_maybe_raw_argument:
          /* The replacement text will contain both the raw and the
             expanded versions of the argument, to allow selection of the
             correct version once it is known how the argument will be used
             in the replacement text.  See choose_raw_or_expanded_arg for
             details.  The combined section consists of an
             LE_RAW_OR_EXPANDED_ARGUMENT lexical escape, the raw text, and
             the expanded text (or a single space if the length of the
             expanded text is zero).  This structure is optimized away,
             however, if the raw length is zero. */
          if (map->raw_len == 0) {
            /* Nothing to insert. */
            sect_len = 0;
          } else {
            /* This rts_kind is only used when pasting to a preceding "("
               or ",", so the complexities above in the processing of
               rt_raw_argument for inert macros and pasted function names
               are not needed here. */
            sect_len = map->raw_len + map->expanded_len + LE_ESCAPE_LEN;
            if (map->expanded_len == 0) {
              ++sect_len;
            }  /* if */
          }  /* if */
          break;
        default:
          unexpected_condition_str2("length_of_replacement_text:",
                                    "expansion section unknown");
      }  /* switch */
    }  /* if */
    /* When extended variadic macros are enabled, a "##" followed by an
       empty variadic argument has a special deletion effect.  The same is
       true for Microsoft variadic macros, with or without the "##". */
    if ((extended_variadic_macros_allowed || ms_compat) &&
        mdp->variadic &&
        ((a_repl_text_seq_kind)*rtp == rt_paste ||
         (a_repl_text_seq_kind)*rtp == rt_microsoft_magic_arg_marker)) {
      adjust_length_for_magic_arg(rts_kind, rtp, n_params, arg_values,
                                  &sect_len, &add_escape);
    }  /* if */
    result += sect_len;
    prev_section_is_paste = (rts_kind == rt_paste);
  }  /* for */
  return result;
}  /* length_of_replacement_text */


void adjust_deletion_counts(a_const_char *line_loc,
                            sizeof_t     deletion_len)
/*
The text beginning at line_loc is being replaced.  If the text being
replaced is in the part of the macro buffer that is subject to compaction,
adjust the running count of deleted text in the buffer and in the source
line modification whose inserted text contains line_loc to account for the
removal of deletion_len characters (less one for the ATTENTION_MARKER
character, which will remain after compaction).
*/
{
  if (ptr_in_range(line_loc, macro_buffer + num_compacted_macro_buffer_chars,
                   after_end_of_macro_buffer)) {
    /* The text being replaced is in the portion of macro_buffer that is
       subject to compaction. */
    a_source_line_modif_ptr slmp = assoc_source_line_modif(line_loc);
    if (memchr(line_loc + 1, ATTENTION_MARKER, deletion_len - 1) != NULL) {
      /* The text being replaced already contains a replacement, so this
         replacement supersedes that one.  Remove the portion of the
         count of deleted characters in macro_buffer due to the earlier
         replacement and substitute this count for the previous one in
         the source line modification.  (Note: this is a conservative
         calculation and will underestimate the number of deleted
         characters in macro_buffer if there are other replacements in
         the inserted text of this modification outside the range of the
         new replacement.  That is a safe error, however, since it only
         means that macro_buffer might be expanded when it could
         otherwise simply have been compacted.  It is thus not worth the
         time and complexity to get the count exactly right in this
         case.) */
      num_chars_deleted_in_macro_buffer -= slmp->num_deleted_chars;
      slmp->num_deleted_chars = deletion_len - 1;
    } else {
      /* Any previous deletions in this modification's inserted text are
         outside the range currently being replaced, so this deletion
         count is in addition to those. */
      slmp->num_deleted_chars += deletion_len - 1;
    }  /* if */
    num_chars_deleted_in_macro_buffer += deletion_len - 1;
  }  /* if */
}  /* adjust_deletion_counts */


/*
The following struct associates a clang __has_feature/__has_extension
string and the WG21 SG10 portable feature macro name and value with the
corresponding global flag controlling whether the feature is enabled in
the current execution of the front end.
*/
typedef struct a_feature_support {
  a_const_char
	*clang_name;	/* The string identifying the feature in the clang
			   __has_feature and __has_extension macros. */
  a_boolean
	*enabled;	/* The address of the flag variable controlling
			   whether the feature is enabled in the current
			   execution of the front end.  NULL if the feature
			   is not yet implemented. */
  a_const_char
	*macro_name;	/* The name of the WG21 SG10 macro that is to be
			   defined if the feature is supported.  NULL if
			   there is no such macro. */
  a_const_char
	*macro_value;	/* The value to which the WG21 SG10 macro is to be
			   defined if the feature is supported.  NULL if
			   there is no such macro. */
} a_feature_support;

/*
The following variables are needed because they represent features that
have feature-test macros but for which there is no corresponding global
flag to which the "enabled" member can point.  The feature_support_list
initializer below will point to these local variables, and
init_predefined_macros will give them the appropriate values before
creating the WG21 SG10 feature-test macros.
*/

static a_boolean
		access_control_sfinae;
			/* TRUE if lack of access is considered to cause a
			   deduction failure as it does in standard C++11.
			   Used to support
			   __has_feature(cxx_access_control_sfinae). */

static a_boolean
		contextual_conversions;
			/* TRUE if the rules in paper WG21 N3323 for
			   contextual implicit conversions are obeyed.
			   Used to support
			   __has_feature(cxx_contextual_conversions). */

static a_boolean
		attribute_deprecated_with_message;
			/* TRUE if the "deprecated" attribute can take a
			   string argument.  Used to support
			   __has_feature(attribute_deprecated_with_message). */

static a_boolean
		decltype_keyword_enabled;
			/* TRUE if the C++11 "decltype" keyword is enabled.
			   This is needed to support
			   __has_feature(cxx_decltype) because
			   decltype_enabled is TRUE when __decltype is
			   enabled but the "decltype" keyword is not. */

/*
The following array describes all the clang __has_feature/__has_extension
feature strings and WG21 SG10 feature-test macros (type trait helpers can
also be tested by the clang macros, but those are represented by a separate
table).  It is sorted by the clang __has_feature string so it can be used
with bsearch when the __has_feature or __has_extension macro is
encountered.  The current contents reflect the 2013-11-27 version of WG21
SG10 SD-6 and clang version 3.5.
*/
static a_feature_support feature_support_list[] = {
  { "",
    &char16_t_and_char32_t_are_keywords,
    "__cpp_unicode_characters",
    "200704" },
  { "",
    &sized_deallocation_enabled,
    "__cpp_sized_deallocation",
    "201309" },
  { "attribute_deprecated_with_message",
    &attribute_deprecated_with_message,
    NULL,
    NULL },
  { "c_atomic",
    &c11_atomic_enabled,
    NULL,
    NULL },
  { "cxx_access_control_sfinae",
    &access_control_sfinae,
    NULL,
    NULL },
  { "cxx_aggregate_nsdmi",
    &aggregate_classes_can_have_field_initializers,
    "__cpp_aggregate_nsdmi",
    "201304" },
  { "cxx_alias_templates",
    &alias_declarations_enabled,
    NULL,
    NULL },
  { "cxx_alignas",
    &alignas_enabled,
    NULL,
    NULL },
  { "cxx_attributes",
    &std_attributes_enabled,
    "__cpp_attributes",
    "200809" },
  { "cxx_auto_type",
    &auto_type_specifier_enabled,
    NULL,
    NULL },
  { "cxx_binary_literals",
    &binary_literals_allowed,
    "__cpp_binary_literals",
    "201304" },
  { "cxx_constexpr",
    &constexpr_enabled,
    NULL,		/* __cpp_constexpr must be handled specially, as a
			   single macro name takes on different values
			   depending on the level of constexpr support. */
    NULL },
  { "cxx_contextual_conversions",
    &contextual_conversions,
    NULL,
    NULL },
  { "cxx_decltype",
    &decltype_keyword_enabled,
    "__cpp_decltype",
    "200707" },
  { "cxx_decltype_auto",
    &decltype_auto_enabled,
    "__cpp_decltype_auto",
    "201304" },
  { "cxx_decltype_incomplete_return_types",
    &decltype_keyword_enabled,
    NULL,
    NULL },
  { "cxx_default_function_template_args",
    &function_template_default_args_allowed,
    NULL,
    NULL },
  { "cxx_defaulted_functions",
    &defaulted_special_members_enabled,
    NULL,
    NULL },
  { "cxx_delegating_constructors",
    &delegating_constructors_enabled,
    NULL,
    NULL },
  { "cxx_deleted_functions",
    &deleted_functions_enabled,
    NULL,
    NULL },
  { "cxx_exceptions",
    &exceptions_enabled,
    NULL,
    NULL },
  { "cxx_explicit_conversions",
    &explicit_conversion_functions_enabled,
    NULL,
    NULL },
  { "cxx_generalized_initializers",
    &list_init_enabled,
    NULL,
    NULL },
  { "cxx_generic_lambda",
    &generic_lambdas_enabled,
    "__cpp_generic_lambdas",
    "201304" },
  { "cxx_implicit_moves",
    &generate_move_operations,
    NULL,
    NULL },
  { "cxx_inheriting_constructors",
    &inheriting_constructors_enabled,
    NULL,
    NULL },
  { "cxx_init_capture",
    &init_capture_enabled,
    "__cpp_init_captures",
    "201304" },
  { "cxx_inline_namespaces",
    &inline_namespaces_enabled,
    NULL,
    NULL },
  { "cxx_lambdas",
    &lambdas_enabled,
    "__cpp_lambdas",
    "200907" },
  { "cxx_local_type_template_args",
    &local_types_as_template_args_enabled,
    NULL,
    NULL },
  { "cxx_noexcept",
    &noexcept_enabled,
    NULL,
    NULL },
  { "cxx_nonstatic_member_init",
    &field_initializers_enabled,
    NULL,
    NULL },
  { "cxx_nullptr",
    &nullptr_enabled,
    NULL,
    NULL },
  { "cxx_override_control",
    &std_override_modifiers_enabled,
    NULL,
    NULL },
  { "cxx_range_for",
    &range_based_for_enabled,
    NULL,		/* __cpp_range_based_for must be handled specially, as
			   the single macro name takes on different values. */
    NULL },
  { "cxx_raw_string_literals",
    &raw_string_literals_enabled,
    "__cpp_raw_strings",
    "200710" },
  { "cxx_reference_qualified_functions",
    &ref_qualifiers_enabled,
    NULL,
    NULL },
  { "cxx_relaxed_constexpr",
    &relaxed_constexpr_enabled,
    NULL,		/* __cpp_constexpr must be handled specially, as
			   the single macro name takes on different values
			   depending on the level of constexpr support. */
    NULL },
  { "cxx_return_type_deduction",
    &deduced_return_types_enabled,
    "__cpp_return_type_deduction",
    "201304" },
  { "cxx_rtti",
    &rtti_enabled,
    NULL,
    NULL },
  { "cxx_runtime_array",
    NULL,
    "__cpp_runtime_arrays",
    "201304" },
  { "cxx_rvalue_references",
    &rvalue_references_enabled,
    "__cpp_rvalue_references",
    "200610" },
  { "cxx_static_assert",
    &static_assert_enabled,
    NULL,		/* __cpp_static_assert must be handled specially, as
			   the single macro name takes on different values. */
    NULL },
  { "cxx_strong_enums",
    &enum_qualifiers_enabled,
    NULL,
    NULL },
  { "cxx_thread_local",
    &std_thread_local_storage_specifier_enabled,
    NULL,
    NULL },
  { "cxx_trailing_return",
    &trailing_return_types_enabled,
    NULL,
    NULL },
  { "cxx_unicode_literals",
    &uliterals_enabled,
    "__cpp_unicode_literals",
    "200710" },
  { "cxx_unrestricted_unions",
    &unrestricted_unions_enabled,
    NULL,
    NULL },
  { "cxx_user_literals",
    &user_defined_literals_enabled,
    "__cpp_user_defined_literals",
    "200809" },
  { "cxx_variable_templates",
    &variable_templates_enabled,
    "__cpp_variable_templates",
    "201304" },
  { "cxx_variadic_templates",
    &variadic_templates_enabled,
    "__cpp_variadic_templates",
    "200704" },
  { "nullability",
    &nullability_qualifiers_enabled,
    NULL,
    NULL }
};

#define NUM_FEATURES (sizeof(feature_support_list) / sizeof(a_feature_support))

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
BEGIN_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static int compare_feature_names(a_const_void_ptr id_ptr,
                                 a_const_void_ptr feature_ptr)
/*
Function used by bsearch to compare the identifier to which id_ptr points
with the clang_name of the feature to which feature_ptr points.
*/
{
  int result = strcmp((const char *)id_ptr,
                      ((a_feature_support *)feature_ptr)->clang_name);
  return result;
}  /* compare_feature_names */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
END_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

/*
The following table (sorted for use with bsearch) has one entry for each
type trait helper function for which support can be tested using the clang
__has_feature/__has_extension macros.  Although the clang documentation
says that these can be tested only by __has_extension and not by
__has_feature, current clang versions treat the two macros identically in
this regard.  This list is maintained separately from the list of features
to facilitate emulation of a potential future version of clang that does
implement the documented distinction.  This list reflects clang version
3.5.
*/
static a_const_char *clang_type_traits_helpers[] = {
  "has_nothrow_assign",
  "has_nothrow_constructor",
  "has_nothrow_copy",
  "has_trivial_assign",
  "has_trivial_constructor",
  "has_trivial_copy",
  "has_trivial_destructor",
  "has_virtual_destructor",
  "is_abstract",
  "is_base_of",
  "is_class",
  "is_convertible_to",
  "is_empty",
  "is_enum",
  "is_final",
#if MICROSOFT_EXTENSIONS_ALLOWED
  "is_interface_class",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* The front end supports __is_literal_type, as does clang, but the
     __has_feature/__has_extension macros only allow testing for
     __is_literal, which is supported by clang but not by the front end.
     Hence we omit: */
  /* "is_literal", */
  "is_pod",
  "is_polymorphic",
  "is_standard_layout",
  "is_trivial",
  "is_trivially_assignable",
  "is_trivially_constructible",
  "is_trivially_copyable",
  "is_union",
  "underlying_type"
};

#define NUM_CLANG_TYPE_TRAITS \
  (sizeof(clang_type_traits_helpers) / sizeof(a_const_char *))

/*
Size of a buffer large enough to contain any supported feature name
(including optional leading/trailing double underscores) and a terminating
null character.
*/
#define MAX_CLANG_FEATURE_NAME_LEN 64

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
BEGIN_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static int compare_type_traits_helper_names(a_const_void_ptr id_ptr,
                                            a_const_void_ptr helper_ptr)
/*
Function used by bsearch to compare the identifier to which id_ptr points
with the type traits helper name to which helper_ptr points.
*/
{
  int result = strcmp((const char *)id_ptr, *(const char **)helper_ptr);
  return result;
}  /* compare_type_traits_helper_names */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
END_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static a_const_char *clang_feature_test_id(a_macro_arg_ptr   macro_arg,
                                           a_source_position *error_pos)
/*
macro_arg is the argument of a clang feature-test macro (__has_feature,
__has_extension, __has_attribute, or __has_builtin); whether the raw_text
or expanded_text is used depends on the value of clang_version.  The
argument must be an identifier, which means that either the text is an
identifier or (if the expanded text is being used) its first character is
an ATTENTION_MARKER denoting the expansion of a macro invocation, which
might itself be the expansion of a macro invocation, etc.  If the ultimate
expansion is not a simple identifier, report an error at error_pos and
return NULL.  Otherwise, return a pointer to the identifier (after
stripping leading/trailing double-underscores, if present).  The returned
pointer may point into a local buffer, which will be overwritten by
subsequent calls.
*/
{
  a_source_line_modif_ptr slmp;
  int                     char_len;
  a_const_char            *id;
  a_const_char            *p;
  static char             buff[MAX_CLANG_FEATURE_NAME_LEN];

  if (clang_version < 30300) {
    /* Versions before 3.3 macro-expand the argument. */
    id = macro_arg->expanded_text;
  } else {
    /* Versions 3.3 and later do not macro-expand the argument. */
    id = macro_arg->raw_text;
  }  /* if */
  while (*id == ATTENTION_MARKER) {
    go_into_insertion(slmp, id);
  }  /* while */
  /* Check to ensure that the expanded string is an identifier. */
  for (p = id; *p != 0; p += char_len) {
    if (!is_identifier_char(p, &char_len, p == id)) {
      break;
    }  /* if */
  }  /* for */
  if (p == id || p[0] != LE_ESCAPE || p[1] != LE_END_OF_INSERTION) {
    /* The argument is empty or contains something other than a simple
       identifier. */
    pos_diagnostic(es_discretionary_error, ec_feature_test_macro_req_id,
                   error_pos);
    id = NULL;
  }  /* if */
  if (id != NULL && p - id > 4 && p - id < MAX_CLANG_FEATURE_NAME_LEN &&
      id[0] == '_' && id[1] == '_' && p[-1] == '_' && p[-2] == '_') {
    /* Need to strip off leading and trailing "__" sequences. */
    strcpy(buff, id + 2);
    buff[p - id - 4] = '\0';
    id = buff;
  }  /* if */
  return id;
}  /* clang_feature_test_id */


static a_boolean scan_has_include(a_boolean is_include_next)
/*
Process the clang and WG21 SG10 __has_include macro or the clang-only
__has_include_next macro, depending on the value of is_include_next.  Return
TRUE if the named header can be found, FALSE otherwise.  Issue a warning if
__has_include_next appears in the primary source file and treat it as if it
were __has_include.
*/
{
  a_boolean file_found = FALSE;

  if (is_include_next && processing_primary_source_file()) {
    /* Issue a warning and treat this as __has_include. */
    pos_warning(ec_has_include_next_in_primary_source_file, &error_position);
    is_include_next = FALSE;
  }  /* if */
  if (get_token() != tok_lparen) {
    pos_error(ec_exp_lparen, &error_position);
  } else if (!get_header_name()) {
    pos_error(ec_exp_file_name, &error_position);
  } else {
    /* The header name is now the current token.  Get the file name from
       the token. */
    a_boolean    is_system_include = (*start_of_curr_token == '<');
    a_const_char *filename = check_for_include_alias();
    if (filename == NULL) {
      /* If a Microsoft-style include_alias pragma for the file has been
         seen, check_for_include_alias will return a pointer to the
         associated file name.  Otherwise, we must extract the file name
         from the header name token.  Note that Microsoft compatibility
         requires ignoring escapes because of the use of '\' as a directory
         separator in file names; however, this can be changed if
         desired. */
      sizeof_t name_len;
      filename = extract_header_name(/*process_escapes=*/FALSE, &name_len);
    }  /* if */
    if (get_token() != tok_rparen) {
      pos_error(ec_exp_rparen, &error_position);
    } else {
      if (is_include_next && is_absolute_file_name(filename)) {
        /* An absolute file name in __has_include_next makes no sense. */
        pos_warning(ec_absolute_file_name_in_has_include_next,
                    &error_position);
      }  /* if */
      file_found = header_can_be_found(filename, is_system_include,
                                       is_include_next);
    }  /* if */
  }  /* if */
  return file_found;
}  /* scan_has_include */


static a_boolean same_macro_at_beginning(a_source_line_modif_ptr slmp,
                                         a_const_char            *str)
/*
slmp is a source line modification containing the expansion of a previous
macro invocation; str designates the expansion of a potential invocation of
the same macro appearing within the previous expansion.  Return TRUE if
both expansions begin with the name of the same macro, FALSE otherwise.
*/
{
  a_boolean    result = FALSE;
  a_symbol_ptr macro_sym = NULL;
  a_const_char *p;
  int          ch_len;
  a_const_char *id;
  sizeof_t     id_len;
  a_boolean    mbc_seen = FALSE;

  if (*slmp->inserted_text == ATTENTION_MARKER) {
    /* The previous expansion of this macro began with a macro invocation
       that has been expanded.  We'll check the name of that macro against
       the current expansion below. */
    macro_sym = nested_source_line_modif(slmp->inserted_text)->assoc_macro;
  } else {
    /* Either the previous invocation did not begin with a macro name or
       the macro invocation hasn't been expanded yet.  We need to check if
       the first token in each expansion is an identifier and, if so, if
       they both designate the same macro.  Scan through the first token of
       the previous expansion to see if it is an identifier. */
    p = slmp->inserted_text;
    while (*p != LE_ESCAPE &&
           is_identifier_char(p, &ch_len, p == slmp->inserted_text)) {
      if (ch_len > 1) {
        /* The character is a multibyte character or UCN, so the identifier
           needs to be canonicalized before looking it up. */
        mbc_seen = TRUE;
      }  /* if */
      p += ch_len;
    }  /* while */
    if ((id_len = p - slmp->inserted_text) > 0) {
      /* The previous expansion begins with an identifier.  Canonicalize
         the identifier if necessary and look it up to see if it is a macro
         name. */
      a_symbol_locator    locator;
      a_symbol_header_ptr sym_hdr;
      id = slmp->inserted_text;
      if (mbc_seen) {
        /* A multibyte character or UCN appeared in the spelling of the
           identifier.  Obtain the canonicalized spelling so it can be
           reliably looked up. */
        id = make_canonical_identifier(id, &id_len, /*force_ucn=*/FALSE);
      }  /* if */
      clear_locator(&locator, &null_source_position);
      sym_hdr = find_symbol_header(id, id_len, &locator);
      for (macro_sym = symbol_list_for_file_scope_symbols(sym_hdr);
           macro_sym != NULL && macro_sym->kind != (a_symbol_kind)sk_macro;
           macro_sym = macro_sym->next) {}
    }  /* if */
  }  /* if */
  if (macro_sym != NULL) {
    /* The previous expansion began with a macro name.  Scan through the
       first token of the current expansion to see if it is an
       identifier. */
    mbc_seen = FALSE;
    p = str;
    while (*p != LE_ESCAPE && is_identifier_char(p, &ch_len, p == str)) {
      if (ch_len > 1) {
        /* The character is a multibyte character or UCN, so the identifier
           needs to be canonicalized before comparing it against the macro
           name. */
        mbc_seen = TRUE;
      }  /* if */
      p += ch_len;
    }  /* while */
    if ((id_len = p - str) > 0) {
      /* The current expansion also begins with an identifier.
         Canonicalize the identifier if necessary and compare it against
         the name of the macro at the beginning of the previous
         expansion. */
      id = str;
      if (mbc_seen) {
        /* A multibyte character or UCN appeared in the spelling of the
           identifier.  Obtain the canonicalized spelling to compare it
           against the macro name. */
        id = make_canonical_identifier(id, &id_len, /*force_ucn=*/FALSE);
      }  /* if */
      /* See if the identifier from the current expansion matches the macro
         name from the previous expansion. */
      result = (strcmp(macro_sym->header->identifier, id) == 0);
    }  /* if */
  }  /* if */
  return result;
}  /* same_macro_at_beginning */


a_token_kind macro_invocation(a_symbol_ptr  macro_symbol,
                              a_boolean     *rescan)
/*
An identifier that is a macro has just been scanned.  Replace the macro call
with its expansion, then return either with *rescan == TRUE to indicate
that the replacement text should be re-tokenized, or with *rescan == FALSE
and return value indicating a token value for the current token (the latter
case is used when the result of an expansion is a known token; the other
associated global variables will also have been set).
*/
{
  a_macro_def_ptr mdp;
  sizeof_t	  repl_text_len = 0;
  sizeof_t        space_for_end_of_top_level_expansion_escape;
  a_boolean       repl_text_len_precomputed = FALSE;
  a_token_kind	  ctoken = tok_error;
  a_boolean	  got_proper_closing_token = FALSE;
  a_boolean	  special_repl_text = FALSE;
  a_macro_arg_ptr special_macro_arg = NULL;
  sizeof_t        sect_len, rts_number, n_params = 0;
  a_repl_text_seq_kind
		  rts_kind;
  a_boolean       any_white_space_skipped;
  a_macro_param_ptr
		  param_list,
		  pp;
  unsigned long	  paren_count;
  a_boolean       paren_found;
  a_boolean	  not_done;
  int		  param_num = 0;
  a_boolean       save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean       save_expand_macros = expand_macros;
  int             recursion_depth;
  a_source_line_modif_ptr
                  slmp,
                  slmp2;
  unsigned long   sequence_id;
  a_boolean       need_end_of_token_marker;
  a_boolean       is_macro_call = TRUE;  /* Assume. */
  a_boolean       is_inert_macro = FALSE;  /* Assume. */
  a_boolean       check_expansion_for_recursion = FALSE;
  a_boolean       pcc_mode_macro_recursion = FALSE;
  a_boolean       comma_ignored_inside_argument = ms_compat;
  a_source_position
                  start_pos;
  a_const_char    *file_name, *full_name;
  a_line_number   line_number;
  a_boolean       at_end_of_source;
  a_boolean       delete_source_from_loc_was_set_on_entry = FALSE;
  unsigned long   saved_macro_depth = macro_depth;
  a_boolean       too_many_args_diag_given = FALSE;
  a_macro_arg_ptr map = NULL;
  a_source_position
                  arg_position;
  a_macro_arg_ptr prev_end_of_macro_arg_list = end_of_macro_arg_list;
  /* The following is used by various macros.  It is therefore important to
     maintain the name "arg_values": */
			/* For parameter counts in the normal range, the
			   arg_values array provides quick look-up.  For
			   parameters beyond that, a slow linear search
			   is used. */
  a_macro_arg_ptr arg_values[ARG_VALUES_SIZE];
  a_source_line_modif_ptr
                  invocation_slmp = NULL;
  unsigned long   macro_name_depth = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_text_map_position_tracker
                  tracker;
  sizeof_t        src_offset = 0;
  sizeof_t        src_token_len = 0;
  a_simple_source_position
                  src_pos;
  sizeof_t        next_targ_offset;
  sizeof_t        first_text_map_entry;
  sizeof_t        ending_src_offset;
  sizeof_t        bytes_before_token;
  a_source_position
                  lparen_pos;
  a_macro_invocation_record_index
                  this_macro_invocation_record = NO_PARENT_MACRO_INVOCATION;
  a_macro_text_map_entry_ptr
                  tmep;
  a_boolean       macro_text_map_in_use = FALSE;
  a_const_char    *after_last_invocation_token;
  a_pointer_registration
                  after_last_invocation_token_reg;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
  a_macro_invocation_record_ptr
                  this_mirp;
  a_macro_invocation_record_index
                  parent_macro_invocation_record;
  unsigned long   macro_invocation_stack_depth;
#endif /* RECORD_MACRO_INVOCATIONS */
  a_concatenation_record_ptr
                  concat_record_head = NULL;
  a_concatenation_record_ptr
                  concat_record_tail = NULL;
  a_feature_support
                  *feature;
  a_boolean       add_escape;
  a_boolean       saved_in_macro_arg_list = in_macro_arg_list;
  unsigned long   saved_macro_name_modif_seq = macro_name_modif_seq;
  a_boolean       saved_single_param_macro = single_param_macro;

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  a_const_char    *rescan_loc;
  a_const_char    *save_delete_source_from_loc;
  char		  *src_loc, *text_loc, *repl_text, *src_loc_before_copy;
  a_pointer_registration
                  src_loc_reg, text_loc_reg, rescan_loc_reg, repl_text_reg,
                  save_delete_source_from_loc_reg, src_loc_before_copy_reg;
			/* repl_text points to the macro replacement string,
			   which is safe, but for special macros like __FILE__,
			   it will point to the raw_text of
			   special_macro_arg. */
  /* The following are safe: */
  a_const_char    *temp_ptr;
			/* Used in climbing through the source line
			   modifications that enclose the macro invocation,
			   to determine inertness or pcc mode recursion.
			   Nothing is reallocated during that process.
			   Also for copying the filename in __FILE__
			   expansion, where it points to an unmovable
			   string. */
  char            *rtp;
			/* Points to a macro replacement string, which is
			   not in the reallocated areas. */
  a_pointer_registration_ptr
                  save_registered_pointers = registered_pointers;

  register_pointer_variable(src_loc,    src_loc_reg);
  register_pointer_variable(text_loc,   text_loc_reg);
  register_pointer_variable(rescan_loc, rescan_loc_reg);
  register_pointer_variable(repl_text,  repl_text_reg);
  register_pointer_variable(save_delete_source_from_loc,
                                        save_delete_source_from_loc_reg);
  register_pointer_variable(src_loc_before_copy,
                                        src_loc_before_copy_reg);
#if FULLY_RESOLVED_MACRO_POSITIONS
  register_pointer_variable(after_last_invocation_token,
                                        after_last_invocation_token_reg);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* See standard, 3.8.3 (Macro Replacement). */
  db_enter(4, "macro_invocation");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "About to expand invocation of macro %s:\n",
                     macro_symbol->header->identifier);
  }  /* if */
#endif /* DEBUG */
#if FULLY_RESOLVED_MACRO_POSITIONS
  src_pos.seq = 0;
  src_pos.column = 0;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* One we begin rescanning a macro, don't allow a PCH to be generated
     at this point. */
  num_macro_invocations_in_process++;
#if FULLY_RESOLVED_MACRO_POSITIONS
  /* LE_END_OF_TOP_LEVEL_EXPANSION escapes are not used with fully-resolved
     macro positions. */
  space_for_end_of_top_level_expansion_escape = 0;
#else /* !FULLY_RESOLVED_MACRO_POSITIONS */
  if (within_curr_source_line(start_of_curr_token)) {
    /* This is a top-level macro invocation, so the expansion will need an
       LE_END_OF_TOP_LEVEL_EXPANSION escape. */
    space_for_end_of_top_level_expansion_escape = LE_ESCAPE_LEN;
  } else {
    /* No LE_END_OF_TOP_LEVEL_EXPANSION escape will be added. */
    space_for_end_of_top_level_expansion_escape = 0;
  }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* Push a new lexical state for tokens scanned as part of the macro
     argument list (if any). */
  push_lexical_state_stack();
  copy_source_position(pos_curr_token, start_pos);
  /* If possible, clear the macro buffer (a buffer where characters of
     expansions are put).  This is tricky in that we can't clear the
     macro buffer while there are expanded macro calls earlier in the
     current line, since those expansions refer to things in the macro
     buffer and may yet have to be written as preprocessed output. */
  /* If none of the source line modifications have inserted text
     in the macro buffer (e.g., there are none, or they're all deleted
     comments), the buffer can be cleared. */
  for (slmp = source_line_modif_list; slmp != NULL; slmp = slmp->next) {
    /* A modification that has its inserted text in the inserted_chars
       buffer in the line modification entry does not depend on
       macro_buffer.  Comments are one example of such a modification. */
    if (slmp->inserted_text != slmp->inserted_chars) {
      goto end_scan_for_macro_modifs;
    }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
    if (ptr_in_range(slmp->text_map.entries, &macro_text_map.entries[0],
                     &macro_text_map.entries[macro_text_map.num_entries])) {
      /* We can't truncate macro_text_map yet (even if macro_buffer can be
         truncated: there may be macro_text_map entries associated with a
         modification that uses only its inserted_chars). */
      macro_text_map_in_use = TRUE;
    }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* for */
  /* No source line modifications from macros. */
  next_avail_in_macro_buffer = macro_buffer;
  num_chars_deleted_in_macro_buffer = 0;
  num_compacted_macro_buffer_chars = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
  if (!macro_text_map_in_use) {
    /* There are no live references into the macro text map, so we can
       truncate it and start over, to save space. */
    macro_text_map.num_entries = 0;
    /* If this is a top-level macro expansion, there shouldn't be any
       leftover text map position trackers at this point. */
    check_assertion(active_text_map_position_trackers == NULL ||
                    macro_depth > 0);
  }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
end_scan_for_macro_modifs:;
  /* Normal case is that the macro expansion is rescanned after this routine
     exits. */
  *rescan = TRUE;
#if CHECKING
  rescan_loc = NULL;  /* To catch error cases. */
#endif /* CHECKING */
  /* When a preprocessing directive like an #if appears within a macro
     invocation, and the #if expression contains a macro invocation,
     delete_source_from_loc will be non-NULL here, and needs to be set
     again at the end of this invocation. */
  if (delete_source_from_loc != NULL) {
    delete_source_from_loc_was_set_on_entry = TRUE;
  }  /* if */
  /* Set the current token to a known state. */
  curr_token = tok_error;
  /* Get a pointer to the macro definition structure. */
  mdp = macro_symbol->variant.macro_def;
  param_list = mdp->param_list;
  repl_text = mdp->repl_text;
  /* See if this occurrence of the macro name resulted from an expansion
     of the macro.  If so, it is just treated as an identifier (i.e., the
     macro is disabled within its own expansion).  We ascertain this by
     seeing whether or not the macro name appears inside text that came
     from a macro expansion. */
  temp_ptr = start_of_curr_token;
  recursion_depth = 0;
  if (!within_curr_source_line(temp_ptr)) {
    /* This location is within a macro expansion. */
    /* Find the source modification that contains this location, and see if
       it's associated with the macro we are about to expand.  If so, the
       macro name is inert in most cases and should be left alone. */
    slmp = assoc_source_line_modif(temp_ptr);
    if (top_microsoft_slmp != NULL)  {
      /* This invocation is nested in the expansion of another macro
         invocation.  Find out how deeply nested the macro name is, which
         matters in the handling of the comma_is_from_argument flag
         below. */
      for (slmp2 = slmp; slmp2 != top_microsoft_slmp && slmp2 != NULL;
           slmp2 = parent_source_line_modif(slmp2)) {
        ++macro_name_depth;
      }  /* for */
    }  /* if */
    invocation_slmp = slmp;
    if (slmp != NULL) {
      macro_name_modif_seq = slmp->sequence_id;
    } else {
      macro_name_modif_seq = 0;
    }  /* if */
    single_param_macro = (mdp->param_list != NULL &&
                          mdp->param_list->next == NULL);
#if RECORD_MACRO_INVOCATIONS
    parent_macro_invocation_record = slmp->invocation_record;
    macro_invocation_stack_depth = slmp->invocation_depth + 1;
#endif /* RECORD_MACRO_INVOCATIONS */
    do {
      if (slmp->assoc_macro == macro_symbol) {
        /* The identifier does appear within its own expansion. */
        if (!pcc_preprocessing_mode) {
          if (ms_compat) {
            /* In some cases, the Microsoft preprocessor expands a macro
               invocation appearing in the expansion of an earlier
               invocation of the same macro.  For example:

                 #define invoke(M, arg) M arg
                 #define X(arg) invoke(Y, (arg))
                 #define Z invoke(X, (0))

               Here, the expansion of "invoke(X, (0))" contains an
               invocation of "invoke" with a different macro.  The standard
               rules would mark the second invocation of "invoke" as inert;
               the Microsoft preprocessor expands it.  It does not expand
               the second invocation if the invoked macro is the same,
               which would result in unbounded recursion; if the example is
               changed to

                 #define X(arg) invoke(X, (arg))

               the Microsoft preprocessor leaves the second invocation
               unexpanded, apparently basing the decision on whether the
               previous expansion began with a macro invocation and the
               current expansion begins with the same macro name.  The
               Microsoft preprocessor also does not expand the second
               invocation if the first appeared directly in the source
               code; that is, given

                 #define Y(arg) arg
                 invoke(X, (0));
                 Z;

               the direct invocation of "invoke" expands to "invoke(Y(0))"
               while the invocation of "Z" yields "0". */
            if (mdp->object_like) {
              /* An object-like macro name appearing in its own expansion
                 would always be an unbounded recursion. */
              is_inert_macro = TRUE;
            } else if (parent_source_line_modif(slmp) == NULL) {
              /* The name is the same as that of the top-level macro
                 invocation, so the macro is inert. */
              is_inert_macro = TRUE;
            } else if (slmp == invocation_slmp) {
              /* The macro name appears directly in its own expansion,
                 which would always lead to unbounded recursion. */
              is_inert_macro = TRUE;
            } else {
              /* We will need to check the expansion to see if the macro
                 should be treated as inert. */
              check_expansion_for_recursion = TRUE;
            }  /* if */
          } else {
            is_inert_macro = TRUE;
          }  /* if */
          break;
        } else {
          /* In pcc mode, arguments to macros are not macro-expanded before
             being put into the macro expansion, which means that a macro
             name can legitimately appear within its own expansion.  The
             identifier is not inert here, but count the depth of calls
             to check for potential recursion.  Note that this test will
             generate an error on some extreme cases that don't actually
             involve recursion, like

               #define x(a) a
               x(x(x(x(x(x(x(x(x(x(x(x  ... etc ... (1))))))))))))
          */
          if (++recursion_depth >= MAX_PCC_RECURSIVE_MACRO_DEPTH) {
            pos_error(ec_macro_recursion, &error_position);
            /* Set a flag for later special processing. */
            pcc_mode_macro_recursion = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Repeat this test for each macro expansion that contains the
         current macro expansion.  We work outward to the outermost macro
         expansion that contains the location we started with, and stop
         when we reach the primary source line. */
    } while ((slmp = parent_source_line_modif(slmp)) != NULL);
#if RECORD_MACRO_INVOCATIONS
  } else {
    parent_macro_invocation_record = NO_PARENT_MACRO_INVOCATION;
    macro_invocation_stack_depth = 1;
#endif /* RECORD_MACRO_INVOCATIONS */
  }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
  if (invocation_slmp != NULL) {
    /* The macro name comes from a source line modification.  Remember
       the offset of the token and its length to allow the existing map
       entry to be cloned later. */
    src_offset = start_of_curr_token - invocation_slmp->inserted_text;
    src_token_len = locator_for_curr_id.symbol_header->identifier_length;
  } else {
    /* The macro name comes from the current source line.  Remember the
       position of the token to allow a map entry for it to be added
       later. */
    src_pos.seq = pos_curr_token.seq;
    src_pos.column = pos_curr_token.column;
  }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  /* Set a flag to cause deletion of the text of the macro invocation.
     This is a global flag so that if we go to a new line during skipping
     of white space, the appropriate part of the current line will be
     deleted (skip_white_space checks the flag). */
  delete_source_from_loc = start_of_curr_token;
  if (is_inert_macro) {
    /* A macro name appearing within its own expansion.  Do not scan
       arguments, and do not expand the macro.  Replace it with an
       LE_INERT_MACRO escape sequence followed by the identifier string. */
make_inert_macro:
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Macro is inert, left as identifier.\n");
    }  /* if */
#endif /* DEBUG */
    is_macro_call = FALSE;
    got_proper_closing_token = TRUE;
    /* Use a special a_macro_arg entry as the expansion text buffer.
       Put it on the list of macro args so it can be found if the
       buffers are resized. */
    special_macro_arg = alloc_macro_arg();
    add_to_macro_arg_list(special_macro_arg);
    special_repl_text = TRUE;
    repl_text = special_macro_arg->raw_text;
    len_of_curr_token = macro_symbol->header->identifier_length;
    repl_text_len = len_of_curr_token + LE_ESCAPE_LEN;
    repl_text_len_precomputed = TRUE;
    ensure_arg_raw_text_space(repl_text_len, special_macro_arg);
    text_loc = repl_text;
    *text_loc++ = LE_ESCAPE;
    *text_loc++ = LE_INERT_MACRO;
    (void)memcpy(text_loc,
                 macro_symbol->header->identifier,
                 size_t_arg(len_of_curr_token));
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Map the inert macro name in the raw text back to its original source
       position (which will have been preserved in the source line modification
       containing the token) and add a terminating entry for the offset after
       the name. */
    clone_macro_text_map_entries(&invocation_slmp->text_map,
                                 src_offset,
                                 len_of_curr_token,
                                 &special_macro_arg->raw_text_map,
                                 LE_ESCAPE_LEN, NO_PARENT_MACRO_INVOCATION);
    add_entry_to_macro_text_map(&special_macro_arg->raw_text_map,
                                repl_text_len, (a_seq_number)0, SP_COL_UNKNOWN,
                                NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  } else if (mdp->object_like) {
    /* "Object-like" macro (has no arguments).  Or, a special predefined
       macro, which might have arguments. */
    macro_depth++;
    got_proper_closing_token = TRUE;
    /* A NULL replacement text pointer indicates one of the special predefined
       macros that must be handled by code. */
    if (repl_text == NULL) {
      /* Special case: see which one (defined, __LINE__, __FILE__, etc). */
      /* Use a special a_macro_arg entry as the expansion text buffer.
         Put it on the list of macro args so it can be found if the
         buffers are resized. */
      special_macro_arg = alloc_macro_arg();
      add_to_macro_arg_list(special_macro_arg);
      special_repl_text = TRUE;
      repl_text = special_macro_arg->raw_text;
      if (macro_symbol == line_macro_symbol) {
        /* __LINE__.  Make and return the string for a decimal integer
           indicating the current line number. */
        /* Convert the sequence number to a line number. */
        conv_seq_to_file_and_line(curr_seq_number, &file_name, &full_name,
                                  &line_number, &at_end_of_source);
        /* We assume we don't need to call ensure_arg_raw_text_space. */
        (void)sprintf(repl_text, "%lu", (unsigned long)line_number);
      } else if (macro_symbol == file_macro_symbol ||
                 macro_symbol == base_file_macro_symbol) {
        /* __FILE__.  Make and return a string for a string literal 
          indicating the current file name. */
        /* Also GNU __BASE_FILE__. */
        if (macro_symbol == base_file_macro_symbol) {
          /* __BASE_FILE__.  Use the primary source file name. */
          file_name = curr_translation_unit->source_file->file_name;
        } else {
          /* __FILE__.  Use the current source file name. */
          /* Convert the sequence number to a file name. */
          conv_seq_to_file_and_line(start_pos.seq, &file_name, &full_name,
                                    &line_number, &at_end_of_source);
        }  /* if */
#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
        if (curr_file_unicode_source_kind == usk_none) {
          /* If the current file is not Unicode, convert the file name to
             native multibyte characters. */
          file_name = format_file_name(file_name);
        }  /* if */
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
        /* Determine the file name length.  Count each backslash as
           two characters because it must be escaped in the string. */
        repl_text_len = 0;
        for (temp_ptr = file_name; *temp_ptr != '\0'; temp_ptr++) {
          char ch = *temp_ptr;
          if (isprint((unsigned char)ch)) {
            if (!exp_header_name && (ch == '"' || ch == '\\')) repl_text_len++;
            repl_text_len++;
          } else if (ch == '\n') {
            /* Newline is put out as \n. */
            repl_text_len += 2;
          } else {
            /* Unprintable characters are put out as \ooo. */
            repl_text_len += 4;
          }  /* if */
        }  /* for */
        /* Allocate space for the filename string. */
        /* "+3" in the following is for the two quotes and the null. */
        ensure_arg_raw_text_space(repl_text_len+3, special_macro_arg);
        /* Copy the filename, expanding each backslash to two backslashes. */
        text_loc = repl_text;
        *text_loc++ = '"';  /* Opening quote. */
        for (temp_ptr = file_name; *temp_ptr != '\0'; temp_ptr++) {
          char ch = *temp_ptr;
          if (isprint((unsigned char)ch)) {
            if (!exp_header_name && (ch == '"' || ch == '\\')) {
              *text_loc++ = '\\';
            }  /* if */
            *text_loc++ = ch;
          } else if (ch == '\n') {
            /* Newline is put out as \n. */
            *text_loc++ = '\\';
            *text_loc++ = 'n';
          } else {
            /* Unprintable characters are put out as \ooo. */
            sprintf(text_loc, "\\%03o",
                        (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
            text_loc += 4;
          }  /* if */
        }  /* for */
        *text_loc++ = '"';  /* Closing quote. */
        *text_loc = '\0';   /* Final null. */
        /* repl_text_len gets recomputed below. */
      } else if (macro_symbol == defined_macro_symbol) {
        /* "defined".  This is not, strictly speaking, a macro -- it's
           an operator allowed only in #if expressions.  However, it is
           most easily handled as a pseudo-macro. */
        is_macro_call = FALSE;
        ctoken = scan_defined_operator();
        *rescan = FALSE;
        /* Whether we end up with the original identifier or a constant,
           we have a token to return and do not need to rescan. */
        /* If the substitution was not done, go return the current token. */
        if (ctoken != tok_int_constant) goto return_point;
        /* Otherwise, replace the defined operator and its operand with
           an integer constant. */
        /* We assume we don't need to call ensure_arg_raw_text_space. */
        (void)strcpy(repl_text,
                     decimal_str_for_integer_constant(&const_for_curr_token));
        (void)strcat(repl_text, "L");
      } else if (macro_symbol == stdc_macro_symbol) {
        /* This macro symbol is only non-NULL when stdc_zero_in_system_headers
           is TRUE.  Use a value of 0 if we are in a system header or if
           __STDC__ should be 0 even outside of a system header, or 1
           otherwise. */
        (void)strcpy(repl_text,
                     curr_ise->from_system_include_dir || !stdc_value ? "0"
                                                                      : "1");
      } else if (macro_symbol == Pragma_macro_symbol) {
        /* The C99-style _Pragma operator.  This is invoked as
               _Pragma("pragma-name pragma-operands(opt)")
           Call a routine to translate the string into a pending pragma
           entry. */
        if (macro_depth > 1) {
          /* Don't recognize the pragma operator when scanning nested macro
             invocations. */
          ctoken = tok_identifier;
          *rescan = FALSE;
        } else {
          a_pragma_kind_description_ptr	pkdp;
          is_macro_call = FALSE;
          delete_source_from_loc = NULL;
          scan_pragma_operator(&got_proper_closing_token, &pkdp); 
          rescan_loc = curr_char_loc;
        }  /* if */
        goto return_point;
      } else if (macro_symbol == microsoft_pragma_macro_symbol) {
        /* The Microsoft __pragma operator.  This is invoked as
               __pragma(pragma-name pragma-operands(opt))
           Call a routine to translate the string into a pending pragma
           entry. */
        if (macro_depth > 1) {
          /* Don't recognize the pragma operator when scanning nested macro
             invocations. */
          ctoken = tok_identifier;
          *rescan = FALSE;
        } else {
          a_pragma_kind_description_ptr	pkdp;
          is_macro_call = FALSE;
          delete_source_from_loc = NULL;
          scan_microsoft_pragma_operator(&got_proper_closing_token, &pkdp);
          rescan_loc = curr_char_loc;
        }  /* if */
        goto return_point;
      } else if (macro_symbol == counter_macro_symbol) {
        /* The Microsoft/GNU __COUNTER__ macro.  This returns a different
           integer value each time it is used, starting with zero. */
        /* We assume we don't need to call ensure_arg_raw_text_space. */
        (void)unsigned_to_string_buf(
                      (a_host_large_unsigned)counter_macro_number, repl_text);
        counter_macro_number += 1;
      } else if (macro_symbol == timestamp_macro_symbol) {
        /* The Microsoft/GNU __TIMESTAMP__ macro.  This returns the
           modification time of the current input file. */
        a_const_char *time_str;
        size_t	     length;
        time_str = get_file_modification_time_string(curr_ise->full_name,
                                                     /*strip_newline=*/TRUE);
        /* The time string should only be NULL if the file was removed
           after it was opened, or if the input is coming from standard
           input. */
        if (time_str == NULL) time_str = "<unknown>";
        check_assertion(time_str != NULL);
        length = strlen(time_str);
        /* "+3" in the following is for the two quotes and the null. */
        ensure_arg_raw_text_space(length+3, special_macro_arg);
        sprintf(repl_text, "\"%s\"", time_str);
      } else if (macro_symbol == has_include_symbol ||
                 macro_symbol == clang_has_include_next_symbol) {
        /* The clang and WG21 SG10 __has_include macro or the clang-only
           has_include_next macro.  Has the value 1 if the named header
           file would be found by #include or #include_next, respectively,
           and 0 otherwise. */
        a_boolean file_found;
        ++macro_depth;
        save_delete_source_from_loc = delete_source_from_loc;
        delete_source_from_loc = NULL;
        file_found =
               scan_has_include(macro_symbol == clang_has_include_next_symbol);
        delete_source_from_loc = save_delete_source_from_loc;
        --macro_depth;
        strcpy(repl_text, file_found ? "1" : "0");
      } else {
        unexpected_condition_str(
                         "macro_invocation: unknown special predefined macro");
      }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
      if (invocation_slmp != NULL) {
        /* The macro name was in a source line modification.  Clone the map
           entry for the macro name (to preserve its original source
           position). */
        clone_macro_text_map_entries(&invocation_slmp->text_map,
                                     src_offset,
                                     src_token_len,
                                     &special_macro_arg->raw_text_map,
                                     /*starting_targ_offset=*/0,
                                     NO_PARENT_MACRO_INVOCATION);
      } else {
        /* The macro name was in the current source line.  Add a map entry
           mapping the expansion back to the original position. */
        add_entry_to_macro_text_map(&special_macro_arg->raw_text_map,
                                    /*start_of_region=*/0, src_pos.seq,
                                    src_pos.column,
                                    NO_PARENT_MACRO_INVOCATION);
      }  /* if */
      add_entry_to_macro_text_map(&special_macro_arg->raw_text_map,
                                  (sizeof_t)strlen(repl_text), (a_seq_number)0,
                                  SP_COL_UNKNOWN, NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    }  /* if */
#if RECORD_MACRO_INVOCATIONS
    if (is_macro_call) {
      /* Only register real macro invocations. */
      this_macro_invocation_record =
                      register_macro_invocation(parent_macro_invocation_record,
                                                macro_invocation_stack_depth,
                                                mdp, &start_pos, &this_mirp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      this_mirp->end.seq = pos_curr_token.orig_seq;
      this_mirp->end.column = pos_curr_token.orig_column +
                                     (end_of_curr_token - start_of_curr_token);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
#endif /* RECORD_MACRO_INVOCATIONS */
  } else {
    /* Function-like macro.  Look for a "(".  If the left parenthesis is
       not found, return the original identifier as simply an identifier. */
    check_for_following_parenthesis(&paren_found, /*allow_id=*/FALSE);
    if (!paren_found) {
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, 
            "Potential macro not followed by \"(\", left as identifier.\n");
      }  /* if */
#endif /* DEBUG */
      if (check_expansion_for_recursion) {
        /* This macro name appeared in its own expansion: treat it as an
           inert macro, even though we wouldn't have expanded it here. */
        is_inert_macro = TRUE;
        delete_source_from_loc = start_of_curr_token;
        curr_char_loc =
                 start_of_curr_token + macro_symbol->header->identifier_length;
        goto make_inert_macro;
      }  /* if */
      ctoken = tok_identifier;
      *rescan = FALSE;
      is_macro_call = FALSE;
      goto return_point;
    } else {
      /* "(" was found, so this is a macro call.  Scan the argument values
         and save them in the parameter list blocks (in both raw and
         macro-expanded form). */
      macro_depth++;
      in_macro_arg_list = TRUE;
      fetch_pp_tokens = TRUE;
      expand_macros = FALSE;
#if RECORD_MACRO_INVOCATIONS
      /* Register this macro invocation.  We need to do this here so that we
         can put the index of the macro invocation record into the source
         line modifications used for expanding the macro arguments, so that
         macro invocations in the argument list will list this invocation as
         the parent. */
      this_macro_invocation_record =
                      register_macro_invocation(parent_macro_invocation_record,
                                                macro_invocation_stack_depth,
                                                mdp, &start_pos, &this_mirp);
#endif /* RECORD_MACRO_INVOCATIONS */
      /* Get the "(" as a token, and delete its characters. */
      (void)arg_get_token(&any_white_space_skipped);
      add_stop_token(tok_rparen);
#if FULLY_RESOLVED_MACRO_POSITIONS
      lparen_pos = pos_curr_token;
      /* Save a pointer to after the "(", in case we need it for the error
         position in a message about a missing ")". */
      after_last_invocation_token = start_of_curr_token + len_of_curr_token;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
      /* Get another token to prime the loop. */
      if (param_list != NULL) {
        /* Set up for a potential call of choose_raw_or_expanded_arg. */
        use_raw_version_of_arg = param_list->is_operand_of_paste;
      }  /* if */
      (void)arg_get_token(&any_white_space_skipped);
      use_raw_version_of_arg = FALSE;
      pp = param_list;
      /* Check for empty argument list. */
      if (curr_token != tok_rparen || pp != NULL) {
        add_stop_token(tok_comma);
        do {
          sizeof_t                token_text_len;
          a_source_line_modif_ptr locked_slmp;
          a_boolean               saved_slm_lock = FALSE;
          a_boolean               need_expanded_form;
          a_boolean               scanning_text_not_in_primary_source_line;
          /* Scan one argument value.  The argument value ends with a
             comma or right parenthesis that is not inside parentheses.
             Note that expand_macros is FALSE, and therefore the argument
             is being scanned in raw form (important, so we are not fooled
             by macros expanding into "," or ")").  Note also that the
             characters of each token (and any white space preceding it)
             are deleted as the token is scanned.  Also, white space at
             the beginning and end of the argument is ignored. */
          map = alloc_macro_arg();
          add_to_arg_values(map);
          arg_position = pos_curr_token;
do_argument_again:
          if (pp == NULL) {
            /* Too many arguments. */
            if (microsoft_bugs &&
                (curr_token == tok_comma || curr_token == tok_rparen)) {
              /* In Microsoft mode, it's not an extra argument if it's
                 empty (see test for empty argument below). */
            } else {
              if (!too_many_args_diag_given) {
                an_error_severity severity;
                a_source_position *pos;
                if (pcc_preprocessing_mode || SVR4_C_mode || ms_compat) {
                  /* In pcc, SVR4 C, and Microsoft mode, this is only a
                     warning. */
                  severity = es_warning;
                } else {
                  severity = es_discretionary_error;
                }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
                if (macro_positions_in_diagnostics) {
                  /* Use the position of the macro name for the diagnostic:
                     the text of the argument might have come from
                     somewhere unrelated to this invocation, and pointing
                     there could result in confusing output. */
                  pos = &start_pos;
                } else {
                  /* Indicate the position of the first excess argument. */
                  pos = &pos_curr_token;
                }  /* if */
#else /* !FULLY_RESOLVED_MACRO_POSITIONS */
                pos = &pos_curr_token;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
                pos_sy_diagnostic(severity, ec_too_many_macro_args, pos,
                                  macro_symbol);
                too_many_args_diag_given = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          paren_count = 0;
          /* Ignore initial white space. */
          any_white_space_skipped = FALSE;
          need_end_of_token_marker = FALSE;
          scanning_text_not_in_primary_source_line = FALSE;
          /* See whether we need the macro-expanded form of the argument.
             We need it only if it is used in the macro definition.
             In pcc mode, the expanded form is never needed. */
          need_expanded_form = (!pcc_preprocessing_mode &&
                                pp != NULL &&
                                pp->need_expanded_form);
          map->offset_in_raw_text_of_primary_source_line_text = 0;
          if (need_expanded_form &&
              !within_curr_source_line(start_of_curr_token)) {
            /* The macro argument starts off in an insertion, i.e., it
               was generated by a macro expansion.  Remember the position
               of the original source so we can scan from there when we
               rescan to get the macro-expanded form of the argument.
               That gives us the original context we need to test for
               macro inertness. */
            map->initial_raw_text_not_in_primary_source_line =
                                     (char *)arg_get_token_start_of_curr_token;
            scanning_text_not_in_primary_source_line = TRUE;
          }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
          /* Initialize the position tracker for the raw text buffer.  Use
             the current macro invocation record to stamp that context into
             the map entries. */
          init_text_map_position_tracker(&tracker, &map->raw_text_map,
                                         this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
          /* A macro argument ends when we encounter:
               (a) the end of the current line or the current translation
                   unit, or
               (b) outside parentheses introduced in the argument (i.e., when
                   paren_count == 0):
                     (b1) a right parenthesis, or
                     (b2) a comma when we are not in the last argument
                          (pp->next == NULL) of a variadic macro.
             In addition, in Microsoft mode commas occurring inside a
             substituted macro argument do not terminate a macro argument.
          */
          while (!(curr_token == tok_newline ||
                   curr_token == tok_end_of_source ||
                   (paren_count == 0 &&
                    (curr_token == tok_rparen ||
                     (curr_token == tok_comma &&
                      !comma_is_from_argument &&
                      !(pp != NULL && pp->next == NULL && mdp->variadic)))))) {
            /* Track nesting of parentheses. */
            if (curr_token == tok_lparen) {
              paren_count++;
            } else if (curr_token == tok_rparen) {
              if (paren_count > 0) paren_count--;
            } else if (comma_ignored_inside_argument &&
                       curr_token == tok_comma && paren_count == 0) {
              /* Mark this comma as not delimiting arguments. */
              ensure_arg_raw_text_space(LE_ESCAPE_LEN, map);
              *(map->raw_text+map->raw_len++) = LE_ESCAPE;
              *(map->raw_text+map->raw_len++) = LE_COMMA_FROM_ARGUMENT;
            }  /* if */
            if (scanning_text_not_in_primary_source_line) {
              /* This token was fetched from a source line modification.
                 Mark that modification so it will be saved if we advance
                 into a new source line. */
              slmp = assoc_source_line_modif(start_of_curr_token);
              slmp->contains_saved_macro_argument_text = TRUE;
            }  /* if */
            /* Put the text of the token into the argument raw_text array. */
            token_text_len =
                          length_for_curr_token_save(need_end_of_token_marker,
                                                     any_white_space_skipped);
#if FULLY_RESOLVED_MACRO_POSITIONS
            /* Map the buffer offset (allowing for added spaces and escapes,
               which are reflected in the difference between len_of_curr_token
               and token_text_len and will occur before the token text itself)
               to the original position of the token. */
            next_targ_offset = map->raw_len +
                                            token_text_len - len_of_curr_token;
            add_token_to_macro_text_map(&tracker, next_targ_offset);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            ensure_arg_raw_text_space(token_text_len, map);
            add_curr_token_text_to_buffer(need_end_of_token_marker,
                                          any_white_space_skipped,
                                          map->raw_text+map->raw_len);
            map->raw_len += token_text_len;
            if (pcc_preprocessing_mode ||
                (ms_compat && curr_token == tok_rparen)) {
              /* Suppress end-of-token markers in pcc mode.  Also, in
                 Microsoft mode, suppress the token separator following a
                 right parenthesis, to allow concatenation of the final
                 token of a macro expansion with the following token. */
              need_end_of_token_marker = FALSE;
            } else if (microsoft_bugs &&
                       (start_of_curr_token[0] == '+' ||
                        start_of_curr_token[0] == '-') &&
                       isdigit((unsigned char)start_of_curr_token[1])) {
              /* Suppress end-of-token markers between a sign character and
                 a digit in Microsoft bugs mode.  This emulates the
                 behavior of the Microsoft preprocessor that enables
                 something like "1e" and "-1" to be concatenated into the
                 single token "1e-1", as opposed to the Standard-conforming
                 behavior that produces the erroneous token "1e-" and the
                 separate token "1". */
              need_end_of_token_marker = FALSE;
            } else {
              need_end_of_token_marker = TRUE;
            }  /* if */
            /* Generate a remark on an invalid token. */
            if (curr_token == tok_error) {
              pos_remark(err_code_for_error_token, &error_position);
            }  /* if */
            (void)arg_get_token(&any_white_space_skipped);
            if (comma_is_from_argument) {
              if (paren_count > 0) {
                /* The Microsoft preprocessor ignores whether a comma
                   originated in a macro argument in invocations appearing
                   within the argument of another macro.  (We take the fact
                   that this comma is nested within parentheses as an
                   indication that it is in a macro argument.  If it's just
                   parenthesized text and not a macro invocation, it doesn't
                   matter because commas nested within parentheses don't
                   delimit macro arguments in any case.)  Overwrite the
                   LE_COMMA_FROM_ARGUMENT escape with LE_END_OF_TOKEN (an
                   innocuous substitution, since all commas start new
                   tokens). */
                char *cp = (char *)start_of_curr_token;
                /* The LE_COMMA_FROM_ARGUMENT escape might be followed by an
                   LE_END_OF_TOKEN escape and/or a single space character,
                   in that order, as per add_curr_token_text_to_buffer. */
                if (cp[-1] == ' ') {
                  --cp;
                }  /* if */
                if (cp[-LE_ESCAPE_LEN  ] == LE_ESCAPE &&
                    cp[-LE_ESCAPE_LEN+1] == LE_END_OF_TOKEN) {
                  cp -= LE_ESCAPE_LEN;
                }  /* if */
                check_assertion(
                               cp[-LE_ESCAPE_LEN  ] == LE_ESCAPE &&
                               cp[-LE_ESCAPE_LEN+1] == LE_COMMA_FROM_ARGUMENT);
                cp[-LE_ESCAPE_LEN+1] = LE_END_OF_TOKEN;
              } else if (top_microsoft_slmp != NULL &&
                         macro_name_depth > 2) {
                /* The Microsoft preprocessor does not give special meaning
                   to a comma from an argument if it's used as an argument
                   in a macro invocation in which the macro name is the
                   result of a deeply-nested macro expansion.  For example,
                   given something like

                     #define M(...) X(__VA_ARGS__)(__VA_ARGS__)
                     M(x,y)

                   where X is a macro whose ultimate expansion is the name
                   of a macro Y, whether Y is invoked with one or two
                   arguments depends on how deeply nested the name Y is in
                   the expansion of the invocation of X relative to the
                   nesting depth of the comma. */
                unsigned long comma_depth = 0;
                for (slmp2 = assoc_source_line_modif(start_of_curr_token);
                     slmp2 != top_microsoft_slmp && slmp2 != NULL;
                     slmp2 = parent_source_line_modif(slmp2)) {
                  ++comma_depth;
                }  /* for */
                if (macro_name_depth > comma_depth + 2) {
                  comma_is_from_argument = FALSE;
                }  /* if */
              }  /* if */
            }  /* if */
            if (scanning_text_not_in_primary_source_line &&
                within_curr_source_line(start_of_curr_token)) {
              /* This argument started out in a macro expansion and now
                 we're back in the primary source line.  Remember where
                 this happens. */
              scanning_text_not_in_primary_source_line = FALSE;
              map->final_modif_for_initial_text =
                      last_source_line_modif_exited_while_skipping_white_space;
              /* Remember also where to pick up in the saved raw text when
                 the rescan continues into the primary source line text. */
              map->offset_in_raw_text_of_primary_source_line_text=map->raw_len;
            }  /* if */
          }  /* while */
          if (scanning_text_not_in_primary_source_line) {
            /* We never got back to the primary source line.  Store the
               offset of the final end-of-insertion as the restart point. */
            map->offset_in_raw_text_of_primary_source_line_text=map->raw_len;
          }  /* if */
          /* Place terminating LE_END_OF_INSERTION lexical escape. */
          ensure_arg_raw_text_space(LE_ESCAPE_LEN, map);
          map->raw_text[map->raw_len]   = LE_ESCAPE;
          map->raw_text[map->raw_len+1] = LE_END_OF_INSERTION;
#if FULLY_RESOLVED_MACRO_POSITIONS
          terminate_macro_text_map(&tracker, map->raw_len+LE_ESCAPE_LEN);
          if (curr_token == tok_comma) {
            map->comma_pos = pos_curr_token;
          }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if DEBUG
          if (debug_level >= 4) {
            fprintf(f_debug, "raw argument %s: \"",
                             pp != NULL ? pp->name : "<extra>");
            print_markered_text(map->raw_text, map->raw_len, FALSE);
            fputs("\"\n", f_debug);
          }  /* if */
#endif /* DEBUG */
          /* Generate a warning on an empty macro argument, since that
             is "undefined" behavior according to the standard.  Do not
             generate the diagnostic if the argument was ended because of
             the end of source or of a preprocessing directive.  This
             is a warning instead of a strict ANSI diagnostic because this
             is "undefined" and not illegal.  In C99 and C++11, empty macro
             arguments are valid. */
          if (map->raw_len == 0 &&
              (curr_token != tok_end_of_source && curr_token != tok_newline)) {
            if (strict_ansi_mode && !c99_mode && !cpp11_mode) {
              pos_warning(ec_empty_macro_argument, &error_position);
            }  /* if */
            /* Strangely, the Microsoft compiler ignores empty macro arguments.
               This has been verified with MSVC++ 4.2, 5.0. and 7.0.
               Fixed in 7.1 */
            if (microsoft_bugs && microsoft_version < 1310 &&
                curr_token == tok_comma && !comma_is_from_argument) {
              (void)arg_get_token(&any_white_space_skipped);
              goto do_argument_again;
            }  /* if */
            /* A zero-length argument is empty (as opposed to omitted) if
               it's the first argument (i.e., pp == param_list) and is
               followed by a comma, or if it is not the first argument
               (and thus, by definition, was preceded by a comma).  That
               is, M1() has an omitted first argument, while M2(,) has
               empty first and second arguments. */
            map->is_empty_arg = (pp != param_list || curr_token == tok_comma);
          }  /* if */
          if (curr_token == tok_end_of_source || curr_token == tok_newline) {
            /* The macro was not correctly terminated -- we won't need the
               expanded form.  (Skipping the expansion in this case is
               important because otherwise we would insert the source line
               modification containing the raw text at the location of the
               current token, overwriting the token's LE_ESCAPE character
               with an ATTENTION_MARKER and possibly causing an error in
               the compaction scan if the macro buffer overflows before the
               LE_ESCAPE is restored.) */
            need_expanded_form = FALSE;
          }  /* if */
          /* The raw form of the argument has been scanned.  Now scan it
             again with macro expansion. */
          if (!need_expanded_form) goto end_arg_expansion;
          /* We do the rescan by reinserting the raw argument text
             temporarily and rescanning it with macro expansion on
             (but still fetching pp-tokens).  Note that the standard
             requires that a macro argument be macro-expanded in
             isolation, without any of the tokens following it.  We
             implement that by setting the is_isolated_text flag
             so that the scan will stop at the end of the insertion. */
          /* It's not possible to rescan the raw argument entirely from
             the original source because we may have advanced to a new source
             line while scanning the argument.  However, parts that came
             from macro expansions are scanned from the original so
             that we get the proper context for macro inertness checking
             (they're in macro_buffer, so they have not disappeared).
             After those parts, we drop into the characters in the
             raw_text buffer to scan the parts that came from the
             primary source line. */
          slmp = add_source_line_modif(start_of_curr_token, 1,
                                       map->raw_text,
                                       map->raw_text+map->raw_len);
          slmp->is_isolated_text = TRUE;
          slmp->source_position = start_pos;
#if FULLY_RESOLVED_MACRO_POSITIONS
          /* Use the raw_text_map as the source line modification's
             text_map. */
          slmp->text_map.num_entries = map->raw_text_map.num_entries;
          slmp->text_map.entries = map->raw_text_map.entries;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
          slmp->invocation_record = this_macro_invocation_record;
          slmp->invocation_depth = macro_invocation_stack_depth;
#endif /* RECORD_MACRO_INVOCATIONS */
          if (map->initial_raw_text_not_in_primary_source_line != NULL) {
            /* Start in the macro-expanded part of the original text of the
               raw argument. */
            curr_char_loc = map->initial_raw_text_not_in_primary_source_line;
            if (map->final_modif_for_initial_text != NULL) {
              /* Force the scan to stop at the point where the scan would
                 return to the primary source line. */
              map->final_modif_for_initial_text->is_isolated_text = TRUE;
            }  /* if */
            scanning_text_not_in_primary_source_line = TRUE;
          } else {
            /* The raw argument came entirely from the primary source line,
               so rescan it entirely out of the raw_text buffer. */
            curr_char_loc = map->raw_text;
            scanning_text_not_in_primary_source_line = FALSE;
          }  /* if */
          expand_macros = TRUE;
          /* slmp->next will be used as a list delimiter.  If non-NULL,
             make sure it does not get moved. */
          locked_slmp = slmp->next;
          if (locked_slmp != NULL) {
            saved_slm_lock = locked_slmp->locked;
            locked_slmp->locked = TRUE;
          }  /* if */
          /* Suspend deletion of the characters of the macro invocation.  We
             don't need to delete the characters of the raw argument during
             rescan, and we need to save the current delete position for
             later use. */
          save_delete_source_from_loc = delete_source_from_loc;
          delete_source_from_loc = NULL;
          (void)arg_get_token(&any_white_space_skipped);
          /* Ignore initial white space. */
          any_white_space_skipped = FALSE;  /* Should be FALSE already. */
          need_end_of_token_marker = FALSE;
          paren_count = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
          /* Reinitialize the tracker for the scan through the raw text.  This
             time we use NO_PARENT_MACRO_INVOCATION as the context to preserve
             either the context set while scanning the raw text or the context
             set by nested macro invocations. */
          init_text_map_position_tracker(&tracker, &map->exp_text_map,
                                         NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
scan_expanded_tokens:
          /* Note that the tok_end_of_source here is returned because
             of the is_isolated_text flag; it's not actually the end of
             source. */
          while (curr_token != tok_end_of_source) {
            a_boolean token_ends_macro_expansion = 
                              (!within_curr_source_line(start_of_curr_token) &&
                               end_of_curr_token[1] == LE_ESCAPE &&
                               end_of_curr_token[2] == LE_END_OF_INSERTION);
            if (comma_ignored_inside_argument) {
              /* In Microsoft mode, top-level (i.e., not nested inside
                 parentheses) commas that originate in the expanded text of
                 a macro in a macro argument are marked so that they do not
                 delimit macro arguments when the expanded text is rescanned.
                 For example, given

                       #define Q(x) M(x)
                       #define A 1,2
                       Q(A)

                 the macro M will be invoked with one argument, not two.  To
                 implement this, we add a marker to the expanded text for
                 every comma that is to be ignored. */
              if (curr_token == tok_lparen) {
                ++paren_count;
              } else if (curr_token == tok_rparen && paren_count > 0) {
                --paren_count;
              } else if (curr_token == tok_comma && paren_count == 0) {
                ensure_arg_expanded_text_space(LE_ESCAPE_LEN, map);
                *(map->expanded_text+map->expanded_len++) = LE_ESCAPE;
                *(map->expanded_text+map->expanded_len++) =
                                                        LE_COMMA_FROM_ARGUMENT;
              }  /* if */
            }  /* if */
            /* Put the text of the token into the argument expanded_text
               array. */
            token_text_len =
                          length_for_curr_token_save(need_end_of_token_marker,
                                                     any_white_space_skipped);
            ensure_arg_expanded_text_space(token_text_len, map);
#if FULLY_RESOLVED_MACRO_POSITIONS
            /* Map the buffer offset (allowing for added spaces and escapes)
               to the original position of the token. */
            next_targ_offset = map->expanded_len +
                                            token_text_len - len_of_curr_token;
            add_token_to_macro_text_map(&tracker, next_targ_offset);
            after_last_invocation_token =
                                       start_of_curr_token + len_of_curr_token;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            add_curr_token_text_to_buffer(need_end_of_token_marker,
                                          any_white_space_skipped,
                                         map->expanded_text+map->expanded_len);
            map->expanded_len += token_text_len;
            if (microsoft_bugs &&
                (start_of_curr_token[0] == '+' ||
                 start_of_curr_token[0] == '-') &&
                isdigit((unsigned char)start_of_curr_token[1])) {
              /* Suppress end-of-token markers between a sign character and
                 a digit in Microsoft bugs mode.  This emulates the
                 behavior of the Microsoft preprocessor that enables
                 something like "1e" and "-1" to be concatenated into the
                 single token "1e-1", as opposed to the Standard-conforming
                 behavior that produces the erroneous token "1e-" and the
                 separate token "1". */
              need_end_of_token_marker = FALSE;
            } else {
              need_end_of_token_marker = TRUE;
            }  /* if */
            (void)arg_get_token(&any_white_space_skipped);
            if (ms_compat && token_ends_macro_expansion &&
                !any_white_space_skipped) {
              /* Suppress the token separator to allow concatenation of the
                 final token of a macro expansion with the following
                 token. */
              need_end_of_token_marker = FALSE;
            }  /* if */
          }  /* while */
          if (scanning_text_not_in_primary_source_line) {
            /* We finished the part of the raw argument that we
               could rescan from the original insertion text.
               Continue in the raw_text buffer. */
            scanning_text_not_in_primary_source_line = FALSE;
            if (map->final_modif_for_initial_text != NULL) {
              map->final_modif_for_initial_text->is_isolated_text = FALSE;
            }  /* if */
            curr_char_loc= map->raw_text +
                           map->offset_in_raw_text_of_primary_source_line_text;
            (void)arg_get_token(&any_white_space_skipped);
            goto scan_expanded_tokens;
          }  /* if */
          /* Place terminating LE_END_OF_INSERTION lexical escape. */
          ensure_arg_expanded_text_space(LE_ESCAPE_LEN, map);
          map->expanded_text[map->expanded_len]   = LE_ESCAPE;
          map->expanded_text[map->expanded_len+1] = LE_END_OF_INSERTION;
#if FULLY_RESOLVED_MACRO_POSITIONS
          terminate_macro_text_map(&tracker, map->expanded_len+LE_ESCAPE_LEN);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if DEBUG
          if (debug_level >= 4) {
            fprintf(f_debug, "expanded argument %s: \"",
                             pp != NULL ? pp->name : "<extra>");
            /* Note that we are printing the "raw" text here, but whatever
               source modifications there are for macro expansions will
               be printed too.  This debug printing must be done at this
               point, before the changes are removed below. */
            print_markered_text(map->expanded_text, (sizeof_t)-1, FALSE);
            fputs("\"\n", f_debug);
          }  /* if */
#endif /* DEBUG */
          /* Remove the temporary source line modification that put the raw
             argument text back into the source line. */
          sequence_id = slmp->sequence_id;
          curr_char_loc = loc_of_insert(slmp);
          rem_source_line_modif(slmp);
          free_source_line_modif(&slmp);
          /* The macro expansions, if any, were done by applying source
             modifications to the raw text.  Remove those (thus restoring
             the original raw text). */
          if (sequence_id == sequence_id_for_source_line_modifs) {
            /* Modifications were not applied (otherwise the global counter
               sequence_id_for_source_line_modifs would have been incremented).
               */
          } else {
            for (slmp = source_line_modif_list; slmp != locked_slmp;) {
              slmp2 = slmp;
              slmp = slmp->next;
              if (slmp2->sequence_id > sequence_id) {
                /* Found a modification to this argument.  Remove it. */
                if (ptr_in_range(
                               slmp2->line_loc,
                               macro_buffer + num_compacted_macro_buffer_chars,
                               after_end_of_macro_buffer) &&
                    slmp2->inserted_text != slmp2->inserted_chars) {
                  /* The modification contributed to the count of deleted
                     characters in the macro buffer; reverse that
                     contribution and, if the parent modification is still
                     extant, update its count of deleted characters as
                     well.  (Modifications in which the inserted_text is
                     located in the modification's inserted_chars buffer
                     were added outside the character-counting regime,
                     typically by skip_white_space, and thus should not be
                     processed.) */
                  a_source_line_modif_ptr parent_slmp;
                  if (slmp2->parent_modif_determined) {
                    parent_slmp = slmp2->parent_modif;
                  } else {
                    parent_slmp = NULL;
                  }  /* if */
                  num_chars_deleted_in_macro_buffer -=
                                                slmp2->num_chars_to_delete - 1;
                  if (parent_slmp != NULL) {
                    check_assertion(parent_slmp->num_deleted_chars >=
                                    slmp2->num_chars_to_delete - 1);
                    parent_slmp->num_deleted_chars -=
                                                slmp2->num_chars_to_delete - 1;
                  }  /* if */
                }  /* if */
                rem_source_line_modif(slmp2);
                free_source_line_modif(&slmp2);
              }  /* if */
            }  /* for */
          }  /* if */
          /* Restore the lock state of the locked entry (if any): */
          if (locked_slmp != NULL) {
            locked_slmp->locked = saved_slm_lock;
          }  /* if */
          expand_macros = FALSE;
          /* Re-establish deletion of the characters of the macro
             invocation. */
          delete_source_from_loc = save_delete_source_from_loc;
          /* Re-get the "," or ")" that is next. */
          (void)arg_get_token(&any_white_space_skipped);
end_arg_expansion:;
          /* Advance to the next argument (unless we've given an error about
             too many arguments). */
          if (pp != NULL) {
            pp = pp->next;
            ++n_params;
          }  /* if */
          /* Keep looping while a comma is the next token. */
          not_done = (curr_token == tok_comma);
          if (not_done) {
            if (pp != NULL) {
              /* Set up for a potential call of choose_raw_or_expanded_arg. */
              use_raw_version_of_arg = pp->is_operand_of_paste;
            }  /* if */
            (void)arg_get_token(&any_white_space_skipped);
            use_raw_version_of_arg = FALSE;
          }  /* if */
        } while (not_done);
        remove_stop_token(tok_comma);
      }  /* if */
      /* Check that all of the formal parameters were taken. */
      if (pp != NULL) {
        /* An argument is missing.  This is an error, except in pcc
           preprocessing mode, SVR4 C mode, Sun mode, and Microsoft mode,
           where we issue a warning. It is also fine (no warning) to omit
           an extended or Microsoft variadic macro argument. */
        if (!((extended_variadic_macros_allowed || ms_compat) &&
              pp->next == NULL && mdp->variadic)) {
          an_error_severity sev;
          if (pcc_preprocessing_mode || SVR4_C_mode || ms_compat ||
              sun_mode) {
            sev = es_warning;
          } else {
            sev = es_discretionary_error;
          }  /* if */
          sym_diagnostic(sev, ec_too_few_macro_args, macro_symbol);
        }  /* if */
        /* Set the rest of the arguments to null (omitted, not empty)
           strings. */
        do {
          map = alloc_macro_arg();
          add_to_arg_values(map);
          map->raw_len = 0;
          map->raw_text[0] = LE_ESCAPE;
          map->raw_text[1] = LE_END_OF_INSERTION;
          map->expanded_len = 0;
          map->expanded_text[0] = LE_ESCAPE;
          map->expanded_text[1] = LE_END_OF_INSERTION;
          map->is_empty_arg = FALSE;
#if FULLY_RESOLVED_MACRO_POSITIONS
          /* Add empty text map entries. */
          add_entry_to_macro_text_map(&map->raw_text_map,
                                      /*start_of_region=*/0,
                                      pos_curr_token.orig_seq,
                                      pos_curr_token.orig_column,
                                      this_macro_invocation_record);
          add_entry_to_macro_text_map(&map->raw_text_map,
                                      LE_ESCAPE_LEN, (a_seq_number)0,
                                      SP_COL_UNKNOWN,
                                      this_macro_invocation_record);
          add_entry_to_macro_text_map(&map->exp_text_map,
                                      /*start_of_region=*/0,
                                      pos_curr_token.orig_seq,
                                      pos_curr_token.orig_column,
                                      this_macro_invocation_record);
          add_entry_to_macro_text_map(&map->exp_text_map,
                                      LE_ESCAPE_LEN, (a_seq_number)0,
                                      SP_COL_UNKNOWN,
                                      this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
          pp = pp->next;
          ++n_params;
        } while (pp != NULL);
      }  /* if */
      /* Check for closing parenthesis.  Note that if it is present, the
         token is not deleted yet; that happens at the time of insertion
         below. */
      if (curr_token != tok_rparen) {
        if (curr_token == tok_end_of_source && macro_depth > 1 &&
            microsoft_bugs) {
          /* This invocation occurs within a macro argument.  The Microsoft
             preprocessor allows the closing parenthesis to occur in the
             text following the outermost macro invocation.  We support
             this by canceling the current macro invocation and copying the
             macro name and raw arguments into a source line modification,
             marking the macro name as temporarily inert.  This suppresses
             the invocation until after the top-level invocation is
             completed and the following text is available to be scanned
             for the closing parenthesis. */
          remove_stop_token(tok_rparen);
          is_macro_call = FALSE;
#if RECORD_MACRO_INVOCATIONS
          /* Remove the record of this invocation. */
          revert_macro_invocation_record();
#endif /* RECORD_MACRO_INVOCATIONS */
          /* Start the replacement text with the macro name, preceded by
             an LE_TEMPORARILY_INERT_MACRO escape and followed by a '('. */
          repl_text_len = LE_ESCAPE_LEN +
                                   macro_symbol->header->identifier_length + 1;
          ensure_macro_buffer_space(repl_text_len);
          rescan_loc = src_loc = next_avail_in_macro_buffer;
          *src_loc++ = LE_ESCAPE;
          *src_loc++ = LE_TEMPORARILY_INERT_MACRO;
          (void)memcpy(src_loc, macro_symbol->header->identifier,
                       macro_symbol->header->identifier_length);
          src_loc += macro_symbol->header->identifier_length;
          *src_loc++ = '(';
          next_avail_in_macro_buffer = src_loc;
#if FULLY_RESOLVED_MACRO_POSITIONS
          first_text_map_entry = macro_text_map.num_entries;
          if (invocation_slmp != NULL) {
            /* The macro name was in a source line modification.  Clone the
               map entry for the macro name (to preserve its original source
               position). */
            clone_macro_text_map_entries(&invocation_slmp->text_map,
                                         src_offset, src_token_len,
                                         &macro_text_map, LE_ESCAPE_LEN,
                                         NO_PARENT_MACRO_INVOCATION);
          } else {
            /* The macro name was in the current source line.  Add a map
               entry mapping the name in the replacement back to the
               original position. */
            add_entry_to_macro_text_map(&macro_text_map, LE_ESCAPE_LEN,
                                        src_pos.seq, src_pos.column,
                                        NO_PARENT_MACRO_INVOCATION);
          }  /* if */
          /* Add a text map entry for the left parenthesis. */
          add_entry_to_macro_text_map(&macro_text_map,
                                      src_loc - rescan_loc - 1,
                                      lparen_pos.orig_seq,
                                      lparen_pos.orig_column,
                                      macro_context_of(lparen_pos));
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
          /* Now copy the raw text of all the macro arguments into the
             replacement text. */
          for (map = arg_values[0]; map != NULL; map = map->next) {
            ensure_macro_buffer_space(map->raw_len + 1);
            (void)memcpy(src_loc, map->raw_text, map->raw_len);
#if FULLY_RESOLVED_MACRO_POSITIONS
            clone_macro_text_map_entries(&map->raw_text_map,
                                         /*starting_src_offset=*/0,
                                         map->raw_len, &macro_text_map,
                                         (sizeof_t)(src_loc - rescan_loc),
                                         NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            src_loc += map->raw_len;
            if (map->next != NULL) {
              /* Add a comma to separate this argument from the next. */
              *src_loc++ = ',';
#if FULLY_RESOLVED_MACRO_POSITIONS
              check_assertion(map->comma_pos.seq != 0);
              add_entry_to_macro_text_map(&macro_text_map,
                                          src_loc - rescan_loc - 1,
                                          map->comma_pos.orig_seq,
                                          map->comma_pos.orig_column,
                                          macro_context_of(map->comma_pos));
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            }  /* if */
            next_avail_in_macro_buffer = src_loc;
          }  /* for */
          /* Terminate the insertion. */
          ensure_macro_buffer_space(LE_ESCAPE_LEN);
          *next_avail_in_macro_buffer++ = LE_ESCAPE;
          *next_avail_in_macro_buffer++ = LE_END_OF_INSERTION;
          /* Add the source line modification for the replacement. */
          slmp = add_source_line_modif(
                            delete_source_from_loc,
                            (sizeof_t)(curr_char_loc - delete_source_from_loc),
                            rescan_loc, src_loc);
          slmp->is_isolated_text = TRUE;
          adjust_deletion_counts(delete_source_from_loc,
                                 slmp->num_chars_to_delete);
          slmp->source_position = start_pos;
#if FULLY_RESOLVED_MACRO_POSITIONS
          /* Add a terminal entry to macro_text_map and point the source line
             modification's text_map at the appropriate subset of those
             entries. */
          add_entry_to_macro_text_map(&macro_text_map, src_loc - rescan_loc,
                                      (a_seq_number)0, SP_COL_UNKNOWN,
                                      NO_PARENT_MACRO_INVOCATION);
          slmp->text_map.num_entries = macro_text_map.num_entries -
                                                          first_text_map_entry;
          slmp->text_map.entries =
                                 &macro_text_map.entries[first_text_map_entry];
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
          goto return_point;
        }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
        if (curr_token == tok_end_of_source && macro_depth > 1) {
          /* We are expanding a macro argument.  The position of the
             end-of-source token will be in the argument list of the
             containing macro invocation, which could lead to inanities like
             pointing to a right parenthesis for the "expected a ')'"
             message.  Instead, set error_position to point after the last
             token we got in this nested macro invocation; if we're lucky,
             that will look like the end of an incomplete macro invocation,
             depending on where the token came from. */
          conv_line_loc_to_source_pos(after_last_invocation_token,
                                      &error_position);
        }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
        syntax_error(ec_exp_rparen);
      }  /* if */
      remove_stop_token(tok_rparen);
      got_proper_closing_token = (curr_token == tok_rparen);
      if (!got_proper_closing_token) {
        /* Issue an error about an improperly terminated macro call, to give
           the user the position of the macro call.  This helps when there's
           a runaway macro call that swallows hundreds of source lines and
           runs into the end of file. */
        pos_error(ec_improperly_terminated_macro_call, &start_pos);
      }  /* if */
#if RECORD_MACRO_INVOCATIONS && EXTRA_SOURCE_POSITIONS_IN_IL
      if (got_proper_closing_token) {
        /* Record the ending position of the macro invocation, i.e., the
           original position of the closing parenthesis. */
        this_mirp->end.seq = pos_curr_token.orig_seq;
        this_mirp->end.column = pos_curr_token.orig_column;
      }  /* if */
#endif /* RECORD_MACRO_INVOCATIONS && EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
    in_macro_arg_list = saved_in_macro_arg_list;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "Arguments scanned, about to do replacement of macro %s:\n",
                     macro_symbol->header->identifier);
  }  /* if */
#endif /* DEBUG */
  if (!got_proper_closing_token) {
    /* Did not get proper closing token, so do not do the replacement.
       This happened because a macro invocation is incomplete at
       end of file, end of a preprocessing directive, or end of
       a macro argument being macro-expanded in isolation.  Suppressing
       the expansion avoids some difficult problems with doing the
       replacement (problems that stem from the fact that the source
       modification technique really only allows replacements, not
       straight insertions). */
    /* Delete any part of the macro invocation that is on this line. */
    if (delete_source_from_loc != NULL &&
        delete_source_from_loc < start_of_curr_token) {
      slmp = add_source_line_modif(delete_source_from_loc,
                                   (sizeof_t)(start_of_curr_token -
                                                       delete_source_from_loc),
                                   (char *)NULL, (char *)NULL);
      adjust_deletion_counts(delete_source_from_loc,
                             slmp->num_chars_to_delete);
      /* Put the null replacement string (for a deletion) in the
         source line modification's inboard inserted_chars array. */
      *slmp->inserted_chars   = LE_ESCAPE;
      slmp->inserted_chars[1] = LE_END_OF_INSERTION;
      slmp->inserted_text = slmp->end_inserted_text = slmp->inserted_chars;
    }  /* if */
    *rescan = FALSE;
    ctoken = curr_token;
    goto return_point;
  }  /* if */
  if (pcc_mode_macro_recursion) {
    /* For pcc mode macro recursion, use an empty string as the expansion
       of the macro to avoid more recursion errors. */
    special_repl_text = TRUE;
    repl_text[0] = '\0';
  }  /* if */
  if (repl_text == NULL) {
    /* This is a predefined macro whose value must be computed. */
    /* Use a special a_macro_arg entry as the expansion text buffer.  Put
       it on the list of macro args so it can be found if the buffers are
       resized. */
    special_macro_arg = alloc_macro_arg();
    add_to_macro_arg_list(special_macro_arg);
    special_repl_text = TRUE;
    repl_text = special_macro_arg->raw_text;
    if (map != arg_values[0]) {
      /* All these predefined macros take exactly one argument, but the
         invocation had none or too many.  A diagnostic was issued above;
         here, just make the value 0 to indicate failure. */
      strcpy(repl_text, "0");
    } else if (macro_symbol == clang_has_feature_symbol ||
               macro_symbol == clang_has_extension_symbol) {
      /* This is the clang __has_feature or __has_extension macro.  The
         result is the value 1 if the named feature or type trait helper is
         available in the current execution of the front end and 0
         otherwise.  (Although the clang documentation describes some
         differences between the two, current implementations treat them
         nearly identically; the only exception is in C++03 mode for C++11
         features that are accepted with a warning.  For those features
         when running in C++03 mode, clang returns 0 for __has_feature but
         1 for __has_extension.  The front end does not make that
         distinction, so the two macros are treated equivalently here.) */
      a_const_char *feature_name = clang_feature_test_id(map, &arg_position);
      a_boolean    feature_supported = FALSE;
      if (feature_name != NULL) {
        /* First check to see if the specified identifier is the name of a
           feature. */
        feature = 
         (a_feature_support *)bsearch((a_bsearch_arg_type)feature_name,
                                      (a_bsearch_arg_type)feature_support_list,
                                      size_t_arg(NUM_FEATURES),
                                      sizeof(a_feature_support),
                                      compare_feature_names);
        if (feature != NULL) {
          feature_supported = (feature->enabled != NULL && *feature->enabled);
        } else if (type_traits_helpers_enabled) {
          /* The identifier is not the name of a feature, so check it
             against the list of supported C++ type trait helpers. */
          feature_supported =
                        (bsearch((a_bsearch_arg_type)feature_name,
                                 (a_bsearch_arg_type)clang_type_traits_helpers,
                                 size_t_arg(NUM_CLANG_TYPE_TRAITS),
                                 sizeof(a_const_char *),
                                 compare_type_traits_helper_names) != NULL);
        }  /* if */
      }  /* if */
      strcpy(repl_text, feature_supported ? "1" : "0");
    } else if (macro_symbol == clang_has_attribute_symbol) {
      /* The clang __has_attribute macro.  Has the value 1 if the named
         attribute is available in the current execution of the front end
         and 0 otherwise. */
      a_const_char *attribute_name = clang_feature_test_id(map, &arg_position);
      if (attribute_name != NULL &&
          gnu_attribute_is_supported(attribute_name)) {
        strcpy(repl_text, "1");
      } else {
        strcpy(repl_text, "0");
      }  /* if */
    } else if (macro_symbol == clang_has_builtin_symbol) {
      /* The clang __has_builtin macro.  Has the value 1 if the named
         builtin function is available in the current execution of the
         front end and 0 otherwise. */
#if BUILTIN_FUNCTIONS_ENABLED
      a_const_char *builtin_name = clang_feature_test_id(map, &arg_position);
      if (builtin_name != NULL &&
          builtin_function_is_enabled(builtin_name)) {
        strcpy(repl_text, "1");
      } else {
        strcpy(repl_text, "0");
      }  /* if */
#else /* !BUILTIN_FUNCTIONS_ENABLED */
      /* There are no GNU-style builtin functions.  Just check the argument for
         correctness and give the value 0. */
      (void)clang_feature_test_id(map, &arg_position);
      strcpy(repl_text, "0");
#endif /* BUILTIN_FUNCTIONS_ENABLED */
    } else {
      unexpected_condition_str(
                         "macro_invocation: unknown special predefined macro");
    }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
    if (invocation_slmp != NULL) {
      /* The macro name was in a source line modification.  Clone the map
         entry for the macro name (to preserve its original source
         position). */
      clone_macro_text_map_entries(&invocation_slmp->text_map,
                                   src_offset,
                                   src_token_len,
                                   &special_macro_arg->raw_text_map,
                                   /*starting_targ_offset=*/0,
                                   NO_PARENT_MACRO_INVOCATION);
    } else {
      /* The macro name was in the current source line.  Add a map entry
         mapping the expansion back to the original position. */
      add_entry_to_macro_text_map(&special_macro_arg->raw_text_map,
                                  /*start_of_region=*/0, src_pos.seq,
                                  src_pos.column,
                                  NO_PARENT_MACRO_INVOCATION);
    }  /* if */
    add_entry_to_macro_text_map(&special_macro_arg->raw_text_map,
                                (sizeof_t)strlen(repl_text), (a_seq_number)0,
                                SP_COL_UNKNOWN, NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* if */
  /* Replace the identifier by the replacement text.  Start by determining
     the length of the replacement string. */
  if (special_repl_text) {
    /* One of the special macros, like __LINE__ and  __FILE__; the text is
       just a string. */
    if (!repl_text_len_precomputed) repl_text_len = strlen(repl_text);
  } else {
    /* Normal replacement text, with sections. */
    /* coverity[uninit_use_in_call] - thinks arg_values may be unset. */
    repl_text_len = length_of_replacement_text(repl_text, n_params, mdp,
                                               arg_values);
  }  /* if */
  /* repl_text_len now indicates the size of the expansion.  Note that
     in the case of an expanded argument value, the expansion may be
     further modified by source line modifications. */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Expansion length is %u\n", (unsigned int)repl_text_len);
  }  /* if */
#endif /* DEBUG */
  /* Make enough room in macro_buffer for the expansion, an
     LE_END_OF_TOP_LEVEL_EXPANSION escape, if needed, and the following
     LE_END_OF_INSERTION lexical escape. */
  ensure_macro_buffer_space(repl_text_len +
                            space_for_end_of_top_level_expansion_escape +
                            LE_ESCAPE_LEN);
  /* Move the text into macro_buffer. */
  rescan_loc = src_loc = next_avail_in_macro_buffer;
  next_avail_in_macro_buffer += repl_text_len;
  if (space_for_end_of_top_level_expansion_escape != 0) {
    *next_avail_in_macro_buffer++ = LE_ESCAPE;
    *next_avail_in_macro_buffer++ = LE_END_OF_TOP_LEVEL_EXPANSION;
  }  /* if */
  /* Store final LE_END_OF_INSERTION lexical escape. */
  *next_avail_in_macro_buffer++ = LE_ESCAPE;
  *next_avail_in_macro_buffer++ = LE_END_OF_INSERTION;
  if (special_repl_text) {
    /* __LINE__,  __FILE__, defined, etc.; the text is just a string. */
    (void)memcpy(src_loc, repl_text, size_t_arg(repl_text_len));
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Remember where the text map entries begin for the source line
       modification we will add and copy the entries from the special
       argument. */
    first_text_map_entry = macro_text_map.num_entries;
    next_targ_offset = next_avail_in_macro_buffer - rescan_loc;
    if (!pcc_mode_macro_recursion) {
      sizeof_t inert_macro_offset = (is_inert_macro) ? 2 : 0;
      clone_macro_text_map_entries(&special_macro_arg->raw_text_map,
                                   inert_macro_offset,
                                   repl_text_len - inert_macro_offset,
                                   &macro_text_map,
                                   inert_macro_offset,
                                   this_macro_invocation_record);
    }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  } else {
    /* More complicated expansion; do it by interpreting the replacement
       text sections. */
    a_boolean prev_section_is_paste = FALSE;
    sizeof_t  prev_sect_len = 0;
    a_boolean is_va_arg_substitution;

    src_loc_before_copy = src_loc;
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Remember where the text map entries begin for the source line
       modification we will add. */
    first_text_map_entry = macro_text_map.num_entries;
    next_targ_offset = next_avail_in_macro_buffer - rescan_loc;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    for (rtp = repl_text; *rtp != (int)rt_null;) {
      is_va_arg_substitution = FALSE;
      rts_kind = (a_repl_text_seq_kind)*(rtp++);
      /* Extract the section length or argument number. */
      get_macro_repl_text_number(rts_number, rtp);
      if (rts_kind == rt_text) {
        sect_len = rts_number;
        text_loc = rtp;
        rtp += sect_len;
#if FULLY_RESOLVED_MACRO_POSITIONS
        /* Clone the text map regions from the text section.  If the section
           begins with an escape, possibly followed by a space, skip over
           them -- the offsets in the text map reflect the location of the
           first token, not that of the preceding escape sequence and/or
           space character. */
        bytes_before_token = (*text_loc == LE_ESCAPE) ? LE_ESCAPE_LEN : 0;
        if (text_loc[bytes_before_token] == ' ') {
          ++bytes_before_token;
        }  /* if */
        clone_macro_text_map_entries(
                             &mdp->text_map,
                             text_loc - mdp->repl_text + bytes_before_token,
                             sect_len - bytes_before_token,
                             &macro_text_map,
                             src_loc - rescan_loc + bytes_before_token,
                             this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
      } else if (rts_kind == rt_paste ||
                 rts_kind == rt_microsoft_magic_arg_marker) {
        sect_len = 0;
      } else {
        char *final_inert_escape;
        /* Other section kinds have an associated parameter number. */
        get_arg_value(rts_number, map);
        switch (rts_kind) {
          case rt_raw_argument:
            /* The raw (non-macro-expanded) value of the argument. */
            is_va_arg_substitution = (mdp->variadic && rts_number == n_params);
            sect_len = map->raw_len;
            text_loc = map->raw_text;
            /* Remove an LE_INERT_MACRO escape at the beginning if present,
               since the token is being pasted to another one. */
            if (prev_section_is_paste &&
                map->raw_text[0] == LE_ESCAPE &&
                map->raw_text[1] == LE_INERT_MACRO) {
              sect_len -= LE_ESCAPE_LEN;
              text_loc += LE_ESCAPE_LEN;
              /* Note that length_of_replacement_text did the same test and
                 reduced the overall repl_text_len for this case.  It's
                 important to actually remove the inert-macro escape (rather
                 than replacing it with an end-of-token escape, as below)
                 because we want to have the identifier text abut the preceding
                 token. */
            }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
            { a_const_char *post_end;
              if (ms_extensions && prev_section_is_paste &&
                  is_microsoft_function_name_paste(map,
                                                   rescan_loc,
                                                   (sizeof_t)(src_loc-
                                                              rescan_loc),
                                                   &post_end)) {
                /* This is token pasting of L##__FUNCTION__ or the like, which
                   is replaced by __LPREFIX(__FUNCTION__).  Note that
                   length_of_replacement_text has to do the right length
                   computation for this. */
                a_const_char *tok = token_names[(int)tok_microsoft_lprefix];
                sizeof_t     tok_len = strlen(tok);
                sizeof_t     fnk_len;
                src_loc--;  /* Back up to remove the "L". */
                /* Add "__LPREFIX(". */
                (void)memcpy(src_loc, tok, size_t_arg(tok_len));
                src_loc += tok_len;
                *src_loc++ = '(';
                /* Copy the function-name keyword. */
                fnk_len = post_end - map->raw_text;
#if FULLY_RESOLVED_MACRO_POSITIONS
                /* Copy the map entry for the function keyword. */
                clone_macro_text_map_entries(&map->raw_text_map,
                                             /*starting_src_offset=*/0,
                                             fnk_len - 1,
                                             &macro_text_map,
                                             (sizeof_t)(src_loc - rescan_loc),
                                             this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
                (void)memcpy(src_loc, text_loc, size_t_arg(fnk_len));
                src_loc += fnk_len;
                /* Add the closing parenthesis. */
                *src_loc++ = ')';
                /* Anything after the keyword is copied below. */
                text_loc += fnk_len;
                sect_len -= fnk_len;
              }  /* if */
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            final_inert_escape = find_final_inert_escape(text_loc, sect_len);
            if (final_inert_escape != NULL) {
              /* Remove an LE_INERT_MACRO escape preceding an identifier
                 at the end if present, since the token is being pasted
                 to another one. */
              /* Replace the inert-macro escape by an end-of-token escape to
                 keep the overall length the same (repl_text_len has already
                 been determined). */
              /* Copy the part before the escape here, and copy the part
                 after the escape (the identifier name) in the normal
                 code below. */
              sizeof_t initial_len = final_inert_escape - text_loc;
              if (initial_len > 0) {
#if FULLY_RESOLVED_MACRO_POSITIONS
                /* Copy the map entries for the text preceding the escape. */
                clone_macro_text_map_entries(&map->raw_text_map,
                                             (sizeof_t)(text_loc -
                                                        map->raw_text),
                                             initial_len - 1,
                                             &macro_text_map,
                                             (sizeof_t)(src_loc -
                                                        rescan_loc),
                                             this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
                (void)memcpy(src_loc, text_loc,
                             size_t_arg(initial_len)); /*lint !e668 */
                src_loc += initial_len;
              }  /* if */
              *src_loc++ = LE_ESCAPE;
              *src_loc++ = LE_END_OF_TOKEN;
              text_loc = final_inert_escape+LE_ESCAPE_LEN;
              sect_len -= initial_len+LE_ESCAPE_LEN;
            }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
            /* Copy the rest of the map entries (or all of them, if none were
               copied above). */
            ending_src_offset = map->raw_text_map.
                    entries[map->raw_text_map.num_entries-1].start_of_region-1;
            clone_macro_text_map_entries(&map->raw_text_map,
                                         (sizeof_t)(text_loc - map->raw_text),
                                         ending_src_offset -
                                                    (text_loc - map->raw_text),
                                         &macro_text_map,
                                         (sizeof_t)(src_loc - rescan_loc),
                                         this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            break;
          case rt_stringized_raw_argument:
          case rt_charized_raw_argument:
            /* The stringized or charized value of the argument. */
            if (map->raw_len == 0 && ms_compat && !map->is_empty_arg) {
              /* The Microsoft preprocessor suppresses all output for
                 omitted (as opposed to empty) arguments.  That is, given

                   #define M(a,b) #b

                 M(1) expands to nothing, while M(1,) expands to "". */
            } else {
#if FULLY_RESOLVED_MACRO_POSITIONS
              /* The result will be a single token, so we only need the
                 starting position from the raw_text_map; the other map
                 entries would point inside the literal and thus could never
                 be used. */
              clone_macro_text_map_entries(&map->raw_text_map,
                                           /*starting_src_offset=*/0,
                                           /*ending_src_offset=*/0,
                                           &macro_text_map,
                                           (sizeof_t)(src_loc - rescan_loc),
                                           this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
              (void)stringized_arg(map, &src_loc,
                                   rts_kind == rt_charized_raw_argument);
#if FULLY_RESOLVED_MACRO_POSITIONS
              /* Add an extra entry to macro_text_map so that the ending
                 position of the stringized token will map to the last
                 character of the argument text.  The starting offset for
                 the entry will be the closing quote, i.e., one before the
                 next available space in the buffer, and the source
                 position will be the same as the last region in the
                 argument's raw text map, offset to the end of the argument
                 text (i.e., one before the ending offset less the final
                 LE_END_OF_INSERTION escape). */
              tmep =
                   &map->raw_text_map.entries[map->raw_text_map.num_entries-2];
              src_offset = tmep[1].start_of_region - tmep[0].start_of_region -
                                                             LE_ESCAPE_LEN - 1;
              add_entry_to_macro_text_map(
                                        &macro_text_map,
                                        (sizeof_t)(src_loc - rescan_loc - 1),
                                        tmep->corresponding_source_pos.seq,
                                        tmep->corresponding_source_pos.column +
                                                                    src_offset,
                                        this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            }  /* if */
            goto copy_done;
          case rt_argument:
            /* The macro-expanded value of the argument. */
            is_va_arg_substitution = (mdp->variadic && rts_number == n_params);
            sect_len = map->expanded_len;
            text_loc = map->expanded_text;
#if FULLY_RESOLVED_MACRO_POSITIONS
            /* Copy the expanded text map entries, using
               NO_PARENT_MACRO_INVOCATION as the macro context to preserve
               the context from the expanded argument. */
            ending_src_offset = map->exp_text_map.
                    entries[map->exp_text_map.num_entries-1].start_of_region-1;
            clone_macro_text_map_entries(&map->exp_text_map,
                                         /*starting_src_offset=*/0,
                                         ending_src_offset,
                                         &macro_text_map,
                                         (sizeof_t)(src_loc - rescan_loc),
                                         NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
            break;
          case rt_microsoft_maybe_raw_argument:
            /* This is either the raw or the expanded text of an argument,
               depending on how it is used in the replacement text. */
            is_va_arg_substitution = (mdp->variadic && rts_number == n_params);
            if (map->raw_len == 0) {
              /* Special handling is not needed. */
              sect_len = 0;
            } else {
              /* The inserted text will be an LE_RAW_OR_EXPANDED_ARGUMENT
                 lexical escape, followed by the raw text of the argument,
                 followed by the expanded text of the argument (or a single
                 space if the length of the expanded text is zero).  Both
                 versions will be deleted via source line modifications,
                 leaving the correct version to be selected when the
                 LE_RAW_OR_EXPANDED_ARGUMENT escape is processed.  See
                 choose_raw_or_expanded_arg for details. */
              *src_loc++ = LE_ESCAPE;
              *src_loc++ = LE_RAW_OR_EXPANDED_ARGUMENT;
              /* Calculate the effective length of the raw version. */
              sect_len = map->raw_len;
              if ((extended_variadic_macros_allowed || ms_compat) &&
                  mdp->variadic &&
                  ((a_repl_text_seq_kind)*rtp == rt_paste ||
                   (a_repl_text_seq_kind)*rtp ==
                                              rt_microsoft_magic_arg_marker)) {
                adjust_length_for_magic_arg(rts_kind, rtp, n_params,
                                            arg_values, &sect_len,
                                            &add_escape);
              }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
              /* Copy the raw text map entries, using
                 NO_PARENT_MACRO_INVOCATION as the macro context to preserve
                 the context from the raw argument. */
              clone_macro_text_map_entries(&map->raw_text_map,
                                           /*starting_src_offset=*/0,
                                           sect_len, &macro_text_map,
                                           (sizeof_t)(src_loc - rescan_loc),
                                           this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
              /* Add the raw argument text and delete it. */
              (void)memcpy(src_loc, map->raw_text, size_t_arg(sect_len));
              add_deletion_source_line_modif(src_loc, sect_len,
                                             /*for_comment=*/FALSE);
              src_loc += sect_len;
              /* Add the expanded argument text. */
#if FULLY_RESOLVED_MACRO_POSITIONS
              /* Copy the expanded text map entries, using
                 NO_PARENT_MACRO_INVOCATION as the macro context to preserve
                 the context from the expanded argument. */
              clone_macro_text_map_entries(&map->exp_text_map,
                                           /*starting_src_offset=*/0,
                                           map->expanded_len, &macro_text_map,
                                           (sizeof_t)(src_loc - rescan_loc),
                                           this_macro_invocation_record);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
              /* Add the expanded argument text or a space to which the
                 deletion source line modification can be attached. */
              sect_len = map->expanded_len;
              if (sect_len > 0) {
                (void)memcpy(src_loc, map->expanded_text,
                             size_t_arg(sect_len));
              } else {
                sect_len = 1;
                *src_loc = ' ';
              }  /* if */
              add_deletion_source_line_modif(src_loc, sect_len,
                                             /*for_comment=*/FALSE);
              src_loc += sect_len;
              goto copy_done;
            }  /* if */
            break;
          default:
            unexpected_condition_str2("macro_invocation:",
                                      "expansion section unknown");
        }  /* switch */
      }  /* if */
      /* When extended variadic macros are enabled, a "##" followed by an
         empty variadic argument has a special deletion effect.  The same
         is true for Microsoft variadic macros, with or without the "##". */
      if ((extended_variadic_macros_allowed || ms_compat) &&
          mdp->variadic &&
          ((a_repl_text_seq_kind)*rtp == rt_paste ||
           (a_repl_text_seq_kind)*rtp == rt_microsoft_magic_arg_marker)) {
        adjust_length_for_magic_arg(rts_kind, rtp, n_params, arg_values,
                                    &sect_len, &add_escape);
      } else {
        add_escape = FALSE;
      }  /* if */
      if (sect_len != 0) {
        /*lint --e(668)*/(void)memcpy(src_loc, text_loc, size_t_arg(sect_len));
        src_loc += sect_len;
        if (add_escape) {
          /* The insertion ended with a comma that might or might not be
             suppressed as a result of appearing before an empty
             __VA_ARGS__ expansion.  Replace the last three characters of
             the insertion with an LE_MICROSOFT_MAGIC_COMMA escape so that
             skip_white_space will be able to handle it correctly during
             the rescan. */
          src_loc[-3] = LE_ESCAPE;
          src_loc[-2] = LE_MICROSOFT_MAGIC_COMMA;
          src_loc[-1] = ',';
        }  /* if */
      }  /* if */
copy_done:
      if (check_concatenations && prev_section_is_paste &&
          src_loc - src_loc_before_copy != 0 && prev_sect_len != 0 &&
          !(gnu_mode && is_va_arg_substitution)) {
        /* The result reflects concatenating two non-empty text sections.
           Record the concatenation so that retokenizing can check for
           having created an invalid token.  (The GNU preprocessor allows
           invalid concatenation when the second operand is __VA_ARG__,
           so we don't record such concatenations in gnu_mode.) */
        add_concatenation_record(&concat_record_head, &concat_record_tail,
                                 src_loc_before_copy, macro_symbol);
      }  /* if */
      if (rts_kind == rt_paste) {
        prev_section_is_paste = TRUE;
      } else {
        prev_section_is_paste = FALSE;
        prev_sect_len = src_loc - src_loc_before_copy;
      }  /* if */
      src_loc_before_copy = src_loc;
    }  /* for */
  }  /* if */
  if (check_expansion_for_recursion) {
    /* This macro invocation appears in the expansion of an earlier
       invocation of the same macro.  Normally that would mark the macro as
       inert.  In the Microsoft preprocessor, however, the macro is only
       treated as inert if the expansions begin with an invocation of the
       same macro.  Find all previous invocations of this macro that are
       still active and check their text against the just-expanded text. */
    for (slmp = invocation_slmp; slmp != NULL;
         slmp = parent_source_line_modif(slmp)) {
      if (slmp->assoc_macro == macro_symbol &&
          same_macro_at_beginning(slmp, rescan_loc)) {
        /* The current expansion begins with the same macro name as the
           previous expansion.  Reset the state appropriately and treat
           the macro name as inert. */
        is_inert_macro = TRUE;
        macro_depth = saved_macro_depth;
#if RECORD_MACRO_INVOCATIONS
        revert_macro_invocation_record();
#endif /* RECORD_MACRO_INVOCATIONS */
        free_macro_arg_entries(prev_end_of_macro_arg_list);
#if FULLY_RESOLVED_MACRO_POSITIONS
        macro_text_map.num_entries = first_text_map_entry;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
        next_avail_in_macro_buffer = (char *)rescan_loc;
          /* Skip over the macro name. */
        curr_char_loc =
              delete_source_from_loc + macro_symbol->header->identifier_length;
        goto make_inert_macro;
      }  /* if */
    }  /* for */
  }  /* if */
  /* Add a source modification that puts the replacement text into the
     logical source line at the right place.  Aside from modifying the
     source that is scanned, this also records the fact that this macro's
     name is protected from expansion (is inert) within its own
     expansion. */
  /* The macro invocation finished correctly, and the
     current token is the macro identifier (for an object-like macro)
     or the closing parenthesis (for a function-like macro).  We can
     delete the proper part of the macro invocation and do the
     insertion with one modification. */
  /* The text logically deleted here is either the entire macro invocation
     (if it is all on one line), or the part of it on this line (if it
     spans several lines). */
  slmp = add_source_line_modif(delete_source_from_loc,
                               (sizeof_t)(curr_char_loc -
                                                       delete_source_from_loc),
                               rescan_loc, rescan_loc + repl_text_len +
                               space_for_end_of_top_level_expansion_escape);
  adjust_deletion_counts(delete_source_from_loc, slmp->num_chars_to_delete);
  slmp->assoc_macro = macro_symbol;
  slmp->source_position = start_pos;
  if (invocation_slmp != NULL &&
      ptr_in_range(delete_source_from_loc, invocation_slmp->inserted_text,
                   invocation_slmp->end_inserted_text)) {
    set_parent_modif(slmp, invocation_slmp);
  }  /* if */
#if FULLY_RESOLVED_MACRO_POSITIONS
  if (first_text_map_entry == macro_text_map.num_entries) {
    /* If the replacement text is empty, the loop exits immediately and no map
       entries are copied.  Clone the initial map entry here. */
    clone_macro_text_map_entries(&mdp->text_map, /*starting_src_offset=*/0,
                                 /*src_region_len=*/0, &macro_text_map,
                                 /*starting_targ_offset=*/0,
                                 this_macro_invocation_record);
  }  /* if */
  /* Add a terminal entry to macro_text_map and point the source line
     modification's text_map at the appropriate subset of those entries. */
  add_entry_to_macro_text_map(&macro_text_map, next_targ_offset,
                              (a_seq_number)0, SP_COL_UNKNOWN,
                              this_macro_invocation_record);
  slmp->text_map.num_entries =
                             macro_text_map.num_entries - first_text_map_entry;
  slmp->text_map.entries = &macro_text_map.entries[first_text_map_entry];
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
  slmp->invocation_record = this_macro_invocation_record;
  slmp->invocation_depth = macro_invocation_stack_depth;
#endif /* RECORD_MACRO_INVOCATIONS */
  slmp->concatenations = concat_record_head;
  /* Can't set the parent modification here without looking it up.  In
     particular, the modification from which the macro identifier came may
     not be the right one in the case of a multi-line macro call or when
     the macro identifier (only) was generated by a macro expansion. */
  if ((pcc_preprocessing_mode ||
       /* Avoid a problem with a missing parenthesis on a "defined"
          operator. */
       (ms_compat && curr_token != tok_newline)) &&
      is_macro_call && macro_depth == 1) {
    /* In pcc mode, in order to more closely approximate the token-pasting
       behavior of pcc, we immediately macro-expand the text resulting from a
       top-level macro invocation, then make a copy of the macro-expanded
       version as one long string. */
    /* This also applies in Microsoft mode (old-style concatenation can
       still be done). */
    /* Free any allocated macro buffers now, to make their space available
       in the macro expansions about to be done. */
    free_macro_arg_entries(prev_end_of_macro_arg_list);
    if (ms_compat) {
      top_microsoft_slmp = slmp;
    }  /* if */
    expand_top_level_pcc_macro(slmp);
    top_microsoft_slmp = NULL;
    /* The location of the inserted text may have changed. */
    rescan_loc = slmp->inserted_text;
  }  /* if */
  if (!*rescan) {
    /* Here, we have a case where we have changed the source line because
       of a macro expansion, and we also know that the replacement text
       is a single token.  Adjust the token bounds, since they may not
       be correct.  len_of_curr_token is set in get_token. */
    start_of_curr_token = rescan_loc;
    end_of_curr_token = start_of_curr_token + repl_text_len - 1;
  }  /* if */
return_point:
  /* End of macro invocation processing.  Clean up and return. */
  if (is_macro_call) {
    /* Record the reference to the macro for cross-reference purposes.
       This is not done for cases where the identifier turned out not to be
       a macro. */
    mark_referenced(macro_symbol, &start_pos);
    if (mdp->ref_suppresses_pch_file) {
      /* Referencing this macro within a header file is not compatible with
         generating a precompiled header. */
      suppress_creation_of_pch();
    }  /* if */
  }  /* if */
  /* Free any allocated macro buffers.  Note that this includes
     special_macro_arg as well as any normal arguments.  Also note that
     in pcc mode this may be the second call of free_macro_arg_entries,
     and it will do nothing (the entries have already been freed, and
     it can tell). */
  free_macro_arg_entries(prev_end_of_macro_arg_list);
  /* Drop any local pointer registrations. */
  registered_pointers = save_registered_pointers;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  if (*rescan) {
    /* For a rescan, set the current position to what is to be rescanned. */
#if CHECKING
    if (rescan_loc == NULL) {
      internal_error("macro_invocation: *rescan TRUE, rescan_loc == NULL");
    }  /* if */
#endif /* CHECKING */
    curr_char_loc = rescan_loc;
  } else {
    /* For cases where no rescan is needed, set the current position just
       past the scanned token so that any white space will be correctly
       picked up before the next token. */
    curr_char_loc = end_of_curr_token+1;
  }  /* if */
  if (delete_source_from_loc_was_set_on_entry) {
    /* delete_source_from_loc was non-NULL on entry to macro_invocation, and
       needs to be set to the current position (the token after the
       macro invocation) on exit.  This is a very unusual case that comes
       up when an #if or the like appears within a macro invocation and
       contains another macro invocation. */
    delete_source_from_loc = curr_char_loc;
  } else {
    /* Normal case: clear delete_source_from_loc. */
    delete_source_from_loc = NULL;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "Remaining source after expansion of macro %s (*rescan = %s):\n",
            macro_symbol->header->identifier, *rescan ? "TRUE" : "FALSE");
    print_markered_text(curr_char_loc, (sizeof_t)-1, TRUE);
    fputc('\n', f_debug);
    fprintf(f_debug, "macro_buffer size = %d\n",
                     (int)(next_avail_in_macro_buffer - macro_buffer));
  }  /* if */
#endif /* DEBUG */
  macro_depth = saved_macro_depth;
  in_macro_arg_list = saved_in_macro_arg_list;
  macro_name_modif_seq = saved_macro_name_modif_seq;
  single_param_macro = saved_single_param_macro;
  num_macro_invocations_in_process--;
  /* Restore the lexical state. */
  pop_lexical_state_stack();
  db_exit();
  return (ctoken);
}  /* macro_invocation */


static sizeof_t id_matches_macro_param_name(a_macro_param_ptr param_list,
                                            a_macro_param_ptr *param_ptr)
/*
Look to see if the current token (an identifier) matches any of the macro
parameters on the given list.  If not, return 0.  If so, set *param_ptr
to point to the parameter entry and return the parameter number (the
first parameter is numbered 1).
*/
{
  a_macro_param_ptr pp;
  sizeof_t          num;
  sizeof_t          pnum;

  pnum = num = 0;
  *param_ptr = NULL;
  for (pp = param_list; pp != NULL; pp = pp->next) {
    num++;
    if (*start_of_curr_token == pp->name[0] &&    /* Test for speed. */
        len_of_curr_token == strlen(pp->name) &&
        strncmp(start_of_curr_token, pp->name,
                size_t_arg(len_of_curr_token)) == 0) {
      /* The identifier matches a macro parameter. */
      pnum = num;
      *param_ptr = pp;
      break;
    }  /* if */
  }  /* for */
  return pnum;
}  /* id_matches_macro_param_name */


static a_token_kind mdefn_get_token(a_macro_param_ptr param_list,
                                    sizeof_t          *param_num,
                                    a_macro_param_ptr *param_ptr,
                                    a_boolean         *any_white_space_skipped)
/*
A functional analogue of get_token, which checks identifiers to see if
they are macro parameters on the given list.  If so, *param_num is set
to the parameter number (the first parameter is numbered 1) and *param_ptr
is set to point to the parameter entry.  Otherwise (including when the
token is not an identifier), *param_num is set to 0 and *param_ptr is
set to NULL.  Return *any_white_space_skipped == TRUE if any white
space was skipped before the token.  This routine is used while
fetching the replacement text of macro definitions.  This routine also
implements the cpp practice of finding macro arguments within the text
of string literals and character constants (see end_of_cpp_string,
start_of_white_space_in_cpp_string).
*/
{
  *param_num = 0;
  *param_ptr = NULL;
  /* If we have already reached the tok_newline (probably because of
     an error), do not get another token. */
  if (curr_token != tok_newline) {
    /* If we are scanning in pcc mode, scan string literals and
       character constants as quoting characters surrounding a sequence
       of preprocessor tokens.  This implements the cpp replacement of
       macro parameters within strings and character constants.
       end_of_cpp_string is non-NULL (and points at the end of the
       string) when we are inside a cpp string. */
    if (end_of_cpp_string != NULL) {
      /* Inside a cpp string.  Skip the white space "manually"
         to avoid problems with things that look like comments.  Remember
         the start location of that white space. */
      start_of_white_space_in_cpp_string = curr_char_loc;
      /* Skip white space characters.  All are allowed without error --
         this is after all inside a string, not in plain text of the
         macro definition.  Note that newline has been transformed to
         an LE_NEWLINE lexical escape sequence, and therefore will
         not be seen as white space here. */
      while (isspace((unsigned char)*curr_char_loc)) curr_char_loc++;
      *any_white_space_skipped = (curr_char_loc !=
                                  start_of_white_space_in_cpp_string);
      if (*curr_char_loc == '"' || *curr_char_loc == '\'') {
quote_process:
        /* A quoting character within (or surrounding) a cpp string.
           Return a special one-character token.  If this is the closing 
	   quote of the string, reset end_of_cpp_string to NULL. */
        curr_token = tok_cpp_quote;
        start_of_curr_token = end_of_curr_token = curr_char_loc;
        len_of_curr_token = 1;
        if (curr_char_loc == end_of_cpp_string) end_of_cpp_string = NULL;
        curr_char_loc++;
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "mdefn_get_token:       cpp quote , \"%.*s\"\n",
                           (int)len_of_curr_token, start_of_curr_token);
        }  /* if */
#endif /* DEBUG */
      } else if (*curr_char_loc == '/' && *(curr_char_loc+1) == '*') {
        /* The sequence / * inside a string should not be interpreted as
            a comment. */
        curr_token = tok_divide;
        start_of_curr_token = end_of_curr_token = curr_char_loc;
        len_of_curr_token = 1;
        curr_char_loc++;
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "mdefn_get_token:       slash     , \"%.*s\"\n",
                           (int)len_of_curr_token, start_of_curr_token);
        }  /* if */
#endif /* DEBUG */
      } else if (*curr_char_loc == 'L' &&
                 (*(curr_char_loc+1) == '"' || *(curr_char_loc+1) == '\'')) {
        /* This would look like the start of a wide string literal or
           wide character constant, so pick up the initial "L" manually
           as an identifier. */
        curr_token = tok_identifier;
        start_of_curr_token = end_of_curr_token = curr_char_loc;
        len_of_curr_token = 1;
        curr_char_loc++;
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "mdefn_get_token:       identifier, \"%.*s\"\n",
                           (int)len_of_curr_token, start_of_curr_token);
        }  /* if */
#endif /* DEBUG */
      } else {
        /* Any other case within a cpp string -- get a token. */
        (void)get_token();
      }  /* if */
    } else {
      /* Not inside a cpp string. */
      start_of_white_space_in_cpp_string = NULL;
      if (SVR4_C_mode &&
          curr_char_loc[0] == '/' && curr_char_loc[1] == '*' &&
          curr_char_loc[2] == '*' && curr_char_loc[3] == '/' &&
          !isspace((unsigned char)curr_char_loc[4])) {
        /* In SVR4 C mode, an empty comment is treated as a token pasting
           operator. */
        *any_white_space_skipped = FALSE;
        curr_token = tok_paste;
        start_of_curr_token = curr_char_loc;
        end_of_curr_token = curr_char_loc+3;
        len_of_curr_token = 4;
        curr_char_loc += 4;
        conv_line_loc_to_source_pos(start_of_curr_token, &pos_curr_token);
        pos_warning(ec_svr4_token_pasting_comment, &pos_curr_token);
      } else {
        /* Normal case -- get a token.  Explicitly skip any white space
           preceding the token so that we can know whether or not there was
           any. */
        macro_skip_white_space(*any_white_space_skipped);
        (void)get_token();
      }  /* if */
    }  /* if */
    /* If the token scanned is an identifier, see if it is a macro name. */
    if (curr_token == tok_identifier) {
      *param_num = id_matches_macro_param_name(param_list, param_ptr);
      if (*param_num == 0) {
        /* This is not a macro parameter.  Hence if it is spelled __VA_ARGS__
           and variadic macros are recognized, this is an error. */
        check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
      }  /* if */
    } else if (pcc_preprocessing_mode &&
               end_of_cpp_string == NULL &&
               (curr_token == tok_char_constant ||
                curr_token == tok_string_literal) &&
               *start_of_curr_token != 'L') {
      /* Start of a string in cpp mode.  Remember the end location, then
         rescan the quoting characters and insides as individual tokens.
         The check for "L" above is to rule out wide literals, which
         would not appear in true cpp-compatible source. */
      end_of_cpp_string = end_of_curr_token;
      curr_char_loc = start_of_curr_token;
      goto quote_process;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      if (curr_token == tok_identifier) {
        fprintf(f_debug, "*param_num = %d\n", (int)(*param_num));
      }  /* if */
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  return (curr_token);
}  /* mdefn_get_token */


/*
Put the start of a replacement text section into the macro buffer.  It
consists of a one-byte kind and a multi-byte number (section length or
argument number).
*/
#define put_start_of_section(kind, number)                            \
{ ensure_macro_buffer_space(1+NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER); \
  *next_avail_in_macro_buffer++ = (char)(kind);                         \
  put_macro_repl_text_number(number, next_avail_in_macro_buffer);     \
}  /* put_start_of_section */

/*
Similar to put_start_of_section, but for non-text sections.  Terminates
any text section underway.
*/
#define put_start_of_non_text_section(kind, number)                   \
{ curr_text_section = NULL;                                           \
  put_start_of_section(kind, number);                                 \
}  /* put_start_of_non_text_section */


static void put_raw_text(a_const_char *str,
                         sizeof_t     length,
                         char         **curr_text_section)
/*
Put the given raw-text string into the macro buffer.
next_avail_in_macro_buffer is the current output position.
*curr_text_section points to the first byte of the current
text section, if there is one, or is NULL otherwise.
*/
{
  char     *rtp;
  sizeof_t sect_len;

  if (*curr_text_section == NULL) {
    /* There is no current text section.  Start a new text section. */
    *curr_text_section = next_avail_in_macro_buffer;
    /* The length is specified as zero; it will be incremented below. */
    put_start_of_section(rt_text, 0);
  }  /* if */
  ensure_macro_buffer_space(length);
  (void)memcpy(next_avail_in_macro_buffer, str, size_t_arg(length));
  next_avail_in_macro_buffer += length;
  /* Increment number of characters in current text section. */
  rtp = *curr_text_section+1;
  get_macro_repl_text_number(sect_len, rtp);
  sect_len += length;
  rtp = *curr_text_section+1;
  put_macro_repl_text_number(sect_len, rtp);
}  /* put_raw_text */


/*
Put a raw-text string (part of a macro definition) into the macro
buffer.  The character will be added to the end of the current text 
section, if there is one, or a new text section will be begun if necessary.
*/
#define put_text_to_macro_buffer(str, length)                         \
{ put_raw_text(str, length, &curr_text_section); }


static char *macro_param_name(sizeof_t        number,
                              a_macro_def_ptr mdp)
/*
Return a pointer to a null-terminated string for the name of the number-th
parameter of the indicated macro (1-origined).
*/
{
  a_macro_param_ptr pp;
  /*lint --e{441} loop variable not used in second expression*/
  for (pp = mdp->param_list; --number > 0; pp = pp->next) {}
  return pp->name;
}  /* macro_param_name */


static void make_definition_string(a_symbol_ptr macro_sym)
/*
Create a string in the temp_text_buffer representing the definition of the
macro described by macro_sym, i.e., "#define <name> <replacement>".
*/
{
  a_macro_def_ptr      mdp = macro_sym->variant.macro_def;
  a_macro_param_ptr    pp;
  a_repl_text_seq_kind rts_kind;
  sizeof_t             rts_number;
  char                 *ptr;

  pos_in_temp_text_buffer = 0;
  if (mdp->repl_text == NULL) {
    /* Predefined macros, like __LINE__ and __FILE__, whose replacement text
       varies between invocations, have a NULL replacement text and will be
       identified in the IL by an empty string (not even "#define"). */
  } else {
    /* Put out #define. */
    put_str_to_temp_text_buffer("#define ");
    /* Put out the macro name. */
    put_str_to_temp_text_buffer(macro_sym->header->identifier);
    /* If the macro is function-like, put out the parameters. */
    if (!mdp->object_like) {
      put_ch_to_temp_text_buffer('(');
      for (pp = mdp->param_list; pp != NULL; pp = pp->next) {
        /* Put out a macro parameter name. */
        if (mdp->variadic && pp->next == NULL) {
          /* This is a variadic parameter (declared with an ellipsis). */
          if (extended_variadic_macros_allowed) {
            /* Generate a variadic macro name as in "M(x, y, z...)". */
            put_str_to_temp_text_buffer(pp->name);
          }  /* if */
          put_str_to_temp_text_buffer("...");
        } else {
          /* The normal case of a nonvariadic macro parameter. */
          put_str_to_temp_text_buffer(pp->name);
        }  /* if */
        /* There are more parameters, so put out a comma separator. */
        if (pp->next != NULL) put_ch_to_temp_text_buffer(',');
      }  /* for */
      put_ch_to_temp_text_buffer(')');
    }  /* if */
    put_ch_to_temp_text_buffer(' ');
    /* Put out the macro body, converting from the internal form to a plain
       string. */
    for (ptr = mdp->repl_text; *ptr != (int)rt_null;) {
      rts_kind = (a_repl_text_seq_kind)*(ptr++);
      /* Extract the section length or argument number. */
      get_macro_repl_text_number(rts_number, ptr);
      switch (rts_kind) {
        case rt_text:
          /* Raw text.  rts_number gives its length.  Copy the text, ignoring
             end-of-token markers. */
          /*lint --e{850} rts_number modified in loop */
          for (; rts_number > 0; rts_number--) {
            char ch = *ptr++;
            if (ch == LE_ESCAPE) {
              if (*ptr == LE_NULL) {
                /* Null (zero) character in line. */
                put_ch_to_temp_text_buffer('\0');
              } else {
                /* End of token marker, ignored. */
                check_assertion_str(*ptr == LE_END_OF_TOKEN,
                                    "make_il_macro_entry: bad lexical escape");
              }  /* if */
              ptr++;
              rts_number--;
            } else {
              put_ch_to_temp_text_buffer(ch);
            }  /* for */
          }  /* for */
          break;
        case rt_raw_argument:
        case rt_microsoft_maybe_raw_argument:
          /* parameter ## normal or parameter ## parameter, or pcc-mode
             parameter. */
          put_str_to_temp_text_buffer(macro_param_name(rts_number, mdp));
          break;
        case rt_paste:
          /* ## placeholder.  Add spaces to preserve the correct
             tokenization for a definition like "# ## #". */
          put_str_to_temp_text_buffer(" ## ");
          break;
        case rt_stringized_raw_argument:
        case rt_charized_raw_argument:
          /* #parameter or #@parameter */
          put_str_to_temp_text_buffer(
            rts_kind == rt_charized_raw_argument ? (char *)"#@" : (char *)"#");
          put_str_to_temp_text_buffer(macro_param_name(rts_number, mdp));
          break;
        case rt_argument:
          /* Simple parameter name. */
          put_str_to_temp_text_buffer(macro_param_name(rts_number, mdp));
          break;
        case rt_microsoft_magic_arg_marker:
          /* Implicit in the definition -- no textual representation. */
          break;
        default:
          unexpected_condition_str(
                    "make_il_macro_entry: bad text section kind in macro def");
      }  /* switch */
    }  /* for */
  }  /* if */
  /* Terminate the string. */
  put_ch_to_temp_text_buffer('\0');
}  /* make_definition_string */


void gen_pp_output_for_macro_definitions(void)
/*
Write a definition line for each currently-defined macro to the
preprocessing output file.
*/
{
  a_scope_pointers_block_ptr file_scope_pointers =
                    assoc_pointers_block_of(&scope_stack[DEPTH_OF_FILE_SCOPE]);
  a_symbol_ptr               sym;

  /* Display the predefined and command-line macros, which are in the list
     of symbols with no scope. */
  for (sym = symbols_with_no_scope; sym != NULL; sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_macro &&
        sym->variant.macro_def->repl_text != NULL &&
        !sym->variant.macro_def->ref_suppresses_pch_file &&
        sym != line_macro_symbol && sym != file_macro_symbol &&
        sym != base_file_macro_symbol) {
      /* Create a string version of the macro and display it to the
         preprocessing output file. */
      make_definition_string(sym);
      fprintf(f_pp_output, "%s\n", temp_text_buffer);
    }  /* if */
  }  /* for */
  /* Display the macros created via #define, which are in the file scope
     list. */
  for (sym = file_scope_pointers->symbols; sym != NULL;
       sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_macro) {
      /* Create a string version of the macro and display it to the
         preprocessing output file. */
      make_definition_string(sym);
      fprintf(f_pp_output, "%s\n", temp_text_buffer);
    }  /* if */
  }  /* for */
}  /* gen_pp_output_for_macro_definitions */

#if RECORD_MACROS_IN_IL

static a_macro_ptr make_il_macro_entry(a_symbol_ptr          macro_sym,
                                       a_source_position_ptr macro_pos)
/*
Create an IL entry for the macro described by macro_sym.  The macro has
source position *macro_pos.  The IL entry contains a string version of
the macro definition.  The return value is a pointer to the newly-created
IL entry.
*/
{
  a_macro_def_ptr      mdp = macro_sym->variant.macro_def;
  a_macro_ptr          mp;
  char                 *ptr;

  /* Make a string for the macro in temp_text_buffer. */
  make_definition_string(macro_sym);
  /* Allocate an IL area of the right size and copy the string into it. */
  ptr = alloc_il((sizeof_t)(pos_in_temp_text_buffer));
  (void)strcpy(ptr, temp_text_buffer);
  /* Allocate and fill in the IL macro entry. */
  mp = alloc_macro();
  mp->text = ptr;
  mp->source_corresp.decl_position = *macro_pos;
  set_source_corresp(&mp->source_corresp, macro_sym);
  mdp->macro = mp;
  mp->is_command_line_definition =
                                  (curr_cmd_line_or_predef_macro_def != NULL &&
                                   !processing_predefined_macro);
  mp->is_predefined = mdp->is_predefined;
  mp->object_like = mdp->object_like;
  /* Add the macro to the IL list. */
  add_to_macros_list(mp);
  return mp;
}  /* make_il_macro_entry */

#endif /* RECORD_MACROS_IN_IL */

#if DEBUG
static void db_dump_macro_def(a_symbol_ptr      assoc_symbol,
                              a_boolean         object_like,
                              a_macro_param_ptr param_list,
                              char              *buffer_start)
/*
This function dumps information about a freshly scanned macro to f_debug.
The symbol associated with the macro is *assoc_symbol.  If the macro is not
function-like, object_like is TRUE.  If the macro is function-like, its
parameters (if any) are pointed to by param_list.  buffer_start points to the
beginning of the encoding of the replacement list.
*/
{
  a_macro_param_ptr pp;
  sizeof_t          param_num;

  if (debug_level >= 3) {
    char *temp_ptr;  /* Doesn't need to be registered. */
    a_repl_text_seq_kind rts_kind;
    sizeof_t             rts_number;
    fprintf(f_debug, "Definition of macro %s:\n",
                     assoc_symbol->header->identifier);
    if (object_like) {
      fprintf(f_debug, "object-like\n");
    } else {
      fprintf(f_debug, "function-like, parameter list:\n");
      for (pp = param_list, param_num = 1; pp != NULL;
           pp = pp->next, param_num++) {
        fprintf(f_debug, "  (%d) %s%s%s\n", (int)param_num, pp->name,
                pp->need_expanded_form ? " (need expanded form)" : "",
                pp->is_operand_of_paste ? " (is_operand_of_paste)" : "");
      }  /* for */
    }  /* if */
    fprintf(f_debug, "replacement text:\n");
    for (temp_ptr = buffer_start; *temp_ptr != (int)rt_null;) {
      rts_kind = (a_repl_text_seq_kind)*(temp_ptr++);
      /* Extract the section length or argument number. */
      get_macro_repl_text_number(rts_number, temp_ptr);
      switch (rts_kind) {
        case rt_text:
          fputs("  raw text: \"", f_debug);
          print_markered_text(temp_ptr, rts_number, FALSE);
          fputs("\"\n", f_debug);
          temp_ptr += rts_number;
          break;
        case rt_raw_argument:
          fprintf(f_debug, "  raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_paste:
          fprintf(f_debug, "  ##\n");
          check_assertion(rts_number == 0);
          break;
        case rt_stringized_raw_argument:
          fprintf(f_debug, "  stringized raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_charized_raw_argument:
          fprintf(f_debug, "  charized raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_argument:
          fprintf(f_debug, "  expanded argument %lu\n",
                           (unsigned long)rts_number);
          break;
        case rt_microsoft_magic_arg_marker:
          fprintf(f_debug, "  magic arg marker\n");
          check_assertion(rts_number == 0);
          break;
        case rt_microsoft_maybe_raw_argument:
          fprintf(f_debug, "  maybe raw argument %lu\n",
                           (unsigned long)rts_number);
          break;
        default:
          unexpected_condition_str2("db_dump_macro_def:",
                                    "bad section kind in macro def");
      }  /* switch */
    }  /* for */
    fprintf(f_debug, "  end\n");
  }  /* if */
}  /* db_dump_macro_def */
#endif /* DEBUG */

static a_boolean equiv_replacement_text(char		*repl_text,
					sizeof_t	repl_text_length,
					a_macro_def_ptr	mdp)
/*
Return TRUE if the replacement text specified by repl_text, with a length
of repl_text_length, is the same as that of the macro definition mdp,
ignoring extra LE_END_OF_TOKEN escapes appearing in repl_text.  Note that
repl_text_length does not include the rt_null terminator.
*/
{
  a_boolean	result = FALSE;

  check_assertion(repl_text != NULL);
  if (mdp->repl_text != NULL) {
    /* Step through the two replacement text strings, skipping over
       LE_END_OF_TOKEN escapes that appear in repl_text but not in the
       original replacement text, and ignoring the respective lengths of
       rt_text sections (which can be different because of the presence of
       LE_END_OF_TOKEN escapes).  (This difference can occur because of
       predefined macros inserted programmatically, which typically will
       not have LE_END_OF_TOKEN escapes, and those coming from the
       predefined macro file, which are handled via proc_define and thus
       might have them.) */
    sizeof_t  orig_idx;
    sizeof_t  new_idx;
    a_boolean mismatch_seen = FALSE;
    for (orig_idx = 0, new_idx = 0;
         !mismatch_seen && new_idx < repl_text_length;
         ++orig_idx, ++new_idx) {
      char ch = repl_text[new_idx];
      if (ch != mdp->repl_text[orig_idx]) {
        if (ch == LE_ESCAPE &&
            repl_text[new_idx + 1] == LE_END_OF_TOKEN) {
          /* An LE_END_OF_TOKEN escape in repl_text.  Skip over it and
             check that the following character matches. */
          new_idx += LE_ESCAPE_LEN;
          mismatch_seen = (repl_text[new_idx] != mdp->repl_text[orig_idx]);
        } else {
          mismatch_seen = TRUE;
        }  /* if */
      } else if (ch == (char)rt_text) {
        /* Don't check the length of rt_text sections, which can be
           different because of extra LE_END_OF_TOKEN escapes.  A real
           mismatch causing a length difference will be caught while
           comparing the text itself. */
        new_idx += 3;
        orig_idx += 3;
      } else if (ch == (char)rt_raw_argument ||
                 ch == (char)rt_stringized_raw_argument ||
                 ch == (char)rt_charized_raw_argument ||
                 ch == (char)rt_argument ||
                 ch == (char)rt_microsoft_maybe_raw_argument) {
        /* Check to ensure the argument number matches, then skip over
           it. */
        mismatch_seen =
                    !(repl_text[new_idx + 1] == mdp->repl_text[orig_idx + 1] &&
                      repl_text[new_idx + 2] == mdp->repl_text[orig_idx + 2] &&
                      repl_text[new_idx + 3] == mdp->repl_text[orig_idx + 3]);
        new_idx += 3;
        orig_idx += 3;
      } else if (ch == (char)rt_paste ||
                 ch == (char)rt_microsoft_magic_arg_marker) {
        /* These take no parameter, so a dummy value of 0 was added.  Skip
           over it. */
        new_idx += 3;
        orig_idx += 3;
      } else if (ch == LE_ESCAPE) {
        /* Check to make sure it's the same escape, then skip over it. */
        mismatch_seen = repl_text[new_idx + 1] != mdp->repl_text[orig_idx + 1];
        ++new_idx;
        ++orig_idx;
      }  /* if */
    }  /* for */
    result = (!mismatch_seen &&
              mdp->repl_text[orig_idx] == (char)rt_null);
  }  /* if */
  return result;
}  /* equiv_replacement_text */


a_symbol_ptr proc_define(void)
/*
Scan and process a #define directive.
*/
{
  sizeof_t	  repl_text_len;
  char		  *repl_text;
  a_macro_def_ptr mdp;
  a_symbol_ptr	  assoc_symbol = NULL;
  a_boolean       any_white_space_skipped;
  sizeof_t	  param_num;
  sizeof_t	  save_param_num;
  sizeof_t	  n_params = 0;
  a_macro_param_ptr
		  param_ptr,
		  save_param_ptr,
		  pp,
		  pp2,
		  last_param,
		  param_list;
  a_boolean	  object_like;
  a_boolean	  variadic = FALSE;
  a_boolean	  redefinition = FALSE;
  a_source_position
                  start_pos;
  a_boolean       need_end_of_token_marker;
  a_token_kind    prev_token = tok_error;
  static char     str_end_of_token_marker[LE_ESCAPE_LEN] =
                                                { LE_ESCAPE, LE_END_OF_TOKEN };
#if RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL
  a_macro_ptr     mp;
#endif /* RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIIONS_IN_IL */
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_text_map_position_tracker
                  tracker;
  sizeof_t        next_targ_offset;
  sizeof_t        first_text_map_entry = macro_text_map.num_entries;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
                  start_of_replacement;
  a_source_position
                  end_of_replacement;
#endif /* RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean         discard_new_definition = FALSE;

  /* WATCH OUT: Pointers into macro_buffer or the raw_text of a macro arg
     are dangerous, since those things can be reallocated.  Such pointers
     must be registered by calling register_pointer_variable so that they
     can be updated on any reallocation. */
  char		  *curr_text_section;
  a_pointer_registration
                  curr_text_section_reg;
  char            *buffer_start;
  a_pointer_registration
		  buffer_start_reg;
  a_pointer_registration_ptr
		  save_registered_pointers = registered_pointers;

  register_pointer_variable(curr_text_section, curr_text_section_reg);
  register_pointer_variable(buffer_start, buffer_start_reg);

  db_enter(3, "proc_define");
  scanning_macro_name = TRUE;
  (void)get_token();
  scanning_macro_name = FALSE;
  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    /* Expected an identifier. */
    pos_error(ec_exp_identifier, &error_position);
    some_error_in_curr_directive = TRUE;
  } else {
    /* Get the canonical spelling of the identifier. */
    a_const_char *id_ptr = start_of_curr_token;
    sizeof_t     id_len = len_of_curr_token;
    if (id_contains_ucn_or_multibyte_char) {
      id_ptr = make_canonical_identifier(start_of_curr_token, &id_len,
                                         /*force_ucn=*/FALSE);
    }  /* if */
    /* The macro name __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(id_len, id_ptr);
    /* Look to see if there is a macro with this name. */
    /* find_defined_macro cannot be used because if we have "#define defined"
       we want to give an error, not ignore it. */
    assoc_symbol = find_macro_symbol_by_name(id_ptr, id_len,
	                                     &locator_for_curr_id);
    if (assoc_symbol != NULL) {
      /* This is an attempt to redefine an already-defined symbol;
         the validity of this attempt is determined below once the
         definition has been processed. */
      redefinition = TRUE;
    } else if (!is_error_locator(locator_for_curr_id)) {
      a_scope_depth  scope_depth;
      /* Enter the macro symbol.  assoc_symbol remains NULL if an error
         locator is being used.  This suppresses the creation of a
         macro IL entry.  It also prevents us from calling mark_defined
         on an error symbol. */
      copy_source_position(pos_curr_token,
                           locator_for_curr_id.source_position);
      /* The macro symbol is entered in file scope, unless it is a macro
         resulting from a "-D" command-line option or from the predefined
         macro file. */
      if (depth_scope_stack == NO_SCOPE_DEPTH) {
        scope_depth = NO_SCOPE_DEPTH;
      } else {
        scope_depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
      assoc_symbol = enter_symbol((a_symbol_kind)sk_macro,
                                  &locator_for_curr_id,
                                  scope_depth,
                                  /*suppress_error=*/TRUE);
    }  /* if */
    param_list = last_param = NULL;
    /* See if this definition has a parameter list. */
    /* Note that the test here is not done on a token, because there can
       be no white space between the identifier and the "(". */
    if (*curr_char_loc != '(') {
      /* Object-like macro definition (no parameters). */
      object_like = TRUE;
      if (C_mode() && strict_ansi_mode) {
        /* Technical Corrigendum number 1 for ISO C requires a diagnostic
           if the first character of an object-like macro replacement list
           is a nonstandard character (one not required by 5.2.1). */
        /* Watch out for the newline represented by a lexical escape
           sequence. */
        if (curr_char_loc[0] != LE_ESCAPE &&
            is_nonstandard_character(*curr_char_loc)) {
          a_source_position err_pos = null_source_position;
          conv_line_loc_to_source_pos(curr_char_loc, &err_pos);
          pos_error(ec_nonstd_character_at_start_of_macro_def, &err_pos);
        }  /* if */
      }  /* if */
    } else {
      /* Function-like.  Scan parameter list. */
      object_like = FALSE;
      /* Get, then advance past, the "(". */
      (void)get_token();
      (void)get_token();
      add_stop_token(tok_rparen);
      param_num = 0;
      /* Test for empty parameter list. */
      if (curr_token != tok_rparen) {
        /* Not empty. */
        add_stop_token(tok_comma);
        do {
          /* Scan one parameter identifier or a terminating ellipsis (for
             variadic macros), and build an entry for it. */
          a_boolean is_variadic_parameter = variadic_macros_allowed &&
                                            (curr_token == tok_ellipsis);
          if (!is_variadic_parameter && curr_token != tok_identifier) {
            (void)required_token(tok_identifier, ec_exp_identifier);
          } else if (!is_variadic_parameter &&
                     id_matches_macro_param_name(param_list, &param_ptr)) {
            /* Duplicate parameter name. */
            pos_error(ec_duplicate_macro_param_name, &error_position);
            (void)get_token();
          } else {
            /* Remember the position of the identifier in case we need to
               issue an error. */
            a_source_position err_pos;
            err_pos = pos_curr_token;
            /* Add the parameter to the list. */
            param_num++;
            pp = alloc_macro_param();
            if (!is_variadic_parameter) {
              pp->name = alloc_fe((sizeof_t)(len_of_curr_token+1));
              (void)memcpy(pp->name, start_of_curr_token,
                           size_t_arg(len_of_curr_token));
              pp->name[len_of_curr_token] = '\0';
            } else {
              /* A variadic parameter named "..." in the parameter list is
                 referred to as "__VA_ARGS__" in the replacement list. */
              variadic = TRUE;
              pp->name = alloc_fe(sizeof("__VA_ARGS__"));
              (void)memcpy(pp->name, "__VA_ARGS__", sizeof("__VA_ARGS__"));
            }  /* if */
#if DEBUG
            param_name_string_space += (unsigned long)(strlen(pp->name)+1);
#endif /* DEBUG */
            if (param_list == NULL) {
              param_list = pp;
            } else {
              /* Link the last entry to this new entry. */
              last_param->next = pp;
            }  /* if */
            last_param = pp;
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "macro parameter %d: %s\n",
                               (int)param_num, pp->name);
            }  /* if */
#endif /* DEBUG */
            /* Eat the identifier or ellipsis that names the parameter: */
            (void)get_token();
            /* A parameter of the form "id ..." is also possible and means
               "id" is a variadic parameter (ordinarily, just "..." is used
               and the implied name is "__VA_ARGS__"). */
            if (extended_variadic_macros_allowed && !variadic &&
                curr_token == tok_ellipsis) {
              variadic = TRUE;
              (void)get_token();
            }  /* if */
            if (!variadic && variadic_macros_allowed &&
                strcmp(pp->name, "__VA_ARGS__") == 0) {
              /* If variadic macros are allowed, macro parameters explicitly
                 called __VA_ARGS__ should be refused (except the variadic
                 parameter itself). */
              pos_error(ec_VA_ARGS_not_allowed, &err_pos);
            }  /* if */
          }  /* if */
        } while (!variadic && loop_token(tok_comma));
        remove_stop_token(tok_comma);
        n_params = param_num;
      }  /* if */
      /* Check for closing parenthesis.  required_token is not used because
         the get_token must be done in a special way, via mdefn_get_token. */
      if (curr_token != tok_rparen) {
        pos_error(ec_exp_rparen, &error_position);
      }  /* if */
      remove_stop_token(tok_rparen);
    }  /* if */
    /* If we're scanning a command-line macro definition option, then the
       next character should be a "=" (or, in GNU mode, a " ").  Skip
       it. */
    if (curr_cmd_line_or_predef_macro_def != NULL &&
        !processing_predefined_macro) {
      if (gnu_mode) {
        /* The command-line syntax of the GNU preprocessor has an optional
           "=", optionally preceded by one or more spaces, before the
           replacement text.  To simplify processing here,
           process_command_line_macro_definitions replaced the "=", if
           present, with a space, so we only have to deal with spaces at
           this point. */
        skip_white_space();
        if (kind_of_white_space_skipped == 0) {
          /* The GNU preprocessor accepts command-line definitions of the
             form -DX3.9 with only a warning, treating the macro name as
             "X3".  We must set curr_cmd_line_or_predef_macro_def to NULL
             before issuing the diagnostic to avoid treating this warning
             as a catastrophic command-line error. */
          a_const_char *saved_command_line_macro_def =
                                             curr_cmd_line_or_predef_macro_def;
          curr_cmd_line_or_predef_macro_def = NULL;
          str_warning(ec_equals_assumed_in_cmd_line_macro_def,
                      locator_for_curr_id.symbol_header->identifier);
          curr_cmd_line_or_predef_macro_def = saved_command_line_macro_def;
        }  /* if */
      } else if (*curr_char_loc != '=') {
        str_command_line_error(ec_bad_cmd_line_macro,
                               curr_cmd_line_or_predef_macro_def);
      } else {
        ++curr_char_loc;
      }  /* if */
    }  /* if */
    /* Scan the replacement-list as tokens, and place in the buffer; then
       allocate space for the text, and build the a_macro_def entry. */
    /* Do not reset the next_avail_in_macro_buffer pointer if there
       is text saved in the macro buffer to be inserted at the beginning of
       the preprocessed output line.  This happens when a macro identifier
       immediately precedes a #define and the #define is encountered while
       looking for the parenthesis following the macro name.  Also do
       not reset the macro buffer if this #define appears within a macro
       argument list. */
    if (line_start_source_line_modif == NULL && macro_depth == 0) {
      next_avail_in_macro_buffer = macro_buffer;
      num_chars_deleted_in_macro_buffer = 0;
      num_compacted_macro_buffer_chars = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
      macro_text_map.num_entries = 0;
      first_text_map_entry = 0;
      /* There shouldn't be any leftover text map position trackers at this
         point. */
      check_assertion(active_text_map_position_trackers == NULL);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
    }  /* if */
    buffer_start = begin_macro_buffer_region();
    /* Not inside a cpp string. */
    end_of_cpp_string = NULL;
    /* Last section in replacement text is not raw text. */
    curr_text_section = NULL;
    /* Get first token of the replacement text. */
    (void)mdefn_get_token(param_list, &param_num, &param_ptr,
                          &any_white_space_skipped);
    if (curr_cmd_line_or_predef_macro_def == NULL &&
        curr_token != tok_newline && object_like && !any_white_space_skipped) {
      /* In C99 and C++11, an object-like macro definition must have white
         space between the macro name and the replacement list: issue an
         error in strict mode and a warning in all other modes. */
      pos_st_diagnostic((strict_ansi_mode && (c99_mode || cpp11_mode)) ?
                        strict_ansi_discretionary_severity : es_warning,
                        ec_white_space_required_after_macro_name,
                        &pos_curr_token,
                        locator_for_curr_id.symbol_header->identifier);
    }  /* if */
    /* Ignore leading white space.  See standard, 3.8.3, semantics. */
    any_white_space_skipped = FALSE;
    need_end_of_token_marker = FALSE;
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Initialize the token position tracker (but not if this is a
       command-line definition, which will be completely handled at the end of
       processing, or a disallowed redefinition, which will be ignored). */
    if (curr_cmd_line_or_predef_macro_def == NULL &&
        assoc_symbol != NULL) {
      init_text_map_position_tracker(&tracker, &macro_text_map,
                                     NO_PARENT_MACRO_INVOCATION);
    }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL
    start_of_replacement = pos_curr_token;
    end_of_replacement = end_pos_curr_token;
#endif /* RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL */
    while (curr_token != tok_newline) {
      if (curr_token == tok_paste) {
        /* "##".  Can be preceded and/or followed by a parameter, but
           need not be.  Cannot be first or last in the replacement text.
           See standard, 3.8.3.3.  If the "##" was preceded by a
           parameter, the parameter has already been handled correctly,
           so that need not be checked for here. */
        /* Any pending end-of-token marker is suppressed. */
        need_end_of_token_marker = FALSE;
        if (next_avail_in_macro_buffer == buffer_start) {
          /* Output buffer is empty, so this is the first token.  Error. */
          pos_error(ec_paste_cannot_be_first, &error_position);
          (void)mdefn_get_token(param_list, &param_num, &param_ptr,
                                &any_white_space_skipped);
        } else {
          /* If the token following the "##" is a parameter, put it out
             as a raw-text substitution.  Otherwise, just let the next
             token be processed on the next iteration of the loop.
             The "##" itself does not appear in the replacement text
             string. */
          if (mdefn_get_token(param_list, &param_num, &param_ptr,
                              &any_white_space_skipped) == tok_newline) {
            pos_error(ec_paste_cannot_be_last, &error_position);
          } else {
            /* Insert a "##" placeholder so that the IL accurately reflects
               the source. */
            put_start_of_non_text_section(rt_paste, 0);
            if (param_num != 0) {
              /* The token following "##" is a parameter. */
              if (ms_compat && 
                  (prev_token != tok_identifier ||
                   (microsoft_version >= 1400 && variadic &&
                    param_num == n_params))) {
                /* The Microsoft preprocessor normally expands variadic
                   arguments before substitution, even after "##".  It also
                   expands a normal argument if the token before "##" is
                   not an identifier. */
                if (prev_token == tok_lparen || prev_token == tok_comma) {
                  /* An exception to this behavior is when the token is
                     used as an argument to a nested macro and that macro
                     uses the argument as an operand of a paste operation.
                     In that case, the raw version of the macro is used.
                     To allow the decision to be deferred until it is known
                     whether the exception applies, a special sequence
                     containing both the raw and expanded versions will be
                     used. */
                  put_start_of_non_text_section(
                                               rt_microsoft_maybe_raw_argument,
                                               param_num);
                } else {
                  put_start_of_non_text_section(rt_argument, param_num);
                }  /* if */
                param_ptr->need_expanded_form = TRUE;
              } else {
                /* The raw form of the argument will be used. */
                put_start_of_non_text_section(rt_raw_argument, param_num);
              }  /* if */
              param_ptr->is_operand_of_paste = TRUE;
              need_end_of_token_marker = TRUE;
              (void)mdefn_get_token(param_list, &param_num, &param_ptr,
                                    &any_white_space_skipped);
            } else {
              /* Anything other than a parameter.  Delete any white space
                 preceding it. */
              any_white_space_skipped = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        prev_token = curr_token;
        if (need_end_of_token_marker) {
          /* Follow the previous token with an end-of-token marker, so that
             when it is tokenized later, it will always be done in the
             same way it is now.  This is important, for example, in

             #define x(a) ..##a

             x(.) should yield three "." tokens, not the single token "...".
             The markers are also helpful when illegal tokens are present.
             For example, the malformed string literal token in

             #define y() "abc

             should still be an error in

             y()"

             in pcc compatibility mode, the token separators are not put
             out. */
          if (!pcc_preprocessing_mode) {
            put_text_to_macro_buffer(str_end_of_token_marker, LE_ESCAPE_LEN);
          }  /* if */
          need_end_of_token_marker = FALSE;
        }  /* if */
        /* Token is not "##", and not newline.  Put out a raw-text
           blank if the token was preceded by any white space. */
        if (any_white_space_skipped) {
          /* If we are inside a cpp string, put the original white space
             characters (rather than the standardized blank) into the
             raw text.  We don't want to drop blanks and the like inside
             character strings. */
          if (start_of_white_space_in_cpp_string == NULL) {
            /* Not inside a cpp string. */
            put_text_to_macro_buffer(" ", 1);
          } else {
            /* Inside a cpp string. */
            put_text_to_macro_buffer(start_of_white_space_in_cpp_string,
                                     (sizeof_t)(start_of_curr_token -
                                          start_of_white_space_in_cpp_string));
          }  /* if */
          any_white_space_skipped = FALSE;
        }  /* if */
        if ((curr_token == tok_sharp 
#if MICROSOFT_EXTENSIONS_ALLOWED
             || (ms_extensions && curr_token == tok_charize)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            ) && !object_like && end_of_cpp_string == NULL) {
          /* "#" -- Must be followed by a parameter name.  Note that this is
             ignored in an object-like macro.  See standard, 3.8.3.2.
             "#" is recognized in pcc preprocessing mode, but not inside
             of string literals (the test of "end_of_cpp_string" makes sure
             that we don't do this substitution in string literals).
             "#@" -- Recognized in Microsoft mode only: similar to the
             stringizing "#" operator, but it produces a character literal
             instead of a string literal. */
          a_boolean  charize = curr_token != tok_sharp;
          (void)mdefn_get_token(param_list, &param_num, &param_ptr,
                                &any_white_space_skipped);
          if (param_num == 0) {
            pos_error(ec_exp_macro_param, &error_position);
          } else {
            put_start_of_non_text_section(charize ? rt_charized_raw_argument
                                                  : rt_stringized_raw_argument,
                                          param_num);
            need_end_of_token_marker = TRUE;
            (void)mdefn_get_token(param_list, &param_num, &param_ptr,
                                  &any_white_space_skipped);
          }  /* if */
        } else if (param_num != 0) {
          /* This token is a parameter of the macro.  Put it out as
             an expansion of the parameter unless "##" is next, in which
             case put it out as the raw value of the argument. */
          /* In pcc mode, always use the raw form of the argument.  Expansion
             is done on rescan of the macro body. */
          a_boolean is_microsoft_va_args =
                                (ms_compat && microsoft_version >= 1400 &&
                                 variadic && param_num == n_params);
          if (is_microsoft_va_args &&
              next_avail_in_macro_buffer != buffer_start) {
            /* The Microsoft version of variadic macros performs the
               "magic deletion" of a preceding comma even without a "##"
               operator, so we need to mark the __VA_ARGS__ parameter. */
            put_start_of_non_text_section(rt_microsoft_magic_arg_marker, 0);
          }  /* if */
          /* Save information on current token because mdefn_get_token will
             change it. */
          save_param_num = param_num;
          save_param_ptr = param_ptr;
          if (mdefn_get_token(param_list, &param_num, &param_ptr,
                              &any_white_space_skipped) == tok_paste ||
              pcc_preprocessing_mode) {
            if (is_microsoft_va_args) {
              /* The Microsoft compiler expands variadic arguments before
                 substitution.  Note that we do not set
                 need_end_of_token_marker to TRUE here so that the end of
                 the expanded text can form a single token with what
                 follows it. */
              put_start_of_non_text_section(rt_argument, save_param_num);
              save_param_ptr->need_expanded_form = TRUE;
            } else {
              /* The argument will be used in its raw form. */
              put_start_of_non_text_section(rt_raw_argument, save_param_num);
            }  /* if */
            save_param_ptr->is_operand_of_paste = TRUE;
          } else {
            /* Not "##", so put expanded version of argument into string. */
            put_start_of_non_text_section(rt_argument, save_param_num);
            save_param_ptr->need_expanded_form = TRUE;
            need_end_of_token_marker = TRUE;
          }  /* if */
        } else {
          /* Any other tokens -- not special, just put into macro buffer
             as raw text. */
          a_const_char *str = start_of_curr_token;
          sizeof_t     len = len_of_curr_token;
#if FULLY_RESOLVED_MACRO_POSITIONS
          if (curr_cmd_line_or_predef_macro_def == NULL &&
              assoc_symbol != NULL) {
            /* This token is from the source file, so we need to register its
               position in the macro text map.  (Command-line definitions are
               handled all at once at the end of processing the definition,
               and disallowed redefinitions are ignored.) */
            next_targ_offset = next_avail_in_macro_buffer - buffer_start;
            if (curr_text_section == NULL) {
              /* Allow for the section header, which will be added before the
                 token is stored. */
              next_targ_offset += 1+NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER;
            }  /* if */
            add_token_to_macro_text_map(&tracker, next_targ_offset);
          }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if UNICODE_SOURCE_SUPPORTED
          if (curr_token == tok_identifier &&
              id_contains_ucn_or_multibyte_char) {
            /* Translate any extended characters in the identifier to UCNs
               so that the text of the macro can be scanned successfully
               regardless of the encoding of the file in which the macro is
               expanded. */
            str = make_canonical_identifier(start_of_curr_token, &len,
                                            /*force_ucn=*/TRUE);
          }  /* if */
#endif /* UNICODE_SOURCE_SUPPORTED */
          put_text_to_macro_buffer(str, len);
          /* Request an end-of_token marker after this token.  This will be
             put out later unless the next thing is "##" or the end of the
             replacement text. */
          need_end_of_token_marker = TRUE;
          if (ms_compat &&
              len_of_curr_token == 1 && *start_of_curr_token == 'L' &&
              start_of_curr_token[1] == '#') {
            /* In Microsoft mode, L#param can be used to create a wide
               string literal. */
            need_end_of_token_marker = FALSE;
          }  /* if */
          /* Generate a remark on an invalid token.  Suppress this remark if
             inside a string because of looking for parameter names; the
             things inside the string aren't expected to be legal tokens. */
          if (curr_token == tok_error && end_of_cpp_string == NULL) {
            pos_remark(err_code_for_error_token, &error_position);
          }  /* if */
          (void)mdefn_get_token(param_list, &param_num, &param_ptr,
                                &any_white_space_skipped);
        }  /* if */
      }  /* if */
#if RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL
      if (curr_token != tok_newline) {
        end_of_replacement = end_pos_curr_token;
      }  /* if */
#endif /* RECORD_MACROS_IN_IL && EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* while */
    /* Store final terminator.  We've ensured that there is room for this. */
    *next_avail_in_macro_buffer = (char)rt_null;
    /* Not inside a cpp string.  Could still be set if there is an 
       unclosed string. */
    end_of_cpp_string = NULL;
#if DEBUG
    if (assoc_symbol != NULL) {
      db_dump_macro_def(assoc_symbol, object_like, param_list, buffer_start);
    }  /* if */
#endif /* DEBUG */
    mdp = NULL;
    if (redefinition) {
      a_boolean         defs_are_same = TRUE;
      an_error_severity severity;
      an_error_code     code = ec_no_error;
      a_const_char      *saved_macro_def = curr_cmd_line_or_predef_macro_def;
      if (curr_cmd_line_or_predef_macro_def == NULL ||
          processing_predefined_macro ||
          assoc_symbol->variant.macro_def->cannot_be_redefined) {
        /* If the redefinition is from the program text or if it is for a
           predefined symbol, check to see if the new definition is benign,
           i.e., identical to the existing one.  (Redefinitions of
           non-predefined symbols from the command line are always honored,
           so there's no need to check.) */
        sizeof_t new_length = next_avail_in_macro_buffer - buffer_start;
        mdp = assoc_symbol->variant.macro_def;
        if ((a_boolean)mdp->object_like != object_like ||
            (a_boolean)mdp->variadic != variadic ||
            !equiv_replacement_text(buffer_start, new_length, mdp)) {
          defs_are_same = FALSE;
        } else {
          /* Check parameter lists to make sure they match. */
          for (pp = param_list, pp2 = mdp->param_list; defs_are_same;
               pp = pp->next, pp2 = pp2->next) {
            if (pp == NULL || pp2 == NULL) {
              if (pp != pp2) {
                /* One has more parameters than the other. */
                defs_are_same = FALSE;
              }  /* if */
              break;
            }  /* if */
            if (strcmp(pp->name, pp2->name) != 0) {
              defs_are_same = FALSE;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
      if (defs_are_same) {
        /* Redefinitions of non-predefined macros on the command line are
           always honored (without checking that they are identical), but
           all other benign redefinitions are discarded. */
        discard_new_definition = (curr_cmd_line_or_predef_macro_def == NULL ||
                                  processing_predefined_macro ||
                                  assoc_symbol->variant.macro_def->
                                                          cannot_be_redefined);
        if (assoc_symbol->variant.macro_def->cannot_be_redefined &&
            curr_cmd_line_or_predef_macro_def == NULL) {
          /* Even benign redefinitions of predefined symbols from the
             program text are diagnosed. */
          severity = ms_extensions ? es_warning : es_discretionary_error;
          code = ec_cannot_redef_predef_macro;
        } else {
          /* No diagnostics for other redefinitions (including command-line
             benign redefinitions of predefined symbols). */
          severity = es_none;
        }  /* if */
      } else {
        /* This is a non-benign redefinition.  The diagnostic issued and
           whether the redefinition is honored or discarded depend on the
           emulation, whether the redefinition is from program text, the
           command line, or the predefined macros file, and whether the
           symbol was predefined. */
        if (processing_predefined_macro) {
          /* A predefined macro that changes the definition of an existing
             macro is always catastrophic. */
#if DEBUG
          make_definition_string(assoc_symbol);
          fprintf(f_debug, "Previous definition was:\n  %s\n",
                  temp_text_buffer);
#endif /* DEBUG */
          str_catastrophe(ec_bad_predef_macro_redef,
                          assoc_symbol->header->identifier);
        } else if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
          /* A redefinition of a predefined symbol. */
          if (ms_compat) {
            discard_new_definition = TRUE;
            severity = es_warning;
            code = ec_cannot_redef_predef_macro;
          } else if (gnu_mode) {
            discard_new_definition = FALSE;
            severity = es_warning;
            code = ec_predef_macro_redefined;
          } else if (sun_mode) {
            discard_new_definition = TRUE;
            severity = es_discretionary_error;
            code = ec_cannot_redef_predef_macro;
          } else if (curr_cmd_line_or_predef_macro_def == NULL) {
            /* From program text. */
            discard_new_definition = TRUE;
            severity = es_discretionary_error;
            code = ec_cannot_redef_predef_macro;
          } else {
            /* From the command line. */
            discard_new_definition = FALSE;
            severity = es_error;
            code = ec_cannot_redef_predef_macro;
          }  /* if */
        } else {
          /* A non-predefined symbol. */
          discard_new_definition = FALSE;
          code = ec_bad_macro_redef;
          if (curr_cmd_line_or_predef_macro_def != NULL &&
              !processing_predefined_macro) {
            /* An invalid command-line redefinition is always an error. */
            severity = es_error;
          } else if (strict_ansi_mode) {
            severity = strict_ansi_error_severity;
          } else {
            severity = es_warning;
          }  /* if */
        }  /* if */
      }  /* if */
      if (severity != es_none) {
        if ((int)severity < (int)es_error) {
          /* Ensure that warnings are printed and discretionary errors do
             not become catastrophic: */
          curr_cmd_line_or_predef_macro_def = NULL;
        }  /* if */
        pos_sy_diagnostic(severity, code, &start_pos, assoc_symbol);
        curr_cmd_line_or_predef_macro_def = saved_macro_def;
      }  /* if */
      if (discard_new_definition) {
#if FULLY_RESOLVED_MACRO_POSITIONS
        if (curr_cmd_line_or_predef_macro_def == NULL) {
          /* Terminate the tracker (and just abandon the text map entries
             added to macro_text_map: they'll be discarded the next time
             macro_buffer is truncated). */
          terminate_macro_text_map(&tracker,
                                   (sizeof_t)(next_avail_in_macro_buffer -
                                              buffer_start));
        }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
        goto def_done;
      }  /* if */
    }  /* if */
    /* Allocate space for the text, and copy it. */
    repl_text_len = next_avail_in_macro_buffer - buffer_start;
    repl_text = alloc_fe((sizeof_t)(repl_text_len+1));
#if DEBUG
    macro_definition_space += (unsigned long)(repl_text_len+1);
#endif /* DEBUG */
    (void)memcpy(repl_text, buffer_start, size_t_arg(repl_text_len));
    repl_text[repl_text_len] = (char)rt_null;
    if (assoc_symbol != NULL) {
      /* Allocate and fill the macro definition block. */
      if (mdp == NULL) {
        mdp = alloc_macro_def();
      } else {
        /* Reuse an existing macro definition on a non-benign redefinition. */
        clear_macro_def(mdp);
      }  /* if */
      mdp->object_like    = object_like;
      mdp->param_list     = param_list;
      mdp->repl_text      = repl_text;
      mdp->variadic       = variadic;
      mdp->is_predefined  = processing_predefined_macro;
#if FULLY_RESOLVED_MACRO_POSITIONS
      if (curr_cmd_line_or_predef_macro_def == NULL) {
        /* The definition is in the program text, so the text map has been
           built via add_token_to_macro_text_map.  We need to terminate that
           map and then copy it to the one in the macro definition. */
        sizeof_t num_entries;
        terminate_macro_text_map(&tracker, repl_text_len+1);
        num_entries = macro_text_map.num_entries - first_text_map_entry;
        init_macro_text_map(num_entries, &mdp->text_map, /*resizable=*/FALSE);
        (void)memcpy((char *)mdp->text_map.entries,
                     (char *)(macro_text_map.entries + first_text_map_entry),
                     size_t_arg(num_entries * sizeof(a_macro_text_map_entry)));
        mdp->text_map.num_entries = num_entries;
      } else {
        /* The definition came from the command line.  There are no positions
           to map, so we just add a beginning and ending map entry, both
           pointing to the command line. */
        init_macro_text_map(2, &mdp->text_map, /*resizable=*/FALSE);
        add_entry_to_macro_text_map(&mdp->text_map, /*start_of_region=*/0,
                                    (a_seq_number)0, SP_COL_CMD_LINE,
                                    NO_PARENT_MACRO_INVOCATION);
        add_entry_to_macro_text_map(&mdp->text_map, repl_text_len + 1,
                                    (a_seq_number)0, SP_COL_CMD_LINE,
                                    NO_PARENT_MACRO_INVOCATION);
      }  /* if */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
      /* Put the macro def block pointer into the symbol entry. */
      assoc_symbol->variant.macro_def = mdp;
    }  /* if */
def_done:;
    if (assoc_symbol != NULL && !discard_new_definition) {
#if RECORD_MACROS_IN_IL
#if EXTRA_SOURCE_POSITIONS_IN_IL
      /* Make an IL entry for the macro and record the start and end
         positions. */
      mp = make_il_macro_entry(assoc_symbol, &start_pos);
      mp->replacement_text_range.start = start_of_replacement;
      mp->replacement_text_range.end = end_of_replacement;
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Make an IL entry for the macro. */
      (void)make_il_macro_entry(assoc_symbol, &start_pos);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* RECORD_MACROS_IN_IL */
      /* Mark the symbol as defined. */
      assoc_symbol->defined = FALSE;  /* Avoid secondary declarations. */
      mark_defined(assoc_symbol, &start_pos);
    }  /* if */
  }  /* if */
  /* Drop any local pointer registrations. */
  release_macro_buffer_region();
  registered_pointers = save_registered_pointers;
  db_exit();
  return assoc_symbol;
}  /* proc_define */


#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
/*
Data structure to contain definitions of #assert predicates.  (They are an
AT&T System V release 4 preprocessing extension).
*/
typedef struct an_assert_value *an_assert_value_ptr;
typedef struct an_assert_value {
  /* One value of an #assert predicate. */
  an_assert_value_ptr
		next;	/* Pointer to the next value for the same predicate,
			   if any. */
  char		*value;	/* Value of the predicate: a character string
			   terminated by a null and made up of the characters
			   of the tokens in the token-sequence separated by
			   blanks. */
} an_assert_value;
typedef struct an_assert_predicate *an_assert_predicate_ptr;
typedef struct an_assert_predicate {
  /* Definition of one #assert predicate name and any associated values. */
  an_assert_predicate_ptr
		next;	/* Next #assert predicate entry, if any. */
  char		*name;	/* Predicate name.  Null-terminated, no leading "#". */
  an_assert_value_ptr
		values;	/* List of values for this predicate name.  (A given
			   predicate may have several values.) */
} an_assert_predicate;
static an_assert_predicate_ptr
		assert_predicates;
			/* List of all the #assert predicates currently
			   defined.  This is a simple linear list because we
			   don't expect too many of these. */


static an_assert_predicate_ptr find_predicate_entry(
                                             a_const_char            *name,
                                             sizeof_t                name_len,
                                             an_assert_predicate_ptr *prev_app)
/*
Find the #assert predicate entry for the name "name" of length "name_len"
and return a pointer to it, or return NULL if there is no such entry.
If there is an entry, also set *prev_app to point to the entry preceding
it on the list, or NULL if it is the first entry on the list.
*/
{
  an_assert_predicate_ptr app;

  /* Search the list of defined predicates looking for a matching name. */
  for (*prev_app = NULL, app = assert_predicates;
       app != NULL;
       *prev_app = app, app = app->next) {
    if (strlen(app->name) == name_len &&
        memcmp(app->name, name, size_t_arg(name_len)) == 0) {
      /* Found it. */
      break;
    }  /* if */
  }  /* for */
  return app;
}  /* find_predicate_entry */


static an_assert_predicate_ptr find_or_make_predicate_entry(
                                                         a_const_char *name,
                                                         sizeof_t     name_len)
/*
Find an existing predicate entry for the name given by "name" of length
"name_len", or create one if one does not exist, and return a pointer to
the entry in either case.
*/
{
  an_assert_predicate_ptr app, prev_app;

  /* Find an entry if one exists already. */
  app = find_predicate_entry(name, name_len, &prev_app);
  if (app == NULL) {
    /* Make a new entry because one does not already exist. */
    app = (an_assert_predicate_ptr)alloc_fe(sizeof(an_assert_predicate));
    app->next   = assert_predicates;
    assert_predicates = app;
    /* The name must be copied to front end storage since it's currently
       part of the source line. */
    app->name   = alloc_fe((sizeof_t)(name_len+1));
    (void)memcpy(app->name, name, size_t_arg(name_len));
    app->name[name_len] = '\0';
    app->values = NULL;
  }  /* if */
  return app;
}  /* find_or_make_predicate_entry */


static char *collect_optional_assert_token_sequence(a_boolean *err)
/*
Collect the optional token-sequence for an #assert or #unassert as a character
string in temp_text_buffer, and return a pointer to the beginning of the
(null-terminated) string.  Return NULL if there was no token sequence.
The caller must know that temp_text_buffer is not in use currently.
Return *err TRUE if there was some error.
*/
{
  char          *start_loc = NULL;
  unsigned long paren_count;
  sizeof_t      offset;

  *err = FALSE;
  /* The directive can end here, after the name, or there can be a list
     of tokens enclosed in parentheses. */
  if (get_token() == tok_newline) {
    /* The directive ends with the predicate name, as in "#assert name". */
  } else if (curr_token != tok_lparen) {
    /* Error -- expected a left parenthesis. */
    pos_error(ec_exp_lparen, &error_position);
    *err = TRUE;
  } else {
    /* The opening parenthesis is present.  Scan the tokens until the
       closing parenthesis. */
    paren_count = 0;
    /* temp_text_buffer starts out empty. */
    pos_in_temp_text_buffer = 0;
    while (get_token() != tok_newline && curr_token != tok_end_of_source) {
      /* Count parentheses within the loop, because nested parentheses
         matter, as in
           #assert xyz(aaa(bbb)ccc)
      */
      /* If you change this, see scan_assert_predicate_reference as well. */
      if (curr_token == tok_rparen) {
        /* Exit the loop on the proper closing parenthesis. */
        if (paren_count == 0) break;
        paren_count--;
      } else if (curr_token == tok_lparen) {
        paren_count++;
      }  /* if */
      /* Put the text of the current token and a blank (as a token separator)
         into temp_text_buffer.  White space is not significant and is not
         saved. */
      for (offset = 0; offset < len_of_curr_token; offset++) {
        put_ch_to_temp_text_buffer(start_of_curr_token[offset]);
      }  /* for */
      put_ch_to_temp_text_buffer(' ');
    }  /* while */
    /* Add a null character to end the token sequence. */
    put_ch_to_temp_text_buffer('\0');
    /* We now have in temp_text_buffer a character string representing the
       token-sequence. */
    start_loc = temp_text_buffer;
    /* Check for the closing parenthesis. */
    if (!required_token(tok_rparen, ec_exp_rparen)) *err = TRUE;
  }  /* if */
  return start_loc;
}  /* collect_optional_assert_token_sequence */


static an_assert_value_ptr find_assert_value(an_assert_predicate_ptr app,
                                             a_const_char            *value,
                                             an_assert_value_ptr     *prev_avp)
/*
Look for an existing value of the #assert predicate indicated by app that
matches value.  If one is found, return a pointer to it, and set *prev_avp to
point to the previous value on the list, or NULL if the value entry is the
first on the list.  If no appropriate value entry is found, return NULL.
*/
{
  an_assert_value_ptr ptr;

  for (*prev_avp = NULL, ptr = app->values;
       ptr != NULL;
       *prev_avp = ptr, ptr = ptr->next) {
    if (strcmp(ptr->value, value) == 0) break;
  }  /* for */
  return ptr;
}  /* find_assert_value */


static void add_assert_value(a_const_char            *value,
                             an_assert_predicate_ptr app)
/*
Make an #assert predicate value entry for the given value and add it to the
front of the value list for the predicate pointed to by app.  value is
null-terminated.  Do not add the value if it exists already on the list.
*/
{
  an_assert_value_ptr avp, prev_avp;

  /* If the value is already present, do not add it again. */
  if (find_assert_value(app, value, &prev_avp) == NULL) {
    avp = (an_assert_value_ptr)alloc_fe(sizeof(an_assert_value));
    /* Put the new value entry on the front of the value list. */
    avp->next = app->values;
    app->values = avp;
    /* Copy the value string into freshly-allocated storage for it. */
    avp->value = strcpy(alloc_fe((sizeof_t)(strlen(value)+1)), value);
  }  /*if */
}  /* add_assert_value */


void proc_assert(void)
/*
Scan and process an #assert directive.  This is an AT&T extension in System V
release 4.  Its form is

  #assert name ( token-list )

or

  #assert name

The defined name can be tested in #if expressions by writing

  #if #name( token-list )

which is true if one of the asserted values of #name is the indicated
token-list.
*/
{
  an_assert_predicate_ptr predicate_entry = NULL;
  char                    *token_str = NULL;
  a_boolean               err = FALSE;

  db_enter(3, "proc_assert");
  /* Get the predicate identifier. */
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    pos_error(ec_exp_identifier, &error_position);
    err = TRUE;
  } else {
    /* The identifier __VA_ARGS__ is not allowed if variadic macros are
       accepted. */
    check_use_of_VA_ARGS(len_of_curr_token, start_of_curr_token);
    /* Find or make a predicate entry for the name. */
    predicate_entry = find_or_make_predicate_entry(start_of_curr_token,
                                                   len_of_curr_token);
    /* Collect the optional token sequence in temp_text_buffer. */
    token_str = collect_optional_assert_token_sequence(&err);
  }  /* if */
  /* If there was no error, install the assertion value. */
  if (err) {
    some_error_in_curr_directive = TRUE;
  } else {
#if DEBUG
    if (debug_level >= 3) {
      fprintf(f_debug, "Processing #assert %s", predicate_entry->name);
      if (token_str != NULL) fprintf(f_debug, " ( %s )", token_str);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* Only add a value if there was a token sequence. */
    if (token_str != NULL) add_assert_value(token_str, predicate_entry);
  }  /* if */
  db_exit();
}  /* proc_assert */


void proc_unassert(void)
/*
Scan and process an #unassert directive.  This is an AT&T extension in System V
release 4.  Its form is

  #unassert name ( token-list )

or

  #unassert name

*/
{
  an_assert_predicate_ptr predicate_entry = NULL, prev_app;
  an_assert_value_ptr     predicate_value, prev_avp;
  char                    *token_str = NULL;
  a_boolean               err = FALSE;

  db_enter(3, "proc_unassert");
  /* Get the predicate identifier. */
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    pos_error(ec_exp_identifier, &error_position);
    err = TRUE;
  } else {
    /* Find any predicate entry for the name.  Do not create one if one is
       not found. */
    predicate_entry = find_predicate_entry(start_of_curr_token,
                                           len_of_curr_token, &prev_app);
    /* Collect the optional token sequence in temp_text_buffer. */
    token_str = collect_optional_assert_token_sequence(&err);
  }  /* if */
  /* If there was no error, do the #unassert. */
  if (err) {
    some_error_in_curr_directive = TRUE;
  } else {
    if (predicate_entry == NULL) {
      /* There's no existing entry, so there's nothing to undo. */
    } else {
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "Processing #unassert %s", predicate_entry->name);
        if (token_str != NULL) fprintf(f_debug, " ( %s )", token_str);
        fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      if (token_str == NULL) {
        /* There was no token sequence, so remove the entire predicate, not
           just one value. */
        if (prev_app == NULL) {
          /* It's first on the list. */
          assert_predicates = predicate_entry->next;
        } else {
          prev_app->next = predicate_entry->next;
        }  /* if */
      } else {
        /* There was a token sequence, so look for a value entry with that
           value. */
        predicate_value = find_assert_value(predicate_entry, token_str,
                                            &prev_avp);
        if (predicate_value == NULL) {
          /* There is no existing value like the one we want, so do nothing. */
        } else {
          /* Remove the value entry. */
          if (prev_avp == NULL) {
            /* The value is first on the list. */
            predicate_entry->values = predicate_value->next;
          } else {
            prev_avp->next = predicate_value->next;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* proc_unassert */


static an_assert_value_ptr next_matching_assert_value(
                                             an_assert_value_ptr matched_value,
                                             sizeof_t            matched_len)
/*
The #assert predicate value being scanned matches the first matched_len
characters of the assert value matched_value.  However, we have discovered
that the following characters do not match, so advance to the next value
on the list that begins with the same characters, and return a pointer
to it.  If there is no such value, return NULL.
*/
{
  an_assert_value_ptr old_matched_value = matched_value;

  while ((matched_value = matched_value->next) != NULL) {
    if (smemcmp(matched_value->value, old_matched_value->value,
                matched_len) == 0) {
      break;
    }  /* if */
  }  /* for */
  return matched_value;
}  /* next_matching_assert_value */


void scan_assert_predicate_reference(a_boolean *rescan)
/*
Scan a reference to an #assert predicate in a preprocessing #if.  Its form
is

  #name(token-sequence)

The current character position is after the "#".  start_of_curr_token
points to the "#".  On return, either *rescan == TRUE and the input has
been replaced with "0" or "1" to indicate whether the token-sequence
exists as a value for the #assert predicate, and the current token
should be rescanned; or an error has been issued and *rescan == FALSE,
and processing should continue in sequence.
*/
{
  a_boolean               result = FALSE;
  a_boolean               save_fetch_pp_tokens = fetch_pp_tokens;
  a_boolean               save_expand_macros = expand_macros;
  an_assert_predicate_ptr app, prev_app;
  an_assert_value_ptr     matched_value;
  sizeof_t                matched_len;
  char                    *after_matched_str;
  unsigned long           paren_count;
  a_boolean               err = FALSE;
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_source_position       start_pos;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

  db_enter(4, "scan_assert_predicate_reference");
  *rescan = FALSE;
  check_assertion(delete_source_from_loc == NULL);
  delete_source_from_loc = start_of_curr_token;
  fetch_pp_tokens = TRUE;
  expand_macros = FALSE;
#if FULLY_RESOLVED_MACRO_POSITIONS
  conv_line_loc_to_source_pos(start_of_curr_token, &start_pos);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  if (get_token() != tok_identifier) {
    /* Error -- expected an identifier. */
    pos_error(ec_exp_identifier, &error_position);
    err = some_error_in_curr_directive = TRUE;
  } else {
    /* Look up the predicate name. */
    app = find_predicate_entry(start_of_curr_token, len_of_curr_token,
                               &prev_app);
    /* Scan the token list whether or not the predicate name is defined. */
    if (get_token() != tok_lparen) {
      /* Error -- expected a left parenthesis. */
      pos_error(ec_exp_lparen, &error_position);
      err = some_error_in_curr_directive = TRUE;
    } else {
      /* Scan the token sequence.  We don't actually build the token string;
         instead, we keep track of the string matched in a value string
         so far.  As soon as we can no longer match the text so far to
         anything in the values, we give up (the predicate is false). */
      /* matched_value points to a value entry, and the assertion is that
         the first match_len characters of that value's string match the
         token sequence scanned so far (including blanks after each token).
         If matched_value == NULL, no value matches the string so far, and
         we're just scanning for the closing parenthesis. */
      matched_value = NULL;
      if (app != NULL) matched_value = app->values;
      matched_len = 0;
      paren_count = 0;
      /* Get tokens until the closing parenthesis is found. */
      while (get_token() != tok_newline && curr_token != tok_end_of_source) {
        /* Count parentheses within the loop, because nested parentheses
           matter, as in
             #if  #xyz(aaa(bbb)ccc)
        */
        /* If you change this, see collect_optional_assert_token_sequence
           as well. */
        if (curr_token == tok_rparen) {
          /* Exit the loop on the proper closing parenthesis. */
          if (paren_count == 0) break;
          paren_count--;
        } else if (curr_token == tok_lparen) {
          paren_count++;
        }  /* if */
        if (matched_value != NULL) {
try_match_again:
          /* See if the text of the token just scanned can be added to the
             string matched so far.  Also check for the blank as a token
             delimiter after the token string. */
          after_matched_str = matched_value->value + matched_len;
          if (smemcmp(after_matched_str,
                      start_of_curr_token,
                      len_of_curr_token) == 0 &&
              *(after_matched_str+len_of_curr_token) == ' ') {
            /* The new token matches the continuation of the matched string,
               so change the matched string length to include the added
               text. */
            matched_len += len_of_curr_token+1;
          } else {
            /* Mismatch.  Look for another value entry later on the list
               that starts with the currently matched string, and then try to
               match the new token against the continuation of that string. */
            matched_value = next_matching_assert_value(matched_value,
                                                       matched_len);
            if (matched_value != NULL) goto try_match_again;
          }  /* if */
        }  /* if */
      }  /* while */
      /* Check for the closing parenthesis.  required_token cannot be used
         because it would do an inappropriate flush on error.  Also, we
         don't want to advance to the next token after the ")". */
      if (curr_token != tok_rparen) {
        pos_error(ec_exp_rparen, &error_position);
        err = some_error_in_curr_directive = TRUE;
        matched_value = NULL;
      }  /* if */
      /* See whether the assert value we've matched so far ends at this
         point. */
      while (matched_value != NULL &&
             matched_value->value[matched_len] != '\0') {
        /* No, it doesn't.  See whether there is another value that
           starts with the same string. */
        matched_value = next_matching_assert_value(matched_value,
                                                   matched_len);
      }  /* while */
      /* The result is TRUE if we have an entire value string that matches
         the entire token sequence (i.e., we have a matching string and
         the next thing after it is the terminating null character). */
      if (matched_value != NULL &&
          matched_value->value[matched_len] == '\0') result = TRUE;
    }  /* if */
  }  /* if */
  if (curr_token != tok_rparen) {
    /* If the construct was not properly closed with a right parenthesis,
       make the current token (e.g., tok_newline) be scanned again.
       Without this, we could run off the end of the #if directive. */
    curr_char_loc = start_of_curr_token;
    *rescan = TRUE;
  }  /* if */
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  if (!err) {
    /* Delete the assertion predicate, replacing it by "0" or "1". */
    a_source_line_modif_ptr slmp;
    slmp = add_source_line_modif(delete_source_from_loc,
                                 (sizeof_t)(curr_char_loc -
                                                       delete_source_from_loc),
                                 (char *)NULL, (char *)NULL);
    /* Put the replacement string in the source line modification's inboard
       inserted_chars array. */
    slmp->inserted_chars[0] = result ? '1' : '0';
    slmp->inserted_chars[1] = LE_ESCAPE;
    slmp->inserted_chars[2] = LE_END_OF_INSERTION;
    slmp->inserted_text = curr_char_loc = slmp->inserted_chars;
    slmp->end_inserted_text = slmp->inserted_chars+1;
    *rescan = TRUE;
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* Add text map entries to describe the original position of the
       predicate reference. */
    add_entry_to_macro_text_map(&macro_text_map, /*start_of_region=*/0,
                                start_pos.seq, start_pos.column,
                                NO_PARENT_MACRO_INVOCATION);
    add_entry_to_macro_text_map(&macro_text_map, LE_ESCAPE_LEN+1,
                                (a_seq_number)0, SP_COL_UNKNOWN,
                                NO_PARENT_MACRO_INVOCATION);
    slmp->text_map.num_entries = 2;
    slmp->text_map.entries =
                       &macro_text_map.entries[macro_text_map.num_entries - 2];
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* if */
  delete_source_from_loc = NULL;
  db_exit();
}  /* scan_assert_predicate_reference */


void enter_assert_predicate(a_const_char *value,
                            a_const_char *name)
/*
Enter an #assert predicate with name "name" and value "value".  This is
used for predefined predicates (see fe_init.c).  CAREFUL:  The value string
must have an extra blank at the end, as in

  enter_assert_predicate("m68k ", "machine");

*/
{
  an_assert_predicate_ptr app;

#if CHECKING
  if (strlen(value) > 0 && value[strlen(value)-1] != ' ') {
    internal_error("enter_assert_predicate: value must have blank at the end");
  }  /* if */
#endif /* CHECKING */
  /* Find or make a predicate entry for the name. */
  app = find_or_make_predicate_entry(name, (sizeof_t)strlen(name));
  /* If there's no existing entry for the value, add one. */
  add_assert_value(value, app);
}  /* enter_assert_predicate */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */


static char *make_repl_text(a_const_char *repl_text,
                            sizeof_t     *repl_text_length)
/*
Make a replacement text string for a macro, corresponding to the raw text
given by repl_text.  repl_text == NULL implies an empty replacement string.
The length of the repl_text string is returned in *repl_text_length if
repl_text_length is not NULL.
*/
{
  char     *repl_text_copy, *rtp;
  sizeof_t repl_text_len, overhead;

  repl_text_len = (repl_text != NULL) ? strlen(repl_text) : 0;
  /* There is always an rt_null at the end of the string.  If the text is
     not empty, there is also a header before it. */
  overhead = 1;
  if (repl_text_len > 0) {
    overhead += 1+NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER;
  }  /* if */
  rtp = repl_text_copy = alloc_fe((sizeof_t)(repl_text_len+overhead));
  if (repl_text_len > 0) {
    /* Put the kind -- raw text -- in the header. */
    *rtp++ = (char)rt_text;
    /* Put the length in the header. */
    put_macro_repl_text_number(repl_text_len, rtp);
    /* Copy the text itself. */
    (void)memcpy(rtp, repl_text, size_t_arg(repl_text_len)); /*lint !e668*/
    rtp += repl_text_len;
  }  /* if */
  /* Put the terminator on the string. */
  *rtp = (char)rt_null;
  /* Return the length of the repl_text_string including the encoded
     information in the "overhead" area. */
  if (repl_text_length != NULL) *repl_text_length = repl_text_len + overhead;
  return(repl_text_copy);
}  /* make_repl_text */


static a_symbol_ptr enter_predef_macro_full(
                                          a_const_char *macro_value,
                                          a_const_char *macro_name,
                                          a_boolean    cannot_be_redefined,
                                          a_boolean    ref_suppresses_pch_file,
                                          a_boolean    function_like)
/*
Enter a predefined macro.  macro_name is the name, macro_value the
replacement text string (or NULL for a special macro).  cannot_be_redefined
is TRUE if this is a predefined macro that cannot be redefined.
ref_suppresses_pch_file is TRUE if a use of the macro should prevent
creation of a precompiled header file.  If function_like is TRUE, the macro
will be created to accept a single argument that will be macro-expanded;
otherwise, the macro will be object-like, taking no argument list.  A
pointer to the symbol entry is returned.
*/
{
  a_symbol_ptr		sym_ptr;
  a_macro_def_ptr	mdp;
  a_symbol_locator	locator;
  char			*repl_text = NULL;
  sizeof_t		repl_text_length = 0;

  /* Construct a replacement text string for the macro value. */
  if (macro_value != NULL) {
    repl_text = make_repl_text(macro_value, &repl_text_length);
  }  /* if */
  /* Look for a previous definition of the predefined macro.  If found, it
     must have the same replacement string. */
  clear_locator(&locator, &null_source_position);
  sym_ptr = find_macro_symbol_by_name(macro_name, (sizeof_t)strlen(macro_name),
                                      &locator);
  if (sym_ptr != NULL) {
    /* Make sure that the replacement text is the same.  The "-1" is
       needed because the length returned by make_repl_text includes the
       rt_null terminator. */
    if (!equiv_replacement_text(repl_text, repl_text_length - 1,
                                 sym_ptr->variant.macro_def)) {
      /* The macro definition is not the same as the previous one. */
      str_catastrophe(ec_bad_predef_macro_redef, macro_name);
    }  /* if */
  } else {
    sym_ptr = full_enter_symbol(macro_name, (sizeof_t)(strlen(macro_name)),
                                (a_symbol_kind)sk_macro, NO_SCOPE_DEPTH);
    sym_ptr->variant.macro_def = mdp = alloc_macro_def();
    if (function_like) {
      /* The macro takes a single, macro-expanded argument. */
      mdp->object_like = FALSE;
      mdp->param_list = alloc_macro_param();
      mdp->param_list->name = (char *)"";
      mdp->param_list->need_expanded_form = TRUE;
    } else {
      /* The macro is object-like, taking no argument list. */
      mdp->object_like = TRUE;
      mdp->param_list  = NULL;
    }  /* if */
    mdp->cannot_be_redefined = cannot_be_redefined;
    mdp->is_predefined = TRUE;
    mdp->ref_suppresses_pch_file = ref_suppresses_pch_file;
    mdp->repl_text   = repl_text;
#if RECORD_MACROS_IN_IL
    /* Insert predefined macros into the macro list.  Predefined macros that
       have a varying replacement list (like __LINE__ and __FILE__) will have
       an empty replacement text in the IL entry. */
    { a_source_position pos = null_source_position;
      pos.column = SP_COL_PREDEFINED_MACRO;
      (void)make_il_macro_entry(sym_ptr, &pos);
    }
#endif /* RECORD_MACROS_IN_IL */
#if FULLY_RESOLVED_MACRO_POSITIONS
    /* There are no positions to map, so we just add a beginning and ending
       map entry, both indicating the original location as a predefined
       macro. */
    init_macro_text_map(2, &mdp->text_map, /*resizable=*/FALSE);
    add_entry_to_macro_text_map(&mdp->text_map, /*start_of_region=*/0,
                                (a_seq_number)0, SP_COL_PREDEFINED_MACRO,
                                NO_PARENT_MACRO_INVOCATION);
    add_entry_to_macro_text_map(&mdp->text_map, repl_text_length,
                                (a_seq_number)0, SP_COL_PREDEFINED_MACRO,
                                NO_PARENT_MACRO_INVOCATION);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* if */
  return(sym_ptr);
}  /* enter_predef_macro_full */


a_symbol_ptr enter_predef_macro(a_const_char *macro_value,
                                a_const_char *macro_name,
                                a_boolean    cannot_be_redefined,
                                a_boolean    ref_suppresses_pch_file)
/*
This is a wrapper for enter_predef_macro_full that creates only object-like
macros.  See enter_predef_macro_full for a description of the parameters.
*/
{
  return enter_predef_macro_full(macro_value, macro_name, cannot_be_redefined,
                                 ref_suppresses_pch_file,
                                 /*function_like=*/FALSE);
}  /* enter_predef_macro */


static a_symbol_ptr f_enter_predef_num_macro(
                                   a_host_large_integer  num,
                                   a_const_char          *macro_name,
                                   a_boolean             cannot_be_redefined)
/*
Predefine a macro with the given name to the decimal representation of num.
cannot_be_redefined is TRUE if this is a predefined macro that cannot be
redefined.
*/
{
  char  macro_value[50];

  (void)signed_to_string_buf(num, macro_value);
  return enter_predef_macro_full(macro_value, macro_name, cannot_be_redefined,
                                 /*ref_suppresses_pch_file=*/FALSE,
                                 /*function_like=*/FALSE);
}  /* f_enter_predef_num_macro */

#define enter_predef_num_macro_noredef(num, name)                            \
  ((void)f_enter_predef_num_macro((a_host_large_integer)(num), (name),       \
                                  /*cannot_be_redefined=*/TRUE))

#define enter_predef_num_macro(num, name)                                    \
  ((void)f_enter_predef_num_macro((a_host_large_integer)(num), (name),       \
                                  /*cannot_be_redefined=*/FALSE))


a_boolean is_valid_identifier(a_const_char     *id_start,
                              sizeof_t         id_len,
                              a_symbol_ptr     *assoc_symbol,
                              a_symbol_locator *locator)
/*
Check the given identifier to see if it is valid as a macro name.
If so, return TRUE; if not, return FALSE.  Return in *assoc_symbol
a symbol entry for the identifier, if there is already one, and return
a symbol locator in *locator.
*/
{
  a_boolean         return_value = FALSE;
  sizeof_t          i;
  a_source_position position;

  *assoc_symbol = NULL;

  /* Identifier "position" is in the command line. */
  set_position_to(position, 0, SP_COL_CMD_LINE);
  clear_locator(locator, &position);
  if (id_len < 1) {
    /* Zero-length identifier is invalid. */
  } else {
    int numch;
    for (i = 0; i < id_len; i += numch) {
      /* Check each character to see if it is valid. */
      if (!is_identifier_char(id_start+i, &numch,
                              /*is_identifier_start=*/(i == 0))) {
        goto return_point;
      }  /* if */
    }  /* for */
    /* The identifier is syntactically valid.  Look it up. */
    *assoc_symbol = find_macro_symbol_by_name(id_start, id_len, locator);
    return_value = TRUE;
  }  /* if */
return_point:
  return(return_value);
}  /* is_valid_identifier */


static void init_date_and_time_macros(char  curr_date_time[26])
/*
Enter predefined macros __DATE__ and __TIME__, based on the string
curr_date_time passed in by the caller.
*/
{
  char             date_of_translation[14];
  char             time_of_translation[11];

  /* Make the date string. */
  date_of_translation[0] = date_of_translation[12] = '"';
  /* Copy "Mmm dd " into [1] .. [7]. */
  (void)memcpy(date_of_translation+1, curr_date_time+4, 7);
  /* If the day-of-month has a leading zero, replace it with a space.
     ctime is allowed to return a leading zero, but __DATE__ is required
     to have a blank there.  Windows NT returns a leading zero from ctime. */
  if (date_of_translation[5] == '0') {
    date_of_translation[5] = ' ';
  }  /* if */
  /* Copy "yyyy" into [8] .. [11]. */
  (void)memcpy(date_of_translation+8, curr_date_time+20, 4);
  date_of_translation[13] = '\0';
  /* Make the time string. */
  time_of_translation[0] = time_of_translation[9] = '"';
  /* Copy "hh:mm:ss" into [1] .. [8]. */
  (void)memcpy(time_of_translation+1, curr_date_time+11, 8);
  time_of_translation[10] = '\0';
  if (!using_a_pch_file) {
    /* Create the symbols. */
    date_macro_symbol = enter_predef_macro(date_of_translation, "__DATE__",
                                           /*cannot_be_redefined=*/TRUE,
                                           /*ref_suppresses_pch_file=*/TRUE);
    time_macro_symbol = enter_predef_macro(time_of_translation, "__TIME__",
                                           /*cannot_be_redefined=*/TRUE,
                                           /*ref_suppresses_pch_file=*/TRUE);
  } else {
    /* The symbols already exist -- they were read in from a precompiled
       header file.  Reset the date and time strings to conform to the new
       date and time. */
    check_assertion(date_macro_symbol != NULL &&
                    date_macro_symbol->variant.macro_def != NULL);
    date_macro_symbol->variant.macro_def->repl_text =
                         make_repl_text(date_of_translation, (sizeof_t*)NULL);
    check_assertion(time_macro_symbol != NULL &&
                    time_macro_symbol->variant.macro_def != NULL);
    time_macro_symbol->variant.macro_def->repl_text =
                         make_repl_text(time_of_translation, (sizeof_t*)NULL);
  }  /* if */

}  /* init_date_and_time_macros */


void fixup_predefined_macros(char  curr_date_time[26])
/*
The symbol table for this compilation has been read in from a precompiled
header file, so some of the predefined macros need to be altered.
*/
{
  /* Reset the replacement text for the __DATE__ and __TIME__ macro symbols. */
  init_date_and_time_macros(curr_date_time);
}  /* fixup_predefined_macros */


static void init_runtime_macros(void)
/*
Initialize a set of macros that are use to pass configuration information
from the front end to the runtime.
*/
{
#if DO_IL_LOWERING
#if DO_FULL_PORTABLE_EH_LOWERING
  a_const_char	*ptr;
  /* Define a macro that specifies the type of an element of the setjmp
     buffer. */
  if (targ_jmp_buf_elements_are_float) {
    ptr = float_kind_name(targ_jmp_buf_element_float_kind);
  } else {
    ptr = int_kind_name(targ_jmp_buf_element_int_kind);
  }  /* if */
  (void)enter_predef_macro(ptr, "__EDG_JMP_BUF_ELEMENT_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the number of elements in the setjmp buffer. */
  enter_predef_num_macro_noredef(targ_jmp_buf_num_elements,
                                 "__EDG_JMP_BUF_NUM_ELEMENTS");
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Define the type of the offset field in the Cfront virtual function
     table. */
  (void)enter_predef_macro(int_kind_name(targ_delta_int_kind),
			   "__EDG_DELTA_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if IA64_ABI
  /* Define the type of an entry in the IA-64 virtual function table. */
  (void)enter_predef_macro(int_kind_name(targ_ia64_vtable_entry_int_kind),
			   "__EDG_IA64_VTABLE_ENTRY_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* IA64_ABI */
  /* Define the type of the virtual function index field of the virtual
     function table. */
  (void)enter_predef_macro(int_kind_name(targ_virtual_function_index_int_kind),
			   "__EDG_VIRTUAL_FUNCTION_INDEX_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if GENERATE_EH_TABLES
  /* Define the type of the variable-handle field in the EH tables. */
  (void)enter_predef_macro(int_kind_name(targ_var_handle_int_kind),
			   "__EDG_VAR_HANDLE_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the type of a region number field in the EH tables. */
  (void)enter_predef_macro(int_kind_name(targ_region_number_int_kind),
			   "__EDG_REGION_NUMBER_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Define the value used as the null region number value in the EH tables. */
  enter_predef_num_macro_noredef(null_eh_region_number,
			         "__EDG_NULL_EH_REGION_NUMBER");
#endif /* GENERATE_EH_TABLES */
  enter_predef_num_macro_noredef((VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS),
                                   /*lint !e506*/
			         "__EDG_LOWER_VARIABLE_LENGTH_ARRAYS");
#if IA64_ABI
  /* Are we using the variant form of array cookies for the IA-64 ABI? */
  enter_predef_num_macro_noredef(IA64_ABI_USE_VARIANT_ARRAY_COOKIES,
			         "__EDG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES");
#endif /* IA64_ABI */
#if !IA64_ABI
  /* What type should we use for number_of_elements arguments in cfront ABI? */
  (void)enter_predef_macro(
                           int_kind_name(targ_runtime_elem_count_int_kind),
			   "__EDG_ELEM_COUNT_PARAM_TYPE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* !IA64_ABI */
#endif /* DO_IL_LOWERING */
  /* Define the ABI compatibility version being used. */
  enter_predef_num_macro_noredef(ABI_COMPATIBILITY_VERSION,
			         "__EDG_ABI_COMPATIBILITY_VERSION");
  /* Are the ABI changes for RTTI implemented? */
  enter_predef_num_macro_noredef(ABI_CHANGES_FOR_RTTI,
			         "__EDG_ABI_CHANGES_FOR_RTTI");
  /* Are the ABI changes for array new and delete implemented? */
  enter_predef_num_macro_noredef(ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE,
			         "__EDG_ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE");
  /* Are the ABI changes for placement delete implemented? */
  enter_predef_num_macro_noredef(ABI_CHANGES_FOR_PLACEMENT_DELETE,
			         "__EDG_ABI_CHANGES_FOR_PLACEMENT_DELETE");
  /* Pass the library dialect flags to the runtime (__BSD__, __SYSV__, and
     __ANSIC__). */
  enter_predef_num_macro_noredef(__BSD__, "__EDG_BSD");
  enter_predef_num_macro_noredef(__SYSV__, "__EDG_SYSV");
  enter_predef_num_macro_noredef(__ANSIC__, "__EDG_ANSIC");
#if CPP11_IL_EXTENSIONS_SUPPORTED
  /* Front end can support C++11 mode (so runtime must also). */
  (void)enter_predef_macro("1", "__EDG_CPP11_IL_EXTENSIONS_SUPPORTED",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
}  /* init_runtime_macros */


static void process_command_line_macro_definitions(a_boolean  process_defs,
                                                   a_boolean  process_undefs)
/*
Process a list of macro definitions as requested by "-D" (when process_defs is
TRUE) and "-U" (when process_undefs is TRUE) options on the command line.
*/
{
  a_def_undef_string_ptr  du_ptr = defs_from_cmd_line;
  a_boolean	          save_expand_macros = expand_macros;
  a_boolean	          save_fetch_pp_tokens = fetch_pp_tokens;

  /* Set a current position indicating we are looking at the command line. */
  set_position_to(pos_curr_token, 0, SP_COL_CMD_LINE);
  set_err_pos_to_curr_token();
  /* Don't expand macros while preprocessing: */
  expand_macros = FALSE;
  in_preprocessing_directive = TRUE;
  fetch_pp_tokens = TRUE;
  for (; du_ptr != NULL; du_ptr = du_ptr->next) {
    sizeof_t     du_len;
    a_const_char *du_str = du_ptr->text, *equal_pos;
    if (du_ptr->is_undef && process_undefs) {
      /* -U option. */
      a_boolean  err = FALSE, suppress_error = FALSE;
      a_symbol_ptr     assoc_symbol;
      a_symbol_locator locator;
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "Command-line undef: %s\n", du_str);
      }  /* if */
#endif /* DEBUG */
      /* Check the identifier to make sure it is valid. */
      if (!is_valid_identifier(du_str, (sizeof_t)strlen(du_str), &assoc_symbol,
                               &locator)) {
        err = TRUE;
        /* The Microsoft compiler ignores invalid definitions. */
        if (ms_extensions) suppress_error = TRUE;
      } else {
        if (assoc_symbol != NULL) {
          if (assoc_symbol->variant.macro_def->cannot_be_redefined) {
            /* The macro is predefined; one is not allowed to undefine it. */
            err = TRUE;
          } else {
            /* Remove the macro's definition.  The a_macro_def entry pointed to
               by the symbol is not freed, and is therefore just lost.  */
            remove_symbol(assoc_symbol);
          }  /* if */
        }  /* if */
      }  /* if */
      if (err && !suppress_error) {
        str_command_line_error(ec_cl_invalid_macro_undefinition, du_str);
      }  /* if */
    } else if (!du_ptr->is_undef && process_defs) {
      /* -D option. */
      char *p;
      if (strchr(du_str, ATTENTION_MARKER) != NULL) {
        /* Definition contains a newline character, which cannot be allowed
           (it would be confused with a lexical escape character). */
        str_command_line_error(ec_cl_invalid_macro_definition, du_str);
        /* Should not reach here. */
      }  /* if */
      /* Turn "-D" options into equivalent define directives so that we can
         leave the processing to proc_define.  Allocate an extra 2 bytes
         for "-D" options that do not contain an equal; they'll be
         processed as define id 1 (i.e., a "=1" is appended, and the "="
         will be skipped).  During this processing, ensure that diagnostics
         are correctly attributed by setting the global variable
         curr_cmd_line_or_predef_macro_def.  This is also used by
         proc_define to decide that the "=" introducing the macro
         definition should be skipped. */
      curr_cmd_line_or_predef_macro_def = du_str;
      du_len = strlen(du_str);
      /* Ensure the buffer holding the logical source line is large enough to
         hold the synthetic line we are going to create. */
      ensure_min_curr_source_line_length(du_len+2+2*LE_ESCAPE_LEN);
      p = (char *)curr_source_line;
      strcpy(p, du_str);
      equal_pos = strchr(p, '=');
      if (equal_pos == NULL) {
        /* "-DNAME(X)" becomes "NAME(X)=1". */
        if (gnu_mode) {
          /* The GNU preprocessor uses " " instead of "=" to prefix the
             default replacement text.  This is only significant in cases
             where the definition has an implicit "=", such as "-Dx3.9":
             the GNU preprocessor treats this as equivalent to
             "#define x3 .9 1". */
          strcpy(p+du_len, " 1");
        } else {
          strcpy(p+du_len, "=1");
        }  /* if */
        du_len += 2;
      } else if (gnu_mode) {
        /* The "=" is optional in the GNU preprocessor command line -- that
           is, the GNU preprocessor accepts "-DFOO=BAR", "-DFOO =BAR", and
           "-DFOO BAR" -- so replace the "=" with a space to simplify the
           processing in proc_define. */
        *(char *)equal_pos = ' ';
      }  /* if */
      p[du_len]   = LE_ESCAPE;
      p[du_len+1] = LE_NEWLINE;
      p[du_len+2] = LE_ESCAPE;
      p[du_len+3] = LE_END_OF_LINE;
      curr_char_loc = curr_source_line;
      logical_char_info_entries_used = 0;
      (void)proc_define();
      /* Reset curr_cmd_line_or_predef_macro_def so that diagnostics are no
         longer attributed to the command-line option we just processed. */
      curr_cmd_line_or_predef_macro_def = NULL;
    }  /* if */
  }  /* for */
  in_preprocessing_directive = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  expand_macros = save_expand_macros;
  /* Set a current position indicating we are in initialization. */
  set_position_to(pos_curr_token, 0, SP_COL_UNKNOWN);
  set_err_pos_to_curr_token();
}  /* process_command_line_macro_definitions */


static void init_new_c_predefined_macros(void)
/*
Enter symbols for the predefined macros in C99 and later revisions.
*/
{
  /* Predefine the C99 __STDC_HOSTED__ macro based on the STDC_HOSTED
     configuration flag. */
  enter_predef_num_macro_noredef(STDC_HOSTED, "__STDC_HOSTED__");
#if STDC_IEC_559
  (void)enter_predef_macro("1", "__STDC_IEC_559__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* STDC_IEC_559 */
#if STDC_IEC_559_COMPLEX
  (void)enter_predef_macro("1", "__STDC_IEC_559_COMPLEX__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* STDC_IEC_559_COMPLEX */
#if STDC_ISO_10646
  enter_predef_num_macro_noredef(STDC_ISO_10646_VALUE, "__STDC_ISO_10646__");
#endif /* STDC_ISO_10646 */
#if STDC_MB_MIGHT_NEQ_WC
  (void)enter_predef_macro("1", "__STDC_MB_MIGHT_NEQ_WC__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* STDC_MB_MIGHT_NEQ_WC */
  /* The following macros are described by the C11 standard, but it is valid
     and useful to define them in plain C99 mode as well. */
  if (!vla_enabled) {
    (void)enter_predef_macro("1", "__STDC_NO_VLA__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /*if */
  if (!c11_atomic_enabled) {
    /* Atomic types are not supported by the front end. */
    (void)enter_predef_macro("1", "__STDC_NO_ATOMICS__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  if (uliterals_enabled) {
    /* Indicate that char16_t and char32_t literals are encoded in UTF-16
       and UTF-32, respectively. */
    (void)enter_predef_macro("1", "__STDC_UTF_16__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "__STDC_UTF_32__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
}  /* init_new_c_predefined_macros */


static char* expanded_version_string(unsigned long version,
                                     a_const_char  *version_string_pattern)
/*
Allocate and return a buffer containing a copy of version_string_pattern with
"%m" expanded to "gcc" or "g++" (depending on the current mode) and "%v"
expanded to the specified version of the compiler being emulated.  The caller
is responsible to deallocate the buffer using free_general.
*/
{
  unsigned long  major_num = (unsigned long)(version/10000);
  unsigned long  minor_num = (unsigned long)((version%10000)/100);
  unsigned long  patch_num = (unsigned long)(version%100);
  a_const_char   *src;
  char           *version_string, *dst;
#if CHECKING
  a_boolean      percent_m_seen = FALSE, percent_v_seen = FALSE;
#endif /* CHECKING */

  check_assertion_str(gnu_mode &&
                      major_num < 100 && minor_num < 100 && patch_num < 100,
                      "version too large");
  version_string = (char*)alloc_general(
                             (sizeof_t)(strlen(version_string_pattern) + 50));
  src = version_string_pattern;
  dst = version_string;
  for (; *src != '\0'; ++src, ++dst) {
    if (*src == '%') {
      if (*(src+1) == 'm') {
#if CHECKING
        check_assertion_str(!percent_m_seen,
                            "too many %m in version_string_pattern");
        percent_m_seen = TRUE;
#endif /* CHECKING */
        ++src;
        (void)strcpy(dst, gcc_mode ? "gcc" : "g++");
        dst += 2;
      } else if (*(src+1) == 'v') {
#if CHECKING
        check_assertion_str(!percent_v_seen,
                            "too many %v in version_string_pattern");
        percent_v_seen = TRUE;
#endif /* CHECKING */
        ++src;
        (void)sprintf(dst, "%lu.%lu", major_num, minor_num);
        while (*dst != '\0') ++dst;
        if (patch_num != 0) {
          (void)sprintf(dst, ".%lu", patch_num);
          while (*dst != '\0') ++dst;
        }  /* if */
        --dst;
      } else {
        *dst = *src;
      }  /* if */
    } else {
      *dst = *src;
    }  /* if */
  }  /* for */
  *dst = '\0';
  check_assertion_str(version_string[0] == '"' && dst[-1] == '"',
                      "version_string_pattern must be quote-delimited string");
  return version_string;
}  /* expanded_version_string */


static void init_gnu_predefined_macros(void)
/*
Enter symbols for the predefined macros of GNU/clang C and C++.
*/
{
  unsigned long  version = clang_mode ? clang_version : gnu_version;
  unsigned long  major_num = (unsigned long)(version/10000),
                 minor_num = (unsigned long)((version%10000)/100),
                 patch_num = (unsigned long)(version%100);

  /* Note that GNU C/C++ permits these macros to be redefined, so we do too. */
  if (gpp_mode) {
    /* In GNU C++ mode (but not in GNU C mode), __GNUG__ is identical to
       __GNUC__. */
    if (!clang_mode) {
      enter_predef_num_macro(major_num, "__GNUG__");
    }  /* if */
    if (rtti_enabled && gnu_version >= 40300) {
      /* g++ introduced the __GXX_RTTI macro in version 4.3.0. */
      (void)enter_predef_macro("1", "__GXX_RTTI",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  }  /* if */
  if (gnu_version >= 40103) {
    /* GCC 4.1.3 and later define a macro to indicate whether the GNU C89 or
       standard C99 rules are in effect for "inline".  GNU C++ 4.1.3+ also
       defines one of these macros, although their meaning is not clear in
       that case: The standard case appears to correspond to C++11 mode. */
    if (gcc_mode ? std_c99_inlining : cpp11_mode) {
      (void)enter_predef_macro("1", "__GNUC_STDC_INLINE__",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    } else {
      check_assertion(gpp_mode || gnu_c89_inlining);
      (void)enter_predef_macro("1", "__GNUC_GNU_INLINE__",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  }  /* if */
  if (clang_mode) {
    (void)enter_predef_macro("1", "__clang__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    enter_predef_num_macro(major_num, "__clang_major__");
    enter_predef_num_macro(minor_num, "__clang_minor__");
    enter_predef_num_macro(patch_num, "__clang_patchlevel__");
    (void)enter_predef_macro(expanded_version_string(version,
                                                     CLANG_VERSION_STRING),
                             "__clang_version__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    /* Note that clang also defines values for __GNUC__, __GNUC_MINOR__,
       __GNUC_PATCHLEVEL__, __GNUG__, and __VERSION__ but those are static (and
       based on a GNU version of 4.2.1) and aren't defined here (they can be
       defined in a predefined_macros.txt file if so desired). */
  } else {
    enter_predef_num_macro(major_num, "__GNUC__");
    enter_predef_num_macro(minor_num, "__GNUC_MINOR__");
    enter_predef_num_macro(patch_num, "__GNUC_PATCHLEVEL__");
    (void)enter_predef_macro(expanded_version_string(version,
                                                     GCC_VERSION_STRING),
                             "__VERSION__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  if (gnu_version >= 40400) {
    (void)enter_predef_macro(int_kind_name(targ_char16_t_int_kind),
                             "__CHAR16_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro(int_kind_name(targ_char32_t_int_kind),
                             "__CHAR32_TYPE__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
}  /* init_gnu_predefined_macros */


void set_predef_macro_mode(a_predef_macro_mode	mode,
			   a_boolean		value)
/*
Set the entry in the predefined macro mode value table entry of "mode"
to "value".
*/
{
  predef_macro_mode_values[(int)mode] = value;
}  /* set_predef_macro_mode */


static unsigned int last_mode_index = 0;
                        /* Keep track of the index of the last predefined
                           macro mode (since it's likely that the next
                           predefined macro will have the same mode). */


static a_boolean get_predef_macro_mode_value(char	*name)
/*
Return the value of the predefined macro mode associated with "name".
A catastrophic error is issued if the mode name is invalid.
*/
{
  unsigned int		i;
  a_predef_macro_mode	mode = pmm_none;

  if (last_mode_index != 0 &&
      strcmp(name, predef_macro_mode_names[last_mode_index]) == 0) {
    /* The mode is the same as the last one. */
    mode = (a_predef_macro_mode)last_mode_index;
  } else {
    /* Look for the specified name in the mode names table. */
    for (i = (int)pmm_none + 1; i < (int)pmm_last; ++i) {
      if (strcmp(name, predef_macro_mode_names[i]) == 0) {
        mode = (a_predef_macro_mode)i;
        last_mode_index = i;
        break;
      }  /* if */
    }  /* for */
    if (mode == pmm_none) {
      /* No matching name was found. */
      str_catastrophe(ec_bad_macro_mode_name, name);
    }  /* if */
  }  /* if */
  return predef_macro_mode_values[(int)mode];
}  /* get_predef_macro_mode_value */


static a_boolean process_predefined_macro_entry(char		*line,
						an_error_code	*error_code)
/*
Scan a predefined macro file entry.  The format is:

mode,!mode,mode   cannot_redefine   macro_name   macro_value

- "mode" is a label from the predefined macro modes table.  The macro is
  defined if the mode is set, or if the mode is not set when "!mode" is
  used.  The macro is defined if any of the mode tests is TRUE.

- cannot_redefine indicates whether the predefined macro may later be
  redefined.  The value must be "yes" or "no".

- macro_name is the name of the macro to be defined.

- macro_value is the value to which the macro should be defined.  All of
  the characters until the end of the line are used as the macro value.

An entry may also be empty (just whitespace).  A line that begins with
a "#" is a comment and is ignored.

Return TRUE if the macro line was processed successfully, FALSE otherwise.
When FALSE is returned, error_code points to a description of the error
that occurred.
*/
{
  char		*ptr;
  a_boolean	result = FALSE;
  a_boolean	done;
  char		*end_pos;
  a_boolean	cannot_redefine;
  a_boolean	mode_value = FALSE;
  sizeof_t      def_len;
  a_symbol_ptr  macro_sym;

#if DEBUG
  if (db_flag_is_set("predef_macro_entry")) {
    fprintf(f_debug, "Predef macro line: %s\n", line);
  }  /* if */
#endif /* DEBUG */
#define skip_blanks() for (; *ptr == ' ' || *ptr == '\t'; ++ptr) {}
  *error_code = ec_no_error;
  ptr = line;
  /* Skip any leading white space. */
  skip_blanks();
  /* Check for an empty or comment line. */
  if (*ptr == '\0' || *ptr == '#') goto exit;
  /* Process the mode flags. */
  for (done = FALSE; !done;) {
    a_boolean	required_value = TRUE;
    /* Check for the "!" that indicates the mode must not be set. */
    if (*ptr == '!') {
      required_value = FALSE;
      ptr++;
    }  /* if */
    /* Find the end of the mode. */
    for (end_pos = ptr;
         *end_pos != ',' && *end_pos != ' ' &&
         *end_pos != '\t' && *end_pos != '\0'; end_pos++) {}
    /* We shouldn't be at the end of the string. */
    if (*end_pos == '\0') {
      *error_code = ec_missing_cannot_redefine_flag;
      goto error_exit;
    }  /* if */
    /* See if this is the last mode value. */
    if (*end_pos != ',') done = TRUE;
    /* Replace the delimiter with a null. */
    *end_pos = '\0';
    /* See if the specified mode matches the required value. */
    if (get_predef_macro_mode_value(ptr) == required_value) mode_value = TRUE;
    ptr = end_pos + 1;
    /* There can't be white space in the mode list. */
    if (!done && (*ptr == ' ' || *ptr == '\t')) {
      *error_code = ec_missing_mode_after_comma;
      goto error_exit;
    }  /* if */
  }  /* for */
  /* Skip any whitespace to find the cannot-redefine flag. */
  skip_blanks();
  /* Find the end of the cannot-redefine flag. */
  for (end_pos = ptr;
       *end_pos != ' ' && *end_pos != '\t' && *end_pos != '\0'; end_pos++) {}
  /* We shouldn't be at the end of the string. */
  if (*end_pos == '\0') {
    *error_code = ec_missing_macro_name;
    goto error_exit;
  }  /* if */
  /* Replace the delimiter with a null. */
  *end_pos = '\0';
  if (strcmp(ptr, "yes") == 0 ) {
    cannot_redefine = TRUE;
  } else if (strcmp(ptr, "no") == 0) {
    cannot_redefine = FALSE;
  } else {
    /* An invalid cannot-redefine value. */
    *error_code = ec_invalid_cannot_redefine_value;
    goto error_exit;
  }  /* if */
  ptr = end_pos + 1;
  /* Skip any whitespace to find the macro name. */
  skip_blanks();
  /* We shouldn't be at the end of the string. */
  if (*ptr == '\0') {
    *error_code = ec_missing_macro_name;
    goto error_exit;
  }  /* if */
  if (mode_value) {
    /* The macro should be defined based on the mode parameters.  Set up to
       process the macro definition as if it were a #define in a source
       file. */
    char *p;
    curr_cmd_line_or_predef_macro_def = ptr;
    def_len = strlen(ptr);
    ensure_min_curr_source_line_length(def_len + 2 * LE_ESCAPE_LEN);
    p = (char *)curr_source_line;
    strcpy(p, ptr);
    p[def_len] = LE_ESCAPE;
    p[def_len + 1] = LE_NEWLINE;
    p[def_len + 2] = LE_ESCAPE;
    p[def_len + 3] = LE_END_OF_LINE;
    curr_char_loc = curr_source_line;
    logical_char_info_entries_used = 0;
    /* Process the definition and get the symbol pointer for the new
       macro. */
    macro_sym = proc_define();
    /* Reset the definition pointer to indicate that we have finished with
       this predefined macro. */
    curr_cmd_line_or_predef_macro_def = NULL;
    if (macro_sym != NULL) {
      /* The macro was successfully defined. */
      a_macro_def_ptr mdp = macro_sym->variant.macro_def;
      mdp->cannot_be_redefined = cannot_redefine;
#if DEBUG
      if (db_flag_is_set("predef_macro_entry")) {
        char* name = ptr;
        char saved_ch;
        for ( ; *ptr != ' ' && *ptr != '\t' && *ptr != '\0'; ++ptr) {}
        saved_ch = *ptr;
        *ptr = '\0';
        fprintf(f_debug, "  name&parms=%s, ", name);
        *ptr = saved_ch;
        skip_blanks();
        fprintf(f_debug, "defn=%s, obj-like=%s, cannot redefine=%s\n",
                ptr, mdp->object_like ? "TRUE" : "FALSE",
                cannot_redefine ? "TRUE" : "FALSE");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
exit:
  result = TRUE;
error_exit:
  /* The error string should be set in error cases. */
  check_assertion(result || *error_code != ec_no_error);
  return result;
}  /* process_predefined_macro_entry */


static FILE *open_predefined_macro_file(void)
/*
Construct the name of the predefined macro file, and open the file.
Return the file descriptor.
*/
{
  a_text_buffer_ptr	buf;
  char			*file_name;
  FILE			*f_file;
  a_const_char		*aux_dir_name;

  /* Make sure the auxiliary directory name is not NULL. */
  aux_dir_name = auxiliary_info_dir_name;
  /* coverity[dead_error_condition] */ /* coverity[dead_error_line] */
  if (aux_dir_name == NULL) aux_dir_name = "";
  buf = combine_dir_and_file_name(edg_base_directory, aux_dir_name,
                                  (a_text_buffer_ptr)NULL);
  append_to_path_name(buf, PREDEFINED_MACRO_FILE_NAME);
  file_name = buf->buffer;
  f_file = fopen_with_error(file_name, "r", OFF_NO_OPTIONS,
                            ec_predef_macro);
  return f_file;
}  /* open_predefined_macro_file */

static FILE	*f_predef_macros;
			/* The file descriptor for the predefined macros
			   file. */

static void process_predefined_macro_file(void)
/*
Read macro definition entries from the file:

  edg_base_directory/lib/PREDEFINED_MACRO_FILE_NAME

PREDEFINED_MACRO_FILE_NAME is a macro whose value is used to create the
file name.
*/
{
  char		*line;
  unsigned long	line_number = 0;
  an_error_code	error_code;
  a_boolean     save_expand_macros = expand_macros;
  a_boolean     save_fetch_pp_tokens = fetch_pp_tokens;

  /* Set a current position indicating that we are looking at the
     predefined macro file. */
  set_position_to(pos_curr_token, 0, SP_COL_PREDEFINED_MACRO);
  set_err_pos_to_curr_token();
  /* Don't expand macros while preprocessing. */
  expand_macros = FALSE;
  in_preprocessing_directive = TRUE;
  fetch_pp_tokens = TRUE;
  processing_predefined_macro = TRUE;
  /* Open the predefined macro file and process its contents one line at
     a time. */
  f_predef_macros = open_predefined_macro_file();
  while ((line = read_line_from_file(f_predef_macros)) != NULL) {
    line_number++;
    if (!process_predefined_macro_entry(line, &error_code)) {
      /* The predefined macro line was invalid. */
      char buf[50];
      (void)unsigned_to_string_buf((a_host_large_unsigned)line_number, buf);
      pos_str2_catastrophe(ec_bad_predef_macro_line, buf,
			   error_text(error_code), &null_source_position);
    }  /* if */
  }  /* while */
  (void)fclose(f_predef_macros);
  f_predef_macros = NULL;
  processing_predefined_macro = FALSE;
  fetch_pp_tokens = save_fetch_pp_tokens;
  in_preprocessing_directive = FALSE;
  expand_macros = save_expand_macros;
  set_position_to(pos_curr_token, 0, SP_COL_UNKNOWN);
  set_err_pos_to_curr_token();
}  /* process_predefined_macro_file */


void init_predefined_macros(char  curr_date_time[26])
/*
Enter symbols for predefined macros, including those established by
command line -D options.
*/
{
  a_boolean process_defs_undefs_in_order = FALSE;
  int       i;

  if (targ_has_signed_chars) {
    /* Target has signed characters. */
    /* Enter macro __SIGNED_CHARS__, which is used to modify the definition
       of CHAR_MIN and CHAR_MAX in the included limits.h. */
    (void)enter_predef_macro("1", "__SIGNED_CHARS__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
  /* Enter macro __PRAGMA_REDEFINE_EXTNAME to indicate that the special pragma
     redefine_extname is available. */
  (void)enter_predef_macro("1", "__PRAGMA_REDEFINE_EXTNAME",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
  /* Enter the symbols for the __DATE__ and __TIME__ macros. */
  init_date_and_time_macros(curr_date_time);
  /* Determine whether __STDC__ should be set, and if so, the value to
     which it should be set.  Normally, __STDC__ is set to 1 in ANSI
     C mode and in C++ (in C++ it is implementation defined whether __STDC__
     is defined, and if so, what value it has).  The setting of __STDC__
     is affected by stdc_zero_in_nonstrict_mode, Microsoft mode, and
     cfront mode.  __STDC__ can be redefined in C++ mode, and in C mode
     except for strict ANSI C mode. */
  if (C_dialect == C_dialect_ANSI || C_dialect == C_dialect_cplusplus) {
    a_boolean	define_stdc = TRUE;
    a_boolean	stdc_cannot_be_redefined = (C_dialect == C_dialect_ANSI &&
                                            strict_ansi_mode);
    stdc_value = TRUE;
    if (stdc_zero_in_nonstrict_mode) {
      /* In this mode, __STDC__ is 1 in strict mode and 0 otherwise. */
      stdc_value = strict_ansi_mode;
    } else if (microsoft_mode) {
      /* The Microsoft compiler does not define __STDC__ in either C or
         C++ mode when it supports extensions.  In some configurations,
         defining __STDC__ in Microsoft emulation mode may be desirable,
         particularly on Linux systems when GNU headers are used
         (not defining __STDC__ can lead to "const" being defined as a
         NULL macro when stdio.h is included). */
      define_stdc = DEFINE_STDC_IN_MICROSOFT_MODE;
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
    } else if (any_cfront_mode()) {
      /* If configured to use old-style preprocessing in cfront
         compatibility mode, do not define __STDC__ in that mode. */
      define_stdc = FALSE;
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
    }  /* if */
    if (define_stdc) {
      char		*stdc_string;
      a_symbol_ptr	sym;
      /* In some modes, the value of __STDC__ is determined by whether a system
         header is being processed.  Treat __STDC__ specially in such modes.
         This feature is provided for GNU compatibility, but can be enabled
         in other modes. */
      if (stdc_zero_in_system_headers) {
        stdc_string = NULL;
      } else {
        stdc_string = (char*)(stdc_value ? "1" : "0");
      }  /* if */
      sym =  enter_predef_macro(stdc_string, "__STDC__",
                                stdc_cannot_be_redefined,
                                /*ref_suppresses_pch_file=*/FALSE);
      if (stdc_zero_in_system_headers) {
        stdc_macro_symbol = sym;
      }  /* if */
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_ANSI) {
    if (!microsoft_mode && (!gcc_mode || c99_mode)) {
      /* __STDC_VERSION__ is defined based on the version of C being used.
         The Microsoft compiler does not define this macro; the GNU and clang
         compilers do so only for C99 ("std=c99") and C11 ("std=c11"). */
      a_const_char *stdc_version = c11_mode ? "201112L"
                                 : c99_mode ? "199901L"
                                            : "199409L";
      (void)enter_predef_macro(stdc_version, "__STDC_VERSION__",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    if (c99_mode) {
      /* Includes C11 mode. */
      init_new_c_predefined_macros();
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    if (upc_mode) {
      (void)enter_predef_macro("1", "__upc__",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  /* __cplusplus is defined to reflect the appropriate variant if we are
     compiling C++, left undefined otherwise.  In most modes, __cplusplus
     can be redefined as this is needed in some environments.  In Microsoft
     mode it can't be redefined because the Microsoft compiler actually
     ignores attempts to redefine it.  For compatibility, c_plusplus is
     defined in cfront mode. */
  if (C_dialect == C_dialect_cplusplus) {
    a_const_char *val;
    a_const_char *cpp98_date = "199711L";
    a_const_char *cpp11_date = "201103L";
    a_const_char *gnu_cpp14_date = "201300L";
    a_const_char *cpp14_date = "201402L";
    if (ms_extensions && !clang_mode) {
      if (microsoft_version < 1310) {
        val = "1";
      } else {
        val = cpp98_date;
      }  /* if */
    } else if (gpp_mode && !clang_mode) {
      if (gnu_version < 40700) {
        val = "1";
      } else if (cpp17_mode && gnu_version >= 50000) {
        /* Temporary value that GCC uses for -std=c++1z: */
        val = "201500L";
      } else if (cpp14_mode && gnu_version >= 40900) {
        /* Version 4.9 of g++ was the first to accept -std=c++14 but set
           the value of __cplusplus to 201300L.  Beginning with version
           5.1, g++ uses the correct value. */
        val = (gnu_version >= 50100) ? cpp14_date : gnu_cpp14_date;
      } else if (cpp11_mode) {
        val = cpp11_date;
      } else {
        val = cpp98_date;
      }  /* if */
    } else if (any_cfront_mode()) {
      val = "1";
    } else if (cpp17_mode && clang_mode) {
      /* Temporary value that clang uses for -std=c++1z: */
      val = "201406L";
    } else if (cpp14_mode) {
      val = cpp14_date;
    } else if (cpp11_mode) {
      val = cpp11_date;
    } else {
      val = cpp98_date;
    }  /* if */
    (void)enter_predef_macro(val, "__cplusplus",
			     /*cannot_be_redefined=*/microsoft_mode,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (any_cfront_mode()) {
      (void)enter_predef_macro("1", "c_plusplus",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    if (cpp11_mode) {
      /* Predefine the __STDC_HOSTED__ macro based on the STDC_HOSTED
         configuration flag. */
      enter_predef_num_macro_noredef(STDC_HOSTED, "__STDC_HOSTED__");
    }  /* if */
    if (report_embedded_cplusplus_noncompliance) {
      /* Define a macro indicating this is an Embedded C++ application. */
      (void)enter_predef_macro("1", "__embedded_cplusplus",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#if DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD
    if (wchar_t_is_keyword) {
      /* Enter a predefined macro that can be used to determine that
         wchar_t is a keyword. */
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD */
    if (ms_extensions && wchar_t_is_keyword) {
      /* In Microsoft mode, always define _WCHAR_T_DEFINED when wchar_t is
         a keyword. */
      (void)enter_predef_macro("1", "_WCHAR_T_DEFINED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
      if (microsoft_version >= 1300) {
        /* For Microsoft versions 1300 and beyond, also define
           _NATIVE_WCHAR_T_DEFINED.  Because _WCHAR_T_DEFINED is also defined
           by the Microsoft header files when wchar_t is not a keyword,
           _NATIVE_WCHAR_T_DEFINED can be used to determine whether wchar_t
           is a keyword. */
        (void)enter_predef_macro("1", "_NATIVE_WCHAR_T_DEFINED",
                                 /*cannot_be_redefined=*/TRUE,
                                 /*ref_suppresses_pch_file=*/FALSE);
      }  /* if */
    }  /* if */
#if DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS
    if (char16_t_and_char32_t_are_keywords) {
      /* Enter a predefined macro that can be used to determine that
         char16_t and char32_t are keywords. */
      (void)enter_predef_macro("1",
                         MACRO_DEFINED_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS,
                         /*cannot_be_redefined=*/TRUE,
                         /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_CHAR16_T_AND_CHAR32_T_ARE_KEYWORDS */
#if DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD
    if (bool_is_keyword) {
      /* Enter a predefined macro that can be used to determine that
         bool is a keyword. */
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD */
    if (microsoft_mode && bool_is_keyword) {
      /* In Microsoft, always define __BOOL_DEFINED when bool is a keyword. */
      (void)enter_predef_macro("1", "__BOOL_DEFINED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#if DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
    if (array_new_and_delete_enabled) {
      /* Enter a predefined macro that can be used to determine that
         array new and delete are enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */
#if DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
    if (exceptions_enabled) {
      /* Enter a predefined macro that can be used to determine that
         exceptions are enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED */
#if DEFINE_MACRO_WHEN_RTTI_ENABLED
    if (rtti_enabled) {
      /* Enter a predefined macro that can be used to determine that
         RTTI is enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_RTTI_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_RTTI_ENABLED */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
#if DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
    /* When placement delete is supported, define a macro that can be used
       to determine that this is the case.  The macro is only defined when
       exceptions are enabled as placement delete routines are only called
       by the EH mechanism. */
    if (exceptions_enabled) {
      /* Enter a predefined macro that can be used to determine that
         placement delete is enabled. */
      (void)enter_predef_macro(
                     "1", MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED,
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
#if RUNTIME_USES_NAMESPACES
    /* Enter a predefined macro that can be used to determine that
       the runtime uses namespaces.  This is also used by the
       standard header files so that they know whether to declare
       things like type_info in the std namespace. */
    (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES,
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (implicit_using_std) {
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* RUNTIME_USES_NAMESPACES */
#if IA64_ABI
    /* Enter a predefined macro that can be used to determine that the
       compiler is using the IA64 C++ ABI. */
    (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_IA64_ABI,
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
#if IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS
    /* Do constructors and destructors return "this" for the IA-64 ABI? */
    (void)enter_predef_macro("1", 
                             MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS,
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
#endif /* IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS */
#if IA64_ABI_USE_INT_STATIC_INIT_GUARD
    /* Are we using the variant "int"-sized guard variables? */
    (void)enter_predef_macro("1",
                             MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD,
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
#endif /* IA64_ABI_USE_INT_STATIC_INIT_GUARD */
#endif /* IA64_ABI */
    /* Initialize the local feature-test "enabled" flags.  This is done
       unconditionally, as the feature_support_list table is used for both
       the WG21 SG10 feature-test macros and the clang feature-test
       macros. */
    access_control_sfinae = cpp11_mode && !cpp11_sfinae_ignore_access;
    contextual_conversions = TRUE;
    attribute_deprecated_with_message = (gnu_version >= 40500);
    decltype_keyword_enabled = decltype_enabled &&
                                              !enable_underscore_decltype_only;
    if (define_portable_feature_test_macros) {
      /* Add definitions as described by WG21 SG10 SD-6 for the features
         that are enabled in the current execution of the front end. */
      for (i = 0; i < (int)NUM_FEATURES; ++i) {
        if (feature_support_list[i].macro_name != NULL &&
            feature_support_list[i].enabled != NULL &&
            *feature_support_list[i].enabled) {
          /* The feature is supported in the current execution of the front
             end.  Define the macro with the appropriate value. */
          (void)enter_predef_macro(feature_support_list[i].macro_value,
                                   feature_support_list[i].macro_name,
                                   /*cannot_be_redefined=*/TRUE,
                                   /*ref_suppresses_pch_file=*/FALSE);
        }  /* if */
      }  /* for */
      /* __cpp_constexpr must be handled specially, as it will have
         different values depending on whether C++11 or C++14 constexpr
         features are supported. */
      if (relaxed_constexpr_enabled) {
        (void)enter_predef_macro("201304", "__cpp_constexpr",
                                 /*cannot_be_redefined=*/TRUE,
                                 /*ref_suppresses_pch_file=*/FALSE);
      } else if (constexpr_enabled) {
        (void)enter_predef_macro("200704", "__cpp_constexpr",
                                 /*cannot_be_redefined=*/TRUE,
                                 /*ref_suppresses_pch_file=*/FALSE);
      }  /* if */
      /* __cpp_range_based_for and __cpp_static_assert must be handled
         specially, as they will have different values depending on whether
         C++11 or C++17 mode is used. */
      if (range_based_for_enabled) {
        (void)enter_predef_macro(relaxed_range_based_for_enabled ? "201603" :
                                                                   "200907",
                                 "__cpp_range_based_for",
                                 /*cannot_be_redefined=*/TRUE,
                                 /*ref_suppresses_pch_file=*/FALSE);
      }  /* if */
      if (static_assert_enabled) {
        (void)enter_predef_macro(terse_static_assert_enabled ? "201411" :
                                                               "200410",
                                 "__cpp_static_assert",
                                 /*cannot_be_redefined=*/TRUE,
                                 /*ref_suppresses_pch_file=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* if */
#if DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED
  { a_boolean	long_long_is_disabled = !LONG_LONG_ALLOWED;
    if (strict_ansi_mode && !long_long_is_standard) {
      long_long_is_disabled = TRUE;
    }  /* if */
    if (long_long_is_disabled) {
      /* Enter a predefined macro that can be used to determine that
         long long is not enabled. */
      (void)enter_predef_macro("1", MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED,
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  }
#endif /* DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    unsigned long eff_microsoft_version = microsoft_version;
    if (microsoft_version == 1901 || microsoft_version == 1902) {
      /* Internally 1901 and 1902 are used to represent Visual Studio 2015
         Update 1 and Update 2 respectively, but externally, they are
         still 1900. */
      eff_microsoft_version = 1900;
    }  /* if */
    /* Define the _MSC_VER variable that indicates the version of the
       Microsoft compiler that is being emulated. */
    enter_predef_num_macro(eff_microsoft_version, "_MSC_VER");
    /* Define _MSC_FULL_VER, which is similar to _MSC_VER but appends the
       "build number", and _MSC_BUILD which is just the "build number". */
    { char  macro_val[100], *ptr = macro_val, *build_ptr;
      ptr += unsigned_to_string_buf(
                                  (a_host_large_unsigned)eff_microsoft_version,
                                  ptr);
      check_assertion((ptr-macro_val) < 10);
      build_ptr = ptr;
      (void)unsigned_to_string_buf(
                    (a_host_large_unsigned)microsoft_build_number, ptr);
      (void)enter_predef_macro(macro_val, "_MSC_FULL_VER",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
      (void)enter_predef_macro(build_ptr, "_MSC_BUILD",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }
    /* Define _MSC_EXTENSIONS. */
    (void)enter_predef_macro("1", "_MSC_EXTENSIONS",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    /* Define _WIN32 (even on _WIN64 configurations). */
    (void)enter_predef_macro("1", "_WIN32",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    if (targ_supports_x86_64) {
      (void)enter_predef_macro("1", "_WIN64",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    if (rtti_enabled) {
      /* Define _CPPRTTI when RTTI is enabled. */
      (void)enter_predef_macro(
                     "1", "_CPPRTTI",
                     /*cannot_be_redefined=*/TRUE,
                     /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    if (nullptr_enabled && microsoft_version >= 1600) {
      /* Define a macro indicating that the non-managed version of nullptr
         can be used. */
      (void)enter_predef_macro("1", "_NATIVE_NULLPTR_SUPPORTED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#if COROUTINES_ALLOWED
    if (coroutines_enabled && coroutine_keywords_enabled) {
      (void)enter_predef_macro("1", "_RESUMABLE_FUNCTIONS_SUPPORTED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
#endif /* COROUTINES_ALLOWED */
    if (cppcli_enabled) {
      /* Define _MANAGED when C++/CLI is enabled. */
      (void)enter_predef_macro("1", "_MANAGED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
      /* Define _M_CEE when C++/CLI is enabled. */
      (void)enter_predef_macro("1", "_M_CEE",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    if (!targ_has_signed_chars) {
      (void)enter_predef_macro("1", "_CHAR_UNSIGNED",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
    /* Enter a macro for the maximum size of an integral value. */
    { long int_max_size;
#if LONG_LONG_ALLOWED
      int_max_size = (long)targ_sizeof_long_long;
#else /* !LONG_LONG_ALLOWED */
      int_max_size = (long)targ_sizeof_long;
#endif /* LONG_LONG_ALLOWED */
      enter_predef_num_macro(int_max_size * CHAR_BIT, /*lint !e647*/
                             "_INTEGRAL_MAX_BITS");
    }
    if (msvc_lang != NULL) {
      (void)enter_predef_macro(msvc_lang, "_MSVC_LANG",
                               /*cannot_be_redefined=*/TRUE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  if (type_traits_helpers_enabled) {
    /* Not a Microsoft mode.  Enter a macro to indicate that native type traits
       helpers are available to ease the implementation of ISO/IEC TR 19768. */
    (void)enter_predef_macro(
              "1", MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED,
              /*cannot_be_redefined=*/TRUE, /*ref_suppresses_pch_file=*/FALSE);

  }  /* if */
#if DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED
  /* Enter a predefined macro that can be used to determine that variadic
     templates are enabled. */
  if (variadic_templates_enabled) {
    (void)enter_predef_macro(
                            "1", MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED,
                            /*cannot_be_redefined=*/TRUE,
                            /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#endif /* DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED */
#if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
  /* Enter a predefined macro that can be used to determine that this
     implementation allows multiple threads. */
  if (std_thread_local_storage_specifier_enabled && !C_mode()) {
    (void)enter_predef_macro("1", "__STDCPP_THREADS__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#else /* !IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
  if (C_mode()) {
    (void)enter_predef_macro("1", "__STDC_NO_THREADS__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#endif /* IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS */
  if (constexpr_enabled) {
    /* Define a fixed macro (not configurable since it is used by the
       EDG-provided <initializer_list> header) indicating whether support for
       constexpr is enabled. */
    (void)enter_predef_macro("1", "__EDG_CONSTEXPR_ENABLED__",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  /* Enter a predefined macro that can be used to determine that the
     EDG front end is being used. */
  (void)enter_predef_macro("1", "__EDG__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Enter a predefined macro that can be used to determine the version of
     the EDG front end being used. */
  enter_predef_num_macro_noredef(VERSION_NUMBER_FOR_MACRO, "__EDG_VERSION__");
  /* Enter a predefined macro for the type of size_t on this target. */
  (void)enter_predef_macro(int_kind_name(targ_size_t_int_kind),
                           "__EDG_SIZE_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Enter a predefined macro for the type of ptrdiff_t on this target. */
  (void)enter_predef_macro(int_kind_name(targ_ptrdiff_t_int_kind),
                           "__EDG_PTRDIFF_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* In GNU C/C++ mode, enter the macros that GNU compilers define. */
  if (gnu_mode) init_gnu_predefined_macros();
  if (building_runtime) {
    /* Define macros used to pass configuration information to the
       runtime library. */
    init_runtime_macros();
  }  /* if */

  /* __LINE__, __FILE__, defined, etc., are special (they cannot be defined
     in terms of a simple replacement string).  Therefore, they are entered
     with a NULL replacement text, and code on the expansion end handles
     them. */
  line_macro_symbol    = enter_predef_macro((char *)NULL, "__LINE__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  file_macro_symbol    = enter_predef_macro((char *)NULL, "__FILE__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  defined_macro_symbol = enter_predef_macro((char *)NULL, "defined",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  /* The GNU __BASE_FILE__ macro is accepted in all modes. */
  base_file_macro_symbol = enter_predef_macro((char *)NULL, "__BASE_FILE__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  if (pragma_operator_allowed) {
    /* Like the special macros defined above, _Pragma is entered as a
       predefined macro but is handled specially during replacement. */
    Pragma_macro_symbol = enter_predef_macro((char *)NULL, "_Pragma",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  if (ms_extensions) {
    /* __pragma is like the C99-style _Pragma operator except the argument is a
       series of tokens, not a string literal.  Like _Pragma, it receives
       special treatment during replacement. */
    microsoft_pragma_macro_symbol = enter_predef_macro(
                                            (char *)NULL, "__pragma",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  if (ms_extensions || gnu_mode) {
    counter_macro_symbol = enter_predef_macro(
                                            (char *)NULL, "__COUNTER__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
    timestamp_macro_symbol = enter_predef_macro(
                                            (char *)NULL, "__TIMESTAMP__",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  if (clang_mode) {
    /* Feature-test macros for clang mode.  Like the preceding macros, they
       are entered with a NULL replacement text and expanded appropriately
       in macro_invocation. */
    clang_has_feature_symbol = enter_predef_macro_full(
                                             (char *)NULL, "__has_feature",
                                             /*cannot_be_redefined=*/TRUE,
                                             /*ref_suppresses_pch_file=*/FALSE,
                                             /*function_like=*/TRUE);
    clang_has_extension_symbol = enter_predef_macro_full(
                                             (char *)NULL, "__has_extension",
                                             /*cannot_be_redefined=*/TRUE,
                                             /*ref_suppresses_pch_file=*/FALSE,
                                             /*function_like=*/TRUE);
    clang_has_attribute_symbol = enter_predef_macro_full(
                                             (char *)NULL, "__has_attribute",
                                             /*cannot_be_redefined=*/TRUE,
                                             /*ref_suppresses_pch_file=*/FALSE,
                                             /*function_like=*/TRUE);
    clang_has_builtin_symbol = enter_predef_macro_full(
                                             (char *)NULL, "__has_builtin",
                                             /*cannot_be_redefined=*/TRUE,
                                             /*ref_suppresses_pch_file=*/FALSE,
                                             /*function_like=*/TRUE);
    /* The argument to __has_include and __has_include_next is special -- a
       header file name -- and must be scanned differently from ordinary
       macro arguments.  In order to suppress the normal macro argument
       processing, these macros are defined as object-like, not
       function-like, and the argument is scanned and processed directly by
       scan_has_include. */
    clang_has_include_next_symbol = enter_predef_macro(
                                            (char *)NULL, "__has_include_next",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
    has_include_symbol = enter_predef_macro((char *)NULL, "__has_include",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  } else if (C_dialect == C_dialect_cplusplus &&
             define_portable_feature_test_macros) {
    /* __has_include is a WG21 SG10 recommendation and must be defined even
       if we are not in clang mode. */
    has_include_symbol = enter_predef_macro((char *)NULL, "__has_include",
                                            /*cannot_be_redefined=*/TRUE,
                                            /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcx_enabled) {
    /* Define a macro that indicates that C++/CX is enabled. */
    (void)enter_predef_macro("201009L", "__cplusplus_winrt",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else if (cli_or_cx_enabled) {
    /* Define a macro that indicates that C++/CLI is enabled. */
    /* Note that the ECMA-372 standard requires a value of 200509L, but the
       Microsoft compiler uses 200406L (checked for microsoft_versions
       1600-1800). */
    (void)enter_predef_macro("200406L", "__cplusplus_cli",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Enter system specific macros and assertions. */
  enter_system_specific_predefined_macros_and_assertions();
  /* Look for a file containing predefined macro definitions. */
  if (use_predefined_macro_file) process_predefined_macro_file();
  /* The cpp preprocessor first processes all the -D options, and later all
     the -U options: That is our default behavior as well.  However, some
     versions of the GNU and Microsoft compilers process the options in the
     order they appear. */
  if ((gnu_mode && gnu_version >= 30000) ||
      (microsoft_mode && microsoft_version >= 1300)) {
    process_defs_undefs_in_order = TRUE;
  }  /* if */
  if (process_defs_undefs_in_order) {
    process_command_line_macro_definitions(/*process_defs=*/TRUE,
                                           /*process_undefs=*/TRUE);
  } else {
    process_command_line_macro_definitions(/*process_defs=*/TRUE,
                                           /*process_undefs=*/FALSE);
    process_command_line_macro_definitions(/*process_defs=*/FALSE,
                                           /*process_undefs=*/TRUE);
  }  /* if */
}  /* init_predefined_macros */


#if DEBUG
unsigned long show_macro_space_used(void)
/*
Display and return the amount of space used for various macro tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Macro table use:");

  db_space_used("macro param", num_macro_params_allocated, a_macro_param);
  db_space_used("macro def", num_macro_defs_allocated, a_macro_def);
  db_space_used_lost_general("macro arg", avail_macro_args,
                             num_macro_args_allocated, a_macro_arg);
  db_space_used_general("Macro arg text", macro_arg_text_space, char);
  db_space_used("Param name strings", param_name_string_space, char);
  db_space_used("Macro definition text", macro_definition_space, char);

  total = (unsigned long)(after_end_of_macro_buffer - macro_buffer);
  db_space_used_general_buffer("macro_buffer", total);

  if (pcc_preprocessing_mode) {
    total = (unsigned long)(after_end_of_aux_buffer_for_pcc_macros -
                            aux_buffer_for_pcc_macros);
    db_space_used_general_buffer("Aux pcc buffer", total);
  }  /* if */

#if FULLY_RESOLVED_MACRO_POSITIONS
  db_space_used("macro text map", macro_text_map.max_entries,
                a_macro_text_map_entry);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  db_space_used_total();

  return (grand_total);
}  /* show_macro_space_used */
#endif /* DEBUG */


void macro_one_time_init(void)
/*
Do one-time initialization of variables related to macro processing.
*/
{
  /* Do the initial allocation for macro_buffer.  (Since the space is
     allocated in general storage, it does not need to be reallocated for
     each source file; for the same reason, after_end_of_macro_buffer should
     not be reset.)  The space will be reallocated (larger) if necessary,
     but the size here should be big enough for the expected cases. */
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  macro_buffer = alloc_general((sizeof_t)(MACRO_BUFFER_INITIAL_ALLOCATION+1));
  after_end_of_macro_buffer = macro_buffer + MACRO_BUFFER_INITIAL_ALLOCATION;
#if FULLY_RESOLVED_MACRO_POSITIONS
  /* Initialize macro_text_map.  It is like macro_buffer, with its storage
     surviving each source file and being extended as needed. */
  init_macro_text_map(MACRO_TEXT_MAP_INITIAL_COUNT, &macro_text_map,
                      /*resizable=*/TRUE);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  if (pcc_preprocessing_mode || ms_compat) {
    /* Allocate the auxiliary buffer for pcc mode.  It is used to construct
       the full text of a first-level macro expansion so that the token
       pasting can match pcc's. */
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    aux_buffer_for_pcc_macros = alloc_resizable_buffer(
                 (sizeof_t)(AUX_BUFFER_FOR_PCC_MACROS_INITIAL_ALLOCATION+1));
    after_end_of_aux_buffer_for_pcc_macros = aux_buffer_for_pcc_macros +
                                AUX_BUFFER_FOR_PCC_MACROS_INITIAL_ALLOCATION;
#if FULLY_RESOLVED_MACRO_POSITIONS
    init_macro_text_map(AUX_TEXT_MAP_FOR_PCC_MACROS_INITIAL_COUNT,
                        &aux_text_map_for_pcc_macros, /*resizable=*/TRUE);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  } else {
    /* Auxiliary buffer will not be used. */
    aux_buffer_for_pcc_macros = NULL;
    after_end_of_aux_buffer_for_pcc_macros = NULL;
#if FULLY_RESOLVED_MACRO_POSITIONS
    init_macro_text_map(/*num_entries=*/0, &aux_text_map_for_pcc_macros,
                        /*resizable=*/FALSE);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
  }  /* if */
  avail_macro_args = NULL;
#if DEBUG
  num_macro_args_allocated = 0;
  macro_arg_text_space = 0;
#endif /* DEBUG */
  registered_pointers = NULL;
  macro_buffer_region_in_progress = NULL;
  f_predef_macros = NULL;
  /* Save variables from macro.h and macro.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(line_macro_symbol),
      pch_saved_var_array_elem(file_macro_symbol),
      pch_saved_var_array_elem(defined_macro_symbol),
      pch_saved_var_array_elem(Pragma_macro_symbol),
      pch_saved_var_array_elem(microsoft_pragma_macro_symbol),
      pch_saved_var_array_elem(timestamp_macro_symbol),
      pch_saved_var_array_elem(counter_macro_symbol),
      pch_saved_var_array_elem(counter_macro_number),
      pch_saved_var_array_elem(date_macro_symbol),
      pch_saved_var_array_elem(time_macro_symbol),
      pch_saved_var_array_elem(base_file_macro_symbol),
      pch_saved_var_array_elem(stdc_macro_symbol),
      pch_saved_var_array_elem(clang_has_feature_symbol),
      pch_saved_var_array_elem(clang_has_extension_symbol),
      pch_saved_var_array_elem(has_include_symbol),
      pch_saved_var_array_elem(clang_has_include_next_symbol),
      pch_saved_var_array_elem(clang_has_attribute_symbol),
      pch_saved_var_array_elem(clang_has_builtin_symbol),
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(assert_predicates),
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#if DEBUG
      pch_saved_var_array_elem(num_macro_params_allocated),
      pch_saved_var_array_elem(num_macro_defs_allocated),
      pch_saved_var_array_elem(param_name_string_space),
      pch_saved_var_array_elem(macro_definition_space),
#endif /* DEBUG */
#if RECORD_MACRO_INVOCATIONS
      pch_saved_var_array_elem(last_macro_invocation_record_block),
      pch_saved_var_array_elem(num_macro_invocation_records),
      pch_saved_var_array_elem(max_macro_invocation_depth),
#endif /* RECORD_MACRO_INVOCATIONS */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(line_macro_symbol);
  register_trans_unit_variable(file_macro_symbol);
  register_trans_unit_variable(defined_macro_symbol);
  register_trans_unit_variable(Pragma_macro_symbol);
  register_trans_unit_variable(microsoft_pragma_macro_symbol);
  register_trans_unit_variable(timestamp_macro_symbol);
  register_trans_unit_variable(counter_macro_symbol);
  register_trans_unit_variable(counter_macro_number);
  register_trans_unit_variable(date_macro_symbol);
  register_trans_unit_variable(time_macro_symbol);
  register_trans_unit_variable(base_file_macro_symbol);
  register_trans_unit_variable(stdc_macro_symbol);
  register_trans_unit_variable(clang_has_feature_symbol);
  register_trans_unit_variable(clang_has_extension_symbol);
  register_trans_unit_variable(has_include_symbol);
  register_trans_unit_variable(clang_has_include_next_symbol);
  register_trans_unit_variable(clang_has_attribute_symbol);
  register_trans_unit_variable(clang_has_builtin_symbol);
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  register_trans_unit_variable(assert_predicates);
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
  /* Note that these are translation unit variables, but they are not
     reinitialized for each translation unit.  They retain the value set
     for the primary translation unit, but are overwritten when loading
     exported template files. */
  register_trans_unit_variable(defs_from_cmd_line);
}  /* macro_one_time_init */


void macro_trans_unit_init(void)
/*
Initialize static variables related to macro processing that must be
initialized for each translation unit.  Note that init_predefined_macros
does additional per-translation-unit initialization, and must be called
after this function.
*/
{
  macro_depth = 0;
  line_macro_symbol = NULL;
  file_macro_symbol = NULL;
  defined_macro_symbol = NULL;
  Pragma_macro_symbol = NULL;
  microsoft_pragma_macro_symbol = NULL;
  timestamp_macro_symbol = NULL;
  counter_macro_symbol = NULL;
  counter_macro_number = 0;
  date_macro_symbol = NULL;
  time_macro_symbol = NULL;
  base_file_macro_symbol = NULL;
  scanning_macro_name = FALSE;
  macro_arg_list = NULL;
  end_of_macro_arg_list = NULL;
  stdc_macro_symbol = NULL;
  stdc_value = FALSE;
  clang_has_feature_symbol = NULL;
  clang_has_extension_symbol = NULL;
  has_include_symbol = NULL;
  clang_has_include_next_symbol = NULL;
  clang_has_attribute_symbol = NULL;
  clang_has_builtin_symbol = NULL;
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  assert_predicates = NULL;
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
  end_of_cpp_string = NULL;
#if RECORD_MACRO_INVOCATIONS
  last_macro_invocation_record_block = NULL;
  ckpt_last_macro_invocation_record_block = NULL;
  saved_next_macro_invocation_record_block = NULL;
  num_macro_invocation_records = 0;
  ckpt_num_macro_invocation_records = 0;
  max_macro_invocation_depth = 0;
  ckpt_max_macro_invocation_depth = 0;
  depth_of_curr_macro_invocation_record = 0;
  ckpt_depth_of_curr_macro_invocation_record = 0;
#endif /* RECORD_MACRO_INVOCATIONS */
  access_control_sfinae = FALSE;
  contextual_conversions = FALSE;
  use_raw_version_of_arg = FALSE;
  top_microsoft_slmp = NULL;
}  /* macro_trans_unit_init */


void macro_init(void)
/*
Initialize static variables related to macro processing that must be
initialized for each compilation.
*/
{
  /* avail_macro_args is not per-compilation and should not be cleared. */
  num_macro_invocations_in_process = 0;
#if FULLY_RESOLVED_MACRO_POSITIONS
  active_text_map_position_trackers = NULL;
#endif  /* FULLY_RESOLVED_MACRO_POSITIONS */
#if DEBUG
  num_macro_params_allocated    = 0;
  num_macro_defs_allocated      = 0;
  /* num_macro_args_allocated is not per-compilation and should not be
     cleared. */
  /* macro_arg_text_space is not per-compilation and should not be
     cleared. */
  param_name_string_space       = 0;
  macro_definition_space        = 0;
#endif /* DEBUG */
}  /* macro_init */

#if MAKE_FRONT_END_CALLABLE

void macro_cleanup(void)
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated.
*/
{
  close_file_if_open(&f_predef_macros);
}  /* macro_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
