/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2002 Edison Design Group Inc.                   [_]          *
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
/* c_gen_be.h is needed only for module_list_for_union_init, and that's
   used only when generating K&R C. */
#if !C_GEN_BE_GENERATES_ANSI_C
#include "c_gen_be.h"
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
#endif /* BACK_END_IS_C_GEN_BE */
#if USER_CONTROL_OF_STRUCT_PACKING
#include "layout.h"
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

#ifdef HOSTID
extern long gethostid(void);
#endif /* HOSTID */


static a_def_undef_string_ptr
		last_defs_from_cmd_line = NULL;
			/* Points to the last element in the list
			   defs_from_cmd_line. */

static a_def_undef_string_ptr
		last_undefs_from_cmd_line = NULL;
			/* Points to the last element in the list
			   undefs_from_cmd_line. */

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

#define SIZE_OF_OPTION_DESCRIPTIONS ((int)optk_last*2)
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

static a_boolean
		old_style_preprocessing = FALSE;
			/* TRUE if old-style preprocessing should be
			   used in ANSI C or C++ mode. */


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
      if ((keyword != NULL && odp->keyword != NULL &&
           strcmp(keyword, odp->keyword) == 0) ||
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
    fprintf(f_debug, "Too many options descriptions.  Current limit is %d\n",
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
  add_option_description(optk_ii_file_name,
                         "ii_file", '\0',
                         /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_suppress_instantiation_flags,
                         "suppress_instantiation_flags", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_template_info_file,
                         "template_info_file",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_definition_list_file_name,
                         "definition_list_file",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_exported_template_file_name,
                         "exported_template_file",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_template_directory, "template_directory", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
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
#endif /* BACK_END_IS_C_GEN_BE */
#if DEBUG
  add_option_description(optk_debug, "db", 'd',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_debug_name, "db_name", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
  add_option_description(optk_debug_alloc_seq, "db_alloc_seq", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#if !EDG_WIN32
  /* This option is only available on Unix. */
  add_option_description(optk_time_limit, "time_limit", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* !EDG_WIN32 */
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
  add_option_description(optk_msvc_target_version, "msvc_target_version",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
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
  add_option_description(optk_pch_verbose, "pch_verbose",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_pch_verbose, "no_pch_verbose",
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
  add_option_description(optk_restrict, "restrict",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_restrict, "no_restrict",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
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
  add_option_description(optk_microsoft_version, "microsoft_version",
                         '\0', /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_microsoft_bugs, "microsoft_bugs",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_microsoft_bugs, "no_microsoft_bugs",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if NEAR_AND_FAR_ALLOWED
  add_option_description(optk_microsoft_16_mode, "microsoft_16",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  add_option_description(optk_far_data_pointers, "far_data_pointers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_far_data_pointers, "near_data_pointers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_far_code_pointers, "far_code_pointers",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_far_code_pointers, "near_code_pointers",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* NEAR_AND_FAR_ALLOWED */
#if WCHAR_T_ENABLING_POSSIBLE
  add_option_description(optk_wchar_t_is_keyword, "wchar_t_keyword",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* WCHAR_T_ENABLING_POSSIBLE */
  add_option_description(optk_wchar_t_is_keyword, "no_wchar_t_keyword",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if USER_CONTROL_OF_STRUCT_PACKING
  /* Note -- the Microsoft-style "-Zpn" option is not supported.  The driver
     that invokes the front end may convert it to "--pack_alignment=n". */
  add_option_description(optk_pack_alignment, "pack_alignment",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  add_option_description(optk_alternative_tokens,
			 "alternative_tokens",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_alternative_tokens,
			 "no_alternative_tokens",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MINIMAL_INLINING
  add_option_description(optk_inlining, "inlining",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_inlining, "no_inlining",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MINIMAL_INLINING */
  add_option_description(optk_SVR4_C_mode,
			 "svr4",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_SVR4_C_mode,
			 "no_svr4",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_brief_diagnostics,
			 "brief_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_brief_diagnostics,
			 "no_brief_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_nonconst_ref_anachronism,
                         "nonconst_ref_anachronism",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonconst_ref_anachronism,
                         "no_nonconst_ref_anachronism",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_no_preproc_only,
                         "no_preproc_only",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if RTTI_ENABLING_POSSIBLE
  add_option_description(optk_rtti, "rtti", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* RTTI_ENABLING_POSSIBLE */
  add_option_description(optk_rtti, "no_rtti", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_building_runtime, "building_runtime", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if BOOL_ENABLING_POSSIBLE
  add_option_description(optk_bool_is_keyword, "bool",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* BOOL_ENABLING_POSSIBLE */
  add_option_description(optk_bool_is_keyword, "no_bool",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE
  add_option_description(optk_array_new_and_delete,
                         "array_new_and_delete", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
  add_option_description(optk_array_new_and_delete,
                         "no_array_new_and_delete", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_explicit,
                         "explicit", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_explicit,
                         "no_explicit", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_namespaces,
                         "namespaces", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_namespaces,
                         "no_namespaces", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_using_std,
                         "using_std", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_using_std,
                         "no_using_std", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MAINTAIN_NEEDED_FLAGS
  add_option_description(optk_remove_unneeded_entities,
                         "remove_unneeded_entities", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MAINTAIN_NEEDED_FLAGS */
  add_option_description(optk_remove_unneeded_entities,
                         "no_remove_unneeded_entities", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_typename,
                         "typename", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_typename,
                         "no_typename", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_typename,
                         "implicit_typename", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_typename,
                         "no_implicit_typename", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_special_subscript_cost,
                         "special_subscript_cost", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_special_subscript_cost,
                         "no_special_subscript_cost", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_style_preprocessing,
                         "old_style_preprocessing", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_for_init,
                         "old_for_init", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_for_init,
                         "new_for_init", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_for_init_diff_warning,
                         "for_init_diff_warning", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_for_init_diff_warning,
                         "no_for_init_diff_warning", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_distinct_template_signatures,
                         "distinct_template_signatures", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_distinct_template_signatures,
                         "no_distinct_template_signatures", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_guiding_decls,
                         "guiding_decls", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_guiding_decls,
                         "no_guiding_decls", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_specializations,
                         "old_specializations", '\0',
                         /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_old_specializations,
                         "no_old_specializations", '\0',
                         /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_wrap_diagnostics,
			 "wrap_diagnostics",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_none);
  add_option_description(optk_wrap_diagnostics,
			 "no_wrap_diagnostics",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_none);
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
  add_option_description(optk_implicit_extern_c_type_conversion,
			 "implicit_extern_c_type_conversion",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_implicit_extern_c_type_conversion,
			 "no_implicit_extern_c_type_conversion",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
  add_option_description(optk_long_preserving_rules,
                         "long_preserving_rules",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_long_preserving_rules,
                         "no_long_preserving_rules",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extern_inline,
			 "extern_inline",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extern_inline,
			 "no_extern_inline",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
  add_option_description(optk_multibyte_chars,
                         "multibyte_chars",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_multibyte_chars,
                         "no_multibyte_chars",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
  add_option_description(optk_embedded_cplusplus,
                         "embedded_c++",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if VLA_ALLOWED
  add_option_description(optk_vla,
			 "vla",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_vla,
			 "no_vla",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* VLA_ALLOWED */
  add_option_description(optk_enum_overloading,
                         "enum_overloading",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_enum_overloading,
                         "no_enum_overloading",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_qualifier_deduction,
                         "nonstd_qualifier_deduction",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_qualifier_deduction,
                         "no_nonstd_qualifier_deduction",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if ONE_INSTANTIATION_PER_OBJECT
  add_option_description(optk_one_instantiation_per_object,
                         "one_instantiation_per_object",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_instantiation_dir,
                         "instantiation_dir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  add_option_description(optk_late_tiebreaker,
                         "late_tiebreaker",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_late_tiebreaker,
                         "early_tiebreaker",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_preinclude, "preinclude", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_preinclude_macros, "preinclude_macros", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_pending_instantiations,
                         "pending_instantiations",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
#if MICROSOFT_EXTENSIONS_ALLOWED
  add_option_description(optk_import_dir,
                         "import_dir",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  add_option_description(optk_const_string_literals,
                         "const_string_literals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_const_string_literals,
                         "no_const_string_literals",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_class_name_injection,
                         "class_name_injection",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_class_name_injection,
                         "no_class_name_injection",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_arg_dependent_lookup,
                         "arg_dep_lookup",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_arg_dependent_lookup,
                         "no_arg_dep_lookup",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_friend_injection,
                         "friend_injection",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_friend_injection,
                         "no_friend_injection",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_using_decl,
                         "nonstd_using_decl",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_nonstandard_using_decl,
                         "no_nonstd_using_decl",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
  add_option_description(optk_designators,
                         "designators",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_designators,
                         "no_designators",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_designators,
                         "extended_designators",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_designators,
                         "no_extended_designators",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
  add_option_description(optk_variadic_macros,
                         "variadic_macros",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_variadic_macros,
                         "no_variadic_macros",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_variadic_macros,
                         "extended_variadic_macros",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_extended_variadic_macros,
                         "no_extended_variadic_macros",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_system_include_dir, "sys_include", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_include_file_suffixes, "incl_suffixes", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
  add_option_description(optk_compound_literals,
                         "compound_literals",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_compound_literals,
                         "no_compound_literals",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
  add_option_description(optk_base_assign_op_is_default,
                         "base_assign_op_is_default",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_base_assign_op_is_default,
                         "no_base_assign_op_is_default",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_sun_mode,
                         "sun",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_sun_mode,
                         "no_sun",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_dependent_name_processing,
                         "dep_name",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_dependent_name_processing,
                         "no_dep_name",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_ignore_namespace_std,
                         "ignore_std",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_parse_nonclass_templates,
                         "parse_templates",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_parse_nonclass_templates,
                         "no_parse_templates",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if C99_IL_EXTENSIONS_SUPPORTED
  /* The C99 IL extensions are required to provide full C99 support.
     The C99 command-line options are only enabled when the front end
     is configured to provide full support. */
  add_option_description(optk_c99_mode,
                         "c99",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_c99_mode,
                         "no_c99",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  add_option_description(optk_export_template,
                         "export",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_export_template,
                         "no_export",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#if DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE
  /* When passing stdarg references to the back end by default, give the
     ability to turn off this feature.  When not passing such references,
     we do not give the ability to turn the feature on. */
  add_option_description(optk_stdarg_builtin,
                         "no_stdarg_builtin",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE */
#if ENABLE_TRANS_UNIT_TEST_MODE
  add_option_description(optk_trans_unit_test_mode,
                         "trans_unit_test",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* ENABLE_TRANS_UNIT_TEST_MODE */
#if GNU_EXTENSIONS_ALLOWED
  add_option_description(optk_gcc_mode,
                         "gcc",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gcc_mode,
                         "no_gcc",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gpp_mode,
                         "g++",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_gpp_mode,
                         "no_g++",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_short_enums,
                         "short_enums",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
#endif /* GNU_EXTENSIONS_ALLOWED */
  add_option_description(optk_long_long,
                         "long_long",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_context_limit, "context_limit", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_none);
  add_option_description(optk_set_flag, "set_flag", '\0',
                         /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
  add_option_description(optk_set_flag, "clear_flag", '\0',
                         /*value=*/FALSE, /*arg_required=*/TRUE,
                         pchek_command_line);
#if UPC_EXTENSIONS_ALLOWED
  add_option_description(optk_upc_mode,
                         "upc",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_mode,
                         "no_upc",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_strict_access,
                         "upc_strict",
                         '\0', /*value=*/TRUE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_strict_access,
                         "upc_relaxed",
                         '\0', /*value=*/FALSE, /*arg_required=*/FALSE,
                         pchek_command_line);
  add_option_description(optk_upc_threads,
                         "upc_threads",
                         '\0', /*value=*/TRUE, /*arg_required=*/TRUE,
                         pchek_command_line);
#endif /* UPC_EXTENSIONS_ALLOWED */
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
  a_boolean			ambiguous = FALSE;
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
  /* If no match was found for a keyword option, look again to see if
     the option is an abbreviation. */
  if (!match && is_keyword_option) {
    an_option_description_ptr	odp_found = NULL;
    for (n = 0; n < option_descriptions_used; ++n) {
      odp = &option_descriptions[n];
      if (odp->keyword != NULL &&
          strncmp(optchar, odp->keyword, size_t_arg(keyword_length)) == 0) {
        if (match) ambiguous = TRUE;
        odp_found = odp;
        match = TRUE;
      }  /* if */
    }  /* for */
    if (ambiguous) {
      /* If the command line option was ambiguous, issue an error and list
         the possible options. */
      start_command_line_error(ec_cl_ambiguous_option, optchar);
      for (n = 0; n < option_descriptions_used; ++n) {
        odp = &option_descriptions[n];
        if (odp->keyword != NULL &&
            strncmp(optchar, odp->keyword, size_t_arg(keyword_length)) == 0) {
          str_add_diag_info(ec_cl_ambiguous_fill_in, odp->keyword);
        }  /* if */
      }  /* for */
      end_command_line_error();
    }  /* if */
    odp = odp_found;
  }  /* if */
  if (!match || ambiguous) odp = NULL;
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

	--option_b=xxx	-- keyword option with an argument (note that no
			   spaces are allowed on either side of the
			   equals sign).
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


void add_to_def_undef_list(char                   *str,
                           a_def_undef_string_ptr *du_list,
                           a_def_undef_string_ptr *du_list_end)
/*
Add the string pointed to by str (which comes from a command-line -D
or -U macro define/undefine option) to the list of def/undef strings
pointed to by *du_list.  The end of the list is pointed to by
*du_list_end.  The new entry is added to the end of the list.
*/
{
  a_def_undef_string_ptr du_new;

  /* alloc_general is called (rather than alloc_fe) because the general
     mem_manage.c routines are not yet initialized, and because the
     strings may be used several times if several files are compiled. */
  du_new = (a_def_undef_string_ptr)alloc_general(sizeof(a_def_undef_string));
  du_new->next = NULL;
  du_new->text = str;
  /* Add the new entry to the end of the list of defs or undefs. */
  if (*du_list_end != NULL) (*du_list_end)->next = du_new;
  *du_list_end = du_new;
  if (*du_list == NULL) *du_list = du_new;
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
					 char		*arg)
/*
Go through a comma separated list of error tags and call an error
processing routine to update the severity.
*/
{
  char			*local_arg;
  int			number_of_arguments = 0;
  int			i;
  an_error_severity	severity;
  char			*ptr;

  /* Make a local copy of the option string.  Remove any blanks and replace
     commas with null characters.  Note that this copy is simply discarded
     after it is used. */
  local_arg = (char *)alloc_general((sizeof_t)(strlen(arg) + 1));
  {
    char	*src = arg;
    char	*dest = local_arg;
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
  ptr = local_arg;
  for (i = 0; i < number_of_arguments; ++i) {
    char	*opt_start = ptr;
    char	*opt_end = strchr(ptr, '\0');
    a_boolean	err;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Setting error severity for: %s\n", opt_start);
    }  /* if */
#endif /* DEBUG */
    if (isdigit((unsigned char)*opt_start)) {
      int error_number = (int)scan_opt_arg_number(opt_start);
      err = set_severity_for_error_number(error_number, severity);
      if (err) {
        str_command_line_error(ec_cl_invalid_error_number, opt_start);
      }  /* if */
    } else {
      err = set_severity_for_error_tag(opt_start, severity);
      if (err) {
        str_command_line_error(ec_cl_invalid_error_tag, opt_start);
      }  /* if */
    }  /* if */
    ptr = opt_end + 1;
  }  /* for */
}  /* process_diag_override_option */


static void process_preinclude_option(an_option_kind	kind,
				      char		*arg)
/*
Process a preinclude or preinclude_macros option (determined by
"kind").  "arg" is the file name.
*/
{
  if (preinclude_file_name != NULL) {
    /* A value has already been specified. */
    command_line_error(ec_cl_more_than_one_preinclude);
  }  /* if */
  preinclude_file_name = arg;
  is_macro_preinclude = (kind == optk_preinclude_macros);
}  /* process_preinclude_option */


/*
Structure used for an array of flag names that can be set using a
command-line option.
*/
typedef struct a_flag_name *a_flag_name_ptr;
typedef struct a_flag_name {
  char		*name;
			/* Name used to set the flag. */
  a_boolean	*variable;
			/* Pointer to the variable to be set. */
} a_flag_name;


/*
Array of flag names that may be set on the command-line.
*/
static a_flag_name
		flag_names[] = {
  { "suppress_inline_corresp_check", &suppress_inline_corresp_check },
  { "allow_anon_types_in_anon_unions", &allow_anon_types_in_anon_unions },
  { NULL, NULL }  /* must be last */
};


static void set_flag_value(char		*flag_name,
			   a_boolean	value)
/*
Set or clear the flag specified by "flag_name".  "value" is the value to
be given to the flag.
*/
{
  a_flag_name_ptr	fnp;
  a_boolean		*flag_var = NULL;

  for (fnp = flag_names; fnp->name != NULL; fnp++) {
    if (strcmp(fnp->name, flag_name) == 0) {
      flag_var = fnp->variable;
    }  /* if */
  }  /* for */
  if (flag_var == NULL) {
    str_command_line_error(ec_cl_invalid_flag_name, flag_name);
  }  /* if */
  *flag_var = value;
}  /* set_flag_value */


#if MICROSOFT_EXTENSIONS_ALLOWED
static void set_microsoft_mode_flags(void)
/*
Set other options whose values should be changed when Microsoft mode
is enabled.  Only set the option values if they were not already set
by a command line option.
*/
{
  enum_types_can_be_smaller_than_int = FALSE;
  enum_types_can_be_larger_than_int = FALSE;
  stack_referenced_include_directories = TRUE;
  defer_friend_instantiation = TRUE;
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  allow_nonstandard_anonymous_unions = TRUE;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  if (!option_kind_used[(int)optk_allow_dollar_in_id_chars]) {
    allow_dollar_in_id_chars = TRUE;
  }  /* if */
  /* '//' is accepted as a comment delimiter in both C and C++. */
  end_of_line_comments_allowed = TRUE;
  IEEE_handling_on_float_operation_exceptions = FALSE;
  floating_point_template_parameters_allowed = TRUE;
  null_chars_allowed_in_source = TRUE;
  if (!C_mode()) {
    /* Microsoft C++ mode. */
    if (!option_kind_used[(int)optk_bool_is_keyword]) {
      /* The bool keyword is supported by Microsoft Visual C++ 5.0. */
      bool_is_keyword = microsoft_version >= 1100;
    }  /* if */
    if (!option_kind_used[(int)optk_wchar_t_is_keyword]) {
      wchar_t_is_keyword = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_explicit]) {
      /* The explicit keyword is supported by Microsoft Visual C++ 5.0. */
      explicit_keyword_enabled = microsoft_version >= 1100;
    }  /* if */
#if !RUNTIME_USES_TYPENAME
    if (!option_kind_used[(int)optk_typename]) {
      /* The typename keyword is supported by Microsoft Visual C++ 5.0. */
      typename_enabled = microsoft_version >= 1100;
    }  /* if */
#endif /* !RUNTIME_USES_TYPENAME */
    if (!option_kind_used[(int)optk_implicit_typename]) {
      implicit_typename_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_guiding_decls]) {
      guiding_decls_allowed = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_old_specializations]) {
      old_specializations_allowed = TRUE;
    }  /* if */
    c_and_cpp_function_types_are_distinct = FALSE;
    if (!option_kind_used[(int)optk_extern_inline]) {
      extern_inline_allowed = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_base_assign_op_is_default]) {
      allow_copy_assignment_op_with_base_class_param = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_old_for_init]) {
      /* As of MSVC++ 6.0, this feature is still not implemented. */
      use_nonstandard_for_init_scope = TRUE;
    }  /* if */
    ptr_to_unknown_bound_array_allowed_in_param_type = TRUE;
    /* Exception specifications should be ignored in Microsoft bugs mode. */
    ignore_exception_specifications = microsoft_bugs;
    if (!option_kind_used[(int)optk_enum_overloading]) {
      /* Enum overloading is supported by Microsoft Visual C++ 4.x. */
      operator_overloading_on_enums_enabled = microsoft_version >= 1000;
    }  /* if */
    if (!option_kind_used[(int)optk_const_string_literals]) {
      string_literals_are_const = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_class_name_injection]) {
      class_name_injection_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_arg_dependent_lookup]) {
      arg_dependent_lookup_enabled = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_friend_injection]) {
      friend_injection_enabled = TRUE;
    }  /* if */
    if (!option_kind_used[(int)optk_dependent_name_processing]) {
      do_dependent_name_processing = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_parse_nonclass_templates]) {
      nonclass_prototype_instantiations = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_nonstandard_using_decl]) {
      nonstandard_using_decl_allowed = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_export_template]) {
      export_template_allowed = FALSE;
      export_keyword_enabled = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_late_tiebreaker]) {
      do_late_ovl_res_tiebreaker = microsoft_bugs;
    }  /* if */
    single_ref_qual_ovl_res_tiebreaker = (microsoft_bugs &&
                                          microsoft_version < 1300);
    allow_nonconst_ref_anachronism = TRUE;
    allow_nonconst_call_anachronism = (microsoft_version < 1000);
    flexible_array_members_allowed = TRUE;
    /* Make template parameters visible in specialization scopes. */
    use_microsoft_specialization_scope = TRUE;
  }  /* if */
}  /* set_microsoft_mode_flags */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void set_cfront_mode_flags(void)
/*
Set other options whose values should be changed when cfront mode
is enabled.  Only set the option values if they were not already set
by a command line option.
*/
{
  check_assertion(any_cfront_mode());
  if (cfront_2_1_mode) {
    /* cfront 2.1 compatibility mode. */
    if (!(option_kind_used[(int)optk_special_subscript_cost])) {
      special_subscript_cost = FALSE;
    }  /* if */
    allow_nonconst_call_anachronism = TRUE;
  } else {
    /* cfront 3.0 compatibility mode. */
    if (!(option_kind_used[(int)optk_special_subscript_cost])) {
      special_subscript_cost = TRUE;
    }  /* if */
    allow_nonconst_call_anachronism = FALSE;
  }  /* if */
  /* Processing common to both cfront modes. */
  /* Set flags to the appropriate mode unless they have been explicitly
     set by other command line options. */
  if (!(option_kind_used[(int)optk_cplusplus_anachronisms])) {
    allow_anachronisms = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_nonconst_ref_anachronism])) {
    allow_nonconst_ref_anachronism = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_long_lifetime_temps])) {
    long_lifetime_temps = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_bool_is_keyword])) {
    bool_is_keyword = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_explicit])) {
    explicit_keyword_enabled = FALSE;
  }  /* if */
#if !RUNTIME_USES_NAMESPACES
  if (!(option_kind_used[(int)optk_arg_dependent_lookup])) {
    arg_dependent_lookup_enabled = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_namespaces])) {
    namespaces_enabled = FALSE;
  }  /* if */
#endif /* !RUNTIME_USES_NAMESPACES */
#if !RUNTIME_USES_TYPENAME
  if (!(option_kind_used[(int)optk_typename])) {
    typename_enabled = FALSE;
  }  /* if */
#endif /* !RUNTIME_USES_TYPENAME */
  if (!(option_kind_used[(int)optk_implicit_typename])) {
    implicit_typename_enabled = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_old_for_init])) {
    use_nonstandard_for_init_scope = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_base_assign_op_is_default])) {
    allow_copy_assignment_op_with_base_class_param = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_guiding_decls])) {
    guiding_decls_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_old_specializations])) {
    old_specializations_allowed = TRUE;
  }  /* if */
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
  if (!(option_kind_used[(int)optk_implicit_extern_c_type_conversion])) {
    impl_conv_between_c_and_cpp_function_ptrs_allowed = TRUE;
  }  /* if */
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
  if (!(option_kind_used[(int)optk_extern_inline])) {
    extern_inline_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_enum_overloading])) {
    operator_overloading_on_enums_enabled = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_const_string_literals])) {
    string_literals_are_const = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_late_tiebreaker])) {
    do_late_ovl_res_tiebreaker = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_friend_injection])) {
    friend_injection_enabled = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_dependent_name_processing])) {
    do_dependent_name_processing = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_export_template]) {
    export_template_allowed = FALSE;
    export_keyword_enabled = FALSE;
  }  /* if */
  if (!option_kind_used[(int)optk_parse_nonclass_templates]) {
    nonclass_prototype_instantiations = FALSE;
  }  /* if */
  /* Set flags that cannot be overridden by command line options. */
  ptr_to_unknown_bound_array_allowed_in_param_type = TRUE;
}  /* set_cfront_mode_flags */


static void check_pch_file_name(char *file_name)
/*
Make sure the specified file name is acceptable as an output file.
If it is not acceptable, issue an error.
*/
{
  if (!okay_as_output_file(file_name)) {
    str_command_line_error(ec_cl_invalid_pch_output_file, file_name);
  }  /* if */
}  /* check_pch_file_name */


#if COMPILE_MULTIPLE_SOURCE_FILES || COMPILE_MULTIPLE_TRANSLATION_UNITS
static char	**argv_file_list;
static int	argc_file_list;
			/* When multiple source input files are accepted,
			   argc_file_list is the count of files remaining
			   after the current one, and argv_file_list
			   points to the argv entry for the first
			   remaining file. */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */


static void set_c99_mode_flags(void)
/*
Set the various flags appropriate to C99 mode.
*/
{
#if VLA_ALLOWED
  if (!(option_kind_used[(int)optk_vla])) {
    /* Support for VLAs is turned on by default in C99 mode. */
    vla_enabled = TRUE;
  }  /* if */
#endif /* VLA_ALLOWED */
  if (!(option_kind_used[(int)optk_restrict])) {
    /* Support for restricted pointers is turned on by default in C99 mode. */
    restrict_enabled = TRUE;
  }  /* if */
#if DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
  if (!(option_kind_used[(int)optk_designators])) {
    /* Support for designators is turned on by default in C99 mode. */
    designators_allowed = TRUE;
  }  /* if */
#endif /* DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
  if (!(option_kind_used[(int)optk_compound_literals])) {
    /* Support for compound literals is turned on by default in C99 mode. */
    compound_literals_allowed = TRUE;
  }  /* if */
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
  if (!(option_kind_used[(int)optk_variadic_macros])) {
    /* Support for variadic macros is turned on by default in C99 mode. */
    variadic_macros_allowed = TRUE;
  }  /* if */
  /* Support for alternative tokens is turned on by default in C99 mode. */
  alternative_tokens_allowed = TRUE;
  /* In C99 mode, strict or otherwise, // comments are allowed. */
  end_of_line_comments_allowed = TRUE;
  /* The final field of a struct may be an incomplete array. */
  flexible_array_members_allowed = TRUE;
  /* Universal character names are allowed. */
  universal_character_names_allowed = TRUE;
  /* The va_copy macro should be recognized. */
  va_copy_macro_allowed = TRUE;
  /* The long long data type is not an extension in C99. */
  long_long_is_standard = TRUE;
  long_long_promotion_allowed = TRUE;
  /* Hexadecimal floating point constants are permitted. */
  hex_floating_point_constants_allowed = TRUE;
}  /* set_c99_mode_flags */


static void set_c_mode_flags(void)
/*
Set the various flags appropriate for the specific C mode we are going to
process.
*/
{
  /* Turn off language features that must not be on in C mode, in case
     the default value is on. */
  exceptions_enabled = FALSE;
  rtti_enabled = FALSE;
  array_new_and_delete_enabled = FALSE;
  explicit_keyword_enabled = FALSE;
  namespaces_enabled = FALSE;
  wchar_t_is_keyword = FALSE;
  bool_is_keyword = FALSE;
  /* Set global flags having to do with the potential sizes of enum types.
     They must be no larger than int in C. */
  enum_types_can_be_larger_than_int = FALSE;
  if (C_dialect == C_dialect_pcc || SVR4_C_mode) {
    enum_types_can_be_smaller_than_int = FALSE;
  } else {
    enum_types_can_be_smaller_than_int =
                            targ_enum_types_can_be_smaller_than_int;
  }  /* if */
  if (C_dialect == C_dialect_pcc) {
    /* Alternative tokens are not recognized in PCC mode. */
    alternative_tokens_allowed = FALSE;
  }  /* if */
  special_subscript_cost = FALSE;  /* Not really needed. */
  use_nonstandard_for_init_scope = TRUE;  /* Not really needed. */
  nonstandard_qualifier_deduction = FALSE;  /* Not really needed. */
  warning_on_for_init_difference = FALSE;
  remove_qualifiers_from_param_types = FALSE;
  impl_conv_between_c_and_cpp_function_ptrs_allowed = FALSE;
  extern_inline_allowed = FALSE;
  operator_overloading_on_enums_enabled = FALSE;  /* Not really needed. */
  string_literals_are_const = FALSE;
  arg_dependent_lookup_enabled = FALSE;
  instantiate_extern_inline = FALSE;
  do_dependent_name_processing = FALSE;
  export_template_allowed = FALSE;
  export_keyword_enabled = FALSE;
  va_list_in_std_namespace = FALSE;
  /* The final field of a struct may be an incomplete array. */
  flexible_array_members_allowed = TRUE;
  /* Set the variable that controls whether "//" is allowed as a comment
     delimiter. */
  if (c99_mode || microsoft_mode) {
    /* The variable is set elsewhere. */
  } else if (strict_ansi_mode) {
    /* Strict ANSI/ISO C (not C99): // comments are not allowed. */
    end_of_line_comments_allowed = FALSE;
  } else if (C_dialect == C_dialect_pcc) {
    /* pcc mode: // comments are not allowed. */
    end_of_line_comments_allowed = FALSE;
  } else {
    /* Normal C mode. */
    end_of_line_comments_allowed = END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE;
  }  /* if */
  if (SVR4_C_mode) {
    /* Turn on features implied by SVR4 C mode. */
    address_of_ellipsis_allowed = TRUE;
    allow_ellipsis_only_param_in_C_mode = TRUE;
  } else if (c99_mode) {
    /* Turn on features implied by C99 mode. */
    set_c99_mode_flags();
  } /* if */
}  /* set_c_mode_flags */


static void check_and_set_c_mode_options(void)
/*
This routine is called in C mode to check that no C++-only command-line
setting is used, and to set various unmentioned settings as needed.
*/
{
  check_assertion(C_mode());
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
  if (option_kind_used[(int)optk_rtti]) {
    command_line_error(ec_cl_rtti_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_array_new_and_delete]) {
    command_line_error(ec_cl_array_new_and_delete_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_explicit]) {
    command_line_error(ec_cl_explicit_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_namespaces]) {
    command_line_error(ec_cl_namespaces_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_wchar_t_is_keyword]) {
    command_line_error(ec_cl_wchar_t_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_bool_is_keyword]) {
    command_line_error(ec_cl_bool_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_special_subscript_cost]) {
    command_line_error(
                      ec_cl_special_subscript_cost_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_typename]) {
    command_line_error(ec_cl_typename_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_implicit_typename]) {
    command_line_error(ec_cl_implicit_typename_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_old_for_init]) {
    command_line_error(ec_cl_old_for_init_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_for_init_diff_warning]) {
    command_line_error(ec_cl_for_init_diff_warning_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_guiding_decls]) {
    command_line_error(ec_cl_guiding_decls_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_old_specializations]) {
    command_line_error(ec_cl_old_specializations_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_implicit_extern_c_type_conversion]) {
    command_line_error(ec_cl_impl_extern_c_conv_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_extern_inline]) {
    command_line_error(ec_cl_extern_inline_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_embedded_cplusplus]) {
    command_line_error(ec_cl_embedded_cplusplus_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_enum_overloading]) {
    command_line_error(ec_cl_enum_overloading_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_nonstandard_qualifier_deduction]) {
    command_line_error(
            ec_cl_nonstandard_qualifier_deduction_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_nonstandard_using_decl]) {
    command_line_error(
            ec_cl_nonstd_using_decl_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_class_name_injection]) {
    command_line_error(ec_cl_class_name_injection_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_arg_dependent_lookup]) {
    command_line_error(ec_cl_arg_dependent_lookup_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_friend_injection]) {
    command_line_error(ec_cl_friend_injection_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_dependent_name_processing]) {
    command_line_error(ec_cl_dep_name_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_parse_nonclass_templates]) {
    command_line_error(
                     ec_cl_parse_nonclass_templates_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_export_template]) {
    command_line_error(ec_cl_export_template_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_ignore_namespace_std]) {
    command_line_error(ec_cl_ignore_std_option_only_in_cplusplus);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (option_kind_used[(int)optk_one_instantiation_per_object]) {
    command_line_error(
            ec_cl_one_instantiation_per_object_option_only_in_cplusplus);
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (option_kind_used[(int)optk_late_tiebreaker]) {
    command_line_error(ec_cl_late_tiebreaker_option_only_in_cplusplus);
  }  /* if */
  if (option_kind_used[(int)optk_pending_instantiations]) {
    command_line_error(
                     ec_cl_pending_instantiations_option_only_in_cplusplus);
  }  /* if */
  set_c_mode_flags();
}  /* check_and_set_c_mode_options */


static void check_and_set_cplusplus_mode_options(void)
/*
This routine is called in C++ mode to check that no non-C++ command-line
setting is used, and to set various unmentioned settings as needed.
*/
{
  check_assertion(!C_mode());
  /* Reset the SVR4 C compatibility flag just in case it is set by
     default. */
  SVR4_C_mode = FALSE;
  /* Likewise for C99 mode. */
  c99_mode = FALSE;
  /* Set global flags having to do with potential size of enum types. */
  enum_types_can_be_smaller_than_int =
                          targ_enum_types_can_be_smaller_than_int;
  enum_types_can_be_larger_than_int = TRUE;
  /* The default for --long_preserving_rules in C++ is FALSE. */
  if (!option_kind_used[(int)optk_long_preserving_rules]) {
    long_preserving_rules = FALSE;
  }  /* if */
#if VLA_ALLOWED
  if (option_kind_used[(int)optk_vla]) {
    command_line_error(ec_cl_vla_option_only_in_C);
  }  /* if */
#endif /* VLA_ALLOWED */
  vla_enabled = FALSE;
  if (option_kind_used[(int)optk_designators]) {
    command_line_error(ec_cl_designators_option_only_in_C);
  }  /* if */
  designators_allowed = FALSE;
  if (option_kind_used[(int)optk_extended_designators]) {
    command_line_error(ec_cl_extended_designators_option_only_in_C);
  }  /* if */
  extended_designators_allowed = FALSE;
  if (option_kind_used[(int)optk_compound_literals]) {
    command_line_error(ec_cl_compound_literals_option_only_in_C);
  }  /* if */
  compound_literals_allowed = FALSE;
  /* "//" is allowed as a comment delimiter. */
  end_of_line_comments_allowed = TRUE;
  /* Universal character names are allowed. */
  universal_character_names_allowed = TRUE;
}  /* check_and_set_cplusplus_mode_options */


static void exclude_cfront_mode(an_error_code  error_code)
/*
Cfront mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off an
otherwise implicitly enabled cfront mode.
*/
{
  if (any_cfront_mode()) {
    if (option_kind_used[(int)optk_cfront_2_1_mode] ||
        option_kind_used[(int)optk_cfront_3_0_mode]) {
      /* cfront mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* cfront mode enabled by default.  Silently disable it. */
      cfront_2_1_mode = FALSE;
      cfront_3_0_mode = FALSE;
    }  /* if */
  }  /* if */
}  /* exclude_cfront_mode */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void exclude_microsoft_mode(an_error_code  error_code)
/*
Microsoft mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off an
otherwise implicitly enabled Microsoft mode.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Microsoft mode is not compatible with another mode set on the command
     line. */
  if (microsoft_mode) {
    if (option_kind_used[(int)optk_microsoft_mode] ||
        option_kind_used[(int)optk_microsoft_version] ||
        option_kind_used[(int)optk_microsoft_16_mode] ||
        option_kind_used[(int)optk_microsoft_bugs]) {
      /* Microsoft mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* Microsoft mode enabled by default.  Silently disable it since the
         explicit mode setting on the command line overrides it. */
      microsoft_mode = FALSE;
      microsoft_bugs = FALSE;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* exclude_microsoft_mode */


static void exclude_sun_mode(an_error_code  error_code)
/*
Sun mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled Sun mode.
*/
{
  if (sun_mode) {
    if (option_kind_used[(int)optk_sun_mode]) {
      /* Sun mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* Sun mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      sun_mode = FALSE;
    }  /* if */
  }  /* if */
}  /* exclude_sun_mode */


static void exclude_SVR4_C_mode(an_error_code  error_code)
/*
SVR4-C mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled SVR4-C mode.
*/
{
  if (SVR4_C_mode) {
    if (option_kind_used[(int)optk_SVR4_C_mode]) {
      /* SVR4-C mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* SVR4-C mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      SVR4_C_mode = FALSE;
    }  /* if */
  }  /* if */
}  /* exclude_SVR4_C_mode */


static void exclude_c99_mode(an_error_code  error_code)
/*
C99 mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled C99 mode.
*/
{
  if (c99_mode) {
    if (option_kind_used[(int)optk_c99_mode]) {
      /* C99 mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* C99 mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      c99_mode = FALSE;
    }  /* if */
  }  /* if */
}  /* exclude_c99_mode */

#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/
#endif /* !GNU_EXTENSIONS_ALLOWED */
static void exclude_gcc_mode(an_error_code  error_code)
/*
GNU C mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled GNU C mode.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (gcc_mode) {
    if (option_kind_used[(int)optk_gcc_mode]) {
      /* GNU C mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* GNU C mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      gcc_mode = FALSE;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* exclude_gcc_mode */


#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/
#endif /* !GNU_EXTENSIONS_ALLOWED */
static void exclude_gpp_mode(an_error_code  error_code)
/*
GNU C++ mode is incompatible with other settings.  Either issue the given
diagnostic (error_code) if the conflict is explicit, or silently turn off
an otherwise implicitly enabled GNU C++ mode.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (gpp_mode) {
    if (option_kind_used[(int)optk_gpp_mode]) {
      /* GNU C++ mode was enabled by a command line option. */
      command_line_error(error_code);
    } else {
      /* GNU C++ mode was enabled by default.  Silently disable it since an
         explicit mode setting on the command line overrides it. */
      gpp_mode = FALSE;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* exclude_gpp_mode */


static void check_and_set_ansi_mode_options(void)
/*
Both for strict ANSI C and C++ modes, check that no command-line setting
conflicts with the ANSI mode and set various unmentioned settings as needed.
*/
{
#if NEAR_AND_FAR_ALLOWED
  /* If near and far were enabled by default, turn off support. */
  il_header.near_and_far_are_enabled = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Strict ANSI mode is incompatible with allowing anachronisms. */
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
  if (allow_nonconst_ref_anachronism) {
    if (option_kind_used[(int)optk_nonconst_ref_anachronism]) {
      /* The nonconst ref anachronism was enabled by a command line
         option. */
      command_line_error(ec_cl_strict_ansi_incompatible_with_anachronisms);
    } else {
      /* The nonconst ref anachronism was enabled by default.
         Silently disable it in strict mode. */
      allow_nonconst_ref_anachronism = FALSE;
    }  /* if */
  }  /* if */
  if (long_preserving_rules) {
    if (option_kind_used[(int)optk_long_preserving_rules]) {
      command_line_error(
                        ec_cl_strict_ansi_incompatible_with_long_preserving);
    } else {
      /* Long preserving rules enabled by default.  Silently disable them. */
	long_preserving_rules = FALSE;
    }  /* if */
  }  /* if */
  if (!(option_kind_used[(int)optk_extended_designators])) {
    /* Support for extended designators is turned off by default in
       strict mode. */
    extended_designators_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_extended_variadic_macros])) {
    /* Support for extended variadic macros is turned off by default
       in strict mode. */
    extended_variadic_macros_allowed = FALSE;
  }  /* if */
  if (!c99_mode) {
    /* In strict mode the final field of a struct may not be an incomplete
       array, except in strict C99 mode. */
    flexible_array_members_allowed = FALSE;
    if (!(option_kind_used[(int)optk_variadic_macros])) {
      /* Support for variadic macros is turned off by default except in
         strict C99 mode. */
      variadic_macros_allowed = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_restrict])) {
      /* Support for restricted pointers is turned off by default except
         in strict C99 mode. */
      restrict_enabled = FALSE;
    }  /* if */
  }  /* if */
  if (C_mode()) {
    /* Set optional features to standard settings for strict C mode. */
    /* Enable recognition of digraphs. */
    alternative_tokens_allowed = TRUE;
    /* Features enabled in C99 but not in older C are handled in
       set_c99_mode_flags. */
    if (!c99_mode) {
      /* Features listed here are those that can be turned on in pre-C99 C
         mode but not in C++ mode. */
#if VLA_ALLOWED
      if (!(option_kind_used[(int)optk_vla])) {
        /* Support for VLAs is turned off by default in strict C mode. */
        vla_enabled = FALSE;
      }  /* if */
#endif /* VLA_ALLOWED */
      if (!(option_kind_used[(int)optk_designators])) {
        /* Support for designators is turned off by default in strict C
           mode. */
        designators_allowed = FALSE;
      }  /* if */
      if (!(option_kind_used[(int)optk_compound_literals])) {
        /* Support for compound literals is turned off by default in strict
           C mode. */
        compound_literals_allowed = FALSE;
      }  /* if */
    }  /* if */
  } else {
    /* Set optional features to standard settings for strict C++ mode. */
    ptr_to_unknown_bound_array_allowed_in_param_type = FALSE;
    single_ref_qual_ovl_res_tiebreaker = FALSE;
    floating_point_template_parameters_allowed = FALSE;
    if (!(option_kind_used[(int)optk_alternative_tokens])) {
      /* If alternative_tokens was not explicitly set by a command line
         option, set it now. */
      alternative_tokens_allowed = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_wchar_t_is_keyword])) {
      /* If wchar_t_is_keyword was not explicitly set by a command line
         option, set it now. */
      wchar_t_is_keyword = WCHAR_T_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_bool_is_keyword])) {
      /* If bool_is_keyword was not explicitly set by a command line
         option, set it now. */
      bool_is_keyword = BOOL_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_long_lifetime_temps])) {
      /* Temporary lifetime is short. */
      long_lifetime_temps = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_rtti])) {
      /* If rtti_enabled was not explicitly set by a command line
         option, set it now. */
      rtti_enabled = RTTI_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_array_new_and_delete])) {
      /* If array_new_and_delete_enabled was not explicitly set by a
         command line option, set it now. */
      array_new_and_delete_enabled = ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE;
    }  /* if */
    if (!(option_kind_used[(int)optk_explicit])) {
      /* If explicit_keyword_enabled was not explicitly set by a command
         line option, set it now. */
      explicit_keyword_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_namespaces])) {
      /* If namespaces_enabled was not explicitly set by a command line
         option, set it now. */
      namespaces_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_implicit_typename])) {
      /* If implicit_typename was not explicitly set by a command line
         option, set it now. */
      implicit_typename_enabled = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_typename])) {
      /* If typename_enabled was not explicitly set by a command line
         option, set it now. */
      typename_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_special_subscript_cost])) {
      /* If special_subscript_cost was not explicitly set by a command line
         option, turn it off now. */
      special_subscript_cost = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_old_for_init])) {
      /* If old/new_for_init was not specified on the command line, turn
         off use_nonstandard_for_init_scope now. */
      use_nonstandard_for_init_scope = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_for_init_diff_warning])) {
      /* If for_init_diff_warning was not specified on the command line, turn
         off warning_on_for_init_difference now. */
      warning_on_for_init_difference = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_guiding_decls])) {
      /* If guiding_decls_allowed was not set on the command line, turn it
         off now. */
      guiding_decls_allowed = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_old_specializations])) {
      /* If old_specializations_allowed was not set on the command line,
         turn it off now. */
      old_specializations_allowed = FALSE;
    }  /* if */
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
    if (!(option_kind_used[(int)optk_implicit_extern_c_type_conversion])) {
      /* If impl_conv_between_c_and_cpp_function_ptrs_allowed was not set
         on the command line, turn it off now. */
      impl_conv_between_c_and_cpp_function_ptrs_allowed = FALSE;
    }  /* if */
#else /* !IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
    /* Set it FALSE in case the implicit conversion between extern "C" and
       extern "C++" function pointers is allowed by default. */
    impl_conv_between_c_and_cpp_function_ptrs_allowed = FALSE;
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
    if (!(option_kind_used[(int)optk_extern_inline])) {
      /* If extern_inline_allowed was not explicitly set by a command line
         option, set it now. */
      extern_inline_allowed = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_enum_overloading])) {
      /* If enum_overloading was not explicitly set by a command line
         option, set it now. */
      operator_overloading_on_enums_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_const_string_literals])) {
      /* If string_literals_are_const was not explicitly set by a
         command line option, set it now. */
      string_literals_are_const = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_class_name_injection])) {
      /* If class name injection was not explicitly set by a command
         line option, set it now. */
      class_name_injection_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_arg_dependent_lookup])) {
      /* If argument dependent lookup not explicitly set by a command
         line option, set it now. */
      arg_dependent_lookup_enabled = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_friend_injection])) {
      /* If friend injection was not explicitly set by a command line
         option, set it now. */
      friend_injection_enabled = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_dependent_name_processing])) {
      /* If dependent name processing was not explicitly set by a command line
         option, set it now. */
      do_dependent_name_processing = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_parse_nonclass_templates])) {
      /* If prototype instantiation of nonclasses was not explicitly set by a
         command line option, set it now. */
      nonclass_prototype_instantiations = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_export_template])) {
      /* If export template processing was not explicitly set by a command line
         option, set it now. */
      export_template_allowed = TRUE;
    }  /* if */
    if (!(option_kind_used[(int)optk_nonstandard_using_decl])) {
      /* If nonstandard using-decl was not explicitly set by a command line
         option, set it now. */
      nonstandard_using_decl_allowed = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_nonstandard_qualifier_deduction])) {
      /* If nonstandard_qualifier_deduction was not set on the command line,
         turn it off now. */
      nonstandard_qualifier_deduction = FALSE;
    }  /* if */
    if (!(option_kind_used[(int)optk_late_tiebreaker])) {
      /* If late tiebreaker was not explicitly set by a command line
         option, force it off. */
      do_late_ovl_res_tiebreaker = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_base_assign_op_is_default]) {
      allow_copy_assignment_op_with_base_class_param = FALSE;
    }  /* if */
    if (!option_kind_used[(int)optk_exception_handling]) {
      exceptions_enabled = TRUE;
    }  /* if */
  }  /* if */
  /* Make sure that strict ANSI messages come out even if the
     error threshold was set at a higher level. */
  if ((int)error_threshold > (int)strict_ansi_error_severity) {
    error_threshold = strict_ansi_error_severity;
  }  /* if */
}  /* check_and_set_ansi_mode_options */


static void check_and_set_sun_mode_options(void)
/*
Set the option needed to emulate the peculiarities of the Sun CC 5.0 compiler,
and check that no other modes conflict with this one.  (The processing of
some modes, like ANSI, exclude the Sun mode already.  Hence those are not
checked again here.)
*/
{
  if (!(option_kind_used[(int)optk_guiding_decls])) {
    /* If guiding_decls_allowed was not set on the command line, turn it
       off now. */
    guiding_decls_allowed = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_nonstandard_using_decl])) {
    /* If nonstandard using-decl was not explicitly set by a command line
       option, set it now. */
    nonstandard_using_decl_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_extern_inline])) {
    /* If extern_inline_allowed was not explicitly set by a command line
       option, turn it off now. */
    extern_inline_allowed = FALSE;
  }  /* if */
  /* The Sun compiler suffers from the same problem as the Microsoft
     compiler with respect to making template parameters visible in
     specializations. */
  use_microsoft_specialization_scope = TRUE;
}  /* check_and_set_sun_mode_options */


static void check_and_set_gnu_mode_options(void)
/*
Set the options common to both GNU C and C++ modes, making sure that no other
options conflict with them.  (The processing of some modes, like ANSI,
exclude the GNU modes already.  Hence those are not checked again here.)
*/
{
#if DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
  if (!(option_kind_used[(int)optk_extended_designators])) {
    /* If extended designators were not enabled or disabled on the command
       line, enable them now. */
    designators_allowed = TRUE;
    extended_designators_allowed = TRUE;
  }  /* if */
#endif /* DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
#if COMPOUND_LITERAL_ENABLING_POSSIBLE
  if (!(option_kind_used[(int)optk_compound_literals])) {
    /* If compound literals were not enabled or disabled on the command line,
       enable them now. */
    compound_literals_allowed = TRUE;
  }  /* if */
#endif /* COMPOUND_LITERAL_ENABLING_POSSIBLE */
  if (!(option_kind_used[(int)optk_extended_variadic_macros])) {
    /* If extended variadic macros were not enabled or disabled on the command
       line, enable them now. */
    variadic_macros_allowed = TRUE;
    extended_variadic_macros_allowed = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_allow_dollar_in_id_chars])) {
    /* If identifiers with dollar signs were not enabled or disabled on the
       command line, enable them now. */
    allow_dollar_in_id_chars = TRUE;
  }  /* if */
  if (!(option_kind_used[(int)optk_stdarg_builtin])) {
    /* <stdarg.h> should be included as a normal header file.  Various
       __builtin_... entities may be predefined to accommodate it (if
       GCC_BUILTIN_VARARGS is TRUE). */
    pass_stdarg_references_to_generated_code = FALSE;
  }  /* if */
  if (!(option_kind_used[(int)optk_restrict])) {
    /* Enable the use of __restrict__ in GNU mode. */
    restrict_enabled = TRUE;
  }  /* if */
  /* Enable flexible array member support. */
  flexible_array_members_allowed = TRUE;
  /* Enable // comments. */
  end_of_line_comments_allowed = TRUE;
  /* Enable recognition of digraphs. */
  alternative_tokens_allowed = TRUE;
  /* Treat "long long" as a standard feature. */
  long_long_is_standard = TRUE;
  long_long_promotion_allowed = FALSE;
  /* Hexadecimal floating point constants are permitted. */
  hex_floating_point_constants_allowed = TRUE;
  null_chars_allowed_in_source = TRUE;
}  /* check_and_set_gnu_mode_options */


static void check_and_set_gcc_mode_options(void)
/*
Set the options needed to emulate GNU C compilers, and check that no other
modes conflict with this one.  (The processing of some modes, like ANSI,
exclude the GNU C mode already.  Hence those are not checked again here.)
*/
{
  check_and_set_gnu_mode_options();
#if VLA_ALLOWED
  if (!(option_kind_used[(int)optk_vla])) {
    /* Support for VLAs is turned on by default in gcc mode. */
    vla_enabled = TRUE;
  }  /* if */
#endif /* VLA_ALLOWED */
  /* The underlying type for an enum could be long long. */
  enum_types_can_be_larger_than_int = TRUE;
}  /* check_and_set_gcc_mode_options */


static void check_and_set_gpp_mode_options(void)
/*
Set the options needed to emulate GNU C++ compilers, and check that no other
modes conflict with this one.  (The processing of some modes, like ANSI,
exclude the GNU C++ mode already.  Hence those are not checked again here.)
*/
{
  check_and_set_gnu_mode_options();
  if (!option_kind_used[(int)optk_exception_handling]) {
    /* Enable exceptions by default in GNU C++ mode. */
    exceptions_enabled = TRUE;
  }  /* if */
  /* Enable recognition of digraphs. */
  alternative_tokens_allowed = TRUE;
  /* GNU C++ doesn't look unqualified names up in dependent base classes. */
  gpp_dependent_base_class_lookup = TRUE;
  /* We will presumably want to pick std::type_info from the GNU headers.
     In that case, we cannot expect an EDG-specific pragma. */
  pragma_defined_type_info_is_required = FALSE;
}  /* check_and_set_gpp_mode_options */


static void exclude_gnu_specific_options(void)
/*
No GNU mode was selected: Make sure no option specific to GNU mode
was selected either.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  if (il_header.short_enums) {
    command_line_error(ec_cl_short_enums_requires_gcc_mode);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
}  /* exclude_gnu_specific_options */


static void check_dialect_and_language_modes(void)
/*
Check for consistent specification of dialects and language modes.  Dialect
inconsistencies are allowed -- the last specified is operative -- but
language modes that are specified are required to be consistent with the
operative dialect.

Here is a summary of the dialects and modes, along with the associated
command line switches.

    dialect/mode         associated global variable       option
    ============         ==========================       ======
  C                                                      --c, -m
    pcc mode            C_dialect == C_dialect_pcc       --old_c, -K
    "ANSI" [= not pcc]  C_dialect == C_dialect_ANSI      (default)
      SVR4 mode         SVR4_C_mode                      --svr4
      microsoft mode    microsoft_mode                   --microsoft
        bugs mode       microsoft_bugs                   --microsoft_bugs
        16-bit mode     il_header.near_and_far_allowed   --microsoft_16
      C99               c99_mode                         --c99
        strict          strict_ansi_mode                 -A, -a, etc.
      "normal"            
        strict          strict_ansi_mode                 -A, -a, etc.
      GNU C             gcc_mode                         --gcc

  C++                   C_dialect == C_dialect_cplusplus --c++, -p
    cfront mode
      2.1 mode          cfront_2_1_mode                  --cfront_2.1
      3.0 mode          cfront_3_0_mode                  --cfront_3.0
    microsoft mode      microsoft_mode                   --microsoft
      bugs mode         microsoft_bugs                   --microsoft_bugs
      16-bit mode       il_header.near_and_far_allowed   --microsoft_16
    sun mode            sun_mode                         --sun
    GNU C++             gpp_mode                         --g++
    "normal"
      strict            strict_ansi_mode                 -A, -a, etc.

The major C dialect (K&R, ANSI, or C++) is determined by the last command line
option that selects a major dialect, either implicitly or explicitly. (For
example, --old_c, --c, and --c++ select a major dialect explicitly, and --svr4,
--cfront_3.0, and --c99 select a major dialect implicitly.)  No major dialect
is implicitly specified with --microsoft et al. or --strict et al.

C99 mode is in some ways considered both a dialect and a mode.  C_dialect
is still C_dialect_ANSI, but C99 is permitted to be used in conjunction with
Microsoft mode.

Whatever major dialect is selected, all language modes specified have to be
consistent with it.  For example, --old_c --c99 is permitted, since the
major dialect implied by --c99 overrides the major dialect specified by -K.
On the other hand, --c99 --old_c produces an error, since the final major
dialect is inconsistent with C99 mode.

Note that the fact that K&R C is its own major dialect, rather than
being a minor dialect under C mode, is a historical accident of the
order of development of this front end, and is inconsistent and strange.
*/
{
  if (C_dialect != C_dialect_ANSI) {
    /* Issue an error for specifying a language mode that is valid only
       when the dialect is ANSI C. */
    exclude_SVR4_C_mode(ec_cl_SVR4_C_option_only_in_ansi_C);
    exclude_c99_mode(ec_cl_incompatible_language_modes);
    exclude_gcc_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (C_dialect != C_dialect_cplusplus) {
    /* Issue an error for specifying a language mode that is valid only
       when the dialect is C++. */
    exclude_cfront_mode(ec_cl_incompatible_language_modes);
    exclude_sun_mode(ec_cl_sun_mode_only_in_cplusplus);
    exclude_gpp_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (C_dialect == C_dialect_pcc) {
    /* Issue an error for specifying a language mode that is valid only
       in ANSI C or C++ modes. */
    exclude_microsoft_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (strict_ansi_mode) {
    /* Strict ANSI mode is incompatible with K&R/pcc mode. */
    if (C_dialect == C_dialect_pcc) {
      command_line_error(ec_cl_strict_ansi_incompatible_with_pcc);
    }  /* if */
    /* Strict ANSI mode is incompatible with cfront compatibility mode. */
    exclude_cfront_mode(ec_cl_strict_ansi_incompatible_with_cfront);
    exclude_microsoft_mode(ec_cl_strict_ansi_incompatible_with_microsoft);
    exclude_sun_mode(ec_cl_strict_ansi_incompatible_with_sun);
    exclude_SVR4_C_mode(ec_cl_strict_ansi_incompatible_with_SVR4);
    exclude_gcc_mode(ec_cl_incompatible_language_modes);
    exclude_gpp_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (any_cfront_mode()) {
    /* Issue an error for specifying any other language mode.  Strict mode
       has already been checked for. */
    check_assertion(C_dialect == C_dialect_cplusplus);
    exclude_sun_mode(ec_cl_sun_incompatible_with_cfront);
    exclude_microsoft_mode(ec_cl_cfront_incompatible_with_microsoft);
  }  /* if */
  if (sun_mode) {
    /* Issue an error for specifying any other language mode.  Strict mode
       has already been checked for. */
    exclude_microsoft_mode(ec_cl_cfront_incompatible_with_microsoft);
    exclude_gcc_mode(ec_cl_incompatible_language_modes);
    exclude_gpp_mode(ec_cl_incompatible_language_modes);
  }  /* if */
  if (microsoft_mode) {
    /* Issue an error for specifying any other language mode.  Strict mode,
       K&R mode, Sun mode, and cfront mode have already been checked for. */
    exclude_SVR4_C_mode(ec_cl_incompatible_language_modes);
    exclude_gcc_mode(ec_cl_incompatible_language_modes);
    exclude_gpp_mode(ec_cl_incompatible_language_modes);
  }  /* if */
}  /* check_dialect_and_language_modes */

#if UPC_EXTENSIONS_ALLOWED

static void check_upc_mode(void)
/*
Check that UPC mode is enabled only in ANSI C or C99 mode; i.e., not K&R C
(which has no type qualifiers) and not C++.
*/
{
  check_assertion(upc_mode);
  if (C_dialect != C_dialect_ANSI) {
    if (option_kind_used[(int)optk_upc_mode]) {
      /* UPC mode was explicitly enabled: Issue an error. */
      command_line_error(ec_cl_upc_requires_ansi_c_dialect);
    }  /* if */
    /* If UPC mode is the default (and not mentioned on the command line),
       this will silently turn it off. */
    upc_mode = FALSE;
  }  /* if */
}  /* check_upc_mode */

#endif /* UPC_EXTENSIONS_ALLOWED */

#if COMPILE_MULTIPLE_TRANSLATION_UNITS

static void check_for_duplicated_file_names(void)
/*
Make sure a given file name was not specified more than once.
*/
{
  int	arg1;
  int	arg2;
  char	**file_list;

  /* argv_file_list already points to the second file.  Get a pointer
     to the array of file names, starting with the first one. */
  file_list = argv_file_list - 1;
  for (arg1 = 0; arg1 < argc_file_list; arg1++) {
    for (arg2 = arg1 + 1; arg2 <= argc_file_list; arg2++) {
      if (compare_file_names(file_list[arg1],
                             file_list[arg2]) == 0) {
        str_command_line_error(ec_cl_duplicate_file_name,
                               file_list[arg1]);
      }  /* if */
    }  /* for */
  }  /* for */
}  /* check_for_duplicated_file_names */

#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */

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
  a_boolean                     suppress_do_preprocessing_only = FALSE;

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
  /* Put the current directory on the template search path. */
  add_to_template_search_path(current_directory_name);
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
        case optk_pch_verbose:
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
        break;
      case optk_list_include_files:
        /* Generate on stdout a list of the names of the #include files
           processed. */
        check_assertion(opt_value == TRUE);
        list_included_files = TRUE;
        list_makefile_dependencies = FALSE;
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
        break;
      case optk_cfront_3_0_mode:
        check_assertion(opt_value == TRUE);
        /* cfront 3.0 compatibility mode.  If both 2.1 and 3.0 modes are
           selected, only the most recent applies. */
        cfront_3_0_mode = TRUE;
        cfront_2_1_mode = FALSE;
        /* This option implies C++ dialect. */
        C_dialect = C_dialect_cplusplus;
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
      case optk_ii_file_name:
        /* The name of the instantiation information file to be used. */
        ii_file_name = opt_arg;
        break;
      case optk_suppress_instantiation_flags:
        /* Enable or disable automatic instantiation processing. */
        suppress_instantiation_flags = opt_value;
        break;
      case optk_template_info_file:
        template_info_file_name = opt_arg;
        break;
      case optk_definition_list_file_name:
        definition_list_file_name = opt_arg;
        break;
      case optk_exported_template_file_name:
        exported_template_file_name = opt_arg;
        break;
      case optk_template_directory:
        /* A directory name to be added to the template search path.*/
        add_to_template_search_path(opt_arg);
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
        fprintf(stderr, "Copyright 1988-2002 Edison Design Group, Inc.\n");
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
      case optk_system_include_dir:
        /* Include file directory, add to list. */
        if (strcmp(opt_arg, "-") == 0) {
          /* -I- marks the dividing line between directories for "..."
             includes and those for <...> includes.  It also suppresses
             pushing the directory of each source file onto the search
             path, which is useful for viewpathing. */
          include_path_boundary = end_incl_search_path;
          put_dir_of_each_opened_source_file_on_incl_search_path = FALSE;
        } else {
          /* Normal -I directive. */
          add_to_include_search_path(opt_arg, kind == optk_system_include_dir);
        }  /* if */
        break;
      case optk_preinclude:
      case optk_preinclude_macros:
        /* File to include at the beginning of compilation. */
        process_preinclude_option(kind, opt_arg);
        break;
      case optk_define_macro:
        /* Define a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(opt_arg, &defs_from_cmd_line,
                              &last_defs_from_cmd_line);
        break;
      case optk_undefine_macro:
        /* Undefine a macro symbol.  Just save the string for later
           processing. */
        add_to_def_undef_list(opt_arg, &undefs_from_cmd_line,
                              &last_undefs_from_cmd_line);
        break;
      case optk_set_error_limit:
        /* Set error limit (numbers of errors at which to give up on
           compilation). */
        error_limit = scan_opt_arg_number(opt_arg);
        if (error_limit == 0) {
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
#if !C_GEN_BE_GENERATES_ANSI_C
        /* Save a string of comma-separated module names that will be linked
           with this one.  This is used by c_gen_be to generate calls
           to file-scope initialization routines that handle union
           initialization.  This option is only needed if c_gen_be is being
           used to generate C output for testing, and then only if K&R
           C is being generated. */
        module_list_for_union_init = opt_arg;
#endif /* !C_GEN_BE_GENERATES_ANSI_C */
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
      case optk_debug_name:
        /* Set debug name (name to be traced). */
        if (proc_debug_name_option(opt_arg)) {
	  command_line_error(ec_cl_error_in_debug_option_argument);
	}  /* if */
        break;
#if MAINTAIN_ALLOCATION_SEQUENCE_NUMBER
      case optk_debug_alloc_seq:
        /* Set debug allocation sequence number to be traced. */
        if (proc_debug_alloc_seq_option(opt_arg)) {
          command_line_error(ec_cl_error_in_debug_option_argument);
        }  /* if */
        break;
#endif /* MAINTAIN_ALLOCATION_SEQUENCE_NUMBER */
#if !EDG_WIN32
      case optk_time_limit:
        /* Debugging option to limit the amount of CPU time used
           during a compilation. */
        { int time_limit;
          time_limit = scan_opt_arg_number(opt_arg);
          set_cpu_time_limit(time_limit);
        }
        break;
#endif /* !EDG_WIN32 */
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
      case optk_msvc_target_version:
        /* The Microsoft C/C++ compiler being targeted. */
        msvc_target_version = scan_opt_arg_number(opt_arg);
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
        /* Make sure the specified name is acceptable as a PCH file name. */
        check_pch_file_name(opt_arg);
        break;
      case optk_use_pch:
        /* Use a precompiled header file as part of this compilation. */
        check_assertion(opt_value == TRUE);
        use_precompiled_header = TRUE;
        pch_input_file_name = opt_arg;
        precompiled_header_processing_required = TRUE;
        automatic_pch_processing = FALSE;
        create_precompiled_header = FALSE;
        /* Make sure the specified name is acceptable as a PCH file name. */
        check_pch_file_name(opt_arg);
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
      case optk_pch_verbose:
        /* Enable or suppress verbose PCH messages. */
        verbose_pch_messages = opt_value;
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
        if (!is_directory(pch_dir_name)) {
          str_command_line_error(ec_cl_invalid_pch_directory, pch_dir_name);
        }  /* if */
        break;
      case optk_restrict:
        /* Enables or disables recognition of the restrict token. */
        restrict_enabled = opt_value;
        break;
      case optk_long_lifetime_temps:
        /* Long or short lifetime temporaries. */
        long_lifetime_temps = opt_value;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_microsoft_bugs:
        /* Enable or disable the emulation of Microsoft bugs. */
        microsoft_bugs = opt_value;
        /* Now enable Microsoft mode. */
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_version:
        /* The version of the Microsoft compiler being emulated. */
        microsoft_version = scan_opt_arg_number(opt_arg);
        if (microsoft_version < 700 || microsoft_version > 2000) {
          str_command_line_error(ec_cl_invalid_microsoft_version, opt_arg);
        }  /* if */
        opt_value = TRUE;
        goto enable_microsoft_mode;
      case optk_microsoft_mode:
        /* Enable or disable Microsoft extensions, in 32-bit mode.
           Note that 16/32 bit mode is not reset when enable_microsoft_mode
           is branched to by other options. */
#if NEAR_AND_FAR_ALLOWED
        il_header.near_and_far_are_enabled = FALSE;
#endif /* NEAR_AND_FAR_ALLOWED */
enable_microsoft_mode:
        microsoft_mode = opt_value;
        break;
#if NEAR_AND_FAR_ALLOWED
      case optk_microsoft_16_mode:
        /* Enable or disable Microsoft extensions, in 16-bit mode. */
        check_assertion(opt_value == TRUE);
        microsoft_mode = TRUE;
        il_header.near_and_far_are_enabled = TRUE;
        break;
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
      case optk_far_data_pointers:
        /* Set size (either near or far) of data pointers when near/far
           support is enabled. */
        il_header.far_data_pointers = opt_value;
        break;
      case optk_far_code_pointers:
        /* Set size (either near or far) of code pointers when near/far
           support is enabled. */
        il_header.far_code_pointers = opt_value;
        break;
#endif /* NEAR_AND_FAR_ALLOWED */
      case optk_wchar_t_is_keyword:
        /* wchar_t is or is not a keyword. */
        wchar_t_is_keyword = opt_value;
        break;
#if USER_CONTROL_OF_STRUCT_PACKING
      case optk_pack_alignment:
        /* If a pack alignment value is given, it means the alignment of
           nonstatic data members may be smaller than what is dictated by
           the member's type.  In effect, the pack alignment value is the
           maximum alignment permitted for a nonstatic data member.  This
           default value may be overridden by #pragma pack. */
        if (!check_pack_alignment_value(
                         (a_host_large_integer)scan_opt_arg_number(opt_arg),
                         &default_max_member_alignment)) {
          /* Invalid value. */
          command_line_error(ec_bad_pack_alignment);
        }  /* if */
        break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case optk_alternative_tokens:
        /* Digraphs should or should not be allowed.  This also controls
           recognition of operator keywords (e.g., "not", "and") in C++. */
        alternative_tokens_allowed = opt_value;
        break;
#if MINIMAL_INLINING
      case optk_inlining:
        /* Minimal inlining should or should not be done. */
        inlining_enabled = opt_value;
        break;
#endif /* MINIMAL_INLINING */
      case optk_SVR4_C_mode:
        /* SVR4 C compatibility mode should or should not be used.  This
           option implies ANSI C mode, even in the "--no_svr4" form.
           In other words, --[no_]svr4 is short for --c --[no_]svr4.
           See --c99 and --sun for similar behavior. */
        SVR4_C_mode = opt_value;
        C_dialect = C_dialect_ANSI;
        break;
      case optk_brief_diagnostics:
        /* Diagnostics should or should not be emitted in a form that
	   omits the source information and suppresses wrapping of
	   the error message text. */
	brief_diagnostics = opt_value;
        break;
      case optk_wrap_diagnostics:
        /* Diagnostics should or should not be emitted in a form that
	   suppresses wrapping of the error message text. */
	do_not_wrap_diagnostics = !opt_value;
        break;
      case optk_nonconst_ref_anachronism:
        /* A reference to nonconst is allowed to bind to a class rvalue. */
        allow_nonconst_ref_anachronism = opt_value;
        break;
      case optk_no_preproc_only:
        /* Override the automatic setting of do_preprocessing_only.  This
	   can be used to force full compilation when it would not normally
	   be done. */
        suppress_do_preprocessing_only = opt_value;
        break;
      case optk_rtti:
        /* Enable/disable runtime type information (RTTI). */
        rtti_enabled = opt_value;
        break;
      case optk_building_runtime:
        /* We are building the runtime library for the compiler. */
        check_assertion(opt_value == TRUE);
        building_runtime = TRUE;
        break;
      case optk_bool_is_keyword:
        /* bool is or is not a keyword. */
        bool_is_keyword = opt_value;
        break;
      case optk_array_new_and_delete:
        /* Enable/disable array new and delete. */
        array_new_and_delete_enabled = opt_value;
        break;
      case optk_explicit:
        /* Enable/disable recognition of the keyword "explicit". */
        explicit_keyword_enabled = opt_value;
        break;
      case optk_namespaces:
        /* Enable/disable namespaces. */
        namespaces_enabled = opt_value;
        break;
      case optk_implicit_using_std:
        /* Enable/disable implicit use of the std namespace by the runtime. */
        implicit_using_std = opt_value;
        break;
      case optk_remove_unneeded_entities:
        /* If FALSE, suppress elimination of unneeded IL entries. */
        remove_unneeded_entities = opt_value;
        break;
      case optk_typename:
        /* Enable/disable typename. */
        typename_enabled = opt_value;
        break;
      case optk_implicit_typename:
        /* Enable/disable implicit determination of whether a template
           dependent name is a type or nontype. */
        implicit_typename_enabled = opt_value;
        break;
      case optk_special_subscript_cost:
        /* Enable/disable a special weighting for the conversion to the
           integral operand of [] in overload resolution. */
        special_subscript_cost = opt_value;
        break;
      case optk_old_style_preprocessing:
        /* Enable PCC style preprocessing. */
        old_style_preprocessing = TRUE;
        break;
      case optk_old_for_init:
        /* Enable/disable old-style scoping for for-init declarations. */
        use_nonstandard_for_init_scope = opt_value;
        break;
      case optk_for_init_diff_warning:
        /* Enable/disable warnings when new for-init scoping gives different
           visibility than old rules. */
        warning_on_for_init_difference = opt_value;
        break;
      case optk_distinct_template_signatures:
        /* Enable distinct name mangling for templates and nontemplates. */
        distinct_template_signatures = opt_value;
        break;
      case optk_guiding_decls:
        /* Enable/disable guiding declarations of template functions. */
        guiding_decls_allowed = opt_value;
        break;
      case optk_old_specializations:
        /* Enable/disable old-style specialization declarations. */
        old_specializations_allowed = opt_value;
        break;
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
      case optk_implicit_extern_c_type_conversion:
        /* Enable/disable conversions between extern "C" and extern "C++"
           function pointers. */
        impl_conv_between_c_and_cpp_function_ptrs_allowed = opt_value;
        break;
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
      case optk_long_preserving_rules:
        long_preserving_rules = opt_value;
        break;
      case optk_extern_inline:
        extern_inline_allowed = opt_value;
        break;
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
      case optk_multibyte_chars:
        multibyte_chars_in_source_enabled = opt_value;
        break;
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
      case optk_embedded_cplusplus:
        report_embedded_cplusplus_noncompliance = opt_value;
        break;
#if VLA_ALLOWED
      case optk_vla:
        vla_enabled = opt_value;
        break;
#endif /* VLA_ALLOWED */
      case optk_enum_overloading:
        operator_overloading_on_enums_enabled = opt_value;
        break;
      case optk_nonstandard_qualifier_deduction:
        nonstandard_qualifier_deduction = opt_value;
        break;
#if ONE_INSTANTIATION_PER_OBJECT
      case optk_one_instantiation_per_object:
        /* Enable one instantiation per object file mode. */
        one_instantiation_per_object = opt_value;
        break;
      case optk_instantiation_dir:
        instantiation_dir_name = opt_arg;
        if (!is_directory(instantiation_dir_name)) {
          str_command_line_error(ec_cl_invalid_instantiation_directory,
                                 instantiation_dir_name);
        }  /* if */
        break;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
      case optk_late_tiebreaker:
        /* Early vs. late overload resolution tiebreaker. */
        do_late_ovl_res_tiebreaker = opt_value;
        break;
      case optk_pending_instantiations:
        /* The number of instantiations of a given template that may be in
           progress at any given time, or zero for an unlimited number. */
        max_pending_instantiations = scan_opt_arg_number(opt_arg);
        if (max_pending_instantiations == 0) {
          max_pending_instantiations = ULONG_MAX;
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case optk_import_dir:
        import_dir_name = opt_arg;
        if (!is_directory(import_dir_name)) {
          str_command_line_error(ec_cl_invalid_import_directory,
                                 import_dir_name);
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case optk_const_string_literals:
        /* String literals are const. */
        string_literals_are_const = opt_value;
        break;
      case optk_class_name_injection:
        /* Class names should or should not be injected into the scope of
           the class. */
        class_name_injection_enabled = opt_value;
        break;
      case optk_arg_dependent_lookup:
        /* Argument dependent lookup should or should not be performed. */
        arg_dependent_lookup_enabled = opt_value;
        break;
      case optk_friend_injection:
        /* Class and function names declared only in friend declarations
           should or should not be visible to normal lookups. */
        friend_injection_enabled = opt_value;
        break;
      case optk_nonstandard_using_decl:
        /* A nonmember using-declaration that specifies an unqualified name
           should or should not be accepted. */
        nonstandard_using_decl_allowed = opt_value;
        break;
      case optk_designators:
        /* Ordinary designators should or should not be accepted. */
        designators_allowed = opt_value;
        break;
      case optk_extended_designators:
        /* Extended designators should or should not be accepted. */
        extended_designators_allowed = opt_value;
        break;
      case optk_variadic_macros:
        /* Ordinary variadic macros should or should not be accepted. */
        variadic_macros_allowed = opt_value;
        break;
      case optk_extended_variadic_macros:
        /* Extended variadic macros should or should not be accepted. */
        extended_variadic_macros_allowed = opt_value;
        break;
      case optk_include_file_suffixes:
        /* Specifies the list of suffixes to be used when searching for an
           include file name specified with no suffix. */
        include_file_suffixes = opt_arg;
        break;
      case optk_compound_literals:
        /* Compound literals should or should not be accepted. */
        compound_literals_allowed = opt_value;
        break;
      case optk_base_assign_op_is_default:
        /* A copy assignment operator that takes a base class as input should
           or should not be considered to be the default copy assignment
           operator for a class. */
        allow_copy_assignment_op_with_base_class_param = opt_value;
        break;
      case optk_sun_mode:
        /* Compatibility with Sun CC 5.0 (various extensions/bugs) should or
           should not be provided.  This option implies C++ mode, even in
           the "--no_sun" form.  In other words, --[no_]sun is short for
           --c++ --[no_]_sun. See --c99 and --svr4 for similar behavior. */
        sun_mode = opt_value;
        C_dialect = C_dialect_cplusplus;
        break;
      case optk_dependent_name_processing:
        /* Enable dependent name processing for templates. */
        do_dependent_name_processing = opt_value;
        break;
      case optk_ignore_namespace_std:
        /* Enable the g++ compatibility option that treats "std" as an
           alias for the global namespace. */
        ignore_std_namespace = opt_value;
        break;
      case optk_parse_nonclass_templates:
        /* Enable prototype instantiation of nonclass templates. */
        nonclass_prototype_instantiations = opt_value;
        break;
      case optk_c99_mode:
        /* C99 mode should or should not be used.  This option implies
           ANSI C mode, even in the "--no_c99" form. In other words,
           --[no_]c99 is short for --c --[no_]c99.  See --svr4 and --sun
           for similar behavior. */
        c99_mode = opt_value;
        C_dialect = C_dialect_ANSI;
        break;
      case optk_export_template:
        /* Enable use of exported templates. */
        export_template_allowed = opt_value;
        /* Make sure the keyword is enabled if export support is being
           enabled. */
        if (export_template_allowed) export_keyword_enabled = TRUE;
        break;
      case optk_stdarg_builtin:
        /* Enable passing of references to stdarg.h macros to the output
           unchanged. */
        pass_stdarg_references_to_generated_code = opt_value;
        break;
#if ENABLE_TRANS_UNIT_TEST_MODE
      case optk_trans_unit_test_mode:
        /* Enable mode to test the compilation of multiple (possibly identical)
           translation units. */
        trans_unit_test_mode = opt_value;
        break;
#endif /* ENABLE_TRANS_UNIT_TEST_MODE */
#if GNU_EXTENSIONS_ALLOWED
      case optk_gcc_mode:
        /* GNU C mode should or should not be used.  This option implies
           ANSI C mode, even in the "--no_gcc" form. In other words,
           --[no_]gcc is short for --c --[no_]gcc.  See --svr4, --c99 and
           --sun for similar behavior. */
        gcc_mode = opt_value;
        C_dialect = C_dialect_ANSI;
        break;
      case optk_gpp_mode:
        /* GNU C++ mode should or should not be used.  This option implies
           C++ mode, even in the "--no_g++" form. In other words,
           --[no_]g++ is short for --c++ --[no_]g++.  See --sun, --c99 and
           --svr4 for similar behavior. */
        gpp_mode = opt_value;
        C_dialect = C_dialect_cplusplus;
        break;
      case optk_short_enums:
        /* An options to specify that all enumeration types should be
           treated as if they were declared with the "packed" attribute. */
        check_assertion(opt_value == TRUE);
        il_header.short_enums = TRUE;
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case optk_long_long:
        /* The long long feature cannot be disabled when the front end is
           configured to use it, but its use results in an error in certain
           modes.  This option is used to suppress such errors. */
        long_long_is_standard = opt_value;
        break;
      case optk_context_limit:
        /* Specify the maximum number of instantiation contexts that should be
           displayed as part of a diagnostic message. */
        context_limit = (int)scan_opt_arg_number(opt_arg);
        /* The smallest allowed value is 2.  If 1 is specified, use 2
           instead.  A value of zero is permitted and is interpreted as
           no limit. */
        if (context_limit == 1) context_limit = 2;
        /* Convert odd numbers to the next lower even number.  We display
           limit/2 lines of initial and trailing context so we can only
           really handle even numbers. */
        context_limit = context_limit & (~1);
        break;
      case optk_set_flag:
        /* Set the value of a specified flag name. */
        set_flag_value(opt_arg, opt_value);
        break;
#if UPC_EXTENSIONS_ALLOWED
      case optk_upc_mode:
        /* Enable (or disable) support for Unified Parallel C.  Specifying
           these options also implies C mode. */
        upc_mode = opt_value;
        C_dialect = C_dialect_ANSI;
        break;
      case optk_upc_strict_access:
        /* Set the default UPC access mode. */
        il_header.default_upc_strict_access = opt_value;
        break;
      case optk_upc_threads:
        /* Set the number of UPC threads at compile time. */
        upc_num_threads = scan_opt_arg_number(opt_arg);
        break;
#endif /* UPC_EXTENSIONS_ALLOWED */
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
  /* Check for consistent specification of dialects and language modes. */
  check_dialect_and_language_modes();
  /* Based on dialect and language mode settings, check for consistency of
     other options, and set global variables as appropriate. */
  if (C_dialect != C_dialect_cplusplus) {
    /* Check for the use of C++ options when the dialect being compiled
       is not C++. */
    check_and_set_c_mode_options();
  } else {
    /* The dialect is C++. */
    if (any_cfront_mode()) {
      /* Turn on cfront features. */
      set_cfront_mode_flags();
    }  /* if */
    check_and_set_cplusplus_mode_options();
  }  /* if */
  if (strict_ansi_mode) {
    check_and_set_ansi_mode_options();
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Turn on features implied by Microsoft mode. */
    set_microsoft_mode_flags();
  } else {
    /* Microsoft mode is not being used. */
    microsoft_bugs = FALSE;
    if (import_dir_name != NULL) {
      /* --import_dir is allowed only in Microsoft mode. */
      command_line_error(ec_cl_import_only_in_microsoft);
    }  /* if */
  }  /* if */
  /* If no directory was specified for #import, use the current directory. */
  if (import_dir_name == NULL) {
    import_dir_name = ".";
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (extended_designators_allowed) {
    /* If extended designators are allowed, the normal designators must
       be allowed also. */
    designators_allowed = TRUE;
  }  /* if */
  if (extended_variadic_macros_allowed) {
    /* If extended variadic macros are allowed, the normal variadic macros
       must be allowed also. */
    variadic_macros_allowed = TRUE;
  }  /* if */
  if (!do_dependent_name_processing && export_template_allowed) {
    /* We're not doing dependent name processing, but export template
       is specified.  If export template processing was not explicitly
       requested, turn it off. */
    if (!option_kind_used[(int)optk_export_template]) {
      export_template_allowed = FALSE;
    }  /* if */
  }  /* if */
  if (export_template_allowed) {
    /* Export template processing requires dependent name processing. */
    if (option_kind_used[(int)optk_dependent_name_processing] &&
        !do_dependent_name_processing) {
      /* The option --no_dep_name was used: export template requires
         that dependent name processing is done. */
      command_line_error(ec_cl_export_template_requires_dep_name);
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (option_kind_used[(int)optk_implicit_template_inclusion] &&
        implicit_template_inclusion_mode) {
      /* The option --implicit_include was used: it cannot be used with
         export template processing. */
      command_line_error(ec_cl_export_template_requires_no_implicit_include);
    }  /* if */
    implicit_template_inclusion_mode = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    do_dependent_name_processing = TRUE;
  }  /* if */
  if (trans_unit_test_mode) {
    /* Exported templates cannot be used in trans_unit_test mode.  Turn
       off the feature but reduce the diagnostic to a warning. */
    export_template_allowed = FALSE;
    (void)set_severity_for_error_number((int)ec_no_export_support, es_warning);
  }  /* if */
  if (!nonclass_prototype_instantiations && do_dependent_name_processing) {
    /* We're not doing nonclass prototype instantiations, but dependent
       name processing is specified.  If dependent name processing was not
       explicitly requested, turn it off. */
    if (!option_kind_used[(int)optk_dependent_name_processing]) {
      do_dependent_name_processing = FALSE;
    }  /* if */
  }  /* if */
  if (do_dependent_name_processing) {
    /* Do nonclass prototype instantiations when dependent name processing
       is being done. */
    if (option_kind_used[(int)optk_parse_nonclass_templates] &&
        !nonclass_prototype_instantiations) {
      /* The option --no_parse_templates was used: it implies that
         no dependent name processing is done. */
      command_line_error(ec_cl_dep_name_requires_parse_nonclass_templates);
    }  /* if */
    nonclass_prototype_instantiations = TRUE;
    /* Do argument dependent lookup when doing dependent name processing. */
    arg_dependent_lookup_enabled = TRUE;
  }  /* if */
  if (nonclass_prototype_instantiations) {
    implicit_typename_enabled = FALSE;
  }  /* if */
  if (sun_mode) {
    check_and_set_sun_mode_options();
  }  /* if */
  if (gcc_mode) {
    check_and_set_gcc_mode_options();
  } else if (gpp_mode) {
    check_and_set_gpp_mode_options();
  } else {
    exclude_gnu_specific_options();
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    check_upc_mode();
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
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
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object) {
    /* If "one instantiation per object" mode is being used, supply a default
       for the instantiation directory.  This should always be specified
       the driver.  The default value is primarily for testing purposes. */
    if (instantiation_dir_name == NULL) instantiation_dir_name = ".";
  } else {
    /* If one instantiation per object mode is not being used, set the
       instantiation directory to NULL just in case one was specified on
       the command line. */
    instantiation_dir_name = NULL;
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Determine whether enum types are considered to be integral. This global
     variable is used by is_integral_type, which returns TRUE for enum types
     in C mode and cfront mode, but otherwise returns FALSE in C++. */
  enum_type_is_integral = C_mode() || any_cfront_mode();
  /* Determine the appropriate error level for anachronism messages based
     on whether anachronisms are to be allowed. */
  anachronism_error_severity = allow_anachronisms ? es_warning : es_error;
  if (allow_anachronisms) {
    /* Enable the nonconst ref anachronism if anachronisms in general are
       enabled. */
    allow_nonconst_ref_anachronism = TRUE;
  }  /* if */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
#if SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED
  /* Suppress typeinfo variables when RTTI is disabled. */
  generate_rtti_typeinfo = rtti_enabled;
#endif /* SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */
  /* warning_on_for_init_difference may be TRUE only if the new for-init
     scoping rules are in effect. */
  if (use_nonstandard_for_init_scope) warning_on_for_init_difference = FALSE;
  /* Choose the style of preprocessing.  PCC preprocessing is always done
     in PCC mode, and may also be done in other modes if specified
     by a command line option. */
  pcc_preprocessing_mode = (C_dialect == C_dialect_pcc) ||
                           old_style_preprocessing;
#if OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
  if (any_cfront_mode()) {
    /* When configured that way, use old-style preprocessing for cfront
       compatibility mode. */
    pcc_preprocessing_mode = TRUE;
  }  /* if */
#endif /* OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */
  /* In PCC preprocessing mode no token separators are emitted (normally these
     make sure that the preprocessor output contains the same sequence of
     tokens as its input). */
  no_token_separators_in_pp_output = pcc_preprocessing_mode;

  /* Add the default directories to the end of the include search path.
     The list is then any -I directories, in the order they were specified,
     and the default directories at the end. */
  add_default_include_search_path(&incl_search_path, &end_incl_search_path);
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
    dir_name_of_primary_source_file = dir_name;
    add_to_front_of_include_search_path(dir_name, &incl_search_path,
                                        &end_incl_search_path);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE && AUTOMATIC_TEMPLATE_INSTANTIATION
  /* The C++-generating back end can't handle instantiations of exported
     templates. */
  export_template_allowed = FALSE;
#endif /* BACK_END_IS_CP_GEN_BE && AUTOMATIC_TEMPLATE_INSTANTIATION */
#if COMPILE_MULTIPLE_SOURCE_FILES || COMPILE_MULTIPLE_TRANSLATION_UNITS
  /* Multiple source files can be compiled.  Save the count and argv
     position of remaining files, if any. */
  argc_file_list = argc - opt_ind;
  argv_file_list = &argv[opt_ind];
#if COMPILE_MULTIPLE_SOURCE_FILES
  more_than_one_source_file = (argc_file_list > 0);
  if (more_than_one_source_file) {
    /* There are at least two files.  The -o, -L, and -X options (those
       that specify output files) cannot be used, because they only specify
       one file. */
    if (ofile_name != NULL || f_raw_listing != NULL || f_xref_info != NULL) {
      command_line_error(ec_cl_output_file_incompatible_with_multiple_inputs);
    }  /* if */
    if (precompiled_header_processing_required) {
      command_line_error(ec_cl_pch_incompatible_with_multiple_inputs);
    }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
    if (one_instantiation_per_object) {
      command_line_error(
         ec_cl_one_instantiation_per_object_incompatible_with_multiple_inputs);
    }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (ii_file_name != NULL) {
      command_line_error(ec_cl_ii_file_name_incompatible_with_multiple_inputs);
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES */
#if COMPILE_MULTIPLE_TRANSLATION_UNITS
  more_than_one_non_export_translation_unit = (argc_file_list > 0);
  if (more_than_one_non_export_translation_unit) {
    /* Multiple translation units were specified.  Check for any options that
       cannot be used when using multiple translation units. */
    if (list_makefile_dependencies) {
      command_line_error(
          ec_cl_list_make_dependencies_incompatible_with_multiple_trans_units);
    }  /* if */
    if (generate_pp_output) {
      command_line_error(
                       ec_cl_pp_output_incompatible_with_multiple_trans_units);
    }  /* if */
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (option_kind_used[(int)optk_implicit_template_inclusion] &&
        implicit_template_inclusion_mode) {
      /* The option --implicit_include was used: it cannot be used when
         compiling multiple translation units. */
      command_line_error(
                ec_cl_implicit_include_incompatible_with_multiple_trans_units);
    }  /* if */
    /* Disable implicit inclusion when using multiple translation units. */
    implicit_template_inclusion_mode = FALSE;
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  }  /* if */
#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */
#else /* !(COMPILE_MULTIPLE_SOURCE_FILES ||
         COMPILE_MULTIPLE_TRANSLATION_UNITS) */
  /* Multiple source files cannot be compiled. */
  /* Check that all command-line arguments were taken. */
  if (opt_ind < argc) {
    command_line_error(ec_cl_too_many_arguments);
  }  /* if */
#endif /* COMPILE_MULTIPLE_SOURCE_FILES ||
          COMPILE_MULTIPLE_TRANSLATION_UNITS */

  /* The -o option controls either the name of the preprocessing output
     file or the name of the IL file, depending on the kind of compilation. */
  if (do_preprocessing_only) {
    /* Since preprocessing output is being generated, the output file
       name can be specified by a -o option. */
    pp_file_name = ofile_name;
    pp_output_file_needed = TRUE;
    ofile_name = NULL;
#if IL_SHOULD_BE_WRITTEN_TO_FILE
    il_file_name = NULL;
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
  if (do_preprocessing_only && suppress_do_preprocessing_only) {
    /* The implicit setting of do_preprocessing_only can be overridden
       by the --no_preproc_only command line option.  Note that this is tested
       after setting the pp_file_name above so that the -o option specifies
       the pp_file_name even if a full compilation is being done. */
    do_preprocessing_only = FALSE;
  }  /* if */
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
    if ((list_makefile_dependencies || list_included_files) &&
        error_threshold == es_warning &&
        do_preprocessing_only) {
      /* When preprocessing only to list makefile dependencies or
         included files, suppress warnings. */
      error_threshold = es_discretionary_error;
    }  /* if */
  }  /* if */
#if DO_IL_LOWERING
  /* Prototype instantiations cannot be lowered, so make sure that they are
     not generated when doing IL lowering. */
  prototype_instantiations_in_il = FALSE;
#endif /* DO_IL_LOWERING */
  /* If the -o option appeared, its file should have been taken for
     something. */
  if (ofile_name != NULL) {
    command_line_error(ec_cl_no_output_file_needed);
  }  /* if */
}  /* proc_command_line */

#if COMPILE_MULTIPLE_TRANSLATION_UNITS

void proc_secondary_translation_units(void)
/*
This is a temporary routine for testing of the routines that handle
multiple translation units.

Call the translation unit routine for the secondary translation units.
*/
{
  char	*file_name;

  /* Make sure the same file name was not specified more than once.
     Duplicates are permitted in trans_unit_test_mode (because that is
     really the whole point of that mode). */
  if (!trans_unit_test_mode) check_for_duplicated_file_names();
  while (argc_file_list > 0) {
    /* There is another file. */
    argc_file_list--;
    file_name = *(argv_file_list)++;
    if (put_dir_of_each_opened_source_file_on_incl_search_path) {
      /* Update the first entry of the include file search list, the one
         that contains the directory of the primary source file. */
      /* If you change this, see the similar code in proc_command_line. */
      dir_name_of_primary_source_file = 
                                    gs_directory_of(file_name);
      change_primary_include_search_dir(dir_name_of_primary_source_file);
    }  /* if */
    process_translation_unit(file_name, /*is_primary=*/FALSE,
                             (an_exported_template_file_ptr)NULL);
  }  /* while */
}  /* proc_secondary_translation_units */

#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */

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
      dir_name_of_primary_source_file = 
                                    gs_directory_of(primary_source_file_name);
      change_primary_include_search_dir(dir_name_of_primary_source_file);
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

void cmd_line_early_init(void)
/*
One time initialization that must take place early on in the front end.
This is done before command line processing.
*/
{
  export_template_allowed = DEFAULT_EXPORT_TEMPLATE_ALLOWED;
  export_keyword_enabled = TRUE;
  curr_command_line_macro_def = NULL;
  gpp_dependent_base_class_lookup = FALSE;
  defer_friend_instantiation = TRUE;
}  /* cmd_line_early_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
