/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lang_feat.h -- Definition of source language features to be accepted.

*/

/* Avoid including these declarations more than once: */
#ifndef LANG_FEAT_H
#define LANG_FEAT_H 1

/*
Flag that is TRUE to allow the AT&T extensions to ANSI C preprocessing,
i.e., #assert, #unassert, and the use of assertions in #if expressions.
These extensions were added in System V release 4.
*/
#ifndef ATT_PREPROCESSING_EXTENSIONS_ALLOWED
#define ATT_PREPROCESSING_EXTENSIONS_ALLOWED TRUE
#endif /* ifndef ATT_PREPROCESSING_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to include asm function definitions in the language.
In the standard version of the front end they are not interpreted but
instead passed on to the back end verbatim.
*/
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED FALSE
#endif /* ifndef ASM_FUNCTION_ALLOWED */

/*
Flag that is TRUE to allow dollar signs ($) in identifiers.  This is the
default value for the flag that can be modified by a command line
option.
*/
#ifndef DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS
#define DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS FALSE
#endif /* ifndef DEFAULT_ALLOW_DOLLAR_IN_ID_CHARS */

/*
Flag that is TRUE to allow C++ anachronisms to be accepted in the source
language.  This is the default value for a flag that can be modified by
a command line option.
*/
#ifndef DEFAULT_ALLOW_ANACHRONISMS
#define DEFAULT_ALLOW_ANACHRONISMS FALSE
#endif /* ifndef DEFAULT_ALLOW_ANACHRONISMS */

/*
Flag that is TRUE to allow the anachronism of calling a non-const
member function on a const object.  This is the default value for the
variable allow_nonconst_call_anachronism.
*/
#ifndef DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM
#define DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM FALSE
#endif /* ifndef DEFAULT_ALLOW_NONCONST_CALL_ANACHRONISM */

/*
Flag that is the default value for allow_nonconst_ref_anachronism,
which controls the anachronism of allowing a reference to nonconst to bind
to a class rvalue of the right type.
*/
#ifndef DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM
#define DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM FALSE
#endif /* ifndef DEFAULT_ALLOW_NONCONST_REF_ANACHRONISM */

/*
Flag that is TRUE if integer arguments to prototyped functions are passed
the same way as integer arguments to unprototyped functions, i.e., they are
widened to something like "int", for example by being passed in a register.
This relaxes an aspect of type-compatibility checking.  When this is TRUE,
something like

  void f(char);
  void f(c) char c; {}

is accepted in normal (non-strict) mode.  ANSI C says the two declarations
above are not compatible, because an argument to the old-style function
must be widened, whereas the argument to the prototyped function may or may
not be widened depending on the implementation.  If we can say "this
implementation always does widening," the two declarations can be
considered compatible.
*/
#ifndef PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED
#define PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED FALSE
#endif /* ifndef PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED */

/*
Flag that is TRUE if pointers to incomplete arrays should be allowed
in pointer addition and subtraction operations, e.g.,

  int (*p)[];
  ...
  p[0];

If this is turned on, the back end must be able to deal with the
resultant operations on pointers to zero-length types.
*/
#ifndef PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
#define PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED FALSE
#endif /* ifndef PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */

/*
Flag that is TRUE if C anachronisms should be allowed.  The anachronisms
are those of Appendix A, section 17 of K&R I:

  (1)  Reversed-form compound assignment operators:
         i =- 1;
  (2)  Omitted "=" in initialization:
         int i 1;
*/
#ifndef C_ANACHRONISMS_ALLOWED
#define C_ANACHRONISMS_ALLOWED FALSE
#endif /* ifndef C_ANACHRONISMS_ALLOWED */

/*
TRUE if pcc-style preprocessing should be done when compiling C++
in cfront compatibility mode.  This is sensible when the version of
cfront with which compatibility is desired uses an old-style Reiser
preprocessor.  Also suppresses definition of __STDC__.
*/
#ifndef OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE
#define OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE FALSE
#endif /* ifndef OLD_STYLE_PREPROCESSING_IN_CFRONT_MODE */

/*
TRUE if __STDC__ should be defined to 0 in nonstrict mode and 1 in
strict mode.  This flag affects both ANSI C and C++ mode and overrides
most other factors that affect the setting of __STDC__.  For example,
__STDC__ will be defined even in Microsoft mode.  The special processing
in when stdc_zero_in_system_headers is TRUE is still done, however.
*/
#ifndef STDC_ZERO_IN_NONSTRICT_MODE
#define STDC_ZERO_IN_NONSTRICT_MODE FALSE
#endif /* ifndef STDC_ZERO_IN_NONSTRICT_MODE */

/*
TRUE if, in GNU mode, __STDC__ should be defined to 0 while processing
system header files.
*/
#ifndef DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS
#define DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS FALSE
#endif /* ifndef DEFAULT_GNU_STDC_ZERO_IN_SYSTEM_HEADERS */

/*
TRUE if __STDC__ should be defined to 0 while processing system header files.
*/
#ifndef DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS
#define DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS FALSE
#endif /* ifndef DEFAULT_STDC_ZERO_IN_SYSTEM_HEADERS */

/*
TRUE if __STDC__ should be defined (to 1) in Microsoft mode.  This is typically
FALSE (as Microsoft compilers do not define it in either C or C++ mode),
but should be set to TRUE in configurations where the system header files
may be expecting it to be set (e.g., GNU headers on non-Windows platforms).
Some versions of GNU's libio.h -- included by stdio.h -- define "const" to a
NULL macro if __STDC__ isn't defined.
*/
#ifndef DEFINE_STDC_IN_MICROSOFT_MODE
#if EDG_WIN32
#define DEFINE_STDC_IN_MICROSOFT_MODE FALSE
#else /* !EDG_WIN32 */
#define DEFINE_STDC_IN_MICROSOFT_MODE TRUE
#endif /* EDG_WIN32 */
#endif /* ifndef DEFINE_STDC_IN_MICROSOFT_MODE */

/*
Flag that is TRUE if the address of a bit field may be taken as long
as the bit field has a size and alignment that match some integral type.
A warning is issued.
*/
#ifndef ADDR_OF_BIT_FIELD_ALLOWED
#define ADDR_OF_BIT_FIELD_ALLOWED FALSE
#endif /* ifndef ADDR_OF_BIT_FIELD_ALLOWED */

/*
Flag that is TRUE if, in C++ mode, support for exception handling is enabled
by default.  This is the default value for the global flag exceptions_enabled,
which can be modified by the "-x" command line option.
*/
#ifndef DEFAULT_EXCEPTIONS_ENABLED
/* If the C++-generating back end is being used, there is no cost to
   enabling exceptions by default. */
#ifdef BACK_END_IS_CP_GEN_BE
#if BACK_END_IS_CP_GEN_BE
#define DEFAULT_EXCEPTIONS_ENABLED TRUE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifdef BACK_END_IS_CP_GEN_BE */
#ifndef DEFAULT_EXCEPTIONS_ENABLED
#define DEFAULT_EXCEPTIONS_ENABLED FALSE
#endif /* ifndef DEFAULT_EXCEPTIONS_ENABLED */
#endif /* ifndef DEFAULT_EXCEPTIONS_ENABLED */

/*
Flag that is TRUE if, in C++, support for runtime type information (RTTI)
is enabled by default.  This is the default value of the variable rtti_enabled,
which can be modified by the "--rtti" or "--no_rtti" command-line options.
*/
#ifndef DEFAULT_RTTI_ENABLED
#define DEFAULT_RTTI_ENABLED TRUE
#endif /* ifndef DEFAULT_RTTI_ENABLED */

/*
Flag that is TRUE if, in C++, support for array new and delete is enabled
by default.  This is the default value of the variable
array_new_and_delete_enabled, which can be modified by the
"--array_new_and_delete" or "--no_array_new_and_delete" command-line options.
*/
#ifndef DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED
#define DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED TRUE
#endif /* ifndef DEFAULT_ARRAY_NEW_AND_DELETE_ENABLED */

/*
Flag that is TRUE if, in C++, support for the "explicit" specifier on
constructor declarations is allowed.  This is the default value of variable
explicit_keyword_enabled, which can be modified by the "--explicit" and
"--no_explicit" command-line options.
*/
#ifndef DEFAULT_EXPLICIT_KEYWORD_ENABLED
#define DEFAULT_EXPLICIT_KEYWORD_ENABLED TRUE
#endif /* ifndef DEFAULT_EXPLICIT_KEYWORD_ENABLED */

/*
Flag that is TRUE if, in C++, support for namespaces is enabled by default.
This is the default value of the variable namespaces_enabled, which can be
modified by the "--namespaces" or "--no_namespaces" command-line options.
*/
#ifndef DEFAULT_NAMESPACES_ENABLED
#define DEFAULT_NAMESPACES_ENABLED TRUE
#endif /* ifndef DEFAULT_NAMESPACES_ENABLED */

/*
Flag that is TRUE if, in C++, when the runtime uses namespaces, the runtime
should implicitly do a "using namespace std" to make names in the std
namespace visible without qualification.  This is the default value of
the variable implicit_using_std, which can be modified by the "--using_std"
or "--no_using_std" command-line options.
*/
#ifndef DEFAULT_IMPLICIT_USING_STD
#define DEFAULT_IMPLICIT_USING_STD FALSE
#endif /* ifndef DEFAULT_IMPLICIT_USING_STD */

/*
Flag that is TRUE if, in C++, support for typename is enabled by default.
This is the default value of the variable typename_enabled, which can be
modified by the "--typename" or "--no_typename" command-line options.
*/
#ifndef DEFAULT_TYPENAME_ENABLED
#define DEFAULT_TYPENAME_ENABLED TRUE
#endif /* ifndef DEFAULT_TYPENAME_ENABLED */

/*
Flag that is TRUE if, in C++, the front end should, by default, determine
from context whether a template parameter dependent name is a type or nontype.
This is the default value of the variable implicit_typename_enabled, which
can be modified by the "--implicit_typename" or "--no_implicit_typename"
command-line options.
*/
#ifndef DEFAULT_IMPLICIT_TYPENAME_ENABLED
#define DEFAULT_IMPLICIT_TYPENAME_ENABLED TRUE
#endif /* ifndef DEFAULT_IMPLICIT_TYPENAME_ENABLED */

/*
Flag that is TRUE if, in C++, an "inline" function is allowed to have
external linkage.  It is the default value for global variable
extern_inline_allowed, which can be modified by the "--extern_inline" and
"--no_extern_inline" command-line options.  When it is TRUE it means
(consistent with the current version of the standard)
  -- for nonmember functions
       the specifier sequence "extern inline" is permitted,
       "inline" by itself implies external linkage, and
       "inline static" must be used to specify internal linkage;
  -- for member functions
       an inline function, like noninline functions, takes the linkage of
       the class of which it is a member (which is usually external).
When it is FALSE (consistent with the ARM and for cfront compatibility) it
means
  -- for nonmember functions:
       "extern" and "inline" are incompatible specifiers, and
       "inline" always implies "static" and internal linkage;
  -- for member functions:
       inline functions always have internal linkage.

Note that the extern_inline_allowed flag is not used in C99 mode, since the
meaning of "extern inline" is somewhat different.
*/
#ifndef DEFAULT_EXTERN_INLINE_ALLOWED
#define DEFAULT_EXTERN_INLINE_ALLOWED TRUE
#endif /* DEFAULT_EXTERN_INLINE_ALLOWED ifndef  */

/*
Flag that is TRUE if template nontype parameters with floating point
types are allowed.  X3J16 made floating point template parameters
ill-formed in 3/94 but they are allowed by some compilers (e.g.,
Borland).  This is the initial value of the variable
floating_point_template_parameters_allowed.
*/
#ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED
#ifdef ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS
/* If the old macro for this feature is set, set the new one. */
#define DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED \
        ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS
#endif /* ifdef ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS */
#endif /* ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED */
#ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED
#define DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED FALSE
#endif /* ifndef DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED */

/*
Flag that is used as the default setting for global variable
operator_overloading_on_enums_enabled.  This controls whether operator
functions can overload builtin operators for arguments of enum type.
The variable can also be controlled from the command line by
--[no_]enum_overloading.
*/
#ifndef DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS
#define DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS TRUE
#endif /* ifndef DEFAULT_OPERATOR_OVERLOADING_ON_ENUMS */

/*
Flag that is used as the default setting for global variable
string_literals_are_const.  This controls whether literals like "abcd"
have type "array[5] of const char" or the older "array[5] of char".
The variable can also be controlled from the command line by
--[no_]const_string_literals.
*/
#ifndef DEFAULT_STRING_LITERALS_ARE_CONST
#define DEFAULT_STRING_LITERALS_ARE_CONST TRUE
#endif /* ifndef DEFAULT_STRING_LITERALS_ARE_CONST */

/*
Flag that is used as the default setting for global variable
class_name_injection_enabled.  This controls whether the name of a
class is injected into the scope of the class.  The variable can also
be controlled from the command line by --[no_]class_name_injection.
*/
#ifndef DEFAULT_CLASS_NAME_INJECTION
#define DEFAULT_CLASS_NAME_INJECTION TRUE
#endif /* DEFAULT_CLASS_NAME_INJECTION */

/*
Flag that is used as the default setting for global variable
arg_dependent_lookup_enabled.  This controls whether argument
dependent lookup is done for unqualified names in function calls.
The variable can also be controlled from the command line by
--[no_]arg_dep_lookup.
*/
#ifndef DEFAULT_ARG_DEPENDENT_LOOKUP
#define DEFAULT_ARG_DEPENDENT_LOOKUP TRUE
#endif /* DEFAULT_ARG_DEPENDENT_LOOKUP */

/*
Flag that is used as the default setting for global variable
friend_injection_enabled.  This controls whether a class or function
first declared only in friend declarations is visible to normal lookups.
The standard specifies that such names are not visible to normal
lookups.  The variable can also be controlled from the command line by
--[no_]friend_injection.
*/
#ifndef DEFAULT_FRIEND_INJECTION
#define DEFAULT_FRIEND_INJECTION TRUE
#endif /* DEFAULT_FRIEND_INJECTION */


/*
Flag that is used as the default setting for global variable
do_dependent_name_processing.  This controls whether the 2-phase lookup
of template names is performed as required by the standard.  It also
controls whether prototype instantiations of function bodies and default
arguments are done.  The variable can also be controlled from the command
line by --[no_]dep_name.  There's a separate default for C++0X mode.
*/
#ifndef DEFAULT_DEPENDENT_NAME_PROCESSING
#define DEFAULT_DEPENDENT_NAME_PROCESSING FALSE
#endif /* DEFAULT_DEPENDENT_NAME_PROCESSING */
#ifndef DEFAULT_CPP0X_DEPENDENT_NAME_PROCESSING
#define DEFAULT_CPP0X_DEPENDENT_NAME_PROCESSING TRUE
#endif /* DEFAULT_CPP0X_DEPENDENT_NAME_PROCESSING */

/*
Flag that is used as the default setting for the global variable
export_template_allowed.  This controls whether the processing required
to define and use exported templates should be done.  The variable can
also be controlled from the command line by --[no_]export.  Note that
exported templates were taken out of the C++ language and are turned off
by default in C++0X mode.
*/
#ifndef DEFAULT_EXPORT_TEMPLATE_ALLOWED
#define DEFAULT_EXPORT_TEMPLATE_ALLOWED FALSE
#endif /* DEFAULT_EXPORT_TEMPLATE_ALLOWED */

/*
Flag that is TRUE if a set of Sun C++ compatibility features should be
allowed.
*/
#ifndef SUN_EXTENSIONS_ALLOWED
#define SUN_EXTENSIONS_ALLOWED FALSE
#endif /* ifndef SUN_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE if Sun CC 5.x compatibility features should be allowed by
default.  It is the default initial value of the associated global variable
sun_mode and can be overridden by the command-line options --sun and --no_sun.
*/
#if SUN_EXTENSIONS_ALLOWED
#ifndef DEFAULT_SUN_COMPATIBILITY
#define DEFAULT_SUN_COMPATIBILITY TRUE
#endif /* ifndef DEFAULT_SUN_COMPATIBILITY */
#endif /* SUN_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE if Sun CC 5.5 linker scope specifiers (__global, __hidden,
__symbolic) should be accepted by default. */
#if SUN_EXTENSIONS_ALLOWED
#ifndef DEFAULT_SUN_LINKER_SCOPE_ALLOWED
#define DEFAULT_SUN_LINKER_SCOPE_ALLOWED DEFAULT_SUN_COMPATIBILITY
#endif /* ifndef DEFAULT_SUN_LINKER_SCOPE_ALLOWED */
#endif /* SUN_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to include code for UPC (Unified Parallel C) support.
*/
#ifndef UPC_EXTENSIONS_ALLOWED
#define UPC_EXTENSIONS_ALLOWED FALSE
#endif /* UPC_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to include code for GNU C compatibility features.
*/
#ifndef GNU_EXTENSIONS_ALLOWED
#define GNU_EXTENSIONS_ALLOWED FALSE
#endif /* ifndef GNU_EXTENSIONS_ALLOWED */

/*
Obsolete flag indicating that GNU compatibility features should be enabled
by default.  Use DEFAULT_GNU_COMPATIBILITY instead.  It now also affects C++
mode.
*/
#ifdef DEFAULT_GCC_COMPATIBILITY
#define DEFAULT_GNU_COMPATIBILITY DEFAULT_GCC_COMPATIBILITY
#endif /* ifdef DEFAULT_GCC_COMPATIBILITY */

/*
Flag that is TRUE if GNU C or C++ compatibility features should be allowed by
default.  In C mode, it is the default value for gcc_mode; in C++ mode it is
the default value for gpp_mode.  This default may be overridden by command-
line options --gcc, --no_gcc, --g++, and --no_g++.
*/
#ifndef DEFAULT_GNU_COMPATIBILITY
#if GNU_EXTENSIONS_ALLOWED
#define DEFAULT_GNU_COMPATIBILITY TRUE
#else /* !GNU_EXTENSIONS_ALLOWED */
#define DEFAULT_GNU_COMPATIBILITY FALSE
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* ifndef DEFAULT_GNU_COMPATIBILITY */

/*
The configuration macros GCC_VERSION and GCC_MINOR_VERSION are now obsolete.
If they were defined, the newer macro DEFAULT_GNU_VERSION should not also be
defined and instead the older (obsolete) macros will determine the value of
the newer macro.
*/
#if defined(GCC_VERSION) || defined(GCC_MINOR_VERSION)
#if !defined(GCC_VERSION) || !defined(GCC_MINOR_VERSION)
 #error -- GCC_VERSION cannot be defined without also defining \
           GCC_MINOR_VERSION and vice versa
#else /* !(!defined(GCC_VERSION) || !defined(GCC_MINOR_VERSION)) */
#if defined(DEFAULT_GNU_VERSION)
 #error -- DEFAULT_GNU_VERSION cannot be defined if the (now obsolete) macros \
           GCC_VERSION and GCC_MINOR_VERSION are also defined
#else /* !defined(DEFAULT_GNU_VERSION) */
#define DEFAULT_GNU_VERSION ((GCC_VERSION)*10000 + (GCC_MINOR_VERSION)*100)
#endif /* defined(DEFAULT_GNU_VERSION) */
#endif /* !defined(GCC_VERSION) || !defined(GCC_MINOR_VERSION) */
#endif /* defined(GCC_VERSION) || defined(GCC_MINOR_VERSION) */

/*
Macro that determines which version of the GNU C/C++ compiler should be
emulated by default.  Version x.y.z of the GNU compiler is represented by
the value x*10000+y*100+z.
*/
#ifndef DEFAULT_GNU_VERSION
#define DEFAULT_GNU_VERSION 40200
#endif /* ifndef DEFAULT_GNU_VERSION */

/*
A configuration macro that determines the minimum GNU C/C++ version that can
be emulated by the front end.  By default, the front end not normally support
emulation of versions of gcc and g++ prior to 3.2 (30200).
*/
#ifndef MIN_GNU_VERSION
#define MIN_GNU_VERSION 30200
#endif /* MIN_GNU_VERSION */
#if (DEFAULT_GNU_VERSION) < (MIN_GNU_VERSION)
 #error -- DEFAULT_GNU_VERSION too small
#endif /* (DEFAULT_GNU_VERSION) < (MIN_GNU_VERSION) */

/*
The value of the __VERSION__ macro in GNU mode.  Note that an extra set of
quotes is needed as this is the actual macro replacement string to be used.
The string can contain one occurrence of "%m" (which will be expanded to
"gcc" or "g++" dependent on the current mode) and one occurrence of "%v"
(which will be expanded to the version number in x.y or x.y.z format).
*/
#ifndef GCC_VERSION_STRING
#define GCC_VERSION_STRING "\"EDG %m %v mode\""
#endif /* GCC_VERSION_STRING */

/*
Flag that is TRUE if GNU builtin operators should be accepted in support of
<stdarg.h> and <varargs.h>.  This flag applies to both GNU C and GNU C++
modes.
*/
#ifndef GCC_BUILTIN_VARARGS
#define GCC_BUILTIN_VARARGS TRUE
#endif /* ifndef GCC_BUILTIN_VARARGS */

/*
Flag that is TRUE if asm expressions target a processor of an x86 family.
If Gnu extensions are enabled and we are building on an x86 system (32-bit
or 64-bit variants), this flag defaults to TRUE.
*/
#ifndef GNU_X86_ASM_EXTENSIONS_ALLOWED
#if GNU_EXTENSIONS_ALLOWED && (defined(__i386) || defined(__x86_64))
#define GNU_X86_ASM_EXTENSIONS_ALLOWED TRUE
#else /* !(GNU_EXTENSIONS_ALLOWED && (defined(__i386) || defined(__x86_64))) */
#define GNU_X86_ASM_EXTENSIONS_ALLOWED FALSE
#endif /* GNU_EXTENSIONS_ALLOWED && (defined(__i386) || defined(__x86_64)) */
#endif /* ifndef GNU_X86_ASM_EXTENSIONS_ALLOWED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_X86_ASM_EXTENSIONS_ALLOWED
 #error -- GNU_X86_ASM_EXTENSIONS_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_X86_ASM_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE if unrecognized extended asm operands should be
accepted.  This lets the front end process source files with asm
directives intended for an architecture for which specific asm support
is not provided.
*/
#ifndef ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
#define ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS FALSE
#endif /* ifndef ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */

#if !GNU_EXTENSIONS_ALLOWED && ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
 #error -- ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */

/*
Flag that is TRUE if GNU asm operand description strings should be recorded
as they appear in the source instead of represented in a more structured form.
When TRUE (the default), the description strings are not checked for validity
and constructs that are otherwise unrecognized by the front end are accepted.
*/
#ifndef RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
#if GNU_EXTENSIONS_ALLOWED
#define RECORD_RAW_ASM_OPERAND_DESCRIPTIONS TRUE
#else /* !GNU_EXTENSIONS_ALLOWED */
#define RECORD_RAW_ASM_OPERAND_DESCRIPTIONS FALSE
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* ifndef RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

#if !GNU_EXTENSIONS_ALLOWED && RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
 #error -- RECORD_RAW_ASM_OPERAND_DESCRIPTIONS requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

/*
Flag that is TRUE if x86-specific attributes should be recognized (and
recorded in the IL).  This includes the stdcall and cdecl attributes.
*/
#ifndef GNU_X86_ATTRIBUTES_ALLOWED
#if GNU_EXTENSIONS_ALLOWED && (defined(__i386) || defined(__x86_64))
#define GNU_X86_ATTRIBUTES_ALLOWED TRUE
#else /* !(GNU_EXTENSIONS_ALLOWED && (defined(__i386) || defined(__x86_64))) */
#define GNU_X86_ATTRIBUTES_ALLOWED FALSE
#endif /* GNU_EXTENSIONS_ALLOWED && (defined(__i386) || defined(__x86_64)) */
#endif /* ifndef GNU_X86_ATTRIBUTES_ALLOWED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_X86_ATTRIBUTES_ALLOWED
 #error -- GNU_X86_ATTRIBUTES_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_X86_ATTRIBUTES_ALLOWED */

/*
Flag that is TRUE if the "naked" attribute should be recognized (and
recorded in the IL).
*/
#ifndef GNU_NAKED_ATTRIBUTE_ALLOWED
#define GNU_NAKED_ATTRIBUTE_ALLOWED FALSE
#endif /* ifndef GNU_NAKED_ATTRIBUTE_ALLOWED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_NAKED_ATTRIBUTE_ALLOWED
 #error -- GNU_NAKED_ATTRIBUTE_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_NAKED_ATTRIBUTE_ALLOWED */

/*
Flag that is TRUE if the GNU "init_priority" attribute should be 
recognized (and recorded in the IL).
*/
#ifndef GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
#define GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED FALSE
#endif /* ifndef GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

#ifndef DEFAULT_GNU_INIT_PRIORITY_ATTRIBUTE_ENABLED
#define DEFAULT_GNU_INIT_PRIORITY_ATTRIBUTE_ENABLED \
	  GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
#endif /* ifndef DEFAULT_GNU_INIT_PRIORITY_ATTRIBUTE_ENABLED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
 #error -- GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */

/*
Flag that is TRUE if the GNU "visibility" attribute should be 
recognized (and recorded in the IL).
*/
#ifndef GNU_VISIBILITY_ATTRIBUTE_ALLOWED
#define GNU_VISIBILITY_ATTRIBUTE_ALLOWED FALSE
#endif /* ifndef GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

#ifndef DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED
#define DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED \
	  GNU_VISIBILITY_ATTRIBUTE_ALLOWED
#endif /* ifndef DEFAULT_GNU_VISIBILITY_ATTRIBUTE_ENABLED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
 #error -- GNU_VISIBILITY_ATTRIBUTE_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

/*
Flag that is TRUE if GNU complex extensions should be allowed (this requires
additional support in a back end or, if LOWER_COMPLEX is TRUE, in the run-time
support library.
*/
#ifndef GNU_COMPLEX_EXTENSIONS_ALLOWED
#if GNU_EXTENSIONS_ALLOWED
#define GNU_COMPLEX_EXTENSIONS_ALLOWED TRUE
#else /* !GNU_EXTENSIONS_ALLOWED */
#define GNU_COMPLEX_EXTENSIONS_ALLOWED FALSE
#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_COMPLEX_EXTENSIONS_ALLOWED
 #error -- GNU_COMPLEX_EXTENSIONS_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_COMPLEX_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE if built-in GNU __sync_... functions should be accepted in
GNU modes.
*/
#ifndef GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED
#define GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED FALSE
#endif /* GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED
 #error -- GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED */

/*
Flag that is TRUE if GNU vector types should be allowed. (This includes, e.g.,
support for __attribute__((vector_size(N))).)
*/
#ifndef GNU_VECTOR_TYPES_ALLOWED
#define GNU_VECTOR_TYPES_ALLOWED FALSE
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#if !GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
 #error -- GNU_VECTOR_TYPES_ALLOWED requires GNU_EXTENSIONS_ALLOWED
#endif /* !GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */

/*
Flag that is TRUE if GNU built-in functions implementing IA-32 vector
instructions should be predeclared.  (These map on machine instruction sets
like Intel's MMX or AMD's 3DNow!.)
*/
#ifndef GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED
#if GNU_VECTOR_TYPES_ALLOWED && (defined(__i386) || defined(__x86_64))
#define GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED TRUE
#else /* !(GNU_VECTOR_TYPES_ALLOWED && (...)) */
#define GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED FALSE
#endif /* GNU_VECTOR_TYPES_ALLOWED && (defined(__i386) || defined(__x86_64)) */
#endif /* GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED */

#if GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED && !GNU_VECTOR_TYPES_ALLOWED
 #error -- GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED requires \
           GNU_VECTOR_TYPES_ALLOWED
#endif /* GNU_BUILTIN_IA32_VECTOR_FUNCTIONS_ALLOWED && !GNU_VECTOR_TYPES_... */

/*
Flag that is TRUE if a set of Microsoft C/C++ compatibility features
should be allowed.  This flag in turn changes the default value of
a set of configuration flags.
*/
#ifndef MICROSOFT_EXTENSIONS_ALLOWED
#define MICROSOFT_EXTENSIONS_ALLOWED FALSE
#endif /* ifndef MICROSOFT_EXTENSIONS_ALLOWED */

/*
if_microsoft_extensions expands to "text" when building with
MICROSOFT_EXTENSIONS_ALLOWED or to nothing otherwise.
if_microsoft_extensions_else expands to "then_text" when building with
MICROSOFT_EXTENSIONS_ALLOWED or to "else_text" otherwise.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define if_microsoft_extensions(text) text
#define if_microsoft_extensions_else(then_text, else_text) then_text
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define if_microsoft_extensions(text) /* nothing */
#define if_microsoft_extensions_else(then_text, else_text) else_text
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to enable Microsoft mode as the default mode.  This
is the default value used to initialize microsoft_mode.  This may
be modified by a command line option.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef DEFAULT_MICROSOFT_MODE
#define DEFAULT_MICROSOFT_MODE TRUE
#endif /* ifndef DEFAULT_MICROSOFT_MODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to enable Microsoft bug emulation as the default when
Microsoft mode is used.  This is the default value used to initialize
microsoft_bugs (but only when microsoft_mode is TRUE).  This may be modified
by a command line option.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#ifndef DEFAULT_MICROSOFT_BUGS
#define DEFAULT_MICROSOFT_BUGS TRUE
#endif /* ifndef DEFAULT_MICROSOFT_BUGS */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE if C++/CLI extensions (ECMA-372) can be accepted.  (Setting
the flag to TRUE enables the command-line options --cppcli and --no_cppcli.)
If the flag is TRUE, MICROSOFT_EXTENSIONS_ALLOWED must be TRUE as well.
*/
#ifndef CPPCLI_ENABLING_POSSIBLE
#define CPPCLI_ENABLING_POSSIBLE FALSE
#endif /* ifndef CPPCLI_ENABLING_POSSIBLE */

#if CPPCLI_ENABLING_POSSIBLE && !MICROSOFT_EXTENSIONS_ALLOWED
 #error -- CPPCLI_ENABLING_POSSIBLE requires MICROSOFT_EXTENSIONS_ALLOWED
#endif /* CPPCLI_ENABLING_POSSIBLE && !MICROSOFT_EXTENSIONS_ALLOWED */

/*
Flag that is TRUE to enable C++/CLI extensions by default when Microsoft
extensions are enabled.
*/
#ifndef DEFAULT_CPPCLI_ENABLED
#define DEFAULT_CPPCLI_ENABLED FALSE
#endif /* ifndef DEFAULT_CPPCLI_ENABLED */

#if DEFAULT_CPPCLI_ENABLED && !CPPCLI_ENABLING_POSSIBLE
 #error -- DEFAULT_CPPCLI_ENABLED requires CPPCLI_ENABLING_POSSIBLE
#endif /* DEFAULT_CPPCLI_ENABLED && !CPPCLI_ENABLING_POSSIBLE */

/*
Flag that is TRUE if the front end is configured to write C++/CLI portable
assemblies.  This internal testing mode can be used on Windows systems
to write portable assembly files (which can then be used on non-Windows systems
as a source of metadata).
*/
#ifndef WRITE_CPPCLI_PORTABLE_ASSEMBLIES
#define WRITE_CPPCLI_PORTABLE_ASSEMBLIES FALSE
#endif /* ifndef WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES && !CPPCLI_ENABLING_POSSIBLE
 #error -- WRITE_CPPCLI_PORTABLE_ASSEMBLIES requires CPPCLI_ENABLING_POSSIBLE
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES && !CPPCLI_ENABLING_POSSIBLE */

#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES && !EDG_WIN32
 #error -- WRITE_CPPCLI_PORTABLE_ASSEMBLIES requires EDG_WIN32
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES && !EDG_WIN32 */

/*
Flag that is TRUE if the front end is configured to read C++/CLI portable
assemblies.  This internal testing mode can be used on non-Windows systems
to read C++/CLI metadata from portable assembly files.
*/
#ifndef READ_CPPCLI_PORTABLE_ASSEMBLIES
#define READ_CPPCLI_PORTABLE_ASSEMBLIES FALSE
#endif /* ifndef READ_CPPCLI_PORTABLE_ASSEMBLIES */

#if READ_CPPCLI_PORTABLE_ASSEMBLIES && !CPPCLI_ENABLING_POSSIBLE
 #error -- READ_CPPCLI_PORTABLE_ASSEMBLIES requires CPPCLI_ENABLING_POSSIBLE
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES && !CPPCLI_ENABLING_POSSIBLE */

/*
Flag that is TRUE if by default in Microsoft modes the front end should accept
64-bit pointer extensions (__ptr32/__ptr64 and __sptr/__uptr).  This is used
to initialize the global variable microsoft_64bit_pointer_extensions_enabled.
*/
#ifndef DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED
#if MICROSOFT_EXTENSIONS_ALLOWED
#define DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED TRUE
#else /* !defined(MICROSOFT_EXTENSIONS_ALLOWED) */
#define DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef DEFAULT_MICROSOFT_64BIT_POINTER_EXTENSIONS_ENABLED */

/*
Flag that is TRUE to permit "near" and "far" memory attributes.  This should
always be TRUE if Microsoft 16-bit mode is supported, but may be set to
FALSE even if Microsoft extensions are supported to disallow Microsoft
16-bit mode altogether.  There is also the option of supporting "near" and
"far" memory attributes without supporting other Microsoft extensions.
*/
#ifndef NEAR_AND_FAR_ALLOWED
#if MICROSOFT_EXTENSIONS_ALLOWED
/* Set this to TRUE to permit Microsoft 16-bit mode and to FALSE if only
   Microsoft 32-bit mode is supported. */
#define NEAR_AND_FAR_ALLOWED TRUE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/* Set this to TRUE to provide near/far support without general Microsoft
   compatibility. */
#define NEAR_AND_FAR_ALLOWED FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef NEAR_AND_FAR_ALLOWED */

/*
Flag that is TRUE to enable use of "near" and "far" memory attributes in
default mode.
*/
#ifndef DEFAULT_NEAR_AND_FAR_ENABLED
#if NEAR_AND_FAR_ALLOWED
#if MICROSOFT_EXTENSIONS_ALLOWED
/* If Microsoft extensions are supported, near/far support is turned on
   in Microsoft 16-bit mode, but that is not the default. */
#define DEFAULT_NEAR_AND_FAR_ENABLED FALSE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/* If NEAR_AND_ALLOWED is set without support for other Microsoft extensions,
   the feature is enabled by default. */
#define DEFAULT_NEAR_AND_FAR_ENABLED TRUE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#else /* !NEAR_AND_FAR_ALLOWED */
#define DEFAULT_NEAR_AND_FAR_ENABLED FALSE    /* Do not change this. */
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* ifndef DEFAULT_NEAR_AND_FAR_ENABLED */
#if DEFAULT_NEAR_AND_FAR_ENABLED && !NEAR_AND_FAR_ALLOWED
 #error -- DEFAULT_NEAR_AND_FAR_ENABLED cannot be true unless \
           NEAR_AND_FAR_ALLOWED is true
#endif /* DEFAULT_NEAR_AND_FAR_ENABLED && !NEAR_AND_FAR_ALLOWED */

/*
Default implicit size for pointers when "near" and "far" memory attributes
are supported (e.g., in Microsoft 16-bit mode).
*/
#if NEAR_AND_FAR_ALLOWED
#ifndef DEFAULT_FAR_DATA_POINTERS
#define DEFAULT_FAR_DATA_POINTERS FALSE
#endif /* ifndef DEFAULT_FAR_DATA_POINTERS */
#ifndef DEFAULT_FAR_CODE_POINTERS
#define DEFAULT_FAR_CODE_POINTERS FALSE
#endif /* ifndef DEFAULT_FAR_CODE_POINTERS */
#endif /* NEAR_AND_FAR_ALLOWED */

/*
Flag that is TRUE if a "__thread" specifier (to indicate that a variable should
be stored in thread-local storage) should be supported.
*/
#ifndef THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
#define THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED FALSE
#endif /* ifndef THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */

/*
Flag indicating whether the "__thread" specifier is recognized by default.
This is the default value of thread_local_storage_specifier_enabled.
*/
#ifndef DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED
#define DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED FALSE
#endif /* DEFAULT_THREAD_LOCAL_STORAGE_SPECIFIER_ENABLED */


/*
Flag that is TRUE when extensions are allowed for additional declaration
modifiers.  It should always be TRUE when support for Microsoft and/or Sun
extensions is included.  It must also be TRUE when support for the __thread
specifier is enabled.  (The decl-modifiers mechanism is a hook by which an
implementation can provide a certain class of custom extensions.)
*/
#ifndef DECL_MODIFIERS_IN_USE
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || \
    THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
#define DECL_MODIFIERS_IN_USE TRUE          /* Do not change this. */
#else /* !(MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || ...) */
#define DECL_MODIFIERS_IN_USE FALSE         /* You can change this. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || ... */
#endif /* ifndef DECL_MODIFIERS_IN_USE */
#if !DECL_MODIFIERS_IN_USE &&                                                \
    (MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED ||               \
     THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED)
 #error -- DECL_MODIFIERS_IN_USE must be true when any of                    \
           MICROSOFT_EXTENSIONS_ALLOWED, SUN_EXTENSIONS_ALLOWED,             \
           or THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED is true
#endif /* !DECL_MODIFIERS_IN_USE && (MICROSOFT_EXTENSIONS_ALLOWED || ...) */

/*
Flag that indicates the version of the Microsoft compiler that should
be emulated in Microsoft mode.  This enables or disables particular
Microsoft mode features when the acceptance of that feature varies
between versions of the Microsoft compiler. The value is specified
using the value of the predefined macro _MSC_VER supplied by the
version of the Microsoft compiler that is being emulated (for example,
1100 corresponds to Visual C++ version 5.0).
*/
#ifndef DEFAULT_MICROSOFT_VERSION
#define DEFAULT_MICROSOFT_VERSION 1400
#endif /* ifndef DEFAULT_MICROSOFT_VERSION */

/*
Flag that is TRUE if Microsoft attributes should be considered to be
recognized.  Microsoft attributes are always parsed in Microsoft mode,
but if this flag is FALSE an "unrecognized attribute" warning will be issued
on the use of any attribute.  See SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING
regarding the processing of recognized attributes.
*/
#ifndef RECOGNIZE_MICROSOFT_ATTRIBUTES
#define RECOGNIZE_MICROSOFT_ATTRIBUTES TRUE
#endif /* ifndef RECOGNIZE_MICROSOFT_ATTRIBUTES */


/*
When Microsoft attributes are recognized (see RECOGNIZE_MICROSOFT_ATTRIBUTES),
this flag controls whether or not semantic checking of the attributes
is done.  This flag applies to all of the attributes whose attribute
kind is msak_misc (e.g., not mask_uuid).  When this flag is TRUE, only a
string version of the attribute is created no analysis of the attribute
arguments is performed, and no verification that the attributes are used
in appropriate locations is done.  This flag causes msak_misc attributes
to be processed in the same manner as msak_unrecognized attributes.
*/
#ifndef SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING
#define SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING TRUE
#endif /* ifndef SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING */

/*
The global variable sun_mode is defined here (rather than in cmd_line.h) so
that it can be available to standalone utilities.
*/
#if SUN_EXTENSIONS_ALLOWED || defined(_lint)
EXTERN a_boolean
                sun_mode;
                        /* Accept C language features supported by Sun C++ 5.x
                           compilers. */
#else /* !(SUN_EXTENSIONS_ALLOWED || defined(_lint)) */
/* Make sun_mode a constant-expression so some code can be optimized away.
   Since lint would warn about such code, we do not do this when processed
   by lint. */
#define sun_mode FALSE
#endif /* SUN_EXTENSIONS_ALLOWED || defined(_lint) */

#if SUN_EXTENSIONS_ALLOWED
EXTERN a_boolean
                sun_linker_scope_allowed;
                        /* TRUE if Sun C++ 5.5 linker scope specifiers
                           (__global, __symbolic, __hidden) should be
                           accepted.  They can also be enabled in C mode. */
#endif /* SUN_EXTENSIONS_ALLOWED */

/*
The global variable gcc_mode is defined here (rather than in cmd_line.h) so
that it can be available to standalone utilities.
*/
#if GNU_EXTENSIONS_ALLOWED || defined(_lint)
EXTERN a_boolean
                gcc_mode;
                        /* Accept C language features supported by GNU C
                           compilers. */
#else /* !(GNU_EXTENSIONS_ALLOWED || defined(_lint)) */
/* Make gcc_mode a constant-expression so some code can be optimized away.
   Since lint would warn about such code, we do not do this when processed
   by lint. */
#define gcc_mode FALSE
#endif /* GNU_EXTENSIONS_ALLOWED || defined(_lint) */

/*
The global variable gpp_mode is defined here (rather than in cmd_line.h) so
that it can be available to standalone utilities.
*/
#if GNU_EXTENSIONS_ALLOWED || defined(_lint)
EXTERN a_boolean
                gpp_mode;
                        /* Accept C++ language features supported by GNU C++
                           compilers. */
#else /* !(GNU_EXTENSIONS_ALLOWED || defined(_lint)) */
/* Make gpp_mode a constant-expression so some code can be optimized away.
   Since lint would warn about such code, we do not do this when processed
   by lint. */
#define gpp_mode FALSE
#endif /* GNU_EXTENSIONS_ALLOWED || defined(_lint) */

/*
Convenience macro that evaluates to TRUE when either GNU C or GNU C++ mode
is enabled.
*/
#define gnu_mode (gcc_mode || gpp_mode)

EXTERN unsigned long
		gnu_version;
			/* The version of the GNU C or C++ compiler with which
			   compatibility is desired.  GNU C/C++ version x.y.z
			   is represented by the value x*10000+y*100+z.  (E.g.,
			   GNU C/C++ 3.4.1 is represented by 30401.) */

/*
Global variables related to Microsoft compatibility mode are defined here
(rather than in cmd_line.h) so that they can be available to standalone
utilities.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_boolean
		microsoft_mode;
			/* TRUE if Microsoft extensions are to be accepted. */

EXTERN a_boolean
		microsoft_bugs;
			/* TRUE if Microsoft bugs are to be emulated. */

EXTERN a_boolean
		cppcli_enabled;
			/* TRUE if C++/CLI features should be accepted. */

EXTERN a_boolean 
		is_scanning_generated_code_from_metadata;
			/* TRUE if we are scanning code generated from
			   metadata. */

#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
/* When Microsoft mode is unavailable, replace the variables for Microsoft
   mode and Microsoft bugs with macros.  This will allow optimizers to remove
   some useless code when the front-end itself is compiled. */
#ifdef _lint
/* When lint is used, avoid warnings about dead code. */
EXTERN a_boolean
		microsoft_mode;
EXTERN a_boolean
		microsoft_bugs;
EXTERN a_boolean
		cppcli_enabled;
EXTERN a_boolean 
		is_scanning_generated_code_from_metadata;
#else /* !defined(_lint) */
#define microsoft_mode FALSE
#define microsoft_bugs FALSE
#define cppcli_enabled FALSE
#define is_scanning_generated_code_from_metadata FALSE
#endif /* ifdef _lint */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN unsigned long
		microsoft_version;
			/* The version of the Microsoft compiler with which
			   compatibility is desired.  This enables or disables
			   particular Microsoft mode features when the
			   acceptance of that feature varies between versions
			   of the Microsoft compiler.  The value is specified
			   using the value of the predefined macro _MSC_VER
			   supplied by the version of the Microsoft compiler
			   that is being emulated. */


/*
Flag that is TRUE if a set of extensions is supported that permits features
similar to C++ anonymous unions (1) in C mode and (2) with structs (in both
C and C++) and classes (in C++) as well.  This functionality emulates an
extension provided by Microsoft and GNU compilers.
*/
#ifndef ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#define ALLOW_NONSTANDARD_ANONYMOUS_UNIONS (MICROSOFT_EXTENSIONS_ALLOWED || \
                                            GNU_EXTENSIONS_ALLOWED)
#endif /* ifndef ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

/*
Default value to which global variable allow_nonstandard_anonymous_unions
is set when ALLOW_NONSTANDARD_ANONYMOUS_UNIONS is TRUE.
*/
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#ifndef DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
#if MICROSOFT_EXTENSIONS_ALLOWED
#define DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS DEFAULT_MICROSOFT_MODE
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef DEFAULT_ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

/*
Flag that is TRUE if comments appearing within the text of an asm function
body should be preserved as part of the string representation (and passed
on to the back end).  This also controls whether comments are preserved
in Microsoft asms.  May be TRUE only if ASM_FUNCTION_ALLOWED or
MICROSOFT_EXTENSIONS_ALLOWED is TRUE.
*/
#ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY FALSE
#endif /* ifndef INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#if !ASM_FUNCTION_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED
#if INCLUDE_COMMENTS_IN_ASM_FUNC_BODY
 #error -- INCLUDE_COMMENTS_IN_ASM_FUNC_BODY cannot be true unless       \
           ASM_FUNCTION_ALLOWED or MICROSOFT_EXTENSIONS_ALLOWED is true
#endif /* INCLUDE_COMMENTS_IN_ASM_FUNC_BODY */
#endif /* !ASM_FUNCTION_ALLOWED && !MICROSOFT_EXTENSIONS_ALLOWED  */

/*
Flag that is TRUE if "#pragma pack(n)" and command-line option
"--pack_alignment=n" are supported.  This feature allows for packing classes
and structs by specifying a maximum alignment for nonstatic data members,
even when that alignment is less than the alignment dictated by the member's
type.
*/
#ifndef USER_CONTROL_OF_STRUCT_PACKING
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
#define USER_CONTROL_OF_STRUCT_PACKING TRUE
#else /* !(MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED) */
#define USER_CONTROL_OF_STRUCT_PACKING FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
#endif /* ifndef USER_CONTROL_OF_STRUCT_PACKING */

/*
Flag that is TRUE to recognize #pragma weak directives.
	#pragma weak <name1> [= <name2>]
This directive is only effective in C_mode().  The first name is to be given
weak binding.  If a second name is present, the first is also defined to be
a synonym for it.
*/
#ifndef PRAGMA_WEAK_ALLOWED
#define PRAGMA_WEAK_ALLOWED FALSE
#endif /* ifndef PRAGMA_WEAK_ALLOWED */

/*
Flag that is TRUE if "//" is recognized by default in C mode as a comment
delimiter.  This flag is ignored in C++ and in Microsoft C compatibility mode,
since the feature is turned on by default in those cases.  Note: even when
this flag is set, end-of-line comments are not allowed in strict ANSI/ISO C
mode.  Used to set global variable end_of_line_comments_allowed.
*/
#ifndef END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE
#define END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE FALSE
#endif /* END_OF_LINE_COMMENTS_ALLOWED_IN_C_MODE */

/*
Flag that is TRUE if "#pragma ident" and "#ident" are recognized.
Both are implemented by recording the string in a pragma entry and passing
it to the back end.
*/
#ifndef IDENT_DIRECTIVE_AND_PRAGMA
#define IDENT_DIRECTIVE_AND_PRAGMA TRUE
#endif /* ifndef IDENT_DIRECTIVE_AND_PRAGMA */

/*
Flag that is TRUE if "#pragma redefine_extname" is recognized.  Compilers that
recognize this pragma should also define a macro __PRAGMA_REDEFINE_EXTNAME.
*/
#ifndef REDEFINE_EXTNAME_PRAGMA_ENABLED
#ifdef __PRAGMA_REDEFINE_EXTNAME
#define REDEFINE_EXTNAME_PRAGMA_ENABLED TRUE
#else /* !(defined __PRAGMA_REDEFINE_EXTNAME) */
#define REDEFINE_EXTNAME_PRAGMA_ENABLED FALSE
#endif /* ifdef __PRAGMA_REDEFINE_EXTNAME */
#endif /* ifndef REDEFINE_EXTNAME_PRAGMA_ENABLED */

/*
Flag that is TRUE if "#alias" is recognized.
*/
#ifndef ALIAS_DIRECTIVE
#define ALIAS_DIRECTIVE FALSE
#endif /* ifndef ALIAS_DIRECTIVE */

/*
Flag that is used as the default setting for global variable
restrict_enabled, which controls the availability of the "restrict"
keyword.  The variable can also be set from the command line by
--[no_]restrict.  Moreover, restrict_enabled is always turned on by
default in C99 mode.

The restrict keyword implements NCEG proposal X3J11.1 92-068
("Aliasing Control via Restricted Pointers" by Bill Homer of CRI),
which was adapted for C++ in proposal X3J16/92-0057 (by Mike Holly).
It is also included in the C99 standard.  Briefly stated, restrict is
a type qualifier that may be applied to pointers and references and to
arrays that appear as function parameter types.  Its use represents a
guarantee by the programmer that, within the scope of the pointer
declaration, the object pointed to can be accessed only by that
pointer; since any violation of this guarantee renders the program
undefined, the compiler may rely on it in performing optimizations.
*/
#ifndef DEFAULT_RESTRICT_ENABLED
#define DEFAULT_RESTRICT_ENABLED FALSE
#endif /* ifndef DEFAULT_RESTRICT_ENABLED */

/*
Flag that is TRUE if, in C++ modes other than GNU and Sun C++ modes, type
traits helpers (like __is_union and __has_virtual_destructor) should be
enabled by default.  Type traits helpers are meant to ease the implementation
of ISO/IEC TR 19768.  This macro is used for the initialization of the global
variable type_traits_helpers_enabled.   (In GNU and Sun C++ modes, the type
traits helpers are never enabled by default to avoid potential future
conflicts if the GNU or Sun compilers add a similar facility.)
*/
#ifndef DEFAULT_TYPE_TRAITS_HELPERS_ENABLED
#define DEFAULT_TYPE_TRAITS_HELPERS_ENABLED TRUE
#endif /* DEFAULT_TYPE_TRAITS_HELPERS_ENABLED */

/*
Flag that is TRUE if in C++ mode "auto" can be a type specifier whose actual
type is to be deduced from an initializer that follows.  This macro is used
to initialize the global variable auto_type_specifier_enabled.
*/
#ifndef DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED
#define DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED FALSE
#endif /* ifndef DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED */

/*
Flag that is TRUE if in C++ mode "auto" can be a storage class specifier (the
traditional meaning of "auto").  This macro is used to initialize the global
variable auto_storage_class_specifier_enabled.
*/
#ifndef DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED
#define DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED TRUE
#endif /* ifndef DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED */

#if !DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED && \
    !DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED
 #error -- DEFAULT_AUTO_STORAGE_CLASS_SPECIFIER_ENABLED or \
           DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED must be TRUE
#endif /* !DEFAULT_AUTO_TYPE_SPECIFIER_ENABLED && ... */

/*
Flag that is TRUE if in C++ mode the C++0x SFINAE rules of N2634
should be enabled by default.  The feature is implicitly enabled in
C++0x mode, and implicitly disabled in Microsoft and GNU modes, so
this macro really sets the default for "default" mode.  This macro is
used to initialize the global variable cpp0x_sfinae_enabled.
*/
#ifndef DEFAULT_CPP0X_SFINAE_ENABLED
#define DEFAULT_CPP0X_SFINAE_ENABLED FALSE
#endif /* DEFAULT_CPP0X_SFINAE_ENABLED */

/*
Flag that is TRUE if, when C++0x SFINAE is enabled (see above), access
errors are not counted as errors that make deduction fail.  In N2634
access errors are ignored, but the committee changed its mind about that
later.
*/
#ifndef DEFAULT_CPP0X_SFINAE_IGNORE_ACCESS
#define DEFAULT_CPP0X_SFINAE_IGNORE_ACCESS FALSE
#endif /* DEFAULT_CPP0X_SFINAE_IGNORE_ACCESS */

/*
Flag that is TRUE if, in C++ mode, wchar_t is a keyword by default.  This is
the default value for the global flag wchar_t_is_keyword, the value of
which may be modified using command line options.
*/
#ifndef DEFAULT_WCHAR_T_IS_KEYWORD
#define DEFAULT_WCHAR_T_IS_KEYWORD TRUE
#endif /* ifndef DEFAULT_WCHAR_T_IS_KEYWORD */

/*
Flag that is TRUE if, when wchar_t is a keyword, a preprocessing symbol
should be defined to prevent the system header files from attempting to
redefine wchar_t as a typedef.
*/
#ifndef DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD
#define DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD */

/*
The name of the macro to be defined when wchar_t is a keyword.  This is
only used when DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD is TRUE.
*/
#if DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD
#ifndef MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD
#define MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD "_WCHAR_T"
#endif /* ifndef MACRO_DEFINED_WHEN_WCHAR_T_IS_KEYWORD */
#endif /* DEFINE_MACRO_WHEN_WCHAR_T_IS_KEYWORD */

/*
Flag that is TRUE if, in C++ mode, bool is a keyword by default.  This is
the default value for the global flag bool_is_keyword, the value of
which may be modified using command line options.
*/
#ifndef DEFAULT_BOOL_IS_KEYWORD
#define DEFAULT_BOOL_IS_KEYWORD TRUE
#endif /* ifndef DEFAULT_BOOL_IS_KEYWORD */

/*
Flag that is TRUE if, when bool is a keyword, a preprocessing symbol
should be defined to prevent header files from attempting to
redefine bool as a typedef.
*/
#ifndef DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD
#define DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD */

/*
The name of the macro to be defined when bool is a keyword.  This is
only used when DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD is TRUE.
*/
#if DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD
#ifndef MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD
#define MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD "_BOOL"
#endif /* ifndef MACRO_DEFINED_WHEN_BOOL_IS_KEYWORD */
#endif /* DEFINE_MACRO_WHEN_BOOL_IS_KEYWORD */

/*
Flag that is TRUE if, when array new and delete are enabled, a
preprocessing symbol should be defined so that header files can
determine whether the array versions of operator new and delete
should be declared.
*/
#ifndef DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
#define DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */

/*
The name of the macro to be defined when array new and delete are
enabled.
This is only used when DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
#ifndef MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED
#define MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED "__ARRAY_OPERATORS"
#endif /* ifndef MACRO_DEFINED_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */
#endif /* DEFINE_MACRO_WHEN_ARRAY_NEW_AND_DELETE_ENABLED */

/*
Flag that is TRUE if, when exceptions handling is enabled, a
preprocessing symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
#define DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED */

/*
The name of the macro to be defined when exceptions is enabled.
This is only used when DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED
#ifndef MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED
#define MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED "__EXCEPTIONS"
#endif /* ifndef MACRO_DEFINED_WHEN_EXCEPTIONS_ENABLED */
#endif /* DEFINE_MACRO_WHEN_EXCEPTIONS_ENABLED */

/*
Flag that is TRUE if, when RTTI is enabled, a
preprocessing symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_RTTI_ENABLED
#define DEFINE_MACRO_WHEN_RTTI_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_RTTI_ENABLED */

/*
The name of the macro to be defined when RTTI is enabled.
This is only used when DEFINE_MACRO_WHEN_RTTI_ENABLED
is TRUE.  Note that in later g++ emulation modes, the
__GXX_RTTI macro is also defined when RTTI is enabled.
*/
#if DEFINE_MACRO_WHEN_RTTI_ENABLED
#ifndef MACRO_DEFINED_WHEN_RTTI_ENABLED
#define MACRO_DEFINED_WHEN_RTTI_ENABLED "__RTTI"
#endif /* ifndef MACRO_DEFINED_WHEN_RTTI_ENABLED */
#endif /* DEFINE_MACRO_WHEN_RTTI_ENABLED */

/*
Flag that is TRUE if, when placement_delete is enabled, a
preprocessing symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
#define DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED */

/*
The name of the macro to be defined when placement delete is enabled.
This is only used when DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
is TRUE.
*/
#if DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED
#ifndef MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED
#define MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED "__PLACEMENT_DELETE"
#endif /* ifndef MACRO_DEFINED_WHEN_PLACEMENT_DELETE_ENABLED */
#endif /* DEFINE_MACRO_WHEN_PLACEMENT_DELETE_ENABLED */

/*
Flag that is TRUE if, when long long is not enabled, a preprocessing symbol
should be defined to prevent header files from attempting to use long long.
*/
#ifndef DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED
#define DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED */

/*
The name of the macro to be defined when long long is not enabled.  This is
only used when DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED is TRUE.
*/
#if DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED
#ifndef MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED
#define MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED "__NO_LONG_LONG"
#endif /* ifndef MACRO_DEFINED_WHEN_LONG_LONG_IS_DISABLED */
#endif /* DEFINE_MACRO_WHEN_LONG_LONG_IS_DISABLED */

/*
The name of the macro to be defined when type traits pseudo-functions (like
"__is_union"; to ease the implementation of ISO/IEC TR 19768) are accepted.
In Microsoft mode, the macro is not defined but the pseudo-functions are
accepted (with slight semantic differences in some cases).
*/
#ifndef MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED
#define MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED                       \
                                                   "__EDG_TYPE_TRAITS_ENABLED"
#endif /* ifndef MACRO_DEFINED_WHEN_TYPE_TRAITS_HELPERS_ENABLED */

/*
Flag that is TRUE if, when variadic templates are enabled, a preprocessing
symbol should be defined.
*/
#ifndef DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED
#define DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED TRUE
#endif /* ifndef DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED */

/*
The name of the macro to be defined when variadic templates are enabled.
This is only used when DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED is
TRUE.
*/
#if DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED
#ifndef MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED
#define MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED "__VARIADIC_TEMPLATES"
#endif /* ifndef MACRO_DEFINED_WHEN_VARIADIC_TEMPLATES_ENABLED */
#endif /* DEFINE_MACRO_WHEN_VARIADIC_TEMPLATES_ENABLED */

/*
Flag that is TRUE if, in C++ mode, operator keywords (e.g., bitand, compl)
and digraphs are recognized.  This is the default value for the global
flag alternative_tokens_allowed, the value of which may also be modified
using command line options.
*/
#ifndef DEFAULT_ALTERNATIVE_TOKENS_ALLOWED
#define DEFAULT_ALTERNATIVE_TOKENS_ALLOWED FALSE
#endif /* ifndef DEFAULT_ALTERNATIVE_TOKENS_ALLOWED */

/*
Flag that is TRUE if trigraphs are recognized.  This is the default value for
the global flag trigraphs_allowed, the value of which may also be modified
using command line options.
*/
#ifndef DEFAULT_TRIGRAPHS_ALLOWED
#define DEFAULT_TRIGRAPHS_ALLOWED TRUE
#endif /* ifndef DEFAULT_TRIGRAPHS_ALLOWED */

/*
Flag that is TRUE if "&..." should be accepted in the source code.  This
extension is provided to support the form of macro va_start that is provided
in some versions of stdarg.h, e.g.,
  #define va_start(list, name) (void)(list = (void *)((char *)&...))
This is the default value for address_of_ellipsis_allowed.
*/
#ifndef DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED
#define DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED FALSE
#endif /* ifndef DEFAULT_ADDRESS_OF_ELLIPSIS_ALLOWED */

/*
Flag that is TRUE if an ellipsis alone is permitted in a function declaration
in C mode -- something like "void f(...)".  A diagnostic is issued in strict
ANSI C mode.  (This usage is standard in C++ mode.)  This is the default value
for allow_ellipsis_only_param_in_C_mode.
*/
#ifndef DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE
#define DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE FALSE
#endif /* ifndef DEFAULT_ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE */

/*
Flag that is TRUE if, in ANSI C mode, a set of features found in the
SVR4 ANSI C compiler should be recognized.  This is the default value
for the global flag SVR4_C_mode, the value of which may be modified using
command line options.
*/
#ifndef DEFAULT_SVR4_C_MODE
#define DEFAULT_SVR4_C_MODE FALSE
#endif /* ifndef DEFAULT_SVR4_C_MODE */

/*
Flag that is TRUE if, in ANSI C mode, support for the C99 standard is
provided.  This is the default value for the global flag C99_mode, the
value of which may be modified using command line options.
*/
#ifndef DEFAULT_C99_MODE
#define DEFAULT_C99_MODE FALSE
#endif /* ifndef DEFAULT_C99_MODE */

#if DEFAULT_C99_MODE && DEFAULT_SVR4_C_MODE
 #error -- C99 and SVR4 C modes are mutually exclusive
#endif /* DEFAULT_C99_MODE && DEFAULT_SVR4_C_MODE */

EXTERN a_boolean
		c99_mode;
			/* When TRUE accept language features defined by the
			   C99 standard. */

/*
Flag that is TRUE if the C99 and C++ (beginning with C++0x) predefined
macro __STDC_HOSTED__ should be set to 1 to indicate a hosted
implementation.  If it is FALSE, the macro is predefined to 0 to indicate a
non-hosted implementation.
*/
#ifndef STDC_HOSTED
#define STDC_HOSTED 1
#endif /* ifndef STDC_HOSTED */

/*
Flag that is TRUE if the C99 macro __STDC_IEC_559__ should be predefined
with the value 1.  When the flag is FALSE, the macro is left undefined.
When this flag is TRUE, the compiler is indicating that both the compiler
and runtime library conform to C99 Annex F, which describes the IEC 60559
floating point requirements.
*/
#ifndef STDC_IEC_559
#define STDC_IEC_559 0
#endif /* ifndef STDC_IEC_559 */

/*
Flag that is TRUE if the C99 macro __STDC_IEC_559_COMPLEX__ should be
predefined with the value 1.  When the flag is FALSE, the macro is left
undefined.  When this flag is TRUE, the compiler is indicating that both
the compiler and runtime library conform to C99 Annex G, which describes
the IEC 60559 complex arithmetic requirements.
*/
#ifndef STDC_IEC_559_COMPLEX
#define STDC_IEC_559_COMPLEX 0
#endif /* ifndef STDC_IEC_559_COMPLEX */

/*
Flag that is TRUE if the C99 macro __STDC_ISO_10646__ should be
predefined with the value STDC_ISO_10646_VALUE.  When the flag is FALSE,
the macro is left undefined.  These macros are used to indicate whether
the wchar_t values being used conform to a particular version of the ISO
10646 standard.  When STDC_ISO_10646_VALUE is defined it should be
defined with a value of the form yyyymmL (e.g., 199712L).
*/
#ifndef STDC_ISO_10646
#define STDC_ISO_10646 0
#endif /* ifndef STDC_ISO_10646 */

#if STDC_ISO_10646
#ifndef STDC_ISO_10646_VALUE
 #error -- STDC_ISO_10646_VALUE must be defined when STDC_ISO_10646 is set
#endif /* ifndef STDC_ISO_10646_VALUE */
#endif /* STDC_ISO_10646 */

/*
Flag that is TRUE if, in ANSI C mode, support for UPC (Unified Parallel C)
extensions is provided.  This is the default value for the global flag
upc_mode, the value of which may be modified using command line options.
*/
#ifndef DEFAULT_UPC_MODE
#define DEFAULT_UPC_MODE FALSE
#endif /* ifndef DEFAULT_UPC_MODE */

/*
Flag that is TRUE if support for bool can be enabled.
*/
#ifndef BOOL_ENABLING_POSSIBLE
#define BOOL_ENABLING_POSSIBLE TRUE
#endif /* ifndef BOOL_ENABLING_POSSIBLE */

/*
Flag that is TRUE if support for wchar_t can be enabled.
*/
#ifndef WCHAR_T_ENABLING_POSSIBLE
#define WCHAR_T_ENABLING_POSSIBLE TRUE
#endif /* ifndef WCHAR_T_ENABLING_POSSIBLE */

/*
Flag that is TRUE to enable a special nonstandard weighting of the
conversion for the integral operand of the [] operator in overload resolution.
Deals with cases like
  struct A {
    A();
    operator int *();
    int operator[](unsigned);
  };
  void main() {
    A a;
    a[0];  // Ambiguous according to standard, but okay with this option
  }
These are fairly common in existing code.  This is the initial value for
the global variable special_subscript_cost, which can be changed via the
--special_subscript_cost and --no_special_subscript_cost options.
*/
#ifndef DEFAULT_SPECIAL_SUBSCRIPT_COST
#define DEFAULT_SPECIAL_SUBSCRIPT_COST FALSE
#endif /* ifndef DEFAULT_SPECIAL_SUBSCRIPT_COST */

/*
Flag that is TRUE if the scope of a name declared in a for-init statement
extends to the end of the scope in which the for-statement appears and FALSE
if it extends only to the end of the for-statement; the latter is required in
standard-conforming programs.  This is the initial value for global variable
use_nonstandard_for_init_scope, which is also controlled by command-line
options --old_for_init and --new_for_init.  Used only in C++ mode, since in
C a for-init statement may not be a declaration.
*/
#ifndef DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE
#define DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE FALSE
#endif /* ifndef DEFAULT_USE_NONSTANDARD_FOR_INIT_SCOPE */

/*
Flag that is TRUE if a diagnostic should be issued when a name that is
visible under the new for-init scoping rules would be hidden (by the for-init
declaration itself) under the old rules.  It is only meaningful when the new
rules are used (i.e., when use_nonstandard_for_init_scope is FALSE).  It is
the initial value of global variable warning_on_for_init_difference, which
can also be controlled by command-line option --[no_]for_init_diff_warning.
Used only in C++ mode.
*/
#ifndef DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE
#define DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE TRUE
#endif /* ifndef DEFAULT_WARNING_ON_FOR_INIT_DIFFERENCE */

/*
Flag that is TRUE if a diagnostic should be issued when a dependent friend
function declaration appears in a class template, and that declaration is
not a definition (and guiding declarations are disabled).  Such declarations
are seldom what was intended.  For example:

  template <class T> void f(T){}
  template <class T> struct A {
    friend void f(T);  // f<> intended
  };
*/
#ifndef DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND
#define DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND TRUE
#endif /* ifndef DEFAULT_WARNING_ON_NON_TEMPLATE_FRIEND */

/*
Flag that is TRUE if, in default mode, an assignment operator for class A
with parameter of type "B", "B&", or "const B&" is viewed as a copy
assignment operator when B is a base class of A.  The effect is that a
user-declared A::operator=(const B&) will block the implicit generation of
A::operator=(const A&).  This behavior was common in older C++ compilers,
e.g., cfront, but the standard-conforming setting is FALSE.  This flag
is the initial value of global variable
allow_copy_assignment_op_with_base_class_param, which can also be controlled
by command-line option --[no_]base_assign_op_is_default.  Note that some
popular older software packages will not compile when the value is FALSE.
*/
#ifndef DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM
#define DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM FALSE
#endif /* ifndef DEFAULT_ALLOW_COPY_ASSIGNMENT_OP_WITH_BASE_CLASS_PARAM */

/*
Flag that is TRUE if, by default, a "guiding declaration" of a function
template instance is allowed.  It is the initial value of global variable
guiding_decls_allowed, which is also controlled by command line option
--[no_]guiding_decls.

A guiding declaration is a function declaration that matches a function
template, does not introduce a function definition (i.e., it implies an
instantiation of the template body and not a explicit specialization), and
is subject to different argument matching rules than those that apply to the
template itself (and therefore it affects overload resolution).  Here's an
example:

  template <class T> void f(T) { ... }
  void f(int);                        // guiding declaration in old C++

However, in the current version of the C++ standard there is no concept of
guiding declaration, and so in strict ANSI mode guiding_decls_allowed is
FALSE by default.  This means, in the example above, that function f is not
regarded as an instance of function template f.  Furthermore, it means that
there are two functions named "f" that take an "int" parameter, the one that
is explicitly declared and the one that is an instance of the template; a
call of "f(0)" would invoke the former, whereas a call of "f<int>(0)" would
be required to invoke the latter.
*/
#ifndef DEFAULT_GUIDING_DECLS_ALLOWED
#define DEFAULT_GUIDING_DECLS_ALLOWED FALSE
#endif /* ifndef DEFAULT_GUIDING_DECLS_ALLOWED */

/*
Flag that is TRUE if, by default, template specializations may be declared
using the "old syntax" -- i.e., if the "template <>" syntax is not required.
It is the initial value of global variable old_specializations_allowed,
which is also controlled by command line option --[no_]old_specializations.
(When old_specializations_allowed is TRUE but guiding_decls_allowed is FALSE,
the effect is that old-style specializations for non-member functions will
not be recognized as such.)
*/
#ifndef DEFAULT_OLD_SPECIALIZATIONS_ALLOWED
#define DEFAULT_OLD_SPECIALIZATIONS_ALLOWED TRUE
#endif /* if DEFAULT_OLD_SPECIALIZATIONS_ALLOWED */

/*
Flag that is TRUE to support the extension to allow implicit conversions
between pointers to extern "C" and extern "C++" function types.  It should be
be FALSE in a target environment in which C and C++ functions use distinct
calling conventions, and setting this flag to TRUE is pointless unless
function types differing only in extern "C" vs. extern "C++" routine linkage
are treated as distinct -- see DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT.
When this flag is TRUE,  --[no_]implicit_extern_c_type_conversion can be used
to adjust global variable impl_conv_between_c_and_cpp_function_ptrs_allowed
from the command line.  This flag is also consulted in setting the value of
DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED.
*/
#ifndef IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
#define IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE TRUE
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */

/*
Flag that is TRUE if, by default, implicit conversion between pointers to
extern "C" and extern "C++" function types is permitted.  It is the initial
value of global variable impl_conv_between_c_and_cpp_function_ptrs_allowed
and should be set to reflect whether C and C++ functions have the same
calling conventions in the target environment.  For example:
  extern "C" void f();         // f's type has extern "C" linkage
  void (*pf)()                 // pf points to an extern "C++" function
               = &f;           // error if conversion is not allowed
The variable is automatically turned off in strict-ANSI mode unless that is
overridden by --implicit_extern_c_type_conversion (which is available if
IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE is TRUE).
*/
#ifndef DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED
#if IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE
/* Normally the values of DEFAULT...ALLOWED and ...POSSIBLE will be the same,
   but it isn't required.  When ...POSSIBLE is TRUE, the value of global
   variable impl_conv_between_c_and_cpp_function_ptrs_allowed can be changed
   from the command-line, even if DEFAULT...ALLOWED, which specifies its
   initial value, is FALSE. */
#define DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED TRUE
#else /* !IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
#define DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED FALSE
#endif /* IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_POSSIBLE */
#endif /* DEFAULT_IMPL_CONV_BETWEEN_C_AND_CPP_FUNCTION_PTRS_ALLOWED */

/*
Flag that is TRUE if the front end should by default be aware of positional
argument notation in printf/scanf format strings.  This is a common extension
to the standard C library that allows format specifiers of the form "%xxx$..."
(where xxx is a positive decimal integer) to indicate that a particular
ellipsis argument (as opposed to the next in sequence) should be formatted.
(The "*" notation for variable-width specifiers is similarly extended to
allow for "*xxx$".)
Most Unix-like implementations of the standard C library support the
extension, but the Microsoft implementation does not.
*/
#ifndef DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS
#if __MICROSOFT_OS__
#define DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS FALSE
#else /* __MICROSOFT_OS__ */
#define DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS TRUE
#endif /* __MICROSOFT_OS__ */
#endif /* DEFAULT_CHECK_PRINTF_SCANF_POSITIONAL_ARGS */

/*
Flag that is TRUE if, by default, the K&R usual arithmetic conversion rules
with respect to "long" should be used.  This means the rules of K&R I,
Appendix A, 6.6, not the rules used by the pcc compiler.
The significant difference is in the handling of "long op unsigned int" when
int and long are the same size.  The ANSI/ISO/pcc rules say the result is
unsigned long, but K&R I says the result is long (unsigned long did not
exist in K&R I).  This is the initial value of the variable
long_preserving_rules, which is also controlled by the command-line
options --[no_]long_preserving_rules.  This feature is independent of
pcc mode.  Note that the default in C++ mode is FALSE regardless of the
setting of this flag.
*/
#ifndef DEFAULT_LONG_PRESERVING_RULES
#define DEFAULT_LONG_PRESERVING_RULES FALSE
#endif /* ifndef DEFAULT_LONG_PRESERVING_RULES */

/*
Flag to control whether in C++ a function parameter type may involve a pointer
to an array of unknown bounds.  It is the initial value of global variable
ptr_to_unknown_bound_array_allowed_in_param_type.  The variable is set to TRUE
in Microsoft and cfront compatibility modes.  It is set to FALSE in strict
ANSI mode.
*/
#ifndef DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE
#define DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE FALSE
#endif /* ifndef DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE */

/*
Flag to control whether in C++ a function parameter type may involve a
reference to an array of unknown bounds.  It is the initial value of global
variable ref_to_unknown_bound_array_allowed_in_param_type.  The variable is
set to TRUE in cfront compatibility mode.  It is set to FALSE in strict ANSI
mode.  This macro should not be set to TRUE if the macro
DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE is set to FALSE.
*/
#ifndef DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE
#define DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE FALSE
#endif /* ifndef DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE */

#if DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE && \
    !DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE
 #error -- DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE requires \
           DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE
#endif /* DEFAULT_REF_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE && ... */

/*
Flag that is TRUE if the nonstandard deduction using the qualifier
portion of a qualified name should be performed.  It is the initial
value of the global variable nonstandard_qualifier_deduction.
Nonstandard qualifier deduction permits T to be deduced in contexts
such as A<T>::B or T::B.  The standard deduction mechanism treats
these as nondeduced contexts that use the values of template parameters
that were either explicitly specified or deduced elsewhere.
*/
#ifndef DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION
#define DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION FALSE
#endif /* ifndef DEFAULT_NONSTANDARD_QUALIFIER_DEDUCTION */

/*
Flag that is TRUE if, in modes that do not do dependent lookup 
processing, a set of nonstandard lookup rules should be used in instantiations.
These rules were part of the C++98 working paper for some time during
the development of the C++98 standard.  In this mode, names are looked
up in both the namespace of the template definition and in the namespace
in which a template entity was first referenced in a way that would
require an instantiation.
*/
#ifndef DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP
#define DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP FALSE
#endif /* ifndef DEFAULT_NONSTANDARD_INSTANTIATION_LOOKUP */

/*
Flag that is TRUE if the default arguments should be retained as part
of deduced function types.  It is the initial value of the global variable
nonstandard_default_arg_deduction. 
*/
#ifndef DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION
#define DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION FALSE
#endif /* ifndef DEFAULT_NONSTANDARD_DEFAULT_ARG_DEDUCTION */

/*
Flag that is TRUE if a nonstandard nonmember using-declaration that
uses an unqualified name should be accepted.  It is the initial
value of the global variable nonstandard_using_decl_allowed.
*/
#ifndef DEFAULT_NONSTANDARD_USING_DECL_ALLOWED
#define DEFAULT_NONSTANDARD_USING_DECL_ALLOWED FALSE
#endif /* DEFAULT_NONSTANDARD_USING_DECL_ALLOWED */

/*
Flag that is TRUE if a macro with a variable number of arguments can be
introduced by adding a final '...' macro parameter.  It is the initial value
of the global variable variadic_macros_allowed.
*/
#ifndef DEFAULT_VARIADIC_MACROS_ALLOWED
#define DEFAULT_VARIADIC_MACROS_ALLOWED FALSE
#endif /* DEFAULT_VARIADIC_MACROS_ALLOWED */

/*
Flag that is TRUE if a macro with a variable number of arguments can be
introduced by appending '...' to the name of the last macro parameter.  A
TRUE value also causes deletion of the comma in a replacement text like
'x, ## __VA_ARGS__' if the variadic arguments are omitted.  It is the
initial value of the global variable extended_variadic_macros_allowed.
*/
#ifndef DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED
#define DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED FALSE
#endif /* DEFAULT_EXTENDED_VARIADIC_MACROS_ALLOWED */

/*
Flag that is TRUE if the tiebreaker processing in overload resolution
(e.g., to decide between "void f(int &)" and "void f(const int &)")
should be done late by default.  It is the initial value of the
global variable do_late_ovl_res_tiebreaker.  FALSE is the setting
required for standard conformance.
*/
#ifndef DEFAULT_DO_LATE_OVL_RES_TIEBREAKER
#define DEFAULT_DO_LATE_OVL_RES_TIEBREAKER FALSE
#endif /* ifndef DEFAULT_DO_LATE_OVL_RES_TIEBREAKER */

/*
Flag that is TRUE if, in overload resolution tiebreaker processing, two
matches can be compared for the "addition of cv-qualifier under reference"
tiebreaker even if only one of them is a reference, by default.  This is
the initial value of the global variable single_ref_qual_ovl_res_tiebreaker.
FALSE is the setting required for standard conformance.
*/
#ifndef DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER
#define DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER FALSE
#endif /* ifndef DEFAULT_SINGLE_REF_QUAL_OVL_RES_TIEBREAKER */

/*
Flag that is TRUE if no access checking should be performed on the
declarator-id of a friend function declaration.  The C++ standard requires
access checking in that case, but many C++ implementation to not perform it.
This flag is used as the initial value of the global variable
no_access_check_on_friend_declarator_ids.
*/
#ifndef DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS
#define DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS FALSE
#endif /* DEFAULT_NO_ACCESS_CHECK_ON_FRIEND_DECLARATOR_IDS */

/*
Flag that is true if the asm string manipulation routines and data
structures are needed.  These are needed when asm functions are allowed
or when Microsoft extensions (including Microsoft asms) are allowed.
*/
#if ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
#define ASM_SUPPORT_NEEDED TRUE
#else /* !(ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED) */
#define ASM_SUPPORT_NEEDED FALSE
#endif /* ASM_FUNCTION_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */

/*
When the EMBEDDED_C_ALLOWED flag is defined it either enables or disables
all Embedded C (ISO/IEC TR 18037) extensions (i.e., fixed-point types, named
address spaces, and named-register storage classes) that are not otherwise
explicitly enabled or disabled.
*/
#ifndef EMBEDDED_C_ALLOWED
#define EMBEDDED_C_ALLOWED FALSE
#endif /* ifndef EMBEDDED_C_ALLOWED */

/*
Flag that is TRUE if Embedded C (ISO/IEC TR 18037) should be enabled by
default.  If this macro is defined, set the "default enabled" macro for
each of the sub-features (e.g., DEFAULT_FIXED_POINT_ENABLED).
*/
#ifndef DEFAULT_EMBEDDED_C_ENABLED
#define DEFAULT_EMBEDDED_C_ENABLED EMBEDDED_C_ALLOWED
#else  /* ifdef DEFAULT_EMBEDDED_C_ENABLED */

#ifndef DEFAULT_FIXED_POINT_ENABLED
#define DEFAULT_FIXED_POINT_ENABLED DEFAULT_EMBEDDED_C_ENABLED
#endif /* ifndef DEFAULT_FIXED_POINT_ENABLED  */

#ifndef DEFAULT_NAMED_ADDRESS_SPACES_ENABLED
#define DEFAULT_NAMED_ADDRESS_SPACES_ENABLED DEFAULT_EMBEDDED_C_ENABLED
#endif /* ifndef DEFAULT_NAMED_ADDRESS_SPACES_ENABLED  */

#ifndef DEFAULT_NAMED_REGISTERS_ENABLED
#define DEFAULT_NAMED_REGISTERS_ENABLED DEFAULT_EMBEDDED_C_ENABLED
#endif /* ifndef DEFAULT_NAMED_REGISTERS_ENABLED  */

#endif /* ifndef DEFAULT_EMBEDDED_C_ENABLED */

/*
Flag that is TRUE if the IL and the front end code supporting Embedded C
(ISO/IEC TR 18037) fixed-point extensions (e.g., support for _Fract and _Accum
types) should be included.  Having this TRUE means the back end is prepared to
accept fixed-point IL entities.  The C-generating and C++-generating back ends
can handle fixed-point extensions (but that's useful only if the downstream
compiler also handles them).
*/
#ifndef FIXED_POINT_ALLOWED
#if EMBEDDED_C_ALLOWED
#define FIXED_POINT_ALLOWED TRUE
#else /* !EMBEDDED_C_ALLOWED */
#define FIXED_POINT_ALLOWED FALSE
#endif /* EMBEDDED_C_ALLOWED */
#endif /* ifndef FIXED_POINT_ALLOWED */

/*
Flag that is TRUE if Embedded C (ISO/IEC TR 18037) fixed-point extensions
should be enabled by default.  It is the initial value of the global variable
fixed_point_enabled.
*/
#ifndef DEFAULT_FIXED_POINT_ENABLED
#define DEFAULT_FIXED_POINT_ENABLED FIXED_POINT_ALLOWED
#endif /* DEFAULT_FIXED_POINT_ENABLED */
#if !FIXED_POINT_ALLOWED && DEFAULT_FIXED_POINT_ENABLED
 #error -- fixed-point enabling not allowed
#endif /* !FIXED_POINT_ALLOWED && DEFAULT_FIXED_POINT_ENABLED */

/*
Flag that is TRUE if the IL and the front end code supporting Embedded C
(ISO/IEC TR 18037) named address spaces should be enabled.
*/
#ifndef NAMED_ADDRESS_SPACES_ALLOWED
#if EMBEDDED_C_ALLOWED
#define NAMED_ADDRESS_SPACES_ALLOWED TRUE
#else /* !EMBEDDED_C_ALLOWED */
#define NAMED_ADDRESS_SPACES_ALLOWED FALSE
#endif /* EMBEDDED_C_ALLOWED */
#endif /* ifndef NAMED_ADDRESS_SPACES_ALLOWED */

/*
Flag that is TRUE if Embedded C (ISO/IEC TR 18037) named address space
qualifiers should be recognized by default.  This is the default initial
value of the global variable named_address_spaces_enabled.
*/
#ifndef DEFAULT_NAMED_ADDRESS_SPACES_ENABLED
#define DEFAULT_NAMED_ADDRESS_SPACES_ENABLED NAMED_ADDRESS_SPACES_ALLOWED
#endif /* DEFAULT_NAMED_ADDRESS_SPACES_ENABLED */
#if !NAMED_ADDRESS_SPACES_ALLOWED && DEFAULT_NAMED_ADDRESS_SPACES_ENABLED
 #error -- Enabling of named address spaces not allowed
#endif /* !NAMED_ADDRESS_SPACES_ALLOWED && DEFAULT_NAMED_ADDRESS_... */

/*
Flag that is TRUE if the IL and the front end code supporting Embedded C
(ISO/IEC TR 18037) named-register storage classes should be enabled.
*/
#ifndef NAMED_REGISTERS_ALLOWED
#if EMBEDDED_C_ALLOWED
#define NAMED_REGISTERS_ALLOWED TRUE
#else /* !EMBEDDED_C_ALLOWED */
#define NAMED_REGISTERS_ALLOWED FALSE
#endif /* EMBEDDED_C_ALLOWED */
#endif /* ifndef NAMED_REGISTERS_ALLOWED */

/*
Flag that is TRUE if Embedded C (ISO/IEC TR 18037) named-register storage
classes should be recognized by default.  This is the default initial value
of the global variable named_registers_enabled.
*/
#ifndef DEFAULT_NAMED_REGISTERS_ENABLED
#define DEFAULT_NAMED_REGISTERS_ENABLED NAMED_REGISTERS_ALLOWED
#endif /* DEFAULT_NAMED_REGISTERS_ENABLED */
#if !NAMED_REGISTERS_ALLOWED && DEFAULT_NAMED_REGISTERS_ENABLED
 #error -- Enabling of named registers not allowed
#endif /* !NAMED_REGISTERS_ALLOWED && DEFAULT_NAMED_REGISTERS_ENABLED */

/*
Flag that is TRUE if a macro concatenation ("a ## b") should cause a
diagnostic by default if it results in an invalid token.  This is the default
initial value of the global variable check_concatenations.
*/
#ifndef DEFAULT_CHECK_CONCATENATIONS
#define DEFAULT_CHECK_CONCATENATIONS FALSE
#endif /* ifndef DEFAULT_CHECK_CONCATENATIONS */

/*
Flag that is TRUE if U-literals (as specified by ISO/IEC TR 19769) should be
accepted by default.
*/
#ifndef DEFAULT_ULITERALS_ENABLED
#define DEFAULT_ULITERALS_ENABLED FALSE
#endif /* DEFAULT_ULITERALS_ENABLED */

EXTERN a_boolean
		cpp0x_mode;
			/* When TRUE accept language features defined by the
			   current working paper for the next C++ standard. */

EXTERN a_boolean
		right_shift_can_be_angle_brackets;
			/* When TRUE, treat right shift (">>") tokens as
			   double closing angle brackets (as mandated by the
			   C++0x working paper). */

/*
Flag that determines the value of right_shift_can_be_angle_brackets in default
C++ mode.
*/
#ifndef DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS
#define DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS FALSE
#endif /* DEFAULT_RIGHT_SHIFT_CAN_BE_ANGLE_BRACKETS */

EXTERN a_boolean
		extended_friends_enabled;
			/* When TRUE, allow the form "friend <type-name>;"
			   where <type-name> is not necessarily an elaborated
			   type specifier (an extension specified in the C++0x
			   working paper). */

EXTERN a_boolean
		mixed_string_concat_enabled;
			/* When TRUE, string literal concatenation is allowed
			   even when one of the string literals is an ordinary
			   "char" string and the other literal is a wide
			   string literal (e.g., L"a" "b" is then accepted; so
			   is "a" U"b" in modes that allow U-literals). */

EXTERN a_boolean
		static_assert_enabled;
			/* When TRUE, the C++0x construct static_assert is
			   supported. */

EXTERN a_boolean
		auto_type_specifier_enabled;
			/* When TRUE, the "auto" token can appear as a type
			   specifier (the type is implied by the mandatory
			   initializer; this is a C++0x feature). */

EXTERN a_boolean
		auto_storage_class_specifier_enabled;
			/* When TRUE, the "auto" token can appear as a storage
			   class specifier (this is the traditional meaning of
			   "auto"; the variable is TRUE by default in all
			   non-C++0x modes). */

EXTERN a_boolean
		decltype_enabled;
			/* When TRUE, the C++0x construct decltype is
			   supported. */

EXTERN a_boolean
		enable_underscore_decltype_only;
			/* When TRUE in GNU C++ mode with decltype_enabled set
			   to TRUE, the C++0x keyword decltype is disabled,
			   but the alternative __decltype is enabled with the
			   same meaning as the standard token. */

EXTERN a_boolean
		nullptr_enabled;
			/* When TRUE, the C++0x keyword "nullptr" is
			   enabled. */

EXTERN a_boolean
		cpp0x_sfinae_enabled;
			/* When TRUE, the C++0x SFINAE rules of N2634 are
			   enabled. */

EXTERN a_boolean
		cpp0x_sfinae_ignore_access;
			/* When cpp0x_sfinae_enabled is TRUE and this is TRUE,
			   access checking errors are ignored and do not cause
			   deduction failure. */

/*
Check that no mutually exclusive dialect emulations are simultaneously
enabled.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#if DEFAULT_MICROSOFT_MODE
#ifndef DEFAULT_DIALECT_SET
#define DEFAULT_DIALECT_SET TRUE
#else /* !defined(DEFAULT_DIALECT_SET) */
#define MULTIPLE_DEFAULT_DIALECTS_SET TRUE
#endif /* ifndef DEFAULT_DIALECT_SET */
#endif /* DEFAULT_MICROSOFT_MODE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if SUN_EXTENSIONS_ALLOWED
#if DEFAULT_SUN_COMPATIBILITY
#ifndef DEFAULT_DIALECT_SET
#define DEFAULT_DIALECT_SET TRUE
#else /* !defined(DEFAULT_DIALECT_SET) */
#define MULTIPLE_DEFAULT_DIALECTS_SET TRUE
#endif /* ifndef DEFAULT_DIALECT_SET */
#endif /* DEFAULT_SUN_COMPATIBILITY */
#endif /* SUN_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED
#if DEFAULT_GNU_COMPATIBILITY
#ifndef DEFAULT_DIALECT_SET
#define DEFAULT_DIALECT_SET TRUE
#else /* !defined(DEFAULT_DIALECT_SET) */
#define MULTIPLE_DEFAULT_DIALECTS_SET TRUE
#endif /* ifndef DEFAULT_DIALECT_SET */
#endif /* DEFAULT_GNU_COMPATIBILITY */
#endif /* GNU_EXTENSIONS_ALLOWED */

#ifdef MULTIPLE_DEFAULT_DIALECTS_SET
 #error -- Cannot set multiple exclusive dialects as defaults
#endif /* ifdef MULTIPLE_DEFAULT_DIALECTS_SET */

#endif /* ifndef LANG_FEAT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
