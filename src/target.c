/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

target.c -- Target configuration support

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Normally, the target macros (e.g., TARG_SIZEOF_INT, etc.) are undefined
   by target.h so that they cannot be used inadvertently in other files where
   the associated variables (e.g., targ_sizeof_int, etc.) should be used
   instead.  The macros are needed in this file, however, because the
   associated variables are initialized here. */
#define DO_NOT_UNDEF_TARGET_MACROS

/* Header files common to all files. */
#include "fe_common.h"


/*
In addition to the "default" configuration, a number of target configurations
may also be specified.  Just as is the case for the default configuration,
a target configuration is defined by the values given for a set of
target-specific configuration macros.  Those macro names are the same as their
"default" conterparts with an underscore and configuration name appended (e.g.,
TARG_SIZEOF_INT_my_config for the "my_config" target).  These target
configurations are conditionally compiled into the front end when one or more
TARGET_CONFIGURATION_* macros are defined with the value of the unique name of
the configuration (e.g., "#define TARGET_CONFIGURATION_1 my_config").  The
number of configurations listed here is arbitrary and may be added to as
necessary (also add entries to target_configurations below).
*/
#ifdef TARGET_CONFIGURATION_1
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_1
#include "target_cfg.h"
#endif /* TARGET_CONFIGURATION_1 */

#ifdef TARGET_CONFIGURATION_2
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_2
#include "target_cfg.h"
#endif /* TARGET_CONFIGURATION_2 */

#ifdef TARGET_CONFIGURATION_3
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_3
#include "target_cfg.h"
#endif /* TARGET_CONFIGURATION_3 */

#ifdef TARGET_CONFIGURATION_4
#define TARGET_CONFIGURATION TARGET_CONFIGURATION_4
#include "target_cfg.h"
#endif /* TARGET_CONFIGURATION_4 */

#ifdef TARGET_CONFIGURATION_5
 #error Need to add additional TARGET_CONFIGURATION_X entries
#endif /* TARGET_CONFIGURATION_5 */

/*
This structure is used to associate a target configuration name with routines
to set the target configuration and dump the target configuration.
*/
typedef struct a_target_configuration {
  a_const_char  *name;  /* The name of this target configuration. */
  void          (*set_target_config)(void);
                        /* The address of a routine that will set the
                           target-specific global variables to the values
                           for this target configuration. */
#if DUMP_CONFIG_ENABLED
  void          (*dump_target_config)(void);
                        /* The address of a routine that will dump the
                           target-specific configuration macros associated
                           with this target configuration.  Used for
                           --dump_configuration. */
#endif /* DUMP_CONFIG_ENABLED */
} a_target_configuration;

/*
Define a macro to initialize an a_target_configuration entry
*/
#if DUMP_CONFIG_ENABLED
#define DEFINE_TARGET_CONFIGURATION(name) \
  { stringize(name), \
    concat(set_target_config ## _, name), \
    concat(dump_target_config ## _, name) \
  }
#else /* !DUMP_CONFIG_ENABLED */
#define DEFINE_TARGET_CONFIGURATION(name) \
  { stringize(name), \
    concat(set_target_config ## _, name) \
  }
#endif /* DUMP_CONFIG_ENABLED */
/*lint -esym(750,DEFINE_TARGET_CONFIGURATION)*/

/*
This array contains an entry for each target configuration that has been
defined at compilation time.
*/
static a_target_configuration target_configurations[] = {
#ifdef TARGET_CONFIGURATION_1
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_1),
#endif /* defined(TARGET_CONFIGURATION_1) */
#ifdef TARGET_CONFIGURATION_2
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_2),
#endif /* defined(TARGET_CONFIGURATION_2) */
#ifdef TARGET_CONFIGURATION_3
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_3),
#endif /* defined(TARGET_CONFIGURATION_3) */
#ifdef TARGET_CONFIGURATION_4
  DEFINE_TARGET_CONFIGURATION(TARGET_CONFIGURATION_4),
#endif /* defined(TARGET_CONFIGURATION_4) */
  /* More can be added if needed (ensure target_cfg.h is included above). */
  { "", (void(*)(void))0, (void(*)(void))0 }
};

/* The number of target configurations defined at compilation time (not
   counting the dummy entry at the end). */
#define NUM_TARGET_CONFIGURATIONS \
  ((int32_t)(sizeof(target_configurations)/sizeof(target_configurations[0])-1))

#if DUMP_CONFIG_ENABLED

/*
Define the dump_as_target_config function that takes a string argument and
produces a list of #defines suitable for using the "default" configuration
as the named target configuration.  E.g., when called with "my_config",
would emit "#define TARG_SIZEOF_INT_my_config 4" if TARG_SIZEOF_INT has
the value 4 in the default configuration (for each target-specific
configuration macro).
*/
/* Routine name: dump_as_target_config. */
#define TARGET_MAP_ROUTINE_NAME(config) \
  dump_as_target_config(a_const_char *suffix)
/* Write "#define MACRO_config MACRO-default-value" to stderr. */
#define TARGET_MAP_MACRO(config_macro, global_var, config) \
  fprintf(f_error, "#define %s_%s %s\n", #config_macro, \
          suffix, stringize(config_macro));
#include "target_map.h"


void dump_default_config_as_target_config(a_const_char *config)
/*
Called when processing the --dump_default_as_target command-line option.
This option is used as an aid in creating new target configurations; when used,
it emits a set of #defines for a new configuration (as named by the argument)
whose values are the same as those for the "default" configuration of the
front end.  The output (after modifying the TARGET_CONFIGURATION_X entry)
can then be included by defines.h and once the front end is recompiled, that
target configuration becomes available.  Note that other command-line options
have no effect on the output.
*/
{
  fprintf(f_error, "/* Target configuration: %s */\n", config);
  fprintf(f_error,
   "/* NOTE: For multiple configurations, change _1 below as necessary. */\n");
  fprintf(f_error, "#define TARGET_CONFIGURATION_1 %s\n\n", config);
  dump_as_target_config(config);
}  /* dump_default_config_as_target_config */

#endif /* DUMP_CONFIG_ENABLED */

int32_t find_target_configuration(a_const_char *config)
/*
Utility to return the target configuration index for the specified target
configuration string or NO_TARGET_CONFIG if no such target configuration
has been specified.
*/
{
  int32_t  i, result = NO_TARGET_CONFIG;

  for (i = 0; i < NUM_TARGET_CONFIGURATIONS; i++) { /*lint !e681*/
    if (strcmp(target_configurations[i].name, config) == 0) {
      result = i;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* find_target_configuration */


void set_target_configuration(int32_t target_index)
/*
Called when --target has been specified to set the appropriate global variables
to their values associated with the specified target configuration index
(an index into &target_configurations).  Note that this is invoked when
the --target command-line option is being processed, so there is a race
condition if any global variable is set here and also as a side-effect of
other command-line processing.
*/
{
  if (target_configuration_index != NO_TARGET_CONFIG) {
    a_target_configuration *target;
    check_assertion(target_index >= 0 &&
                    target_index < NUM_TARGET_CONFIGURATIONS);
    target = &target_configurations[target_configuration_index];
    target->set_target_config();
    /* Create a target-specific version of this name so that predefined
       macros can be different for each target configuration. */
    check_assertion(EDG_AUXILIARY_INFO_DIR_NAME != NULL); /*lint !e779*/
    auxiliary_info_dir_name = alloc_general(
                                        strlen(EDG_AUXILIARY_INFO_DIR_NAME) +
                                        strlen(target->name) + 2);
    (void)strcpy(auxiliary_info_dir_name, EDG_AUXILIARY_INFO_DIR_NAME);
    (void)strcat(auxiliary_info_dir_name, "_");
    (void)strcat(auxiliary_info_dir_name, target->name);
  }  /* if */
}  /* set_target_configuration */

#if DUMP_CONFIG_ENABLED

void dump_target_configurations(void)
/*
Dumps any target-configuration specific macros in such a format that they
can be re-read as a defines.h (as part of the processing for
--dump_configuration).
*/
{
  int  i;

  for (i = 0; i < NUM_TARGET_CONFIGURATIONS; i++) { /*lint !e681*/
    fprintf(f_error, "\n/* Target configuration: %s */\n",
            target_configurations[i].name);
    fprintf(f_error, "#define TARGET_CONFIGURATION_%d %s\n", i+1,
            target_configurations[i].name);
    target_configurations[i].dump_target_config();
  }  /* for */
}  /* dump_target_configurations */

#endif /* DUMP_CONFIG_ENABLED */

static void set_plain_char_int_kind(a_boolean plain_chars_are_signed)
/*
Set plain_char_int_kind, which indicates the integer kind for "plain"
(neither signed or unsigned) char.  plain_chars_are_signed indicates
whether it should be signed.
*/
{
  if (C_dialect == C_dialect_pcc ||
      (microsoft_mode && C_mode())) {
    /* In pcc mode, a "plain" char is the same as either "signed char"
       or "unsigned char".  Likewise in Microsoft C mode. */
    plain_char_int_kind = plain_chars_are_signed ?
                               (an_integer_kind)ik_signed_char :
                               (an_integer_kind)ik_unsigned_char;
  } else {
    /* In standard mode, a "plain" char is different than "signed char" and
       "unsigned char". */
    plain_char_int_kind = (an_integer_kind)ik_char;
  }  /* if */
}  /* set_plain_char_int_kind */

#if MICROSOFT_EXTENSIONS_ALLOWED

void init_microsoft_sized_int_types(void)
/*
Map __int8, __int16, __int32, and __int64 to the appropriate integer
kinds.  Leave the variables set to ik_none if a match can't be found;
only if a corresponding integer kind is found will the corresponding
keyword be entered into the symbol table.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  /* The signedness of characters can be set on the command line, but for
     standalone utilities, we determine this from the IL header. */
  set_plain_char_int_kind(il_header.plain_chars_are_signed);
#endif /* STANDALONE_UTILITY_PROGRAM */
  /* Map __int8 to plain char if and only if 8-bit chars are being used. */
  if (targ_char_bit == 8) {
    targ_int8_int_kind = plain_char_int_kind;
    targ_unsigned_int8_int_kind = (an_integer_kind)ik_unsigned_char;
  }  /* if */
  /* For the other cases, find the first integer kinds, signed and unsigned,
     that hold exactly 16, 32 and 64 bits, respectively. */  
  targ_int16_int_kind = int_kind_for_bit_size(16, /*signed=*/TRUE);
  if (targ_int16_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int16_int_kind = int_kind_for_bit_size(16, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int16_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int16");
  }  /* if */
  targ_int32_int_kind = int_kind_for_bit_size(32, /*signed=*/TRUE);
  if (targ_int32_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int32_int_kind = int_kind_for_bit_size(32, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int32_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int32");
  }  /* if */
  targ_int64_int_kind = int_kind_for_bit_size(64, /*signed=*/TRUE);
  if (targ_int64_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int64_int_kind = int_kind_for_bit_size(64, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int64_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int64");
  }  /* if */
}  /* init_microsoft_sized_int_types */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if TARG_ALL_POINTERS_SAME_SIZE
/*ARGSUSED*/ /* Because tp is not used. */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
a_targ_size_t size_of_pointer_to(a_type_ptr        tp,
                                 a_targ_alignment  *alignment)
/*
Return the size and alignment for a pointer type that points to the indicated
type.  This routine should be rewritten for implementations in which
TARG_ALL_POINTERS_SAME_SIZE may not always be TRUE.
*/
{
  a_targ_size_t size;

#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    /* Pointers come in "near" and "far" sizes (e.g., Microsoft 16-bit
       mode). */
    if (is_far_type(tp)) {
      size = targ_sizeof_far_pointer;
      *alignment = targ_alignof_far_pointer;
    } else {
      size = targ_sizeof_near_pointer;
      *alignment = targ_alignof_near_pointer;
    }  /* if */
  } else
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Do not add code here. */
  {
#if TARG_ALL_POINTERS_SAME_SIZE
    /* All pointers have the same size and alignment. */
    size = targ_sizeof_pointer;
    *alignment = targ_alignof_pointer;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
 #error -- code must be added here to determine the size of pointers.
/* If you set TARG_ALL_POINTERS_SAME_SIZE FALSE only because you want 
   to support near/far, see the comments on TARG_ALL_POINTERS_SAME_SIZE
   in targ_def.h and the internal documentation; it's probably not what
   you want. */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  }
  return size;
}  /* size_of_pointer_to */

#if CHECKING

void check_target_configuration(void)
/*
Perform consistency check on target configuration variables.  Note that
some checks that are performed at compilation time are re-checked here
to ensure that incorrect values for configurations specified with --target
are diagnosed.
*/
{
  a_targ_size_t    size, size_max_value;
  a_targ_alignment alignment;
  a_boolean        err;

  /* The target char may be no bigger than the host long. */
  get_integer_size_and_alignment((an_integer_kind)ik_char,
                                 &size, &alignment);
  if (size > sizeof(long)) {
    internal_error("check_target_config: target char is too large");
  }  /* if */
  /* The target wchar_t may be no bigger than the host long. */
  if (targ_sizeof_wchar_t > sizeof(long)) {
    internal_error("check_target_config: target wchar_t is too large");
  }  /* if */
  /* char16_t and char32 require a check similar to wchar_t, and in addition
     they have minimum size requirements.  They must also be unsigned. */
  if (targ_sizeof_char16_t > sizeof(long)) {
    internal_error("check_target_config: target char16_t is too large");
  } else if (targ_sizeof_char16_t*targ_char_bit < 16) {
    internal_error("check_target_config: target char16_t is too small");
  }  /* if */
  check_assertion_str(!int_kind_is_signed[(int)targ_char16_t_int_kind],
                      "check_target_config: target char16_t must be unsigned");
  if (targ_sizeof_char32_t > sizeof(long)) {
    internal_error("check_target_config: target char32_t is too large");
  } else if (targ_sizeof_char32_t*targ_char_bit < 32) {
    internal_error("check_target_config: target char32_t is too small");
  }  /* if */
  check_assertion_str(!int_kind_is_signed[(int)targ_char32_t_int_kind],
                      "check_target_config: target char32_t must be unsigned");
  /* targ_size_t_max must fit in the target integer type targ_size_t_int_kind
     (but it need not fit exactly). */
  get_integer_size_and_alignment((an_integer_kind)targ_size_t_int_kind,
                                 &size, &alignment);
  size *= targ_char_bit;
  if (size > sizeof(a_targ_size_t)*CHAR_BIT) {
    size = sizeof(a_targ_size_t)*CHAR_BIT;
  }  /* if */
  /* Make a mask of "size" 1 bits for the maximum value. */
  size_max_value = ((((a_targ_size_t)1 << (size-1))-1) << 1);
  /* Final "or" done separately to avoid a bug in Borland C++ 3.0 with -O. */
  size_max_value |= 1;
  if (size_max_value < targ_size_t_max) {
    internal_error("check_target_config: targ_size_t_max is too large");
  }  /* if */
  if (targ_sizeof_largest_integer > MAX_SIZEOF_LARGEST_INTEGER) {
    /* The target-specific value for largest integer must fit in the
       representation the front end was configured for. */
    internal_error(
              "check_target_config: targ_sizeof_largest_integer is too large");
  }  /* if */
#if FIXED_POINT_ALLOWED
  if (targ_sizeof_largest_fixed_point > MAX_SIZEOF_LARGEST_FIXED_POINT) {
    /* The target-specific value for largest fixed point must fit in the
       representation the front end was configured for. */
    internal_error(
          "check_target_config: targ_sizeof_largest_fixed_point is too large");
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
#if LONG_LONG_ALLOWED
  if (targ_sizeof_largest_integer < targ_sizeof_long_long) {
    internal_error("check_target_config: invalid targ_sizeof_largest_integer");
  }  /* if */
#else /* !LONG_LONG_ALLOWED */
  if (targ_sizeof_largest_integer < targ_sizeof_long) {
    internal_error("check_target_config: invalid targ_sizeof_largest_integer");
  }  /* if */
#endif /* LONG_LONG_ALLOWED */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  /* When using host integers to represent target integers, make sure the
     host integer selected is large enough. */
  /* Use variable err instead of testing directly to avoid warnings about
     testing invariant values on some compilers. */
  err = (MAX_SIZEOF_LARGEST_INTEGER*targ_char_bit >
         sizeof(an_integer_value)*CHAR_BIT);
  if (err) {
    internal_error("check_target_config: an_integer_value is too small");
  }  /* if */
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  /* When using the simulated large integer approach to represent target
     integers, the sizes must be right. */

  /* Use variable err instead of testing directly to avoid warnings about
     testing invariant values on some compilers. */
  err = (BITS_IN_HOST_LARGE_INTEGER != sizeof(a_host_large_integer)*CHAR_BIT);
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid BITS_IN_HOST_LARGE_INTEGER");
  }  /* if */
  err = (SIZEOF_INT_VALUE_PART > sizeof(an_int_value_part));
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid SIZEOF_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART != SIZEOF_INT_VALUE_PART*CHAR_BIT ||
         2*BITS_IN_INT_VALUE_PART > BITS_IN_HOST_LARGE_INTEGER); /*lint !e506*/
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid BITS_IN_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART*INT_VALUE_PARTS_PER_INTEGER_VALUE !=
         MAX_SIZEOF_LARGEST_INTEGER*targ_char_bit);
  if (err) {
    internal_error(
             "check_target_config: invalid INT_VALUE_PARTS_PER_INTEGER_VALUE");
  }  /* if */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  if (targ_host_string_char_bit > CHAR_BIT) {
    internal_error("check_target_config: targ_host_string_char_bit too large");
  }  /* if */
#if USER_CONTROL_OF_STRUCT_PACKING
  /* Be sure the maximum and minimum values for "pack alignment" are
     appropriate and may be stored within a_targ_alignment, which is a_byte
     (= unsigned char). */
  { a_targ_alignment temp = UCHAR_MAX;  /* Use variable to avoid
                                           lint/gcc complaints. */
    if (targ_minimum_pack_alignment < 1 ||
        targ_minimum_pack_alignment > temp /*lint --e(685)*/) {
      internal_error(
                   "check_target_config: invalid targ_minimum_pack_alignment");
    }  /* if */
  }
  if (targ_maximum_pack_alignment < targ_minimum_pack_alignment ||
      (a_targ_alignment)targ_maximum_pack_alignment !=
                                                targ_maximum_pack_alignment) {
    internal_error("check_target_config: invalid targ_maximum_pack_alignment");
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED
  /* The GNU built-in functions that map on IA-32 vector instructions require
     that integers of specific sizes exist.  To keep things simple, we make
     the slightly stronger requirement that sizeof(short) == 2,
     sizeof(int) == 4, and sizeof(long long) == 8.  (LONG_LONG_ALLOWED is
     always TRUE when GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED is TRUE.) */
  check_assertion_str2(targ_sizeof_short == 2 && targ_sizeof_int == 4 &&
                         targ_sizeof_long_long == 8,
                       "check_target_config: invalid integer sizes for",
                       " GNU IA-32 vector functions");
#endif /* GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED */
#if IA64_ABI && DO_IL_LOWERING
  { 
    /* Verify that the integer kind used for a vtable entry is signed and
       large enough to accommodate offsets.  Must also be the same size as
       a pointer (for type_info and virtual function pointers). */
    a_targ_size_t    vtbl_entry_size, delta_int_size;
    a_targ_alignment dummy_alignment;

    get_integer_size_and_alignment(targ_ia64_vtable_entry_int_kind,
                                   &vtbl_entry_size, &dummy_alignment);
    get_integer_size_and_alignment(targ_delta_int_kind,
                                   &delta_int_size, &dummy_alignment);
#if TARG_ALL_POINTERS_SAME_SIZE
    if (targ_sizeof_pointer != vtbl_entry_size) {
      internal_error(
	    "check_target_config: targ_ia64_vtable_entry_int_kind wrong size");
    }  /* if */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
    if (delta_int_size > vtbl_entry_size) {
      internal_error(
          "check_target_config: targ_ia64_vtable_entry_int_kind is too small");
    }  /* if */
    if (!int_kind_is_signed[(int)targ_ia64_vtable_entry_int_kind]) {
      internal_error(
        "check_target_config: targ_ia64_vtable_entry_int_kind must be signed");
    }  /* if */
  }
#endif /* IA64_ABI && DO_IL_LOWERING */
  if (targ_microsoft_bit_field_allocation &&
      targ_bit_field_container_size != -1) {
    internal_error("check_target_config: targ_microsoft_bit_field_allocation "
                      "must be -1 when targ_bit_field_container_size is TRUE");
  }  /* if */
#if ABI_COMPATIBILITY_VERSION <= 241
  check_assertion(!targ_optimize_empty_base_class_layout);
#endif /* ABI_COMPATIBILITY_VERSION <= 241 */
#if IA64_ABI
  check_assertion(targ_optimize_empty_base_class_layout);
#endif /* IA64_ABI */
}  /* check_target_configuration */

#endif /* CHECKING */

static void init_character_sizes(void)
/*
Initialize the global variables describing the sizes of various character
kinds.
*/
{
  a_targ_alignment  alignment;

  get_integer_size_and_alignment((an_integer_kind)targ_wchar_t_int_kind,
                                 &targ_sizeof_wchar_t, &alignment);
  get_integer_size_and_alignment((an_integer_kind)targ_char16_t_int_kind,
                                 &targ_sizeof_char16_t, &alignment);
  get_integer_size_and_alignment((an_integer_kind)targ_char32_t_int_kind,
                                 &targ_sizeof_char32_t, &alignment);
}  /* init_character_sizes */

#if BACK_END_IS_CP_GEN_BE

void select_cp_gen_be_target_dialect(void)
/*
If CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT is TRUE, select the target dialect
to match the source dialect (including the version of the dialect).
*/
{
#if CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
  check_assertion_str(!gcc_is_generated_code_target &&
                      !microsoft_dialect_is_generated_code_target &&
                      !sun_is_generated_code_target &&
                      !clang_is_generated_code_target,
                      "Target dialect already set.");
#if CHECKING
  {
    /* Note that clang mode is a dialect of GNU mode. */
    int n_dialects =
       (gnu_mode != 0) +
       (microsoft_mode != 0) + /*lint !e514*/
       (sun_mode != 0); /*lint !e514*/
    check_assertion(n_dialects < 2);
  }
#endif /* CHECKING */
  if (gnu_mode) {
    gcc_or_clang_is_generated_code_target = TRUE;
#if GCC_BUILTIN_VARARGS
    gcc_builtin_varargs_in_generated_code = TRUE;
#else /* !GCC_BUILTIN_VARARGS */
    gcc_builtin_varargs_in_generated_code = FALSE;
#endif /* GCC_BUILTIN_VARARGS */
    if (clang_mode) {
      clang_is_generated_code_target = TRUE;
      clang_target_version_number = clang_version;
    } else {
      gcc_is_generated_code_target = TRUE;
      gnu_target_version_number = gnu_version;
    }  /* if */
  } else if (microsoft_mode) {
    microsoft_dialect_is_generated_code_target = TRUE;
    msvc_target_version_number = microsoft_version;
    msvc_is_generated_code_target = MSVC_IS_GENERATED_CODE_TARGET;
  } else if (sun_mode) {
    sun_is_generated_code_target = TRUE;
#ifdef SUN_TARGET_VERSION_NUMBER
    sun_target_version_number = SUN_TARGET_VERSION_NUMBER;
#endif /* ifdef SUN_TARGET_VERSION_NUMBER */
  }  /* if */
#endif /* CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
}  /* select_cp_gen_be_target_dialect */

#endif /* BACK_END_IS_CP_GEN_BE */

void target_early_init(void)
/*
One time initialization that must take place early on in the front end.
Sets global variables to their "default" configuration; a subset of these
variables may be re-set to target-specific values if the --target command-line
option is specified (so these variables should not be used until after
command-line processing, or for the STANDALONE_UTILITY case, after the IL
header has been read and the target has been determined).
*/
{
  target_configuration_index = NO_TARGET_CONFIG;
  targ_little_endian = TARG_LITTLE_ENDIAN;
  targ_char_bit = TARG_CHAR_BIT;
  targ_host_string_char_bit = TARG_HOST_STRING_CHAR_BIT;
  targ_has_signed_chars = TARG_HAS_SIGNED_CHARS;
  targ_char_constant_first_char_most_significant =
                                TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT;
  targ_wchar_t_int_kind = TARG_WCHAR_T_INT_KIND;
  targ_wint_t_int_kind = TARG_WINT_T_INT_KIND;
  targ_char16_t_int_kind = TARG_CHAR16_T_INT_KIND;
  targ_char32_t_int_kind = TARG_CHAR32_T_INT_KIND;
  targ_bool_int_kind = TARG_BOOL_INT_KIND;
  targ_c_bool_int_kind = TARG_C_BOOL_INT_KIND;
  targ_sizeof_short = TARG_SIZEOF_SHORT;
  targ_alignof_short = TARG_ALIGNOF_SHORT;
  targ_sizeof_int = TARG_SIZEOF_INT;
  targ_alignof_int = TARG_ALIGNOF_INT;
  targ_sizeof_long = TARG_SIZEOF_LONG;
  targ_alignof_long = TARG_ALIGNOF_LONG;
#if LONG_LONG_ALLOWED
  targ_sizeof_long_long = TARG_SIZEOF_LONG_LONG;
  targ_alignof_long_long = TARG_ALIGNOF_LONG_LONG;
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  targ_sizeof_int128 = TARG_SIZEOF_INT128;
  targ_alignof_int128 = TARG_ALIGNOF_INT128;
#endif /* INT128_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  targ_int8_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int8_int_kind = ((an_integer_kind)ik_none);
  targ_int16_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int16_int_kind = ((an_integer_kind)ik_none);
  targ_int32_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int32_int_kind = ((an_integer_kind)ik_none);
  targ_int64_int_kind = ((an_integer_kind)ik_none);
  targ_unsigned_int64_int_kind = ((an_integer_kind)ik_none);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  targ_max_class_object_size = TARG_MAX_CLASS_OBJECT_SIZE;
  targ_max_base_class_offset = TARG_MAX_BASE_CLASS_OFFSET;
  targ_optimize_empty_base_class_layout =
                                         TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT;
  targ_bit_field_container_size = TARG_BIT_FIELD_CONTAINER_SIZE;
  targ_microsoft_bit_field_allocation = TARG_MICROSOFT_BIT_FIELD_ALLOCATION;
  targ_plain_int_bit_field_is_unsigned =
                           TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED /*lint !e506*/;
  targ_force_one_bit_bit_field_to_be_unsigned =
                                   TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED;
  targ_enum_bit_fields_are_always_unsigned =
                                      TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED;
  targ_nonnegative_enum_bit_field_is_unsigned =
                                   TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED;
  targ_zero_width_bit_field_alignment = TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT;
  targ_zero_width_bit_field_affects_struct_alignment =
                            TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT;
  targ_unnamed_bit_field_affects_struct_alignment =
                               TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT;
  targ_bit_field_affects_union_alignment =
                                        TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT;
  targ_user_control_of_struct_packing_affects_bit_fields =
                        TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS;
  targ_pad_bit_fields_larger_than_base_type =
                                     TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE;
#if TARG_ALL_POINTERS_SAME_SIZE
  targ_sizeof_pointer = TARG_SIZEOF_POINTER;
  targ_alignof_pointer = TARG_ALIGNOF_POINTER;
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
#if NEAR_AND_FAR_ALLOWED
  targ_sizeof_far_pointer = TARG_SIZEOF_FAR_POINTER;
  targ_alignof_far_pointer = TARG_ALIGNOF_FAR_POINTER;
  targ_sizeof_near_pointer = TARG_SIZEOF_NEAR_POINTER;
  targ_alignof_near_pointer = TARG_ALIGNOF_NEAR_POINTER;
#endif /* NEAR_AND_FAR_ALLOWED */
  targ_ptrdiff_t_int_kind = TARG_PTRDIFF_T_INT_KIND;
  targ_size_t_max = TARG_SIZE_T_MAX;
  targ_size_t_int_kind = TARG_SIZE_T_INT_KIND;
  targ_sizeof_float = TARG_SIZEOF_FLOAT;
  targ_alignof_float = TARG_ALIGNOF_FLOAT;
  targ_sizeof_double = TARG_SIZEOF_DOUBLE;
  targ_alignof_double = TARG_ALIGNOF_DOUBLE;
  targ_sizeof_long_double = TARG_SIZEOF_LONG_DOUBLE;
  targ_alignof_long_double = TARG_ALIGNOF_LONG_DOUBLE;
#if GNU_EXTENSIONS_ALLOWED
  targ_word_mode = (a_type_mode_kind)TARG_WORD_MODE;
  targ_unwind_word_mode = (a_type_mode_kind)TARG_UNWIND_WORD_MODE;
  targ_libgcc_cmp_return_mode = (a_type_mode_kind)TARG_LIBGCC_CMP_RETURN_MODE;
  targ_libgcc_shift_count_mode =
                               (a_type_mode_kind)TARG_LIBGCC_SHIFT_COUNT_MODE;
#if TARG_ALL_POINTERS_SAME_SIZE
  targ_pointer_mode = (a_type_mode_kind)TARG_POINTER_MODE;
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  targ_ssize_t_int_kind = TARG_SSIZE_T_INT_KIND;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES
  targ_short_field_alignment = TARG_SHORT_FIELD_ALIGNMENT;
  targ_int_field_alignment = TARG_INT_FIELD_ALIGNMENT;
  targ_long_field_alignment = TARG_LONG_FIELD_ALIGNMENT;
#if LONG_LONG_ALLOWED
  targ_long_long_field_alignment = TARG_LONG_LONG_FIELD_ALIGNMENT;
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  targ_int128_field_alignment = TARG_INT128_FIELD_ALIGNMENT;
#endif /* INT128_EXTENSIONS_ALLOWED */
  targ_float_field_alignment = TARG_FLOAT_FIELD_ALIGNMENT;
  targ_double_field_alignment = TARG_DOUBLE_FIELD_ALIGNMENT;
  targ_long_double_field_alignment = TARG_LONG_DOUBLE_FIELD_ALIGNMENT;
#endif /* TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES */
  targ_sizeof_ptr_to_data_member = TARG_SIZEOF_PTR_TO_DATA_MEMBER;
  targ_alignof_ptr_to_data_member = TARG_ALIGNOF_PTR_TO_DATA_MEMBER;
  targ_sizeof_ptr_to_member_function = TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION;
  targ_alignof_ptr_to_member_function = TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION;
  targ_sizeof_virtual_function_info = TARG_SIZEOF_VIRTUAL_FUNCTION_INFO;
  targ_alignof_virtual_function_info = TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO;
#if !IA64_ABI
  targ_sizeof_ptr_to_virtual_base_class =
                                         TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS;
  targ_alignof_ptr_to_virtual_base_class =
                                        TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS;
#endif /* !IA64_ABI */
  targ_enum_types_can_be_smaller_than_int =
                                       TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT;
  targ_right_shift_is_arithmetic = TARG_RIGHT_SHIFT_IS_ARITHMETIC;
  targ_too_large_shift_count_is_taken_modulo_size =
                               TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE;
  targ_minimum_struct_alignment = TARG_MINIMUM_STRUCT_ALIGNMENT;
#if USER_CONTROL_OF_STRUCT_PACKING
  targ_minimum_pack_alignment = TARG_MINIMUM_PACK_ALIGNMENT;
  targ_maximum_pack_alignment = TARG_MAXIMUM_PACK_ALIGNMENT;
  targ_maximum_intrinsic_alignment = TARG_MAXIMUM_INTRINSIC_ALIGNMENT;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  packing_applies_to_base_classes =
                      TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES;
  distinct_template_signatures = DEFAULT_DISTINCT_TEMPLATE_SIGNATURES;
  assume_references_cannot_be_null = ASSUME_REFERENCES_CANNOT_BE_NULL;
#if DO_IL_LOWERING
  force_variable_definition_via_zeroing =
                                         FORCE_VARIABLE_DEFINITION_VIA_ZEROING;
  make_all_functions_unprototyped = MAKE_ALL_FUNCTIONS_UNPROTOTYPED;
  assume_this_cannot_be_null_in_conditional_operators =
                           ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS;
#if DO_FULL_PORTABLE_EH_LOWERING
  targ_jmp_buf_num_elements = TARG_JMP_BUF_NUM_ELEMENTS;
  targ_jmp_buf_elements_are_float = TARG_JMP_BUF_ELEMENTS_ARE_FLOAT;
  targ_jmp_buf_element_int_kind = TARG_JMP_BUF_ELEMENT_INT_KIND;
  targ_jmp_buf_element_float_kind = TARG_JMP_BUF_ELEMENT_FLOAT_KIND;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
  targ_var_handle_int_kind = TARG_VAR_HANDLE_INT_KIND;
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  targ_flt_mant_dig = TARG_FLT_MANT_DIG;
  targ_flt_min_exp = TARG_FLT_MIN_EXP;
  targ_flt_max_exp = TARG_FLT_MAX_EXP;
  targ_dbl_mant_dig = TARG_DBL_MANT_DIG;
  targ_dbl_min_exp = TARG_DBL_MIN_EXP;
  targ_dbl_max_exp = TARG_DBL_MAX_EXP;
  targ_ldbl_mant_dig = TARG_LDBL_MANT_DIG;
  targ_ldbl_min_exp = TARG_LDBL_MIN_EXP;
  targ_ldbl_max_exp = TARG_LDBL_MAX_EXP;
  remove_qualifiers_from_param_types =
                                    DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES;
  c_and_cpp_function_types_are_distinct =
                                 DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT;
#if BACK_END_IS_CP_GEN_BE
  old_specializations_for_generated_instances =
                           DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES;
#endif /* BACK_END_IS_CP_GEN_BE */
  type_info_in_namespace_std = DEFAULT_TYPE_INFO_IN_NAMESPACE_STD;
  pass_stdarg_references_to_generated_code =
                              DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE;
  va_list_in_std_namespace = DEFAULT_VA_LIST_IN_STD_NAMESPACE;
  va_list_using_using_decl_in_std_namespace = FALSE;
  instantiate_extern_inline = INSTANTIATE_EXTERN_INLINE;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  sun_is_generated_code_target = SUN_IS_GENERATED_CODE_TARGET;
  clang_is_generated_code_target = CLANG_IS_GENERATED_CODE_TARGET;
  gcc_is_generated_code_target = GCC_IS_GENERATED_CODE_TARGET;
  gcc_or_clang_is_generated_code_target = gcc_is_generated_code_target ||
                                          clang_is_generated_code_target;
#if GCC_IS_GENERATED_CODE_TARGET || \
    (BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT)
  gnu_target_version_number = GNU_TARGET_VERSION_NUMBER;
#endif /* GCC_IS_GENERATED_CODE_TARGET || ... */
#if CLANG_IS_GENERATED_CODE_TARGET
  clang_target_version_number = CLANG_TARGET_VERSION_NUMBER;
#endif /* CLANG_IS_GENERATED_CODE_TARGET */
#ifdef SUN_TARGET_VERSION_NUMBER
  sun_target_version_number = SUN_TARGET_VERSION_NUMBER;
#endif /* ifdef SUN_TARGET_VERSION_NUMBER */
  gcc_builtin_varargs_in_generated_code =
                                         GCC_BUILTIN_VARARGS_IN_GENERATED_CODE;
#if BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
  /* Will be set to MSVC_IS_GENERATED_CODE_TARGET if the source dialect is
     Microsoft mode. */
  msvc_is_generated_code_target = FALSE;
#else /* !(BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT) */
  msvc_is_generated_code_target = MSVC_IS_GENERATED_CODE_TARGET;
#endif /* BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
  msvc_target_version_number = MSVC_TARGET_VERSION_NUMBER;
  microsoft_dialect_is_generated_code_target =
                                    MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET;
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
  cp_gen_be_target_matches_source_dialect =
                                       CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT;
#endif /* BACK_END_IS_CP_GEN_BE */
#if !IA64_ABI
  targ_runtime_elem_count_int_kind = TARG_RUNTIME_ELEM_COUNT_INT_KIND;
#endif /* !IA64_ABI */
#if BACK_END_IS_C_GEN_BE
  use_empty_struct_in_generated_c = USE_EMPTY_STRUCT_IN_GENERATED_C;
#endif /* BACK_END_IS_C_GEN_BE */
  auxiliary_info_dir_name = (char *)EDG_AUXILIARY_INFO_DIR_NAME;
#if DO_IL_LOWERING
  targ_delta_int_kind = TARG_DELTA_INT_KIND;
  targ_virtual_function_index_int_kind = TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND;
#if IA64_ABI
  targ_ia64_vtable_entry_int_kind = TARG_IA64_VTABLE_ENTRY_INT_KIND;
#endif /* IA64_ABI */
#if GENERATE_EH_TABLES
  targ_region_number_int_kind = TARG_REGION_NUMBER_INT_KIND;
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */
  targ_sizeof_largest_integer = TARG_SIZEOF_LARGEST_INTEGER;
#if FIXED_POINT_ALLOWED
  targ_sizeof_largest_fixed_point = TARG_SIZEOF_LARGEST_FIXED_POINT;
#endif /* FIXED_POINT_ALLOWED */
}  /* target_early_init */


void target_one_time_init(void)
/*
Do one-time initialization of variables related to the target.  This is
executed once after command-line processing, and not again for each source
file.  Called after target configuration (if any) has been determined.
*/
{
  /* Record character sizes in an array that can be indexed by character
     kind. */
  init_character_sizes();
  character_size[(int)chk_char] = 1;
  character_size[(int)chk_wchar_t] = targ_sizeof_wchar_t;
  character_size[(int)chk_char16_t] = targ_sizeof_char16_t;
  character_size[(int)chk_char32_t] = targ_sizeof_char32_t;
#if CHECKING
  check_target_configuration();
#endif /* CHECKING */
}  /* target_one_time_init */


void target_init(void)
/*
Initialize target machine characteristics.  This is the per-compilation
initialization and must be done after command-line processing.
*/
{
  /* The signedness of characters can be set on the command line. */
  set_plain_char_int_kind(targ_has_signed_chars);
  /* Set the element of int_kind_is_signed that corresponds to "plain"
     char. */
  int_kind_is_signed[(int)ik_char] = targ_has_signed_chars;
#if CHECKING && !USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS
  /* Check that int_kind_is_signed is correctly initialized.  This
     guards against someone changing the enumeration and forgetting to update
     the initialization. */
  if (int_kind_is_signed[(int)ik_last] != 111) {
    internal_error(
           "target_init: initialization of int_kind_is_signed is not correct");
  }  /* if */
#endif /* CHECKING && !USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */
  /* String literals are shared, except in pcc mode and Microsoft mode.  (pcc
     and Microsoft C allow string literals to be overwritten.) */
  string_literals_shared = (C_dialect != C_dialect_pcc && !microsoft_mode);
  /* Determine the integer kind for the largest integer types. */
#if LONG_LONG_ALLOWED
  targ_intmax_kind = (an_integer_kind)ik_long_long;
  targ_uintmax_kind = (an_integer_kind)ik_unsigned_long_long;
#else /* !LONG_LONG_ALLOWED */
  targ_intmax_kind = (an_integer_kind)ik_long;
  targ_uintmax_kind = (an_integer_kind)ik_unsigned_long;
#endif /* LONG_LONG_ALLOWED */
  /* Determine the maximum size of a class object. */
  if (targ_max_class_object_size == 0) {
    targ_max_class_object_size = targ_size_t_max;
  }  /* if */
  /* Determine the maximum base class offset. */
  if (targ_max_base_class_offset == 0) {
    targ_max_base_class_offset = targ_size_t_max;
#if DO_IL_LOWERING
  } else {
    /* Compute the maximum base class offset value that will fit in the
       delta field of a virtual function table. */
    a_targ_size_t		size;
    a_host_large_unsigned	temp;
    a_targ_alignment		alignment;
    a_host_large_unsigned	bits;

    /* Get the size of whatever integer kind is associated with delta field
       of the virtual function table. */
    get_integer_size_and_alignment(targ_delta_int_kind, &size, &alignment);
    /* Now given the size, compute the maximum integer value it will
       accommodate. */
    bits = size * targ_char_bit;
    if (int_kind_is_signed[targ_delta_int_kind]) bits -= 1;
    temp = ~((~(a_host_large_unsigned)0) << bits);
    if (temp > (a_host_large_unsigned)targ_size_t_max) {
      /* It shouldn't exceed the maximum that can fit in a_targ_size_t. */
      temp = (a_host_large_unsigned)targ_size_t_max;
    }  /* if */
    if (temp >= targ_max_base_class_offset) {
      /* Don't increase the maximum offset beyond what was specified. */
    } else {
      /* Set the maximum offset to the computed value. */
      targ_max_base_class_offset = (a_targ_size_t)temp;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  init_microsoft_sized_int_types();
  /* Determine whether the target is a 64-bit target. */
  { a_targ_size_t     size;
    a_targ_alignment  alignment;
    get_integer_size_and_alignment(targ_size_t_int_kind, &size, &alignment);
    is_64bit_target = (size * targ_char_bit == 64);
  }
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  always_fold_calls_to_builtin_constant_p =
                              DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P;
}  /* target_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
