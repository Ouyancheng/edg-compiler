/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

Declarations for EDG template prelink utility.

*/


/* The following flag indicates whether all external names have an additional
   underscore at the beginning. */
#define UNDERSCORE_PREFIX TRUE

/* Type code output by "nm" for externally visible function definitions. */
#define EXTERN_TYPE 'T'

/* Suffix to be used for the instantiation information file. */
#define INSTANTIATION_INFO_SUFFIX ".ii"

/* Special mangled name prefixes used by the prelinker. */
#define PL_CAN_BE_INSTANTIATED_PREFIX		"__CBI__"
#define PL_CAN_BE_INSTANTIATED_PREFIX_LEN	7
#define PL_DO_NOT_INSTANTIATE_PREFIX		"__DNI__"
#define PL_DO_NOT_INSTANTIATE_PREFIX_LEN	7
#define PL_INSTANCE_REQUIRED_PREFIX		"__TIR___"
#define PL_INSTANCE_REQUIRED_PREFIX_LEN		7

/* The maximum number of iterations after which we give up under the
   assumption that we've encountered an instantiation loop. */
#define PL_MAX_ITERATIONS	30

/* Indicates that the object file should be removed when the .ii file
   is updated.  This can be useful if the compilation is terminated
   before the new object file has been written since it will prevent the
   .ii file and .o file from getting out of sync. */
#define PL_REMOVE_OBJECT_FILE_BEFORE_RECOMPILATION TRUE

/* Function that executes "command" and directs its output to the
   returned file pointer. */
extern FILE* popen(char *command, char *mode);

/* Command to be used to produce a namelist of an object file. */
static char		default_nm_command[] = "/bin/nm -og";
static char		solaris_nm_command[] = "/bin/nm -pxR";
static char		SGI_nm_command[] = "/bin/nm -Bopg";
static char		CLIX_nm_command[] = "/bin/nm -pxre";
static char		alternate_nm_command[] = "/bin/nm -pxr";
static char		nm_command_suffix[] = " 2>/dev/null";

static char		*pl_predefined_names[] = {
#ifdef sparc
				"etext",
				"edata",
				"end",
				"_DYNAMIC",
				"_GLOBAL_OFFSET_TABLE",
#endif /* sparc */
				NULL
};


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
