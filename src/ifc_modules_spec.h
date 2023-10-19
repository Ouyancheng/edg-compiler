/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_modules_spec.h -- Explicit template specializations needed at link time by
                      ifc_modules.c and ifc_map_functions.c.

** NOTICE: This file is produced by an external script. **

While EDG staff should update the generation script rather than manually
editing this file, customers are welcome to modify this file and create patches
as they see fit.

Please contact EDG Support if you would be interested in using, or learning
more about, the tool that generated this file.
*/

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


/*
Explicit specializations of functions for ExprNamedDeclOffset.
*/
SPEC_OFFSET_PARTITION_KIND(an_ifc_expr_named_decl_offset)
SPEC_OFFSET_PARTITION_INDEX(an_ifc_expr_named_decl_offset)


/*
Explicit specializations of functions for FormSpecOffset.
*/
SPEC_OFFSET_PARTITION_KIND(an_ifc_form_spec_offset)
SPEC_OFFSET_PARTITION_INDEX(an_ifc_form_spec_offset)


/*
Explicit specializations of functions for LineOffset.
*/
SPEC_OFFSET_PARTITION_KIND(an_ifc_line_offset)
SPEC_OFFSET_PARTITION_INDEX(an_ifc_line_offset)


/*
Explicit specializations of functions for ScopeOffset.
*/
SPEC_OFFSET_PARTITION_KIND(an_ifc_scope_offset)
SPEC_OFFSET_PARTITION_INDEX(an_ifc_scope_offset)


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
