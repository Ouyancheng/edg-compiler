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

func_def.h -- Declarations related to func_def.c (having to do with
              processing for function definitions).

*/

#ifndef FUNC_DEF_H
#define FUNC_DEF_H 1

#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */

/* Constants defining bits in the input bit vector used in calls to
   scan_function_body. */
#define SFB_NO_FLAGS (a_decl_flag_set)(0x0)
#define SFB_IMPLICITLY_DECLARED_RETURN_TYPE (a_decl_flag_set)(0x1)
			/* If this bit is set the return type was not
			   explicitly declared (and is "int" by default). */
#define SFB_NO_CLASS_REACTIVATION (a_decl_flag_set)(0x2)
			/* If this bit is set the scope for the parent
			   class of a member function has already been
			   reactivated. */
#define SFB_NEW_STRUCT_STMT_STACK_REQUIRED (a_decl_flag_set)(0x4)
			/* If this bit is set the function definition may be
			   within a statement context -- e.g., an inline
			   member function of a local class or an inline
			   template function being instantiated "on demand".
			   In such cases the structured statement stack should
			   be reinitialized, and then restored once the
			   function definition is complete. */
#define SFB_IS_INSTANTIATION (a_decl_flag_set)(0x8)
			/* If this bit is set the definition is being generated
			   by the compiler based on a template. */

extern void scan_function_body(a_routine_ptr      rout_ptr,
                               a_func_info_block  *func_info,
                               a_decl_flag_set    flags);

extern a_boolean check_function_return_type(a_type_ptr         return_type,
                                            a_source_position  *err_pos,
                                            a_boolean          is_expr_use);

extern
a_symbol_ptr function_definition(a_symbol_locator  *locator,
                                 a_type_ptr        rout_type,
                                 a_func_info_block *func_info,
                                 a_storage_class   storage_class,
                                 a_boolean         has_explicit_type_spec,
                                 a_decl_modifier   decl_modifiers);

extern void force_definition_of_compiler_generated_routine(a_routine_ptr rp);

extern void generate_required_virtual_destructor_bodies(a_type_ptr types_list);

#if ASM_FUNCTION_ALLOWED
extern void copy_from_source_to_asm_func_buffer(char *stop_char,
                                                char *after_comment_stop_char);
#endif /* ASM_FUNCTION_ALLOWED */

#endif /* FUNC_DEF_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
