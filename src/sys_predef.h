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

sys_predef.h -- System dependent predefined macros and assertions.

*/

/* Avoid including these declarations more than once: */
#ifndef SYS_PREDEF_H
#define SYS_PREDEF_H 1

#if BUILTIN_FUNCTIONS_ENABLED

/*
To add a user-defined builtin function, follow these steps:

  - Add a new enumeration value to a_builtin_user_function_kind_tag below.
  - Add a new entry in builtin_user_table below.  See the description for
    a_builtin_user_descr which details the values of each of the fields.
*/

/*
This typedef is used for character strings used to specify a compact encoding
of a condition under which the builtin function should be enabled.  See the
description of the "cond" field in a_builtin_user_descr below for more
information.
*/
typedef a_const_char *a_builtin_condition_string;

/*
This typedef is used for character strings that contain a C representation
of a builtin function's type.  The string is parsed by the front end if the
builtin function is referenced.
*/
typedef a_const_char *a_builtin_type_string;

/*
Describes a user-defined builtin function.
*/
typedef struct a_builtin_user_descr *a_builtin_user_descr_ptr;
typedef struct a_builtin_user_descr {
  a_const_char  *name;
                        /* The name of the builtin function. */
  a_builtin_condition_string
                cond;
                        /* A compact encoding of the condition(s) in which this
                           builtin function is enabled.  The encoding consists
                           of a sequence of conditions, each condition has six
                           potential parts (in the following order):

                             - prefix ('S') [optional]
                             - emulation ('L', 'g', or 'm')
                             - mode ('c', '+', or 'x')
                             - arch ('4' or '8') [optional]
                             - version (version range in parens) [optional]
                             - restrictions ['v', 'i'] [optional]

                           A prefix of 'S' indicates that the name in the
                           entry (which must start with "__builtin_") also
                           has a secondary declaration with the same type
                           but without the "__builtin_" prefix.

                           The 'L' emulation mode indicates that the function
                           applies to clang mode; 'g' indicates GNU mode, and
                           'm' is for Microsoft emulation mode.

                           A mode of 'c' indicates C mode, '+' indicates
                           C++ mode, and 'x' indicates both C and C++ modes.

                           A '4' indicates the function applies only to
                           architectures where targ_supports_x86_64 is FALSE
                           and an '8' indicates the function applies only to
                           architectures where targ_supports_x86_64 is TRUE.

                           If a parenthesized range of applicable versions is
                           given, the function is only enabled when the version
                           is within that range.  Either end of the range can
                           be dropped; e.g., "gc(40800-)" means the attribute
                           is valid in GNU C mode with gnu_version >= 40800.

                           If restrictions exist, they are a non-empty
                           sequence of 'v' and/or 'i', where 'v' indicates that
                           the signature depends on vector types, and 'i'
                           indicates that the signature depends on 128-bit
                           integer types.  If a function with restrictions is
                           referenced, a check is made to ensure that all
                           restrictions are satisfied (otherwise an error is
                           given). */
  a_builtin_type_string
                type_string;
                        /* The type of the builtin function(s).  This takes
                           the form of a C declaration, e.g., a function
                           taking an int argument and returning a float would
                           be: "float (int)". */
  a_builtin_function_kind
                kind;
                        /* A unique identifier for this builtin function.  It
                           should be an enumeration value from
                           a_builtin_function_kind_tag or
                           a_builtin_user_function_kind_tag. */
} a_builtin_user_descr;

/*
Mapping between the string representation of a builtin function type and its
internal representation.  Entries containing the C representations of each
builtin function's type are automatically generated by an external tool, then,
if referenced, the internal type is generated by invoking the parser on the
string.  Corresponds to the same index in builtin_type_strings.
*/
typedef struct a_builtin_function_type {
  a_type_ptr    type;
                        /* If non-NULL, contains the internal representation
                           of the routine type specified by the corresponding
                           entry in builtin_type_strings.  In cases where the
                           function type has attributes, this can be a
                           tk_typeref (which records those attributes). */
} a_builtin_function_type;

EXTERN a_builtin_function_type
                *builtin_type_table;
                        /* A dynamically-allocated array of
                           a_builtin_function_type entries that
                           corresponds to the type strings in
                           builtin_type_strings. */

/*
Mapping between the string representation of a condition string and its
internal representation.  Entries containing the C representations of each
builtin function's type are automatically generated by an external tool, then,
if referenced, the internal type is generated by invoking the parser on the
string.  Corresponds to the same index in builtin_condition_strings.
*/
typedef struct a_builtin_function_condition {
  a_const_char  *restrictions;
                        /* If non-NULL, points to any "restrictions" for this
                           builtin function.  See the description of "cond" in
                           a_builtin_user_descr for more information. */
  a_boolean     evaluated;
                        /* If TRUE, this condition string has been evaluated
                           (and hence the flags that follow have been set
                           appropriately). */
  a_boolean     primary_enabled;
                        /* If TRUE, this condition string indicates that any
                           primary builtin declarations that use it are enabled
                           in the current configuration. */
  a_boolean     secondary_enabled;
                        /* If TRUE, this condition string indicates that any
                           secondary builtin declarations that use it are
                           enabled in the current configuration. */
} a_builtin_function_condition;

EXTERN a_builtin_function_condition
                *builtin_condition_table;
                        /* A dynamically-allocated array of
                           a_builtin_function_condition entries that
                           corresponds to the condition strings in
                           builtin_condition_strings. */

/*
Data structure describing the name and signature of a builtin function, as well
as the modes in which the builtin function should be recognized.  These entries
are automatically generated by an external tool.
*/
typedef struct a_builtin_descr {
  a_const_char  *name;
                        /* The name of the builtin function. */
  unsigned short
                cond_index;
                        /* An index into builtin_condition_table that gives the
                           condition string for the builtin function.  See the
                           comment for the "cond" field in a_builtin_user_descr
                           for a full explanation. */
  unsigned short
                type_index;
                        /* An index into builtin_type_table that specifies
                           the type of the builtin function(s) for this
                           entry. */
  a_builtin_function_kind
                kind;
                        /* An indicator of which builtin function (for use
                           within the front end).  Note that the same value is
                           used for cases where an entry refers to two symbols
                           (i.e., because an 'S' is present in the condition
                           string indicating that there are primary and
                           secondary versions of the builtin function). */
} a_builtin_descr;

/*
Include the GCC/clang/Microsoft builtins that have been automatically
generated by an external tool.
*/
#include "builtin_defs.h"

/*
An enumeration of unique user-defined builtin functions.  This is effectively
a continuation of the automatically-generated a_builtin_function_kind_tag
enumeration.
*/
enum a_builtin_user_function_kind_tag {
  bufk_first = bfk_last,          /* initial entry */
  bufk_choose_expr,               /* __builtin_choose_expr */
  bufk_last                       /* final entry */
};

/*
This table contains entries for "manually added" builtin functions.  The
builtin functions described in builtin_table are automatically generated by
an external tool; this table should be used for builtin functions that are
not captured by the external tool, or are added by the customer.

Note that any entries in this table are lazily loaded, i.e., they are
loaded into the symbol table for each translation unit, but a routine entry
(and associated symbol) are not created until the builtin function is
referenced.  If the builtin function must be defined each time, a different
mechanism (e.g., calling enter_builtin_function directly) should be used.

Note also that the ordering of this table is arbitrary (i.e., it does not need
to be kept sorted).
*/
/*lint -e641 */ /* Suppress lint messages about converting enums to int. */
EXTERN a_builtin_user_descr builtin_user_table[]
#if VAR_INITIALIZERS
= {
  /* __builtin_choose_expr is available in all gcc modes. */
  { "__builtin_choose_expr", "gc", "int (...)", bufk_choose_expr },

  /* __builtin_va_arg is not picked up by the automatic tools because it
     is implemented as a keyword by GCC and clang. */
  { "__builtin_va_arg", "gx(40500-)Lx", "void (...)", bfk_va_arg },

  /* __builtin_stdarg_start is also not picked up for GCC. */
  { "__builtin_stdarg_start", "gx(40500-)", "void (...)", bfk_stdarg_start },

  /* Manually add the size-specific versions of the __atomic builtins for
     clang.  These entries were copied from the corresponding automatically-
     generated GCC entries. */
  { "__atomic_add_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_add_fetch_1 },
  { "__atomic_add_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_add_fetch_16 },
  { "__atomic_add_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_add_fetch_2 },
  { "__atomic_add_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_add_fetch_4 },
  { "__atomic_add_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_add_fetch_8 },
  { "__atomic_add_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_add_fetch_8 },
  { "__atomic_and_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_and_fetch_1 },
  { "__atomic_and_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_and_fetch_16 },
  { "__atomic_and_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_and_fetch_2 },
  { "__atomic_and_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_and_fetch_4 },
  { "__atomic_and_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_and_fetch_8 },
  { "__atomic_and_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_and_fetch_8 },
  { "__atomic_compare_exchange_1", "Lx", "__edg_bool_type__ (volatile void*,void*,unsigned char,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_1 },
  { "__atomic_compare_exchange_16", "Lx[i]", "__edg_bool_type__ (volatile void*,void*,__uint128_t,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_16 },
  { "__atomic_compare_exchange_2", "Lx", "__edg_bool_type__ (volatile void*,void*,unsigned short,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_2 },
  { "__atomic_compare_exchange_4", "Lx", "__edg_bool_type__ (volatile void*,void*,unsigned,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_4 },
  { "__atomic_compare_exchange_8", "Lx4", "__edg_bool_type__ (volatile void*,void*,unsigned long long,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_8 },
  { "__atomic_compare_exchange_8", "Lx8", "__edg_bool_type__ (volatile void*,void*,unsigned long,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_8 },
  { "__atomic_exchange_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_exchange_1 },
  { "__atomic_exchange_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_exchange_16 },
  { "__atomic_exchange_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_exchange_2 },
  { "__atomic_exchange_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_exchange_4 },
  { "__atomic_exchange_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_exchange_8 },
  { "__atomic_exchange_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_exchange_8 },
  { "__atomic_fetch_add_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_add_1 },
  { "__atomic_fetch_add_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_add_16 },
  { "__atomic_fetch_add_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_add_2 },
  { "__atomic_fetch_add_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_add_4 },
  { "__atomic_fetch_add_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_add_8 },
  { "__atomic_fetch_add_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_add_8 },
  { "__atomic_fetch_and_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_and_1 },
  { "__atomic_fetch_and_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_and_16 },
  { "__atomic_fetch_and_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_and_2 },
  { "__atomic_fetch_and_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_and_4 },
  { "__atomic_fetch_and_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_and_8 },
  { "__atomic_fetch_and_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_and_8 },
  { "__atomic_fetch_nand_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_nand_1 },
  { "__atomic_fetch_nand_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_nand_16 },
  { "__atomic_fetch_nand_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_nand_2 },
  { "__atomic_fetch_nand_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_nand_4 },
  { "__atomic_fetch_nand_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_nand_8 },
  { "__atomic_fetch_nand_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_nand_8 },
  { "__atomic_fetch_or_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_or_1 },
  { "__atomic_fetch_or_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_or_16 },
  { "__atomic_fetch_or_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_or_2 },
  { "__atomic_fetch_or_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_or_4 },
  { "__atomic_fetch_or_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_or_8 },
  { "__atomic_fetch_or_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_or_8 },
  { "__atomic_fetch_sub_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_sub_1 },
  { "__atomic_fetch_sub_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_sub_16 },
  { "__atomic_fetch_sub_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_sub_2 },
  { "__atomic_fetch_sub_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_sub_4 },
  { "__atomic_fetch_sub_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_sub_8 },
  { "__atomic_fetch_sub_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_sub_8 },
  { "__atomic_fetch_xor_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_xor_1 },
  { "__atomic_fetch_xor_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_xor_16 },
  { "__atomic_fetch_xor_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_xor_2 },
  { "__atomic_fetch_xor_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_xor_4 },
  { "__atomic_fetch_xor_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_xor_8 },
  { "__atomic_fetch_xor_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_xor_8 },
  { "__atomic_load_1", "Lx", "unsigned char (const volatile void*,int)", bfk_atomic_load_1 },
  { "__atomic_load_16", "Lx[i]", "__uint128_t (const volatile void*,int)", bfk_atomic_load_16 },
  { "__atomic_load_2", "Lx", "unsigned short (const volatile void*,int)", bfk_atomic_load_2 },
  { "__atomic_load_4", "Lx", "unsigned (const volatile void*,int)", bfk_atomic_load_4 },
  { "__atomic_load_8", "Lx4", "unsigned long long (const volatile void*,int)", bfk_atomic_load_8 },
  { "__atomic_load_8", "Lx8", "unsigned long (const volatile void*,int)", bfk_atomic_load_8 },
  { "__atomic_nand_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_nand_fetch_1 },
  { "__atomic_nand_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_nand_fetch_16 },
  { "__atomic_nand_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_nand_fetch_2 },
  { "__atomic_nand_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_nand_fetch_4 },
  { "__atomic_nand_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_nand_fetch_8 },
  { "__atomic_nand_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_nand_fetch_8 },
  { "__atomic_or_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_or_fetch_1 },
  { "__atomic_or_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_or_fetch_16 },
  { "__atomic_or_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_or_fetch_2 },
  { "__atomic_or_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_or_fetch_4 },
  { "__atomic_or_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_or_fetch_8 },
  { "__atomic_or_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_or_fetch_8 },
  { "__atomic_store_1", "Lx", "void (volatile void*,unsigned char,int)", bfk_atomic_store_1 },
  { "__atomic_store_16", "Lx[i]", "void (volatile void*,__uint128_t,int)", bfk_atomic_store_16 },
  { "__atomic_store_2", "Lx", "void (volatile void*,unsigned short,int)", bfk_atomic_store_2 },
  { "__atomic_store_4", "Lx", "void (volatile void*,unsigned,int)", bfk_atomic_store_4 },
  { "__atomic_store_8", "Lx4", "void (volatile void*,unsigned long long,int)", bfk_atomic_store_8 },
  { "__atomic_store_8", "Lx8", "void (volatile void*,unsigned long,int)", bfk_atomic_store_8 },
  { "__atomic_sub_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_sub_fetch_1 },
  { "__atomic_sub_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_sub_fetch_16 },
  { "__atomic_sub_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_sub_fetch_2 },
  { "__atomic_sub_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_sub_fetch_4 },
  { "__atomic_sub_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_sub_fetch_8 },
  { "__atomic_sub_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_sub_fetch_8 },
  { "__atomic_xor_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_xor_fetch_1 },
  { "__atomic_xor_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_xor_fetch_16 },
  { "__atomic_xor_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_xor_fetch_2 },
  { "__atomic_xor_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_xor_fetch_4 },
  { "__atomic_xor_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_xor_fetch_8 },
  { "__atomic_xor_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_xor_fetch_8 },

#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
  /* Clang doesn't define these (though GCC does).  When performing function
     multiversioning lowering, they are required (and must therefore be
     implemented by a back end in clang mode). */
  { "__builtin_cpu_init", "Lx", "int (void)", bfk_cpu_init },
  { "__builtin_cpu_is", "Lx", "int (const char*)", bfk_cpu_is },
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */

  { NULL, NULL, 0, bfk_none }   /* end of table marker */
}
#endif /* VAR_INITIALIZERS */
;
/*lint +e641 */ /* Re-enable lint messages about converting enums to int. */
extern void load_matching_builtin_function(a_symbol_header *sym_hdr);

extern void load_matching_builtin_function_by_name(a_const_char *name);

extern a_boolean builtin_function_is_enabled(a_const_char *name);

extern a_boolean builtin_needs_to_be_loaded_in_secondary_translation_unit
                                                    (a_symbol_header *sym_hdr);

#endif /* BUILTIN_FUNCTIONS_ENABLED */

extern a_type_ptr get_default_va_list_type(void);

extern void enter_system_specific_predeclared_symbols(void);

extern void enter_system_specific_predefined_macros_and_assertions(void);

#if GNU_EXTENSIONS_ALLOWED
#if USE_X86_FUNCTION_MULTIVERSIONING
/*
This enumeration lists the valid CPU and Instruction Set Architectures (ISAs)
for the Intel/AMD line of processors and is used for the GNU function
multiversioning feature (as specified by the "target" attribute).  The ordering
of the list is important: CPU architectures are first, then "default", then the
ISA architectures (in the order specified in the GCC Function Multiversioning
Wiki).  When an entry is added here, the target_distinction table must also be
updated.  If an ISA entry is added here, an entry must also be added to
isa_alphabetic_order.

The "target" attributes listed below correspond to those that GCC appears
to use in its resolver functions.  All other "target" attributes are mapped
to mvak_unknown and a warning is issued (which is suppressed in system
headers).  An error is issued if an mvak_unknown routine is needed by a
lowering-created resolver routine.
*/
enum a_multiversion_arch_kind_tag {
  mvak_invalid = -1,                /* An invalid entry. */
  mvak_unknown = 0,                 /* Not otherwise on this list. */
  mvak_lowest_cpu,                  /* Lowest CPU architecture entry. */
  /* CPU architectures: */
  mvak_cpu_bdver1 = mvak_lowest_cpu,
  mvak_cpu_bdver2,
  mvak_cpu_corei7,
  mvak_cpu_amdfam10h,
  mvak_cpu_core2,
  mvak_cpu_atom,
  mvak_highest_cpu = mvak_cpu_atom, /* Highest CPU architecture entry. */
  mvak_default_target,              /* Default entry.*/
  mvak_lowest_isa,                  /* Marks first ISA entry. */
  /* ISA architectures: */
  mvak_isa_mmx = mvak_lowest_isa,
  mvak_isa_sse,
  mvak_isa_sse2,
  mvak_isa_sse3,
  mvak_isa_ssse3,
  mvak_isa_sse4,
  mvak_isa_sse4a,
  mvak_isa_sse4_1,
  mvak_isa_sse4_2,
  mvak_isa_popcnt,
  mvak_isa_aes,
  mvak_isa_pclmul,
  mvak_isa_avx,
  mvak_isa_bmi,
  mvak_isa_fma4,
  mvak_isa_xop,
  mvak_isa_fma,
  mvak_isa_bmi2,
  mvak_isa_avx2,
  mvak_isa_avx512f,
  mvak_highest_isa = mvak_isa_avx512f, /* Marks last ISA entry. */
  mvak_last                         /* Must be last. */
};

/* Type to hold an a_target_architecture enumeration value. */
typedef signed char a_multiversion_arch_kind;

/*
Macro that returns TRUE if the specified architecture corresponds to a
CPU architecture.
*/
#define is_mv_cpu_arch(t)                                                     \
  ((t) >= (a_multiversion_arch_kind)mvak_lowest_cpu &&                        \
   (t) <= (a_multiversion_arch_kind)mvak_highest_cpu)

/*
Macro that returns TRUE if a CPU architecture is specified in a bitset.
*/
#define is_any_mv_arch_bit_set(bs)                                            \
  (((bs) & (((a_mv_target_bitset)1 <<                                         \
             ((a_multiversion_arch_kind)mvak_highest_cpu + 1)) - 1)) != 0)

/*
Macro that returns TRUE if the specified bitset indicates the "default"
routine.
*/
#define is_default_targ_bitset(bs)                                            \
  ((bs) == (a_mv_target_bitset)1 <<                                           \
           (a_multiversion_arch_kind)mvak_default_target)

/*
Macro that returns TRUE if the specified bitset indicates an unknown target
attribute.
*/
#define is_unknown_targ_bitset(bs)                                            \
  ((bs) & (a_mv_target_bitset)1 <<                                           \
          (a_multiversion_arch_kind)mvak_unknown)

/*
Macro that returns TRUE if the specific-target routine is the "default"
routine.
*/
#define is_mv_default_routine(rp)                                             \
 (has_gnu_routine_supp(rp) &&                                                 \
  is_default_targ_bitset(                                                     \
                 (rp)->gnu_extra_info->mv_info.targeted_version.target_bitset))

/*
Macro that takes a representative routine and returns TRUE in the special
case when there is exactly one target-specific routine on the list.
*/
#define has_exactly_one_target_specific_routine(rp)                           \
  (has_gnu_routine_supp(rp) &&                                                \
   (rp)->gnu_extra_info->mv_info.representative.targeted_versions != NULL &&  \
   (rp)->gnu_extra_info->mv_info.representative.targeted_versions->next== NULL)

/*
Macro that takes a representative routine and returns TRUE if there is
a "default" routine on the list (which will be the first routine on the list).
*/
#define has_mv_default_routine(rp)                                            \
  (has_gnu_routine_supp(rp) &&                                                \
   (rp)->gnu_extra_info->mv_info.representative.targeted_versions != NULL &&  \
   is_mv_default_routine((rp)->gnu_extra_info->                               \
                            mv_info.representative.targeted_versions->routine))

#if DO_IL_LOWERING
extern a_const_char *target_name_for_builtin(a_multiversion_arch_kind arch);
#endif /* DO_IL_LOWERING */

extern void reference_to_mv_routine(a_routine_ptr      routine,
                                    a_source_position  *error_pos);

extern a_routine_ptr find_mv_target_specific_routine(
                                            a_routine_ptr routine,
                                            a_routine_ptr surrounding_routine);

#endif /* USE_X86_FUNCTION_MULTIVERSIONING */

#if GNU_FUNCTION_MULTIVERSIONING
extern a_routine_ptr find_existing_mv_routine(
                                       a_routine_ptr representative_routine,
                                       a_routine_ptr candidate,
                                       an_attribute_arg_ptr aap);

extern void add_to_specific_version_list(a_routine_ptr representative_routine,
                                         a_routine_ptr target_routine);

extern a_const_char *target_specific_distinction(a_routine_ptr routine);
#endif /* GNU_FUNCTION_MULTIVERSIONING */

extern void validate_target_argument(a_const_char         *str,
                                     size_t               str_len,
                                     an_attribute_arg_ptr aap,
                                     a_routine_ptr        routine,
                                     a_boolean            *error_issued);

#endif /* GNU_EXTENSIONS_ALLOWED */

extern void sys_predef_trans_unit_init(void);

extern void sys_predef_one_time_init(void);

#endif /* ifndef SYS_PREDEF_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
