/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_file.h -- Definitions related to the intermediate language file.

*/

/* Avoid including these declarations more than once: */
#ifndef IL_FILE_H
#define IL_FILE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

#if IL_SHOULD_BE_WRITTEN_TO_FILE

/* String written at the start of an il file.  This is checked on the
   reading end.  The current IL version number is inserted in place of the
   "%s". */
#define IL_FILE_MAGIC_STRING "EDG IL file (v%s)\f\f"
/* The length of the filled-in magic string (with a terminating null) is
   the length of the string here minus two for "%s", plus the length of
   the version number, minus one for its terminating null. */
#define LEN_IL_FILE_MAGIC_STRING \
  (sizeof(IL_FILE_MAGIC_STRING) - 2 + sizeof(IL_VERSION_NUMBER) - 1)
                                

typedef long	a_file_position;
			/* Position in a file; type of value returned by ftell
			   and accepted by fseek. */
EXTERN a_file_position
		*index_for_il_file /* = NULL */;
			/* Parallel array to mem_region_table.
			   index_for_il_file[i] contains the file offset of
			   region i in the file, or 0 if the region has not
			   yet been written. */

#if ALTERNATE_IL_FILE_FORMAT
/* Array giving, for each IL entry kind, the size of the entry in bytes.
   For string type entries, 1.  This must match the order of the
   enumeration an_il_entry_kind. */
/* Following is used to check that the initialization is done correctly;
   its value should be something unlikely to be sizeof() some IL entry. */
#define IEK_LAST_CHECK_SIZE 9999
EXTERN sizeof_t	sizeof_il_entry[(int)iek_last+1]
#if VAR_INITIALIZERS
= {
  0 /* iek_none */,
  sizeof(a_source_file),
  sizeof(a_constant),
  sizeof(a_param_type),
  sizeof(a_routine_type_supplement),
  sizeof(a_based_type_list_member),
  sizeof(a_type),
  sizeof(a_variable),
#ifdef CIL
  sizeof(a_field),
  sizeof(a_throw_specification),
  sizeof(a_throw_spec_type),
#endif /* ifdef CIL */
  sizeof(a_routine),
  sizeof(a_label),
  sizeof(an_expr_node),
#ifdef CIL
  sizeof(a_for_loop),
  sizeof(a_switch_clause),
  sizeof(a_handler),
#endif /* ifdef CIL */
  sizeof(a_block),
  sizeof(a_statement),
  sizeof(a_scope),
  1 /* iek_id_name */,
  1 /* iek_string_text */,
  1 /* iek_other_text */,
#ifdef FIL
  sizeof(an_internal_complex_value),
  sizeof(a_bound_info_entry),
  sizeof(a_do_loop),
  sizeof(a_label_list_entry),
  sizeof(an_io_specifier),
  sizeof(an_io_list_item),
  sizeof(a_namelist_group_member),
  sizeof(a_namelist_group),
  sizeof(an_input_output_description),
  sizeof(an_entry_param),
  sizeof(an_entry_description),
#endif /* ifdef FIL */
#ifdef CIL
  sizeof(a_dynamic_init),
  sizeof(an_access_adjustment),
  sizeof(an_overriding_virtual_function),
  sizeof(a_derivation_step),
  sizeof(a_base_class_derivation),
  sizeof(a_base_class),
  sizeof(a_class_list_entry),
  sizeof(a_routine_list_entry),
  sizeof(a_class_type_supplement),
  sizeof(a_constructor_init),
  sizeof(an_asm_entry),
  sizeof(a_template_arg),
  sizeof(a_new_delete_supplement),
  sizeof(a_throw_supplement),
  sizeof(an_accessible_base_class),
#endif /* ifdef CIL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sizeof(a_source_sequence_entry),
  sizeof(a_src_seq_secondary_decl),
#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
  sizeof(a_comment),
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  sizeof(an_orphaned_il_list),
  IEK_LAST_CHECK_SIZE /* iek_last */
}
#endif /* VAR_INITIALIZERS */
;
#endif /* ALTERNATE_IL_FILE_FORMAT */

#if ALTERNATE_IL_FILE_FORMAT
/*
In the alternate file format, pointers in the IL entries are replaced by
integers that encode the entry number.  This type is the container for
the encoded form; it is converted to "char *" when stored in memory.
*/
typedef unsigned long
		an_encoded_entry_number;
/*
Tag bit used to indicate a function-scope entry number instead of a
file-scope entry number in an encoded IL entry number.
*/
#define FUNC_ENTRY_NUMBER_BIT ((unsigned long)LONG_MAX+1)
#endif /* ALTERNATE_IL_FILE_FORMAT */

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#endif /* ifndef IL_FILE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
