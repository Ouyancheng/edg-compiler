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

cmd_line.c -- Command-line parsing.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "pch.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */

#ifdef HOSTID
extern long gethostid(void);
#endif /* HOSTID */




/*
Structure used to map keyword and/or letter options into the
corresponding option kind.  There may be multiple option descriptions
that map to the same option kind.
*/
typedef struct an_option_description *an_option_description_ptr;
typedef struct an_option_description {
  an_option_kind
		kind;
			/* Code that indicates the action to be taken
			   when this option is used. */
  char		*keyword;
			/* The keyword option used to specify this option.
			   May be NULL if a keyword option may not be used. */
  char		letter;
			/* A single character that may be used to specify
			   this option.  May be the null character if
			   a single character option may not be used. */
  a_byte_boolean
	       	value;
			/* TRUE if the option is used to enable the
			   option, FALSE if it should disable it. */
  a_byte_boolean
	       	arg_required;
			/* TRUE if this option requires that an argument be
			   specified. */
  sizeof_t	keyword_length;
			/* Length of the keyword (not including the null
			   terminator). */
  a_pch_event_kind
		pch_event_kind;
			/* Indicates how a given command line is to be
			   handled for purposes of precompiled header
			   prefix matching. */
} an_option_description;

#define SIZE_OF_OPTION_DESCRIPTIONS ((int)optk_last + (int)optk_last/3)
 			/* The number of entries in the option descriptions
			   array.  This is larger than the number of options
			   because some options have entries to both enable
			   and disable the option.  Also, it is possible
			   to have synonyms for options. */

static an_option_description
		option_descriptions[SIZE_OF_OPTION_DESCRIPTIONS];
			/* Pointer to a linked list of option descriptions. */

static int	option_descriptions_used = 0;
			/* The number of entries that are used in the option
			   description array. */

static a_byte_boolean
		option_kind_used[(int)optk_last+1];
			/* An array indexed by option kind that indicates
			   whether the option kind has been specified in
			   the command line.  Initialized to zero by
			   static initialization. */


static void add_option_description(an_option_kind	kind,
				   char			*keyword,
				   char			letter,
				   a_boolean		value,
				   a_boolean		arg_required,
				   a_pch_event_kind	pch_event_kind)
/*
Add an entry to the linked list of option descriptions.  "keyword" is
the string to be used as the keyword form of the option and may be
NULL if there is no keyword version of the option.  "letter" is the
option letter to be used for letter style options.  It may be
the null character (\0) if no letter form of the option exists.
"value" indicates whether this option is used to turn the option
on (TRUE) or off (FALSE).  "arg_required" indicates whether an
option must be followed by an argument.  Note that optional arguments
are not supported.  "pch_event_kind" specifies how this argument should
be compared with a similar argument for precompiled header prefix
matching.
*/
{
  int				option_description_number;
  an_option_description_ptr	odp;
#if CHECKING
  {
    int	n;
    /* Make sure the option keyword and/or letter are not already in use. */
    for (n = 0; n < option_descriptions_used; ++n) {
      odp = &option_descriptions[n];
      if ((keyword != NULL && strcmp(keyword, odp->keyword) == 0) ||
          (letter != '\0' && letter == odp->letter)) {
        unexpected_condition_str2("add_option_description:",
                                  "duplicate option keyword or letter");
      }  /* if */
    }  /* for */
  }
#endif /* CHECKING */
  /* alloc_general is called (rather than alloc_fe) because the general
     mem_manage.c routines are not yet initialized. */
  option_description_number = option_descriptions_used++;
  if (option_description_number == SIZE_OF_OPTION_DESCRIPTIONS) {
    /* There are more entries than fit in the array. */
#if DEBUG
    fprintf(f_debug, "Too many options descriptions.  Current limit is %0d\n",
            SIZE_OF_OPTION_DESCRIPTIONS);
#endif /* DEBUG */
    unexpected_condition();
  } else {
    odp = &option_descriptions[option_description_number];
    odp->kind = kind;
    odp->keyword = keyword;
    odp->keyword_length = keyword == NULL ? 0 : strlen(keyword);
    odp->letter = letter;
    odp->value = value;
    odp->arg_required = arg_required;
    odp->pch_event_kind = pch_event_kind;
  }  /* if */
}  /* add_option_description */


static void initialize_option_descriptions(void)
/*
Initialize the option information table.
*/
{
  add_option_description(optk_strict_ansi_error, "strict", 'A',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_strict_ansi_warning, "strict_warnings", 'a',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_preprocess_only_no_line_dirs, "no_line_commands",
                         'P', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_preprocess_only_emit_line_dirs, "preprocess",
                         'E', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_keep_comments_in_pp_output, "comments", 'C',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  add_option_description(optk_old_line_dirs, "old_line_commands", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  add_option_description(optk_C_dialect_pcc, "old_c", 'K',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_list_makefile_dependencies, "dependencies", 'M',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_list_include_files, "trace_includes", 'H',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#if DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE
  add_option_description(optk_write_unlowered_il, "no_il_lowering", 'N',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE */
  add_option_description(optk_cplusplus_anachronisms, "anachronisms", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cplusplus_anachronisms, "no_anachronisms", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cfront_2_1_mode, "cfront_2.1", 'b',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_cfront_3_0_mode, "cfront_3.0", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_front_end_only, "no_code_gen", 'n',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_use_signed_chars, "signed_chars", 's',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_use_signed_chars, "unsigned_chars", 'u',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_template_instantiation_mode, "instantiate", 't',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  add_option_description(optk_automatic_template_instantiation,
                         "auto_instantiation", 'T',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_automatic_template_instantiation,
                         "no_auto_instantiation", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* !AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
  add_option_description(optk_implicit_template_inclusion,
                         "implicit_include", 'B',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_template_inclusion,
                         "no_implicit_include", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  add_option_description(optk_virtual_function_table_definition,
                         "suppress_vtbl", 'V',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_virtual_function_table_definition,
                         "force_vtbl", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_allow_dollar_in_id_chars,
                         "dollar", '$',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_display_compilation_time, "timing", '#',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_display_compiler_version, "version", 'v',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_suppress_warnings, "no_warnings", 'w',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_enable_remarks, "remarks", 'r',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_C_dialect_ANSI, "c", 'm',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_C_dialect_cplusplus, "c++", 'p',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_exception_handling, "exceptions", 'x',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_exception_handling, "no_exceptions", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_suppress_used_before_set_warnings,
                         "no_use_before_set_warnings", 'j',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_include_directory, "include_directory", 'I',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_define_macro, "define_macro", 'D',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_undefine_macro, "undefine_macro", 'U',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_set_error_limit, "error_limit", 'e',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_generate_raw_listing, "list", 'L',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_generate_cross_reference, "xref", 'X',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_stderr_file_name, "error_output", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_output_file_name, "output", 'o',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if BACK_END_IS_C_GEN_BE
  add_option_description(optk_module_list_for_union_init, "module_init", 'i',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* !BACK_END_IS_C_GEN_BE */
#if DEBUG
  add_option_description(optk_debug, "db", 'd',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* DEBUG */
  add_option_description(optk_diag_suppress, "diag_suppress", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_remark, "diag_remark", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_warning, "diag_warning", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_diag_error, "diag_error", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_display_error_number, "display_error_number",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  add_option_description(optk_gen_c_file_name, "gen_c_file_name",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  add_option_description(optk_create_pch, "create_pch",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_use_pch, "use_pch",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_pch, "pch",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_messages, "pch_messages",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_messages, "no_pch_messages",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
#if !USE_MMAP_FOR_MEMORY_REGIONS
  add_option_description(optk_pch_mem, "pch_mem",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  add_option_description(optk_pch_dir, "pch_dir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if RESTRICT_ALLOWED
  add_option_description(optk_restrict, "restrict",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_restrict, "no_restrict",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* RESTRICT_ALLOWED */
  add_option_description(optk_long_lifetime_temps, "long_lifetime_temps",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_long_lifetime_temps, "short_lifetime_temps",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MICROSOFT_EXTENSIONS_ALLOWED
  add_option_description(optk_microsoft_mode, "microsoft",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_mode, "no_microsoft",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  add_option_description(optk_wchar_t_is_keyword, "wchar_t_keyword",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_wchar_t_is_keyword, "no_wchar_t_keyword",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
}  /* initialize_option_descriptions */


static an_option_description_ptr look_up_option_description
					(char		*optchar,
					 a_boolean	is_keyword_option,
					 sizeof_t	keyword_length)
/*
Go through the linked list of option descriptions and look for one that
matches the specified option.  If is_keyword_option is TRUE then optchar
points to the keyword string, otherwise the character that optchar points
to is the option letter. 
*/
{
  an_option_description_ptr	odp;
  a_boolean			match = FALSE;
  int				n;

  for (n = 0; n < option_descriptions_used; ++n) {
    odp = &option_descriptions[n];
    if (is_keyword_option) {
      match = odp->keyword != NULL &&
              keyword_length == odp->keyword_length &&
              strncmp(optchar, odp->keyword, size_t_arg(keyword_length)) == 0;
    } else {
      match = odp->letter != '\0' && *optchar == odp->letter;
    }  /* if */
    if (match) break;
  }  /* for */
  if (!match) odp = NULL;
  /* Record the fact that this option kind has been used. */
  if (odp != NULL) option_kind_used[(int)odp->kind] = TRUE;
  return odp;
}  /* look_up_option_description */


static char	*opt_arg;
			/* Returned from get_option -- Pointer to the current
			   option argument. */
static int	opt_ind = 1;
			/* Index of the current option in argv. */

static void invalid_argument_error(int	argc,
                                   char	**argv)
/*
Issue a invalid command line argument diagnostic.
*/
{
  /* Reset opt_ind in the case of an option requiring
     an argument where the argument is missing. */
  if (opt_ind >= argc) opt_ind = argc-1;
  opt_arg = argv[opt_ind];
  /* This call terminates the program. */
  str_command_line_error(ec_cl_invalid_option, opt_arg);
}  /* invalid_argument_error */


static an_option_description_ptr get_option(int     argc,
                                            char    **argv)
/*
Fetch a command-line option.  argc and argv are the count of
command-line arguments and the array containing the command-line
argument strings.  This routine accepts both single character options
and keyword options.  One option is returned on each call.  The option
is looked up in the options_descriptions list and a pointer to the
option description structure is returned to the caller.  A NULL
pointer is returned when the end of the option list is found.  opt_ind
at that point indicates the argv index of the non-option argument.  If
an invalid option is used, a command line error will be issued (and
the compilation will be terminated).

The following option formats are supported:

	-abcxxx		-- turns on the "a" and "b" options and supplies the
			   argument "xxx" to the "c" option.

	-abc xxx	-- same as above

	--option_a	-- keyword option with no argument

	--option_b xxx	-- keyword option with an argument

	--option_b=xxx	-- keyword option with an argument (not that no
			   spaces are allowed on either side of the
			   equals sign.
*/
{
  static char			*optchar = NULL;
				/* The character position containing the
				   next option letter to be examined, or NULL
				   if a new argument should be begun. */
  a_boolean			is_keyword_option = FALSE;
  sizeof_t			keyword_length = 0;
  an_option_description_ptr	odp = NULL;
  char				*after_keyword;

  /* See if a new argument must be begun (i.e., there is not
     part of an existing option to finish). */
  while (optchar == NULL || *optchar == '\0') {
    /* Here, optchar points to the next option character.  See if the
       current option letter list has been exhausted. */
    if (optchar != NULL && *optchar == '\0') {
      /* Start the next argument. */
      opt_ind++;
    } /* if */
    if (opt_ind >= argc) {
      /* No more arguments. */
      goto end_of_routine;
    } else {
      optchar = argv[opt_ind];
      if (*optchar != '-') {
        /* The argument string does not begin with a "-". */
        goto end_of_routine;
      } else if (*(optchar+1) == '-') {
        /* Either the beginning of a keyword option, or "--", which marks the
           end of the options. */
        if (*(optchar+2) == '\0') {
          /* The argument is "--", which marks the end of the options.
             Swallow this argument. */
          opt_ind++;
          goto end_of_routine;
        } else {
          /* The beginning of a keyword option (e.g., --exceptions).
             Update optchar to point to the keyword after the "--". */
          is_keyword_option = TRUE;
          optchar += 2;
          /* Find the end of the keyword.  The keyword ends either at
             the end of the current command line argument or when an
             equals sign is found.  The equals sign separates the
             keyword from its argument. */
          after_keyword = optchar;
          while (*after_keyword != '\0' && *after_keyword != '=') {
            after_keyword++;
          }  /* while */
          keyword_length = after_keyword - optchar;
        }  /* if */
      } else if (*(optchar+1) == '\0') {
        /* The argument is "-", which is used to indicate stdin as a
           file name.  Return without swallowing this argument. */
        goto end_of_routine;
      } else {
        /* We have the start of a new option.  Advance past the "-". */
        optchar++;
      }  /* if */
    }  /* if */
  }  /* while */
  /* See if the option letter or keyword is valid. */
  odp = look_up_option_description(optchar, is_keyword_option,
                                   keyword_length);
  /* See if the option letter appears in the string of legal options. */
  if (odp == NULL) invalid_argument_error(argc, argv);
  /* See if the option takes an argument. */
  if (odp->arg_required) {
    if (is_keyword_option) {
      /* Keyword options may be specified as either:

		--keyword=argument        (with no spaces)
         or     --keyword argument
      */
      if (*after_keyword == '=') {
        /* Set the option pointer to the character after the "=". */
        opt_arg = after_keyword + 1;
        if (*opt_arg == '\0') invalid_argument_error(argc, argv);
      } else {
        /* Use the next argument as the option value, as in "--output xxx". */
        opt_ind++;
        /* If there are no more arguments, the option is missing. */
        if (opt_ind >= argc) invalid_argument_error(argc, argv);
        opt_arg = argv[opt_ind];
      }  /* if */
    } else {
      /* Not a keyword option, the argument may immediately following the
         letter or may be in the next argv element. */
      if (*(optchar+1) == '\0') {
        /* The option letter is the last thing in the argument, so use the
           next argument as the option value, as in "-I xxx". */
        opt_ind++;
        /* If there are no more arguments, the option is missing. */
        if (opt_ind >= argc) invalid_argument_error(argc, argv);
        opt_arg = argv[opt_ind];
      } else {
        /* The option argument is the remainder of the current argument,
           as in "-Ixxx". */
        opt_arg = optchar+1;
      }  /* if */
    }  /* if */
    /* In any case, take no more characters of the current argument. */
    optchar = NULL;
    opt_ind++;
  } else {
    /* The option does not take an argument. */
    opt_arg = NULL;
    if (is_keyword_option) {
      /* Skip to the next element of argv. */
      optchar = NULL;
      opt_ind++;
    } else {
      /* Skip to the next character of the current element of argv. */
      optchar++;
    }  /* if */
  }  /* if */
end_of_routine:
  return odp;
}  /* get_option */


static void add_to_def_undef_list(char *str,
                                  a_def_undef_string_ptr *du_list)
/*
Add the string pointed to by str (which comes from a command-line -D
or -U macro define/undefine option) to the list of def/undef strings
pointed to by *du_list.
*/
{
  a_def_undef_string_ptr du_new;

  /* alloc_general is called (rather than alloc_fe) because the general
     mem_manage.c routines are not yet initialized, and because the
     strings may be used several times if several files are compiled. */
  du_new = (a_def_undef_string_ptr)alloc_general(sizeof(a_def_undef_string));
  du_new->next = *du_list;
  du_new->text = str;
  /* Add the new entry to the front of the list of defs or undefs. */
  *du_list = du_new;
}  /* add_to_def_undef_list */


static long scan_opt_arg_number(char *optstr)
/*
Scan an argument option as a decimal number, and return its value.
*/
{
  char *arg_ptr;
  long result = 0;
  int  digit;

  for (arg_ptr = optstr; *arg_ptr != '\0'; arg_ptr++) {
    if (!isdigit((unsigned char)*arg_ptr)) goto number_error;
    digit = *arg_ptr - '0';
    if (result > LONG_MAX / 10) goto number_error;
    result *= 10;
    if (result > LONG_MAX-digit) goto number_error;
    result += digit;
  }  /* for */
  goto return_point;
number_error:
  str_command_line_error(ec_cl_invalid_number, optstr);
return_point:
  return result;
}  /* scan_opt_arg_number */


static void process_diag_override_option(an_option_kind kind,
					 char		*opt_arg)
/*
Go through a comma separated list of error tags and call an error
processing routine to update the severity.
*/
{
  char			*local_opt_arg;
  int			number_of_arguments = 0;
  int			i;
  an_error_severity	severity;
  char			*ptr;

  /* Make a local copy of the option string.  Remove any blanks and replace
     commas with null characters.  Note that this copy is simply discarded
     after it is used. */
  local_opt_arg = (char *)alloc_general((sizeof_t)(strlen(opt_arg) + 1));
  {
    char	*src = opt_arg;
    char	*dest = local_opt_arg;
    char	ch;
    do {
      ch = *src;
      /* Remove blanks. */
      if (ch == ' ') continue;
      /* Replace commas with null characters. */
      if (ch == ',') ch = '\0';
      /* Keep track of the number of arguments found. */
      if (ch == '\0') number_of_arguments++;
      *dest++ = ch;
    } while (*src++ != '\0');
  }
  /* Convert the option kind into an error severity. */
  switch (kind) {
    case optk_diag_suppress: severity = es_none;                break;
    case optk_diag_remark:   severity = es_remark;              break;
    case optk_diag_warning:  severity = es_warning;             break;
    case optk_diag_error:    severity = es_discretionary_error; break;
    default: unexpected_condition();
  }  /* switch */
  /* Loop through the arguments and call a routine to update the
     error severity for the specified tag. */
  ptr = local_opt_arg;
  for (i = 0; i < number_of_arguments; ++i) {
    char	*opt_start = ptr;
    char	*opt_end = strchr(ptr, '\0');
    a_boolean	error;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Setting error severity for: %s\n", opt_start);
    }  /* if */
#endif /* DEBUG */
    if (isdigit(*opt_start)) {
      int error_number = (int)scan_opt_arg_number(opt_start);
      error = set_severity_for_error_number(error_number, severity);
      if (error) {
        str_command_line_error(ec_cl_invalid_error_number, opt_start);
      }  /* if */
    } else {
      error = set_severity_for_error_tag(opt_start, severity);
      if (error) {
        str_command_line_error(ec_cl_invalid_error_tag, opt_start);
      }  /* if */
    }  /* if */
    ptr = opt_end + 1;
  }  /* for */
}  /* process_diag_override_option */


#if COMPILE_MULTIPLE_SOURCE_FILES
static char	**argv_file_list;
static int	argc_file_list;
			/* When multiple source input files are accepted,
			   argc_file_list is the count of files remaining
			   after the current one, and argv_file_list
			   points to the argv entry for the first
			   remaining file. */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */


void proc_command_line(int argc, char *argv[])
/*
Process the arguments on the command line that invoked the compiler.
*/
{
  an_option_description_ptr	odp;
  char 			        *ofile_name = NULL;
  a_boolean			cannot_open;
  a_boolean			bad_name;
  char				*instantiation_mode_string = NULL;
  a_directory_name_entry_ptr	include_path_boundary = NULL;
#if !USE_MMAP_FOR_MEMORY_REGIONS
  a_boolean			non_pch_option_used = FALSE;
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */

  /* Set a current position indicating we are looking at the command line. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_CMD_LINE;
  set_err_pos_to_curr_token();

  /* Initialize the table of option descriptions used by the options
     processing routine. */
  initialize_option_descriptions();

#ifdef HOSTID
  /* Check that this code is running on an acceptable CPU. */
  /* Use an exclusive-or to make it harder to find and patch the host id
     in the compiler executable. */
#define HOSTID_MASK 0x08251954
  { long lhostid = gethostid() ^ HOSTID_MASK;
    if (lhostid != (HOSTID ^ HOSTID_MASK) && 
#ifdef HOSTID2
        lhostid != (HOSTID2 ^ HOSTID_MASK) &&
#endif /* ifdef HOSTID2 */
                                             ) {
      command_line_error(ec_cl_incorrect_host_id);
    }  /* if */
  }
#endif /* ifdef HOSTID */

  /* Start with empty include file search paths.  Entries may be added
     because of command line options, and others will be added as defaults. */
  incl_search_path = end_incl_search_path = sys_incl_search_path = NULL;
  put_dir_of_each_opened_source_file_on_incl_search_path = TRUE;
  /* Scan the command-line options. */
  while ((odp = get_option(argc, argv)) != NULL) {
    an_option_kind	kind = odp->kind;
    a_boolean		opt_value = odp->value;
    /* Record information about this option for precompiled header
       processing. */
    if (odp->pch_event_kind != pchek_none) {
      add_command_line_pch_event(odp->pch_event_kind, kind, opt_value,
                                 opt_arg);
    }  /* if */
#if !USE_MMAP_FOR_MEMORY_REGIONS
    {
      a_boolean		is_pch_option;
      /* See if this is a PCH option. */
      switch (kind) {
        case optk_create_pch:
        case optk_use_pch:
        case optk_pch:
        case optk_pch_messages:
        case optk_pch_mem:
        case optk_pch_dir:
          is_pch_option = TRUE;
          break;
        default:
          is_pch_option = FALSE;
         break;
      }  /* switch */
      /* When using preallocated memory for PCH processing, the PCH options
         must be first on the command line. */
      if (non_pch_option_used && is_pch_option) {
        /* The PCH options must precede all other options. */
        command_line_error(ec_cl_pch_must_be_first);
      }  /* if */
      if (!non_pch_option_used && !is_pch_option) {
        /* When we have processed all of the PCH options, do the preallocation
           of the PCH memory. */
        if (precompiled_header_processing_required) {
          preallocate_pch_memory();
        }  /* if */
        non_pch_option_used = TRUE;
      }  /* if */
    }
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
    switch (kind) {
      case optk_strict_ansi_error:
      case optk_strict_ansi_warning:
        /* Warn on non-ANSI features, disable features that conflict
           with ANSI.  Note that "ANSI" means ANSI C or ANSI C++, depending
           on the C_dialect setting. */
        check_assertion(opt_value == TRUE);
        strict_ansi_mode = TRUE;
        if (kind == optk_strict_ansi_error) {
          strict_ansi_error_severity = es_error;
          strict_ansi_discretionary_severity = es_discretionary_error;
        } else {
          strict_ansi_error_severity = es_warning;
          strict_ansi_discretionary_severity = es_warning;
        }  /* if */
        break;
      case optk_preprocess_only_emit_line_dirs:
        /* Do preprocessing only, output to stdout, with #line information. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = TRUE;
        gen_line_info_in_pp_output = TRUE;
        break;
      case optk_preprocess_only_no_line_dirs:
        /* Do preprocessing only, output to stdout (driver remaps to .i file),
           without #line information. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = TRUE;
        gen_line_info_in_pp_output = FALSE;
        break;
      case optk_keep_comments_in_pp_output:
        /* Keep comments in preprocessing output. */
        check_assertion(opt_value == TRUE);
        keep_comments_in_pp_output = TRUE;
        break;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
      case optk_old_line_dirs:
        /* Generate old-style line directives in generated C/C++ output,
           i.e., "# nnn" instead of "#line nnn". */
        check_assertion(opt_value == TRUE);
        gen_old_style_line_dirs = TRUE;
        break;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
      case optk_C_dialect_pcc:
        /* Compile K&R/pcc dialect of C. */
        check_assertion(opt_value == TRUE);
        C_dialect = C_dialect_pcc;
        break;
      case optk_list_makefile_dependencies:
        /* Generate makefile dependency lines for #include files encountered,
           but do not compile. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = FALSE;
        list_included_files = FALSE;
        list_makefile_dependencies = TRUE;
        error_threshold = es_discretionary_error;
        break;
      case optk_list_include_files:
        /* Generate on stdout a list of the names of the #include files
           processed, but do not compile. */
        check_assertion(opt_value == TRUE);
        do_preprocessing_only = TRUE;
        generate_pp_output = FALSE;
        list_included_files = TRUE;
        list_makefile_dependencies = FALSE;
        error_threshold = es_discretionary_error;
        break;
#if DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE
      case optk_write_unlowered_il:
	/* Suppress IL lowering and write an unlowered IL file. */
        check_assertion(opt_value == TRUE);
	suppress_il_lowering = TRUE;
        suppress_back_end = TRUE;
        /* Note that suppress_il_file_write is not set. */
	break;
#endif /* DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE */
      case optk_cplusplus_anachronisms:
        /* Enable or disable acceptance of anachronisms. */
        allow_anachronisms = opt_value;
        break;
      case optk_cfront_2_1_mode:
        /* cfront 2.1 compatibility mode.  If both 2.1 and 3.0 modes are
           selected, only the most recent applies. */
        check_assertion(opt_value == TRUE);
        cfront_2_1_mode = TRUE;
        cfront_3_0_mode = FALSE;
        /* This option implies C++ dialect. */
        C_dialect = C_dialect_cplusplus;
        allow_anachronisms = TRUE;
        long_lifetime_temps = TRUE;
        break;
      case optk_cfront_3_0_mode:
        /* cfront 3.0 compatibility mode.  If both 2.1 and 3.0 modes are
           selected, only the most recent applies. */
        check_assertion(opt_value == TRUE);
        cfront_3_0_mode = TRUE;
        cfront_2_1_mode = FALSE;
        /* This option implies C++ dialect. */
        C_dialect = C_dialect_cplusplus;
        allow_anachronisms = TRUE;
        long_lifetime_temps = TRUE;
        break;
      case optk_front_end_only:
        /* Run just the front end to do syntax checking; do not run the back
           end. */
        check_assertion(opt_value == TRUE);
        suppress_back_end = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
        suppress_il_file_write = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if DO_IL_LOWERING
	suppress_il_lowering = TRUE;
#endif /* DO_IL_LOWERING */
        break;
      case optk_use_signed_chars:
        /* Use signed or unsigned chars. */
        targ_has_signed_chars = opt_value;
        break;
      case optk_template_instantiation_mode:
        /* Template instantiation mode. */
        instantiation_mode_string = opt_arg;
        /* Determine the template instantiation mode to be used. */
        if (instantiation_mode_string != NULL) {
          if (strcmp(instantiation_mode_string, "none") == 0) {
            instantiation_mode = tim_none;
          } else if (strcmp(instantiation_mode_string, "all") == 0) {
            instantiation_mode = tim_all;
          } else if (strcmp(instantiation_mode_string, "used") == 0) {
            instantiation_mode = tim_used;
          } else if (strcmp(instantiation_mode_string, "local") == 0) {
            instantiation_mode = tim_local;
          } else {
            str_command_line_error(ec_cl_invalid_instantiation_mode,
      			           instantiation_mode_string);
          }  /* if */
        }  /* if */
        break;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
      case optk_automatic_template_instantiation:
        /* Enable or disable automatic instantiation processing. */
        automatic_instantiation_mode = opt_value;
        break;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      case optk_implicit_template_inclusion:
        /* Enable or disable implicit inclusion of template definition source
           files. */
        implicit_template_inclusion_mode = opt_value;
        break;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
      case optk_virtual_function_table_definition:
        /* Control generation of a virtual function table if unable to
	   determine absolute means to avoid duplicate virtual function 
	   table entries in separate compilations.  --force_vtbl gives
           opt_value TRUE; --suppress_vtbl gives opt_value FALSE. */
        virtual_function_table_definition =
                                          opt_value ? vfd_force : vfd_suppress;
	break;
      case optk_allow_dollar_in_id_chars:
        /* Determines whether dollar signs are accepted in identifiers. */
        allow_dollar_in_id_chars = opt_value;
        break;
      case optk_display_compilation_time:
        /* Generate compilation timing information. */
        check_assertion(opt_value == TRUE);
        display_compilation_time = TRUE;
        break;
      case optk_display_compiler_version:
        /* Print out compiler version. */
        check_assertion(opt_value == TRUE);
        fprintf(stderr,
                "Edison Design Group C/C++ Front End, version %s (%s %s)\n",
                VERSION_NUMBER, build_date, build_time);
        fprintf(stderr, "Copyright 1988-1994 Edison Design Group Inc.\n");
#ifdef DEMO_VERSION_ID
        fprintf(stderr, "Demonstration version for %s\n", DEMO_VERSION_ID);
#endif /* ifdef DEMO_VERSION_ID */
        fputc('\n', stderr);
        break;
      case optk_suppress_warnings:
        /* Suppress warnings. */
        check_assertion(opt_value == TRUE);
        error_threshold = es_discretionary_error;
        break;
      case optk_enable_remarks:
        /* Enable remarks. */
        check_assertion(opt_value == TRUE);
        error_threshold = es_remark;
        break;
      case optk_C_dialect_ANSI:
        /* Compile ANSI C. */
        check_assertion(opt_value == TRUE);
        C_dialect = C_dialect_ANSI;
        break;
      case optk_C_dialect_cplusplus:
        /* Compile C++. */
        check_assertion(opt_value == TRUE);
        C_dialect = C_dialect_cplusplus;
        break;
      case optk_exception_handling:
        /* Enables or disables support for exceptions. */
        exceptions_enabled = opt_value;
        break;
      case optk_suppress_used_before_set_warnings:
        /* Suppress used-before-set warnings. */
        check_assertion(opt_value == TRUE);
        suppress_used_before_set_warnings = TRUE;
        break;
      case optk_include_directory:
        /* Include file directory, add to list. */
        if (*opt_arg == '-') {
          /* -I- marks the dividing line between directories for "..."
             includes and those for <...> includes.  It also suppresses
             pushing the directory of each source file onto the search
             path, which is useful for viewpathing. */
          include_path_boundary = incl_search_path;
          put_dir_of_each_opened_source_file_on_incl_search_path = FALSE;
        } else {
          /* Normal -I directive. */
          add_to_include_search_path(opt_arg);
        }  /* if */
        break;
      case optk_define_macro:
        /* Define a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(opt_arg, &defs_from_cmd_line);
        break;
      case optk_undefine_macro:
        /* Undefine a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(opt_arg, &undefs_from_cmd_line);
        break;
      case optk_set_error_limit:
        /* Set error limit (numbers of errors at which to give up on
           compilation). */
        error_limit = scan_opt_arg_number(opt_arg);
        if (error_limit <= 0) {
          str_command_line_error(ec_cl_invalid_error_limit, opt_arg);
        }  /* if */
        break;
      case optk_generate_raw_listing:
        /* Generate a file of raw listing information (source lines,
           file/line information, and indications of which lines are which,
           to be read later by a program that will generate an
           interspersed listing). */
        f_raw_listing = open_output_file(opt_arg, /*binary_file=*/FALSE,
                                         /*update_mode=*/FALSE,
                                         &cannot_open, &bad_name);
        if (bad_name) {
          str_command_line_error(ec_cl_invalid_raw_listing_output_file,
                                 opt_arg);
        } else if (cannot_open) {
          str_command_line_error(ec_cl_cannot_open_raw_listing_output_file,
                                 opt_arg);
        }  /* if */
        break;
      case optk_generate_cross_reference:
        /* Generate a file of cross-reference information (locations and
	   kinds of references to symbols) */
        f_xref_info = open_output_file(opt_arg, /*binary_file=*/FALSE,
                                       /*update_mode=*/FALSE,
                                       &cannot_open, &bad_name);
        if (bad_name) {
          str_command_line_error(ec_cl_invalid_xref_output_file,
                                 opt_arg);
        } else if (cannot_open) {
          str_command_line_error(ec_cl_cannot_open_xref_output_file,
                                 opt_arg);
        }  /* if */
        break;
      case optk_stderr_file_name:
        /* Redirect stderr to a file.  This is useful on systems where
           redirection is not well supported. */
        reopen_error_output_file(opt_arg, &cannot_open, &bad_name);
        if (bad_name) {
          str_command_line_error(ec_cl_invalid_error_output_file,
                                 opt_arg);
        } else if (cannot_open) {
          str_command_line_error(ec_cl_cannot_open_error_output_file,
                                 opt_arg);
        }  /* if */
        break;
      case optk_output_file_name:
        /* Specify output file for preprocessing output or IL. */
        ofile_name = opt_arg;
        break;
#if BACK_END_IS_C_GEN_BE
      case optk_module_list_for_union_init:
        /* Save a string of comma-separated module names that will be linked
           with this one.  This is used by c_gen_be to generate calls
           to file-scope initialization routines that handle union
           initialization.  This option is only needed if c_gen_be is being
           used to generate C output for testing. */
        module_list_for_union_init = opt_arg;
        break;
#endif /* BACK_END_IS_C_GEN_BE */
#if DEBUG
      case optk_debug:
        /* Set debug level. */
        if (proc_debug_option(opt_arg)) {
	  command_line_error(ec_cl_error_in_debug_option_argument);
	}  /* if */
        init_debug_level = debug_level;
        break;
#endif /* DEBUG */
      case optk_diag_suppress:
      case optk_diag_remark:
      case optk_diag_warning:
      case optk_diag_error:
        /* Options that override the severity of a given diagnostic.  The
           option argument contains a comma separated list of error tags. */
        process_diag_override_option(kind, opt_arg);
        break;
      case optk_display_error_number:
        /* Display the error number in diagnostic messages. */
        check_assertion(opt_value == TRUE);
        display_error_number = TRUE;
        break;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
      case optk_gen_c_file_name:
        /* The name to be used for the generated C file. */
        gen_c_file_name = opt_arg;
        break;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
      case optk_create_pch:
        /* Create precompiled header file as part of this compilation. */
        check_assertion(opt_value == TRUE);
        create_precompiled_header = TRUE;
        precompiled_header_processing_required = TRUE;
        pch_output_file_name = opt_arg;
        automatic_pch_processing = FALSE;
        use_precompiled_header = FALSE;
        break;
      case optk_use_pch:
        /* Use a precompiled header file as part of this compilation. */
        check_assertion(opt_value == TRUE);
        use_precompiled_header = TRUE;
        pch_input_file_name = opt_arg;
        precompiled_header_processing_required = TRUE;
        automatic_pch_processing = FALSE;
        create_precompiled_header = FALSE;
        break;
      case optk_pch:
        /* Do automatic precompiled header processing as part of this
           compilation. */
        check_assertion(opt_value == TRUE);
        automatic_pch_processing = TRUE;
        use_precompiled_header = FALSE;
        create_precompiled_header = FALSE;
        precompiled_header_processing_required = TRUE;
        break;
      case optk_pch_messages:
        /* Enable or suppress PCH messages. */
        suppress_pch_messages = !opt_value;
        break;
#if !USE_MMAP_FOR_MEMORY_REGIONS
      case optk_pch_mem:
        /* Specify the size of the preallocated memory to be used for
           PCH processing.  The value specified on the command line is
           size in 1k (1024) units to be allocated. */
        pch_mem_size = scan_opt_arg_number(opt_arg) * 1024;
        if (pch_mem_size <= 0 ||
            pch_mem_size > (SIZE_OF_MEM_ALLOC_HISTORY *
                                                 HOST_ALLOCATION_INCREMENT)) {
          str_command_line_error(ec_cl_invalid_pch_size, opt_arg);
        }  /* if */
        break;
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
      case optk_pch_dir:
        /* Directory to be used for PCH files. */
        pch_dir_name = opt_arg;
        break;
#if RESTRICT_ALLOWED
      case optk_restrict:
        /* Enables or disables recognition of the restrict token. */
        restrict_recognized = opt_value;
        break;
#endif /* RESTRICT_ALLOWED */
      case optk_long_lifetime_temps:
        /* Long or short lifetime temporaries. */
        long_lifetime_temps = opt_value;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_microsoft_mode:
        /* Enable or disable Microsoft extensions. */
        microsoft_mode = opt_value;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case optk_wchar_t_is_keyword:
        /* wchar_t is or is not a keyword. */
        wchar_t_is_keyword = opt_value;
        break;
      default:
        /* It should not be possible to get here. */
        unexpected_condition();
    }  /* switch */
  }  /* while */
#if !USE_MMAP_FOR_MEMORY_REGIONS
    if (!non_pch_option_used) {
      /* If all of the command line options are PCH options, then the PCH
         memory may not have been allocated yet. */
      if (precompiled_header_processing_required) {
        preallocate_pch_memory();
      }  /* if */
    }  /* if */
#endif /* !USE_MMAP_FOR_MEMORY_REGIONS */
  /* Check for the use of C++ options when the dialect being compiled
     is not C++. */
  if (C_dialect != C_dialect_cplusplus) {
    if (option_kind_used[(int)optk_cplusplus_anachronisms]) {
      command_line_error(ec_cl_anachronism_option_only_in_cplusplus);
    }  /* if */
    if (option_kind_used[(int)optk_virtual_function_table_definition]) {
      command_line_error(ec_cl_vtbl_option_only_in_cplusplus);
    }  /* if */
    if (option_kind_used[(int)optk_template_instantiation_mode]) {
      command_line_error(ec_cl_instantiation_option_only_in_cplusplus);
    }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (option_kind_used[(int)optk_automatic_template_instantiation]) {
      command_line_error(ec_cl_auto_instantiation_option_only_in_cplusplus);
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (option_kind_used[(int)optk_implicit_template_inclusion]) {
      command_line_error(ec_cl_implicit_inclusion_option_only_in_cplusplus);
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    if (option_kind_used[(int)optk_exception_handling]) {
      command_line_error(ec_cl_exceptions_option_only_in_cplusplus);
    }  /* if */
    if (option_kind_used[(int)optk_wchar_t_is_keyword]) {
      command_line_error(ec_cl_wchar_t_option_only_in_cplusplus);
    }  /* if */
    /* Set wchar_t_is_keyword to FALSE, just in case the default value
       is TRUE.  The value must not be TRUE in C mode. */
    wchar_t_is_keyword = FALSE;
  }  /* if */
  if (strict_ansi_mode) {
    /* Strict ANSI mode is incompatible with K&R/pcc mode. */
    if (C_dialect == C_dialect_pcc) {
      command_line_error(ec_cl_strict_ansi_incompatible_with_pcc);
    }  /* if */
    /* Strict ANSI mode is incompatible with cfront compatibility mode. */
    if (any_cfront_mode()) {
      command_line_error(ec_cl_strict_ansi_incompatible_with_cfront);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Strict ANSI mode is incompatible with Microsoft mode. */
    if (microsoft_mode) {
      command_line_error(ec_cl_strict_ansi_incompatible_with_microsoft);
    }  /* if */
    /* cfront mode is incompatible with Microsoft mode. */
    if (microsoft_mode) {
      command_line_error(ec_cl_cfront_incompatible_with_microsoft);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Strict ANSI mode is incompatible with allowing anachronisms.  Don't
       give an error if allow anachronisms is the default -- quietly
       set the flag to not allow anachronisms. */
    if (allow_anachronisms) {
      if (option_kind_used[(int)optk_cplusplus_anachronisms]) {
        /* Anachronisms were enabled by a command line option. */
        command_line_error(ec_cl_strict_ansi_incompatible_with_anachronisms);
      } else {
        /* Anachronisms enabled by default.  Silently disable them in
           strict mode. */
        allow_anachronisms = FALSE;
      }  /* if */
    }  /* if */
    /* Make sure that strict ANSI messages come out even if the
       error threshold was set at a higher level. */
    if ((int)error_threshold > (int)strict_ansi_error_severity) {
      error_threshold = strict_ansi_error_severity;
    }  /* if */
  }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (instantiation_mode == tim_local && automatic_instantiation_mode) {
    /* -tlocal mode cannot be used with automatic instantiation.  If
       automatic instantiation was explicitly requested on the command
       line then issue an error; otherwise disable automatic instantiation. */
    if (automatic_instantiation_mode != DEFAULT_AUTOMATIC_INSTANTIATION_MODE) {
      command_line_error(ec_cl_tim_local_conflicts_with_auto_instantiation);
    } else {
      automatic_instantiation_mode = FALSE;
    }  /* if */
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  /* Determine the appropriate error level for anachronism messages based
     on whether anachronisms are to be allowed. */
  anachronism_error_severity = allow_anachronisms ? es_warning : es_error;
  /* Choose the style of preprocessing. */
  pcc_preprocessing_mode = (C_dialect == C_dialect_pcc);
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
  if (any_cfront_mode()) {
    /* When configured that way, use old-style preprocessing for cfront
       compatibility mode. */
    pcc_preprocessing_mode = TRUE;
  }  /* if */
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */

  /* Add the default directories to the end of the include search path.
     The list is then any -I directories, in the order they were specified,
     and the default directories at the end. */
  add_default_include_search_path();
  /* If there was a -I- option, the system include search path starts at
     the indicated point.  Otherwise, the system include search path is
     the same as the normal search path. */
  if (include_path_boundary != NULL) {
    sys_incl_search_path = include_path_boundary->next;
  } else {
    sys_incl_search_path = incl_search_path;
  }  /* if */

  /* Pick up the source file name. */
  if (opt_ind >= argc) {
    command_line_error(ec_cl_missing_source_file_name);
  }  /* if */
  opt_arg = argv[opt_ind++];
  /* If the name is "-", use stdin for input. */
  if (strcmp(opt_arg, "-") == 0) opt_arg = FILE_NAME_FOR_STDIN;
  primary_source_file_name = opt_arg;
  if (put_dir_of_each_opened_source_file_on_incl_search_path) {
    /* Add the directory of the source file to the front of the include file
       search path.  gs_directory_of returns the directory part of the
       name allocated in general (not IL) storage. */
    /* If you change this, see the similar code in get_next_source_file. */
#ifdef USING_PURIFY
    /* This directory name is, under certain conditions, discarded later
       in the compilation process.  Save a pointer here to prevent
       Purify from complaining about the leaked memory. */
    static char	*dir_name;
#else /* !USING_PURIFY */
    char	*dir_name;
#endif /* USING_PURIFY */
    dir_name = gs_directory_of(primary_source_file_name);
    add_to_front_of_include_search_path(dir_name);
  }  /* if */
#if COMPILE_MULTIPLE_SOURCE_FILES
  /* Multiple source files can be compiled.  Save the count and argv
     position of remaining files, if any. */
  argc_file_list = argc - opt_ind;
  more_than_one_source_file = (argc_file_list > 0);
  if (more_than_one_source_file) {
    argv_file_list = &argv[opt_ind];
    /* There are at least two files.  The -o, -L, and -X options (those
       that specify output files) cannot be used, because they only specify
       one file. */
    if (ofile_name != NULL || f_raw_listing != NULL || f_xref_info != NULL) {
      command_line_error(ec_cl_output_file_incompatible_with_multiple_inputs);
    }  /* if */
    if (precompiled_header_processing_required) {
      command_line_error(ec_cl_pch_incompatible_with_multiple_inputs);
    }  /* if */
  }  /* if */
#else /* !COMPILE_MULTIPLE_SOURCE_FILES */
  /* Multiple source files cannot be compiled. */
  /* Check that all command-line arguments were taken. */
  if (opt_ind < argc) {
    command_line_error(ec_cl_too_many_arguments);
  }  /* if */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */

  if (do_preprocessing_only) {
    /* Doing preprocessing only suppresses running the back end (and
       generating an IL file). */
    suppress_back_end = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    suppress_il_file_write = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if DO_IL_LOWERING
    suppress_il_lowering = TRUE;
#endif /* DO_IL_LOWERING */
    /* Since preprocessing output is being generated, the output file
       name can be specified by a -o option. */
    pp_file_name = ofile_name;
    ofile_name = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  } else {
    /* Establish the intermediate file name (if it is needed). */
    if (!suppress_il_file_write) {
      /* The intermediate language should be written to a file; the file
         name can be specified by a -o option. */
      il_file_name = ofile_name;
      ofile_name = NULL;
    }  /* if */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  }  /* if */

  /* If the -o option appeared, its file should have been taken for
     something. */
  if (ofile_name != NULL) {
    command_line_error(ec_cl_no_output_file_needed);
  }  /* if */
}  /* proc_command_line */


#if COMPILE_MULTIPLE_SOURCE_FILES
a_boolean get_next_source_file(void)
/*
Check to see if there is another source input file on the command line.
If not, return FALSE.  If so, return TRUE and also set
primary_source_file_name appropriately.
This routine is only called for file names after the first;
proc_command_line handles the first file directly.
*/
{
  a_boolean another_file;

  another_file = (argc_file_list > 0);
  if (another_file) {
    /* There is another file. */
    argc_file_list--;
    primary_source_file_name = *(argv_file_list)++;
    if (put_dir_of_each_opened_source_file_on_incl_search_path) {
      /* Update the first entry of the include file search list, the one
         that contains the directory of the primary source file. */
      /* If you change this, see the similar code in proc_command_line. */
      change_primary_include_search_dir(
                                    gs_directory_of(primary_source_file_name));
    }  /* if */
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    il_file_name = NULL;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
    pp_file_name = NULL;
    f_raw_listing = f_xref_info = NULL;
  }  /* if */
  return another_file;
}  /* get_next_source_file */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
