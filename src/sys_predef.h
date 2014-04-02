/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

sys_predef.h -- System dependent predefined macros and assertions.

*/

/* Avoid including these declarations more than once: */
#ifndef SYS_PREDEF_H
#define SYS_PREDEF_H 1

extern a_type_ptr get_default_va_list_type(void);

extern void enter_system_specific_predeclared_symbols(void);

extern void enter_system_specific_predefined_macros_and_assertions(void);

#if GNU_EXTENSIONS_ALLOWED
#if USE_X86_FUNCTION_MULTIVERSIONING
/* FIXME: clean this up as much as possible. */
/*
Support for the GNU multiversioning feature.
This enumeration defines the allowable target machines within the
GNU target attribute.
The enumeration type is a mixture of CPU architectures and Instruction Set
Architectures (ISAs).
The CPU architecture enumerators come first, then "default",
then the isa enumerators: which are ordered from least sophisticated
to most sophisticated.
There are 2 string tables that need to be kept in synch with this enumeration:
         mv_arch_name_string
         mv_arch_name
         target_attribute_map
Note that these entries are specific to the Intel/AMD line of processors,
but the mechanism may be used as the basis for other types of processors as
well.
*/
typedef enum a_mv_arch_isa {
  mv_invalid = -1,                /* An invalid entry. */
  mv_lowest_arch = 0,             /* Lowest CPU architecture entry. */
  mv_arch_bdver1 = mv_lowest_arch,
  mv_arch_bdver2,
  mv_arch_corei7,
  mv_arch_amdfam10h,
  mv_arch_core2,
  mv_arch_atom,
  mv_highest_arch = mv_arch_atom, /* Highest CPU architecture entry. */
  mv_default_target,              /* Default entry (delineates CPU arch/ISA).*/
  mv_lowest_isa,                  /* Marks first ISA entry. */
  mv_isa_mmx = mv_lowest_isa,
  mv_isa_sse,
  mv_isa_sse2,
  mv_isa_sse3,
  mv_isa_ssse3,
  mv_isa_sse4_1,
  mv_isa_sse4_2,
  mv_isa_popcnt,
  mv_isa_avx,
  mv_isa_avx2,
  mv_highest_isa = mv_isa_avx2,   /* Marks last ISA entry. */
  mv_last                         /* Must be last. */
} a_mv_arch_isa;

typedef a_byte a_mv_arch_isa_kind;

extern a_const_char *source_mv_isa_arch_name(int idx);

extern int mv_display_count(void);

extern int mv_display_order(int i);

/* Check if the target enumerator is a CPU architecture. */
#define is_mv_arch(t) ((t) >= 0 && (t) <= mv_highest_arch)
/* Mask that has a bit set for every CPU architecture. */
#define MV_CPU_ARCH_MASK ((1 << ((int)mv_highest_arch + 1)) - 1)
/* Check if a CPU architecture is specified in a bitset. */
#define is_any_mv_arch_bit_set(bs) (((bs) & MV_CPU_ARCH_MASK) != 0)
/* Check if the target bitset is the GNU multiversion "default" target. */
#define is_default_targ_bitset(bs) ((bs) == 1 << mv_default_target)
/* Check if the routine is the "default" routine. */
#define is_mv_default_routine(routine) \
 (has_gnu_routine_supp(routine) && \
  is_default_targ_bitset( \
            (routine)->gnu_extra_info->mv_info.targeted_version.target_bitset))
/*
Macro that takes a representative routine and returns TRUE in the special
case when there is exactly one target-specific routine on the list.
*/
#define has_exactly_one_target_specific_routine(routine) \
  (has_gnu_routine_supp(routine) && \
   (routine)->gnu_extra_info->mv_info.representative.targeted_versions \
                                                                   != NULL && \
   (routine)->gnu_extra_info->mv_info.representative.targeted_versions->next \
                                                                      == NULL)

extern void reference_to_mv_routine(a_routine_ptr      routine,
                                    a_source_position  *error_pos);

extern a_routine_ptr find_mv_target_specific_routine(
                                            a_routine_ptr routine,
                                            a_routine_ptr surrounding_routine);

extern a_routine_ptr get_mv_default_routine(a_routine_ptr routine);

#endif /* USE_X86_FUNCTION_MULTIVERSIONING */

#if GNU_FUNCTION_MULTIVERSIONING
extern a_routine_ptr find_existing_mv_routine(
                                       a_routine_ptr representative_routine,
                                       a_routine_ptr candidate);

extern void add_to_specific_version_list(a_routine_ptr representative_routine,
                                         a_routine_ptr target_routine);

extern a_const_char *mangled_mv_identifier_for_routine(a_routine_ptr routine);
#endif /* GNU_FUNCTION_MULTIVERSIONING */

extern void validate_target_argument(a_const_char         *str,
                                     size_t               str_len,
                                     an_attribute_arg_ptr aap,
                                     a_routine_ptr        routine,
                                     a_boolean            *error_issued);

#endif /* GNU_EXTENSIONS_ALLOWED */

extern void sys_predef_one_time_init(void);

#endif /* ifndef SYS_PREDEF_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
