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

lexical.c -- Source input and lexical scanning routines.

These routines and data structures handle reading of source lines
and parsing of them into tokens.

*/

#include "basics.h"
#include "lexical.h"
#include "preproc.h"
#include "error.h"
#include "trans_lims.h"
#include "host_envir.h"
#include "cmd_line.h"
#include "mem_manage.h"
#include "symbol_tbl.h"
#include "macro.h"
#include "il.h"
#include "literals.h"
#include "statements.h"

#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */

/*
Variables pertaining to the input stack (for include files and the
primary source file) and the current input file (the top entry on the
stack).
*/
#define MAX_INCLUDE_FILES_OPEN_AT_ONCE 3
			/* To avoid having too many open files: After include
			   nesting gets this deep, the same file will be
			   re-opened for all other include files.  The primary
			   source file is not included in this count. */
static an_input_stack_entry_ptr
		input_stack = NULL;
			/* Input stack, one entry for each active source 
			   file.  Entry [0] is for the primary source file.
			   Dynamically allocated, reallocated if necessary;
			   size_input_stack gives the current allocated
			   size.  Not per-file. */
static int	size_input_stack = 0;
			/* Allocated size of input_stack, in elements not
			   bytes.  Not per-file. */
#define INPUT_STACK_INCREMENTAL_ALLOCATION 30
			/* The number of elements added to input_stack each
			   time it is reallocated; also the initial
			   allocation. */
static int	depth_input_stack;
			/* Depth of the input stack, minus 1.
			   input_stack[depth_input_stack] is the active
			   entry.  -1 if the stack is completely empty. */
static a_seq_number
		seq_number_last_read;
			/* The sequence number of the physical line last read
			   from curr_input_stream.  This is often not the
			   same as the line number in the file.  Once the
			   end of file on the primary source file has been
			   passed, this indicates a sequence number one past
			   the highest sequence number actually read, as an
			   indication of a sort of end-of-file line. */
static FILE
		*curr_input_stream;
			/* The currently active source input stream. */
static a_boolean
		eof_read_on_curr_input_stream;
			/* TRUE if end of file has been reached on the
			   current source stream.  This can indicate the
			   end of an include file or the end of the primary
			   source file.  This is an internal variable
			   indicating a physical end of file, and should not
			   be confused with the logical end of file variables
			   like after_end_of_all_source. */
/*
Variables related to the current source line (see lexical.h):
*/
static a_boolean
		at_end_of_source_file;
			/* If TRUE, there is no current logical source line;
			   the last attempt to read one ran into an end of
			   file instead.  This may, however, be only the end
			   of an include file, and not of the entire source
			   sequence (see pop_input_stack).  The current
			   position is at the end of the current input file,
			   but still within it -- the stack has not yet been
			   popped.  curr_source_line still contains the line
			   most recently read. */
static a_boolean
		after_end_of_all_source;
			/* If TRUE, there is no current logical source line;
			   all source has been read, all files popped off the
			   input stack.  The current sequence number indicates
			   a position one line beyond the last line actually
			   read.  We are no longer inside any source file; we
			   are past the end of the primary file. */
static a_boolean
		any_tokens_gotten_from_curr_source_line;
			/* If TRUE, one or more tokens (normal or
			   preprocessor) have been gotten from the current
			   source line.  If FALSE, nothing (except possibly
			   white space) has been gotten from that line.
			   This is used in determining whether or not a "#"
			   is the start of a preprocessing directive. */
static a_boolean
		no_modifs_to_curr_source_line;
			/* TRUE if source_line_modif_list == NULL and
			   orig_line_modif_list == NULL.  Used as a speed
			   optimization in deciding whether or not one can
			   use the fast method of converting a line location
			   into a source position. */
static char	curr_raw_listing_line_code;
			/* When a raw listing file is being generated,
			   this indicates the code for the current line:
			   'S' for a line skipped by an if-skip, 'N' for
			   a normal line, or '\0' if there is no current 
			   line. */
static	char	*raw_listing_buffer = NULL;
			/* Buffer used in writing the macro-expanded versions
			   of source lines to the raw listing file.  Space is
			   dynamically allocated, and its upper bound is given
			   by after_end_of_raw_listing_buffer.
			   See lexical_init for the initial allocation. */
#define RAW_LISTING_BUFFER_INITIAL_ALLOCATION 3000
#define RAW_LISTING_BUFFER_INCREMENTAL_ALLOCATION 5000
			/* Initial and incremental allocation sizes for
			   raw_listing_buffer.  Should probably match the
			   corresponding constants for curr_source_line. */
static char	*after_end_of_raw_listing_buffer = NULL;
			/* Address past the last element of raw_listing_buffer,
			   as an aid to checking for overflow, etc.  A variable
			   because raw_listing_buffer can be reallocated larger
			   if needed. */
static char	*loc_in_raw_listing_buffer;
			/* Current output location in raw_listing_buffer. */
static a_boolean
		must_display_raw_listing_buffer;
			/* TRUE if raw_listing_buffer contains a line or lines
			   modified in nontrivial ways (comments are considered
			   trivial modifications).  If so, it must be
                           displayed.*/

/*
Data structure used in deciding where to put extra blanks to separate
adjacent tokens in textual preprocessing output.
*/
static a_byte	pp_lexical_category[CHAR_MAX-CHAR_MIN+1];
			/* For each character, the lexical category to
			   be used in preprocessing output.  These categories
			   are used to decide when extra token-separating
			   blanks must be inserted between tokens resulting
			   from macro expansion. */
#define PLC_SINGLETON 1
			/* A character in the singleton category always
			   stands alone as a token, and thus no extra blank
			   is ever required next to it for token separation. */
#define PLC_ID_OR_NUMBER 2
			/* Characters appearing in identifiers or pp-numbers
			   (see is_id_char). */
#define PLC_OTHER 3
			/* All other characters. */

#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_orig_line_modifs_allocated,
		num_source_line_modifs_allocated;
#endif /* DEBUG */

/*
Data structure for trapping a token (to look ahead one token), and for
saving/restoring the current token state.
*/
typedef struct a_token_state {
  /* Saved copy of the information related to a token returned from
     get_token, for later restoration.  The fields here have the
     same names as the global variables whose state they save. */
  a_token_kind	curr_token;
  a_symbol_locator
		locator_for_curr_id;
  a_symbol_ptr	symbol_list_for_curr_id;
  a_constant	const_for_curr_token;
  a_source_position
		pos_curr_token,
		error_position;
  int		kind_of_white_space_skipped;
  a_boolean	lint_argsused_flag;
  short		lint_varargs_count;
  a_boolean	lint_notreached_flag;
  /* start_of_curr_token, end_of_curr_token, and len_of_curr_token
     do not need to be saved (since saving/restoring is not done at
     the pp-token level), nor do variables describing the input stream. */
} a_token_state;
static a_boolean
		trapped_token;
			/* TRUE if the next token after the current one
			   has already been scanned, and the information
			   about it is in trapped_token_state. */
static a_token_state
		trapped_token_state;
			/* When trapped_token is TRUE, information about
			   the trapped token. */


static void save_curr_token_state(a_token_state *tsp)
/*
Save all information about the current token in *tsp, for later restoration.
*/
{
  tsp->curr_token = curr_token;
  tsp->locator_for_curr_id = locator_for_curr_id;
  tsp->symbol_list_for_curr_id = symbol_list_for_curr_id;
  copy_constant(&const_for_curr_token, &tsp->const_for_curr_token);
  copy_source_position(pos_curr_token, tsp->pos_curr_token);
  copy_source_position(error_position, tsp->error_position);
  tsp->kind_of_white_space_skipped = kind_of_white_space_skipped;
  tsp->lint_argsused_flag = lint_argsused_flag;
  tsp->lint_varargs_count = lint_varargs_count;
  tsp->lint_notreached_flag = lint_notreached_flag;
}  /* save_curr_token_state */


static void restore_curr_token_state(a_token_state *tsp)
/*
Restore all information about the current token from *tsp.
*/
{
  curr_token = tsp->curr_token;
  locator_for_curr_id = tsp->locator_for_curr_id;
  symbol_list_for_curr_id = tsp->symbol_list_for_curr_id;
  copy_constant(&tsp->const_for_curr_token, &const_for_curr_token);
  copy_source_position(tsp->pos_curr_token, pos_curr_token);
  copy_source_position(tsp->error_position, error_position);
  kind_of_white_space_skipped = tsp->kind_of_white_space_skipped;
  lint_argsused_flag = tsp->lint_argsused_flag;
  lint_varargs_count = tsp->lint_varargs_count;
  lint_notreached_flag = tsp->lint_notreached_flag;
  /* start_of_curr_token, end_of_curr_token, and len_of_curr_token were
     not saved.  Clear start_of_curr_token to catch any (illegal) use of it. */
  start_of_curr_token = NULL;
}  /* restore_curr_token_state */


static an_orig_line_modif_ptr add_orig_line_modif(
                                an_orig_line_modif_kind kind,
                                char                    *line_loc)
/*
Allocate an original line modification entry, set its kind to "kind",
set its line location to "line_loc", set its fields to default values,
add it to the end of the orig_line_modif_list, and return a pointer to it.
Entries of this kind indicate the locations of modifications to the
original source line because of trigraphs and line splices.
*/
{
  an_orig_line_modif_ptr olmp;

  if (avail_orig_line_modifs != NULL) {
    /* Reuse a freed entry. */
    olmp = avail_orig_line_modifs;
    avail_orig_line_modifs = avail_orig_line_modifs->next;
  } else {
    /* Allocate a new entry. */
    olmp = (an_orig_line_modif_ptr)alloc_fe(sizeof(an_orig_line_modif));
#if DEBUG
    num_orig_line_modifs_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Set the fixed fields. */
  olmp->next     = NULL;
  olmp->line_loc = line_loc;
  olmp->kind     = kind;
  /* Set the variant fields. */
  switch ((int)kind) {
    case olm_trigraph:
      olmp->variant.trigraph_orig_char = ' ';  /* To be neat. */
      break;
    case olm_line_splice:
      olmp->variant.line_splice_seq_number = 0;  /* To be neat. */
      break;
#if CHECKING
    default:
      internal_error("add_orig_line_modif: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* Add the entry to the end of the list. */
  if (orig_line_modif_list == NULL) {
    /* This is the first entry on the list. */
    orig_line_modif_list = olmp;
  } else {
    /* Link the current last entry to this new last entry. */
    end_orig_line_modif_list->next = olmp;
  }  /* if */
  /* The new entry is the new end of list. */
  end_orig_line_modif_list = olmp;
  no_modifs_to_curr_source_line = FALSE;

  return(olmp);
}  /* add_orig_line_modif */


static void free_orig_line_modif(an_orig_line_modif_ptr *olmp)
/*
Free the original line modification pointed to by *olmp, and set *olmp 
to NULL.
*/
{
  /* Add the entry to the front of the list of available entries. */
  (*olmp)->next = avail_orig_line_modifs;
  avail_orig_line_modifs = *olmp;
  *olmp = NULL;
}  /* free_orig_line_modif */


a_source_line_modif_ptr add_source_line_modif(
                          char                      *line_loc,
                          sizeof_t                  num_chars_to_delete,
                          char                      *inserted_text,
                          char                      *end_inserted_text)
/*
Allocate a source line modification entry, put "line_loc",
"num_chars_to_delete", "inserted_text", and "end_inserted_text" in its
like-named fields, set its other fields to default values, add it to
the front of the source_line_modif_list, and return a pointer to it.
line_loc is NULL to indicate an insertion before the first character
of curr_source_line.  Inserted_text and end_inserted_text are passed in
as NULL when the caller plans to set those fields explicitly later.
Entries of this kind are used to record changes made during
preprocessing, such as deletion of comments and expansion of macro
invocations.
*/
{
  a_source_line_modif_ptr slmp;

  if (avail_source_line_modifs != NULL) {
    /* Reuse a freed entry. */
    slmp = avail_source_line_modifs;
    avail_source_line_modifs = avail_source_line_modifs->next;
  } else {
    /* Allocate a new entry. */
    slmp = (a_source_line_modif_ptr)alloc_fe(sizeof(a_source_line_modif));
#if DEBUG
    num_source_line_modifs_allocated++;
#endif /* DEBUG */
  }  /* if */
  slmp->next                = source_line_modif_list;
  slmp->line_loc            = line_loc;
  slmp->num_chars_to_delete = num_chars_to_delete;
  slmp->is_isolated_text    = FALSE;
  slmp->is_for_comment      = FALSE;
  slmp->inserted_text       = inserted_text;
  slmp->end_inserted_text   = end_inserted_text;
  slmp->assoc_macro         = (a_macro_def_ptr)NULL;
  /* Give this entry a sequence id indicating the "time" at which it was
     added. */
  slmp->sequence_id         = ++sequence_id_for_source_line_modifs;
  slmp->assoc_copy_modif    = (a_source_line_modif_ptr)NULL;
  if (line_loc != NULL) {
    /* Normal case, line_loc points to the point of insertion.  Save the
       original character, and replace it with a marker that will call
       attention to the source modification. */
    slmp->orig_char         = *line_loc;
    *line_loc               = ATTENTION_MARKER;
  } else {
    /* Insertion before the first character of curr_source_line. */
    slmp->orig_char         = ' ';
    /* Save a pointer to this entry so that it can be found easily. */
#if CHECKING
    if (line_start_source_line_modif != NULL) {
      /* There should only be one of these. */
      internal_error(
          "add_source_line_modif: more than one line_start_source_line_modif");
    }  /* if */
#endif /* CHECKING */
    line_start_source_line_modif = slmp;
  }  /* if */
  /* Add the entry to the front of the list.  (Adding to the front helps
     keep those entries most likely to be referenced where they will be
     found most quickly.) */
  source_line_modif_list = slmp;
  no_modifs_to_curr_source_line = FALSE;

  return(slmp);
}  /* add_source_line_modif */


/*
Add a source line modification entry to indicate deletion of num_chars
characters starting at line_loc.  The inserted_chars area is used for
the zero-length replacement string.  for_comment is TRUE if the
modification is due to a comment.
*/
#define add_deletion_source_line_modif(line_loc, num_chars, for_comment) \
{ a_source_line_modif_ptr dslmp; \
  dslmp = add_source_line_modif(line_loc, num_chars, \
                                (char *)NULL, (char *)NULL); \
  *dslmp->inserted_chars = '\0'; \
  dslmp->inserted_text = dslmp->end_inserted_text = dslmp->inserted_chars; \
  dslmp->is_for_comment = for_comment; \
}  /* add_deletion_source_line_modif */


/*
Add a source line modification to indicate replacement of num_chars
characters staring at line_loc by a space.  This is used for deletion
of comments.  A different space string must be used for each comment
in the current line so that one can get back from each one to the
right place based only on line position; for this reason, the
space-null string is placed in the a_source_line_modif entry.
*/
#define replace_source_string_by_space(line_loc, num_chars) \
{ a_source_line_modif_ptr rslmp; \
  rslmp = add_source_line_modif(line_loc, num_chars, \
                                (char *)NULL, (char *)NULL); \
  rslmp->inserted_chars[0] = ' '; \
  rslmp->inserted_chars[1] = '\0'; \
  rslmp->inserted_text     = rslmp->inserted_chars; \
  rslmp->end_inserted_text = rslmp->inserted_chars+1; \
  rslmp->is_for_comment    = TRUE; \
}  /* replace_source_string_by_space */


void free_source_line_modif(a_source_line_modif_ptr *slmp)
/*
Free the source line modification entry pointed to by *slmp, and set *slmp
to NULL.
*/
{
  /* Add the entry to the front of the list of available entries. */
  (*slmp)->next = avail_source_line_modifs;
  avail_source_line_modifs = *slmp;
  /* Clear the parameter so that the pointer cannot be inadvertently
     used again. */
  *slmp = NULL;
}  /* free_source_line_modif */


void rem_source_line_modif(a_source_line_modif_ptr slmp)
/*
Remove the source line modification pointed to by *slmp from the
source_line_modif_list.  The entry is not freed.
*/
{
  a_source_line_modif_ptr prev_slmp;

  /* Remove this entry from the source_line_modif_list. */
  if (slmp == source_line_modif_list) {
    /* The entry is first on the list, so removing it is easy. */
    source_line_modif_list = slmp->next;
  } else {
    /* Find the entry preceding this one, and link around this entry. */
    for (prev_slmp = source_line_modif_list;
         prev_slmp->next != slmp;
         prev_slmp = prev_slmp->next) {};
    prev_slmp->next = slmp->next;
  }  /* if */
  slmp->next = NULL;
  /* If this entry was line_start_source_line_modif (a special entry
     inserted preceding the first character of curr_source_line), clear
     that pointer now, because there is no longer such an entry. */
  if (line_start_source_line_modif == slmp) {
    line_start_source_line_modif = NULL;
  } else {
#if CHECKING
    if (slmp->line_loc == NULL) {
      internal_error("rem_source_line_modif: line_loc NULL");
    }  /* if */
#endif /* CHECKING */
    /* Restore the original character (thus removing the attention marker
       character put in when this entry was added). */
    *(slmp->line_loc) = slmp->orig_char;
  }  /* if */
}  /* rem_source_line_modif */


a_source_line_modif_ptr assoc_source_line_modif(char *loc_in_line)
/*
Find the source line modification entry that defines the character location
loc_in_line.  The location must be one that appears in a source modification
(i.e., the caller probably has had to check already that
within_curr_source_line(loc_in_line) == FALSE).
*/
{
  register a_source_line_modif_ptr slmp, prev_slmp;

  for (prev_slmp = NULL, slmp = source_line_modif_list;
       ;
       prev_slmp = slmp, slmp = slmp->next) {
#if CHECKING
    if (slmp == NULL) {
      internal_error("assoc_source_line_modif: bad address");
    }  /* if */
#endif /* CHECKING */
    if (ptr_in_range(loc_in_line, slmp->inserted_text,
                                  slmp->end_inserted_text+1)) {
      /* loc_in_line falls within this entry's text.  Move the entry to 
         the front of the list to speed up other searches. */
      if (prev_slmp != NULL) {
        prev_slmp->next = slmp->next;
        slmp->next = source_line_modif_list;
        source_line_modif_list = slmp;
      }  /* if */
      break;
    }  /* if */
  }  /* for */

  return(slmp);
}  /* assoc_source_line_modif */


a_source_line_modif_ptr nested_source_line_modif(char *loc_in_line)
/*
*loc_in_line contains an ATTENTION_MARKER, indicating that the text at that
point is altered by a source line modification entry.  Find the entry,
and return a pointer to it.
*/
{
  register a_source_line_modif_ptr slmp;

  for (slmp = source_line_modif_list; ; slmp = slmp->next) {
#if CHECKING
    if (slmp == NULL) {
      internal_error("nested_source_line_modif: bad address");
    }  /* if */
#endif /* CHECKING */
    if (slmp->line_loc == loc_in_line) break;
  }  /* for */

  return(slmp);
}  /* nested_source_line_modif */


/*ARGSUSED*/ /* <-- because "kind" is not used in some versions. */
void gen_pp_line_info(char kind,
		      int  increment)
/*
Write a line-identification directive to f_pp_output as part of preprocessor
output.  It should identify the current line plus "increment" lines.
The kind character is the third operand: '1' for entry into a file,
'2' for exit from a file, and ' ' for anything else.  This routine should
only be called when generate_pp_output is TRUE.
*/
{
  if (gen_line_info_in_pp_output) {
    /* Put out a line-identifying directive.  The form is
         #line line-number "file-name"
       or, in pcc mode,
         # line-number "file-name" kind
       This is similar to, but is not, a #line directive.  The "kind"
       is not always wanted (the SUN cc generates it, but not all pcc-based
       compilers do); the flag GEN_EXTRA_LINE_ID_INFO controls whether or
       not it is generated. */
    if (C_dialect != C_dialect_pcc) {
      /* ANSI version. */
      fprintf(f_pp_output, "#line %lu \"%s\"",
                           (a_line_number)(curr_ise->line_number+increment),
                           curr_ise->file_name);
    } else {
      /* pcc version. */
      fprintf(f_pp_output, "# %lu \"%s\"",
                           (a_line_number)(curr_ise->line_number+increment),
                           curr_ise->file_name);
#if GEN_EXTRA_LINE_ID_INFO
      if (kind != ' ') {
        putc(' ', f_pp_output);
        putc(kind, f_pp_output);
      }  /* if */
#endif /* GEN_EXTRA_LINE_ID_INFO */
    }  /* if */
    putc('\n', f_pp_output);
    next_seq_in_pp_output = curr_seq_number + increment;
  }  /* if */
}  /* gen_pp_line_info */


/*
Execute "statement" if a blank should be put out before the first character
of a new token to separate it from the previous token.  This is used
in producing preprocessing output and raw listing output, in cases
where token confusion might result without the blank.  token_start is TRUE
if this is the start of a new token; if so, ch is the first character
of the new token, and prev_ch is the previous character.  prev_ch is
updated on return (always).  token_start is reset to FALSE if it was TRUE.
In pcc mode, no blank is ever put out.

NOTE:  This check needs to be pretty fast.  It is only run when
preprocessing or raw listing output is being created, and then
only when there are expansions.  Still, it gets done a lot.

If this is the first character following a token mark, see if the
previous character (prev_ch) and the current character (ch) could
appear next to each other in a token.  If so, a blank is put out
to separate them.  The decision is made conservatively -- if there
is any doubt, or if it's too hard to figure out, the blank is put out.
Start by getting the lexical category of the two characters.
If either is a "singleton", i.e., a character that is always its own
token and not part of another, then no space is needed.
*/
#define token_separator_blank_if_needed(ch, prev_ch, token_start, statement) \
{ a_byte cat_ch, cat_prev_ch;                                         \
  if (token_start) {                                                  \
    token_start = FALSE;                                              \
    if (C_dialect != C_dialect_pcc) {                                 \
      if ((cat_prev_ch = pp_lexical_category[prev_ch-CHAR_MIN]) ==    \
                                                         PLC_SINGLETON || \
          (cat_ch = pp_lexical_category[ch-CHAR_MIN]) == PLC_SINGLETON) { \
        /* At least one is a singleton, so no space is needed. */     \
      } else if (cat_prev_ch != cat_ch &&                             \
        /* The two characters have different categories (i.e., one is \
           a character that can appear in identifiers or pp-numbers,  \
           and the other is not).  We probably do not need the space. \
           However, check for one bizarre special case having to do   \
           with pp-numbers (3.1.8): "e" or "E" followed by "+" or "-" \
           can appear in a pp-number. */                              \
                 ((prev_ch != 'e' && prev_ch != 'E') ||               \
                  (ch != '+' && ch != '-'))) {                        \
        /* No space is needed. */                                     \
      } else {                                                        \
        /* An extra space to separate tokens is needed. */            \
        statement;                                                    \
      }  /* if */                                                     \
    }  /* if */                                                       \
  }  /* if */                                                         \
  prev_ch = ch;                                                       \
}  /* token_separator_blank_if_needed */


void gen_pp_output_for_curr_line(void)
/*
Write out the current line to f_pp_output (preprocessing output), if
necessary.  This routine should be called only if generate_pp_output
is TRUE.
*/
{
  register char                    *loc_in_line;
  register char                    ch;
  register a_source_line_modif_ptr slmp;
           char                    prev_ch;
           a_boolean               token_start;

  /* do_not_put_curr_line_in_pp_output is TRUE if there is no current
     source line (as at the start of source), or if the line should not
     be put out (for example, because it's a preprocessing directive). */
  if (!do_not_put_curr_line_in_pp_output) {
    /* See if the new line immediately follows the line previously written.
       If not, put out a directive to indicate the new line's position.
       If the previous output line did not end with a newline,
       line numbering can't be adjusted at this point. */
    if (curr_seq_number != next_seq_in_pp_output &&
        prev_pp_output_line_was_complete) {
      if (curr_seq_number <= next_seq_in_pp_output+5) {
        /* Optimization -- For changes of a small number of lines,
           it is more efficient to put out one or more blank lines to
           move up to the desired line number.  This is smaller in the
           output file, and also more efficient to process on the
           receiving end.  Note that if line position information is not
           wanted in the output, the blank lines are not put out, since
           they serve no real purpose. */
        while (curr_seq_number != next_seq_in_pp_output) {
          if (gen_line_info_in_pp_output) putc('\n', f_pp_output);
          next_seq_in_pp_output++;
        }  /* while */
      } else {
        /* Larger skip; put out a line-identifying directive. */
        gen_pp_line_info(' ', 0);
      }  /* if */
    }  /* if */
    /* Put out the previous line itself.  The text in curr_source_line
       has trigraphs and line splices processed, but is not correct for
       preprocessing output if there are in it any comments to be deleted
       or macros to be expanded.  For those cases, the preprocessed line
       must be constructed by using the information in
       source_line_modif_list. */
    if (source_line_modif_list == NULL) {
      /* For the common case, output the line quickly. */
      if (fputs(curr_source_line, f_pp_output) == EOF) {
        /* Error in writing the pp output file.  This check is done on 
           most lines and supplements the check done when the file is closed.
           Checking here is so that a disk full error is caught fairly
           quickly. */
        str_catastrophe(ec_file_write_error, "preprocessing output");
      }  /* if */
      next_seq_in_pp_output++;
      prev_pp_output_line_was_complete = TRUE;
    } else {
      /* Construct the preprocessing output from the source line and the
         list of modifications.  Start at the beginning, and scan through
         characters.  Remove end-of-token markers.  On hitting attention
         markers, find and process the associated source line modification
         entries.  On hitting a null, leave a source line modification entry
         or (if not in a modification entry) stop. */
      /* The logic here is very similar to that in
         gen_expanded_raw_listing_output_for_curr_line.  If you change
         this routine, change the other too. */
      loc_in_line = first_char_of_modified_source_line();
      prev_ch = '\n';
      token_start = FALSE;
      for (;;) {
        /* Fetch the next character, stepping into and out of macro
           expansions. */
        ch = *loc_in_line;
        if (ch == ATTENTION_MARKER) {
          /* Attention marker.  Find the associated source line modification
             and process it. */
          go_into_insertion(slmp, loc_in_line);
        } else if (ch == '\0') {
          /* Null indicates either the end of the whole line or the end
             of a macro expansion.  Find out which. */
          if (within_curr_source_line(loc_in_line)) {
            /* End of the entire logical source line. */
            break;
          } else {
            /* End of a macro.  Pick up after the invocation text. */
            slmp = assoc_source_line_modif(loc_in_line);
            leave_insertion(slmp, loc_in_line);
            token_start = TRUE;
          }  /* if */
        } else {
          if (ch == END_OF_TOKEN_MARKER) {
            /* Do not output end-of-token markers. */
            token_start = TRUE;
          } else {
            /* Normal character. */
            /* Output a blank to separate the character from the previous
               character if necessary to prevent tokenizing confusion. */
            token_separator_blank_if_needed(ch, prev_ch, token_start,
                                            putc(' ', f_pp_output));
            /* Output the character. */
            putc(ch, f_pp_output);
            /* Count newlines.  This is done inside the loop because there are
               cases where no newline is output (two lines are joined in the
               output), and cases where one line has two newlines, and we
               don't wan't to miscount. */
            prev_pp_output_line_was_complete = (ch == '\n');
            if (prev_pp_output_line_was_complete) next_seq_in_pp_output++;
          }  /* if */
          loc_in_line++;
        }  /* if */
      }  /* for */
    }  /* if */
    /* Make sure this line is written only once. */
    do_not_put_curr_line_in_pp_output = TRUE;
  }  /* if */
}  /* gen_pp_output_for_curr_line */


void gen_rlisting_line_info(char kind)
/*
Generate line information for the raw listing file (which is optionally
generated as input to a program that will generate an interspersed listing).
kind is '1' when going into an include file, '2' after having left an include
file, and ' ' for any other cases (like for the primary file and for #line
directives).  This routine should only be called when f_raw_listing
is non-NULL.
The slightly convoluted name of this routine is due to 8-char external
uniqueness requirements.  Sometimes there's no good way to get a unique name.
*/
{
  /* Generate line information for the raw listing file.  The form is
       L line-number "file-name" kind
  */
  fprintf(f_raw_listing, "L %lu \"%s\"",
                         (a_line_number)(curr_ise->line_number+1),
                         curr_ise->file_name);
  if (kind != ' ') {
    putc(' ', f_raw_listing);
    putc(kind, f_raw_listing);
  }  /* if */
  putc('\n', f_raw_listing);
}  /* gen_rlisting_line_info */


static void write_orig_line_piece(char *loc_in_line,
                                  char *stop_loc)
/*
Write the characters from *loc_in_line to just before *stop_loc in
the current source line to the raw listing file.  If stop_loc is NULL,
write to the end of the source line.  Write the characters in their original
source form, i.e., if there are macro expansions replace the attention
markers with the original characters.  The caller guarantees that no
orig_line_modif_list modifications apply to the indicated text.
*/
{
  char                    *local_stop_loc;
  a_source_line_modif_ptr slmp;

  for (;;) {
    if (source_line_modif_list == NULL) {
      /* There are no macro modifications, so the text can just be written. */
      local_stop_loc = stop_loc;
    } else if (stop_loc == NULL) {
      /* Writing to end of line, and there are macro modifications to the
         current line.  See if there are any attention markers in the rest
         of the line. */
      local_stop_loc = strchr(loc_in_line, ATTENTION_MARKER);
    } else {
      /* Writing part of the line, and there are macro modifications to the
         current line.  See if there are any attention markers in the part
         of the line we want to write. */
      for (local_stop_loc = loc_in_line;
           local_stop_loc < stop_loc && *local_stop_loc != ATTENTION_MARKER;
           local_stop_loc++) {}
    }  /* if */
    /* Now write whatever is just raw text. */
    if (local_stop_loc == NULL) {
      /* Write to the end of the line.  The end of every line will be written
         by this code, and in most cases the entire line. */
      if (fputs(loc_in_line, f_raw_listing) == EOF) {
        /* Error in writing the raw listing file.  This check supplements 
           the check done when the file is closed.  Checking here is done so
           that a disk full error is caught fairly quickly. */
        str_catastrophe(ec_file_write_error, "raw listing");
      }  /* if */
    } else {
      /* Write a piece of the line. */
      fprintf(f_raw_listing, "%.*s", local_stop_loc-loc_in_line, loc_in_line);
    }  /* if */
    loc_in_line = local_stop_loc;
    /* If the whole requested piece has now been written out, exit the loop. */
    if (loc_in_line == stop_loc) break;
    /* *loc_in_line must be an attention marker.  Find and write the original
       character for that position. */
    slmp = nested_source_line_modif(loc_in_line);
    putc(slmp->orig_char, f_raw_listing);
    loc_in_line++;
    /* If the whole requested piece has now been written out, exit the loop. */
    if (loc_in_line == stop_loc) break;
  }  /* for */
}  /* write_orig_line_piece */


/*
Clear the raw_listing_buffer.
*/
#define clear_raw_listing_buffer()                                    \
{ loc_in_raw_listing_buffer = raw_listing_buffer;                     \
  must_display_raw_listing_buffer = FALSE;                            \
}  /* clear_raw_listing_buffer */


static void expand_raw_listing_buffer(void)
/*
We have run into a source line that won't fit in raw_listing_buffer;
reallocate raw_listing_buffer to make it bigger.
*/
{
  sizeof_t old_size, new_size, loc_offset;
  char     *new_raw_listing_buffer;

  db_enter(4, "expand_raw_listing_buffer");
  old_size = after_end_of_raw_listing_buffer - raw_listing_buffer;
  /* Increase the size of raw_listing_buffer. */
  new_size = old_size + RAW_LISTING_BUFFER_INCREMENTAL_ALLOCATION;
  new_raw_listing_buffer = realloc_general(raw_listing_buffer,
                                           old_size, new_size);
  loc_offset = loc_in_raw_listing_buffer - raw_listing_buffer;
  raw_listing_buffer = new_raw_listing_buffer;
  after_end_of_raw_listing_buffer = raw_listing_buffer + new_size;
  loc_in_raw_listing_buffer = raw_listing_buffer + loc_offset;
  db_exit();
}  /* expand_raw_listing_buffer */


/*
Add a character to the raw listing buffer.  Expand the buffer if necessary.
*/
#define add_char_to_raw_listing_buffer(ch)                            \
{ if (loc_in_raw_listing_buffer == after_end_of_raw_listing_buffer) { \
    expand_raw_listing_buffer();                                      \
  }  /* if */                                                         \
  *loc_in_raw_listing_buffer++ = ch;                                  \
}  /* add_char_to_raw_listing_buffer. */


void gen_expanded_raw_listing_output_for_curr_line(void)
/*
Generate the raw listing file output for the macro-expanded version of
the current source line.  This routine should not be called unless
f_raw_listing != NULL.
*/
{
  register char                    *loc_in_line;
  register char                    ch;
  register a_source_line_modif_ptr slmp;
           char                    prev_ch;
           a_boolean               token_start;

  /* The output line is generated in raw_listing_buffer first, then
     written to output.  This expensive and unfortunate technique is
     required because of cases like
       #pragma hello / *
       * / there
     (where the / * and * / are really comment delimiters, of course).
     For that case, we'll get here with "#pragma hello " with no newline,
     and then later with " there".  The whole line is written when the
     piece with the newline arrives.  That's because the raw listing
     output has to look like
       N#pragma hello / *
       N* / there
       X#pragma hello   there
     Without this trick, the two parts of the "X" line could not be written
     next to one another.
     Another issue:  Lines that contain only comment modifications are not
     written.  However, we could get a first segment of a line (not ending
     in a newline) that contains only comment modifications, and then a
     second segment (ending in a newline) that contains macro modifications.
     The entire line (both segments) must be written out, but we can't
     determine that while processing the first segment, so we have to save it.
  */
  /* One easy speed optimization:  If the line has no modifications of
     any kind (which implies it ends with a newline), and the buffer
     contains no nontrivial modifications (which implies that there is
     no previous line segment that would force the printing of this line),
     just throw the line away and clear the buffer.  This deals with
     a substantial percentage of source lines. */
  if (no_modifs_to_curr_source_line && !must_display_raw_listing_buffer) {
    clear_raw_listing_buffer();
  } else {
    /* The logic here is very similar to that in gen_pp_output_for_curr_line.
       If you change this routine, change the other too. */
    loc_in_line = first_char_of_modified_source_line();
    prev_ch = '\n';
    token_start = FALSE;
    for (;;) {
      /* Fetch the next character, stepping into and out of macro
         expansions. */
      ch = *loc_in_line;
      if (ch == ATTENTION_MARKER) {
        /* Attention marker.  Find the associated source line modification
           and process it. */
        go_into_insertion(slmp, loc_in_line);
        if (!slmp->is_for_comment) {
          /* Keep track of whether or not there are noncomment (i.e., macro)
             modifications in the lines in the buffer. */
          must_display_raw_listing_buffer = TRUE;
        }  /* if */
      } else if (ch == '\0') {
        /* Null indicates either the end of the whole line or the end
           of a macro expansion.  Find out which. */
        if (within_curr_source_line(loc_in_line)) {
          /* End of the entire logical source line. */
          break;
        } else {
          /* End of a macro.  Pick up after the invocation text. */
          slmp = assoc_source_line_modif(loc_in_line);
          leave_insertion(slmp, loc_in_line);
          token_start = TRUE;
        }  /* if */
      } else {
        if (ch == END_OF_TOKEN_MARKER) {
          /* Do not output end-of-token markers. */
          token_start = TRUE;
        } else {
          /* Normal character. */
          /* Output a blank to separate the character from the previous
             character if necessary to prevent tokenizing confusion. */
          token_separator_blank_if_needed(ch, prev_ch, token_start,
                                          add_char_to_raw_listing_buffer(' '));
          /* Output the character. */
          add_char_to_raw_listing_buffer(ch);
        }  /* if */
        loc_in_line++;
      }  /* if */
    }  /* for */
    /* A line containing trigraphs or line splices is considered modified
       and must be displayed. */
    if (orig_line_modif_list != NULL) must_display_raw_listing_buffer = TRUE;
    /* If the last character in the buffer is a newline, we have a complete
       line and we should output it now (that's almost always what will
       happen).  If not, we leave the line to be output on a later call of
       this routine. */
    if (loc_in_raw_listing_buffer != raw_listing_buffer &&
        loc_in_raw_listing_buffer[-1] == '\n') {
      /* Suppress the output if there are no nontrivial modifications in the
         line. */
      if (must_display_raw_listing_buffer) {
        *loc_in_raw_listing_buffer = '\0';
        putc('X', f_raw_listing);
        fputs(raw_listing_buffer, f_raw_listing);
      }  /* if */
      clear_raw_listing_buffer();
    }  /* if */
  }  /* if */
}  /* gen_expanded_raw_listing_output_for_curr_line */


static void gen_raw_listing_output_for_curr_line(void)
/*
Generate raw listing output for the current source line.  This routine should
only be called when f_raw_listing is non-NULL.
*/
{
  an_orig_line_modif_ptr olmp, olmp_next;
  char                   *loc_in_line;

  /* No output if there is no current line. */
  if (curr_raw_listing_line_code != '\0') {
    /* If we are now (at the end of the line) not in an if-skip, force
       the line to be an "N" (normal) line.  This gets #else and #endif
       lines out as "N" lines rather than "S" (skipped) lines. */
    if (!currently_in_pp_if_skip) curr_raw_listing_line_code = 'N';
    /* Put out the start of the line. */
    putc(curr_raw_listing_line_code, f_raw_listing);
    /* Reconstruct the original line and output it. */
    loc_in_line = curr_source_line;
    for (olmp = orig_line_modif_list; olmp != NULL; olmp = olmp->next) {
      /* Process each modification in order. */
      /* Write unaffected text that precedes this modification. */
      write_orig_line_piece(loc_in_line, olmp->line_loc);
      switch(olmp->kind) {
        case olm_trigraph:
          fprintf(f_raw_listing, "??%c", olmp->variant.trigraph_orig_char);
          /* If the trigraph is "??/", which turns into "\", and it's at the
             end of a line, the "\" will indicate a line splice.  In that
             case, the "\" for the line splice should not be put out. */
          olmp_next = olmp->next;
          if (olmp_next != NULL && olmp_next->kind == olm_line_splice &&
              olmp_next->line_loc == olmp->line_loc) {
            /* Partially process the line-splice entry. */
            olmp = olmp_next;
            goto partially_process_line_splice;
          }  /* if */
          /* Normal trigraph. */
          loc_in_line = olmp->line_loc + 1;
          break;
        case olm_line_splice:
          putc('\\', f_raw_listing);
partially_process_line_splice:
          putc('\n', f_raw_listing);
          putc(curr_raw_listing_line_code, f_raw_listing);
          loc_in_line = olmp->line_loc;
          break;
#if CHECKING
        default:
          internal_error(
       "gen_raw_listing_output_for_curr_line: bad orig_modif_list entry kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* for */
    /* Write out the last piece of unaffected text, including the newline.
       In the most common case (no modifications), this writes the entire
       line. */
    write_orig_line_piece(loc_in_line, (char *)NULL);
    /* Write the macro-expanded form of the line if there are modifications. */
    gen_expanded_raw_listing_output_for_curr_line();
    /* Make sure this line is written only once. */
    curr_raw_listing_line_code = '\0';
  }  /* if */
}  /* gen_raw_listing_output_for_curr_line */


void push_input_stack (char                       *file_name,
                       a_directory_name_entry_ptr search_path)
/*
Push the indicated file onto the input stack, so that the next time a line
is read, it will come from that file.  If the file cannot be opened,
generate a catastrophic error and do not return.  search_path gives the
list of directories to be tried, in order, or is NULL if there is no
search path.  file_name must be allocated in IL storage.
*/
{
  a_directory_name_entry_ptr  curr_directory_name_entry;
  char                        *temp_file_name;
  int                         isnum, times_name_appears;
  a_boolean                   not_found = FALSE,
                              bad_format = FALSE,
                              bad_name = FALSE,
			      empty_search_path = FALSE;
  /* Buffer in which directory names and file names are combined.  Longer
     names will bypass the buffer and be allocated directly via
     alloc_il. */
#define BUFFER_SIZE 130
  char                        buffer[BUFFER_SIZE];

  db_enter(2, "push_input_stack");
#if DEBUG
  if (debug_level >= 2) {
    fprintf(f_debug, "file_name = %s\n", file_name);
  }  /* if */
#endif /* DEBUG */
  /* If preprocessing output is being generated, force out the previous
     source line before the input stack information is changed. */
  if (generate_pp_output) {
    gen_pp_output_for_curr_line();
  }  /* if */
  /* If a raw listing file is being generated, force out the previous
     line before the input stack information is changed. */
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */

  /* Check for the need to expand the input stack. */
  if (depth_input_stack+1 == size_input_stack) {
    /* Expand the input stack by reallocating it. */
    int new_size = size_input_stack + INPUT_STACK_INCREMENTAL_ALLOCATION;
    input_stack = (an_input_stack_entry_ptr)realloc_general(
                     (char *)input_stack,
                     (sizeof_t)(size_input_stack*sizeof(an_input_stack_entry)),
                     (sizeof_t)(new_size*sizeof(an_input_stack_entry)));
    size_input_stack = new_size;
    if (depth_input_stack >= 0) curr_ise = &input_stack[depth_input_stack];
  }  /* if */
  /* If the maximum number of files has already been opened, close the
     top-most file in the input stack after remembering its current
     position for later re-opening.  This prevents hogging of system
     resources, and works efficiently for moderate nesting depths. */
  if (depth_input_stack >= MAX_INCLUDE_FILES_OPEN_AT_ONCE) {
    curr_ise->position = ftell(curr_ise->file);
    (void)fclose(curr_ise->file);
    curr_ise->file = NULL;
  }  /* if */
  /* Push the new input stack entry. */
  curr_ise = &input_stack[++depth_input_stack];
  curr_ise->file        = NULL;
  curr_ise->file_name   = file_name;
  curr_ise->full_name   = NULL;
  curr_ise->dir_name    = NULL;
  curr_ise->line_number = 0;
  curr_ise->position    = 0;
  /* Open the new file. */
  if (depth_input_stack == 0 && strcmp(file_name, FILE_NAME_FOR_STDIN) == 0) {
    /* Special code for stdin; no open needed. */
    temp_file_name = file_name;
    curr_ise->file = stdin;
  } else if (depth_input_stack == 0 || is_absolute_file_name(file_name)) {
    /* File name is absolute, so search path is not used. */
    /* Also used for primary source input file; search current directory. */
    temp_file_name = file_name;
    curr_ise->file = open_source_file(temp_file_name,
                                      &not_found, &bad_format, &bad_name);
  } else {
    /* File name is relative, use search path. */
    if (search_path == NULL) {
      /* No search path, so file can't be found.  Use special message
         to make it clearer, since problem may be that there are no -I
         options on the command line. */
      empty_search_path = TRUE;
    } else {
      curr_directory_name_entry = search_path;
      while (curr_directory_name_entry != NULL) {
        /* Try opening the file name with this directory name. */
        temp_file_name = combine_dir_and_file_name(
                                          curr_directory_name_entry->dir_name,
                                          file_name, buffer, BUFFER_SIZE);
        /* Now try opening the file.  Exit the loop on success. */
        if ((curr_ise->file = 
             open_source_file(temp_file_name, 
                         &not_found, &bad_format, &bad_name)) != NULL) break;
        /* File could not be opened.  If simply because the file was not found,
           try the next directory on the search path.  For other errors, exit
           the loop and give an error message. */
        if (!not_found) break;
        curr_directory_name_entry = curr_directory_name_entry->next;
      }  /* while */
    }  /* if */
  }  /* if */
  eof_read_on_curr_input_stream = FALSE;
  curr_input_stream = curr_ise->file;
  if (curr_input_stream != NULL) {
    /* The file was opened successfully. */
    /* Check for recursion of #includes.  This is done by looking through the
       stack for the file name we just opened. */
    times_name_appears = 0;
    for (isnum = depth_input_stack-1; isnum >= 0; isnum--) {
      if (strcmp(input_stack[isnum].full_name, temp_file_name) == 0) {
        /* The entry in the input stack has the same file name as the
           file we just opened.  This is okay once (it has to be), but
           if it happens several times, it probably means recursion. */
        times_name_appears++;
        if (times_name_appears >= 10 /* Arbitrary, must be > 1 */) {
          str_catastrophe(ec_include_recursion, temp_file_name);
        }  /* if */
      }  /* if */
    }  /* for */
    /* If the name is in "buffer", allocate it now.  The names are generated 
       there first because many directory/file name combinations might be tried
       before the right one is found.  We don't allocate space for the name 
       until we find a file of that name. */
    if (temp_file_name == buffer) {
      temp_file_name = alloc_il((sizeof_t)(strlen(buffer)+1));
      (void)strcpy(temp_file_name, buffer);
    }  /* if */
    curr_ise->full_name = temp_file_name;
    /* Save the full name also as the print form of the name. */
    curr_ise->file_name = temp_file_name;
    curr_ise->dir_name = directory_of(temp_file_name);
    /* Create an intermediate file record describing this file.  It is
       useful later in converting sequence numbers into file name/line
       information. */
    record_start_of_source_file(
           depth_input_stack != 0 ? /* Parent file */
             input_stack[depth_input_stack-1].assoc_il_file :
             (a_source_file_ptr)NULL,
           (a_seq_number)seq_number_last_read+1,
           (a_line_number)1,
           curr_ise->file_name,
           temp_file_name,
           &(curr_ise->assoc_il_file));
    /* The two il file pointers start out the same.  They will be made to
       point to distinct entries if a #line directive is processed:
       assoc_il_file will point to the entry for the #line, and
       assoc_actual_il_file will stay as it is (pointing to the entry
       for the file actually being read). */
    curr_ise->assoc_actual_il_file = curr_ise->assoc_il_file;
    /* If generating preprocessing output, put out a line-identifying
       directive for the new file. */
    if (generate_pp_output) {
      /* The entry into the primary source file should not be tagged with
         "1"; that's the way cpp does it. */
      if (depth_input_stack == 0) {
        gen_pp_line_info(' ', 1);
      } else {
        gen_pp_line_info('1', 1);
      }  /* if */
    }  /* if */
    /* If generating raw listing output (for input to a program that
       will generate an interspersed listing), put out a line-information
       record. */
    if (f_raw_listing != NULL) {
      /* The entry into the primary source file should not be tagged with
         "1". */
      if (depth_input_stack == 0) {
        gen_rlisting_line_info(' ');
      } else {
        gen_rlisting_line_info('1');
      }  /* if */
    }  /* if */
    /* If generating makefile dependency information (-M option), write a
       line of the form

       primaryfile.o: includefile.h

       Note that the code here is executed also for the primary source
       file, and we do want that dependency line as well. */
    if (list_makefile_dependencies) {
      fprintf(f_pp_output, "%s: %s\n", object_file_name, curr_ise->file_name);
    }  /* if */
    /* If generating a list of include files (-H option), put out the
       file name.  Do not put out the name of the primary source file. */
    if (list_included_files && depth_input_stack != 0) {
      fprintf(f_pp_output, "%s\n", curr_ise->file_name);
    }  /* if */
    if (C_dialect == C_dialect_pcc) {
      /* If in pcc mode, modify the search rules for #include directives
         found within this source file, so that the directory containing
         the current include file will be searched first. */
      change_primary_include_search_dir(curr_ise->dir_name);
    } else {
      /* If not in pcc mode, keep the base of the preprocessing if stack
         up to date.  Each file's #ifs are kept separate; an #if must
         be ended in the same file in which it began. */
      curr_ise->base_pp_if_stack_depth = base_pp_if_stack_depth =
                                                            pp_if_stack_depth;
    }  /* if */
  } else {
    /* The file could not be opened. */
    if (bad_format) {
      str_catastrophe(ec_source_file_has_bad_format, file_name);
    } else if (bad_name) {
      str_catastrophe(ec_illegal_source_file_name, file_name);
    } else if (empty_search_path) {
      str_catastrophe(ec_empty_include_search_path, file_name);
    } else {
      str_catastrophe(ec_source_file_could_not_be_opened, file_name);
    }  /* if */
  }  /* if */
  db_exit();
}  /* push_input_stack */


static void pop_input_stack(void)
/*
Pop the input stack, and correctly prepare for input from the file
at the next level down.
*/
{
  db_enter(2, "pop_input_stack");
  /* Remember the final sequence number in the file, for sequence number
     mapping purposes. */
  record_end_of_source_file(curr_ise->assoc_actual_il_file,
                            seq_number_last_read);
  /* If a #line is in effect, also enter the final sequence number in the
     entry for that. */
  if (curr_ise->assoc_actual_il_file != curr_ise->assoc_il_file) {
    record_end_of_source_file(curr_ise->assoc_il_file, seq_number_last_read);
  }  /* if */
  /* Close the current input file. */
  (void)fclose(curr_input_stream);
  eof_read_on_curr_input_stream = FALSE;
  /* If preprocessing output is being generated, force out the previous
     source line before the input stack information is changed. */
  if (generate_pp_output) {
    gen_pp_output_for_curr_line();
  }  /* if */
  /* If a raw listing file is being generated, force out the previous
     line before the input stack information is changed. */
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */
  /* Check that all #ifs were closed.  In ANSI, this has to happen at
     the end of each source file.  In pcc, this has to happen only at
     the end of the whole compilation. */
  if (depth_input_stack == 0 || C_dialect != C_dialect_pcc) {
    verify_that_all_pp_ifs_were_closed();
  }  /* if */
  /* Pop the input stack. */
  /* If the stack is empty, there is no current input file any more.
     This happens at the end of the primary source file. */
  if (--depth_input_stack < 0) {
    curr_ise = NULL;
    curr_input_stream = NULL;
  } else {
    curr_ise = &input_stack[depth_input_stack];
    if (curr_ise->file == NULL) {
#if DEBUG
      if (debug_level >= 2) {
        fprintf(f_debug, "re-opening at level %d, name = \"%s\", pos = %ld\n",
                         depth_input_stack, curr_ise->full_name,
                         curr_ise->position);
      }  /* if */
#endif /* DEBUG */
      /* This include file was closed on a push_input_stack to keep down
         the number of open files.  Re-open it and reposition it now. */
      if ((curr_ise->file = reopen_source_file(curr_ise->full_name)) == NULL) {
        /* File could not be re-opened; it was probably deleted since the
           compilation started. */
        catastrophe(ec_source_file_could_not_be_opened);
      }  /* if */
      if (fseek(curr_ise->file, curr_ise->position, SEEK_SET) != 0) {
        /* The seek could not be done.  Again, this implies some change
           in the file since last it was opened. */
        catastrophe(ec_source_file_could_not_be_opened);
      }  /* if */
#if __VMS__ && 0
      /* This change was only necessary for some of the later 4.n versions
	 of VMS, before a library bug was corrected. */
      /* On VMS, ftell returns the position of the start of the previous
         record (i.e., the line containing the #include), so advance to
         the next record. */
      while (fgetc(curr_ise->file) != '\n') {};
#endif /* __VMS__ */
    }  /* if */
    curr_input_stream = curr_ise->file;
    /* If generating preprocessing output, put out a line-identifying
       directive for the new file. */
    if (generate_pp_output) {
      gen_pp_line_info('2', 1);
    }  /* if */
    /* If generating raw listing information (for input to a program that
       will generate an interspersed listing), put out line information. */
    if (f_raw_listing != NULL) {
      gen_rlisting_line_info('2');
    }  /* if */
    if (C_dialect == C_dialect_pcc) {
      /* If in pcc mode, modify the search rules for #include directives
         found within this source file, so that the directory containing
         the current include file will be searched first. */
      change_primary_include_search_dir(curr_ise->dir_name);
    } else {
      /* If not in pcc mode, keep the base of the preprocessing if stack
         up to date.  Each file's #ifs are kept separate; an #if must
         be ended in the same file in which it began. */
      base_pp_if_stack_depth = curr_ise->base_pp_if_stack_depth;
    }  /* if */
  }  /* if */
  db_exit();
}  /* pop_input_stack */


static void expand_curr_source_line(void)
/*
We have run into a source line that won't fit in curr_source_line;
reallocate curr_source_line to make it bigger.
*/
{
  sizeof_t old_size, new_size;
  char     *new_curr_source_line;

  db_enter(4, "expand_curr_source_line");
  old_size = after_end_of_curr_source_line - curr_source_line;
  /* Increase the size of curr_source_line. */
  new_size = old_size + CURR_SOURCE_LINE_INCREMENTAL_ALLOCATION;
  /* Allocate one more byte than required, so that a pointer past the end
     will not have the same address as a pointer to the next object in
     memory. */
  new_curr_source_line = realloc_general(curr_source_line,
                                         (sizeof_t)(old_size+1),
                                         (sizeof_t)(new_size+1));
  /* Update any pointers to the old curr_source_line in the
     curr_source_line data structure. */
  adjust_curr_source_line_structure_after_realloc(curr_source_line,
                                                 after_end_of_curr_source_line,
                                                  new_curr_source_line);
  curr_source_line = new_curr_source_line;
  after_end_of_curr_source_line = curr_source_line + new_size;
  db_exit();
}  /* expand_curr_source_line */


void conv_line_loc_to_source_pos(char              *loc_in_line,
                                 a_source_position *position_var)
/*
Convert a pointer to somewhere in curr_source_line or macro_buffer
(loc_in_line) into the corresponding source sequence number and column
(in position_var).  This is used to develop a position for later use
in error reporting.  This function version is for cases where the processing
need not be very fast; macro_line_loc_to_source_pos should be used
when speed is critical.
*/
{
  char                    *adj_loc_in_line;
  a_source_line_modif_ptr slmp;
  an_orig_line_modif_ptr  olmp                     = orig_line_modif_list;
  char                    *start_of_curr_phys_line = curr_source_line;
  a_seq_number            seq_number               = curr_seq_number;
  int                     trigraph_adjustment      = 0;

  adj_loc_in_line = loc_in_line;
  if (!within_curr_source_line(adj_loc_in_line)) {
    /* If loc_in_line is not in curr_source_line, it must be in a macro
       expansion or macro argument.  Find the location in curr_source_line
       that begins the macro expansion that ultimately generates
       adj_loc_in_line.  This gives us a source line position we can
       convert. */
    do {
      slmp = assoc_source_line_modif(adj_loc_in_line);
      adj_loc_in_line = loc_of_insert(slmp);
    } while (!within_curr_source_line(adj_loc_in_line));
    /* If the topmost source line modification is for a macro, it gives us
       the source position of the macro invocation, and we need do no
       further work.  This is useful for multi-line macro invocations. */
    if (slmp->assoc_macro != NULL) {
      copy_source_position(slmp->macro_invocation_position, *position_var);
      goto have_position;
    }  /* if */
  }  /* if */
  /* We now have in adj_loc_in_line a position within curr_source_line, which
     must be converted to the corresponding source position. */
  if (olmp != NULL) {
    /* There are trigraphs and/or line splices, so the position
       must be determined by finding where adj_loc_in_line falls relative
       to those modifications in the line. */
    do {
      if (adj_loc_in_line < olmp->line_loc) {
        /* This position precedes the current entry, so we now know
           which physical line adj_loc_in_line is in. */
        break;
      } else if (olmp->kind == olm_line_splice) {
        /* If the case that a line splice is followed by the end of the
           logical source line, use the position of the "\" on the current
           line as the error position.  This is useful when the last line
           of a file ends with a backslash. */
        if (*adj_loc_in_line == '\n') break;
        /* Keep track of the current physical line. */
        start_of_curr_phys_line = olmp->line_loc;
        seq_number              = olmp->variant.line_splice_seq_number;
        trigraph_adjustment     = 0;
      } else {
#if CHECKING
        if (olmp->kind != olm_trigraph) {
          internal_error(
                 "conv_line_loc_to_source_pos: bad orig line modification");
        }  /* if */
#endif /* CHECKING */
        /* Keep a column adjustment to compensate for trigraphs. */
        trigraph_adjustment += 2;
      }  /* if */
    } while ((olmp = olmp->next) != NULL);
  }  /* if */
  /* Set the source position now that we know what physical line we 
     are in and how many trigraphs appear before adj_loc_in_line on this
     line. */
  position_var->seq    = seq_number;
  position_var->column = adj_loc_in_line - start_of_curr_phys_line +
                         trigraph_adjustment + 1;
  /* If the character location is the null at the end of curr_source_line,
     back up the column so that it indicates the newline (or is zero, for
     the end of file line case). */
  if (*adj_loc_in_line == '\0') position_var->column--;
have_position:;
}  /* conv_line_loc_to_source_pos */


/*
Convert a pointer to somewhere in curr_source_line or macro_buffer
(loc_in_line) into the corresponding source sequence number and column
(in position_var).  This is used to develop a position for later use
in error reporting.  This macro version is for cases where the processing
must be very fast; conv_line_loc_to_source_pos should be used
when speed is not so critical.

If the position is within curr_source_line (i.e., it's not in a macro
expansion), and there is no modification information of any kind,
curr_source_line is all one line with no trigraphs to perturb the
column numbers.  The position can be determined directly.  This is the
most common case.  If the character location is the null at the end of
curr_source_line, the column is backed up so that it indicates the
newline (or is zero, for the end of file line case).
*/
#define macro_line_loc_to_source_pos(loc_in_line, position_var) \
{ if (no_modifs_to_curr_source_line || \
      (within_curr_source_line(loc_in_line) && \
       orig_line_modif_list == NULL)) { \
    (position_var).seq    = curr_seq_number; \
    (position_var).column = (loc_in_line) - curr_source_line + 1; \
    if (*(loc_in_line) == '\0') (position_var).column--; \
  } else { \
    conv_line_loc_to_source_pos((loc_in_line), &(position_var)); \
  }  /* if */ \
}  /* macro_line_loc_to_source_pos */


static void error_at_line_pos(an_error_code error_code,
                              char          *loc_in_line)
/*
Record the occurrence of the indicated error at the indicated character
position of the current logical source line.
*/
{
  /* Convert the character position into an error position. */
  conv_line_loc_to_source_pos(loc_in_line, &error_position);
  error(error_code);
}  /* error_at_line_pos */


static void warning_at_line_pos(an_error_code error_code,
                                char          *loc_in_line)
/*
Record the occurrence of the indicated warning at the indicated character
position of the current logical source line.
*/
{
  /* Convert the character position into an error position. */
  conv_line_loc_to_source_pos(loc_in_line, &error_position);
  warning(error_code);
}  /* warning_at_line_pos */


/*
Macro to temporarily add newline/null to the current contents of the
source buffer.  Needed when an error is detected while building the
source line.  Without the newline/null, the partial line could not
be properly displayed with the error message.  Fortunately, the errors
that there are occur at the end of lines, so the "partial" line is really
the full line.
*/
#define finish_off_source_line_so_it_can_be_displayed_in_error()      \
{ *loc_in_line = '\n'; *(loc_in_line+1) = '\0'; }


a_boolean read_logical_source_line(a_boolean do_pop_on_end_of_file)
/*
Read the next logical source line into curr_source_line and
the related variables.  A "logical source line" is what results after
trigraph characters (see standard, 2.2.1.1) have been replaced, and
lines ending in newline-backslash have been spliced with the lines
immediately following (see standard, 2.1.1.2).  The logical source
line thus formed is returned in curr_source_line, terminated
with both a newline and a null.  The input is read from curr_input_stream.
Information that allows the mapping of characters in curr_source_line
back to the corresponding source sequence number and column is
maintained in orig_line_modif_list.

If end of file is not encountered, curr_char_loc is pointed at the first
character of the line just read.

If the end of the current input file is reached, then:

1)  If do_pop_on_end_of_file is TRUE, pop_input_stack is called (perhaps
more than once) to get to the next line of input, and that is returned
in the usual way.  If the end of the primary source file is reached
and that file is popped, then after_end_of_all_source is set to TRUE,
an empty line (just a null) is placed in curr_source_line, and
curr_char_loc is pointed at the null.

2)  If do_pop_on_end_of_file is FALSE, then the current line and the
current position within it are left unchanged, and at_end_of_source_file is
set to TRUE.

The return value from this function is at_end_of_source_file ||
after_end_of_all_source -- i.e., TRUE if no current source line was read.
*/
{
  register int    ch;
  register char   *loc_in_line;
  a_boolean       return_value;
  int		  curr_column;
  int             next_ch;
  a_boolean       char_is_trapped = FALSE;
  an_orig_line_modif_ptr
		  olmp;
  a_source_line_modif_ptr
		  slmp;
  sizeof_t        offset_in_line;
  char		  *after_curr_source_line_minus_2 =
                                             after_end_of_curr_source_line - 2;
			/* For checking of buffer overflow -- "-2" to leave
			   room for a newline and null. */

  /* This routine handles translation phases 1 (trigraphs, newlines) and
     2 (line splices) from the description of translation phases in
     2.1.1.2 of the standard. */
  /* If the compiler is being run just to produce preprocessing output,
     and not to compile (i.e., it's supposed to act like cpp), dump the
     previous line of input (possibly modified since being read in) to the
     preprocessing output file. */
  if (generate_pp_output) {
    gen_pp_output_for_curr_line();
  }  /* if */
  /* If a raw listing file is being generated, write out the previous
     line. */
  if (f_raw_listing != NULL) {
    gen_raw_listing_output_for_curr_line();
  }  /* if */
  /* If there are entries on either of the lists indicating modifications
     to the current source line, clear those lists now, since they are for the
     old source line.  We also want the lists empty to start building
     them for the new line. */
  if (orig_line_modif_list != NULL) {
    do {
      olmp = orig_line_modif_list;
      orig_line_modif_list = orig_line_modif_list->next;
      free_orig_line_modif(&olmp);
    }  while (orig_line_modif_list != NULL);
  }  /* if */
  if (source_line_modif_list != NULL) {
    do {
      slmp = source_line_modif_list;
      rem_source_line_modif(slmp);
      free_source_line_modif(&slmp);
    } while (source_line_modif_list != NULL);
  }  /* if */
  no_modifs_to_curr_source_line = TRUE;
start_read_of_line:
  /* Get the first character of the line, checking for end of file in
     doing so.  If eof_read_on_curr_input_stream is already TRUE,
     the end of file has already been read (this handles the case
     where the previous call of this routine read an incomplete last
     line; this call needs to return the end of file indication). */
  if (eof_read_on_curr_input_stream || (ch = getc(curr_input_stream)) == EOF) {
    /* End of file encountered in expected way, i.e., before a line
       has started. */
    eof_read_on_curr_input_stream = TRUE;
    at_end_of_source_file = TRUE;
    if (do_pop_on_end_of_file) {
      /* We are supposed to pop the input stack and attempt again to
         read the next line. */
      pop_input_stack();
      at_end_of_source_file = FALSE;
      if (depth_input_stack >= 0) goto start_read_of_line;
      /* We have popped out of the primary source file; this is the real
         end of file. */
      after_end_of_all_source = TRUE;
      /* This position is treated as a pseudo-line, so increment the
         sequence number to the "after all source" position, and put
         an empty line (just a null) in curr_source_line. */
      curr_seq_number = ++seq_number_last_read;
      loc_in_line = curr_source_line;
      goto return_with_line;
    } else {
      /* We're asked not to do the pop, so just return things as they
         are (at_end_of_source_file is TRUE). */
      goto simple_return;
    }  /* if */
  } else {
    /* Not end of file, read the line. */
    curr_seq_number = ++seq_number_last_read;
    curr_ise->line_number++;
    loc_in_line = curr_source_line;
    /* Read characters until the newline indicating end of line. */
    /* Every attempt is made to make this FAST, since every character of
       the source program passes through this loop.  The assumption is
       that trigraphs will almost never appear, and that line splices will
       not appear too often (mostly in long macro definitions).
       In general, we try to do tests that are cheap in the normal case,
       and we exit out to a general-purpose expensive version of this
       algorithm on anything out of the ordinary.  The exits out are
       by simple goto, to keep even non-executed code out of the inner
       loop, to improve pipelining. */
    if (ch != '\n') {
      do {
        /* Check for question marks.  Presence of 2 in a row suggests there
           may be a trigraph in the line. */
        if (ch == '?') {
          /* One "?", check previous character to see if it is also a "?". */
          if (loc_in_line != curr_source_line &&
              *(loc_in_line-1) == '?') {
            /* On reasonable suspicion of a trigraph, exit to more expensive
               processing code.  Note that this is done before the second
               "? is stored, so that we do not ever store more characters
               than ultimately required, and therefore avoid spurious
               buffer overflows on trigraphs at the ends of very long lines. */
            goto possible_trigraph;
          }  /* if */
        }  /* if */
        /* Check that there is still room in the line buffer.  We have to
           leave room for both the final newline and null. */
        if (loc_in_line == after_curr_source_line_minus_2) {
          /* The line is too long; the buffer must be expanded.  Note that
             after the buffer is expanded we do not return to this loop for
             the current line; the rest of the line is processed in the
             more expensive loop. */
          goto expand_buffer;
        }  /* if */
        /* Put the character into curr_source_line. */
        *loc_in_line++ = ch;
        /* Get next character, check for end of file without newline. */
        if ((ch = getc(curr_input_stream)) == EOF) goto partial_final_line;
        /* Check for newline, which ends loop. */
      } while (ch != '\n');
      /* End of a line containing at least one character.  Check to see
         if the last character is a backslash.  If so, the current line
         should be spliced with the line following. */
      if (*(loc_in_line-1) == '\\') goto line_splice;
    }  /* if */
  }  /* if */

add_newline_and_null_and_return:
  /* Store the final newline and null. */
  *loc_in_line++ = '\n';

return_with_line:
  /* Store the final null. */
  *loc_in_line = '\0';
  /* Set the input character position to the start of the line. */
  curr_char_loc = curr_source_line;
  any_tokens_gotten_from_curr_source_line = FALSE;

simple_return:
  /* The return value is TRUE on any end-of-file case. */
  return_value = at_end_of_source_file || after_end_of_all_source;
  /* The current line should not be put out as preprocessing output
     if there isn't a current line, if it's being skipped in an #if
     or the like, or if it is a line after the first in a preprocessing
     directive (that happens when there are multi-line comments in a
     preprocessing directive).  The flag will be set later for other
     preprocessing-related cases.  If this is a line after the first
     in a directive being passed unchanged to preprocessing output,
     put the line out. */
  if (generate_pp_output) {
    do_not_put_curr_line_in_pp_output = return_value ||
                                        currently_in_pp_if_skip ||
                                        (in_preprocessing_directive &&
                                         !pass_pp_directive_to_output);
  }  /* if */
  if (f_raw_listing != NULL) {
    /* If a raw listing file is being generated, save the line type:
       "N" indicating that this is a source line, "S" if this line
       is part of an #if-skip, or '\0' if there is no source line. */
    curr_raw_listing_line_code = return_value ? '\0' :
                                         (currently_in_pp_if_skip ? 'S' : 'N');
  }  /* if */
#if DEBUG
  if (debug_level >= 1) {
    /* Display the line just read. */
    fprintf(f_debug, "Returning from read_logical_source_line, ");
    fprintf(f_debug, "return value = %d, ", return_value);
    if (after_end_of_all_source) {
      fprintf(f_debug, "\nafter_end_of_all_source = TRUE.\n");
    } else {
      fprintf(f_debug, "seq = %lu\n%s", curr_seq_number, curr_source_line);
      if (debug_level >= 4) {
        /* Dump out the modification list, which shows the location
           of the trigraphs and line splices. */
        for (olmp = orig_line_modif_list; olmp != NULL; olmp = olmp->next) {
          /* Put a caret under the proper character of the source line. */
          fprintf(f_debug, "%*c ", olmp->line_loc-curr_source_line+1, '^');
          switch ((int)olmp->kind) {
            case olm_trigraph:
              fprintf(f_debug, "trigraph: ??%c\n",
                               olmp->variant.trigraph_orig_char);
              break;
            case olm_line_splice:
              fprintf(f_debug, "line splice: seq = %lu\n",
                               olmp->variant.line_splice_seq_number);
              break;
#if CHECKING
            default:
              internal_error(
               "read_logical_source_line: bad orig_modif_list entry kind (2)");
#endif /* CHECKING */
          }  /* switch */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* DEBUG */

  return (return_value);

expand_buffer:
  /* The curr_source_line buffer is too small to fit the next character,
     so go into the expensive loop and expand it. */
  curr_column = loc_in_line - curr_source_line + 1;
  goto entry_for_expand_buffer;

partial_final_line:
  /* The final line of a file does not end with a newline.  Issue a warning,
     add a newline and null to the line, and return. */
  eof_read_on_curr_input_stream = TRUE;
  finish_off_source_line_so_it_can_be_displayed_in_error();
  warning_at_line_pos(ec_last_line_incomplete, loc_in_line);
  goto add_newline_and_null_and_return;

possible_trigraph:
  /* Two "?"s in a row were detected in the first line.  Go into the
     general-purpose algorithm.  Note that we don't actually know that
     the two "?"s are followed by something that makes them a trigraph,
     but that should happen seldom enough that, from an efficiency point
     of view, we don't care. */
  curr_column = loc_in_line - curr_source_line + 1;
  goto entry_for_possible_trigraph;

line_splice:
  /* A backslash has been detected at the end of the first line.
     Go into the general-purpose algorithm. */
  curr_column = loc_in_line - curr_source_line;
  goto entry_for_line_splice;

  /* The general-purpose expensive algorithm.  Handles trigraphs, tracks
     mapping to original source characters.  Efficiency is less important
     in this version. */
line_loop:
  /* Start of a line after the first, first character already read into ch. */
  seq_number_last_read++;
  curr_ise->line_number++;
  curr_column = 0;
  /* Check for an empty line. */
  if (ch != '\n') {
    /* Process characters until a newline is read. */
    do {
      /* Process one character (ch). */
      curr_column++;
      /* Check for trigraphs.  A trigraph is two "?"s followed by another
         character. */
      if (ch == '?') {
        /* One "?", check previous character to see if it is also a "?".
           Note the use of curr_column rather than the start of buffer, since
           a "?" at the end of the previous line should not be counted
           as part of a trigraph in this line (trigraphs are handled in
           translation phase 1, line splices in phase 2). */
        if (curr_column != 1 && *(loc_in_line-1) == '?') {
entry_for_possible_trigraph:
          /* Trigraphs are disabled if the C dialect being compiled is
             not ANSI. */
          if (C_dialect == C_dialect_ANSI) {
            /* Get the next character, the one following the two "?"s. */
            next_ch = getc(curr_input_stream);
            /* Check for the possible third characters of trigraphs.  If one
               is found, replace the three characters by the new one.  If not,
               pass the "?" through, and trap the next character for processing
               on the next time around the loop. */
            /* See standard, 2.2.1.1. */
            switch (next_ch) {
              case '=':  ch = '#'; break;
              case '(':  ch = '['; break;
              case '/':  ch = '\\'; break;
              case ')':  ch = ']'; break;
              case '\'': ch = '^'; break;
              case '<':  ch = '{'; break;
              case '!':  ch = '|'; break;
              case '>':  ch = '}'; break;
              case '-':  ch = '~'; break;
              default:   char_is_trapped = TRUE;
            }  /* switch */
            if (!char_is_trapped) {
              /* Trigraph detected.  Add a modification entry indicating
                 the position of the trigraph, remove the first "?" from the
                 buffer, then go on to store the remapped character. */
              loc_in_line--;
              curr_column++;
              olmp = add_orig_line_modif(olm_trigraph, loc_in_line);
              olmp->variant.trigraph_orig_char = next_ch;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      /* Check that there is still room in the line buffer.  We have to
         leave room for both the final newline and null. */
      if (loc_in_line == after_curr_source_line_minus_2) {
entry_for_expand_buffer:
        /* The line is too long; the buffer must be expanded. */
        offset_in_line = loc_in_line - curr_source_line;
        expand_curr_source_line();
        loc_in_line = curr_source_line + offset_in_line;
        after_curr_source_line_minus_2 = after_end_of_curr_source_line - 2;
      }  /* if */
      /* Put the character into curr_source_line. */
      *loc_in_line++ = ch;
      /* Get next character, check for end of file without newline. */
      if (char_is_trapped) {
        ch = next_ch;
        char_is_trapped = FALSE;
      } else {
        ch = getc(curr_input_stream);
      }  /* if */
      if (ch == EOF) goto partial_final_line;
    } while (ch != '\n');
    /* Check for backslash indicating line-splice.  Go add trailing newline
       and null, and then exit, if no backslash is present. */
    if (*(loc_in_line-1) == '\\') {
entry_for_line_splice:
      /* Remove the backslash in the buffer. */
      loc_in_line--;
      /* Add a modification entry recording the position of the line splice. */
      olmp = add_orig_line_modif(olm_line_splice, loc_in_line);
      olmp->variant.line_splice_seq_number = seq_number_last_read+1;
      /* Begin reading the next line.  It is an error if end of file is
         encountered. */
      if ((ch = getc(curr_input_stream)) != EOF) goto line_loop;
      eof_read_on_curr_input_stream = TRUE;
      /* Backslash at end of last line in a file -- error. */
      finish_off_source_line_so_it_can_be_displayed_in_error();
      error_at_line_pos(ec_last_line_backslash, loc_in_line);
      /* Ignore the backslash, end the logical line at this point. */
    }  /* if */
  }  /* if */
  goto add_newline_and_null_and_return;
  
}  /* read_logical_source_line */


/*
Flag that is TRUE while scanning a pcc-mode half-comment (see routine
below).
*/
static a_boolean in_pcc_mode_half_comment = FALSE;

static void skip_pcc_mode_half_comment(void)
/*
In pcc mode, a comment can be begun by "/" followed by "*" as a result
of macro expansion.  This is a strange sort of comment, however: the
preprocessor does not see it as a comment, and the compiler does.
That's because in pcc, the preprocessor writes out the characters
"/" and "*" and continues processing, and the compiler sees those
later as a comment delimiter.  This is a strange mode.  To implement
it, we skip tokens until the closing delimiter.
*/
{
  a_boolean         saved_fetch_pp_tokens = fetch_pp_tokens;
  a_source_position comment_start_pos;

  in_pcc_mode_half_comment = TRUE;
  /* Determine the start position of the comment in case it's needed
     for an unclosed-comment error. */
  conv_line_loc_to_source_pos(curr_char_loc, &comment_start_pos);
  /* Advance past the "/" and "*". */
  curr_char_loc += 2;
  /* Flush raw pp tokens until the end of the comment. */
  fetch_pp_tokens = TRUE;
  for (;;) {
    /* The "*" and "/" must be fetched as tokens, because they may come
       from macro expansions. */
    (void)get_token();
    while (curr_token == tok_star) {
      /* No white space is allowed between the tokens. */
      skip_white_space();
      (void)get_token();
      if (kind_of_white_space_skipped == 0 && *start_of_curr_token == '/') {
        /* End of comment.  Skip the "*" and "/". */
        curr_char_loc = start_of_curr_token + 1;
        goto end_of_comment;
      }  /* if */
    }  /* while */
    if (curr_token == tok_end_of_source) {
      /* Comment unclosed at end of source. */
      error_position = comment_start_pos;
      error(ec_comment_unclosed_at_eof);
      /* Consider the comment closed. */
      goto end_of_comment;
    }  /* if */
  }  /* for */
end_of_comment:
  in_pcc_mode_half_comment = FALSE;
  fetch_pp_tokens = saved_fetch_pp_tokens;
}  /* skip_pcc_mode_half_comment */


/*
Test whether curr_char_loc is the start of a comment.  It is already
known that *curr_char_loc == '/'.  The test can be tricky if
*curr_char_loc and *(curr_char_loc+1) are "/" and "*" but those
resulted from pasting of tokens in a macro expansion; that case should
not be considered to be the start of a comment.  In pcc mode, the
token pasting rules are different, and the start of a comment is
assumed.
Also tests for "//" in C++ mode.
*/
#define start_of_comment()                                            \
  ((*(curr_char_loc+1) == '*' ||                                      \
    (C_dialect == C_dialect_cplusplus && *(curr_char_loc+1) == '/')) && \
    (within_curr_source_line(curr_char_loc) ||                        \
     (C_dialect == C_dialect_pcc && !in_pcc_mode_half_comment)))


void skip_white_space(void)
/*
Skip over any white space in the input.  White space includes blanks,
horizontal tabs, and comments.  Ordinarily, newline, vertical tab, and
form feed are also considered white space; however, when the global
variable in_preprocessing_directive indicates that we are inside such
a directive, newline is not white space, and vertical tab and form
feed are flagged as errors.  See standard sections 2.1.1.2
(translation phases; this routine handles part of phase 3), 3.1, and
3.8.  The global variable kind_of_white_space_skipped is set to indicate
the kinds of white space that were skipped, if any (this is useful for 
scanning of #defines, where white space is significant).
This routine also handles the special marker characters that appear in
source text (end of token, start of expansion, end of expansion).
*/
{
  register char      ch;
  register int	     kind_skipped;
  char               *comment_start_loc;
  a_boolean          comment_pos_determined;
  a_source_position  comment_start_pos;
  char               *delete_from;
  char               *delete_to;
  a_source_line_modif_ptr
		     slmp;
  a_boolean          delete_only_for_comment;

/* Macro used later to test if comments must be deleted.  Except for the
   keep_comments_in_pp_output switch, this is basically a time optimization --
   we avoid doing the comment deletion if we will not be outputting the
   modified line text in some way (as preprocessing or raw listing output). */
#if ASM_FUNCTION_ALLOWED
/* If asm functions are allowed, also delete comments if inside an asm
   function body. */
#define NEED_TO_DELETE_COMMENT                                        \
  ((((generate_pp_output && !do_not_put_curr_line_in_pp_output) ||    \
     f_raw_listing != NULL) &&                                        \
    !keep_comments_in_pp_output) ||                                   \
   in_asm_function_body)
#else /* !ASM_FUNCTION_ALLOWED */
#define NEED_TO_DELETE_COMMENT                                        \
  (((generate_pp_output && !do_not_put_curr_line_in_pp_output) ||     \
    f_raw_listing != NULL) &&                                         \
   !keep_comments_in_pp_output)
#endif /* ASM_FUNCTION_ALLOWED */

  /* Forget that we know where the current token's characters are. */
  start_of_curr_token = NULL;
  kind_skipped = 0;  /* No white space skipped so far. */
white_space_loop:
  /* Examine the current character to see if it is white space.  Throw away
     spaces and horizontal tabs quickly, since they are always white space
     and there are lots of them (so we want that to be fast). */
  if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
    kind_skipped |= WHITE_SPACE_OTHER;
    do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
  }  /* if */
  switch (ch) {
    case '\n':  /* Newline. */
      /* Newline is white space ordinarily, but a token to be returned if
         in a preprocessing directive. */
      if (in_preprocessing_directive) goto end_skip;
#if ASM_FUNCTION_ALLOWED
      /* Newline is also a token in asm functions. */
      if (in_asm_function_body) goto end_skip;
#endif /* ASM_FUNCTION_ALLOWED */
      /* The newline character is white space, and is being thrown away. */
      kind_skipped |= WHITE_SPACE_OTHER;
      curr_char_loc++;
      /* Fall through to process the null that must follow this newline. */
    case '\0':
      /* Null.  Usually, this indicates the end of a line.  However, it
         can also mean the end of source, or the end of the text of a macro
         expansion, so check for that. */
      /* Note that kind_skipped is not set for the null itself, since
         the null is not white space. */
      /* Check to see if the hanging deletion flag is set. */
      if ((delete_from = delete_source_from_loc) != NULL) {
        /* Source from the indicated position to the end of the line or
           macro is to be deleted.  This is typically because a macro
           invocation argument list is being scanned.  Keep the final null,
           but delete the newline. */
        delete_to = curr_char_loc - 1;
        if (delete_from <= delete_to) {
          add_deletion_source_line_modif(delete_from,
                                         (sizeof_t)(delete_to-delete_from+1),
                                         /*for_comment=*/FALSE);
        }  /* if */
        /* Clear the flag to be sure it is cleared for error exit cases.
           It will be set later to the location of further text if there is
           any. */
        delete_source_from_loc = NULL;
      }  /* if */
      if (within_curr_source_line(curr_char_loc)) {
        /* curr_char_loc falls within curr_source_line, so this is not
           the end of a macro expansion. */
        /* We have to read a new logical source line now. 
           read_logical_source_line will pop the input stack if end of
           file is encountered.  On the final end of file, TRUE is returned,
           and we want to exit this routine.  If we are already at the
           final end of file, don't try reading again, just exit. */
        if (after_end_of_all_source ||
            read_logical_source_line(/*do_pop_on_end_of_file=*/TRUE)) {
          /* End of file, end the white-space skip. */
          goto end_skip;
        } /* if */
        /* Not end of file, keep checking for white space in the new line. */
      } else {
        /* End of the expansion text for a macro.  Find the character
           location of the character following the macro invocation, and
           continue there. */
        slmp = assoc_source_line_modif(curr_char_loc);
        /* See if the current position is part of the text of a macro argument
           being macro-expanded; such text is expanded in isolation from
           the rest of the source file (see 3.8.3.1).  In that case, the
           null is significant, and is returned to the caller. */
        if (slmp->is_isolated_text) goto end_skip;
        /* Normal case; continue with the text following the macro
           invocation. */
        leave_insertion(slmp, curr_char_loc);
      }  /* if */
      /* If the hanging deletion flag was set, reset it to the new
         current position. */
      if (delete_from != NULL) {
        delete_source_from_loc = curr_char_loc;
      }  /* if */
      goto white_space_loop;
    case '\f':  /* Form feed. */
    case VERTICAL_TAB_CHARACTER:
      /* These are white space if not in a preprocessing directive.  Inside
         a preprocessing directive, it is implementation-defined whether
         or not they are replaced by a space (conceptually), and that's
         what this implementation does, so they are allowed there as well. */
      curr_char_loc++;
      kind_skipped |= WHITE_SPACE_OTHER;
      goto white_space_loop;
    case END_OF_TOKEN_MARKER:
      /* Marker put into text by preprocessing of macros, to force the same
         interpretation of token boundaries as during the macro definition.
         At this level, should be ignored.  Note that kind_skipped is not
         set, since this is not white space. */
      curr_char_loc++;
      goto white_space_loop;
    case ATTENTION_MARKER:
      /* Marker placed into source text to provide a cue to the fact that
         a source modification (probably a text replacement due to a
         macro expansion) begins here. */
      if (delete_source_from_loc != NULL) {
        /* Source from the indicated position to the current position
           is to be deleted.  This is typically because a macro
           invocation argument list is being scanned. */
        delete_to = curr_char_loc - 1;
        if (delete_source_from_loc <= delete_to) {
          add_deletion_source_line_modif(delete_source_from_loc,
                                (sizeof_t)(delete_to-delete_source_from_loc+1),
                                         /*for_comment=*/FALSE);
        }  /* if */
      }  /* if */
      /* Find the appropriate source line modification entry, and begin
         scanning text in that entry.  kind_skipped is not set, since this
         is not white space. */
      go_into_insertion(slmp, curr_char_loc);
      /* If the hanging deletion flag is set, reset it to the new
         current position. */
      if (delete_source_from_loc != NULL) {
        delete_source_from_loc = curr_char_loc;
      }  /* if */
      goto white_space_loop;
    case '/':
      /* Possible start of a comment.  Check for "*" following the "/".
         (Or another "/" in C++ mode.) */
      if (!start_of_comment()) goto end_skip;
      /* This is the start of a comment. */
      if (C_dialect == C_dialect_pcc &&
          !within_curr_source_line(curr_char_loc)) {
        /* In pcc mode, the token pasting rules are different, and the
           characters are considered to start a comment of sorts.  However,
           in pcc it's a comment to the compiler but not a comment to the
           preprocessor.  Therefore, we have to skip tokens until the
           closing delimiter.  This is a strange mode. */
        skip_pcc_mode_half_comment();
      } else if (*(curr_char_loc+1) == '/') {
        /* C++ comment -- //.  Skip to end of line. */
        comment_start_loc = curr_char_loc;
        /* Advance past the first "/". */
        curr_char_loc++;
        /* Advance to the end of line. */
        do {} while (*(++curr_char_loc) != '\n');
        /* Delete the comment entirely. */
        add_deletion_source_line_modif(comment_start_loc,
                                   (sizeof_t)(curr_char_loc-comment_start_loc),
                                       /*for_comment=*/TRUE);
      } else {
        /* C-style comment. */
        /* Save the position of the start of the comment.  Do not determine
           the source position yet (it may be costly, and it's only needed
           if there is an error). */
        comment_start_loc = curr_char_loc;
        comment_pos_determined = FALSE;
        /* Advance past the "/" and "*". */
        curr_char_loc += 2;
        if (!in_preprocessing_directive && !currently_in_pp_if_skip) {
          /* Look for the special lint comments "notreached", "argsused", and
             "varargs" (all of those in caps -- written here in lower case 
             letters because UNIX lint finds the keywords even in the middle of
             comments).  Ignore blanks or tabs before the keyword (lint
             actually allows any white space, and even other identifiers,
             before the keyword; this compiler doesn't). */
          if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
            do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
          }  /* if */
          if (ch == 'N' && curr_char_loc[1] == 'O' &&
              strncmp(curr_char_loc+2, "TREACHED", 8) == 0 &&
              !isalpha(curr_char_loc[10])) {
            /* The special lint comment "notreached" (in caps), asserting that
               the code following is unreachable. */
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "lint NOTREACHED comment\n");
            }  /* if */
#endif /* DEBUG */
            lint_notreached_flag = TRUE;
            curr_char_loc += 10;
          } else if (ch == 'A' && curr_char_loc[1] == 'R' &&
                     strncmp(curr_char_loc+2, "GSUSED", 6) == 0 &&
                     !isalpha(curr_char_loc[8])) {
            /* The special lint comment "argsused" (in caps), asserting that
               the arguments of the function following are all used (or more
               accurately, that it's okay that they aren't all used). */
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "lint ARGSUSED comment\n");
            }  /* if */
#endif /* DEBUG */
            lint_argsused_flag = TRUE;
            curr_char_loc += 8;
          } else if (ch == 'V' && curr_char_loc[1] == 'A' &&
                     strncmp(curr_char_loc+2, "RARGS", 5) == 0 &&
                     !isalpha(curr_char_loc[7])) {
            /* The special lint comment "varargs" (in caps), asserting that
               the function following takes a variable number of arguments.
               If a number follows the keyword, it is the number of arguments
               that are fixed and should always be present and checked; 0 is
               assumed if the number is omitted. */
            curr_char_loc += 7;
            lint_varargs_count = 0;
            /* Skip any blanks, then scan a decimal number.  lint itself only
               looks at one digit. */
            while (*curr_char_loc == ' ') curr_char_loc++;
            while (isdigit(ch = *curr_char_loc)) {
              if (lint_varargs_count > SHRT_MAX / 10) {
                lint_varargs_count = 0;
                break;
              }  /* if */
              lint_varargs_count *= 10;
              if (lint_varargs_count > SHRT_MAX - (ch - '0')) {
                lint_varargs_count = 0;
                break;
              }  /* if */
              lint_varargs_count += ch - '0';
              curr_char_loc++;
            }  /* while */
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "lint VARARGS comment, count = %d\n",
                               lint_varargs_count);
            }  /* if */
#endif /* DEBUG */
          }  /* if */
        }  /* if */
        /* Scan to the * / marking the end.  This may involve reading extra
           lines, and it is interesting to note that the newlines passed in
           those cases do not terminate preprocessing directives. */
        /* Again, a speed note:  All text inside comments goes through this
           loop, so it should be very fast. */
        while ((ch = *curr_char_loc) != '*' || *(curr_char_loc+1) != '/') {
          if (ch == '\n') {
            if (!comment_pos_determined) {
              /* End of the first line of the comment. */
              /* Determine the source position for the start of the comment
                 now, before we lose the current source line. */
              macro_line_loc_to_source_pos(comment_start_loc,
                                           comment_start_pos);
              comment_pos_determined = TRUE;
            }  /* if */
            /* We are supposed to delete the characters of the source line
               from delete_source_from_loc on, if it is non-NULL.  This would
               be, for example, because we are scanning a macro invocation. */
            delete_from = delete_source_from_loc;
            if (delete_from != NULL) {
              /* Reset the "from" position for subsequent lines. */
              delete_source_from_loc = curr_source_line;
              delete_only_for_comment = FALSE;
            } else if (NEED_TO_DELETE_COMMENT) {
              /* Delete the characters of the comment if writing preprocessor
                 output with the comments deleted.  The newline will be kept,
                 and therefore the comment is deleted entirely rather than
                 replaced by one blank -- since there is already white space
                 here in the form of the newline. */
              delete_from = comment_start_loc;
              delete_only_for_comment = TRUE;
            }  /* if */
            if (delete_from != NULL) {
              /* Delete the required characters.  Leave the newline and null.
                 If this comment is part of a preprocessing directive,
                 delete the newline as well so that multi-line directives
                 will become one-line directives. */
              if (!in_preprocessing_directive) {
                delete_to = curr_char_loc-1;
              } else {
                delete_to = curr_char_loc;
              }  /* if */
              if (delete_from <= delete_to) {
                add_deletion_source_line_modif(delete_from,
                                           (sizeof_t)(delete_to-delete_from+1),
                                      /*for_comment=*/delete_only_for_comment);
              }  /* if */
            }  /* if */
            /* Read a new line.  Note the parameter asking that the input
               stack not be popped, since we need to know about ends of files
               (it is an error for a comment to be unclosed at the end of
               the file in which it was opened). */
            if (read_logical_source_line(/*do_pop_on_end_of_file=*/FALSE)) {
              /* End of file encountered, unclosed comment. */
              error_position = comment_start_pos;
              error(ec_comment_unclosed_at_eof);
              /* Consider the comment closed. */
              goto end_of_comment;
            }  /* if */
            /* Reset the start of comment location for subsequent lines. */
            comment_start_loc = curr_source_line;
          } else {
            /* Not a newline. */
            /* Check for possible nested comment, issue a warning.  This
               helps catch unclosed comments. */
            if (ch == '/' && *(curr_char_loc+1) == '*') {
              warning_at_line_pos(ec_nested_comment, curr_char_loc);
            }  /* if */
            /* Advance to the next character position. */
            curr_char_loc++;
          }  /* if */
        }  /* while */
        /* End of comment.  Take the "*" and "/", go back to throw away more
           white space. */
        curr_char_loc += 2;
        if (NEED_TO_DELETE_COMMENT && delete_source_from_loc == NULL) {
          /* Delete the characters of the comment if writing preprocessor
             output with the comments deleted.  Under ANSI rules, the comment
             must be replaced by one space if there is no other white space
             here (2.1.1.2, phase 3); we replace it with a blank always.
             Under pcc rules, the comment is always deleted entirely.
             The deletion here is suppressed if we are deleting everything 
             up to this point anyway (delete_source_from_loc != NULL). */
          if (C_dialect == C_dialect_pcc) {
            /* pcc mode; delete the comment entirely. */
            add_deletion_source_line_modif(comment_start_loc,
                                   (sizeof_t)(curr_char_loc-comment_start_loc),
                                           /*for_comment=*/TRUE);
          } else {
            /* ANSI mode; replace the comment with a space. */
            replace_source_string_by_space(comment_start_loc,
                                  (sizeof_t)(curr_char_loc-comment_start_loc));
          }  /* if */
        }  /* if */
end_of_comment:;
      }  /* if */
      /* Remember that a comment was skipped in white space.  Note that
         this must be done after the test of kind_skipped above. */
      kind_skipped |= WHITE_SPACE_COMMENTS;
      goto white_space_loop;
  }  /* switch */
end_skip:
  /* "Return" the mask of kinds of white space skipped. */
  kind_of_white_space_skipped = kind_skipped;
}  /* skip_white_space */


static a_token_kind scan_number(void)
/*
Scan a numeric token (integer or floating constant).  Return the kind of
token.
*/
{
  register char	ch;
  register enum {k_decimal, k_octal, k_hex, k_float}
		kind;
  register a_token_kind 
		ctoken;
  a_boolean     err = FALSE;
  char		*err_pos;
  an_error_code	err_code;

  /* Collect the characters of the constant, and figure out where it
     ends.  In the process, figure out what kind of token it is.
     This is done according to the syntax for integer constants (3.1.3.2)
     and floating constants (3.1.3.1), rather than according to the
     pp-number syntax (3.1.8).  */
  if (*curr_char_loc == '0') {
    /* First digit is a zero.  This is probably an octal constant (like
       0777) or a hex constant (like 0xfff), but it could also be a
       solitary 0 (which is octal, though that doesn't matter), or
       a floating constant that begins with 0. */
    if ((ch = *(curr_char_loc+1)) == 'x' || ch == 'X') {
      /* 0x... or 0X..., hexadecimal. */
      kind = k_hex;
      /* The hex constant stops on a non-hex digit. */
      curr_char_loc++;
      do {} while (isxdigit(*(++curr_char_loc)));
      /* Check for just "0x" by itself.  pcc allows this and interprets
         it as zero. */
      if (curr_char_loc == start_of_curr_token+2 && !fetch_pp_tokens) {
        if (C_dialect != C_dialect_pcc) {
          error_at_line_pos(ec_bad_hex_digit, start_of_curr_token);
          err = TRUE;
        } else {
          warning_at_line_pos(ec_bad_hex_digit, start_of_curr_token);
        }  /* if */
      }  /* if */
    } else {
      /* Octal or floating point.  Accumulate digits.  Digits 8 and 9
         are valid in floating point, but in octal they are only valid in
         pcc mode.  That is checked later.  For now, the "8" and "9"
         are accumulated. */
      do {} while (isdigit(*(++curr_char_loc)));
      /* Check for floating point. */
      if ((ch = *curr_char_loc) == '.') goto float_accum_1;
      if (ch == 'e' || ch == 'E')       goto float_accum_2;
      /* Definitely octal. */
      kind = k_octal;
    }  /* if */
  } else if (*curr_char_loc == '.') {
    /* Number starting with ".".  The character following the "." must be
       a digit (already checked by get_token). */
    goto float_accum_1;
  } else {
    /* Number not starting with "0" or ".".  Could be a decimal integer or
       a floating-point number.  Accumulate the initial digit sequence. */
    do {} while (isdigit(*(++curr_char_loc)));
    /* A ".", "e", or "E" now indicates a floating-point constant. */
    if ((ch = *curr_char_loc) == '.') goto float_accum_1;
    if (ch == 'e' || ch == 'E')       goto float_accum_2;
    /* Definitely a decimal integer. */
    kind = k_decimal;
  }  /* if */
  /* Integer constant of some kind.  Check for final suffix of "u" or
     "l", or both, in upper or lower case. */
  if ((ch = *curr_char_loc) == 'u' || ch == 'U') {
    curr_char_loc++;
    if ((ch = *curr_char_loc) == 'l' || ch == 'L') curr_char_loc++;
  } else if (ch == 'l' || ch == 'L') {
    curr_char_loc++;
    if ((ch = *curr_char_loc) == 'u' || ch == 'U') curr_char_loc++;
  }  /* if */
  goto constant_accumulated;

float_accum_1:
  /* At the decimal point in a floating constant.  Take whatever decimal
     digits follow it. */
  do {} while (isdigit(*(++curr_char_loc)));
  if ((ch = *curr_char_loc) != 'e' && ch != 'E') goto end_float_accum;
float_accum_2:
  /* At the "e" or "E" indicating the start of the exponent of a floating
     constant.  Take an optional sign, then decimal digits of the exponent. */
  if ((ch = *(curr_char_loc+1)) == '+' || ch == '-') curr_char_loc++;
  if (!isdigit(*(curr_char_loc+1)) && !fetch_pp_tokens) {
    /* No digits of the exponent are present. pcc treats this as an exponent
       of zero. */
    if (C_dialect != C_dialect_pcc) {
      error_at_line_pos(ec_bad_float_constant, curr_char_loc+1);
      err = TRUE;
    } else {
      warning_at_line_pos(ec_bad_float_constant, curr_char_loc+1);
    }  /* if */
  }  /* if */
  do {} while (isdigit(*(++curr_char_loc)));
end_float_accum:
  kind = k_float;
  /* Check for a final suffix of "f" or "l", in upper or lower case. */
  if ((ch = *curr_char_loc) == 'f' || ch == 'F' || ch == 'l' || ch == 'L' ) {
     curr_char_loc++;
  }  /* if */

constant_accumulated:
  /* Here, start_of_curr_token marks the beginning, and curr_char_loc one
     past the end of the constant.  kind and ctoken are set correctly.
     The suffix (if any) has been accumulated. */
  end_of_curr_token = curr_char_loc - 1;

#if DEBUG
  if (debug_level >= 4) {
    char *ks;
    switch ((int)kind) {
      case k_decimal:   ks = "decimal"; break;
      case k_octal:     ks = "octal";   break;
      case k_hex:       ks = "hex";     break;
      case k_float:     ks = "float";   break;
    }  /* switch */
    fprintf(f_debug, "Numeric token = \"%.*s\", kind = %s\n",
                     (end_of_curr_token - start_of_curr_token + 1),
                     start_of_curr_token, ks);
  }  /* if */
#endif /* DEBUG */
  /* See if there are more characters that would have been part of
     this number had it been scanned as a pp-number.  The pp-number
     syntax is (3.1.8):

     pp-number:
            digit
            . digit
            pp-number digit
            pp-number nondigit
            pp-number e sign
            pp-number E sign
            pp-number .

     digit is any decimal digit (0-9).
     nondigit is an alphabetic or "_".
  */
  /* Do this only in strict ANSI mode, or in default mode when scanning
     preprocessing tokens, but never in pcc mode. */
  if ((strict_ansi_mode || fetch_pp_tokens) && C_dialect != C_dialect_pcc) {
    while (is_id_char[(ch = *curr_char_loc)-CHAR_MIN] || ch == '.' ||
           ((ch == '+' || ch == '-') &&
            ((ch = *(curr_char_loc-1)) == 'e' || ch == 'E'))) {
      /* 0-9, a-z, A-Z, "_", ".", or sign preceded by "e" or "E".
         Keep accumulating. */
      curr_char_loc++;
    }  /* while */
#if DEBUG
    if (debug_level >= 4) {
      if (curr_char_loc != (end_of_curr_token + 1)) {
        fprintf(f_debug, "Extra pp-number text: \"%.*s\"\n",
                         (curr_char_loc - end_of_curr_token - 1),
                         end_of_curr_token+1);
      }  /* if */
    }  /* if */
#endif /* DEBUG */
  }  /* if */

  if (fetch_pp_tokens) {
    /* Raw pp-tokens are wanted, do not convert.  Take the whole pp-number. */
    end_of_curr_token = curr_char_loc - 1;
    ctoken = tok_pp_number;
  } else {
    /* Preprocessing number is not wanted. */
    /* Check for extra pp-number characters of token to be converted.
       This is an error. */
    if (curr_char_loc != (end_of_curr_token + 1) && !err) {
      error_at_line_pos(ec_extra_chars_on_number, end_of_curr_token+1);
      /* Note that the extra characters just get thrown away. */
    }  /* if */
    /* Convert the constant.  Errors are still possible, since the checking
       above allows certain cases by. */
    switch ((int)kind) {
      case k_decimal:
        conv_integer_literal(10, &err_code, &err_pos);
        ctoken = tok_int_constant;
        break;
      case k_octal:
        conv_integer_literal(8, &err_code, &err_pos);
        ctoken = tok_int_constant;
        break;
      case k_hex:
        conv_integer_literal(16, &err_code, &err_pos);
        ctoken = tok_int_constant;
        break;
      case k_float:
        conv_float_literal(&err_code, &err_pos);
        ctoken = tok_float_constant;
        break;
    }  /* switch */
    /* Check for errors detected. */
    if (err_code != ec_no_error) {
      error_at_line_pos(err_code, err_pos);
    }  /* if */
  }  /* if */
  return (ctoken);
}  /* scan_number */


static void accum_quoted_string(a_boolean may_have_zero_characters,
                                a_boolean is_header_name,
                                long      *num_chars,
                                a_boolean *err)
/*
Scan a quoted construct.  The initial quote is at curr_char_loc.  Scan to
the matching closing quote, and do not be confused by escaped characters.
Return in *num_chars the actual number of characters (after escape
processing) contained within the quotes.  may_have_zero_characters
is TRUE if the entity is allowed to have zero characters between the
quotes.  is_header_name is TRUE if the entity is a header name (file name
specification for a #include).
This routine is used for character constants and string literals, in
both the "wide" and normal forms, and for header names in #include
directives.  *err is returned TRUE if there was an error.
*/
{
  register char quoting_char, ch;

  *err = FALSE;
  *num_chars = 0;
  quoting_char = *curr_char_loc;
  /* For <...> header names, the closing quoting character is different
     than the opening one. */
  if (quoting_char == '<') quoting_char = '>';
  while ((ch = *(++curr_char_loc)) != quoting_char) {
    (*num_chars)++;
    if (ch == '\\') {
      /* Backslash, escapes the next character.  If followed by "0" or
         "x", an octal or hexadecimal value must be scanned.  We recognize
         those digits so we can accurately count characters, but we do
         not convert them at this point. */
      if (isdigit(ch = *(++curr_char_loc)) && ch != '8' && ch != '9') {
        /* Octal escape, one to three digits.  Note that neither ANSI nor
           pcc allows 8 and 9 as octal digits in this case.  Note that
           there is code in conv_single_char that must match this code.*/
        if (isdigit(ch = *(curr_char_loc+1)) && ch != '8' && ch != '9') {
          curr_char_loc++;
          if (isdigit(ch = *(curr_char_loc+1)) && ch != '8' && ch != '9') {
            curr_char_loc++;
          }  /* if */
        }  /* if */
      } else if (ch == 'x') {
        /* Hex escape, any number of digits. */
        while (isxdigit(*(curr_char_loc+1))) curr_char_loc++;
      }  /* if */
    } else if (ch == '\n' ||
               (ch == END_OF_TOKEN_MARKER && !is_header_name) ||
               ch == '\0') {
      /* Newline -- error, quoted string unclosed. */
      /* Similar error for other strange cases of incomplete strings, which
         can come up with preprocessing.  Note that end-of-token markers do
         not terminate header names, because header names can result from
         several adjacent preprocessing tokens when macro expansion is
         involved (see 3.8.2).  The end-of-token markers are removed when the
         file name is constructed later (see proc_include). */
      if (!fetch_pp_tokens) {
        /* Message is generic -- "Missing closing quote". */
        error_at_line_pos(ec_unclosed_string, start_of_curr_token);
      }  /* if */
      *err = TRUE;
      goto return_point;
    }  /* if */
  }  /* while */
  /* Skip the closing quote. */
  curr_char_loc++;
  if (*num_chars == 0 && !may_have_zero_characters) {
    /* Error -- The string may not have zero characters. */
    if (!fetch_pp_tokens) {
      error_at_line_pos(ec_zero_length_string, start_of_curr_token);
    }  /* if */
    *err = TRUE;
  }  /* if */
return_point:
  end_of_curr_token = curr_char_loc - 1;
}  /* accum_quoted_string */


static a_token_kind scan_char_constant(void)
/*
Scan a character constant token, return the token kind or tok_error.
*/
{
  a_token_kind  ctoken;
  a_boolean     err;
  long		num_chars;
  an_error_code err_code;
  char          *err_pos;

  accum_quoted_string(/*may_have_zero_characters=*/FALSE,
                      /*is_header_name=*/FALSE, &num_chars, &err);
  ctoken = tok_char_constant;
  if (fetch_pp_tokens) {
    /* Raw tokens wanted, constant is not converted. */
    if (err) ctoken = tok_error;
  } else {
    /* Convert the constant to internal form. */
    if (err) {
      set_error_constant(&const_for_curr_token);
    } else {
      conv_char_literal(num_chars, &err_code, &err_pos);
      /* Check for errors detected. */
      if (err_code != ec_no_error) {
        error_at_line_pos(err_code, err_pos);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return(ctoken);
}  /* scan_char_constant */


static a_token_kind scan_string_literal(void)
/*
Scan a string literal token, return the token kind or tok_error.
*/
{
  a_token_kind  ctoken;
  a_boolean     err;
  an_error_code err_code;
  char          *err_pos;
  long          num_chars;

  accum_quoted_string(/*may_have_zero_characters=*/TRUE,
                      /*is_header_name=*/FALSE,
                      &num_chars, &err);
  ctoken = tok_string_literal;
  if (fetch_pp_tokens) {
    /* Raw tokens wanted, constant is not converted. */
    if (err) ctoken = tok_error;
  } else {
    /* Convert the constant to internal form. */
    if (err) {
      set_error_constant(&const_for_curr_token);
    } else {
      conv_string_literal(num_chars, &err_code, &err_pos);
      /* Check for errors detected. */
      if (err_code != ec_no_error) {
        error_at_line_pos(err_code, err_pos);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return(ctoken);
}  /* scan_string_literal */


static a_token_kind scan_wide_char_constant(void)
/*
Scan a wide character constant token, return the token kind or tok_error.
*/
{
  /* Skip over the "L". */
  curr_char_loc++;
  /* Otherwise, handle the same as the non-wide case. */
  return(scan_char_constant());
}  /* scan_wide_char_constant */


static a_token_kind scan_wide_string_literal(void)
/*
Scan a wide string literal token, return the token kind or tok_error.
*/
{
  /* Skip over the "L". */
  curr_char_loc++;
  /* Otherwise, handle the same as the non-wide case. */
  return(scan_string_literal());
}  /* scan_wide_string_literal */


static void adjust_pp_int_constant(void)
/*
The current token is an integer constant scanned within a preprocessing
#if expression.  It should be made long if it is not already so.
See standard, 3.8.1.
*/
{
  an_integer_kind ik;

  ik = const_for_curr_token.type->variant.integer.int_kind;
  if (!(ik == (an_integer_kind)ik_long ||
        ik == (an_integer_kind)ik_unsigned_long)) {
    /* The type is not long, so change it.  It's changed to unsigned long
       if the current type is unsigned, otherwise to long. */
    if (ik == (an_integer_kind)ik_unsigned_char  ||
        (ik == (an_integer_kind)ik_char && !targ_has_signed_chars) ||
        ik == (an_integer_kind)ik_unsigned_short ||
        ik == (an_integer_kind)ik_unsigned_int)  {
      ik = (an_integer_kind)ik_unsigned_long;
    } else {
      ik = (an_integer_kind)ik_long;
    }  /* if */
    const_for_curr_token.type = integer_type(ik);
  }  /* if */
}  /* adjust_pp_int_constant */


a_token_kind scan_literal_constant(a_token_kind kind)
/*
Scan a literal constant token, according to the value of kind:
Integer or floating point for tok_pp_number, character for 
tok_char_constant, and string literal for tok_string_literal.
Return the type of token scanned, or tok_error if there was an error.
This is used in scanning the expansion of a macro that expands simply
to a constant, in order to be able to save its value.
Note that adjacent string literals are not supposed to be concatenated,
and integer constants are not adjusted in length even if we are
within a preprocessing #if expression, since we want the generic
reusable value of the constant.
*/
{
  a_token_kind ctoken;

  switch (kind) {
    case tok_pp_number:
      ctoken = scan_number();
      break;
    case tok_char_constant:
      if (*curr_char_loc == 'L') {
        ctoken = scan_wide_char_constant();
      } else {
        ctoken = scan_char_constant();
      }  /* if */
      break;
    case tok_string_literal:
      if (*curr_char_loc == 'L') {
        ctoken = scan_wide_string_literal();
      } else {
        ctoken = scan_string_literal();
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error("scan_literal_constant: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return(ctoken);
}  /* scan_literal_constant */


/*
Macro called to check for compound assignment operators written as
two tokens in pcc mode (e.g., "+=" written as "+" and "=").  "tok"
is the compound assignment token that should be placed in ctoken if the
"=" is present; ctoken is left unchanged otherwise.  Note that this macro
does a "goto" in the pcc case, and continues in sequence in the non-pcc
case; its position is therefore highly important.
*/
#define check_for_pcc_compound_assignment_operator(tok)               \
{ if (C_dialect == C_dialect_pcc) {                                   \
    compound_token = tok;                                             \
    goto check_for_compound_assignment_operator;                      \
  }  /* if */                                                         \
}  /* check_for_pcc_compound_assignment_operator */


/*
Remember that a token has been gotten from the current source line.
Remember the start position (sequence number, column) of the token,
and also put that into error_position.
*/
#define remember_token_start()                                        \
{ any_tokens_gotten_from_curr_source_line = TRUE;                     \
  macro_line_loc_to_source_pos(start_of_curr_token, pos_curr_token);  \
  error_position.seq    = pos_curr_token.seq;                         \
  error_position.column = pos_curr_token.column;                      \
}  /* remember_token_start */


a_token_kind get_token(void)
/*
Scan the next token of input, and return its kind.  The kind of token is
also saved in curr_token.

If fetch_pp_tokens is TRUE when this routine is called, a preprocessing
token (pp-token) is fetched instead of a regular token.  Constants are
not converted (and numeric ones are returned as pp-numbers); adjacent
string literals are not concatenated; keywords are not recognized; and
errors are not issued for malformed tokens.  Also, start_of_curr_token
and end_of_curr_token will be set to point to the beginning and end of
the current token, and len_of_curr_token will be set to its length.

If expand_macros is TRUE, preprocessing macros are expanded as they are
scanned.  The caller sees only the tokens after expansion.

If in_preprocessing_directive is TRUE, the definition of white space is
changed to that for within preprocessing directives, keywords are not
recognized, and newline, "#", and "##" are recognized and returned as tokens.

If in_pp_if_expression is TRUE (indicating that we are inside a
preprocessing #if expression), integer constants will get an implicit
"L" suffix, and undefined identifiers will be returned as the integer
constant 0L.

If exp_header_name is TRUE, then <...> and "..." will be scanned
as a header name for a #include (tok_header_name).  Other tokens will
be processed normally.

If exp_digit_sequence is TRUE, then a string of decimal digits will be
scanned as a tok_digit_sequence (used in the #line directive).  Other tokens
will be processed normally.

If trapped_token is TRUE, the next token was previously fetched (see
next_token), and the information for it is retrieved from
trapped_token_state, rather than fetching another token.

This routine is called an enormous number of times, and therefore has
been written to be as fast as possible.  Structure has been sacrificed
to speed in some cases.
*/
#if ASM_FUNCTION_ALLOWED
/*
If in_asm_function_body is TRUE, return tok_newline for ends of lines.
*/
#endif /* ASM_FUNCTION_ALLOWED */
{
  register a_token_kind ctoken;
  register char         ch;
  register a_symbol_ptr	assoc_symbol;
  a_boolean             err;
  long			num_chars;
  a_constant		con_copy;
  a_source_position     save_pos_curr_token;
  a_symbol_kind		id_kind;
  a_boolean		rescan;
  a_token_kind          compound_token;

  /* If there is a trapped token, reuse it (see next_token). */
  if (trapped_token) {
    trapped_token = FALSE;
    restore_curr_token_state(&trapped_token_state);
    ctoken = curr_token;
    goto return_from_token_scan;
  }  /* if */
rescan_token:
  /* Skip over any initial white space blanks and horizontal tabs.
     These are very common, so they're handled inline here.  The
     other potential white space characters (newline, vertical tab,
     form feed, and "/" and "*" indicating the start of a comment) are
     processed out of the main "switch" statement. */
  if ((ch = *curr_char_loc) == ' ' || ch == '\t') {
    do {} while ((ch = *(++curr_char_loc)) == ' ' || ch == '\t');
  }  /* if */
start_of_token_scan:  /* Restart here after scanning white space. */
  /* Remember the start character position of the token. */
  start_of_curr_token = curr_char_loc;
  /* Branch to different processing code according to the first
     character of the token.  *curr_char_loc must be used instead of
     ch because ch is not set when arriving at start_of_token_scan
     via goto from elsewhere. */
  switch (*curr_char_loc) {
    case '\0':
      /* Null.  Usually, this indicates the end of a line (probably
         after some error at end of a file).  However, it can also
         mean the end of source, or the end of the text of a macro
         expansion.  Let the white-space routine figure it out. */
      skip_white_space();
      /* If we are not at end of file, go scan the next token. */
      if (*curr_char_loc != '\0') goto start_of_token_scan;
      /* This is the ultimate end of file, or the end of a macro argument
         string being scanned in isolation from the rest of the source.
         Return end of file. */
      ctoken = tok_end_of_source;
      /* Go exit with zero-length token. */
      start_of_curr_token = curr_char_loc;
      goto save_end_position;
    case '\n':
      /* Newline.  Is white space ordinarily, but a token within
         preprocessing directives. */
      if (in_preprocessing_directive
#if ASM_FUNCTION_ALLOWED
          /* ... or if inside an asm function body. */
          || in_asm_function_body
#endif /* ASM_FUNCTION_ALLOWED */
                                    ) {
        ctoken = tok_newline;
      } else {
        skip_white_space();
        goto start_of_token_scan;
      }  /* if */
      break;
    case '\f':
    case VERTICAL_TAB_CHARACTER:
      /* Form feed, vertical tab.  Usually white space, but implementation-
         defined when inside a preprocessing directive.  Let the white space
         routine decide. */
      skip_white_space();
      goto start_of_token_scan;
    case END_OF_TOKEN_MARKER:
      /* Marker put into text by preprocessing of macros, to force the same
         interpretation of token boundaries as during the macro definition.
         At this level, should be ignored. */
      curr_char_loc++;
      goto rescan_token;
    case ATTENTION_MARKER:
      /* Marker placed into source text to provide a cue to the fact that
         a source modification (probably a text replacement due to a
         macro expansion) begins here.  Let the white-space routine
         handle it. */
      skip_white_space();
      goto start_of_token_scan;
    case '[':
      ctoken = tok_lbracket;
      break;
    case ']':
      ctoken = tok_rbracket;
      break;
    case '(':
      ctoken = tok_lparen;
      break;
    case ')':
      ctoken = tok_rparen;
      break;
    case '{':
      ctoken = tok_lbrace;
      break;
    case '}':
      ctoken = tok_rbrace;
      break;
    case ',':
      ctoken = tok_comma;
      break;
    case '~':
      ctoken = tok_compl;
      break;
    case ':':
      /* In C++, "::" is a possibility. */
      if (C_dialect == C_dialect_cplusplus &&
          *(curr_char_loc+1) == ':') {
        ctoken = tok_colon_colon;
        goto two_char_token;
      }  /* if */
      ctoken = tok_colon;
      break;
    case ';':
      ctoken = tok_semicolon;
      break;
    case '?':
      ctoken = tok_quest_mark;
      break;
    case '-':
      /* One of "--", "->", "-=", or "-". */
      /* In C++, "->*" is also a possibility. */
      if ((ch = *(curr_char_loc+1)) == '-') {
        ctoken = tok_minus_minus;
        goto two_char_token;
      } else if (ch == '>') {
        if (C_dialect == C_dialect_cplusplus &&
            *(curr_char_loc+2) == '*') {
          ctoken = tok_arrow_star;
          goto three_char_token;
        }  /* if */
        ctoken = tok_arrow;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_minus_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "-". */
      ctoken = tok_minus;
      check_for_pcc_compound_assignment_operator(tok_minus_assign);
      break;
    case '+':
      /* One of "++", "+=", or "+". */
      if ((ch = *(curr_char_loc+1)) == '+') {
        ctoken = tok_plus_plus;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_plus_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "+". */
      ctoken = tok_plus;
      check_for_pcc_compound_assignment_operator(tok_plus_assign);
      break;
    case '*':
      /* One of "*=" or "*". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_times_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "*". */
      ctoken = tok_star;
      check_for_pcc_compound_assignment_operator(tok_times_assign);
      break;
    case '/':
      /* One of "/ *" (start of comment), "/=", or "/". */
      /*          ^--- space here to avoid nested-comment problem. */
      if (start_of_comment()) {
        /* Go skip the comment. */
        skip_white_space();
        goto start_of_token_scan;
      } else if (*(curr_char_loc+1) == '=') {
        ctoken = tok_divide_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "/". */
      ctoken = tok_divide;
      check_for_pcc_compound_assignment_operator(tok_divide_assign);
      break;
    case '&':
      /* One of "&&", "&=", or "&". */
      if ((ch = *(curr_char_loc+1)) == '&') {
        ctoken = tok_and_and;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_and_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "&". */
      ctoken = tok_ampersand;
      check_for_pcc_compound_assignment_operator(tok_and_assign);
      break;
    case '%':
      /* One of "%=" or "%". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_remainder_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "%". */
      ctoken = tok_remainder;
      check_for_pcc_compound_assignment_operator(tok_remainder_assign);
      break;
    case '<':
      /* One of "<<", "<<=", "<=", or "<".  If exp_header_name is
         TRUE, a header name of the form <filename>. */
      if (exp_header_name) {
        accum_quoted_string(/*may_have_zero_characters=*/FALSE,
                            /*is_header_name=*/TRUE, &num_chars, &err);
        if (err) {
          ctoken = tok_error;
        } else {
          ctoken = tok_header_name;
        }  /* if */
        goto end_of_token_scan;
      } else if ((ch = *(curr_char_loc+1)) == '<') {
        if (*(curr_char_loc+2) == '=') {
          ctoken = tok_shift_left_assign;
          goto three_char_token;
        } else {
           ctoken = tok_shift_left;
           check_for_pcc_compound_assignment_operator(tok_shift_left_assign);
           goto two_char_token;
        }  /* if */
      } else if (ch == '=') {
        ctoken = tok_le;
        goto two_char_token;
      }  /* if */
      /* Just plain "<". */
      ctoken = tok_lt;
      break;
    case '>':
      /* One of ">>", ">>=", ">=", or ">". */
      if ((ch = *(curr_char_loc+1)) == '>') {
        if (*(curr_char_loc+2) == '=') {
          ctoken = tok_shift_right_assign;
          goto three_char_token;
        } else {
          ctoken = tok_shift_right;
          check_for_pcc_compound_assignment_operator(tok_shift_right_assign);
          goto two_char_token;
        }  /* if */
      } else if (ch == '=') {
        ctoken = tok_ge;
        goto two_char_token;
      }  /* if */
      /* Just plain ">". */
      ctoken = tok_gt;
      break;
    case '=':
      /* One of "==" or "=". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_eq;
        goto two_char_token;
      }  /* if */
      /* Just plain "=". */
      ctoken = tok_assign;
      if (C_dialect == C_dialect_pcc) {
        /* For pcc mode, the old-fashioned assignment operators of K&R
           (first edition) Appendix A section 17 (Anachronisms) should
           be recognized (with a warning).  An example is "=-" instead
           of "-=".  Note that no space is allowed between the "=" and
           the other character. */
        if ((ch = *(curr_char_loc+1)) == '+') {
          /* "=+" becomes "+=". */
          ctoken = tok_plus_assign;
        } else if (ch == '-') {
          /* "=-" becomes "-=". */
          ctoken = tok_minus_assign;
        } else if (ch == '*') {
          /* "=*" becomes "*=". */
          ctoken = tok_times_assign;
        } else if (ch == '/' && *(curr_char_loc+2) != '*') {
          /* "=/" becomes "/=".  Watch out for "=/" followed by "*"; that's
             a "=" followed by the start of a comment. */
          ctoken = tok_divide_assign;
        } else if (ch == '%') {
          /* "=%" becomes "%=". */
          ctoken = tok_remainder_assign;
        } else if (ch == '&') {
          /* "=&" becomes "&=". */
          ctoken = tok_and_assign;
        } else if (ch == '^') {
          /* "=^" becomes "^=". */
          ctoken = tok_excl_or_assign;
        } else if (ch == '|') {
          /* "=|" becomes "|=". */
          ctoken = tok_or_assign;
        } else if (ch == '>' && *(curr_char_loc+2) == '>') {
          /* "=>>" becomes ">>=". */
          ctoken = tok_shift_right_assign;
        } else if (ch == '<' && *(curr_char_loc+2) == '<') {
          /* "=<<" becomes "<<=". */
          ctoken = tok_shift_left_assign;
        } else {
          /* Not an old-fashioned compound assignment operator. */
        }  /* if */
        if (ctoken != tok_assign) {
          if (!fetch_pp_tokens) warning(ec_old_fashioned_assignment_operator);
          if (ctoken == tok_shift_right_assign ||
              ctoken == tok_shift_left_assign) {
            goto three_char_token;
          } else {
            goto two_char_token;
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case '!':
      /* One of "!=" or "!". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_ne;
        goto two_char_token;
      }  /* if */
      /* Just plain "!". */
      ctoken = tok_not;
      break;
    case '^':
      /* One of "^=" or "^". */
      if (*(curr_char_loc+1) == '=') {
        ctoken = tok_excl_or_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "^". */
      ctoken = tok_excl_or;
      check_for_pcc_compound_assignment_operator(tok_excl_or_assign);
      break;
    case '|':
      /* One of "||", "|=", or "|". */
      if ((ch = *(curr_char_loc+1)) == '|') {
        ctoken = tok_or_or;
        goto two_char_token;
      } else if (ch == '=') {
        ctoken = tok_or_assign;
        goto two_char_token;
      }  /* if */
      /* Just plain "|". */
      ctoken = tok_or;
      check_for_pcc_compound_assignment_operator(tok_or_assign);
      break;
    case '.':
      /* One of ".", a float constant, or "...". */
      /* In C++, ".*" is also a possibility. */
      if (isdigit(ch = *(curr_char_loc+1))) {
        ctoken = scan_number();
        goto end_of_token_scan;
      } else if (ch == '.' && *(curr_char_loc+2) == '.') {
        ctoken = tok_ellipsis;
        goto three_char_token;
      } else if (ch == '*' && C_dialect == C_dialect_cplusplus) {
        ctoken = tok_period_star;
        goto two_char_token;
      }  /* if */
      /* Just plain ".". */
      ctoken = tok_period;
      break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      /* Integer or float constant. If exp_digit_sequence is TRUE, scan
          digit_sequence instead (used in #line directive). */
      if (!exp_digit_sequence) {
        ctoken = scan_number();
        /* Adjust the length of an integer constant in a preprocessing #if
           expression. */
        if (in_pp_if_expression &&
            ctoken == tok_int_constant) {
          adjust_pp_int_constant();
        }  /* if */
      } else {
        do {} while (isdigit(*(++curr_char_loc)));
        end_of_curr_token = curr_char_loc - 1;
        ctoken = tok_digit_sequence;
      }  /* if */
      goto end_of_token_scan;
    case 'L':
      /* Probably an identifier, but check for a wide character
         constant (L'x') or wide string literal (L"xyz") first. */
      if ((ch = *(curr_char_loc+1)) == '\'') {
        ctoken = scan_wide_char_constant();
        goto end_of_token_scan;
      } else if (ch == '"') {
        remember_token_start();
        ctoken = scan_wide_string_literal();
        goto concatenate_adjacent_string_literals;
      }  /* if */
      /* Neither of those cases, fall through into identifier processing. */
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
    case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
    case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
    case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
    case 'H': case 'I': case 'J': case 'K': /*above*/ case 'M': case 'N':
    case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
    case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_':
      /* Identifier (including keywords, macros, etc.). */
      /* See check_for_following_parenthesis in macro.c for code that
         also checks for the first character of an identifier. */
      /* Find end of identifier.  Identifiers can contain alphabetic
         characters, underscores, and digits after the first character. */
      remember_token_start();
      ctoken = tok_identifier;
      do {} while (is_id_char[*(++curr_char_loc)-CHAR_MIN]);
      end_of_curr_token = curr_char_loc - 1;
      /* Save the position of the identifier in the locator.  This is done 
         even if the identifier is not looked up in the symbol table. */
      copy_source_position(pos_curr_token,
                           locator_for_curr_id.source_position);
      locator_for_curr_id.qualified_name_symbol = NULL;
      if ((fetch_pp_tokens || in_preprocessing_directive) && !expand_macros) {
        /* Raw preprocessing tokens wanted, so do not look up the
           identifier. */
        symbol_list_for_curr_id = NULL;
      } else {
        /* Look up the identifier in the symbol table. */
        assoc_symbol = symbol_list_for_curr_id = 
                  find_symbol(start_of_curr_token,
                              (sizeof_t)((end_of_curr_token -
                                                     start_of_curr_token + 1)),
                              &locator_for_curr_id);
        /* See if the identifier is a macro or keyword.  "Macro" should
           take precedence over "keyword", but it will naturally, since
           keywords are entered first and therefore appear at the end of the
           list. */
        while (assoc_symbol != NULL) {
          if ((id_kind = assoc_symbol->kind) == (a_symbol_kind)sk_macro) {
            /* Macro to be expanded. */
            if (expand_macros) {
              ctoken = macro_invocation(assoc_symbol, &rescan);
              /* In the usual case, we rescan the expanded form of the
                 macro. */
              if (rescan) goto rescan_token;
              /* Otherwise, expand_token has returned a token we can use
                 immediately.  If it is an identifier, fall into the
                 remaining processing for non-macro identifiers.
                 If it is a string literal, go look for
                 adjacent string literals with which it might be
                 concatenated.  If it is an integer constant in a
                 preprocessing #if expression, adjust its length.
                 Otherwise, just return what we have been given. */
              if (ctoken == tok_identifier) {
                /* Fall into remaining processing for non-macro identifiers. */
              } else if (ctoken == tok_string_literal) {
                goto concatenate_adjacent_string_literals;
              } else if (in_pp_if_expression &&
                         ctoken == tok_int_constant) {
                adjust_pp_int_constant();
                goto end_id_scan;
              } else {
                /* Anything else. */
                goto end_id_scan;
              }  /* if */
            }  /* if */
          } else if (id_kind == (a_symbol_kind)sk_keyword) {
            /* Keyword, return the proper token for it.  When fetching raw
               preprocessing tokens, or inside a preprocessing directive,
               the keywords mean nothing. */
            if (!fetch_pp_tokens && !in_preprocessing_directive) {
              ctoken = assoc_symbol->variant.keyword_token;
              goto end_id_scan;
            }  /* if */
          }  /* if */
          assoc_symbol = assoc_symbol->next;
        }  /* while */
        /* The identifier is not a macro and not a keyword.  If we are
           in a preprocessing #if expression, replace the identifier with
           the value 0L. */
        if (in_pp_if_expression) {
          remark(ec_undefined_preproc_id);
          ctoken = make_pp_int_constant(0L);
        }  /* if */
      }  /* if */
end_id_scan:
      /* Use "_b" exit because we may no longer be on the line on which
         the identifier appears if it is a macro name and the opening
         parenthesis of the parameter list is on a different line. */
      goto end_of_token_scan_b;
    case '\'':
      /* Character constant. */
      ctoken = scan_char_constant();
      goto end_of_token_scan;
    case '"':
      /* String literal. */
      if (exp_header_name) {
        /* If in a preprocessing directive, and exp_header_name is
           TRUE, the string should be scanned as a header name (file
           name on a #include). */
        accum_quoted_string(/*may_have_zero_characters=*/FALSE,
                            /*is_header_name=*/TRUE, &num_chars, &err);
        if (err) {
          ctoken = tok_error;
        } else {
          ctoken = tok_header_name;
        }  /* if */
        goto end_of_token_scan;
      } else {
        /* Scan as a string literal, not a header name.  We could still
	   be in a preprocessing directive, though. */
        remember_token_start();
        ctoken = scan_string_literal();
        goto concatenate_adjacent_string_literals;
      }  /* if */
      /* No break needed, both branches end with a goto. */
    case '#':
      /* As the first token on a line, "#" opens a preprocessing directive.
         "#" and "##" are also allowed within the body of a #define
         (for stringizing and pasting).  */
      if (C_dialect != C_dialect_pcc && in_preprocessing_directive) {
        /* We recognize and return these preprocessing tokens even if
           we do not know that we are in the body of a #define; this
           helps produce reasonable error messages. */
        if (*(curr_char_loc+1) == '#') {
          ctoken = tok_paste;
          goto two_char_token;
        } else {
          ctoken = tok_sharp;
        }  /* if */
      } else if (!any_tokens_gotten_from_curr_source_line) {
        /* A sharp that is the first thing on a line -- This is a
           preprocessing directive. */
        if (!currently_in_pp_if_skip) {
          remember_token_start();  /* For the "#" pseudo-token. */
          curr_char_loc++;  /* Skip over the "#". */
          pp_directive();
          /* After the directive has been processed, go skip white space and
             scan another token. */
          skip_white_space();
          goto start_of_token_scan;
        }  /* if */
        /* Skipping because of an #if or the like, just return this
           as a token for further checking. */
        ctoken = tok_sharp;
      } else {
        /* "#" outside of a preprocessing directive, and not at the start of a
           line; don't know what it means. */
        if (!fetch_pp_tokens) {
          error_at_line_pos(ec_bad_use_of_sharp, start_of_curr_token);
        }  /* if */
        ctoken = tok_error;
      }  /* if */
      break;
    default:
      /* Something else, an error. */
      if (!fetch_pp_tokens) {
        error_at_line_pos(ec_bad_token, start_of_curr_token);
      }  /* if */
      ctoken = tok_error;
  }  /* switch */

one_char_token:
  /* Normal assumption on break from switch is that the current character
     is part of the token, and therefore the current position needs to be
     incremented. */
  curr_char_loc++;

save_end_position:
  /* Remember character position of end of token.  It's one before the
     current position.  In cases where that is not right (as when it is
     necessary to scan white space following the token to check something),
     set end_of_curr_token explicitly and goto end_of_token_scan. */
  end_of_curr_token = curr_char_loc - 1;

end_of_token_scan:
  remember_token_start();
end_of_token_scan_b:
  /* The "_b" entry here is for cases where something beyond the end
     of the token has been scanned, and therefore we may be on a new
     line now.  Cases where this is true (perhaps because skip_white_space
     has been called) should call remember_token_start before scanning
     the initial token, and then should branch here after all other
     processing is done. */
  if (start_of_curr_token != NULL) {
    len_of_curr_token = end_of_curr_token - start_of_curr_token + 1;
  }  /* if */
return_from_token_scan:
#if DEBUG
  if (debug_level >= 3) {
    /* Write out the current token. */
    fprintf(f_debug, "get_token: pos = %lu/%2d, %-10s",
                     pos_curr_token.seq, pos_curr_token.column,
                     db_token_names[(int)ctoken]);
    if (start_of_curr_token != NULL) {
      /* Print token string if valid. */
      fprintf(f_debug, ", \"%.*s\"", len_of_curr_token,
                                     start_of_curr_token);
    }  /* if */
    /* Dump constants only if they have been converted. */
    if (!fetch_pp_tokens &&
        (ctoken == tok_float_constant || ctoken == tok_int_constant ||
         ctoken == tok_char_constant  || ctoken == tok_string_literal)) {
      /* Dump value for constant. */
      fprintf(f_debug, ", ");
      db_constant(&const_for_curr_token);
    }  /* if */
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  return (curr_token = ctoken);

two_char_token:
  /* For two-character tokens, increment the source position appropriately
     and then rejoin the main code. */
  curr_char_loc += 2;
  goto save_end_position;

three_char_token:
  /* For three-character tokens, increment the source position appropriately
     and then rejoin the main code. */
  curr_char_loc += 3;
  goto save_end_position;

concatenate_adjacent_string_literals:
  /* Come here after scanning a string literal or wide string literal.
     If appropriate, string literals following the current one will be
     concatenated with it.  See 2.1.1.2, translation phase 6. */
  /* The standard says that a wide string literal next to a normal
     string literal is undefined; we choose to concatenate them. */
  if (!(fetch_pp_tokens || in_preprocessing_directive)) {
    /* Scan forward to the next token, to see if it is another string literal.
       One catch: the second string literal may come from a macro expansion,
       so if the next thing looks like the start of an identifier, scan
       it and expand it. */
    /* remember_token_start should already have been called before the
       token was accumulated. */
    copy_source_position(pos_curr_token, save_pos_curr_token);
    skip_white_space();
    while (*curr_char_loc == '"' ||
           (*curr_char_loc == 'L' && *(curr_char_loc+1) == '"') ||
           (is_id_char[*curr_char_loc-CHAR_MIN] && !isdigit(*curr_char_loc) &&
            (*curr_char_loc != 'L' || *(curr_char_loc+1) != '\''))) {
      /* The next thing is a string literal, a wide string literal,
          or an identifier (the last test rules out wide character constants,
          like L'a'). */
      /* Scan the next token.  Scan it as a pp token to avoid string literal
         concatenation and errors during tokenization.  Note that we know that
         fetch_pp_tokens is already FALSE, because of the test above. */
      fetch_pp_tokens = TRUE;
      (void)get_token();
      fetch_pp_tokens = FALSE;
      curr_char_loc = start_of_curr_token;
      if (curr_token != tok_string_literal) {
        /* The next token is not a string literal, so do not take it; leave
           it for next time. */
        start_of_curr_token = NULL;
        break;
      }  /* if */
      /* The next token is a string literal.  Save the old constant, and
         convert the new. */
      copy_constant(&const_for_curr_token, &con_copy);
      /* Note that the constant must be re-scanned, not just converted,
         because we need to have the count of characters.  In particular,
         there is a problem if the macro that generated this string has
         previously been expanded as a manifest constant; in that case,
         the string has not been scanned at this time. */
      if (*start_of_curr_token == 'L') {
        ctoken = scan_wide_string_literal();
      } else {
        ctoken = scan_string_literal();
      }  /* if */
      if (ctoken == tok_string_literal) {
        /* No error; concatenate the two. */
        concat_string_literals(&con_copy, &const_for_curr_token);
      }  /* if */
      skip_white_space();
    }  /* while */
    /* Use the source position of the initial piece of the string as the
       position for the overall compound token. */
    copy_source_position(save_pos_curr_token, pos_curr_token);
  }  /* if */
  goto end_of_token_scan_b;

check_for_compound_assignment_operator:
  /* In pcc mode, compound assignment operators can be written as two
     tokens (e.g., "+=" can be written as "+ =").  If we come here, we
     are in pcc mode and a token has been scanned that could be the
     first part of such a compound token.  compound_token has been set
     to the token to be used for the compound token if "=" is next. */
  /* If fetching preprocessing tokens, the two parts of a compound
     token are passed through separately (i.e., one keeps the original
     tokens).  Note that "<<" and ">>" are two-character tokens. */
  if (fetch_pp_tokens) {
    if (ctoken == tok_shift_left || ctoken == tok_shift_right) {
      goto two_char_token;
    } else {
      goto one_char_token;
    }  /* if */
  }  /* if */
  remember_token_start();
  /* Advance past the first operator (two characters for "<<" and ">>"). */
  curr_char_loc++;
  if (ctoken == tok_shift_left || ctoken == tok_shift_right) curr_char_loc++;
  (void)skip_white_space();
  /* Give up if the next character is not "=". */
  /* The check for the "=" next is done on a character basis, where pcc 
     does it on a token basis.  This means that something like "a + == b"
     would be parsed here as "+=" then "=" where pcc would see "+" then
     "==".  No legal programs are affected, however. */
  if (*curr_char_loc != '=') goto end_of_token_scan_b;
  /* This IS the strange compound assignment operator. */
  start_of_curr_token = curr_char_loc;
  ctoken = compound_token;
  goto one_char_token;

}  /* get_token */


void flush_until_matching_token(void)
/*
The current token is the opening token of a pair of matched tokens (e.g.,
an opening parenthesis).  Flush to the corresponding closing token.
*/
{
  a_token_kind      closing_token;
  a_source_position start_pos;
  unsigned long     paren_count   = 0,
		    bracket_count = 0,
		    brace_count   = 0;
  unsigned long     max_lines;

  db_enter(3, "flush_until_matching_token");
  /* Save the current position, to see later how much we have flushed. */
  copy_source_position(pos_curr_token, start_pos);

  /* Determine the associated closing token and the maximum number of lines
     to throw away. */
  max_lines = 2;
  switch (curr_token) {
    case tok_lparen:    closing_token = tok_rparen;   break;
    case tok_lbracket:  closing_token = tok_rbracket; break;
    case tok_lbrace:    closing_token = tok_rbrace; max_lines = 20; break;
#if CHECKING
    default:
      internal_error("flush_until_matching_token: bad opening token");
#endif /* CHECKING */
  }  /* switch */
  (void)get_token();

  while (curr_token != closing_token ||
         paren_count != 0 || bracket_count != 0 || brace_count != 0) {
    /* Count paired tokens within the skip. */
    switch (curr_token) {
      case tok_lparen:                           paren_count++;   break;
      case tok_rparen:    if (paren_count > 0)   paren_count--;   break;
      case tok_lbracket:                         bracket_count++; break;
      case tok_rbracket:  if (bracket_count > 0) bracket_count--; break;
      case tok_lbrace:                           brace_count++;   break;
      case tok_rbrace:    if (brace_count > 0)   brace_count--;   break;
    }  /* switch */
    /* Always stop the flush on:
       1)  End of source;
       2)  A newline, if in a preprocessing directive. */
    if (curr_token == tok_end_of_source ||
        (in_preprocessing_directive && curr_token == tok_newline)) break;
    /* If we've skipped too many lines, give up the flush. */
    if ((pos_curr_token.seq - start_pos.seq) > max_lines) break;
    /* None of the conditions was satisfied, so keep flushing tokens. */
    (void)get_token();
  }  /* while */

  db_exit();
}  /* flush_until_matching_token */


void flush_tokens(void)
/*
Get and throw away tokens until a token is read that is in the set
of stop tokens.  This routine is called to recover from syntax
errors.
*/
{
  a_source_position start_pos;

  db_enter(3, "flush_tokens");
  /* Save the current position, to see later how much we have flushed. */
  copy_source_position(pos_curr_token, start_pos);

  /* Flush tokens until something in the stop tokens set turns up.
     Stop flushing if the end of file is reached.
     While flushing, note parentheses, etc., and flush to matching tokens. */
  /* Stop the flush on finding a token in the stop token set. */
  while (stop_token_array[(int)curr_token] == 0) {
    /* On paired tokens, skip to the corresponding closing token. */
    if (curr_token == tok_lparen || curr_token == tok_lbracket ||
        curr_token == tok_lbrace) flush_until_matching_token();
    /* Always stop the flush on:
       1)  End of source;
       2)  A newline, if in a preprocessing directive. */
    if (curr_token == tok_end_of_source ||
        (in_preprocessing_directive && curr_token == tok_newline)) break;
    /* None of the conditions was satisfied, so keep flushing tokens. */
    (void)get_token();
  }  /* while */
  set_err_pos_to_curr_token();
  /* If the flushing threw away more than just a little bit, put out
     a diagnostic to tell the user where the parsing recovered. */
  if (pos_curr_token.seq - start_pos.seq > 2) {
    warning(ec_end_of_flush);
  }  /* if */
  db_exit();
}  /* flush_tokens */


a_boolean required_token(a_token_kind  token,
			 an_error_code error_code)
/*
The current token is required to be "token".  If it is, advance normally
by calling get_token.  If not, issue the error message and call flush_tokens.
In either case, return TRUE if the required token showed up.
*/
{
  a_boolean token_present;

  db_enter(5, "required_token");

  if (curr_token == token) {
    (void)get_token();
    token_present = TRUE;
  } else {
    /* Token not present.  Report an error, flush to it or something
       else in the stop tokens set. */
    add_stop_token(token);
    set_err_pos_to_curr_token();
    syntax_error(error_code);
    remove_stop_token(token);
    /* If the token did show up, take it now. */
    token_present = (curr_token == token);
    if (token_present) (void)get_token();
  }  /* if */

  db_exit();

  return token_present;
}  /* required_token */


a_boolean loop_token(a_token_kind token)
/*
If the current token is the indicated token, then take that token (by
calling get_token), and return TRUE; otherwise, do not call get_token,
and return FALSE.  This function is used for a bottom-of-loop test for
repeated constructs, as in

   do {
     ... Scan the construct to be repeated. ...
   } while (loop_token(tok_comma));

*/
{
  a_boolean return_value;

  if (curr_token == token) {
    (void)get_token();
    return_value = TRUE;
  } else {
    return_value = FALSE;
    set_err_pos_to_curr_token();
  }  /* if */
  return(return_value);
}  /* loop_token */


a_token_kind next_token(void)
/*
Return the next token after the current one while leaving the current
token unchanged.  This is used in recursive-descent parsing routines
to peek ahead at the next token and decide on a path through the syntax.
This routine cannot be used when fetching raw preprocessing tokens.
It can be called more than once.  It works by saving the information for
the current token, fetching the next token, then restoring the information
for the current token.  On the next call of get_token, the trapped token
will be released instead of scanning a new token.  Note that the current
settings of the "exp_" flags (e.g., exp_header) and expand_macros (etc.)
will affect the scanning of the next token.  The input file position
and the preprocessing macros symbol table are not restored after the
call to get_token.
This scheme must be used, rather than just skipping forward and looking at
the start of the next token, because getting the proper next token might
involve macro expansion or scanning of a preprocessing directive.
*/
{
  a_token_state curr_token_state;

  db_enter(3, "next_token");
#if CHECKING
  if (fetch_pp_tokens) {
    internal_error("next_token: called with fetch_pp_tokens TRUE");
  }  /* if */
#endif /* CHECKING */
  /* If the next token has already been fetched, there's no need to do
     the get_token call. */
  if (!trapped_token) {
    /* Save the information about the current token. */
    save_curr_token_state(&curr_token_state);
    /* Fetch the next token. */
    (void)get_token();
    /* Save the information about this new token for use at the next
       "real" call of get_token. */
    trapped_token = TRUE;
    save_curr_token_state(&trapped_token_state);
    /* Restore the original token's information. */
    restore_curr_token_state(&curr_token_state);
  }  /* if */
  db_exit();
  return trapped_token_state.curr_token;
}  /* next_token */


a_boolean get_class_qualifier(a_scope_number *scope_number)
/*
Scan an optional class qualifier, e.g., "A::B::" (note that the final
identifier of a qualified name is not scanned here; see get_qualified_name).
Return TRUE if there was a qualifier, FALSE if not.  If there was a qualifier,
set *scope_number to the scope number for the class indicated by the
qualifier.  This routine should only be called in C++ mode.
*/
{
  a_boolean      is_qualifier = FALSE;
  a_symbol_ptr   class_symbol, first_symbol_in_class;
  a_scope_number class_scope;

  if (curr_token == tok_identifier) {
    /* Look up the symbol to see if it could be a class name.  Note that
       we don't consider the normal eclipsing rules.  A class can be found
       even when hidden by something else:
         class A {int i;};
         int f() {
           int A;
           A::i = 1;   // The class A is found.
         }
    */
    class_symbol = normal_id_lookup(&locator_for_curr_id,
                                    /*must_be_class=*/TRUE);
    if (class_symbol != NULL && next_token() == tok_colon_colon) {
      /* This is a qualified name. */
      /* Keep looping while there are more levels of class qualification.
         Stop on something that is not a class name followed by "::". */
      do {
        /* Skip over the "class-name ::". */
        (void)get_token();
        (void)get_token();
        /* Determine the scope number for the class. */
        first_symbol_in_class =
                              class_symbol->variant.class_struct_union.symbols;
        /* Clear class_symbol early to end the loop in the error case. */
        class_symbol = NULL;
        if (first_symbol_in_class == NULL) {
          /* There are no members of the class, so we cannot determine the
             scope number */
          class_scope = NO_SCOPE_NUMBER;
          break;
        }  /* if */
        class_scope = first_symbol_in_class->decl_scope;
        if (curr_token == tok_identifier) {
          /* The next thing is an identifier (it must be, but if it's not,
             the error is given later).  See if the identifier could be a
             class name, indicating further qualification, as in A::B::x. */
          /* Search for the identifier in the given scope. */
          class_symbol = scope_qualified_id_lookup(&locator_for_curr_id,
                                                   class_scope,
                                                   /*must_be_class=*/TRUE);
        }  /* if */
      } while (class_symbol != NULL && next_token() == tok_colon_colon);
      is_qualifier = TRUE;
      *scope_number = class_scope;
    }  /* if */
  }  /* if */
  return is_qualifier;
}  /* get_class_qualifier */


a_boolean get_qualified_name(void)
/*
If the current token is an identifier, see if it is the start of a
qualified name of the form

   A::x
   A::B::x
   etc.

If a qualified name is next, scan it and look up the qualified name.
Set qualified_name_symbol in locator_for_curr_id to point to the symbol
for the qualified identifier.  This is allowed only in C++ mode.
If a qualified name is not next, leave qualified_name_symbol
set to NULL.  Return FALSE if there is a qualifier but no final identifier,
e.g., "int A:: = 1;"; the error is already issued in that case.
*/
{
  a_boolean      is_qualified_name = FALSE, okay = TRUE;
  a_symbol_ptr   name_symbol;
  a_scope_number class_scope;

  if (C_dialect == C_dialect_cplusplus) {
    if (curr_token == tok_identifier) {
      /* See if there is a class qualifier (the "A::" part of "A::x"), and
         if so, get it and determine the scope number it represents. */
      if (get_class_qualifier(&class_scope)) {
        /* The current token must now be the final identifier of the qualified
           name, e.g., "x" in "A::B::x". */
        if (curr_token != tok_identifier) {
          /* syntax_error is deliberately not called. */
          error(ec_exp_identifier);
          okay = FALSE;
        } else {
          if (class_scope != NO_SCOPE_NUMBER) {
            /* There was a valid class qualifier.  Look up the identifier in
               the scope. */
            name_symbol = scope_qualified_id_lookup(&locator_for_curr_id,
                                                    class_scope,
                                                    /*must_be_class=*/FALSE);
            if (name_symbol != NULL) {
              /* The name was found. */
              is_qualified_name = TRUE;
              locator_for_curr_id.qualified_name_symbol = name_symbol;
              /* Clear the symbol list to be neat. */
              symbol_list_for_curr_id = NULL;
            }  /* if */
          }  /* if */
          if (!is_qualified_name) {
            /* The identifier could not be found in the class. */
            error(ec_name_not_found_in_class);
            set_to_error_locator(locator_for_curr_id);
          }  /* if */
        }  /* if */
      }  /* if */
#if DEBUG
      if (debug_level >= 4) {
        if (is_qualified_name) {
          fprintf(f_debug, "get_qualified_name: name = %s\n",
                           locator_for_curr_id.symbol_header->identifier);
        } else {
          fprintf(f_debug, "get_qualified_name: not qualified name\n");
        }  /* if */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  return okay;
}  /* get_qualified_name */


#if DEBUG
unsigned long show_lexical_space_used(void)
/*
Display and return the amount of space used for various lexical tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  fprintf(f_debug, "\nLexical table use:\n");
  fprintf(f_debug, "%25s %8s %8s %8s\n", "Table", "Number", "Each", "Total");

#define write_one(name, counter, type)                                \
{ num = counter; size = sizeof(type); total = num*size;               \
  fprintf(f_debug, "%25s %8lu %8lu %8lu\n", name, num, size, total);  \
  grand_total += total;                                               \
}  /* write_one */

  write_one("orig line modif", num_orig_line_modifs_allocated,
                               an_orig_line_modif);
  write_one("source line modif", num_source_line_modifs_allocated,
                                 a_source_line_modif);

  total = after_end_of_curr_source_line - curr_source_line;
  fprintf(f_debug, "%25s %8s %8s %8lu (gen. storage)\n", "curr_source_line",
                   "", "", total);
  grand_total += total;

  if (after_end_of_raw_listing_buffer != NULL) {
    total = after_end_of_raw_listing_buffer - raw_listing_buffer;
    fprintf(f_debug, "%25s %8s %8s %8lu (gen. storage)\n",
                     "raw_listing_buffer", "", "", total);
    grand_total += total;
  }  /* if */

  fprintf(f_debug, "%25s %8s %8s %8lu\n", "Total", "", "", grand_total);

  return (grand_total);
}  /* show_lexical_space_used */
#endif /* DEBUG */


void lexical_init(void)
/*
Initialize static variables related to the lexical routines.  This is done
as a subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  register int c;  /* Has to be "int" so "for" loop will work. */

  /* Variables in lexical.h: */
  depth_input_stack = -1;
  curr_ise = NULL;
  seq_number_last_read = 0;
  curr_seq_number = 0;
  orig_line_modif_list = NULL;
  end_orig_line_modif_list = NULL;
  avail_orig_line_modifs = NULL;
  source_line_modif_list = NULL;
  line_start_source_line_modif = NULL;
  avail_source_line_modifs = NULL;
  sequence_id_for_source_line_modifs = 0;
  delete_source_from_loc = NULL;
  /* Clear the set of tokens on which to stop a flush following a
     syntax error. */
  clear_stop_tokens();
  lint_argsused_flag = FALSE;
  lint_varargs_count = NOT_LINT_VARARGS;
  lint_notreached_flag = FALSE;

  /* Static variables in lexical.c: */
  curr_input_stream = NULL;
  eof_read_on_curr_input_stream = FALSE;
  at_end_of_source_file = FALSE;
  after_end_of_all_source = FALSE;
  curr_raw_listing_line_code = '\0';
  trapped_token = FALSE;
#if DEBUG
  num_orig_line_modifs_allocated = 0;
  num_source_line_modifs_allocated = 0;
#endif /* DEBUG */

  /* Do the initial allocation for curr_source_line the first time this
     routine is called.  Since the space is allocated in general storage,
     it does not need to be reallocated for each source file.  For the same
     reason, after_end_of_curr_source_line should not be reset. */
  if (after_end_of_curr_source_line == NULL) {
    /* First time through.  Do initial allocation for the source line.
       The space will be reallocated (larger) if necessary, but the size
       here should be big enough for the expected cases. */
    /* Allocate one more byte than required, so that a pointer past the end
       will not have the same address as a pointer to the next object in
       memory. */
    curr_source_line = alloc_general(
                            (sizeof_t)(CURR_SOURCE_LINE_INITIAL_ALLOCATION+1));
    after_end_of_curr_source_line = curr_source_line +
                                    CURR_SOURCE_LINE_INITIAL_ALLOCATION;
  }  /* if */
  /* Similar allocation for raw_listing_buffer.  Similar reasoning. */
  if (f_raw_listing != NULL) {
    if (after_end_of_raw_listing_buffer == NULL) {
      raw_listing_buffer = alloc_general(
                              (sizeof_t)RAW_LISTING_BUFFER_INITIAL_ALLOCATION);
      after_end_of_raw_listing_buffer = raw_listing_buffer +
                                        RAW_LISTING_BUFFER_INITIAL_ALLOCATION;
    }  /* if */
    clear_raw_listing_buffer();
  }  /* if */

#if CHECKING && DEBUG
  /* Check that the table of token names is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to
     update db_token_names. */
  if (db_token_names[(int)tok_last] == NULL ||
      strcmp(db_token_names[(int)tok_last], "last") != 0) {
    internal_error(
              "lexical_init: initialization of db_token_names is not correct");
  }  /* if */
#endif /* CHECKING && DEBUG */
  /* Initialize is_id_char to the characters that can appear in an identifier
     after the first character (i.e., a-z, A-Z, 0-9, and "_").
     See standard, 3.1.2.  Also used in scanning pp-numbers; the same
     set applies.  See standard, 3.1.8. */
  for (c = CHAR_MIN; c <= CHAR_MAX; c++) {
    is_id_char[c-CHAR_MIN] = isascii(c) &&
                             (isalpha(c) || isdigit(c) || (c == '_'));
  }  /* for */
  /* Also initialize pp_lexical_category, used to determine whether or
     not extra token-separating blanks are required between tokens resulting
     from macro expansion.  See gen_pp_output_for_curr_line. */
  for (c = CHAR_MIN; c <= CHAR_MAX; c++) {
    if (is_id_char[c-CHAR_MIN] || c == '.') {
      pp_lexical_category[c-CHAR_MIN] = PLC_ID_OR_NUMBER;
    } else {
      switch (c) {
        /* White-space characters are always singletons: */
        case '\f':
        case VERTICAL_TAB_CHARACTER:
        case '\t':
        case ' ':
        case '\n':
        /* Operators and punctuators that don't appear as part of other
           tokens are also singletons (see 3.1.5, 3.1.6). */
        case '[':  case ']':  case '{':  case '}':  case '(':  case ')':
        case ',':  case '~':  case ';':  case '?':
          pp_lexical_category[c-CHAR_MIN] = PLC_SINGLETON;
          break;
        case ':':
          /* In C++, ":" can be part of "::". */
          pp_lexical_category[c-CHAR_MIN] =
                            (C_dialect == C_dialect_cplusplus) ? PLC_OTHER :
                                                                 PLC_SINGLETON;
          break;
        default:
          pp_lexical_category[c-CHAR_MIN] = PLC_OTHER;
      }  /* switch */
    }  /* if */
  }  /* for */
}  /* lexical_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
