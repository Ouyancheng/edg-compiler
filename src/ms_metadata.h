/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2010 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

#ifndef MS_METADATA
#define MS_METADATA 1

enum a_cpp_cli_feature_tag {
  cpp_cli_none                   = 0x0000,
  cpp_cli_ref_classes            = 0x0001,
  cpp_cli_value_types            = 0x0002,
  cpp_cli_interfaces             = 0x0004,
  cpp_cli_enumerations           = 0x0008,
  cpp_cli_nested_types           = 0x0010,
  cpp_cli_generic_types          = 0x0020,
  cpp_cli_generic_methods        = 0x0040,
  cpp_cli_delegates              = 0x0080,
  cpp_cli_properties             = 0x0100,
  cpp_cli_events                 = 0x0200,
  cpp_cli_as_friend_assembly     = 0x0400,
  cpp_cli_implementation_details = 0x0800,
  cpp_cli_declspec_assemby_info  = 0x1000,
  cpp_cli_declspec_member_info   = 0x2000,
  cpp_cli_define_all_types       = 0x4000
};


typedef unsigned int
                a_cpp_cli_token;
                        /* A metadata token */
typedef unsigned int
                an_assembly_index;
                        /* An assembly index */

typedef unsigned int a_cpp_cli_feature_set;

EXTERN a_cpp_cli_feature_set 
                edg_supported_features;
                        /* Current features supported by EDG. */

extern an_assembly_index import_metadata_file(
                          char                  *assembly_full_name,
                          a_cpp_cli_feature_set supported_features,
                          a_boolean             *is_duplicate);
extern void import_all_types(an_assembly_index assembly_index,
                             char              *buffer,
                             size_t            *buffer_size);
extern void import_class_definition(an_assembly_index assembly_index,
                                    a_cpp_cli_token   metadata_type_def_token,
                                    char              *buffer,
                                    size_t            *buffer_size);
extern void ms_metadata_trans_unit_init(char *trans_unit_file_name);
extern void ms_metadata_trans_unit_wrapup(void);
extern void ms_metadata_cleanup(void);
#endif /* MS_METADATA */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2010 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
