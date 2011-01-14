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

#if READ_CPPCLI_PORTABLE_ASSEMBLIES || WRITE_CPPCLI_PORTABLE_ASSEMBLIES

typedef struct a_portable_assembly_header {
  /* This structure defines the data found at the beginning of a
     portable assembly file.  These fields are each converted to ASCII and
     formatted as %08x in the file.  The header is terminated with a
     newline. */
#define PORTABLE_ASSEMBLY_HEADER_FORMAT "%08x %08x %08x\n"
#define PORTABLE_ASSEMBLY_MAGIC_NUMBER  0x11223344
  uint32_t      magic;  /* Identifying "magic" number for portable assembly
                           files. */
  uint32_t      num_entries;
                        /* The number of a_portable_assembly_table_entrys
                           this file contains. */
  uint32_t      table_offset;
                        /* An offset (from the beginning of the file) to the
                           a_portable_assembly_table_entry table. */
  /* This header is followed by the string data, then the table. */
} a_portable_assembly_header;

typedef struct a_portable_assembly_table_entry {
  /* Each entry in this table represents the metadata associated with a
     particular C++/CLI metadata token.  These fields are each converted to
     ASCII and formatted as %08x in the file.  Each entry is terminated
     with a newline. */
#define PORTABLE_ASSEMBLY_TABLE_FORMAT "%08x %08x %08x\n"
  uint32_t      token;  /* The C++/CLI metadata type_def token associated with
                           this entry.  The first entry in this table
                           (token == 0) refers to the string returned by
                           import_all_types. */
  uint32_t      offset; /* Offset (from the beginning of the file) to the
                           metadata string associated with token. */
  uint32_t      size;   /* Size (in bytes) of the associated metadata. */
} a_portable_assembly_table_entry;

#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES || WRITE_CPPCLI_PORTABLE_ASSEMBLIES*/

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
