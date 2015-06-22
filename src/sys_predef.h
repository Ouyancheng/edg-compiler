/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
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
/*
This enumeration lists the valid CPU and Instruction Set Architectures (ISAs)
for the Intel/AMD line of processors and is used for the GNU function
multiversioning feature (as specified by the "target" attribute).  The ordering
of the list is important: CPU architectures are first, then "default", then the
ISA architectures (in the order specified in the GCC Function Multiversioning
Wiki).  When an entry is added here, the target_distinction table must also be
updated.  If an ISA entry is added here, an entry must also be added to
isa_alphabetic_order.
*/
enum a_multiversion_arch_kind_tag {
  mvak_invalid = -1,                /* An invalid entry. */
  mvak_lowest_cpu = 0,              /* Lowest CPU architecture entry. */
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
  mvak_isa_sse4_1,
  mvak_isa_sse4_2,
  mvak_isa_popcnt,
  mvak_isa_avx,
  mvak_isa_avx2,
  mvak_highest_isa = mvak_isa_avx2, /* Marks last ISA entry. */
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
  (((bs) & ((1 << ((a_multiversion_arch_kind)mvak_highest_cpu + 1)) - 1)) != 0)

/*
Macro that returns TRUE if the specified bitset indicates the "default"
routine.
*/
#define is_default_targ_bitset(bs)                                            \
  ((bs) == 1 << (a_multiversion_arch_kind)mvak_default_target)

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
                                       a_routine_ptr candidate);

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

extern void sys_predef_one_time_init(void);

#endif /* ifndef SYS_PREDEF_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
