/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

cmd_line.c -- Command-line parsing.

*/

#include "basics.h"
#include "cmd_line.h"
#include "host_envir.h"
#include "error.h"
#include "lang_feat.h"
#include "lexical.h"
#include "mem_manage.h"
#include "il.h"
#include "debug.h"
#include "version.h"

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if BACK_END_IS_C_GEN_BE
#include "c_gen_be.h"
#endif /* BACK_END_IS_C_GEN_BE */

#ifdef HOSTID
extern long gethostid(void);
#endif /* HOSTID */

#if __SYSV__ && !__VMS__

/* External declarations for getopt. */
int getopt(int argc, char *argv[], char *optstring);
extern char *optarg;
extern int optind, opterr;

#else /* __ANSIC__ || __BSD__ || __VMS__ */

/*
Define a getopt-equivalent for systems that don't have one.
*/
char		*optarg;
			/* Returned from getopt -- Pointer to the current
			   option argument. */
int		optind = 1;
			/* Index of the current option in argv. */
int		opterr = 1;
			/* If non-zero, produce an error message on
			   a bad option. */

int getopt(int argc, char *argv[], char *optstring)
/*
Fetch a command-line option.  This routine is a functional analogue of
the System V getopt routine (see the SVID, getopt(BA_LIB)).  argc and
argv are the count of command-line arguments and the array containing
the command-line argument strings.  *optstring is a string of
recognized option letters; for those options that take an argument,
the letter is followed by a ":".  One option letter (with optarg
pointing to the option value if appropriate) is returned on each call.
When there are no more options (i.e., the next thing does not begin with
a "-", or it is "--" or "-"), getopt returns EOF.  optind at that
point indicates the argv index of the non-option argument.  If an
invalid option is used, getopt will return a "?"; if opterr is non-zero,
getopt will also output an error message.
*/
{
  int         return_value;
  char        *optpos;
  static char *optchar = NULL;
			/* The character position containing the
			   next option letter to be examined, or NULL
			   if a new argument should be begun. */

  /* See if a new argument must be begun (i.e., there is not
     part of an existing option to finish). */
  if (optchar == NULL) {
start_new_argument:
    if (optind >= argc) {
      /* No more arguments. */
      return_value = EOF;
      goto end_of_routine;
    } else {
      optchar = argv[optind];
      if (*optchar != '-') {
        /* The argument string does not begin with a "-". */
        return_value = EOF;
        goto end_of_routine;
      } else if (*(optchar+1) == '-') {
        /* The argument is "--", which marks the end of the options.
           Swallow this argument. */
        optind++;
        return_value = EOF;
        goto end_of_routine;
      } else if (*(optchar+1) == '\0') {
        /* The argument is "-", which is used to indicate stdin as a
           file name.  Return without swallowing this argument. */
        return_value = EOF;
        goto end_of_routine;
      }  /* if */
      /* We have the start of a new option.  Advance past the "-". */
      optchar++;
    }  /* if */
  }  /* if */
  /* Here, optchar points to the next option character.  See if the
     current option letter list has been exhausted. */
  if (*optchar == '\0') {
    /* Start the next argument. */
    optind++;
    goto start_new_argument;
  } /* if */
  /* See if the option letter appears in the string of legal options. */
  optpos = strchr(optstring, *optchar);
  if (optpos == NULL) {
    /* Bad option letter. */
    if (opterr) fprintf(stderr, "%s: illegal option -- %c\n", argv[0],
                                *optchar);
    return_value = '?';
    goto end_of_routine;
  }  /* if */
  /* Valid option letter, return it. */
  return_value = *optchar;
  /* See if the option takes an argument. */
  if (*(optpos+1) == ':') {
    if (*(optchar+1) == '\0') {
      /* The option letter is the last thing in the argument, so use the
         next argument as the option value, as in "-I xxx". */
      optind++;
      if (optind >= argc) {
        /* There are no remaining arguments, so the option argument is
           missing. */
        if (opterr) fprintf(stderr, "%s: option requires an argument -- %c\n",
                                    argv[0], *optchar);
        return_value = '?';
        goto end_of_routine;
      }  /* if */
      optarg = argv[optind];
    } else {
      /* The option argument is the remainder of the current argument,
         as in "-Ixxx". */
      optarg = optchar+1;
    }  /* if */
    /* In either case, take no more characters of the current argument. */
    optchar = NULL;
    optind++;
  } else {
    /* The option does not take an argument. */
    optchar++;
    optarg = NULL;
  }  /* if */
end_of_routine:
  return(return_value);
}  /* getopt */
#endif /* else of __SYSV__ */


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


static long scan_optarg_number(char *optstr)
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
  return(result);
number_error:
  str_command_line_error("invalid number: ", optstr);
  /*NOTREACHED*/
}  /* scan_optarg_number */


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
  int       optchar;
  char      *ofile_name = NULL;
  a_boolean cannot_open, bad_name;
  char	    *instantiation_mode_string = NULL;

  /* Set a current position indicating we are looking at the command line. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_CMD_LINE;
  set_err_pos_to_curr_token();

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
      command_line_error("incorrect host CPU id");
    }  /* if */
  }
#endif /* ifdef HOSTID */

  /* Start with empty include file search paths.  Entries may be added
     because of command line options, and others will be added as defaults. */
  incl_search_path = end_incl_search_path = sys_incl_search_path = NULL;
  /* Suppress getopt's error on non-recognized option. */
  opterr = 0;
  /* Scan the command-line options. */
#define COMMAND_LIST "ABCEHKMNOPTabnsuvwxrmpjV$I:D:U:e:L:X:S:o:i:d:t:"
  while ((optchar = getopt(argc, argv, COMMAND_LIST)) != EOF) {
    switch (optchar) {
      case 'A':
      case 'a':
        /* Warn on non-ANSI features, disable features that conflict
           with ANSI.  Note that "ANSI" means ANSI C or ANSI C++, depending
           on the C_dialect setting.  'A' issues errors for violations,
	   'a' issues warnings. */
        strict_ansi_mode = TRUE;
        strict_ansi_error_severity = (optchar == 'A') ? es_error : es_warning;
        break;
      case 'E':
        /* Do preprocessing only, output to stdout, with #line information. */
        do_preprocessing_only = TRUE;
        generate_pp_output = TRUE;
        gen_line_info_in_pp_output = TRUE;
        break;
      case 'P':
        /* Do preprocessing only, output to stdout (driver remaps to .i file),
           without #line information. */
        do_preprocessing_only = TRUE;
        generate_pp_output = TRUE;
        gen_line_info_in_pp_output = FALSE;
        break;
      case 'C':
        /* Keep comments in preprocessing output. */
        keep_comments_in_pp_output = TRUE;
        break;
      case 'K':
        /* Compile K&R/pcc dialect of C. */
        C_dialect = C_dialect_pcc;
        break;
      case 'M':
        /* Generate makefile dependency lines for #include files encountered,
           but do not compile. */
        do_preprocessing_only = TRUE;
        generate_pp_output = FALSE;
        list_included_files = FALSE;
        list_makefile_dependencies = TRUE;
        error_threshold = es_error;
        break;
      case 'H':
        /* Generate on stdout a list of the names of the #include files
           processed, but do not compile. */
        do_preprocessing_only = TRUE;
        generate_pp_output = FALSE;
        list_included_files = TRUE;
        list_makefile_dependencies = FALSE;
        error_threshold = es_error;
        break;
      case 'N':
#if DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE
	/* Suppress IL lowering and write an unlowered IL file. */
	suppress_il_lowering = TRUE;
        suppress_back_end = TRUE;
        /* Note that suppress_il_file_write is not set. */
	break;
#else /* !(DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE) */
	optarg = "-N";
        goto unknown_option;
#define DID_GOTO_UNKNOWN_OPTION
#endif /* DO_IL_LOWERING && IL_SHOULD_BE_WRITTEN_TO_FILE */
      case 'O':
        /* Allow anachronisms. Toggle the value (use the non-default value)
           of the flag that specifies whether anachronisms should be
           accepted. */
        allow_anachronisms = !DEFAULT_ALLOW_ANACHRONISMS;
        break;
      case 'b':
        /* cfront compatibility mode. */
        cfront_compatibility_mode = TRUE;
        /* This option implies C++ dialect. */
        C_dialect = C_dialect_cplusplus;
        /* It also implies that exception support is disabled. */
        if (exceptions_enabled != DEFAULT_EXCEPTIONS_ENABLED) {
          /* The -x option has already been seen.  An error will be issued
             later if it has caused exceptions to be enabled. */
        } else {
          exceptions_enabled = FALSE;
        }  /* if */
        break;
      case 'n':
        /* Run just the front end to do syntax checking; do not run the back
           end. */
        suppress_back_end = TRUE;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
        suppress_il_file_write = TRUE;
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#if DO_IL_LOWERING
	suppress_il_lowering = TRUE;
#endif /* DO_IL_LOWERING */
        break;
      case 's':
        /* Use signed chars. */
        targ_has_signed_chars = TRUE;
        break;
      case 't':
        /* Template instantiation mode. */
        instantiation_mode_string = optarg;
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
            str_command_line_error("invalid instantiation mode: ",
      			           instantiation_mode_string);
          }  /* if */
        }  /* if */
        break;
      case 'T':
#if AUTOMATIC_TEMPLATE_INSTANTIATION
        /* Enable or disable automatic instantiation processing. */
        automatic_instantiation_mode = !DEFAULT_AUTOMATIC_INSTANTIATION_MODE;
        break;
#else /* !AUTOMATIC_TEMPLATE_INSTANTIATION */
	optarg = "-T";
        goto unknown_option;
#define DID_GOTO_UNKNOWN_OPTION
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
      case 'B':
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
        /* Enable or disable implicit inclusion of template definition source
           files. */
        implicit_template_inclusion_mode =
				 !DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE;
        break;
#else /* !INSTANTIATION_BY_IMPLICIT_INCLUSION */
	optarg = "-B";
        goto unknown_option;
#define DID_GOTO_UNKNOWN_OPTION
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
      case 'u':
        /* Use unsigned chars. */
        targ_has_signed_chars = FALSE;
        break;
      case 'V':
        /* Suppress generation of a virtual function table if unable to
	   determine absolute means to avoid duplicate virtual function 
	   table entries in separate compilations. */
	suppress_virtual_function_table_definition = TRUE;
	break;
      case '$':
        /* Toggle the value (use the non-default value) of the flag that
           determines whether dollar signs are accepted in identifiers. */
        allow_dollar_in_id_chars = !DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS; 
        break;
      case 'v':
        /* Print out compiler version. */
        fprintf(stderr, "Edison Design Group C/C++ Front End, version %s\n",
                         VERSION_NUMBER);
        fprintf(stderr, "Copyright 1988-1993 Edison Design Group Inc.\n");
#ifdef DEMO_VERSION_ID
        fprintf(stderr, "Demonstration version for %s\n", DEMO_VERSION_ID);
#endif /* ifdef DEMO_VERSION_ID */
        fputc('\n', stderr);
        break;
      case 'w':
        /* Suppress warnings. */
        error_threshold = es_error;
        break;
      case 'r':
        /* Enable remarks. */
        error_threshold = es_remark;
        break;
      case 'm':
        /* Compile ANSI C. */
        C_dialect = C_dialect_ANSI;
        break;
      case 'p':
        /* Compile C++. */
        C_dialect = C_dialect_cplusplus;
        break;
      case 'x':
        /* Toggle the value (use the non-default value) of the flag that
           determines whether support for exceptions is disabled. */
        exceptions_enabled = !DEFAULT_EXCEPTIONS_ENABLED;
        break;
      case 'j':
        /* Suppress used-before-set warnings. */
        suppress_used_before_set_warnings = TRUE;
        break;
      case 'I':
        /* Include file directory, add to list. */
        if (*optarg == '-') {
          /* Directory name was probably omitted; next option was taken
             as the directory name. */
          command_line_error("missing include file directory name");
        }  /* if */
        add_to_include_search_path(optarg);
        break;
      case 'D':
        /* Define a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(optarg, &defs_from_cmd_line);
        break;
      case 'U':
        /* Undefine a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(optarg, &undefs_from_cmd_line);
        break;
      case 'e':
        /* Set error limit (numbers of errors at which to give up on
           compilation). */
        error_limit = scan_optarg_number(optarg);
        if (error_limit <= 0) {
          str_command_line_error("invalid error limit: ", optarg);
        }  /* if */
        break;
      case 'L':
        /* Generate a file of raw listing information (source lines,
           file/line information, and indications of which lines are which,
           to be read later by a program that will generate an
           interspersed listing). */
        f_raw_listing = open_output_file(optarg, /*binary_file=*/FALSE,
                                         /*update_mode=*/FALSE,
                                         &cannot_open, &bad_name);
        if (bad_name) {
          str_command_line_error("invalid raw-listing output file ",
                                 optarg);
        } else if (cannot_open) {
          str_command_line_error("cannot open raw-listing output file ",
                                 optarg);
        }  /* if */
        break;
      case 'X':
        /* Generate a file of cross-reference information (locations and
	   kinds of references to symbols) */
        f_xref_info = open_output_file(optarg, /*binary_file=*/FALSE,
                                       /*update_mode=*/FALSE,
                                       &cannot_open, &bad_name);
        if (bad_name) {
          str_command_line_error("invalid cross-reference output file ",
                                 optarg);
        } else if (cannot_open) {
          str_command_line_error("cannot open cross-reference output file ",
                                 optarg);
        }  /* if */
        break;
      case 'S':
        /* Redirect stderr to a file.  This is useful on systems where
           redirection is not well supported. */
        reopen_error_output_file(optarg, &cannot_open, &bad_name);
        if (bad_name) {
          str_command_line_error("invalid error output file ",
                                 optarg);
        } else if (cannot_open) {
          str_command_line_error("cannot open error output file ",
                                 optarg);
        }  /* if */
        break;
      case 'o':
        /* Specify output file for preprocessing output or IL. */
        ofile_name = optarg;
        break;
      case 'i':
#if BACK_END_IS_C_GEN_BE
        /* Save a string of comma-separated module names that will be linked
           with this one.  This is used by c_gen_be to generate calls
           to file-scope initialization routines that handle union
           initialization.  This option is only needed if c_gen_be is being
           used to generate C output for testing. */
        module_list_for_union_init = optarg;
        break;
#else /* !BACK_END_IS_C_GEN_BE */
	optarg = "-i";
        goto unknown_option;
#define DID_GOTO_UNKNOWN_OPTION
#endif /* BACK_END_IS_C_GEN_BE */
      case 'd':
#if DEBUG
        /* Set debug level. */
        if (proc_debug_option(optarg)) {
	  command_line_error("error in debug option argument");
	}  /* if */
        init_debug_level = debug_level;
        break;
#else /* !DEBUG */
	optarg = "-d";
        goto unknown_option;
#define DID_GOTO_UNKNOWN_OPTION
#endif /* DEBUG */
      default:
        /* Get the option out in the case of an option requiring
           an argument where the argument is missing. */
        if (optind >= argc) optind = argc-1;
        optarg = argv[optind];
#ifdef DID_GOTO_UNKNOWN_OPTION
unknown_option:
#endif /* ifdef DID_GOTO_UNKNOWN_OPTION */
        str_command_line_error("invalid option: ", optarg);
    }  /* switch */
  }  /* while */
  /* Check for the use of C++ options when the dialect being compiled
     is not C++. */
  if (C_dialect != C_dialect_cplusplus) {
    if (allow_anachronisms != DEFAULT_ALLOW_ANACHRONISMS) {
      command_line_error
        ("anachronism option (-O) can be used only when compiling C++");
    }  /* if */
    if (suppress_virtual_function_table_definition) {
      command_line_error(
      "virtual function tables can only be suppressed (-V) when compiling C++"
                         );
    }  /* if */
    if (instantiation_mode_string != NULL) {
      command_line_error(
      "instantiation mode (-t) can be used only when compiling C++");
    }  /* if */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (automatic_instantiation_mode != DEFAULT_AUTOMATIC_INSTANTIATION_MODE) {
      command_line_error(
      "automatic instantiation mode (-T) can be used only when compiling C++");
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (implicit_template_inclusion_mode !=
                                    DEFAULT_IMPLICIT_TEMPLATE_INCLUSION_MODE) {
      command_line_error(
  "implicit template inclusion mode (-B) can be used only when compiling C++");
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    if (exceptions_enabled != DEFAULT_EXCEPTIONS_ENABLED) {
      if (!exceptions_enabled) {
        command_line_error(
        "support for exceptions can be disabled (-x) only when compiling C++");
      } else {
        command_line_error(
         "support for exceptions can be enabled (-x) only when compiling C++");
      }  /* if */
    }  /* if */
  }  /* if */
  if (strict_ansi_mode) {
    /* Strict ANSI mode is incompatible with K&R/pcc mode. */
    if (C_dialect == C_dialect_pcc) {
      command_line_error("strict ANSI mode is incompatible with K&R mode");
    }  /* if */
    /* Strict ANSI mode is incompatible with cfront compatibility mode. */
    if (cfront_compatibility_mode) {
      command_line_error("strict ANSI mode is incompatible with cfront mode");
    }  /* if */
    /* Strict ANSI mode is incompatible with allowing anachronisms.  Don't
       give an error if allow anachronisms is the default -- quietly
       set the flag to not allow anachronisms. */
    if (allow_anachronisms) {
#if DEFAULT_ALLOW_ANACHRONISMS
      allow_anachronisms = FALSE;
#else /* DEFAULT_ALLOW_ANACHRONISMS */
      command_line_error
        ("strict ANSI mode is incompatible with allowing anachronisms");
#endif /* DEFAULT_ALLOW_ANACHRONISMS */
    }  /* if */
    /* Make sure that strict ANSI messages come out even if the
       error threshold was set at a higher level. */
    if ((int)error_threshold > (int)strict_ansi_error_severity) {
      error_threshold = strict_ansi_error_severity;
    }  /* if */
  }  /* if */
  /* Determine the appropriate error level for anachronism messages based
     on whether anachronisms are to be allowed. */
  anachronism_error_severity = allow_anachronisms ? es_warning : es_error;
  /* Choose the style of preprocessing. */
  pcc_preprocessing_mode = (C_dialect == C_dialect_pcc);
  if (cfront_compatibility_mode) {
    /* In cfront compatibility mode support for exceptions should be
       disabled. */
    if (exceptions_enabled) {
      /* Exception support must have been enabled by a command line option. */
      command_line_error(
               "support for exceptions cannot be enabled (-x) in cfront mode");
    }  /* if */
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
    /* When configured that way, use old-style preprocessing for cfront
       compatibility mode. */
    pcc_preprocessing_mode = TRUE;
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
  }  /* if */

  /* Add the default directories to the end of the include search path.
     The list is then any -I directories, in the order they were specified,
     and the default directories at the end. */
  add_default_include_search_path();
  /* Set the system include search path to be the same as the user search
     path at this point (the directory of the source file will be added to the
     front of the user search path in a moment, making the two lists
     different). */
  sys_incl_search_path = incl_search_path;

  /* Pick up the source file name. */
  if (optind >= argc) {
    command_line_error("missing source file name");
  }  /* if */
  optarg = argv[optind++];
  /* If the name is "-", use stdin for input. */
  if (strcmp(optarg, "-") == 0) optarg = FILE_NAME_FOR_STDIN;
  primary_source_file_name = optarg;
  /* Add the directory of the source file to the front of the include file
     search path.  gs_directory_of returns the directory part of the
     name allocated in general (not IL) storage. */
  /* If you change this, see the similar code in get_next_source_file. */
  add_to_front_of_include_search_path(
                                    gs_directory_of(primary_source_file_name));

#if COMPILE_MULTIPLE_SOURCE_FILES
  /* Multiple source files can be compiled.  Save the count and argv
     position of remaining files, if any. */
  argc_file_list = argc - optind;
  more_than_one_source_file = (argc_file_list > 0);
  if (more_than_one_source_file) {
    argv_file_list = &argv[optind];
    /* There are at least two files.  The -o, -L, and -X options (those
       that specify output files) cannot be used, because they only specify
       one file. */
    if (ofile_name != NULL || f_raw_listing != NULL || f_xref_info != NULL) {
      command_line_error(
       "output files may not be specified when compiling several input files");
    }  /* if */
  }  /* if */
#else /* !COMPILE_MULTIPLE_SOURCE_FILES */
  /* Multiple source files cannot be compiled. */
  /* Check that all command-line arguments were taken. */
  if (optind < argc) {
    command_line_error("too many arguments on command line");
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
    command_line_error("-o was specified, but no output file is needed");
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
    /* Update the first entry of the include file search list, the one
       that contains the directory of the primary source file. */
    /* If you change this, see the similar code in proc_command_line. */
    change_primary_include_search_dir(
                                    gs_directory_of(primary_source_file_name));
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
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
