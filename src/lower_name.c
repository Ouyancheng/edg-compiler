/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_name.c -- Do name mangling for IL lowering.

*/

#include "basic_hdrs.h"
#if NEED_NAME_MANGLING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* NEED_NAME_MANGLING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if NEED_NAME_MANGLING
#if DO_IL_LOWERING
#include "templates.h"
#endif /* DO_IL_LOWERING */
#include "il_walk.h"

#if IA64_ABI
/* IA-64 name mangling codes. */
#define MANGLING_CODE_FOR_CONST 'K'
#define MANGLING_CODE_FOR_VOLATILE 'V'
#define MANGLING_CODE_FOR_RESTRICT 'r'
#define MANGLING_CODE_FOR_ELLIPSIS 'z'
#define MANGLING_CODE_FOR_EXTERN_C 'Y'
#define MANGLING_STRING_FOR_VOID "v"
#define MANGLING_STRING_FOR_WCHAR_T "w"
#define MANGLING_STRING_FOR_BOOL "b"
#define MANGLING_STRING_FOR_CHAR "c"
#define MANGLING_STRING_FOR_SIGNED_CHAR "a"
#define MANGLING_STRING_FOR_UNSIGNED_CHAR "h"
#define MANGLING_STRING_FOR_SHORT "s"
#define MANGLING_STRING_FOR_UNSIGNED_SHORT "t"
#define MANGLING_STRING_FOR_INT "i"
#define MANGLING_STRING_FOR_UNSIGNED_INT "j"
#define MANGLING_STRING_FOR_LONG "l"
#define MANGLING_STRING_FOR_UNSIGNED_LONG "m"
#define MANGLING_STRING_FOR_LONG_LONG "x"
#define MANGLING_STRING_FOR_UNSIGNED_LONG_LONG "y"
#define MANGLING_STRING_FOR_FLOAT "f"
#define MANGLING_STRING_FOR_DOUBLE "d"
#define MANGLING_STRING_FOR_LONG_DOUBLE "e"
#define MANGLING_STRING_FOR_REFERENCE "R"
#define MANGLING_STRING_FOR_POINTER "P"
#define MANGLING_STRING_FOR_POINTER_TO_MEMBER "M"
#define MANGLING_STRING_FOR_ARRAY "A"
#define MANGLING_STRING_FOR_OPERATOR_NEW "nw"
#define MANGLING_STRING_FOR_OPERATOR_DELETE "dl"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW "na"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE "da"
#define MANGLING_STRING_FOR_OPERATOR_PLUS "pl"
#define MANGLING_STRING_FOR_OPERATOR_NEGATE "ng"
#define MANGLING_STRING_FOR_OPERATOR_MINUS "mi"
#define MANGLING_STRING_FOR_OPERATOR_MULT "ml"
#define MANGLING_STRING_FOR_OPERATOR_DEREFERENCE "de"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE "dv"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER "rm"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR "eo"
#define MANGLING_STRING_FOR_OPERATOR_AND "an"
#define MANGLING_STRING_FOR_OPERATOR_ADDRESS "ad"
#define MANGLING_STRING_FOR_OPERATOR_OR "or"
#define MANGLING_STRING_FOR_OPERATOR_COMPLEMENT "co"
#define MANGLING_STRING_FOR_OPERATOR_NOT "nt"
#define MANGLING_STRING_FOR_OPERATOR_ASSIGN "aS"
#define MANGLING_STRING_FOR_OPERATOR_LT "lt"
#define MANGLING_STRING_FOR_OPERATOR_GT "gt"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_ASSIGN "pL"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_ASSIGN "mI"
#define MANGLING_STRING_FOR_OPERATOR_TIMES_ASSIGN "mL"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE_ASSIGN "dV"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER_ASSIGN "rM"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR_ASSIGN "eO"
#define MANGLING_STRING_FOR_OPERATOR_AND_ASSIGN "aN"
#define MANGLING_STRING_FOR_OPERATOR_OR_ASSIGN "oR"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT "ls"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT "rs"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT_ASSIGN "rS"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT_ASSIGN "lS"
#define MANGLING_STRING_FOR_OPERATOR_EQ "eq"
#define MANGLING_STRING_FOR_OPERATOR_NE "ne"
#define MANGLING_STRING_FOR_OPERATOR_LE "le"
#define MANGLING_STRING_FOR_OPERATOR_GE "ge"
#define MANGLING_STRING_FOR_OPERATOR_AND_AND "aa"
#define MANGLING_STRING_FOR_OPERATOR_OR_OR "oo"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS "pp"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS "mm"
#define MANGLING_STRING_FOR_OPERATOR_COMMA "cm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW_STAR "pm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW "pt"
#define MANGLING_STRING_FOR_OPERATOR_CALL "cl"
#define MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT "ix"
#define MANGLING_STRING_FOR_OPERATOR_QUESTION "qu"
#define MANGLING_STRING_FOR_CONSTRUCTOR "C9"  /* "9" changed later */
#define MANGLING_STRING_FOR_DESTRUCTOR "D9"   /* "9" changed later */
#define MANGLING_STRING_FOR_CONVERSION_FUNC "cv"

#else /* !IA64_ABI */
/* Cfront-like name mangling codes. */
#define MANGLING_CODE_FOR_CONST 'C'
#define MANGLING_CODE_FOR_VOLATILE 'V'
#define MANGLING_CODE_FOR_ELLIPSIS 'e'
#define MANGLING_CODE_FOR_EXTERN_C 'K'
#define MANGLING_STRING_FOR_VOID "v"
#define MANGLING_STRING_FOR_WCHAR_T "w"
#define MANGLING_STRING_FOR_BOOL "b"
#define MANGLING_STRING_FOR_CHAR "c"
#define MANGLING_STRING_FOR_SIGNED_CHAR "Sc"
#define MANGLING_STRING_FOR_UNSIGNED_CHAR "Uc"
#define MANGLING_STRING_FOR_SHORT "s"
#define MANGLING_STRING_FOR_UNSIGNED_SHORT "Us"
#define MANGLING_STRING_FOR_INT "i"
#define MANGLING_STRING_FOR_UNSIGNED_INT "Ui"
#define MANGLING_STRING_FOR_LONG "l"
#define MANGLING_STRING_FOR_UNSIGNED_LONG "Ul"
#define MANGLING_STRING_FOR_LONG_LONG "L"
#define MANGLING_STRING_FOR_UNSIGNED_LONG_LONG "UL"
#define MANGLING_STRING_FOR_FLOAT "f"
#define MANGLING_STRING_FOR_DOUBLE "d"
#define MANGLING_STRING_FOR_LONG_DOUBLE "r"
#define MANGLING_STRING_FOR_REFERENCE "R"
#define MANGLING_STRING_FOR_POINTER "P"
#define MANGLING_STRING_FOR_POINTER_TO_MEMBER "M"
#define MANGLING_STRING_FOR_ARRAY "A"
#define MANGLING_STRING_FOR_OPERATOR_NEW "nw"
#define MANGLING_STRING_FOR_OPERATOR_DELETE "dl"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW "nwa"
#define MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE "dla"
#define MANGLING_STRING_FOR_OPERATOR_PLUS "pl"
#define MANGLING_STRING_FOR_OPERATOR_MINUS "mi"
#define MANGLING_STRING_FOR_OPERATOR_MULT "ml"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE "dv"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER "md"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR "er"
#define MANGLING_STRING_FOR_OPERATOR_AND "ad"
#define MANGLING_STRING_FOR_OPERATOR_OR "or"
#define MANGLING_STRING_FOR_OPERATOR_COMPLEMENT "co"
#define MANGLING_STRING_FOR_OPERATOR_NOT "nt"
#define MANGLING_STRING_FOR_OPERATOR_ASSIGN "as"
#define MANGLING_STRING_FOR_OPERATOR_LT "lt"
#define MANGLING_STRING_FOR_OPERATOR_GT "gt"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_ASSIGN "apl"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_ASSIGN "ami"
#define MANGLING_STRING_FOR_OPERATOR_TIMES_ASSIGN "amu"
#define MANGLING_STRING_FOR_OPERATOR_DIVIDE_ASSIGN "adv"
#define MANGLING_STRING_FOR_OPERATOR_REMAINDER_ASSIGN "amd"
#define MANGLING_STRING_FOR_OPERATOR_EXCL_OR_ASSIGN "aer"
#define MANGLING_STRING_FOR_OPERATOR_AND_ASSIGN "aad"
#define MANGLING_STRING_FOR_OPERATOR_OR_ASSIGN "aor"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT "ls"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT "rs"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT_ASSIGN "ars"
#define MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT_ASSIGN "als"
#define MANGLING_STRING_FOR_OPERATOR_EQ "eq"
#define MANGLING_STRING_FOR_OPERATOR_NE "ne"
#define MANGLING_STRING_FOR_OPERATOR_LE "le"
#define MANGLING_STRING_FOR_OPERATOR_GE "ge"
#define MANGLING_STRING_FOR_OPERATOR_AND_AND "aa"
#define MANGLING_STRING_FOR_OPERATOR_OR_OR "oo"
#define MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS "pp"
#define MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS "mm"
#define MANGLING_STRING_FOR_OPERATOR_COMMA "cm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW_STAR "rm"
#define MANGLING_STRING_FOR_OPERATOR_ARROW "rf"
#define MANGLING_STRING_FOR_OPERATOR_CALL "cl"
#define MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT "vc"
#define MANGLING_STRING_FOR_OPERATOR_QUESTION "qs"
#define MANGLING_STRING_FOR_CONSTRUCTOR "ct"
#define MANGLING_STRING_FOR_DESTRUCTOR "dt"
#define MANGLING_STRING_FOR_CONVERSION_FUNC "op"

#endif /* IA64_ABI */

#if IA64_ABI

/*
Data structure used to represent entities in mangled names for which
substitution can be done in the IA-64 ABI name mangling scheme.
Substitution is a technique used to reduce the size of mangled names
by allowing a substitution reference of the form "Snnn_" to indicate
repetition of something that has appeared earlier in the mangled name.
*/
typedef struct a_substitution *a_substitution_ptr;
typedef struct a_substitution {
  a_substitution_ptr
		next;
			/* The next (more recent) available substitution 
			   candidate. */
  an_il_entry_kind
		kind;
			/* The kind of object represented by this 
			   substitution candidate. */
  union {
    /* When kind == iek_type: */
    a_type_ptr	type;
			/* The type to which this substitution applies.	 */
    /* When kind == iek_namespace */
    a_namespace_ptr
		namespace_ptr;
			/* The namespace to which this substitution applies. */
    /* When kind == iek_template */
    a_template_ptr
		template_ptr;
			/* The template to which this substitution applies. */
  } variant;
} a_substitution;

typedef unsigned long 
		a_substitution_index;
			/* The type of a numerical substitution index.	The
			   first index is index zero. */

#endif /* !IA64_ABI */

/*
Control block for mangling.
*/
typedef struct a_mangling_control_block *a_mangling_control_block_ptr;
typedef struct a_mangling_control_block {
  sizeof_t	length;
			/* Current length of the mangled name.  Note that
			   this differs from mangling_text_buffer->size in
			   that it does not count the blanks left as reserved
			   space for leading lengths, which will be removed
			   at the end of generating the name. */
  sizeof_t	num_leftover_spaces;
			/* Count of the extra leftover spaces described
			   above. */
#if IA64_ABI
  a_substitution_ptr
                first_substitution;
			/* The first (reading left-to-right) substitution 
			   candidate for this mangling operation. */
  a_substitution_ptr
                last_substitution;
			/* The last (most recent) substitution candidate for
			   this mangling operation. */
#else /* !IA64_ABI */
  a_boolean	suppress_partial_spec_args;
			/* TRUE to suppress extra information on partial
			   specialization arguments. */
#endif /* !IA64_ABI */
} a_mangling_control_block;


/*
Text buffer used for mangling.
*/
static a_text_buffer_ptr
		mangling_text_buffer;

/*
Second text buffer, needed when a recursive call to the mangling routines
is made, e.g., when generating a module id.
*/
static a_text_buffer_ptr
		second_mangling_text_buffer;


static void mangled_encoding_for_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl);
static void mangled_function_base_name(
                                      a_source_correspondence  *scp,
                                      a_special_function_kind  special_kind,
                                      an_opname_kind           opname_kind,
                                      unsigned int             num_operands,
                                      a_type_ptr               conversion_type,
                                      a_mangling_control_block *mctl);
static void mangled_function_name(
                              a_routine_ptr            routine,
                              a_boolean                suppress_param_encoding,
                              sizeof_t                 *base_name_offset,
                              a_mangling_control_block *mctl);
static void mangled_function_name_externalized_if_necessary(
                              a_routine_ptr            routine,
                              a_boolean                suppress_param_encoding,
                              sizeof_t                 *base_name_offset,
                              a_mangling_control_block *mctl);
static void mangled_member_variable_name(a_variable_ptr           variable,
                                         a_mangling_control_block *mctl);
static char *mangled_expr_operator_name(an_expr_operator_kind op);
static void mangled_encoding_for_expression(an_expr_node_ptr         expr,
                                            a_mangling_control_block *mctl);
static void mangled_member_name(a_source_correspondence  *scp,
                                a_boolean                is_specialization,
                                a_mangling_control_block *mctl);
static void mangled_encoding_for_constant(a_constant_ptr           con,
                                          a_boolean                old_form,
                                          a_mangling_control_block *mctl);
#if !IA64_ABI
static char *compress_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl);
#endif /* !IA64_ABI */
static char *truncate_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl);
static void r_mangled_parent_qualifier(a_source_correspondence  *scp,
                                       unsigned long            nesting_level,
                                       a_mangling_control_block *mctl);
static void mangled_template_arguments(
                                    a_template_arg_ptr       template_arg_list,
                                    a_boolean                partial_spec,
                                    a_boolean                old_form,
                                    a_mangling_control_block *mctl);
static a_boolean variable_name_mangling_needed(a_variable_ptr variable);

/*
Interface to r_mangled_parent_qualifier, to provide nesting_level == 1.
*/
#define mangled_parent_qualifier(parent, mctl)                        \
  r_mangled_parent_qualifier((parent), (unsigned long)1, (mctl))


static void clear_mangling_control_block(a_mangling_control_block_ptr mctl)
/*
Set the fields of the indicated mangling control block to default values.
*/
{
  mctl->length = 0;
  mctl->num_leftover_spaces = 0;
#if IA64_ABI
  mctl->first_substitution = NULL;
  mctl->last_substitution = NULL;
#else /* !IA64_ABI */
  mctl->suppress_partial_spec_args = FALSE;
#endif /* !IA64_ABI */
}  /* clear_mangling_control_block */

#if IA64_ABI

/* Pointer to a list of available (freed) substitutions. */
static a_substitution_ptr
		avail_substitutions;


static void alloc_substitution(char                         *entity,
                               an_il_entry_kind             kind,
                               a_mangling_control_block_ptr mctl)
/*
Allocate a substitution entry for entity, which has the indicated kind,
and add it to the list of substitutions pointed to by
mctl->first_substitution/mctl->last_substitution.
*/
{
  a_substitution_ptr sp;

  if (avail_substitutions != NULL) {
    sp = avail_substitutions;
    avail_substitutions = sp->next;
  } else {
    sp = (a_substitution_ptr)alloc_fe(sizeof(a_substitution));
  }  /* if */
  sp->kind = kind;
  switch (kind) {
    case iek_type:
      sp->variant.type = (a_type_ptr)entity;
      break;
    case iek_namespace:
      sp->variant.namespace_ptr = (a_namespace_ptr)entity;
      break;
    case iek_template:
      sp->variant.template_ptr = (a_template_ptr)entity;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  sp->next = NULL;
  if (mctl->last_substitution != NULL) {
    mctl->last_substitution->next = sp;
    mctl->last_substitution = sp;
  } else {
    mctl->first_substitution = mctl->last_substitution = sp;
  }  /* if */
}  /* alloc_substitution */

#endif /* IA64_ABI */

static void start_mangling(a_mangling_control_block_ptr mctl)
/*
Do initialization for mangling one name.  This includes clearing
mangling_text_buffer and mctl.
*/
{
  clear_mangling_control_block(mctl);
  reset_text_buffer(mangling_text_buffer);
}  /* start_mangling */


static void add_to_mangled_name(char                         ch,
                                a_mangling_control_block_ptr mctl)
/*
Add the indicated character to the mangled name.
*/
{
  /* Count characters. */
  mctl->length++;
  add_char_to_text_buffer(mangling_text_buffer, ch);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_to_mangled_name */


static void add_str_to_mangled_name(char                         *str,
                                    a_mangling_control_block_ptr mctl)
/*
Add the indicated null-terminated string to the mangled name.
*/
{
  sizeof_t len = strlen(str);

  /* Count characters. */
  mctl->length += len;
  add_to_text_buffer(mangling_text_buffer, str, len);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_str_to_mangled_name */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- mctl is not used in that case. */
#endif /* !IA64_ABI */
static void add_mangled_name_prefix(a_mangling_control_block_ptr mctl)
/*
Add any prefix required at the beginning of a mangled name.
*/
{
#if IA64_ABI
  add_str_to_mangled_name("_Z", mctl);
#endif /* IA64_ABI */
}  /* add_mangled_name_prefix */


static char *end_mangling(a_source_correspondence      *scp,
                          a_boolean                    final,
                          a_mangling_control_block_ptr mctl)
/*
Do processing at the end of mangling a name, which is in the mangling
buffer.  At the least, this includes adding the final null character.
Return the address of the mangled name in the buffer.  If scp is
non-NULL, allocate a copy of the name in the IL memory region, and
update scp to point to it.  If final is TRUE, it's okay to do final
mangling, which may produce a name that can no longer be embedded in
other mangled names.
*/
{
  char *buffer;

  /* Add the final null. */
  add_to_mangled_name('\0', mctl);
  if (mctl->num_leftover_spaces) {
    /* This string contains some leftover spaces, the result of saving extra
       room for potentially large leading length indications.  Remove those
       spaces now. */
    char *src = mangling_text_buffer->buffer;
    char *dest = src;
    char ch;
    do {
      ch = *src++;
      if (ch != ' ') {
        *dest++ = ch;
      } else {
        /* Removing a space. */
        mangling_text_buffer->size--;
        mctl->num_leftover_spaces--;
      }  /* if */
    } while (ch != '\0');
    check_assertion_str(mctl->num_leftover_spaces == 0 &&
                        mangling_text_buffer->size == mctl->length,
                        "end_mangling: wrong nunber of leftover spaces");
  }  /* if */
  buffer = mangling_text_buffer->buffer;
  if (final) {
#if !IA64_ABI
    /* Compress the mangled name to make it smaller. */
    buffer = compress_mangled_name((char *)NULL, scp, mctl);
#endif /* !IA64_ABI */
    /* Truncate the mangled name if necessary. */
    buffer = truncate_mangled_name(buffer, scp, mctl);
  }  /* if */
  if (scp != NULL) {
    /* Allocate space for the mangled name and copy it. */
    char *mangled_name = alloc_lowered_name_string(mctl->length);
    (void)strcpy(mangled_name, buffer);
    /* Save the unmangled form of the name.  Do not save the unmangled
       name for a class that was originally unnamed and has been given a
       name. */
    if (!scp->name_has_been_mangled) {
      scp->unmangled_name = scp->name;
    }  /* if */
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
    scp->final_name_mangling_pending = !final;
  }  /* if */
#if IA64_ABI
  /* Free the substitutions created during this mangling. */
  if (mctl->first_substitution != NULL) {
    mctl->last_substitution->next = avail_substitutions;
    avail_substitutions = mctl->first_substitution;
  }  /* if */
#endif /* IA64_ABI */
  return buffer;
}  /* end_mangling */


static void add_number_to_mangled_name(unsigned long            value,
                                       a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
simple output -- just the digits of the value, with no additional
encoding.
*/
{
  char buffer[50];

  (void)sprintf(buffer, "%lu", value);
  add_str_to_mangled_name(buffer, mctl);
}  /* add_number_to_mangled_name */

#if IA64_ABI

static void add_signed_number_to_mangled_name(long                     value,
                                              a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
simple output -- just the digits of the value, with no additional
encoding.  A negative value is prefixed by "n".
*/
{
  char buffer[50];

  (void)sprintf(buffer, "%ld", (long)value);
  /* Handle negative numbers by replacing '-' with 'n'. */
  if (buffer[0] == '-') buffer[0] = 'n';
  add_str_to_mangled_name(buffer, mctl);
}  /* add_signed_number_to_mangled_name */


static void add_base_36_number_to_mangled_name(a_substitution_index      value,
					       a_mangling_control_block  *mctl)
/*
Adds a base-36 representation (using digits and upper case letters) of
value to the mangled name.
*/
{
  static char          base_36_digits[37] =
                                        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  a_substitution_index power = 1;

  /* Figure out the smallest power of 36 that will contain value. */
  while (power <= value) {
    power *= 36;
  }  /* while */
  /* Pull back one power of 36 to get the multiplier of the most significant
     digit.  Make sure at least one digit is used, even for zero. */
  if (power > 1) {
    power /= 36;
  }  /* if */
  /* Scan from most significant to least significant digit. */
  do {
    /* Compute the most significant digit for the remaining value. */
    unsigned int digit = value / power;  /*lint !e414*/
    /* Emit the digit. */
    add_to_mangled_name(base_36_digits[digit], mctl);
    /* Subtract the value of the digit just emitted. */
    value -= digit * power;
    /* Now do the next smaller power of 36. */
    power /= 36;
  } while (power > 0);
}  /* add_base_36_number_to_mangled_name */


static void add_substitution_index_to_mangled_name(
                                        a_substitution_index      index,
                                        a_mangling_control_block  *mctl)
/*
Add a representation of the substitution with the given index to the mangled
name.
*/
{
  add_to_mangled_name('S', mctl);
  /* The substitution number is written in the mangling as a -1-indexed value
     in base 36.  For the first substitution, the number is omitted
     altogether. */
  if (index > 0) {
    add_base_36_number_to_mangled_name(index - 1, mctl);
  }  /* if */
  add_to_mangled_name('_', mctl);
}  /* add_substitution_index_to_mangled_name */


static a_template_ptr class_template_of(a_type_ptr type)
/*
If type is an instance of a template, return a pointer to the class template
of which type is an instance.  Return NULL otherwise.
*/
{
  a_symbol_ptr                      template_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_ptr                    class_template = NULL;

  type = skip_typedefs(type);
  if (is_immediate_class_type(type) && 
      type->variant.class_struct_union.is_template_class) {
    /* The class is an instantiation or specialization -- but it might be a
       nested class within a template class. */
    template_sym = class_template_for_type(type);
    if (template_sym != NULL) {
      tssp = template_sym->variant.template_info;
      class_template = tssp->il_template_entry;
    }  /* if */
  }  /* if */

  return class_template;
}  /* class_template_of */


/* Returns TRUE if ns is the "std" namespace. */
#define is_namespace_std(ns) \
  ((a_symbol_ptr)(ns)->source_corresp.assoc_info == symbol_for_namespace_std)

/* Returns TRUE if scp is the source correspondence for an entity that is a
   member of the "std" namespace. */
#define is_source_corresp_in_namespace_std(scp)         \
  (!(scp)->is_class_member &&                           \
   (scp)->parent.namespace_ptr != NULL &&               \
   is_namespace_std((scp)->parent.namespace_ptr))

/* Returns TRUE if il_entry is (immediately) within the "std" namespace. */
#define is_in_namespace_std(il_entry)                                \
  (is_source_corresp_in_namespace_std(&((il_entry)->source_corresp)))


static a_boolean is_Sa_substitution(a_template_ptr  template_ptr)
/*
Return TRUE if template_ptr represents ::std::allocator and thus is
eligible for the `Sa' special substitution.
*/
{
  return is_in_namespace_std(template_ptr) && has_name(template_ptr) &&
                   strcmp(template_ptr->source_corresp.name, "allocator") == 0;
}  /* is_Sa_substitution */


static a_boolean is_Sb_substitution(a_template_ptr  template_ptr)
/*
Return TRUE if template_ptr represents ::std::basic_string and thus is
eligible for the `Sb' special substitution.
*/
{
  return is_in_namespace_std(template_ptr) && has_name(template_ptr) &&
               strcmp(template_ptr->source_corresp.name, "basic_string") == 0;
}  /* is_Sb_substitution */


static a_boolean is_char_type(a_type_ptr type)
/*
Return TRUE if the indicated type is "char".
*/
{
  a_type_ptr char_type = integer_type((an_integer_kind)ik_char);

  return identical_types(type, char_type);
}  /* is_char_type */


static a_boolean is_special_char_template(a_type_ptr  type,
					  const char  *template_name)
/*
Return TRUE if type represents ::std::`template_name'<char>. 
*/
{
  a_template_ptr      template_ptr;
  a_template_arg_ptr  arg;
  a_type_ptr          arg_type;
  a_boolean           result = FALSE;

  /* Check that type is an instance of ::std::`template_name'. */
  template_ptr = class_template_of(type);
  if (template_ptr != NULL && 
      is_in_namespace_std(template_ptr) &&
      has_name(template_ptr) &&
      strcmp(template_ptr->source_corresp.name, template_name) == 0) {
    /* Check that the first template argument is char and that there is
       only one argument. */
    type = skip_typerefs(type);
    arg = type->variant.class_struct_union.extra_info->template_arg_list;
    if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type &&
        arg->next == NULL) {
      arg_type = arg->variant.type;
      if (is_char_type(arg_type)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_special_char_template */

  
static a_boolean is_Ss_substitution(a_type_ptr  type)
/*
Return TRUE if type represents ::std::string, a.k.a.
::std::basic_string<char, ::std::char_traits<char>, ::std_allocator<char> >
and thus is eligible for the `Ss' substitution.
*/
{
  a_template_arg_ptr  arg;
  a_type_ptr          arg_type;
  a_template_ptr      tmpl;
  a_boolean           result = FALSE;

  /* First check that type is a template instance of ::std::basic_string. */
  tmpl = class_template_of(type);
  if (tmpl != NULL && is_Sb_substitution(tmpl)) {
    /* Now check the template arguments. */
    /* The first argument should be char. */
    type = skip_typerefs(type);
    arg = type->variant.class_struct_union.extra_info->template_arg_list;
    if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type) {
      arg_type = arg->variant.type;
      if (is_char_type(arg_type)) {
        /* The second argument should be ::std::char_traits<char>. */
        arg = arg->next;
        if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type) {
          arg_type = arg->variant.type;
          if (is_special_char_template(arg_type, "char_traits")) {
            /* The third argument should be ::std::allocator<char> and
               should be the last argument. */
            arg = arg->next;
            if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type &&
                arg->next == NULL) {
              arg_type = arg->variant.type;
              if (is_special_char_template(arg_type, "allocator")) {
                result = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_Ss_substitution */


static a_boolean is_stream_substitution(a_type_ptr  type,
					const char  *stream_name)
/*
Return TRUE if type represents 
::std::`stream_name'<char, ::std::char_traits<char> >
and thus is eligible for a special substitution.
*/
{
  a_template_ptr      template_ptr;
  a_template_arg_ptr  arg;
  a_type_ptr          arg_type;
  a_boolean           result = FALSE;

  /* Check that type is an instance of ::std::`stream_name'. */
  template_ptr = class_template_of(type);
  if (template_ptr != NULL &&
      is_in_namespace_std(template_ptr) &&
      has_name(template_ptr) &&
      strcmp(template_ptr->source_corresp.name, stream_name) == 0) {
    /* Now check the template arguments. */
    /* The first argument should be char. */
    type = skip_typerefs(type);
    arg = type->variant.class_struct_union.extra_info->template_arg_list;
    if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type) {
      arg_type = arg->variant.type;
      if (is_char_type(arg_type)) {
        /* The second argument should be ::std::char_traits<char> and
           should be the last argument. */
        arg = arg->next;
        if (arg != NULL && arg->kind == (a_templ_arg_kind)tak_type &&
            arg->next == NULL) {
          arg_type = arg->variant.type;
          if (is_special_char_template(arg_type, "char_traits")) {
            result = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_stream_substitution */


static a_boolean add_substitution_if_available(
                                             void                     *entity,
                                             an_il_entry_kind         kind,
                                             a_mangling_control_block *mctl)
/*
If there is a substitution available for entity, add it to the mangled name
and return TRUE.  Otherwise return FALSE.  The kind indicates the kind of
entity processed.
*/
{
  a_substitution_ptr   sp;
  a_substitution_index index;
  a_boolean            result = FALSE;

  /* See if the entity is one of the special entities for which an
     abbreviation exists. */
  switch (kind) {
    case iek_type:
      {
        a_type_ptr type = (a_type_ptr)entity;
        /* Compare to ::std::string. */
        if (is_Ss_substitution(type)) {
          add_str_to_mangled_name ("Ss", mctl);
          result = TRUE;
          break;
        } else if (is_stream_substitution(type, "basic_istream")) {
          add_str_to_mangled_name ("Si", mctl);
          result = TRUE;
          break;
        } else if (is_stream_substitution(type, "basic_ostream")) {
          add_str_to_mangled_name ("So", mctl);
          result = TRUE;
          break;
        } else if (is_stream_substitution(type, "basic_iostream")) {
          add_str_to_mangled_name ("Sd", mctl);
          result = TRUE;
          break;
        }  /* if */
      }
      break;
    case iek_template:
      if (is_Sa_substitution((a_template_ptr)entity)) {
        add_str_to_mangled_name("Sa", mctl);
        result = TRUE;
      } else if (is_Sb_substitution((a_template_ptr)entity)) {
        add_str_to_mangled_name("Sb", mctl);
        result = TRUE;
      }  /* if */
      break;
    case iek_namespace:
      if (is_namespace_std((a_namespace_ptr)entity)) {
        add_str_to_mangled_name("St", mctl);
        result = TRUE;
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
  /* Otherwise, see if there is an existing substitution for something
     that appears earlier in the mangled name. */
  if (!result) {
    for (sp = mctl->first_substitution, index = 0; 
         sp != NULL; 
         sp = sp->next, index++) {
      if (sp->kind == kind) {
        switch (kind) {
          case iek_type:
            if (identical_types((a_type_ptr)entity, sp->variant.type)) {
              result = TRUE;
            }  /* if */
            break;
          case iek_namespace:
            if (same_entities((a_namespace_ptr)entity,
                              sp->variant.namespace_ptr)) {
              result = TRUE;
            }  /* if */
            break;
          case iek_template:
            if (same_entities((a_template_ptr)entity,
                              sp->variant.template_ptr)) {
              result = TRUE;
            }  /* if */
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }  /* if */
      if (result) {
        /* We found a substitution for this entity. */
        add_substitution_index_to_mangled_name(index, mctl);
        break;
      }  /* if */
    }  /* for */
  }  /* if */

  return result;
}  /* add_substitution_if_available */


static a_boolean add_substitution(char                     *entity,
                                  an_il_entry_kind         kind,
                                  a_mangling_control_block *mctl)
/*
If there is a substitution available for entity, add it to the mangled name
and return TRUE.  Otherwise return FALSE, but create a new substitution entry
for entity.  The kind indicates the kind of entity processed.
*/
{
  a_boolean result;

  result = add_substitution_if_available(entity, kind, mctl);
  if (!result) {
    alloc_substitution(entity, kind, mctl);
  }  /* if */
  
  return result;
}  /* add_substitution */


static void add_prefix_for_local_class(a_type_ptr               type,
                                       a_mangling_control_block *mctl)
/*
Add a prefix indicating the routine containing type, which is a local class,
for the IA-64 ABI.
*/
{
  a_class_symbol_supplement_ptr ssp;

  check_assertion(is_immediate_class_type(type) &&
                  type->source_corresp.is_local_to_function);
  ssp = symbol_supplement_for_class(type);
  add_to_mangled_name('Z', mctl);
  mangled_function_name(ssp->enclosing_routine,
                        /*suppress_param_encoding=*/FALSE,
                        /*base_name_offset=*/(sizeof_t *)NULL,
                        mctl);
  add_to_mangled_name('E', mctl);
}  /* add_prefix_for_local_class */


static void add_prefix_for_local_class_if_necessary(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
If type is a local type, or a member of a local class, output the prefix
indicating the routine containing the type, for the IA-64 ABI.
*/
{
  if (type->source_corresp.is_local_to_function) {
    while (type->source_corresp.is_class_member) {
      type = type->source_corresp.parent.class_type;
    }  /* while */
    if (is_enum_type(type)) {
      /* We do not have any way of getting the containing function for
         an enum at this point.  */
    } else {
      add_prefix_for_local_class(type, mctl);
    }  /* if */
  }  /* if */
}  /* add_prefix_for_local_class_if_necessary */

#endif /* IA64_ABI */

/*
Information about a spot where space was reserved by
reserve_space_for_length for later use by fill_in_length to fill in
a leading length.
*/
typedef struct a_length_reservation {
  sizeof_t	start_position;
			/* The offset in the mangling_text_buffer of the
			   first character of the space reserved for insertion
			   of the length. */
  sizeof_t	start_length;
			/* The length of the mangled name (not counting the
			   leftover spaces, which will be removed later)
			   preceding the length, used to compute the length
			   of the text following. */
} a_length_reservation;


static void reserve_space_for_length(
                                  a_length_reservation     *length_reservation,
                                  a_mangling_control_block *mctl)
/*
Reserve some space in the mangled name so that we can insert a length
later.  Return information on the position of the reserved space in
*length_reservation.
*/
{
  int i;

  length_reservation->start_position = mangling_text_buffer->size;
  length_reservation->start_length = mctl->length;
  /* Leave room for lengths of up to 9,999,999. */
#define NUM_CHARS_RESERVED_FOR_LENGTH 7
  /* Fill the space with blanks, which cannot be part of a valid mangled
     name.  We'll overwrite some of those blanks with the actual length
     determined later.  The leftover blanks will be removed at the end of
     mangling. */
  for (i = 1; i <= NUM_CHARS_RESERVED_FOR_LENGTH; i++) {
    add_to_mangled_name(' ', mctl);
  }  /* for */
  mctl->length -= NUM_CHARS_RESERVED_FOR_LENGTH;
  mctl->num_leftover_spaces += NUM_CHARS_RESERVED_FOR_LENGTH;
}  /* reserve_space_for_length */


static void fill_in_length(a_length_reservation     *length_reservation,
                           a_mangling_control_block *mctl)
/*
Fill in the length of an item in the space previously reserved by a
call of reserve_space_for_length.  *length_reservation contains the
information returned from that call.
*/
{
  sizeof_t length, length_length;
  char     *length_pos;
  char     buffer[20];

  /* Determine the length, and the number of digits needed to
     represent the length. */
  length = mctl->length - length_reservation->start_length;
  (void)sprintf(buffer, "%lu", (unsigned long)length);
  length_length = strlen(buffer);
  if (length_length > NUM_CHARS_RESERVED_FOR_LENGTH) {
    catastrophe(ec_mangled_name_too_long);
  }  /* if */
  /* Determine the position of the start of the length in the buffer. */
  length_pos = mangling_text_buffer->buffer +
               length_reservation->start_position;
  /* Copy the length. */
  (void)memcpy(length_pos, buffer, size_t_arg(length_length));
  /* The characters overwritten are no longer leftover spaces. */
  mctl->length += length_length;
  mctl->num_leftover_spaces -= length_length;
}  /* fill_in_length */


static void mangled_encoding_for_type_qualifiers(
                                           a_type_qualifier_set     qualifiers,
                                           a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the cv-qualifiers (if any)
in the set "qualifiers".
*/
{
#if IA64_ABI
  /* Note that the order matters: restrict, volatile, const must be in
     that order. */
  if (qualifiers & TQ_RESTRICT) {
    add_to_mangled_name(MANGLING_CODE_FOR_RESTRICT, mctl);
  }  /* if */
  if (qualifiers & TQ_VOLATILE) {
    add_to_mangled_name(MANGLING_CODE_FOR_VOLATILE, mctl);
  }  /* if */
  if (qualifiers & TQ_CONST) {
    add_to_mangled_name(MANGLING_CODE_FOR_CONST, mctl);
  }  /* if */
#else /* !IA64_ABI */
  if (qualifiers & TQ_CONST) {
    add_to_mangled_name(MANGLING_CODE_FOR_CONST, mctl);
  }  /* if */
  if (qualifiers & TQ_VOLATILE) {
    add_to_mangled_name(MANGLING_CODE_FOR_VOLATILE, mctl);
  }  /* if */
#ifdef MANGLING_CODE_FOR_RESTRICT
  if (qualifiers & TQ_RESTRICT) {
    add_to_mangled_name(MANGLING_CODE_FOR_RESTRICT, mctl);
  }  /* if */
#endif /* ifdef MANGLING_CODE_FOR_RESTRICT */
#endif /* IA64_ABI */
}  /* mangled_encoding_for_type_qualifiers */


static void mangled_encoding_for_parameter_types(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the parameters of function
type "type".
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param;
#if !IA64_ABI
  a_param_type_ptr              existing_param;
  unsigned long                 existing_param_num, num_matching_types;
#endif /* !IA64_ABI */

  /* The encoding for parameter types is as follows:
       (1)  For each parameter, the encoding for the type.  Except in the IA64
            ABI, if a parameter has a type that has appeared already in the
            parameter list, "Tn" is used to repeat the type of parameter "n"
            ("n" can be a multi-digit number; the first parameter is numbered
            1).  If several consecutive parameters have the same type as a
            previous parameter, "Nmn" is used to indicate "m" repetitions of
            the type of parameter "n" ("n" is as for "Tn"; "m" is a one-digit
            number, so a maximum of 9 repetitions is possible).
            If the parameter list is empty, "v" for "void".
       (2)  If the parameter list ends with an ellipsis, a code for the
            ellipsis.
  */
  rtsp = type->variant.routine.extra_info;
  param = rtsp->param_type_list;
  if (param == NULL) {
    /* Void parameter list. */
#if IA64_ABI
    /* No "v" if there is an ellipsis. */
    if (!rtsp->has_ellipsis)
#endif /* IA64_ABI */
    /* Do not add code here. */
    {
      add_to_mangled_name('v', mctl);
    }  /* if */
  } else {
    /* Output the parameter types. */
    for (; param != NULL; param = param->next) {
#if !IA64_ABI
      /* See if the parameter type is the same as any existing parameter
         type. */
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
      /* Only check the first 9 parameters to avoid multi-digit
         numbers which would cause ambiguous mangling. */
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
      for (existing_param = rtsp->param_type_list, existing_param_num = 1;
           existing_param != param
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
                                   && existing_param_num < 10
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
                                                             ;
           existing_param = existing_param->next, existing_param_num++) {
        if (types_are_compatible(existing_param->type, param->type)) {
          /* Found a type that is being reused.  See if there are more
             instances following this one, in which case we can use the "Nmn"
             encoding.  Stop when 9 matches are found, since that's the most
             that can be encoded in a single "Nmn" sequence. */
          for (num_matching_types = 1;
               num_matching_types < 9 && param->next != NULL &&
                 types_are_compatible(existing_param->type, param->next->type);
               num_matching_types++, param = param->next) {}
          if (num_matching_types == 1) {
            /* Only one match, so use the "Tn" form. */
            add_to_mangled_name('T', mctl);
          } else {
            /* More than one match, so use the "Nmn" form. */
            add_to_mangled_name('N', mctl);
            /* Output the "m" (repetition count). */
            add_number_to_mangled_name(num_matching_types, mctl);
          }  /* if */
          /* Output the "n" (existing parameter number). */
          add_number_to_mangled_name(existing_param_num, mctl);
          goto arg_done;
        }  /* if */
      }  /* for */
#endif /* !IA64_ABI */
      /* The parameter type does not match any of the previous parameter
         types, so just put it out. */
      mangled_encoding_for_type(param->type, mctl);
#if !IA64_ABI
arg_done:;
#endif /* !IA64_ABI */
    }  /* for */
  }  /* if */
  /* Output the final "e" (or "z" in the IA64 ABI) for an ellipsis. */
  if (rtsp->has_ellipsis) {
    add_to_mangled_name(MANGLING_CODE_FOR_ELLIPSIS, mctl);
  }  /* if */
}  /* mangled_encoding_for_parameter_types */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- do_markers is unused in that case. */
#endif /* !IA64_ABI */
static void mangled_encoding_for_function_type(
                                       a_type_ptr               type,
                                       a_boolean                do_return_type,
                                       a_boolean                do_markers,
                                       a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the function type "type".
The return type of the function is encoded if do_return_type is TRUE.  If
do_markers is TRUE, markers indicating the start and end of the function type,
as well as whether or not the type is extern "C", are emitted.
*/
{
  check_assertion(type->kind == (a_type_kind)tk_routine);
#if !IA64_ABI
  /* We always emit markers in the Cfront-like ABI. */
  check_assertion(do_markers);
#endif /* !IA64_ABI */
  /* The encoding for a function type is "F" followed by the encoding
     for the parameter types.  mangled_function_name takes care of putting
     out additional information preceding the "F" if the function is a
     member function. */
  if (do_markers) {
    /* Start with the "F" indicating a function type. */
    add_to_mangled_name('F', mctl);
    if (c_and_cpp_function_types_are_distinct &&
        type->variant.routine.extra_info->routine_name_linkage ==
                                           (a_name_linkage_kind)nlk_external) {
      /* The function type is marked as extern "C", and the distinction
         between extern "C" and extern "C++" is significant.  Put out a "K"
         (or a "Y" in the IA64 ABI) to mark the function type as a C
         function. */
      add_to_mangled_name(MANGLING_CODE_FOR_EXTERN_C, mctl);
    }  /* if */
  }  /* if */
#if IA64_ABI
  if (do_return_type) {
    /* Add the return type. */
    mangled_encoding_for_type(type->variant.routine.return_type, mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* Add the parameter types. */
  mangled_encoding_for_parameter_types(type, mctl);
#if !IA64_ABI
  if (do_return_type) {
    /* Add the return type at the end, as "_" followed by the type. */
    add_to_mangled_name('_', mctl);
    mangled_encoding_for_type(type->variant.routine.return_type, mctl);
  }  /* if */
#else /* IA64_ABI */
  if (do_markers) {
    /* Mark the end of the function type. */
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* IA64_ABI */
}  /* mangled_encoding_for_function_type */


static void mangled_encoding_for_function_qualifiers(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type qualifiers (if any)
on the member function type "type".
*/
{
  a_routine_type_supplement_ptr rtsp =
                              skip_typerefs(type)->variant.routine.extra_info;

  if (rtsp->this_class != NULL) {
    /* The function is a nonstatic member function. */
    /* Add any qualifiers on the "this" parameter type (actually, the type
       pointed to by the "this" parameter). */
    a_type_qualifier_set  qualifiers = rtsp->qualifiers;

    if (qualifiers != TQ_NONE) {
      mangled_encoding_for_type_qualifiers(qualifiers, mctl);
    }  /* if */
#if !IA64_ABI
  } else {
    /* Static member function. */
    add_to_mangled_name('S', mctl);
#endif /* !IA64_ABI */
  }  /* if */
}  /* mangled_encoding_for_function_qualifiers */


static void store_digits_and_underscore(unsigned long            value,
                                        a_boolean                old_form,
                                        a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
used for cases where the distinction between single-digit and multi-digit
cases needs to be indicated.  With old_form TRUE, the representation will
be simply "d" for single-digit cases, and "dd_" for multi-digit cases.
With old_form FALSE, the representation is "_dd_" regardless of the length.
*/
{
  if (old_form) {
    add_number_to_mangled_name(value, mctl);
    if (value > 9) add_to_mangled_name('_', mctl);
  } else {
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name(value, mctl);
    add_to_mangled_name('_', mctl);
  }  /* if */
}  /* store_digits_and_underscore */


static void mangled_encoding_for_template_parameter(
                                       a_template_param_coordinate *coordinate,
                                       a_template_arg_ptr          args,
                                       a_mangling_control_block    *mctl)
/*
Add to the mangled name the encoding for a template parameter with the
given coordinates.  args points to the template argument list (for a
template template parameter), if any.
*/
{
  check_assertion(distinct_template_signatures);
#if !IA64_ABI
  /* The encoding is "ZnZ" for a first-level parameter, and "Zn_mZ" for
     a non-first-level parameter, with "n" the parameter number, and
     "m" the depth number.  The "Z" on the end is to avoid ambiguities
     when this construct is followed by something that begins with a
     number, e.g., when a template parameter in a function parameter
     list is followed by a class name. */
  add_to_mangled_name('Z', mctl);
  /* Put out the parameter position number. */
  add_number_to_mangled_name((unsigned long)coordinate->position, mctl);
  if (coordinate->depth != 1) {
    /* Put out "_depth". */
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name((unsigned long)coordinate->depth, mctl);
  }  /* if */
#else /* IA64_ABI */
  /* The IA-64 encoding is "Tnnn_".  The first parameter is "T_". */
  add_to_mangled_name('T', mctl);
  /* Put out the parameter position number. */
  if (coordinate->position != 1) {
    add_number_to_mangled_name((unsigned long)coordinate->position - 2, mctl);
  }  /* if */
  add_to_mangled_name('_', mctl);
#endif /* IA64_ABI */
  if (args != NULL) {
    /* Put out template arguments of a template template parameter. */
    mangled_template_arguments(args,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               mctl);
  }  /* if */
#if !IA64_ABI
  /* Put out the final "Z" for the Cfront-like encoding. */
  add_to_mangled_name('Z', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_template_parameter */


static void mangled_encoding_for_constant_cast(a_type_ptr               type,
                                               a_constant_ptr           con,
                                               a_mangling_control_block *mctl)
/*
Add to the mangled name the mangled encoding for the constant "con" cast
to the type "type".
*/
{
#if !IA64_ABI
  a_boolean cast_to_unknown =
             (type->kind == (a_type_kind)tk_template_param &&
              type->variant.template_param.kind ==
                    (a_template_param_type_kind)tptk_unknown);

  /* Output has the form
       Ocsi1Z1ZO <-- "(int)Z1", Z1 indicating a nontype template parameter.
               ^---- "O" to end the operation encoding.
            ^^^----- Operand.
           ^-------- Count of operands, always 1 for cast.
          ^--------- Encoding for type to cast to.
        ^^---------- Operation, always "cs" for cast.
       ^------------ "O" for operation.
     mangled_encoding_for_expression generates a compatible structure, so
     if you change this be sure to change that as well.
  */
  /* If the cast is to an unknown type, omit the cast and just put out the
     underlying constant. */
  if (!cast_to_unknown) {
    /* Put out the initial "O" followed by the operator name "cs". */
    add_str_to_mangled_name("Ocs", mctl);
    /* The operator name "cs" is followed by the encoding for the
       type cast to. */
    mangled_encoding_for_type(type, mctl);
    /* Put out the count of operands. */
    add_to_mangled_name('1', mctl);
  }  /* if */
#else /* IA64_ABI */
  /* IA-64 encoding.  "cv" is the operator for a cast.  Implicit casts
     are not rendered. */
  if (con->explicit_cast_applied) {
    add_str_to_mangled_name("cv", mctl);
    mangled_encoding_for_type(type, mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* Put out the operand. */
  mangled_encoding_for_constant(con, /*old_form=*/FALSE, mctl);
#if !IA64_ABI
  if (!cast_to_unknown) {
    /* Put out the final "O". */
    add_to_mangled_name('O', mctl);
  }  /* if */
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_constant_cast */


static void mangled_encoding_for_sizeof(a_type_ptr                     type,
                                        an_expr_node_ptr               expr,
                                        a_template_param_constant_kind kind,
                                        a_mangling_control_block       *mctl)
/*
Add to the mangled name the encoding of sizeof(type), __ALIGNOF__(type),
or __uuidof(type); kind indicates which.  If expr is non-NULL, the
original form used an expression, which expr points to.
*/
{
#if !IA64_ABI
  /* Output has the form
       OszZ1Z0O <-- "sizeof(Z1)", Z1 indicating a template parameter.
              ^---- "O" to end the operation encoding.
             ^----- Count of operands, always 0 for sizeof.
          ^^^------ Encoding for type.
        ^^--------- Operation ("sz" for sizeof, "af" for __ALIGNOF__, or
                    "uu" for uuidof)
       ^----------- "O" for operation.
     mangled_encoding_for_expression generates a compatible structure, so
     if you change this be sure to change that as well.
     For an expression case, the type is replaced by "e"; there is still no
     expression.  (This is potentially a violation of the standard, since
     two templates that differ only in the expression under a sizeof could
     be mangled to the same name; however, doing mangling for full
     non-constant expressions -- think "delete[] x" -- would be quite
     a lot of additional work for very little gain.  We'll take this
     up with the standards committee.
  */
  /* Put out the initial "O". */
  add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
  /* Put out the operator name. */
  switch (kind) {
    case tpck_sizeof:
#if !IA64_ABI
      add_str_to_mangled_name("sz", mctl);
#else /* IA64_ABI */
      if (expr != NULL) {
        add_str_to_mangled_name("sz", mctl);
      } else {
        add_str_to_mangled_name("st", mctl);
      }  /* if */
#endif /* IA64_ABI */
      break;
    case tpck_alignof:
#if !IA64_ABI
      add_str_to_mangled_name("af", mctl);
#else /* IA64_ABI */
      /* Use a "vendor extended operator". */
      add_str_to_mangled_name("v111__ALIGNOF__", mctl);
#endif /* IA64_ABI */
      break;
    case tpck_uuidof:
#if !IA64_ABI
      add_str_to_mangled_name("uu", mctl);
#else /* IA64_ABI */
      /* Use a "vendor extended operator". */
      add_str_to_mangled_name("v18__uuidof", mctl);
#endif /* IA64_ABI */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* The operator name is followed by the encoding for the type or the
     expression. */
  if (expr != NULL) {
#if !IA64_ABI
    /* The expression form.  Put out "e" instead of the type. */
    add_to_mangled_name('e', mctl);
#else /* IA64_ABI */
    mangled_encoding_for_expression(expr, mctl);
#endif /* IA64_ABI */
  } else {
    /* No expression, so put out the type. */
    mangled_encoding_for_type(type, mctl);
  }  /* if */
#if !IA64_ABI
  /* Put out the count of operands. */
  add_to_mangled_name('0', mctl);
  /* Put out the final "O". */
  add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_sizeof */


static void mangled_encoding_for_float_constant(
                                             a_constant_ptr           con,
                                             a_boolean                old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_float constant con.
This is used to encode floating-point constants as part of the
mangled names of template classes.  If old_form is TRUE, use the old form
of length specification in the mangling for lengths of literals.
*/
{
  sizeof_t str_length;
  char     *str;

  /* Float: the encoding is like
       L4n1p5 <-- encoding for "-1.5"
          ^^^---- Literal value ("p" for decimal point).
         ^------- "n" indicates negative.
        ^-------- Length of the literal.
       ^--------- "L" indicates a number.
     cfront 3.0.1 does not implement this, so we made it up. */
  str = fp_to_string(skip_typerefs(con->type)->variant.float_kind,
                     &con->variant.float_value,
                     (a_boolean *)NULL, (a_boolean *)NULL, (a_boolean *)NULL);
  str_length = strlen(str);  /* Includes "-" sign if any. */
  /* Remove unnecessary trailing zeroes, e.g., change
     "1.50000e+10" to "1.5    e+10".  The blanks are then dropped
     in the copy below. */
  { char *p = strchr(str, '.'), *last_signif;
    if (p != NULL) {
      /* There is a decimal point.  Find the last significant digit
         following the decimal point. */
      /* The first digit after the decimal is considered significant even
         if it is a zero. */
      for (last_signif = ++p; isdigit((unsigned char)*p); p++) {
        if (*p != '0') last_signif = p;
      }  /* for */
      /* Change any insignificant zeroes to blanks. */
      while (last_signif < --p) {
        *p = ' ';
        str_length--;
      }  /* while */
    }  /* if */
  }
  add_to_mangled_name('L', mctl);
#if IA64_ABI
  /* Add the encoding for the type. */
  mangled_encoding_for_type(con->type, mctl);
#endif /* IA64_ABI */
  store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
  while (str_length > 0) {
    /* Move the string and recode non-alphanumeric characters. */
    char c = *str++;
    if (c == ' ') {
      /* A blank is an insignificant digit removed above. */
    } else {
      if (c == '-') {
        /* Use "n" to represent a minus sign. */
        c = 'n';
      } else if (c == '.') {
        /* Use "d" to represent a decimal point. */
        c = 'd';
      } else if (c == '+') {
        /* Use "p" to represent a plus sign. */
        c = 'p';
      }  /* if */
      add_to_mangled_name(c, mctl);
      str_length--;
    }  /* if */
  }  /* while */
#if IA64_ABI
  /* Add the end-of-literal marker. */
  add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
}  /* mangled_encoding_for_float_constant */


static void mangled_name_with_length(char                     *name,
                                     a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for a name, with a prefix that
indicates the length, e.g., "3abc" for the name "abc".  name is
null-terminated.
*/
{
  add_number_to_mangled_name((unsigned long)strlen(name), mctl);
  add_str_to_mangled_name(name, mctl);
}  /* mangled_name_with_length */


static void mangled_encoding_for_address_constant(
                                                a_constant_ptr           con,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_address constant con.
This is used to encode address constants as part of the mangled names of
template classes.
*/
{
  an_address_base_kind abkind;
#if !IA64_ABI
  char                 *str;
  a_length_reservation length_reservation;
#endif /* !IA64_ABI */

  /* The offset can be non-zero in cases where a pointer to class was
     cast to a related class.  That's ignored in the output. */
  abkind = con->variant.address.kind;
  check_assertion_str(abkind != (an_address_base_kind)abk_constant,
                      "mangled_encoding_for_address_constant: abk_constant");
#if !IA64_ABI
  /* Address of something other than a constant, i.e., a variable or
     routine.  The encoding is like
       4abcd <-- encoding for address of "abcd"
        ^^^^---- Name of entity.
       ^-------- Length of the name.
     This is compatible with cfront 3.0.1. */
  reserve_space_for_length(&length_reservation, mctl);
#else /* IA64_ABI */
  /* IA-64 encoding.  Unary "&" Operator "ad" followed by literal "L". */
  add_str_to_mangled_name("adL", mctl);
#endif /* IA64_ABI */
  if (abkind == (an_address_base_kind)abk_variable) {
    a_variable_ptr variable = con->variant.address.variant.variable;
    if (variable->source_corresp.is_class_member ||
        variable->source_corresp.parent.namespace_ptr != NULL) {
      /* Static data member or namespace member variable. */
      mangled_member_variable_name(variable, mctl);
    } else {
      /* Normal variable. */
#if !IA64_ABI
      str = unmangled_name_of(&variable->source_corresp);
      check_assertion_str(str != NULL,
                     "mangled_encoding_for_address_constant: addr of unnamed");
      add_str_to_mangled_name(str, mctl);
#else /* IA64_ABI */
      mangled_name_with_length(variable->source_corresp.name, mctl);
#endif /* IA64_ABI */
    }  /* if */
  } else if (abkind == (an_address_base_kind)abk_routine) {
    a_boolean     suppress_param_encoding = TRUE;
    a_routine_ptr routine = con->variant.address.variant.routine;
#if IA64_ABI
    add_str_to_mangled_name("_Z", mctl);
    if (is_name_linkage_kind_subject_to_name_mangling(
                                      routine->source_corresp.name_linkage)) {
      suppress_param_encoding = FALSE;
    }  /* if */
#endif /* IA64_ABI */
    mangled_function_name(routine, suppress_param_encoding,
                          /*base_name_offset=*/(sizeof_t *)NULL,
                          mctl);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (abkind == (an_address_base_kind)abk_uuidof) {
    a_type_ptr uuid_type;
    char       *uuid_str;

    /* Microsoft __uuidof. */
    /* The uuid string attached to the associated type has the format
         hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
       (where "h" is a hexadecimal digit).  The mangled form is
       a length followed by "__UUID" followed by the string, with hyphens
       removed.  This is just made up; the Microsoft compiler uses a
       completely different mangling scheme, so compatibility is a moot
       point here. */
    add_str_to_mangled_name("__UUID", mctl);
    uuid_type = con->variant.address.variant.type;
    if (uuid_type == NULL) {
      /* Null GUID case. */
      uuid_str = "00000000-0000-0000-000000000000";
    } else {
      uuid_str = uuid_type->variant.class_struct_union.extra_info->uuid_string;
      if (uuid_str == NULL) {
        /* This can happen in error cases. */
        uuid_str = "00000000-0000-0000-000000000000";
      }  /* if */
    }  /* if */
    for (; *uuid_str != '\0'; uuid_str++) {
      if (*uuid_str != '-') add_to_mangled_name(*uuid_str, mctl);
    }  /* for */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    unexpected_condition_str(
                          "mangled_encoding_for_address_constant: bad abkind");
  }  /* if */
#if !IA64_ABI
  fill_in_length(&length_reservation, mctl);
#else /* IA64_ABI */
  /* Mark end of literal. */
  add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
}  /* mangled_encoding_for_address_constant */


#if IA64_ABI
/*ARGSUSED*/ /* <-- old_form is not used in that case. */
#endif /* IA64_ABI */
static void mangled_encoding_for_ptr_to_member_constant(
                                             a_constant_ptr           con,
                                             a_boolean                old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_ptr_to_member constant con.
This is used to encode pointer-to-member constants as part of the mangled
names of template classes.  If old_form is TRUE, use the old form of length
specification in the mangling for lengths of literals.
*/
{
#if !IA64_ABI
  sizeof_t str_length;
  char     *str;
  char     buffer[50];

  /* Pointer to member:
     For pointers to data members, the offset value encoded as an integer:
       L212  <--- encoding for an offset of "12"
         ^^------ Literal value.
        ^-------- Length of the literal.
       ^--------- "L" indicates a number.
     For pointers to member functions, the __mptr triplet of
     values (delta, index, function or offset), encoded as follows:
       LM0_L2n1_1j
                ^^- Function name, or alternatively "0" if the pointer
                    to member uses an offset (e.g., LM0_L11_0).
           ^^^^---- Index value, encoded as an integer.
         ^--------- Delta value.
       ^^---------- "LM" indicates a pointer to member function.
     This is compatible with cfront 3.0.1.  Note that "0" is always
     used for the offset, not the actual offset value.  This follows
     cfront.  The idea seems to be that "0" is really a way of saying
     "there is no function;" the offset value itself would not be
     of interest to a name demangler. */
  if (!con->variant.ptr_to_member.is_function_ptr) {
    /* Pointer to data member. */
    a_targ_ptrdiff_t delta;
    repr_for_ptr_to_data_member_constant(con, &delta);
    (void)sprintf(buffer, "%ld", (long)delta);
    str = buffer;
    /* Use "n" to represent a minus sign. */
    if (str[0] == '-') str[0] = 'n';
    str_length = strlen(str);  /* Includes "-" sign if any. */
    add_to_mangled_name('L', mctl);
    store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
    add_str_to_mangled_name(str, mctl);
  } else {
    /* Pointer to member function. */
    a_targ_ptrdiff_t delta, index, offset;
    a_routine_ptr    func;

    repr_for_ptr_to_member_function_constant(con, &delta, &index, &func,
                                             &offset);
    add_str_to_mangled_name("LM", mctl);
    /* Delta value. */
    (void)sprintf(buffer, "%ld", (long)delta);
    str = buffer;
    /* Use "n" to represent a minus sign. */
    if (str[0] == '-') str[0] = 'n';
    str_length = strlen(str);  /* Includes "-" sign if any. */
    add_str_to_mangled_name(str, mctl);
    /* Index value. */
    (void)sprintf(buffer, "%ld", (long)index);
    str = buffer;
    /* Use "n" to represent a minus sign. */
    if (str[0] == '-') str[0] = 'n';
    str_length = strlen(str);  /* Includes "-" sign if any. */
    add_str_to_mangled_name("_L", mctl);
    store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
    add_str_to_mangled_name(str, mctl);
    add_to_mangled_name('_', mctl);
    if (func != NULL) {
      a_length_reservation length_reservation;
      /* Name of function. */
      /* The newer version of this includes parent information, but that's
         not compatible with cfront. */
      a_boolean include_parent_info;
#if ABI_COMPATIBILITY_VERSION < 235
      include_parent_info = FALSE;
#else /* ABI_COMPATIBILITY_VERSION >= 235 */
      /* Making this conditional on the new-style mangling for templates
         is a little strange, but if you have the new-style mangling
         you're completely incompatible with cfront, so it's not
         a ridiculous idea. */
      include_parent_info = distinct_template_signatures;
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
      reserve_space_for_length(&length_reservation, mctl);
      if (include_parent_info) {
        /* Include class and namespace information in the name. */
        mangled_function_name(func, /*suppress_param_encoding=*/TRUE, 
                              /*base_name_offset=*/(sizeof_t *)NULL,
                              mctl);
      } else {
        /* Use a simple name (no class or namespace information). */
        str = unmangled_name_of(&func->source_corresp);
        check_assertion(str != NULL);
        /* Output the name.  Stop on two underscores. */
        for (str_length = 0;
             str[str_length] != '\0' &&
               (str[str_length] != '_' || str[str_length+1] != '_');
             str_length++) {
          add_to_mangled_name(str[str_length], mctl);
        }  /* for */
      }  /* if */
      fill_in_length(&length_reservation, mctl);
    } else {
      /* Offset, always coded as "0". */
      add_to_mangled_name('0', mctl);
    }  /* if */
  }  /* if */
#else /* IA64_ABI */
  a_source_correspondence *scp = NULL;
  a_routine_ptr           rout = NULL;
  a_field_ptr             field = NULL;

  if (con->variant.ptr_to_member.is_function_ptr) {
    rout = con->variant.ptr_to_member.variant.routine;
    if (rout != NULL) {
      scp = &rout->source_corresp;
    }  /* if */
  } else {
    field = con->variant.ptr_to_member.variant.field;
    if (field != NULL) {
      scp = &field->source_corresp;
    }  /* if */
  }  /* if */
  if (scp != NULL) {
    /* Unary "&" encoding "ad" followed by scope resolution operator "sr". */
    add_str_to_mangled_name("adsr", mctl);
    add_to_mangled_name('N', mctl);
    mangled_encoding_for_type(scp->parent.class_type, mctl);
    if (rout != NULL && 
        rout->special_kind == (a_special_function_kind)sfk_conversion) {
      add_str_to_mangled_name(MANGLING_STRING_FOR_CONVERSION_FUNC, mctl);
      mangled_encoding_for_type(rout->type->variant.routine.return_type,
                                mctl);
    } else {
      mangled_name_with_length(unmangled_name_of(scp), mctl);
    }  /* if */
    add_to_mangled_name('E', mctl);
  } else {
    /* We have a NULL pointer-to-member constant.  Although not allowed by the
       standard, some compilers accept this as an extension.  The IA64 ABI
       does not specify a mangling for this case; we choose to use the same
       mangling as would be used for an integer constant of this type.  */
    add_to_mangled_name('L', mctl);
    mangled_encoding_for_type(con->type, mctl);
    add_to_mangled_name('0', mctl);
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* IA64_ABI */
}  /* mangled_encoding_for_ptr_to_member_constant */


static void mangled_encoding_for_unknown_function(
                                    a_constant_ptr           con,
                                    a_boolean                has_template_args,
                                    a_template_arg_ptr       template_arg_list,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con, which is
a ck_template_param/tpck_unknown_function constant.  This is used
to encode unknown functions that appear in template argument lists
of prototype instantiations.  If has_template_args is TRUE, the function
has an explicit template argument list, given by template_arg_list.
*/
{
  a_type_ptr              conversion_type =
                                         con->variant.template_param.variant.
                                              unknown_function.conversion_type;
  a_special_function_kind special_kind = (a_special_function_kind)sfk_none;
  a_boolean               is_member;

  /* This routine is a simplified version of mangled_function_name. */
  is_member = (con->source_corresp.is_class_member ||
               con->source_corresp.parent.namespace_ptr != NULL);
#if IA64_ABI
  if (is_in_namespace_std(con)) {
    add_str_to_mangled_name("St", mctl);
    is_member = FALSE;
  } else if (is_member) {
    if (con->source_corresp.is_class_member) {
      add_prefix_for_local_class_if_necessary(con->source_corresp.
                                                      parent.class_type, mctl);
    }  /* if */
    /* Mark the start of the nested name. */
    add_to_mangled_name('N', mctl);
    /* Add a parent qualifier for a member. */
    mangled_parent_qualifier(&con->source_corresp, mctl);
  }  /* if */
#endif /* IA64_ABI */
  if (conversion_type != NULL) {
    special_kind = (a_special_function_kind)sfk_conversion;
  }  /* if */
  mangled_function_base_name(&con->source_corresp,
                             special_kind,
                             (an_opname_kind)onk_none,
                             /*num_operands=*/0,
                             conversion_type,
                             mctl);
#if IA64_ABI
  /* Mark the end of the nested name. */
  if (is_member) {
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* IA64_ABI */
  if (has_template_args) {
    /* Put out the template argument list. */
    mangled_template_arguments(template_arg_list,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               mctl);
  }  /* if */
#if !IA64_ABI
  if (is_member) {
    /* Add a parent qualifier for a member. */
    add_str_to_mangled_name("__", mctl);
    mangled_parent_qualifier(&con->source_corresp, mctl);
  }  /* if */
#endif /* !IA64_ABI */
}  /* mangled_encoding_for_unknown_function */


static void literal_representation(a_constant_ptr           con,
                                   a_boolean                old_form,
                                   a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con.
This is used to encode constants as part of the mangled names of
template classes.  If old_form is TRUE, use the old form of length
specification in the mangling for lengths of literals.
*/
{
#if !IA64_ABI
  sizeof_t            str_length;
#endif /* !IA64_ABI */
  char                *str;
  a_boolean           has_template_args;
  a_template_arg_ptr  template_arg_list;
  a_constant_ptr      unk_func_con;

  switch (con->kind) {
    case ck_error:
      /* This might come up in mangling names for template instantiations. */
      add_to_mangled_name('?', mctl);
      break;
    case ck_integer:
#if !IA64_ABI
      /* Integer: the encoding is like
           L3n12  <-- encoding for "-12"
              ^^----- Literal value.
             ^------- "n" indicates negative.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         This is compatible with cfront 3.0.1. */
#endif /* !IA64_ABI */
      str = str_for_integer_constant(con);
      /* Use "n" to represent a minus sign. */
      if (str[0] == '-') str[0] = 'n';
#if !IA64_ABI
      str_length = strlen(str);  /* Includes "-" sign if any. */
      add_to_mangled_name('L', mctl);
      store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
      add_str_to_mangled_name(str, mctl);
#else /* IA64_ABI */
      /* IA-64 ABI: "L", type of literal, value, terminating "E". */
      add_to_mangled_name('L', mctl);
      mangled_encoding_for_type(con->type, mctl);
      add_str_to_mangled_name(str, mctl);
      add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
      break;
    case ck_float:
      /* Float constant. */
      mangled_encoding_for_float_constant(con, old_form, mctl);
      break;
    case ck_address:
      /* Address.  Put out the name of the entity whose address is involved. */
      mangled_encoding_for_address_constant(con, mctl);
      break;
    case ck_ptr_to_member:
      /* Pointer to member. */
      mangled_encoding_for_ptr_to_member_constant(con, old_form, mctl);
      break;
    case ck_template_param:
      /* This comes up when mangling the names for template entities using
         the modern mangling approach. */
      switch (con->variant.template_param.kind) {
        case tpck_param:
          /* A simple reference to a template parameter. */
          mangled_encoding_for_template_parameter(
                              &con->variant.template_param.variant.coordinates,
                              (a_template_arg *)NULL,
                              mctl);
          break;
        case tpck_expression:
          /* An expression involving template parameters. */
          mangled_encoding_for_expression(
                                      con->variant.template_param.variant.expr,
                                      mctl);
          break;
        case tpck_template_ref:
          /* An unknown function template with a list of explicit template
             arguments.  The template is given by an underlying
             tpck_unknown_function constant. */
          has_template_args = TRUE;
          template_arg_list = con->variant.template_param.variant.
                                                         template_ref.arg_list;
          unk_func_con = con->variant.template_param.variant.template_ref.con;
          check_assertion(unk_func_con->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
                          unk_func_con->variant.template_param.kind ==
                        (a_template_param_constant_kind)tpck_unknown_function);
          goto do_unknown_function;
        case tpck_unknown_function:
          /* An unknown function, which may be a member of a class or
             namespace, and may be a conversion function (if conversion_type
             is non-NULL). */
          has_template_args = FALSE;
          template_arg_list = NULL;
          unk_func_con = con;
do_unknown_function:
          { 
#if !IA64_ABI
            a_length_reservation length_reservation;
            reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
            mangled_encoding_for_unknown_function(unk_func_con,
                                                  has_template_args,
                                                  template_arg_list,
                                                  mctl);
#if !IA64_ABI
            fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
          }
          break;
        case tpck_member:
          /* A member of a template parameter type, e.g., T::x. */
          { 
#if !IA64_ABI
            a_length_reservation length_reservation;
            reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
            mangled_member_name(&con->source_corresp,
                                /*is_specialization=*/FALSE,
                                mctl);
#if !IA64_ABI
            fill_in_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
          }
          break;
        case tpck_cast:
          mangled_encoding_for_constant_cast(
                                  con->type,
                                  con->variant.template_param.variant.constant,
                                  mctl);
          break;
        case tpck_address:
#if !IA64_ABI
          /* For an address, just mangle the member name. */
          literal_representation(con->variant.template_param.variant.constant,
                                 old_form, mctl);
#else /* IA64_ABI */
          { a_source_correspondence *scp;
            /* Unary "&" operator "ad" followed by scope resolution "sr". */
            add_str_to_mangled_name("adsr", mctl);
            con = con->variant.template_param.variant.constant;
            check_assertion(con->kind == 
                                   (a_constant_repr_kind)ck_template_param &&
                            con->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_member);
            scp = &con->source_corresp;
            mangled_encoding_for_type(scp->parent.class_type, mctl);
            mangled_name_with_length(unmangled_name_of(scp), mctl);
          }
#endif /* IA64_ABI */
          break;
        case tpck_sizeof:
        case tpck_alignof:
        case tpck_uuidof:
          mangled_encoding_for_sizeof(
                         con->variant.template_param.variant.templ_sizeof.type,
                         con->variant.template_param.variant.templ_sizeof.expr,
                         con->variant.template_param.kind,
                         mctl);
          break;
        default:
          unexpected_condition_str(
                            "literal_representation: bad template param kind");
      }  /* switch */
      break;
#if CHECKING
    case ck_string:
      /* Strings should be converted to addresses. */
    default:
      internal_error("literal_representation: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
}  /* literal_representation */


static void mangled_encoding_for_constant(a_constant_ptr           con,
                                          a_boolean                old_form,
                                          a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con.
If old_form is TRUE, use the old form of length specification in the
mangling for lengths of literals.
*/
{
#if !IA64_ABI
  /* Representation is something like
       CiL15   <-- integer constant 5
           ^-- Literal constant representation.
          ^--- Length of literal constant.
         ^---- L indicates literal constant; c indicates address
               of variable, etc.
       ^^----- Type of constant, with "const" added.
     If the constant is a template parameter constant, skip the "C" and
     the type.  Likewise for an address constant. */
  if (con->kind != (a_constant_repr_kind)ck_template_param &&
      con->kind != (a_constant_repr_kind)ck_address) {
    add_to_mangled_name('C', mctl);
    /* Put out the constant type. */
    mangled_encoding_for_type(con->type, mctl);
  }  /* if */
#endif /* !IA64_ABI */
  /* Put out the literal representation for the constant. */
  literal_representation(con, old_form, mctl);
}  /* mangled_encoding_for_constant */


static void mangled_encoding_for_expression(an_expr_node_ptr         expr,
                                            a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the expression pointed to by expr.
These expressions come up in ck_template_param expressions as template
arguments, and as dimensions of arrays in template signatures.
*/
{
  char             *operation_name;
  an_expr_node_ptr operand;
#if !IA64_ABI
  unsigned long    num_operands;
#endif /* !IA64_ABI */

  /* Drop eok_lvalue and eok_rvalue. */
  while (is_operation_node(expr) &&
         (expr->variant.operation.kind == (an_expr_operator_kind)eok_lvalue ||
          expr->variant.operation.kind == (an_expr_operator_kind)eok_rvalue
#if IA64_ABI
          /* Also drop unary plus in the IA-64 ABI.  There's no representation
             for it. */
                                                                            ||
          expr->variant.operation.kind == (an_expr_operator_kind)eok_unary_plus
          /* Also drop implicit casts. */
                                                                            ||
          (expr->variant.operation.kind == (an_expr_operator_kind)eok_cast &&
           expr->variant.operation.compiler_generated)
#endif /* IA64_ABI */
                                                                           )) {
    expr = expr->variant.operation.operands;
  }  /* while */
  switch (expr->kind) {
    case enk_constant:
      mangled_encoding_for_constant(expr->variant.constant,
                                    /*old_form=*/FALSE,
                                    mctl);
      break;
    case enk_operation:
#if MICROSOFT_EXTENSIONS_ALLOWED
      check_assertion(expr->variant.operation.kind !=
                                            (an_expr_operator_kind)eok_assume);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !IA64_ABI
      /* Operation.  Output has the form
           Opl2Z1ZZ2ZO <-- "Z1 + Z2", Z1/Z2 indicating nontype template
                           parameters.
                     ^---- "O" to end the operation encoding.
                  ^^^----- Second operand.
               ^^^-------- First operand.
              ^----------- Count of operands.
            ^^------------ Operation, using same encoding as for operator
                           function names.
           ^-------------- "O" for operation.
         mangled_encoding_for_constant_cast generates a compatible structure,
         so if you change this be sure to change that as well.
      */
      /* Put out the initial "O". */
      add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
      /* Get the operator name and put it out. */
      operation_name= mangled_expr_operator_name(expr->variant.operation.kind);
      add_str_to_mangled_name(operation_name, mctl);
      /* For a cast, put out the type cast to. */
      if (operation_name[0] == 'c' && 
#if !IA64_ABI
                                      operation_name[1] == 's'
#else /* IA64_ABI */
                                      operation_name[1] == 'v'
#endif /* IA64_ABI */          
                                                              ) {
        mangled_encoding_for_type(expr->type, mctl);
      }  /* if */
#if !IA64_ABI
      /* Put out the count of operands. */
      for (num_operands = 0, operand = expr->variant.operation.operands;
           operand != NULL;
           num_operands++, operand = operand->next) {}
      check_assertion(num_operands <= 9);
      add_number_to_mangled_name(num_operands, mctl);
#endif /* !IA64_ABI */
      /* Put out the operands. */
      for (operand = expr->variant.operation.operands;
           operand != NULL;
           operand = operand->next) {
        mangled_encoding_for_expression(operand, mctl);
      }  /* for */
#if !IA64_ABI
      /* Put out the final "O". */
      add_to_mangled_name('O', mctl);
#endif /* !IA64_ABI */
      break;
    default:
      unexpected_condition_str("mangled_encoding_for_expression: bad kind");
  }  /* switch */
}  /* mangled_encoding_for_expression */


/*
Seed number for unnamed class names.
*/
static unsigned long
		unnamed_class_name_seed;


static void give_unnamed_class_a_name(a_type_ptr type)
/*
If the indicated class type is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;
  char     buffer[50];

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The class is unnamed, so make up a name. */
    /* The name is __Cnn, where nn is a unique number for the
       class.  This is not from the ARM.  cfront uses the __Cn form, but
       the number is different. */
    unnamed_class_name_seed++;
    (void)sprintf(buffer, "__C%lu", (unsigned long)unnamed_class_name_seed);
    name_len = strlen(buffer) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, buffer);
    type->source_corresp.name = name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_class_a_name */


static void give_unnamed_namespace_a_name(a_namespace_ptr nsp)
/*
If the indicated namespace is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;

  /* Note that we may be changing a namespace that is not being lowered yet,
     but that's okay -- the name in the IL entry is not used by the front
     end. */
  if (nsp->source_corresp.name == NULL) {
    /* The namespace is unnamed, so make up a name. */
    char            *module_id;
    a_namespace_ptr parent_nsp;
    /* The name is __N followed by the module id. */
    check_assertion(!nsp->source_corresp.is_class_member);
    parent_nsp = nsp->source_corresp.parent.namespace_ptr;
    if (parent_nsp != NULL &&
        unmangled_name_of(&parent_nsp->source_corresp) == NULL) {
      /* A nested unnamed namespace within an unnamed namespace.
         Just use __N.  The name will be unique within the parent namespace. */
      module_id = "";
    } else {
      a_translation_unit_ptr tup;
      check_assertion(!nsp->is_namespace_alias);
      tup = trans_unit_for_scope[nsp->variant.assoc_scope->number];
      if (curr_translation_unit == tup) {
        /* Normal case -- the namespace is from the current translation
           unit. */
        /* Because generation of the module id can make a recursive call
           to the name mangling routines, save and restore the mangling
           buffer. */
        a_text_buffer_ptr saved_text_buffer = mangling_text_buffer;
        check_assertion(mangling_text_buffer != second_mangling_text_buffer);
        if (second_mangling_text_buffer == NULL) {
          second_mangling_text_buffer = alloc_text_buffer(2048);
        }  /* if */
        mangling_text_buffer = second_mangling_text_buffer;
        module_id = make_module_id();
        mangling_text_buffer = saved_text_buffer;
      } else {
        /* The namespace is from a translation unit other than the current
           one. */
        module_id = *(tup->module_id_ptr);
        /* The module id must have been generated already. */
        check_assertion(module_id != NULL);
      }  /* if */
    }  /* if */
    name_len = 3 + strlen(module_id) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, "__N");
    (void)strcpy(name+3, module_id);
    nsp->source_corresp.name = name;
    nsp->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_namespace_a_name */


/*
Seed number for unnamed enum names.
*/
static unsigned long
		unnamed_enum_name_seed;


static void give_unnamed_enum_a_name(a_type_ptr type)
/*
If the indicated enum type is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;
  char     buffer[50];

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The enum is unnamed, so make up a name. */
    /* The name is __Enn, where nn is a unique number for the
       enum.  This is not from the ARM.  cfront uses the __En form, but
       the number is different. */
    unnamed_enum_name_seed++;
    (void)sprintf(buffer, "__E%lu", (unsigned long)unnamed_enum_name_seed);
    name_len = strlen(buffer) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, buffer);
    type->source_corresp.name = name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_enum_a_name */


/*
Seed number for unnamed member variable names.
*/
static unsigned long
		unnamed_member_variable_name_seed;


static void give_unnamed_member_variable_a_name(a_variable_ptr var)
/*
If the indicated member variable is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;
  char     buffer[50];

  if (var->source_corresp.name == NULL) {
    /* The member variable is unnamed, so make up a name. */
    /* The name is __Vnn, where nn is a unique number for the
       member variable.  This is not from the ARM or cfront. */
    unnamed_member_variable_name_seed++;
    (void)sprintf(buffer, "__V%lu",
                  (unsigned long)unnamed_member_variable_name_seed);
    name_len = strlen(buffer) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, buffer);
    var->source_corresp.name = name;
    var->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_member_variable_a_name */


static void mangled_encoding_for_template_template_argument(
                                                a_template_arg_ptr       tap,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template template argument
given by tap.
*/
{
  a_template_ptr temp = tap->variant.templ;

  if (temp->kind == (a_template_kind)templk_template_template_param) {
    /* The value of the argument is itself a template template parameter. */
    mangled_encoding_for_template_parameter(
                                     &temp->coordinates,
                                     (a_template_arg *)NULL,
                                     mctl);
  } else {
    /* The value of the argument is a template. */
    a_source_correspondence *scp = &temp->source_corresp;
#if !IA64_ABI
    a_length_reservation    length_reservation;

    /* Name of template.  The encoding is like
         4abcd <-- encoding for template "abcd"
          ^^^^---- Name of entity.
         ^-------- Length of the name.
    */
    check_assertion(scp->name != NULL);
    reserve_space_for_length(&length_reservation, mctl);
    /* Put out the base part of the name. */
    add_str_to_mangled_name(scp->name, mctl);
    if (scp->is_class_member || scp->parent.namespace_ptr != NULL) {
      /* Add two underscores after the name. */
      add_str_to_mangled_name("__", mctl);
      /* Put out the name of the class or namespace of which this template
         is a member. */
      mangled_parent_qualifier(scp, mctl);
    }  /* if */
    fill_in_length(&length_reservation, mctl);
#else /* IA64_ABI */
    if (!add_substitution((char *)temp,
                          (an_il_entry_kind)iek_template, mctl)) {
      a_boolean is_member = FALSE;
      if (is_in_namespace_std(temp)) {
        add_str_to_mangled_name("St", mctl);
      } else if (scp->is_class_member || scp->parent.namespace_ptr != NULL) {
        is_member = TRUE;
        /* Mark the start of the nested name. */
        add_to_mangled_name('N', mctl);
        /* Add the qualifying name.  The count starts at 2 because the template
           name itself is level 1. */
        r_mangled_parent_qualifier(scp, (unsigned long)2, mctl);
      }  /* if */
      /* Add the name for the template itself. */
      mangled_name_with_length(scp->name, mctl);
      if (is_member) {
        /* Mark the end of the nested name. */
        add_to_mangled_name('E', mctl);
      }  /* if */
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
}  /* mangled_encoding_for_template_template_argument */


#if IA64_ABI
/*ARGSUSED*/ /* <-- partial_spec is unused in that case. */
#endif /* IA64_ABI */
static void mangled_template_arguments(
                                    a_template_arg_ptr       template_arg_list,
                                    a_boolean                partial_spec,
                                    a_boolean                old_form,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template arguments given
by template_arg_list.  If partial_spec is TRUE, this argument list is
the first one on a partial specialization.  If old_form is TRUE, use
the old form of length specification in the mangling for lengths of
literals.
*/
{
  a_template_arg_ptr   tap;
#if !IA64_ABI
  char                 *str;
  a_length_reservation length_reservation;
  a_boolean            saved_suppress_partial_spec_args =
                                              mctl->suppress_partial_spec_args;

  /* The mangled form of template arguments is something like
       __tm__3_ii
               ^^--- Two template arguments of type int.
             ^------ Total length of template argument list string,
                     including the underscore.
         ^^--------- Fixed string, indicates "parameterized type".
     When distinct_template_signatures is FALSE, "__pt__" is used instead
     of "__tm__".  For the first argument list of a partial specialization,
     "__ps__" is used.
  */
  if (!distinct_template_signatures) {
    str = "__pt__";
  } else if (partial_spec) {
    str = "__ps__";
  } else {
    str = "__tm__";
  }  /* if */
  add_str_to_mangled_name(str, mctl);
#if ABI_COMPATIBILITY_VERSION > 245
  /* Suppress information on partial specializations in any parent types
     referenced in the template arguments. */
  mctl->suppress_partial_spec_args = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION > 245 */
  reserve_space_for_length(&length_reservation, mctl);
  add_to_mangled_name('_', mctl);
#else /* IA64_ABI */
  /* Mark the start of the template arguments. */
  add_to_mangled_name('I', mctl);
#endif /* IA64_ABI */
  /* Run through the template argument list, determining the representation
     for each argument. */
  for (tap = template_arg_list; tap != NULL; tap = tap->next) {
    if (is_type_templ_arg(tap)) {
      /* Type argument. */
      mangled_encoding_for_type(tap->variant.type, mctl);
    } else if (is_template_templ_arg(tap)) {
      /* A template template argument. */
      mangled_encoding_for_template_template_argument(tap, mctl);
    } else {
#if IA64_ABI
      a_constant_ptr con;
      a_boolean      is_expression = FALSE;
#endif /* IA64_ABI */
      check_assertion_str2(!tap->is_array_bound_of_unknown_type,
                           "mangled_template_arguments:",
                           "is_array_bound_of_unknown_type set");
#if !IA64_ABI
      /* Constant argument.  The encoding for the constant begins with
         an "X". */
      add_to_mangled_name('X', mctl);
#else /* IA64_ABI */
      /* If this is argument is an expression, mark it accordingly. */
      con = tap->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_template_param ||
          con->kind == (a_constant_repr_kind)ck_ptr_to_member ||
          con->kind == (a_constant_repr_kind)ck_address) {
        /* These are treated as expressions. */
        is_expression = TRUE;
        /* Mark the start of the expression. */
        add_to_mangled_name('X', mctl);
      }  /* if */
#endif /* IA64_ABI */
      mangled_encoding_for_constant(tap->variant.constant,
                                    old_form,
                                    mctl);
#if IA64_ABI
      if (is_expression) {
        /* Mark the end of the expression. */
        add_to_mangled_name('E', mctl);
      }  /* if */
#endif /* IA64_ABI */
    }  /* if */
  }  /* for */
#if !IA64_ABI
  /* Go back and fill in the length. */
  fill_in_length(&length_reservation, mctl);
  mctl->suppress_partial_spec_args = saved_suppress_partial_spec_args;
#else /* IA64_ABI */
  /* Mark the end of the template arguments. */
  add_to_mangled_name('E', mctl);
#endif /* IA64_ABI */
}  /* mangled_template_arguments */

#if !IA64_ABI

static void mangled_specialization_indication(a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding qualifier that indicates specialization.
*/
{
  add_str_to_mangled_name("__S", mctl);
}  /* mangled_specialization_indication */


static void add_local_name_suffix(unsigned long            id_number,
                                  a_routine_ptr            routine,
                                  a_mangling_control_block *mctl)
/*
Add a suffix to the current name for a function-local entity.  id_number
is an id number for the entity (usually a scope number within the routine)
and "routine" is the routine to which the entity is local.
*/
{
  /* The mangling is "__Lnn", where "nn: is the id number, followed by the
     mangled name of the function. */
  add_str_to_mangled_name("__L", mctl);
  add_number_to_mangled_name(id_number, mctl);
  add_str_to_mangled_name("__", mctl);
  if (routine->source_corresp.name != NULL) {
    mangled_function_name_externalized_if_necessary(
                                         routine,
                                         /*suppress_param_encoding=*/FALSE,
                                         /*base_name_offset=*/(sizeof_t *)NULL,
                                         mctl);
  }  /* if */
}  /* add_local_name_suffix */

#endif /* !IA64_ABI */

#if IA64_ABI
/*ARGSUSED*/ /* <-- show_partial_spec_args, show_template_specialization,
                    and show_specialization are not used in that case. */
#else /* !IA64_ABI */
/*ARGSUSED*/ /* <-- show_length is not used in that case. */
#endif /* IA64_ABI */
static void mangled_full_class_name(
                         a_type_ptr               type,
                         a_boolean                show_partial_spec_args,
                         a_boolean                show_template_specialization,
                         a_boolean                show_specialization,
                         a_boolean                show_length,
                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is not the version that contains a leading count of the number
of characters in the name; here, the name is usually just the original
name, but is different if the class is a template class or is unnamed.
Also, this routine does not do anything special with nested types.
show_partial_spec_args is TRUE if template arguments for a partial
specialization should be put out.  show_template_specialization is TRUE
if the class is generated from a specialization of a template and an
indication of that fact should be put out.  show_specialization is TRUE
if the class is itself a specialization and an indication of that fact
should be put out.  show_length is TRUE if the length of the class name
should be put out.
*/
{
  char                        *name;
  a_class_type_supplement_ptr ctsp;
  a_template_arg_ptr          template_args;
#if !IA64_ABI
  a_boolean                   use_previously_mangled_name = FALSE;
#endif /* !IA64_ABI */

  check_assertion(is_immediate_class_type(type));
  ctsp = type->variant.class_struct_union.extra_info;
  check_assertion_str(ctsp != NULL,
                      "mangled_full_class_name: no class type supplement");
#if !IA64_ABI
  if (type->source_corresp.name_has_been_mangled) {
    /* The name is already mangled, including any template parameters.
       We can use the mangled form unless we need to add specialization
       indicators, which are not present in the saved mangled form, or
       unless the name has been processed in some way that prevents its
       use as part of another mangled name. */
    if (!show_partial_spec_args &&
        !show_template_specialization &&
        !show_specialization &&
        !type->source_corresp.mangled_name_cannot_be_included_in_other_name) {
      use_previously_mangled_name = TRUE;
    }  /* if */
  }  /* if */
  if (use_previously_mangled_name) {
    /* Use the previously mangled version of the name. */
    add_str_to_mangled_name(type->source_corresp.name, mctl);
  } else 
#endif /* !IA64_ABI */
  /* Do not add code here. */
  {
    /* Develop the mangled name. */
    /* See if template arguments are needed.  For partial specializations,
       there are two argument lists. */
    template_args = ctsp->template_arg_list;
#if IA64_ABI
    if (template_args != NULL) {
      a_template_ptr tmpl = class_template_of(type);
      check_assertion(tmpl != NULL);
      /* Create a substitution entry for the template.  */
      (void)add_substitution((char *)tmpl,
                             (an_il_entry_kind)iek_template,
                             mctl);
    }  /* if */
#endif /* IA64_ABI */
    /* Always start with the name of the class, which applies even in the
       template class case. */
    name = unmangled_name_of(&type->source_corresp);
    if (name == NULL) {
      /* For an unnamed class, generate a name (or use the name previously
         generated). */
      give_unnamed_class_a_name(type);
      name = type->source_corresp.name;
    }  /* if */
#if IA64_ABI
    if (show_length) {
      add_number_to_mangled_name((unsigned long)strlen(name), mctl);
    }  /* if */
#endif /* IA64_ABI */
    add_str_to_mangled_name(name, mctl);
#if !IA64_ABI
    if (mctl->suppress_partial_spec_args) show_partial_spec_args = FALSE;
#if ABI_COMPATIBILITY_VERSION < 241
    /* Before this change, all names included partial specialization
       arguments. */
    show_partial_spec_args = distinct_template_signatures;
#endif /* ABI_COMPATIBILITY_VERSION < 241 */
    if (show_partial_spec_args &&
        ctsp->partial_spec_template_arg_list != NULL) {
      /* A partial specialization.  The first list is the argument list
         from the prototype instantiation of the partial specialization.
           template <class T> struct A { ... };
           template <class T> struct A<T *> { ... };
                                       ^^^this argument list
      */
      a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
      a_class_type_supplement_ptr   proto_ctsp;

      if (type->variant.class_struct_union.is_prototype_instantiation) {
        proto_ctsp = ctsp;
      } else {
        a_symbol_ptr proto_sym = cssp->corresp_prototype_sym;
        a_type_ptr   proto_type = proto_sym->variant.class_struct_union.type;
        proto_ctsp = proto_type->variant.class_struct_union.extra_info;
      }  /* if */
      mangled_template_arguments(proto_ctsp->template_arg_list,
                                 /*partial_spec=*/TRUE,
                                 /*old_form=*/FALSE,
                                 mctl);
      /* The second argument list is the deduced argument values for the
         template parameter list of the partial specialization. */
      template_args = ctsp->partial_spec_template_arg_list;
    }  /* if */
    if (show_template_specialization) {
      /* Put out an indication of the fact the template from which this
         class is generated is specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
#endif /* !IA64_ABI */
    if (template_args != NULL) {
      /* A template class.  Add information on template arguments. */
      /* old_form=TRUE forces use of the cfront-compatible mangling convention
         for lengths on literals, which though ambiguous is okay here because
         the class cannot be followed by an "_". */
      a_boolean old_form = !distinct_template_signatures;
#if ABI_COMPATIBILITY_VERSION < 235
      old_form = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
      mangled_template_arguments(template_args,
                                 /*partial_spec=*/FALSE,
                                 old_form,
                                 mctl);
    }  /* if */
#if !IA64_ABI
    if (show_specialization) {
      /* Put out an indication of the fact that this class is specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
    /* If the class is a local class, put out "__Lnn" using the declaration
       scope number for "nn".  This is not from the ARM.  cfront uses a
       similar form but it also includes the function mangling in the name
       and the number is probably different. */
    /* Don't do this for nested classes. */
    if (type->source_corresp.is_local_to_function &&
        !type->source_corresp.is_class_member) {
      /* This is a local name. */
      a_class_symbol_supplement_ptr ssp = symbol_supplement_for_class(type);
      add_local_name_suffix(ssp->local_class_number, ssp->enclosing_routine,
                            mctl);
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
}  /* mangled_full_class_name */


/*
Interface to mangled_full_class_name for the case where
show_partial_spec_args, show_template_specialization, show_specialization and
show_length are FALSE (meaning no information about those things
should be put out).
*/
#define mangled_basic_class_name(type, mctl)                          \
  mangled_full_class_name((type), FALSE, FALSE, FALSE, FALSE, (mctl))

#if IA64_ABI

static void add_discriminator_if_necessary(a_source_correspondence  *scp,
                                           a_mangling_control_block *mctl)
/*
The entity (of kind entry_kind) whose source correspondence entry is
scp is local to the function "routine".  Add a discriminator to the
mangled name if necessary.  A discriminator is a number used in the
IA-64 ABI to distinguish function-local entities with the same name.
*/
{
  a_discriminator discriminator = 0;
  a_symbol_ptr  sym = (a_symbol_ptr)scp->assoc_info;

  if (scp->is_local_to_function && sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_constant &&
        is_enum_constant(sym->variant.constant)) {
      /* This is an enumerator constant.  The constant itself never appears
         to ABI consumers (only its value as a template argument), but for
         the purpose of generating C code, we do need to ensure uniqueness.
         To that end, use the discriminator of the enum type. */
      a_type_ptr  enum_type = skip_typerefs(sym->variant.constant->type);
      check_assertion(is_immediate_enum_type(enum_type));
      sym = (a_symbol_ptr)enum_type->source_corresp.assoc_info;
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_variable) {
      discriminator = sym->variant.variable.discriminator;
    } else if (is_class_struct_union_symbol(sym) &&
               sym->variant.class_struct_union.extra_info != NULL) {
      discriminator = sym->variant.class_struct_union.extra_info
                         ->discriminator;
    } else if (sym->kind == (a_symbol_kind)sk_enum_tag) {
      discriminator = sym->variant.enumeration.discriminator;
    }  /* if */
    if (discriminator > 0) {
      add_to_mangled_name('_', mctl);
      add_number_to_mangled_name((unsigned long)(discriminator - 1), mctl);
    }  /* if */
  }  /* if */
}  /* add_discriminator_if_necessary */

#endif /* IA64_ABI */

static void mangled_class_encoding(
                         a_type_ptr               type,
                         a_boolean                show_partial_spec_args,
                         a_boolean                show_template_specialization,
                         a_boolean                show_specialization,
                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is the version that contains a leading count of the number of
characters in the name, but not information on parents.  If the class
is a proxy class for a template parameter, the encoding for the template
parameter is put out (without a length).  If the class is a template
template parameter with a template argument list, put out an encoding
for that.  show_partial_spec_args is TRUE if template arguments for a
partial specialization should be put out.  show_template_specialization
is TRUE if the class is generated from a specialization of a template
and an indication of that fact should be put out.  show_specialization
is TRUE if the class is itself a specialization and an indication of
that fact should be put out.
*/
{
  a_type_ptr template_param = NULL;
  char       *name;

  check_assertion(is_immediate_class_type(type));
  if (type->source_corresp.assoc_info != NULL) {
    /* See if this class is a proxy class for a template parameter.  If so,
       we will use the template parameter encoding. */
    template_param =
             symbol_supplement_for_class(type)->template_param_for_proxy_class;
  }  /* if */
  if (template_param != NULL) {
    /* This class is the proxy for a template parameter.  Use the encoding
       for the template parameter as the name for the class. */
    check_assertion(template_param->kind == (a_type_kind)tk_template_param);
    switch (template_param->variant.template_param.kind) {
      case tptk_param:
        mangled_encoding_for_template_parameter(
               &template_param->variant.template_param.extra_info->coordinates,
               (a_template_arg *)NULL,
               mctl);
        break;
      case tptk_member:
        /* For something like T::x, where T is a template parameter, just
           put out "x" here. */
        name = unmangled_name_of(&type->source_corresp);
        check_assertion_str(name != NULL,
                            "mangled_class_encoding: tptk_member has no name");
        mangled_name_with_length(name, mctl);
        break;
      default:
        unexpected_condition_str(
                            "mangled_class_encoding: bad template param kind");
    }  /* switch */
  } else {
    /* Not a proxy for a template parameter. */
    /* See whether this is the proxy for a template template parameter. */
    a_boolean    is_template_template_param = FALSE;
    a_symbol_ptr template_sym = class_template_for_type(type);
    if (template_sym != NULL) {
      a_template_symbol_supplement_ptr tssp =
                                           template_sym->variant.template_info;
      if (tssp->variant.class_template.template_template_param) {
        /* Yes, this is a template template parameter. */
        is_template_template_param = TRUE;
        mangled_encoding_for_template_parameter(
                                     &tssp->il_template_entry->coordinates,
                                     type->variant.class_struct_union.
                                                 extra_info->template_arg_list,
                                     mctl);
      }  /* if */
    }  /* if */
    if (!is_template_template_param) {
      /* Not a template template parameter. */
      /* Put out the class name preceded by its length. */
#if !IA64_ABI
      a_length_reservation length_reservation;
      reserve_space_for_length(&length_reservation, mctl);
#endif /* !IA64_ABI */
      mangled_full_class_name(type,
                              show_partial_spec_args,
                              show_template_specialization,
                              show_specialization,
                              /*show_length=*/TRUE,
                              mctl);
#if !IA64_ABI
      fill_in_length(&length_reservation, mctl);
#else /* IA64_ABI */
      add_discriminator_if_necessary(&type->source_corresp, mctl);
#endif /* !IA64_ABI */
    }  /* if */
  }  /* if */
}  /* mangled_class_encoding */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- nesting_level is unused in that case. */
#endif /* !IA64_ABI */
static void r_mangled_parent_qualifier(a_source_correspondence  *scp,
                                       unsigned long            nesting_level,
                                       a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the parent qualifier needed in
the mangled name for a member of a class or namespace whose source
correspondence is pointed to by scp.  nesting_level is used to track
recursive calls of this routine to deal with multiple levels of parents.
nesting_level == 1 refers to the innermost qualifier of a type,
nesting_level == 2 is the next level out, etc.  See the macro
mangled_parent_qualifier, which supplies the usual nesting_level == 1.
*/
{
  a_source_correspondence *parent_scp;
  a_boolean               more_levels;

  /* See if the present level is nested inside some other class or
     namespace. */
  if (scp->is_class_member) {
    a_type_ptr class_type = scp->parent.class_type;
#if CHECKING
    if (!class_type_has_body(class_type) &&
        !class_type->variant.class_struct_union.is_nonreal_class) {
#if DEBUG
      (void)fprintf(f_debug, "Parent class = ");
      db_abbr_type(class_type);
#endif /* DEBUG */
      unexpected_condition_str(
                       "r_mangled_parent_qualifier: parent class has no body");
    }  /* if */
#endif /* CHECKING */
    parent_scp = &scp->parent.class_type->source_corresp;
  } else {
    check_assertion(scp->parent.namespace_ptr != NULL);
    parent_scp = &scp->parent.namespace_ptr->source_corresp;
  }  /* if */
  more_levels = (parent_scp->is_class_member ||
                 parent_scp->parent.namespace_ptr != NULL);
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  /* If this a nested type name promoted into the file scope in
     cfront 2.1 mode, do not use the nested form. */
  if (scp->is_class_member &&
      scp->parent.class_type->
                           use_cfront_transitional_nested_type_name_mangling) {
    more_levels = FALSE;
  }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if !IA64_ABI
  if (more_levels) {
    /* This level is nested inside something else.  Do a recursive call to
       deal with all of the parents. */
    r_mangled_parent_qualifier(parent_scp, nesting_level + 1, mctl);
  } else {
    /* This is the topmost qualifier. */
    if (nesting_level > 1) {
      /* More than one level of nesting, so use the ARM (7.2.1c) encoding
         for nested class names, like "outer::inner", using a "Q" description:
           Q2_5outer5inner
              ^-----^-----mangled class names, outer to inner
            ^----count of levels of qualification
         Note that the ARM description does not include the underscore, which
         is necessary if you allow more than 9 levels of nesting.
         The same scheme is used for namespace names. */
      add_to_mangled_name('Q', mctl);
      add_number_to_mangled_name(nesting_level, mctl);
      add_to_mangled_name('_', mctl);
    }  /* if */
  }  /* if */
#endif /* !IA64_ABI */
  /* Put the class or namespace name at this level into the mangled name. */
  /* The name is preceded by a count of the number of characters in
     the name. */
  if (scp->is_class_member) {
    /* Class name. */
    a_type_ptr type = scp->parent.class_type;
    a_boolean  show_partial_spec_args; 
    a_boolean  is_specialization;
    a_boolean  is_template_specialization;
#if IA64_ABI
    if (add_substitution_if_available((char *)type,
                                      (an_il_entry_kind)iek_type,
                                      mctl)) {
      goto done;
    } else {
      a_template_ptr              tmpl;
      a_class_type_supplement_ptr ctsp;
      tmpl = class_template_of(type);
      if (tmpl != NULL &&
          add_substitution_if_available(tmpl, 
                                        (an_il_entry_kind)iek_template, 
                                        mctl)) {
        ctsp = type->variant.class_struct_union.extra_info;
        mangled_template_arguments(ctsp->template_arg_list,
                                   /*partial_spec=*/FALSE,
                                   /*old_form=*/FALSE,
                                   mctl);
        goto new_substitution;
      }  /* if */
      if (more_levels) {
        /* This level is nested inside something else.  Do a recursive call to
           deal with all of the parents. */
        r_mangled_parent_qualifier(parent_scp, nesting_level + 1, mctl);
      }  /* if */
    }  /* if */
#endif /* IA64_ABI */
    show_partial_spec_args = FALSE;
    is_specialization = FALSE;
    is_template_specialization = FALSE;
#if !IA64_ABI
    if (distinct_template_signatures) {
      /* When templates get distinct mangling from normal functions,
         information is included for specialization in parent classes. */
      /* See if the class comes from a template and that template is
         specialized. */
      a_symbol_ptr template_sym =
                             symbol_supplement_for_class(type)->class_template;
      if (template_sym != NULL) {
        /* This class is an instance of a template. */
        if (template_sym->variant.template_info->is_specific_definition) {
          /* The template is specialized. */
          is_template_specialization = TRUE;
        }  /* if */
      }  /* if */
      /* See if the class itself is specialized (but not with the old
         syntax). */
      if (type->variant.class_struct_union.is_specialized &&
          !type->variant.class_struct_union.specialized_with_old_syntax) {
        is_specialization = TRUE;
      }  /* if */
      show_partial_spec_args = distinct_template_signatures;
    }  /* if */
#endif /* !IA64_ABI */
    mangled_class_encoding(type,
                           show_partial_spec_args,
                           is_template_specialization,
                           is_specialization,
                           mctl);
#if IA64_ABI
  new_substitution:
    /* Add a substitution for this type. */
    alloc_substitution((char *)type, (an_il_entry_kind)iek_type, mctl);
#endif /* IA64_ABI */
  } else {
    /* Namespace name. */
    a_namespace_ptr nsp = scp->parent.namespace_ptr;
    char            *name;
#if IA64_ABI
    if (add_substitution((char *)nsp,
                         (an_il_entry_kind)iek_namespace, mctl)) {
      goto done;
    } else if (more_levels) {
      /* This level is nested inside something else.  Do a recursive call to
         deal with all of the parents. */
      r_mangled_parent_qualifier(parent_scp, nesting_level + 1, mctl);
    }  /* if */
#endif /* IA64_ABI */
    name = unmangled_name_of(&nsp->source_corresp);
    if (name == NULL) {
      /* For an unnamed namespace, generate a name (or use the name previously
         generated). */
      give_unnamed_namespace_a_name(nsp);
      name = nsp->source_corresp.name;
    }  /* if */
    /* Put out the namespace name preceded by the length of the name, e.g.,
       "NNN" --> "3NNN". */
    mangled_name_with_length(name, mctl);
  }  /* if */
#if IA64_ABI
done:;
#endif /* IA64_ABI */
}  /* r_mangled_parent_qualifier */


/* Return TRUE if the indicated type needs a parent (class or namespace)
   qualifier. */
#if !IA64_ABI
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define type_needs_parent_qualifier(type)                             \
  ((type)->source_corresp.is_class_member ||                          \
   (type)->source_corresp.parent.namespace_ptr != NULL)
#else /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#define type_needs_parent_qualifier(type)                             \
  (((type)->source_corresp.is_class_member ||                         \
    (type)->source_corresp.parent.namespace_ptr != NULL) &&           \
   !type->use_cfront_transitional_nested_type_name_mangling)
#endif /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#else /* IA64_ABI */
#define type_needs_parent_qualifier(type)                             \
  (((type)->source_corresp.is_class_member ||                         \
    (type)->source_corresp.parent.namespace_ptr != NULL) &&           \
   !is_in_namespace_std(type))
#endif /* IA64_ABI */


static void mangled_type_name(a_type_ptr               type,
                              a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the type "type".
This routine is used for named types (classes, enums, and typedefs;
typedefs come up when doing final name mangling for nested types)
and for unnamed classes and enums.  Nested types are encoded as such.
*/
{
  char                        *name;
#if IA64_ABI
  a_template_ptr              tmpl;
  a_class_type_supplement_ptr ctsp;
#endif /* IA64_ABI */

  /* cv-qualifiers are not allowed here. */
  check_assertion(type->kind != (a_type_kind)tk_typeref ||
                  typeref_is_typedef(type));
#if IA64_ABI
  /* The caller has already checked to see if a substitution is available for
     this entire type.  Check here to see if the type is an instantiation of a
     template for which a substitution is available. */
  /* Don't do this for typedefs passed from final_type_name_mangling. */
  if (is_immediate_class_type(type)) {
    tmpl = class_template_of(type);
    if (tmpl != NULL && 
        add_substitution_if_available(tmpl, 
                                      (an_il_entry_kind)iek_template, 
                                      mctl)) {
      ctsp = type->variant.class_struct_union.extra_info;
      mangled_template_arguments(ctsp->template_arg_list,
                                 /*partial_spec=*/FALSE,
                                 /*old_form=*/FALSE,
                                 mctl);
      goto done;
    }  /* if */
  }  /* if */
  add_prefix_for_local_class_if_necessary(type, mctl);
#endif /* IA64_ABI */
  if (type_needs_parent_qualifier(type)) {
#if IA64_ABI
    /* Mark the start of the nested name. */
    add_to_mangled_name('N', mctl);
#endif /* IA64_ABI */
    /* The type is a member of a class or namespace, so put out a qualifier.
       Note that the count starts at 2 because the type name itself is level
       1. */
    r_mangled_parent_qualifier(&type->source_corresp,
                               (unsigned long)2,
                               mctl);
#if IA64_ABI
  } else if (is_in_namespace_std(type)) {
    add_str_to_mangled_name("St", mctl);
#endif /* IA64_ABI */
  }  /* if */
  /* Put out the type name itself. */
  /* The mangled form of a type name is the type name with a length
       preceding it:
         AB          --> 2AB
         ABCDEFGHIJK --> 11ABCDEFGHIJK
  */
  if (is_immediate_class_type(type)) {
    /* Class name. */
    mangled_class_encoding(type,
                           /*show_partial_spec_args=*/FALSE,
                           /*show_template_specialization*/FALSE,
                           /*show_specialization=*/FALSE,
                           mctl);
  } else {
    /* Not a class name (typedef or enum). */
    name = unmangled_name_of(&type->source_corresp);
    if (name == NULL) {
      /* For an unnamed enum, generate a name (or use the name previously
         generated). */
      check_assertion(is_enum_type(type));
      give_unnamed_enum_a_name(type);
      name = type->source_corresp.name;
    }  /* if */
    mangled_name_with_length(name, mctl);
#if IA64_ABI
    add_discriminator_if_necessary(&type->source_corresp, mctl);
#endif /* IA64_ABI */
  }  /* if */
#if IA64_ABI
  if (type_needs_parent_qualifier(type)) {
    /* Mark the end of the nested name. */
    add_to_mangled_name('E', mctl);
  }  /* if */
done:;
#endif /* IA64_ABI */
}  /* mangled_type_name */


static void mangled_class_name_internal(a_type_ptr               type,
                                        a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is the encoding used for the name of the class as opposed to
the encoding for the class as a type (for example, it has no length
preceding a simple class name).  This routine has the name "_internal"
because it's intended to be called from inside a name mangling
operation; compare mangled_class_name (no "_internal").
*/
{
#if !IA64_ABI
  if (type_needs_parent_qualifier(type)) {
#endif /* !IA64_ABI */
    /* For a nested class, use the nested type encoding for the class. */
    mangled_type_name(type, mctl);
#if !IA64_ABI
  } else {
    /* For a non-nested class, use the simple form of the name (with
       no preceding length). */
    mangled_basic_class_name(type, mctl);
  }  /* if */
#endif /* !IA64_ABI */
}  /* mangled_class_name_internal */


static void mangled_encoding_for_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type "type".
*/
{
  a_type_ptr named_type, pm_base_type;
#if ABI_COMPATIBILITY_VERSION < 230
  a_type_ptr named_typedef = NULL;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  char       *s;
  a_type_qualifier_set
             qualifiers;
#if IA64_ABI
  a_type_ptr qualified_type = type;
#endif /* IA64_ABI */

#if IA64_ABI
  /* If the type has appeared previously, use a substitution for it. */
  if (add_substitution_if_available(type, (an_il_entry_kind)iek_type, mctl)) {
    goto end_of_routine;
  }  /* if */
#endif /* IA64_ABI */
  /* Walk through any typerefs above the type.  Remember type qualifiers
     and skip down to the "real" underlying type. */
  qualifiers = 0;
  for (; type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    /* Remember type qualifiers encountered. */
    qualifiers |= type->variant.typeref.qualifiers;
#if ABI_COMPATIBILITY_VERSION < 230
    /* Remember the bottommost named typedef encountered. */
    if (has_name(type)) named_typedef = type;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
#if DO_IL_LOWERING
    if (type->variant.typeref.orig_type != NULL) {
      /* A type like a pointer-to-member, which has been lowered.
         Switch to the original type. */
      type = type->variant.typeref.orig_type;
      break;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* for */
  /* Put out type qualifiers, if any. */
  if (qualifiers != 0) {
    mangled_encoding_for_type_qualifiers(qualifiers, mctl);
  }  /* if */
#if IA64_ABI
  if (add_substitution_if_available(type, (an_il_entry_kind)iek_type, mctl)) {
    goto add_substitution_for_qualified_type;
  }  /* if */
#endif /* IA64_ABI */
  /* See if the type is a named class or enum. */
  named_type = NULL;
  if (has_name(type) &&
      (is_immediate_class_type(type) || is_immediate_enum_type(type))) {
    /* Named class or enum type. */
    named_type = type;
#if ABI_COMPATIBILITY_VERSION < 230
  } else if (named_typedef != NULL && is_immediate_enum_type(type)) {
    /* Unnamed enum with a typedef above it.  Use the typedef name for the
       enum even though it's the name of a qualified version of the enum.
       In ABI versions >= 2.30, the processing for this was moved
       to decls.c for greater compatibility with cfront when
       CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE, and eliminated otherwise. */
    named_type = named_typedef;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  }  /* if */
  /* If the type is named, use the name. */
  if (named_type != NULL) {
    /* Put out the mangled form of the name, e.g., "2AB" for "AB". */
    mangled_type_name(named_type, mctl);
  } else {
    /* The type is not named, so develop a description string. */
    switch (type->kind) {
      case tk_error:
      case tk_unknown:
        /* This might come up in mangling names for template instantiation
           after errors have been detected. */
        check_assertion(total_errors != 0);
        s = "?";
        break;
      case tk_void:
        s = MANGLING_STRING_FOR_VOID;
        break;
      case tk_integer:
        if (type->variant.integer.enum_type) {
          /* Unnamed enum.  mangled_type_name will make up a name. */
          mangled_type_name(type, mctl);
          goto have_whole_mangled_name;
        }  /* if */
        if (type->variant.integer.wchar_t_type) {
          s = MANGLING_STRING_FOR_WCHAR_T;
        } else if (type->variant.integer.bool_type) {
          s = MANGLING_STRING_FOR_BOOL;
#if MICROSOFT_EXTENSIONS_ALLOWED && !IA64_ABI
        } else if (type->variant.integer.microsoft_sized_int_type) {
          /* Mangling of __intN types in certain Microsoft modes (Visual C++
             6.0 treated these as new intrinsic types; 7.0 went back to
             treating them as the same as the corresponding integral type). */
          an_integer_kind  kind = type->variant.integer.int_kind;
          if (kind == targ_int8_int_kind) {
            s = "m1";
          } else if (kind == targ_unsigned_int8_int_kind) {
            s = "Um1";
          } else if (kind == targ_int16_int_kind) {
            s = "m2";
          } else if (kind == targ_unsigned_int16_int_kind) {
            s = "Um2";
          } else if (kind == targ_int32_int_kind) {
            s = "m4";
          } else if (kind == targ_unsigned_int32_int_kind) {
            s = "Um4";
          } else if (kind == targ_int64_int_kind) {
            s = "m8";
          } else if (kind == targ_unsigned_int64_int_kind) {
            s = "Um8";
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !IA64_ABI */
        } else {
          switch (type->variant.integer.int_kind) {
            case ik_char:           
              s = MANGLING_STRING_FOR_CHAR;
              break;
            case ik_signed_char:    
              s = MANGLING_STRING_FOR_SIGNED_CHAR;
              break;
            case ik_unsigned_char:  
              s = MANGLING_STRING_FOR_UNSIGNED_CHAR;
              break;
            case ik_short:          
              s = MANGLING_STRING_FOR_SHORT;
              break;
            case ik_unsigned_short: 
              s = MANGLING_STRING_FOR_UNSIGNED_SHORT;
              break;
            case ik_int:            
              s = MANGLING_STRING_FOR_INT;
              break;
            case ik_unsigned_int:   
              s = MANGLING_STRING_FOR_UNSIGNED_INT;
              break;
            case ik_long:           
              s = MANGLING_STRING_FOR_LONG;
              break;
            case ik_unsigned_long:  
              s = MANGLING_STRING_FOR_UNSIGNED_LONG;
              break;
#if LONG_LONG_ALLOWED
            case ik_long_long:      
              s = MANGLING_STRING_FOR_LONG_LONG;
              break;
            case ik_unsigned_long_long:
              s = MANGLING_STRING_FOR_UNSIGNED_LONG_LONG;
              break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
            default:
              internal_error("mangled_encoding_for_type: bad int kind");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        break;
      case tk_float:
        switch (type->variant.float_kind) {
          case fk_float:          
            s = MANGLING_STRING_FOR_FLOAT;
            break;
          case fk_double:         
            s = MANGLING_STRING_FOR_DOUBLE;
            break;
          case fk_long_double:    
            s = MANGLING_STRING_FOR_LONG_DOUBLE;
            break;
#if CHECKING
          default:
            internal_error("mangled_encoding_for_type: bad float kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case tk_pointer:
        if (type->variant.pointer.is_reference) {
          s = MANGLING_STRING_FOR_REFERENCE;
        } else {
          s = MANGLING_STRING_FOR_POINTER;
        }  /* if */
        /* More of this below -- the "P" or "R" is followed by the
           type pointed to/referenced. */
        break;
      case tk_ptr_to_member:
        /* Pointer to member.  int S::* is put out as M1Si (Cfront-style). */
        s = MANGLING_STRING_FOR_POINTER_TO_MEMBER;
        /* More of this below -- the "M" is followed by the class name and
           the type pointed to. */
        break;
      case tk_array:
        s = MANGLING_STRING_FOR_ARRAY;
        /* More of this below -- int[10] is put out as A10_i (Cfront-style). */
        break;
      case tk_routine:
#if IA64_ABI
        /* There's no way to distinguish static member function types from
           ordinary function types -- but fortunately that doesn't matter in
           the IA64 ABI so the following function call is safe. */
        mangled_encoding_for_function_qualifiers(type, mctl);
#endif /* IA64_ABI */
        /* Function.  Put out "F" and the argument types. */
        mangled_encoding_for_function_type(type,
                                           /*do_return_type=*/TRUE,
                                           /*do_markers=*/TRUE,
                                           mctl);
        goto have_whole_mangled_name;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Unnamed classes.  mangled_type_name will make up a name. */
        mangled_type_name(type, mctl);
        goto have_whole_mangled_name;
      case tk_template_param:
        /* This comes up when mangling the names for template entities using
           the modern mangling approach. */
        switch (type->variant.template_param.kind) {
          case tptk_param:
            mangled_encoding_for_template_parameter(
                         &type->variant.template_param.extra_info->coordinates,
                         (a_template_arg *)NULL,
                         mctl);
            break;
          case tptk_member:
            /* Type selected from a template parameter type, e.g., T::x. */
            mangled_type_name(type, mctl);
            break;
          default:
            unexpected_condition_str(
                      "mangled_encoding_for_type: bad tk_template_param kind");
        }  /* if */
        goto have_whole_mangled_name;
#if CHECKING
      default:
        internal_error("mangled_encoding_for_type: bad type kind");
#endif /* CHECKING */
    }  /* switch */
    /* s is now set to a type description string to be output. */
    add_str_to_mangled_name(s, mctl);
    /* Do any processing needed after the description letter. */
    switch (type->kind) {
      case tk_pointer:
        /* Put out the type pointed to. */
        mangled_encoding_for_type(type->variant.pointer.type, mctl);
        break;
      case tk_ptr_to_member:
        /* Put out the mangled name of the class for which this is a member
           pointer. */
        mangled_encoding_for_type(type->variant.ptr_to_member.
                                                       class_of_which_a_member,
                                  mctl);
        pm_base_type = type->variant.ptr_to_member.type;
#if !IA64_ABI
        if (is_function_type(pm_base_type)) {
          /* This is a pointer to member function.  Put out the type qualifiers
             (if any) on the member function type. */
          mangled_encoding_for_function_qualifiers(pm_base_type, mctl);
        }  /* if */
#endif /* !IA64_ABI */
        /* Put out the type pointed to. */
        mangled_encoding_for_type(pm_base_type, mctl);
        break;
      case tk_array:
        /* Put out the array size, an underscore, and then the element type,
           i.e., int[10] is put out as A10_i. */
        check_assertion(!type->variant.array.is_variable_size_array);
        if (type->variant.array.is_template_dependent_size_array) {
          /* Template-dependent size arrays are possible when putting out
             function prototypes. */
#if !IA64_ABI 
          /* For that case the prefix is "A_". */
#endif /* !IA64_ABI */
          check_assertion(distinct_template_signatures);
#if !IA64_ABI
          add_to_mangled_name('_', mctl);
#endif /* !IA64_ABI */
          /* Put out an encoding for the bound. */
          mangled_encoding_for_constant(
                            type->variant.array.variant.element_count_constant,
                            /*old_form=*/FALSE,
                            mctl);
#if IA64_ABI
        } else if (!type->variant.array.bound_is_zero && 
                   type->variant.array.variant.number_of_elements == 0) {
          /* If there is no bound, nothing is output.  */
#endif /* IA64_ABI */
        } else {
          /* Put out the (constant) number of elements. */
          add_number_to_mangled_name((unsigned long)type->variant.array.
                                                    variant.number_of_elements,
                                     mctl);
        }  /* if */
        add_to_mangled_name('_', mctl);
        /* Put out the element type. */
        mangled_encoding_for_type(type->variant.array.element_type, mctl);
        break;
      default:;
        /* Many cases don't require any handling. */
    }  /* switch */
  }  /* if */
have_whole_mangled_name:;
#if IA64_ABI
  /* Create a substitution for the unqualified type. */
  if (!is_integral_type(type) && !is_floating_type(type) && 
      !is_void_type(type)) {
    alloc_substitution((char *)type, (an_il_entry_kind)iek_type, mctl);
  }  /* if */
add_substitution_for_qualified_type:
  /* Create a substitution for the original type, if it was qualified. */
  if (qualifiers != TQ_NONE) {
    alloc_substitution((char *)qualified_type,
                       (an_il_entry_kind)iek_type, mctl);
  }  /* if */
end_of_routine:;
#endif /* IA64_ABI */
}  /* mangled_encoding_for_type */


#if !IA64_ABI
/*ARGSUSED*/ /* <-- num_operands is unused in that case. */
#endif /* !IA64_ABI */
static char *mangled_operator_name(an_opname_kind kind,
                                   unsigned int   num_operands)
/*
Return the string used to indicate the indicated operator name in mangled
names.  The string does not have the leading "__" used in some cases.  The
number of operands is given by num_operands; in some configurations unary and
binary versions of operators are mangled differently.
*/
{
  char *name;

  switch (kind) {
    case onk_new:               /* "new" */
      name = MANGLING_STRING_FOR_OPERATOR_NEW;
      break;
    case onk_delete:            /* "delete" */
      name = MANGLING_STRING_FOR_OPERATOR_DELETE;
      break;
    case onk_array_new:         /* "new[]" */
      name = MANGLING_STRING_FOR_OPERATOR_ARRAY_NEW;
      break;
    case onk_array_delete:      /* "delete[]" */
      name = MANGLING_STRING_FOR_OPERATOR_ARRAY_DELETE;
      break;
    case onk_plus:              /* "+" */
      name = MANGLING_STRING_FOR_OPERATOR_PLUS;
      break;
    case onk_minus:             /* "-" */
#ifdef MANGLING_STRING_FOR_OPERATOR_NEGATE
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_NEGATE;
      } else 
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_NEGATE */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_MINUS;
      }  /* if */
      break;
    case onk_star:              /* "*" */
#ifdef MANGLING_STRING_FOR_OPERATOR_DEREFERENCE
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_DEREFERENCE;
      } else 
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_DEREFERENCE */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_MULT;
      }  /* if */
      break;
    case onk_divide:            /* "/" */
      name = MANGLING_STRING_FOR_OPERATOR_DIVIDE;
      break;
    case onk_remainder:         /* "%" */
      name = MANGLING_STRING_FOR_OPERATOR_REMAINDER;
      break;
    case onk_excl_or:           /* "^" */
      name = MANGLING_STRING_FOR_OPERATOR_EXCL_OR;
      break;
    case onk_ampersand:         /* "&" */
#ifdef MANGLING_STRING_FOR_OPERATOR_ADDRESS
      if (num_operands == 1) {
        name = MANGLING_STRING_FOR_OPERATOR_ADDRESS;
      } else
#endif /* ifdef MANGLING_STRING_FOR_OPERATOR_ADDRESS */
      /* Do not add code here. */
      {
        name = MANGLING_STRING_FOR_OPERATOR_AND;
      }  /* if */
      break;
    case onk_or:                /* "|" */
      name = MANGLING_STRING_FOR_OPERATOR_OR;
      break;
    case onk_compl:             /* "~" */
      name = MANGLING_STRING_FOR_OPERATOR_COMPLEMENT;
      break;
    case onk_not:               /* "!" */
      name = MANGLING_STRING_FOR_OPERATOR_NOT;
      break;
    case onk_assign:            /* "=" */
      name = MANGLING_STRING_FOR_OPERATOR_ASSIGN;
      break;
    case onk_lt:                /* "<" */
      name = MANGLING_STRING_FOR_OPERATOR_LT;
      break;
    case onk_gt:                /* ">" */
      name = MANGLING_STRING_FOR_OPERATOR_GT;
      break;
    case onk_plus_assign:       /* "+=" */
      name = MANGLING_STRING_FOR_OPERATOR_PLUS_ASSIGN;
      break;
    case onk_minus_assign:      /* "-=" */
      name = MANGLING_STRING_FOR_OPERATOR_MINUS_ASSIGN;
      break;
    case onk_times_assign:      /* "*=" */
      name = MANGLING_STRING_FOR_OPERATOR_TIMES_ASSIGN;
      break;
    case onk_divide_assign:     /* "/=" */
      name = MANGLING_STRING_FOR_OPERATOR_DIVIDE_ASSIGN;
      break;
    case onk_remainder_assign:  /* "%=" */
      name = MANGLING_STRING_FOR_OPERATOR_REMAINDER_ASSIGN;
      break;
    case onk_excl_or_assign:    /* "^=" */
      name = MANGLING_STRING_FOR_OPERATOR_EXCL_OR_ASSIGN;
      break;
    case onk_and_assign:        /* "&=" */
      name = MANGLING_STRING_FOR_OPERATOR_AND_ASSIGN;
      break;
    case onk_or_assign:         /* "|=" */
      name = MANGLING_STRING_FOR_OPERATOR_OR_ASSIGN;
      break;
    case onk_shift_left:        /* "<<" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT;
      break;
    case onk_shift_right:       /* ">>" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT;
      break;
    case onk_shift_right_assign:/* ">>=" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_RIGHT_ASSIGN;
      break;
    case onk_shift_left_assign: /* "<<=" */
      name = MANGLING_STRING_FOR_OPERATOR_SHIFT_LEFT_ASSIGN;
      break;
    case onk_eq:                /* "==" */
      name = MANGLING_STRING_FOR_OPERATOR_EQ;
      break;
    case onk_ne:                /* "!=" */
      name = MANGLING_STRING_FOR_OPERATOR_NE;
      break;
    case onk_le:                /* "<=" */
      name = MANGLING_STRING_FOR_OPERATOR_LE;
      break;
    case onk_ge:                /* ">=" */
      name = MANGLING_STRING_FOR_OPERATOR_GE;
      break;
    case onk_and_and:           /* "&&" */
      name = MANGLING_STRING_FOR_OPERATOR_AND_AND;
      break;
    case onk_or_or:             /* "||" */
      name = MANGLING_STRING_FOR_OPERATOR_OR_OR;
      break;
    case onk_plus_plus:         /* "++" */
      name = MANGLING_STRING_FOR_OPERATOR_PLUS_PLUS;
      break;
    case onk_minus_minus:       /* "--" */
      name = MANGLING_STRING_FOR_OPERATOR_MINUS_MINUS;
      break;
    case onk_comma:             /* "," */
      name = MANGLING_STRING_FOR_OPERATOR_COMMA;
      break;
    case onk_arrow_star:        /* "->*" */
      name = MANGLING_STRING_FOR_OPERATOR_ARROW_STAR;
      break;
    case onk_arrow:             /* "->" */
      name = MANGLING_STRING_FOR_OPERATOR_ARROW;
      break;
    case onk_function_call:     /* "()" */
      name = MANGLING_STRING_FOR_OPERATOR_CALL;
      break;
    case onk_subscript:         /* "[]" */
      name = MANGLING_STRING_FOR_OPERATOR_SUBSCRIPT;
      break;
    case onk_question:          /* "?" */
      name = MANGLING_STRING_FOR_OPERATOR_QUESTION;
      break;
#if CHECKING
    default:
      internal_error("mangled_operator_name: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return name;
}  /* mangled_operator_name */


static char *mangled_expr_operator_name(an_expr_operator_kind op)
/*
Return the string used to mangle the indicated expression operator.
This routine only needs to handle the operators that can be used in
expressions on nontype template parameters in function signatures.
*/
{
  char           *name = NULL;
  an_opname_kind opkind;
  unsigned int   num_operands = 2;

  switch (op) {
    case eok_inegate:
    case eok_fnegate:
    case eok_negate:
      opkind = (an_opname_kind)onk_minus;
      num_operands = 1;
      break;
    case eok_unary_plus:
#if !IA64_ABI
      opkind = (an_opname_kind)onk_plus;
      num_operands = 1;
#else /* IA64_ABI */
      unexpected_condition();
#endif /* IA64_ABI */
      break;
    case eok_not:
      opkind = (an_opname_kind)onk_not;
      num_operands = 1;
      break;
    case eok_cast:
    case eok_base_class_cast:
    case eok_derived_class_cast:
    case eok_pm_base_class_cast:
    case eok_pm_derived_class_cast:
    case eok_lvalue_cast:
    case eok_bool_cast:
#if !IA64_ABI
      name = "cs";
#else /* IA64_ABI */
      name = "cv";
#endif /* IA64_ABI */
      num_operands = 1;
      break;
    case eok_complement:
      opkind = (an_opname_kind)onk_compl;
      num_operands = 1;
      break;
    case eok_iadd:
    case eok_fadd:
    case eok_add:
      opkind = (an_opname_kind)onk_plus;
      break;
    case eok_isubtract:
    case eok_fsubtract:
    case eok_subtract:
      opkind = (an_opname_kind)onk_minus;
      break;
    case eok_imultiply:
    case eok_fmultiply:
    case eok_multiply:
      opkind = (an_opname_kind)onk_star;
      break;
    case eok_idivide:
    case eok_fdivide:
    case eok_divide:
      opkind = (an_opname_kind)onk_divide;
      break;
    case eok_ieq:
    case eok_feq:
    case eok_eq:
      opkind = (an_opname_kind)onk_eq;
      break;
    case eok_ine:
    case eok_fne:
    case eok_ne:
      opkind = (an_opname_kind)onk_ne;
      break;
    case eok_igt:
    case eok_fgt:
    case eok_gt:
      opkind = (an_opname_kind)onk_gt;
      break;
    case eok_ilt:
    case eok_flt:
    case eok_lt:
      opkind = (an_opname_kind)onk_lt;
      break;
    case eok_ige:
    case eok_fge:
    case eok_ge:
      opkind = (an_opname_kind)onk_ge;
      break;
    case eok_ile:
    case eok_fle:
    case eok_le:
      opkind = (an_opname_kind)onk_le;
      break;
    case eok_remainder:
      opkind = (an_opname_kind)onk_remainder;
      break;
    case eok_shiftl:
      opkind = (an_opname_kind)onk_shift_left;
      break;
    case eok_shiftr:
      opkind = (an_opname_kind)onk_shift_right;
      break;
    case eok_and:
      opkind = (an_opname_kind)onk_ampersand;
      break;
    case eok_or:
      opkind = (an_opname_kind)onk_or;
      break;
    case eok_xor:
      opkind = (an_opname_kind)onk_excl_or;
      break;
    case eok_comma:
      opkind = (an_opname_kind)onk_comma;
      break;
    case eok_land:
      opkind = (an_opname_kind)onk_and_and;
      break;
    case eok_lor:
      opkind = (an_opname_kind)onk_or_or;
      break;
    case eok_question:
#if GNU_EXTENSIONS_ALLOWED
    case eok_binary_question:
#endif /* GNU_EXTENSIONS_ALLOWED */
      opkind = (an_opname_kind)onk_question;
      num_operands = 3;
      break;
    case eok_lvalue:                     /* Handled higher up */
    case eok_rvalue:                     /* Handled higher up */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_assume:                     /* Handled higher up */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("mangled_expr_operator_name: bad operator");
  }  /* switch */
  if (name == NULL) {
    /* Convert opkind to a name. */
    name = mangled_operator_name(opkind, num_operands);
  }  /* if */
  return name;
}  /* mangled_expr_operator_name */


static void mangled_function_base_name(
                                      a_source_correspondence  *scp,
                                      a_special_function_kind  special_kind,
                                      an_opname_kind           opname_kind,
                                      unsigned int             num_operands,
                                      a_type_ptr               conversion_type,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the base name of the function
indicated by scp.  special_kind, opname_kind, num_operands, and
conversion_type give additional information for special functions like
constructors and conversion functions.
*/
{
  char      *name;
#if !IA64_ABI
  a_boolean add_leading_underscores = FALSE;
#endif /* !IA64_ABI */

  if (special_kind == (a_special_function_kind)sfk_none) {
    /* Normal name. */
    name = unmangled_name_of(scp);
    check_assertion_str(name != NULL,
                        "mangled_function_base_name: unnamed routine");
#if IA64_ABI
    add_number_to_mangled_name((unsigned long)strlen(name), mctl);
#endif /* IA64_ABI */
  } else {
    /* Use a special name for the routine. */
#if !IA64_ABI
    add_leading_underscores = TRUE;
#endif /* !IA64_ABI */
    switch (special_kind) {
      case sfk_constructor:
        name = MANGLING_STRING_FOR_CONSTRUCTOR;
        break;
      case sfk_destructor:
        name = MANGLING_STRING_FOR_DESTRUCTOR;
        break;
      case sfk_conversion:
        name = MANGLING_STRING_FOR_CONVERSION_FUNC;
        /* Type signature is put out below. */
        break;
      case sfk_operator:
        name = mangled_operator_name(opname_kind, num_operands);
        break;
      default:
        unexpected_condition_str(
                               "mangled_function_base_name: bad special kind");
    }  /* switch */
  }  /* if */
#if !IA64_ABI
  if (add_leading_underscores) {
    add_str_to_mangled_name("__", mctl);
  }  /* if */
#endif /* !IA64_ABI */
  /* Copy the name. */
  add_str_to_mangled_name(name, mctl);
  /* For a conversion function, add the type signature. */
  if (special_kind == (a_special_function_kind)sfk_conversion) {
    check_assertion(conversion_type != NULL);
    mangled_encoding_for_type(conversion_type, mctl);
  }  /* if */
}  /* mangled_function_base_name */


#if !IA64_ABI || !DO_IL_LOWERING
/*ARGSUSED*/ /* <-- base_name_offset is not used in that case. */
#endif /* !IA64_ABI || !DO_IL_LOWERING */
static void mangled_function_name(
                              a_routine_ptr            routine,
                              a_boolean                suppress_param_encoding,
                              sizeof_t                 *base_name_offset,
                              a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the function "routine".
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.  If base_name_offset is not NULL,
*base_name_offset is set to the offset from the start of the mangling to
the point where the base name appears.
*/
{
  a_type_ptr       conversion_type, routine_type;
  a_boolean        is_member, mangle_as_template;
#if !IA64_ABI
  a_boolean        is_specialization = FALSE;
  a_boolean        is_template_specialization = FALSE;
#endif /* !IA64_ABI */
  unsigned int     num_operands;
  a_param_type_ptr ptp;
  an_opname_kind   opname_kind = (an_opname_kind)onk_none;

  /* Most of the processing is done in mangled_encoding_for_function_type,
     but this routine handles:
       (1)  The output of the name of the function, followed by "__".
            For special member functions, a special name is used, e.g.,
            "__ct" for constructors.
       (2)  If the function is a member function, the name of the
            class pointed to, followed by
              (a) if the function is nonstatic, "C", "V", or "CV" if there
                  are type qualifiers on the "this" parameter type, or
              (b) if the function is static, "S".
     mangled_encoding_for_function_type is then called to do the rest of the
     processing.
  */
  routine_type = skip_typerefs(routine->type);
  /* See if the function is a class member function or a member of a
     namespace. */
  is_member = (routine->source_corresp.is_class_member ||
               routine->source_corresp.parent.namespace_ptr != NULL);
#if IA64_ABI
  if (is_in_namespace_std(routine)) {
    is_member = FALSE;
    add_str_to_mangled_name("St", mctl);
  } else if (is_member) {
    /* Mark the start of the nested name. */
    if (routine->source_corresp.is_class_member) {
      add_prefix_for_local_class_if_necessary(
                                     routine->source_corresp.parent.class_type,
                                     mctl);
    }  /* if */
    add_to_mangled_name('N', mctl);
    if (routine->source_corresp.is_class_member) {
      /* Class member function.  Put out the qualifiers on the member function
         type. */
      mangled_encoding_for_function_qualifiers(routine_type, mctl);
    }  /* if */
    /* Put out the name of the class or namespace of which this function
       is a member. */
    mangled_parent_qualifier(&routine->source_corresp, mctl);
  }  /* if */
#endif /* IA64_ABI */
  /* See if the function should be mangled as a template.  In the modern C++
     language, template functions are mangled using the template arguments
     and the prototype for the function.  This allows overloading of function
     templates (the instances have the same function parameter types, but
     one can be chosen over the other based on whether it is more
     specialized). */
  mangle_as_template = (distinct_template_signatures &&
#if IA64_ABI
                        /* Member functions of template classes are not
                           considered templates for mangling purposes. */
                        routine->template_arg_list != NULL &&
#endif /* IA64_ABI */
                        routine->is_template_function);
  if (mangle_as_template) {
    /* See if the function comes from a template and that template is
       specialized. */
    a_symbol_ptr sym = (a_symbol_ptr)(routine->source_corresp.assoc_info);
    if (sym->variant.routine.instance_ptr != NULL) {
      /* This function is an instance of a template. */
      a_symbol_ptr template_sym =
                               sym->variant.routine.instance_ptr->template_sym;
      a_template_symbol_supplement_ptr tssp =
                                  template_supplement_for_symbol(template_sym);
#if !IA64_ABI
      if (tssp->is_specific_definition) {
        /* The template is specialized. */
        is_template_specialization = TRUE;
      }  /* if */
#endif /* !IA64_ABI */
      /* Use the type of the prototype routine from the template as the
         routine type for the rest of the mangling. */
      /* Note that "routine" is not updated. */
      routine_type = tssp->variant.function.routine->type;
      routine_type = skip_typerefs(routine_type);
#if IA64_ABI
      if (add_substitution((char *)tssp->il_template_entry,
                           (an_il_entry_kind)iek_template, mctl)) {
        goto mangle_template;
      }  /* if */
#endif /* IA64_ABI */
    }  /* if */
#if !IA64_ABI
    /* See if the function itself is specialized (but not with the old
       syntax). */
    if (routine->is_specialized && !routine->specialized_with_old_syntax) {
      is_specialization = TRUE;
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
  /* Put out the base name of the function. */
  conversion_type = NULL;
  if (routine->special_kind == (a_special_function_kind)sfk_conversion) {
    conversion_type = routine_type->variant.routine.return_type;
  }  /* if */
   /* Count the routine's parameters. */
  num_operands = 0;
  for (ptp = routine_type->variant.routine.extra_info->param_type_list;
       ptp != NULL;
       ptp = ptp->next) {
    ++num_operands;
  }  /* for */
  /* If this is a member function, the object pointed to by this is an
     implicit operand.  This counts wrong for static member functions,
     but it's used only for operator functions and those can't be
     static. */
  if (routine->source_corresp.is_class_member) {
    ++num_operands;
  }  /* if */
  if (routine->special_kind == (a_special_function_kind)sfk_operator) {
    opname_kind = routine->variant.opname_kind;
  }  /* if */
#if IA64_ABI && DO_IL_LOWERING
  if (base_name_offset != NULL) {
    *base_name_offset = mctl->length;
  }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
  mangled_function_base_name(&routine->source_corresp, routine->special_kind,
                             opname_kind, num_operands, conversion_type, mctl);
  if (mangle_as_template) {
#if IA64_ABI
  mangle_template:
#else /* !IA64_ABI */
    if (is_template_specialization) {
      /* Put out an indication of the fact the template from which this
         function is generated is specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
#endif /* !IA64_ABI */
    if (routine->template_arg_list != NULL) {
      /* Put out the template arguments. */
      mangled_template_arguments(routine->template_arg_list,
                                 /*partial_spec=*/FALSE,
                                 /*old_form=*/FALSE,
                                 mctl);
    }  /* if */
#if !IA64_ABI
    if (is_specialization) {
      /* Put out an indication of the fact that this function is
         specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
#endif /* !IA64_ABI */
  }  /* if */
#if !IA64_ABI
  /* If we will be adding the class or namespace name or the parameter types,
     put out two underscores to separate the function name from the rest. */
  if (is_member || !suppress_param_encoding) {
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", mctl);
  }  /* if */
  if (is_member) {
    /* Put out the name of the class or namespace of which this function
       is a member. */
    mangled_parent_qualifier(&routine->source_corresp, mctl);
  }  /* if */
#endif /* !IA64_ABI */
#if IA64_ABI
  if (is_member) {
    /* Mark the end of the nested name. */
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* !IA64_ABI */
  if (!suppress_param_encoding) {
    a_boolean do_return_type;
#if !IA64_ABI
    if (routine->source_corresp.is_class_member) {
      /* Class member function.  Put out the qualifiers on the member function
         type. */
      mangled_encoding_for_function_qualifiers(routine_type, mctl);
    }  /* if */
#endif /* !IA64_ABI */
    /* Templates have their return types included. */
    do_return_type = mangle_as_template;
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
#if ABI_COMPATIBILITY_VERSION >= 243
        routine->special_kind == (a_special_function_kind)sfk_conversion ||
#endif /* ABI_COMPATIBILITY_VERSION >= 243 */
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      /* No return type on constructors, destructors, or conversion
         functions. */
      do_return_type = FALSE;
    }  /* if */
    /* Output the function type, including the parameter types. */
    mangled_encoding_for_function_type(routine_type, do_return_type,
#if !IA64_ABI
                                       /*do_markers=*/TRUE,
#else /* IA64_ABI */
                                       /*do_markers=*/FALSE, 
#endif /* IA64_ABI */
                                       mctl);
  }  /* if */
}  /* mangled_function_name */


static a_boolean function_name_mangling_needed(
                                        a_routine_ptr routine,
                                        a_boolean     *suppress_param_encoding)
/*
Return TRUE if the name of the indicated routine needs to be mangled.
If so, also return *suppress_param_encoding TRUE if the name should be
mangled without parameter encoding.
*/
{
  a_boolean mangling_needed = FALSE;

  *suppress_param_encoding = FALSE;
  /* All names except C external names must be mangled, because they might
     be overloaded.  All member function names must be mangled because
     they exist in a scope that does not exist in the C version of the
     program (of course, none of them have C external linkage, so no
     separate test is needed). */
  if (!has_name(routine)) {
    /* Unnamed routines generally do not need mangled names. */
    /* Compiler-generated routines have no name, and they are left alone.
       But constructors for unnamed classes that got a name for linkage
       purposes should get mangled names. */
    if (routine->special_kind == (a_special_function_kind)sfk_constructor &&
        has_name(routine->source_corresp.parent.class_type)) {
      mangling_needed = TRUE;
    }  /* if */
  } else if (routine == il_header.main_routine) {
    /* Don't mangle "main" regardless of its linkage. */
  } else if (is_name_linkage_kind_subject_to_name_mangling(
                                       routine->source_corresp.name_linkage)) {
    /* Routines other than extern "C" routines need to be mangled. */
    mangling_needed = TRUE;
  } else if (routine->special_kind != (a_special_function_kind)sfk_none) {
    /* Operator function names must be somewhat mangled even if they are
       not C++ external, because their names are not normal C names --
       they contain special characters, etc. */
    mangling_needed = TRUE;
    *suppress_param_encoding = TRUE;
  }  /* if */
  return mangling_needed;
}  /* function_name_mangling_needed */

#if DO_IL_LOWERING

static void start_externalized_name(a_boolean                is_variable,
                                    a_mangling_control_block *mctl)
/*
Begin the output of the externalized mangled name for the entity with
the indicated source correspondence.  The entity is a variable if
is_variable is TRUE, a routine otherwise.
*/
{
  char *prefix = (is_variable ? (char *)"__STV__" : (char *)"__STF__");

  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
     Only the prefix is put out here.
  */
  add_str_to_mangled_name(prefix, mctl);
}  /* start_externalized_name */

#endif /* DO_IL_LOWERING */
#if DO_IL_LOWERING

static void end_externalized_name(a_source_correspondence  *scp,
                                  a_mangling_control_block *mctl)
/*
End the output of the externalized mangled name for the entity with
the indicated source correspondence.
*/
{
  a_translation_unit_ptr tup;
  char                   *module_id;

  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
     Only the part after "name" is put out here.
  */
  /* Get the module id for the translation unit which this source
     correspondence is part of.  For a source correspondence with no
     associated symbol, use the current translation unit. */
  tup = (scp->assoc_info != NULL) ? trans_unit_for_source_corresp(scp) :
                                    curr_translation_unit;
  module_id = *tup->module_id_ptr;
  /* The module id must have been created previously. */
  check_assertion(module_id != NULL);
  add_str_to_mangled_name("__", mctl);
  add_str_to_mangled_name(module_id, mctl);
}  /* end_externalized_name */


char *externalized_mangled_name(a_source_correspondence  *scp,
                                a_boolean                is_variable)
/*
Generate and return the externalized name for the entity with the
indicated source correspondence.  An externalized name is a name given
to a static entity when it is made external so that its name will remain
unique across the program.  The entity is a variable if is_variable
is TRUE, a routine otherwise.  The name returned is in a temporary
buffer, and must be copied elsewhere promptly.
*/
{
  a_mangling_control_block mctl;
  char                     *name = scp->name;
  char                     buffer[50];
  a_source_correspondence  *module_scp = scp;

#if CHECKING
  /* If the name needs to be mangled, the mangling should have been done
     already. */
  { a_boolean dummy;
    if (scp->name_has_been_mangled) {
      /* Okay, mangling already done. */
      /* Compression and truncation shouldn't have been done already,
         however. */
      check_assertion_str(!scp->mangled_name_cannot_be_included_in_other_name,
                      "externalized_mangled_name: mangled name already final");
    } else if (is_variable ?
                           variable_name_mangling_needed((a_variable_ptr)scp) :
                           function_name_mangling_needed((a_routine_ptr)scp,
                                                         &dummy)) {
#if DEBUG
      db_entity_info((char *)scp, is_variable ? iek_variable : iek_routine);
#endif /* DEBUG */
      internal_error("externalized_mangled_name: name not mangled");
    }  /* if */
  }
#endif /* CHECKING */
  start_mangling(&mctl);
  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
  */
  start_externalized_name(is_variable, &mctl);
  if (name == NULL) {
    /* Entity has no name, e.g., a generated routine.  Generate one. */
    if (is_variable) {
      a_variable_ptr var = (a_variable_ptr)scp;
      if (var->is_anonymous_parent_object) {
        /* Give a name to an anonymous union variable based on its first
           member's name.  This is necessary so that the name will come out
           the same whether compiled in a primary translation unit or a
           secondary one. */
        a_type_ptr  union_type = var->type;
        a_field_ptr field;
        check_assertion(union_type->kind == (a_type_kind)tk_union);
        for (;;) {
          field = union_type->variant.class_struct_union.field_list;
          if (field == NULL) break;
          /* Use the name of the first member. */
          name = field->source_corresp.name;
          if (name != NULL) {
            module_scp = &field->source_corresp;
            break;
          }  /* if */
          /* Loop if the first member is itself an anonymous union. */
          if (!field->is_anonymous_parent_object) break;
          union_type = field->type;
        }  /* for */
      }  /* if */
    }  /* if */
    if (name == NULL) {
      /* Generate a name. */
      (void)sprintf(buffer, "%lu", unique_id_for_il_pointer(scp));
      name = buffer;
    }  /* if */
  }  /* if */
  add_str_to_mangled_name(name, &mctl);
  end_externalized_name(module_scp, &mctl);
  add_to_mangled_name('\0', &mctl);
  return mangling_text_buffer->buffer;
}  /* externalized_mangled_name */

#endif /* DO_IL_LOWERING */

static void mangled_function_name_externalized_if_necessary(
                              a_routine_ptr            routine,
                              a_boolean                suppress_param_encoding,
                              sizeof_t                 *base_name_offset,
                              a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the function "routine".
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.  If base_name_offset is not NULL,
*base_name_offset is set to the offset from the start of the mangling to
the point where the base name appears.  If the routine will be
externalized, use the encoding for the externalized form.
*/
{
#if DO_IL_LOWERING
  a_boolean needs_to_be_externalized;

  /* Static entities are potentially referenced from exported templates
     and therefore get externalized, which gives them a different kind
     of mangled name. */
  /* Note that if the name has been externalized already it fails the
     "should be externalized" test, but we still need to generate an
     externalized name here (and the lower-level routine will fetch the
     non-externalized name). */
  needs_to_be_externalized =
                routine->source_corresp.externalized ||
                routine_should_be_externalized_for_exported_templates(routine);
  if (needs_to_be_externalized) {
    start_externalized_name(/*is_variable=*/FALSE, mctl);
  }  /* if */
#endif /* DO_IL_LOWERING */
  mangled_function_name(routine, suppress_param_encoding,
                        base_name_offset, mctl);
#if DO_IL_LOWERING
  if (needs_to_be_externalized) {
    end_externalized_name(&routine->source_corresp, mctl);
  }  /* if */
#endif /* DO_IL_LOWERING */
}  /* mangled_function_name_externalized_if_necessary */


#if TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED || MODULE_ID_NEEDED

char *get_mangled_function_name(a_routine_ptr routine)
/*
Get the mangled name for the indicated routine, and return a pointer
to it.  If the routine name has not been mangled yet, create a copy
of the mangled name in a temporary buffer but do not change the
name in the routine entry.
*/
{
  a_mangling_control_block mctl;
  a_boolean                suppress_param_encoding;
  char                     *mangled_name;
  sizeof_t                 *base_name_offset = NULL;
  a_boolean                needs_to_be_externalized = FALSE;

#if DO_IL_LOWERING
  /* Static entities are potentially referenced from exported templates
     and therefore get externalized, which gives them a different kind
     of mangled name. */
  needs_to_be_externalized =
                routine_should_be_externalized_for_exported_templates(routine);
#endif /* DO_IL_LOWERING */
  if ((routine->source_corresp.name_has_been_mangled &&
       !routine->source_corresp.final_name_mangling_pending &&
       (!needs_to_be_externalized || routine->source_corresp.externalized)) ||
      !function_name_mangling_needed(routine, &suppress_param_encoding)) {
    /* The name has already been (completely) mangled, or it doesn't need
       to be mangled, so just return it. */
    mangled_name = routine->source_corresp.name;
    /* The routine should not be unnamed. */
    check_assertion(mangled_name != NULL);
  } else {
    /* Generate the mangled name in a buffer. */
    start_mangling(&mctl);
    add_mangled_name_prefix(&mctl);
    /* Create the name. */
#if IA64_ABI
    /* It's OK to set the base_name_offset here; it will be set to the same
       value every time.  By setting the value here, we make it available to
       callers of get_mangled_function_name, even if mangle_function_name has
       not yet been called. */
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      base_name_offset = &routine->variant.ctor_dtor.base_name_offset;
    }  /* if */
#endif /* IA64_ABI */
    mangled_function_name_externalized_if_necessary(routine,
                                                    suppress_param_encoding,
                                                    base_name_offset,
                                                    &mctl);
    mangled_name = end_mangling((a_source_correspondence *)NULL,
                                /*final=*/TRUE, &mctl);
  }  /* if */
#if IA64_ABI
  if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
      routine->special_kind == (a_special_function_kind)sfk_destructor) {
    /* Change the mangled name of a constructor or destructor to the
       complete-object version instead of the internal name (e.g.,
       "C1" in the mangled name instead of "C9"). */
    if (mangled_name == routine->source_corresp.name) {
      /* Copy the name to the mangling buffer so we can change it. */
      reset_text_buffer(mangling_text_buffer);
      add_to_text_buffer(mangling_text_buffer, mangled_name,
                         strlen(mangled_name)+1);
      mangled_name = mangling_text_buffer->buffer;
    }  /* if */
    mangled_name[routine->variant.ctor_dtor.base_name_offset+1] = '1';
  }  /* if */
#endif /* IA64_ABI */
  return mangled_name;
}  /* get_mangled_function_name */

#endif /* TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED ||
          MODULE_ID_NEEDED */

#if IA64_ABI
/*ARGSUSED*/ /* <-- is_specialization is not used in that case. */
#endif /* IA64_ABI */
static void mangled_member_name(a_source_correspondence  *scp,
                                a_boolean                is_specialization,
                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class or
namespace member whose source correspondence is given by scp.
This routine must be called only for static data member variables,
namespace member variables, and class and namespace member constants.
is_specialization is TRUE if the variable is a template static data
member specialization.
*/
{
#if !IA64_ABI
  char *name;

  /* The mangled name of a static data member or member constant is the
     original name followed by two underscores followed by the mangled
     class name.  For example:
       AB::xy --> xy__2AB
     The same encoding is used for members of namespaces.
  */
  name = unmangled_name_of(scp);
  if (name == NULL) {
    /* For an unnamed member, use the generated name.  This can happen for
       an anonymous union in a namespace. */
    name = scp->name;
    check_assertion(name != NULL);
  }  /* if */
  /* Copy the name. */
  add_str_to_mangled_name(name, mctl);
  if (scp->member_of_unknown_base) {
    /* We're pretending that we found the member in a dependent
       base class.  That means the original form of reference
       was unqualified.  Don't put out the parent qualifier. */
  } else {
    if (distinct_template_signatures && is_specialization) {
      /* Put out an indication of the fact that a static data member is
         specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", mctl);
    /* Output the mangled parent name. */
    mangled_parent_qualifier(scp, mctl);
  }  /* if */
#else /* IA64_ABI */
  if (is_source_corresp_in_namespace_std(scp)) {
    add_str_to_mangled_name("St", mctl);
  } else {
    if (scp->is_class_member) {
      add_prefix_for_local_class_if_necessary(scp->parent.class_type,
                                              mctl);
    }  /* if */
    /* Mark the start of the nested name. */
    add_to_mangled_name('N', mctl);
    /* Output the mangled parent name. */
    mangled_parent_qualifier(scp, mctl);
  }  /* if */
  /* Output the name of the member. */
  mangled_name_with_length(unmangled_name_of(scp), mctl);
  if (!is_source_corresp_in_namespace_std(scp)) {
    /* Mark the end of the nested name. */
    add_to_mangled_name('E', mctl);
  }  /* if */
#endif /* IA64_ABI */
}  /* mangled_member_name */


static void mangled_member_variable_name(a_variable_ptr           variable,
                                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the member variable
"variable" (a static data member or namespace member variable).
*/
{
  a_boolean is_specialization;

  if (!has_name(variable)) {
    /* An anonymous union can cause an unnamed member of a namespace:
         namespace {
           static union {float bf;};
         }
    */
    check_assertion_str(!variable->source_corresp.is_class_member,
                        "mangled_member_variable_name: unnamed class member");
    give_unnamed_member_variable_a_name(variable);
  }  /* if */
  is_specialization = (variable->is_specialized &&
                       !variable->specialized_with_old_syntax);
  mangled_member_name(&variable->source_corresp, is_specialization, mctl);
}  /* mangled_member_variable_name */

#if TEMPLATE_LOOKUP_NEEDED || MODULE_ID_NEEDED

char *get_mangled_member_variable_name(a_variable_ptr variable)
/*
Get the mangled name for the indicated variable or static data member, and
return a pointer to it.  If the variable name has not been mangled yet,
create a copy of the mangled name in a temporary buffer but do not change
the name in the variable entry.  The variable must be a namespace member
or a static data member (e.g., not a file scope variable).
*/
{
  a_mangling_control_block mctl;
  char                     *mangled_name;
  a_boolean                needs_to_be_externalized = FALSE;

#if DO_IL_LOWERING
  /* Static entities are potentially referenced from exported templates
     and therefore get externalized, which gives them a different kind
     of mangled name. */
  needs_to_be_externalized =
                      (variable->storage_class == (a_storage_class)sc_static &&
                       any_exported_templates());
#endif /* DO_IL_LOWERING */
  if (variable->source_corresp.name_has_been_mangled &&
      !variable->source_corresp.final_name_mangling_pending &&
      (!needs_to_be_externalized || variable->source_corresp.externalized)) {
    /* The name has already been completely mangled, so just return it. */
    mangled_name = variable->source_corresp.name;
    /* The variable should not be unnamed. */
    check_assertion(mangled_name != NULL);
  } else {
    /* Generate the mangled name in a buffer. */
    start_mangling(&mctl);
    add_mangled_name_prefix(&mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      start_externalized_name(/*is_variable=*/TRUE, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_member_variable_name(variable, &mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      end_externalized_name(&variable->source_corresp, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_name = end_mangling((a_source_correspondence *)NULL,
                                /*final=*/TRUE, &mctl);
  }  /* if */
  return mangled_name;
}  /* get_mangled_member_variable_name */

#endif /* TEMPLATE_LOOKUP_NEEDED || MODULE_ID_NEEDED */

/* Declaration required because of forward reference: */
static void do_scope_other_name_mangling(a_scope_ptr scope);


static void do_local_name_mangling(
                      a_type_list_processing_routine_ptr list_mangling_routine)
/*
For local types in the current translation unit, call the indicated
mangling routine for type lists.  When orphan lists have been generated,
use them; otherwise, visit the local scopes from the routine scope.
*/
{
  process_local_types(il_header.primary_scope, list_mangling_routine);
}  /* do_local_name_mangling */


static void mangle_class_name(a_type_ptr class_type)
/*
Mangle the name of the indicated class, if necessary.
*/
{
  a_mangling_control_block mctl;

  error_position = class_type->source_corresp.decl_position;
  /* Template class names must be mangled because otherwise all instances
     of the same class template have the same name. */
  if (class_type->variant.class_struct_union.extra_info->
                                                   template_arg_list != NULL &&
      !class_type->source_corresp.name_has_been_mangled) {
    start_mangling(&mctl);
    mangled_basic_class_name(class_type, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, so that the name can be reused more often.
       final_type_name_mangling will do the compression or truncation if
       necessary. */
    (void)end_mangling(&class_type->source_corresp, /*final=*/FALSE, &mctl);
  }  /* if */
}  /* mangle_class_name */


static void do_type_list_class_name_mangling(a_type_ptr type_list)
/*
Do class name mangling for the types on the indicated type list and subscopes
thereunder.  Note that this does not include final processing for type names.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, process it and its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      mangle_class_name(type);
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_type_list_class_name_mangling(class_scope->types);
      }  /* if */
#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_class_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */
    }  /* if */
  }  /* for */
}  /* do_type_list_class_name_mangling */


static void do_scope_class_name_mangling(a_scope_ptr scope)
/*
Do name mangling for class names in the indicated scope (a file or
namespace scope) and all subscopes.  Note that this does not include
final processing for type names.
*/
{
  a_namespace_ptr nsp;

  /* Process the types in the scope. */
  do_type_list_class_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_class_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
}  /* do_scope_class_name_mangling */


void do_class_name_mangling(void)
/*
Do name mangling for all class names.  Note that this does not include
final processing for type names.
*/
{
  /* Process the file scope and all subscopes in the file-scope memory
     region. */
  do_scope_class_name_mangling(il_header.primary_scope);
  /* Process local types. */
  do_local_name_mangling(do_type_list_class_name_mangling);
}  /* do_class_name_mangling */


static void mangle_member_constant_name(a_constant_ptr con)
/*
Mangle the name of the indicated member constant, if necessary.  con
is either an enumerator constant, a namespace member constant, or (as an
extension) a declared class member constant.
*/
{
  a_mangling_control_block mctl;

  error_position = con->source_corresp.decl_position;
  if (!con->source_corresp.name_has_been_mangled) {
    start_mangling(&mctl);
#if IA64_ABI
    /* Add a prefix to avoid name conflicts.  Don't use "_Z" because it's
       sort of reserved for external names.  The mangling here is mostly
       to avoid conflicts in generated C code.*/
    add_str_to_mangled_name("__", &mctl);
#endif /* !IA64_ABI */
    mangled_member_name(&con->source_corresp,
                        /*is_specialization=*/FALSE, &mctl);
    (void)end_mangling(&con->source_corresp, /*final=*/TRUE, &mctl);
  }  /* if */
}  /* mangle_member_constant_name */


static void do_type_list_other_name_mangling(a_type_ptr type_list)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) for the types on the indicated type list and subscopes
thereunder.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, do its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_scope_other_name_mangling(class_scope);
      }  /* if */
#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_other_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */
    } else if (is_immediate_enum_type(type) &&
               (type->source_corresp.is_class_member ||
                type->source_corresp.parent.namespace_ptr != NULL)) {
      /* Mangle the names of member enum constants. */
      a_constant_ptr enum_con;
      for (enum_con = type->variant.integer.enum_info.constant_list;
           enum_con != NULL;
           enum_con = enum_con->next) {
        mangle_member_constant_name(enum_con);
      }  /* for */
    }  /* if */
  }  /* for */
}  /* do_type_list_other_name_mangling */


static void mangle_function_name(a_routine_ptr routine)
/*
Mangle the name of the indicated function, if necessary.
*/
{
  a_boolean                suppress_param_encoding;
  a_mangling_control_block mctl;
  sizeof_t                 *base_name_offset = NULL;

  error_position = routine->source_corresp.decl_position;
  if (!routine->source_corresp.name_has_been_mangled &&
      function_name_mangling_needed(routine, &suppress_param_encoding)) {
    /* Mangle the function name. */
    start_mangling(&mctl);
    add_mangled_name_prefix(&mctl);
#if IA64_ABI && DO_IL_LOWERING
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      base_name_offset = &routine->variant.ctor_dtor.base_name_offset;
    }  /* if */
#endif /* IA64_ABI && DO_IL_LOWERING */
    mangled_function_name(routine, suppress_param_encoding, 
                          base_name_offset,
                          &mctl);
#if !IA64_ABI
    /* Note final=FALSE to prevent compression and truncation at this
       time, so that the name can be used in building names of types
       and variables promoted out of the routine.  do_final_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling(&routine->source_corresp, /*final=*/FALSE, &mctl);
#else /* IA64_ABI */
    /* In the IA64 ABI, the final mangled name should be computed
       immediately. */
    (void)end_mangling(&routine->source_corresp, /*final=*/TRUE, &mctl);
#endif /* IA64_ABI */
  }  /* if */
}  /* mangle_function_name */


static a_boolean variable_name_mangling_needed(a_variable_ptr variable)
/*
Return TRUE if the name of the indicated variable needs to be mangled.
*/
{
  a_boolean mangling_needed = FALSE;

  if (!has_name(variable)) {
    /* Unnamed variables do not need mangled names. */
  } else if (variable->source_corresp.is_class_member ||
             variable->source_corresp.parent.namespace_ptr != NULL) {
    /* Static data members and members of namespaces need mangled names. */
    mangling_needed = TRUE;
    /* But do not mangle namespace members with extern "C" linkage. */
    if (!is_name_linkage_kind_subject_to_name_mangling(
                                      variable->source_corresp.name_linkage)) {
      mangling_needed = FALSE;
    }  /* if */
  }  /* if */
  return mangling_needed;
}  /* variable_name_mangling_needed */


static void mangle_member_variable_name(a_variable_ptr variable)
/*
Mangle the name of the indicated static data member or namespace member
variable.
*/
{
  a_mangling_control_block mctl;

  error_position = variable->source_corresp.decl_position;
  if (!variable->source_corresp.name_has_been_mangled &&
      variable_name_mangling_needed(variable)) {
    start_mangling(&mctl);
    add_mangled_name_prefix(&mctl);
    mangled_member_variable_name(variable, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, in case the name is externalized later.  do_final_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling(&variable->source_corresp, /*final=*/FALSE, &mctl);
  }  /* if */
}  /* mangle_member_variable_name */


static void do_scope_other_name_mangling(a_scope_ptr scope)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) in the indicated scope and its subscopes.  The scope is
a file, namespace, or class scope.  If the scope is the file scope,
function-local entities are also processed.
*/
{
  a_namespace_ptr nsp;
  a_routine_ptr   routine;
  a_variable_ptr  variable;
  a_constant_ptr  con;

  /* Visit all types. */
  do_type_list_other_name_mangling(scope->types);
  if (scope->kind == (a_scope_kind)sck_file) {
    /* When processing the file scope, also process function-local types. */
    do_local_name_mangling(do_type_list_other_name_mangling);
    /* Visit all constants.  This is generally useless, but there might be
       constants that were promoted out of a local class into the file
       scope. */
    /* Look for member constants (an extension in classes) and mangle their
       names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      if (con->source_corresp.is_class_member) {
        mangle_member_constant_name(con);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Visit all namespaces. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Make sure that an unnamed namespace in a secondary translation unit
         is given a mangled name in that translation unit, so it has the
         right module id.  If the namespace contains only types, the name
         wouldn't otherwise be mangled at this time. */
      give_unnamed_namespace_a_name(nsp);
      do_scope_other_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    mangle_function_name(routine);
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_class_struct_union ||
      scope->kind == (a_scope_kind)sck_namespace) {
    /* For a class or namespace scope, visit the member variables
       and constants. */
    /* Look for static data members/namespace member variables and mangle
       their names. */
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      mangle_member_variable_name(variable);
    }  /* for */
    /* Look for member constants (an extension in classes) and mangle their
       names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      mangle_member_constant_name(con);
    }  /* for */
  }  /* if */
}  /* do_scope_other_name_mangling */


void do_all_name_mangling(void)
/*
Do any required name mangling.  This is called at the beginning of lowering of
the file scope.  It processes everything in the file scope and also
function-local entities that require mangling.  Final name mangling
is not done yet -- see do_final_name_mangling.
*/
{
  /* Mangle class names, not including final mangling on type names. */
  do_class_name_mangling();
  /* Do function, namespace, and static data member name mangling, not
     including some final mangling. */
  do_scope_other_name_mangling(il_header.primary_scope);
}  /* do_all_name_mangling */


static void final_entity_name_mangling(a_source_correspondence *scp)
/*
Do any final name mangling processing required on the entity with
the indicated source correspondence.  This means checking for
compression and truncation.
*/
{
  if (scp->final_name_mangling_pending) {
    a_mangling_control_block mctl;
    char                     *name = scp->name;
    sizeof_t                 length = strlen(name)+1;

    error_position = scp->decl_position;
    check_assertion(name != NULL);
    /* One reason for calling start_mangling here is to zero
       mangling_text_buffer->size. */
    /* If neither compression nor truncation is done, the name pointer
       is passed through unchanged.  If compression is done, the compressed
       name is allocated in IL memory.  If truncation is done, the existing
       name is truncated in place. */
    start_mangling(&mctl);
    mctl.length = length;
#if !IA64_ABI
    name = compress_mangled_name(name, scp, &mctl);
#endif /* !IA64_ABI */
    name = truncate_mangled_name(name, scp, &mctl);
    scp->name = name;
    scp->final_name_mangling_pending = FALSE;
  }  /* if */
}  /* final_entity_name_mangling */


/*
The prefix put on the front of the type encoding for a nested type to get
the name placed in the nested type itself.
*/
#define PREFIX_ON_NESTED_TYPE_NAME "__"


static void final_type_name_mangling(a_type_ptr type)
/*
Do final mangling on a type name, mangling that would prevent the mangled
form of the name from being usable as part of another mangled name.
Such processing is delayed to the end to allow reuse of the mangled name
(and the attendant time savings) as many times as possible.
This does special processing for nested type names, compressed names,
and truncated names.
*/
{
  a_mangling_control_block mctl;

  error_position = type->source_corresp.decl_position;
  check_assertion_str2(!type->source_corresp.
                                 mangled_name_cannot_be_included_in_other_name,
                       "final_type_name_mangling:", 
                       "mangled_name_cannot_be_included_in_other_name is set");
  if (has_name(type)) {
    if (type_needs_parent_qualifier(type)
#if IA64_ABI
        || is_in_namespace_std(type)
#endif /* IA64_ABI */
                                         ) {
      /* Nested type names must be mangled (because they exist in a scope
         that does not exist in the generated C code).  The mangled form
         is something like
           __Q2_1A1B
         The "Q2_1A1B" part is the normal representation for a mangled
         name, and the prefix makes it unique (i.e., makes it distinct
         from all user identifiers).  Similar mangling is used for members
         of namespaces (a different kind of "nested" type). */
      start_mangling(&mctl);
      add_str_to_mangled_name(PREFIX_ON_NESTED_TYPE_NAME, &mctl);
      mangled_type_name(type, &mctl);
      /* The following does compression and truncation if necessary. */
      (void)end_mangling(&type->source_corresp, /*final=*/TRUE, &mctl);
      type->source_corresp.mangled_name_cannot_be_included_in_other_name= TRUE;
#if IA64_ABI
    } else if (is_immediate_class_type(type) &&
               type->variant.class_struct_union.extra_info != NULL &&
               type->variant.class_struct_union.extra_info->assoc_template 
                                                                    != NULL) {
      /* A type instantiated from a template will have a name that might
         collide with other types in the user namespace.  Therefore, we add
         the prefix in this case as well.  */
      start_mangling(&mctl);
      add_str_to_mangled_name(PREFIX_ON_NESTED_TYPE_NAME, &mctl);
      add_str_to_mangled_name(type->source_corresp.name, &mctl);
      (void)end_mangling(&type->source_corresp, /*final=*/TRUE, &mctl);
      type->source_corresp.mangled_name_cannot_be_included_in_other_name= TRUE;
#endif /* IA64_ABI */
    } else {
      /* Not a nested type.  Check for compression and truncation. */
      final_entity_name_mangling(&type->source_corresp);
    }  /* if */
  }  /* if */
}  /* final_type_name_mangling */


static void do_scope_final_name_mangling(a_scope_ptr scope);


static void do_type_list_final_name_mangling(a_type_ptr type_list)
/*
Do final name mangling for the types on the indicated type list
and subscopes thereunder.  Functions and variables in the subscopes are
also processed.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, do its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_scope_final_name_mangling(class_scope);
      }  /* if */
#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_final_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */
    }  /* if */
    /* Do name mangling on the type. */
    /* Note that the call here must be done after all subscopes have been
       visited; we don't want to change the name of a class until the
       classes nested within it have been processed. */
    final_type_name_mangling(type);
  }  /* for */
}  /* do_type_list_final_name_mangling */


static void do_scope_final_name_mangling(a_scope_ptr scope)
/*
Do final name mangling for all type, function, and variable names in the
indicated scope (a file, namespace, or class scope) and all subscopes.
*/
{
  a_namespace_ptr nsp;
  a_routine_ptr   routine;
  a_variable_ptr  variable;

  /* Process the types in the scope. */
  do_type_list_final_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_final_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    final_entity_name_mangling(&routine->source_corresp);
  }  /* for */
  /* Visit all variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    final_entity_name_mangling(&variable->source_corresp);
  }  /* for */
}  /* do_scope_final_name_mangling */


void do_final_name_mangling(void)
/*
Do final name mangling for all type, function, and variable names.  This
must be done separately from and later than normal name mangling because
the simple form of the name must remain available for use in mangled names
(e.g., virtual function table variable names).
*/
{
  /* Process the file scope and all subscopes in the file-scope memory
     region. */
  do_scope_final_name_mangling(il_header.primary_scope);
  /* Process local types. */
  do_local_name_mangling(do_type_list_final_name_mangling);
}  /* do_final_name_mangling */

#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY

static a_boolean base_class_of_same_name_exists(a_base_class_ptr orig_bcp)
/*
Return TRUE if in the base class list of which orig_bcp is a part there is
another base class with the same name.  Note that this is "same name," not
necessarily "same type."
*/
{
  a_boolean        same_name_exists = FALSE;
  a_base_class_ptr bcp;

  for (bcp = orig_bcp->derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp != orig_bcp) {
      char *bcp_name      = unmangled_name_of(&bcp->type->source_corresp);
      char *orig_bcp_name = unmangled_name_of(&orig_bcp->type->source_corresp);
      if (bcp_name != NULL && orig_bcp_name != NULL &&
          strcmp(bcp_name, orig_bcp_name) == 0) {
        /* Found another base class with the same name. */
        same_name_exists = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* base_class_of_same_name_exists */

#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */

static void mangled_derivation_name(a_derivation_step_ptr    dsp,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the indicated
derivation.  This is used for the base class part of virtual function
table names.
*/
{
  a_type_ptr class_type;

  /* The name must be put out backwards, so use recursion to get to the
     bottom of the list. */
  if (dsp->next != NULL) {
    mangled_derivation_name(dsp->next, mctl);
    /* Add two underscores to separate names. */
    add_str_to_mangled_name("__", mctl);
  }  /* if */
  /* Put out the name on the first derivation step. */
  class_type = dsp->base_class->type;
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
  /* cfront doesn't encode nested class information in base class names.
     This doesn't work in general, because it is possible to have base
     classes with the same basic name and different qualified names (e.g.,
     "A" and "A::B").  cfront doesn't seem to work right on all the cases
     with repeated basic names, so it gets away with it.  We use the
     cfront-compatible mangling only if there is no other base class
     with the same name. */
  if (!base_class_of_same_name_exists(dsp->base_class)) {
    mangled_basic_class_name(class_type, mctl);
  } else
#endif /* ABI_COMPATIBILITY_VERSION >= 230  && ... */
  /* Do not insert code here -- this is the "else" of an "if". */
  {
    /* Note the use of mangled_class_name_internal instead of
       mangled_vtbl_class_name because we do not want two lengths on
       the front of nested class names. */
    mangled_class_name_internal(class_type, mctl);
  }
}  /* mangled_derivation_name */


static a_boolean virtual_base_class_of_same_name_exists(
                                                      a_base_class_ptr dir_bcp)
/*
Return TRUE if in the base class list of which dir_bcp (a direct, nonvirtual
base class) is a part there is also a virtual base class of the same name.
*/
{
  a_boolean        same_name_exists = FALSE;
  a_base_class_ptr bcp;

  for (bcp = dir_bcp->derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->is_virtual && same_entities(bcp->type, dir_bcp->type)) {
      same_name_exists = TRUE;
      break;
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* virtual_base_class_of_same_name_exists */


static long ambiguous_base_class_number(a_base_class_ptr bcp)
/*
Return an "ambiguous base class number" for the (ambiguous) base class bcp,
to be used in qualifying its name in a virtual function table mangled
name.  A return value of -1 indicates the base class that cfront discards,
which gets special treatment; otherwise, the value is non-negative.
*/
{
  long             num, count = -1;
  a_base_class_ptr test_bcp;
  a_type_ptr       class_type = bcp->derived_class;

  check_assertion(bcp->ambiguous);
  for (test_bcp = base_classes_of(class_type);
       ;
       test_bcp = test_bcp->next) {
    check_assertion(test_bcp != NULL);
    /* Count ambiguous base classes with the same name, in order. */
    if (test_bcp->ambiguous &&
        same_entities(test_bcp->type, bcp->type)) {
      if (bcp->direct && !bcp->is_virtual &&
          virtual_base_class_of_same_name_exists(bcp)) {
        /* This base class is a direct nonvirtual base class and there is
           a virtual base class with the same name.  cfront discards this
           base class (and therefore its virtual function table too), so
           this one is always qualified, even if it is the first one.
           That allows us to generate the same mangled name (an unqualified
           one) for the base class that cfront does keep (at least, if there
           is only one of those). */
        num = -1;
      } else {
        count++;
        num = count;
      }  /* if */
      /* Stop if we have found the base class we were looking for.  num is
         the number to use for it. */
      if (test_bcp == bcp) break;
    }  /* if */
  }  /* for */
  return num;
}  /* ambiguous_base_class_number */


static void mangled_vtbl_base_class_name(a_base_class_ptr         bcp,
                                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of a base class in
a virtual function table.  The name describes the base class given by bcp.
*/
{
  a_derivation_step_ptr dsp;
  a_length_reservation  length_reservation;

  /* The form of the name is like
       4abcd
     or
       8abcd__ef  (this for base class "abcd" in "ef")
     For virtual base classes, or nonvirtual base classes within virtual
     base classes, the first step is directly to the virtual base class.
  */
  dsp = cast_derivation_path_of(bcp);
  /* Put out the name length and the name. */
  reserve_space_for_length(&length_reservation, mctl);
  mangled_derivation_name(dsp, mctl);
  fill_in_length(&length_reservation, mctl);
  if (bcp->ambiguous) {
    /* Ambiguous base classes get a suffix to differentiate the different
       like-named base classes. */
    long num = ambiguous_base_class_number(bcp);
    if (num == 0) {
      /* The first ambiguous base class gets no suffix. */
    } else {
      add_str_to_mangled_name("__A", mctl);
      if (num < 0) {
        /* The base class that cfront discards (a direct nonvirtual base class
           with the same name as a virtual base class) gets the simple "__A"
           encoding.  This is for historical reasons: until version 3.0 of
           the EDG C++ Front End, this was the only ambiguity qualifier
           (we hadn't realized that there were other possibilities), so
           this one is kept the same to avoid an ABI change. */
      } else {
        /* For other base classes, use a __Ann encoding, where nn is the
           base class number. */
        add_number_to_mangled_name((unsigned long)num, mctl);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* mangled_vtbl_base_class_name */


static void mangled_vtbl_class_name(a_type_ptr               type,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type"
for use in a virtual function table name.
*/
{
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY && \
    !IA64_ABI
  /* cfront mode. */
  if (type_needs_parent_qualifier(type)) {
    /* The type is a nested type.  Add a length in front of the mangled
       form (e.g., "7Q2_1A1B" instead of "Q2_1A1B"). */
    a_length_reservation length_reservation;
    reserve_space_for_length(&length_reservation, mctl);
    mangled_type_name(type, mctl);
    fill_in_length(&length_reservation, mctl);
  } else {
    /* Not a nested type name; just put out the type encoding. */
    mangled_type_name(type, mctl);
  }  /* if */
#else /* ABI_COMPATIBILITY_VERSION < 230 || ... */
  /* In non-cfront mode, or in old ABI versions, just pass through to
     mangled_type_name. */
  mangled_type_name(type, mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */
}  /* mangled_vtbl_class_name */


#if !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*ARGSUSED*/ /* <-- ctor_bcp is not used in that case. */
#endif /* !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
char *mangled_vtbl_name(a_type_ptr               class_type,
                        a_base_class_ptr         bcp,
                        a_base_class_ptr         ctor_bcp)
/*
Return the mangled name for the virtual function table for base class
bcp of class class_type.  If bcp == NULL, the virtual function table is
for class_type itself.  If ctor_bcp is non-NULL, it is the base class
for class_type as a subobject of some larger class type that is the
actual complete object type (used in determining layout); class_type
in that case is the type considered to be the complete object type
for purposes of overriding (this is used during constructors and
destructors).  The name returned is in a temporary buffer and must
be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  /* Determine the mangled name.  It is
       __vtbl__<mangled-base-class-name>__<mangled-class-name> or
       __vtbl__<mangled-class-name>
     The mangled-base-class-name is really a sort of pathname for the
     base class, giving the base class names from base to derived.
     For example, __vtbl__5X__X1__1B for base class X inside X1 inside
     a whole object of type B.
  */
#if !IA64_ABI
  add_str_to_mangled_name("__vtbl__", &mctl);
#else /* IA64_ABI */
  add_mangled_name_prefix(&mctl);
  add_str_to_mangled_name("TV", &mctl);
#endif /* IA64_ABI */
  if (bcp != NULL) {
    /* Add the base class name. */
    mangled_vtbl_base_class_name(bcp, &mctl);
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", &mctl);
  }  /* if */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  if (ctor_bcp != NULL) {
    /* There is a complete class type, so the name looks like
       __vtbl__<mangled-base-class-name>__<mangled-base-class-name>
                                        __<mangled-complete-class-name>
    */
    /* Add the second base class name. */
    mangled_vtbl_base_class_name(ctor_bcp, &mctl);
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", &mctl);
    class_type = ctor_bcp->derived_class;
  }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
  /* Add the derived class name. */
  mangled_vtbl_class_name(class_type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_vtbl_name */


char *mangled_class_name(a_type_ptr type)
/*
Return the mangled name of the class "type".  This is the encoding used
for the name of the class as opposed to the encoding for the class as
a type (for example, it has no length preceding a simple class name).
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  mangled_class_name_internal(type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_class_name */


void mangle_subobject_class_name(a_type_ptr class_type,
                                 a_type_ptr subobject_type)
/*
Set the mangled name of a type generated by IL lowering as the
type-as-subobject of class_type.
*/
{
  a_mangling_control_block mctl;
  char                     *temp_name, *new_name_ptr;

  if (has_name(class_type)) {
    start_mangling(&mctl);
    add_str_to_mangled_name("__SO__", &mctl);
    mangled_basic_class_name(class_type, &mctl);
    /* Not "final" because this type will go through the final processing
       later.  We don't want to (e.g.) compress twice. */
    temp_name = end_mangling((a_source_correspondence *)NULL,
                             /*final=*/FALSE, &mctl);
    new_name_ptr = alloc_lowered_name_string((sizeof_t)strlen(temp_name)+1);
    (void)strcpy(new_name_ptr, temp_name);
    subobject_type->source_corresp.name = new_name_ptr;
    subobject_type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_subobject_class_name */


static char *mangled_prefixed_type_encoding(char       *prefix,
                                            a_type_ptr type)
/*
Return a mangled name that is the indicated prefix followed by the encoding
for the indicated type.  The name returned is in a temporary buffer and must
be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  add_mangled_name_prefix(&mctl);
  /* Start with the prefix. */
  add_str_to_mangled_name(prefix, &mctl);
  /* Add the mangled name of the type. */
  mangled_encoding_for_type(type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_prefixed_type_encoding */


char *mangled_typeinfo_name(a_type_ptr type)
/*
Return the mangled name for the typeinfo variable for type "type".
A typeinfo variable is used to describe runtime type information.
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
#if !IA64_ABI
  /* The mangled name looks like
       __T_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__T_", type);
#else /* IA64_ABI */
  return mangled_prefixed_type_encoding("TI", type);
#endif /* IA64_ABI */
}  /* mangled_typeinfo_name */

#if !IA64_ABI

char *mangled_id_object_name(a_type_ptr type)
/*
Return the mangled name for the id object variable for type "type".
The id object variable is pointed to by the typeinfo variable used
to provide runtime type information.  The name returned is in
a temporary buffer and must be copied elsewhere.
*/
{
  /* The mangled name looks like
       __TID_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__TID_", type);
}  /* mangled_id_object_name */

#else /* IA64_ABI */

char *mangled_typeinfo_string(a_type_ptr type)
/*
Return the mangled name for type, as suitable for using in a typeinfo string.
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  /* Add the mangled name of the type. */
  mangled_encoding_for_type(type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_typeinfo_string */


char *mangled_typeinfo_string_name(a_type_ptr type)
/*
Return the mangled name for the typeinfo string variable for type "type".
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  /* The mangled name looks like
       _ZTS<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("TS", type);
}  /* mangled_typeinfo_string_name */


char *mangled_virtual_table_table_name(a_type_ptr type)
/*
Return the mangled name for the virtual table table variable for type
"type". The name returned is in a temporary buffer and must be copied
elsewhere.
*/
{
  /* The mangled name looks like
       _ZTT<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("TT", type);
}  /* mangled_typeinfo_string_name */

#endif /* IA64_ABI */

#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE || LOWER_EXTERN_INLINE

#if !IA64_ABI

static unsigned long search_scope_list(a_scope_ptr scope,
                                       a_scope_ptr scope_to_search,
                                       a_boolean   *found)
/*
Look for "scope" in "scope_to_search".  If it is found, set *found to TRUE
and return the position where it was found: 0 means scope and scope_to_search
are the same scope; scopes under scope_to_search are numbered in tree
traversal order starting from 1.  If the scope is not found, *found is not
changed (it is expected to be FALSE) and the count of scopes in the tree is
returned.
*/
{
  unsigned long scope_number;

  if (scope == scope_to_search) {
    scope_number = 0;
    *found = TRUE;
  } else {
    a_scope_ptr sp;
    scope_number = 1;
    for (sp = scope_to_search->scopes; sp != NULL; sp = sp->next) {
      scope_number += search_scope_list(scope, sp, found);
      if (*found) break;
    }  /* for */
  }  /* if */
  return scope_number;
}  /* search_scope_list */

#endif /* !IA64_ABI */

#if IA64_ABI
/*ARGSUSED*/  /* <-- scope is not used in that case. */
#endif /* IA64_ABI */
void mangle_promoted_entity_name(a_source_correspondence *scp,
                                 a_boolean               final,
                                 a_routine_ptr           routine,
                                 a_scope_ptr             scope)
/*
scp points to the source correspondence field of an entity that is
being promoted out of the routine "routine" (or one of its block
scopes) to the file scope.  scope indicates the scope out of which the
entity is being promoted (a function or block scope).  Give the entity
a mangled name if necessary.  If final is TRUE, do the final name
mangling, which may produce a name that can no longer be embedded in
other mangled names.
*/
{
  a_mangling_control_block mctl;

  /* Leave the name alone if the entity is unnamed. */
  if (scp->name != NULL) {
    start_mangling(&mctl);
    /* Name mangling is needed. */
#if !IA64_ABI
    { unsigned long scope_number;
      /* The encoding is the original name, followed by "__Lnn", where "nn"
         is the scope number within the function, followed by two underscores,
         followed by the mangled name of the routine. */
      /* Develop a scope number for the scope in which the entity appears.
         This number must be relative to the function rather than to the
         whole compilation so that if a given function (e.g., an extern inline
         function) is compiled in more than one compilation unit the scope
         number -- and therefore the mangled name -- will be the same in each
         compilation. */
      a_scope_ptr rout_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
      a_boolean   found = FALSE;
      scope_number = search_scope_list(scope, rout_scope, &found);
      check_assertion_str(found,
                          "mangle_promoted_entity_name: scope not found");
      add_str_to_mangled_name(scp->name, &mctl);
      add_local_name_suffix(scope_number, routine, &mctl);
    }
#else /* IA64_ABI */
    add_mangled_name_prefix(&mctl);
    add_str_to_mangled_name("Z", &mctl);
    mangled_function_name(routine, /*suppress_param_encoding=*/FALSE,
                          /*base_name_offset=*/(sizeof_t *)NULL,
                          &mctl);
    add_to_mangled_name('E', &mctl);
    mangled_name_with_length(scp->name, &mctl);
    add_discriminator_if_necessary(scp, &mctl);
#endif /* !IA64_ABI */
    (void)end_mangling(scp, final, &mctl);
  }  /* if */
}  /* mangle_promoted_entity_name */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE || LOWER_EXTERN_INLINE */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN

void mangle_covariant_return_type_entry_name(a_routine_ptr entry_routine)
/*
entry_routine points to a routine that represents an entry point of
another routine (which is a virtual function with a covariant return type, 
and/or which requires adjustments to "this" on entry).  The entry routine is
like the primary routine, but does a derived-to-base cast on the returned
pointer, or performs the "this" adjustments.
*/
{
  a_mangling_control_block mctl;
  a_routine_ptr            prim_routine;
#if !IA64_ABI
  a_type_ptr               overridden_class;
#endif /* !IA64_ABI */

  prim_routine = entry_routine->overriding_function_for_covariant_return_type;
  start_mangling(&mctl);
#if !IA64_ABI
  overridden_class = 
            entry_routine->overridden_function_for_covariant_return_type->
                                             source_corresp.parent.class_type;
  /* The mangled name has the form
       __VFE__<overridden_class>__<prim_routine>
     where <overridden_class> and <prim_routine> are the mangled names for
     those entities. */
  add_str_to_mangled_name("__VFE__", &mctl);
  /* Add the class name. */
  mangled_type_name(overridden_class, &mctl);
  /* Add two underscores after the class name. */
  add_str_to_mangled_name("__", &mctl);
#else /* IA64_ABI */
  add_mangled_name_prefix(&mctl);
  /* Distinguish between covariant returns and ordinary thunks. */
  if (entry_routine->return_delta != 0 ||
      entry_routine->vbase_index != 0) {
    add_str_to_mangled_name("Tc", &mctl);
  } else {
    add_to_mangled_name('T', &mctl);
  }  /* if */
  /* Add the this-adjustment information. */
  if (entry_routine->vcall_index != 0) {
    add_to_mangled_name('v', &mctl);
  } else {
    add_to_mangled_name('h', &mctl);
  }  /* if */
  add_signed_number_to_mangled_name(entry_routine->delta, &mctl);
  add_to_mangled_name('_', &mctl);
  if (entry_routine->vcall_index != 0) {
    add_signed_number_to_mangled_name((entry_routine->vcall_index *
                                       make_vtbl_entry_type()->size), 
                                      &mctl);
    add_to_mangled_name('_', &mctl);
  }  /* if */
  /* Add the return-adjustment information. */
  if (entry_routine->return_delta != 0 ||
      entry_routine->vbase_index != 0) {
    if (entry_routine->vbase_index != 0) {
      add_to_mangled_name('v', &mctl);
    } else {
      add_to_mangled_name('h', &mctl);
    }  /* if */
    add_signed_number_to_mangled_name(entry_routine->return_delta, &mctl);
    add_to_mangled_name('_', &mctl);
    if (entry_routine->vbase_index != 0) {
      add_signed_number_to_mangled_name((entry_routine->vbase_index * 
                                         make_vtbl_entry_type()->size), 
                                        &mctl);
      add_to_mangled_name('_', &mctl);
    }  /* if */
  }  /* if */
  if (prim_routine->source_corresp.name_has_been_mangled) {
    /* The name of the primary routine has already been mangled, so we can
       reuse it.  This is not just an optimization; if the primary routine is
       an alternate entry point for a destructor, there will be no unmangled
       name and mangled_function_name would abort. */
    add_str_to_mangled_name(prim_routine->source_corresp.name + 2,
                            &mctl);
  } else 
#endif /* IA64_ABI */
  /* Do not add code here. */
  {
    /* Add the routine name. */
    mangled_function_name(prim_routine,
                          /*suppress_param_encoding=*/FALSE,
                          /*base_name_offset=*/(sizeof_t *)NULL,
                          &mctl);
  }  /* if */
  (void)end_mangling(&entry_routine->source_corresp, /*final=*/TRUE, &mctl);
}  /* mangle_covariant_return_type_entry_name */

#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#endif /* DO_IL_LOWERING */

#if !IA64_ABI

static a_compressible_string_pos_ptr alloc_compressible_string_pos(void)
/*
Allocate compressible string position entry, set its fields to default
values, and return a pointer to it.
*/
{
  a_compressible_string_pos_ptr cspp;

  if (avail_compressible_string_pos != NULL) {
    /* Reuse a freed entry. */
    cspp = avail_compressible_string_pos;
    avail_compressible_string_pos = cspp->next;
  } else {
    /* Allocate a new entry. */
    cspp = (a_compressible_string_pos_ptr)alloc_fe(
                                            sizeof(a_compressible_string_pos));
#if DEBUG
    num_compressible_string_pos_allocated++;
#endif /* DEBUG */
  }  /* if */
  cspp->next = NULL;
  cspp->str_pos = 0;
  return cspp;
}  /* alloc_compressible_string_pos */


static void free_compressible_string_pos(a_compressible_string_pos_ptr cspp)
/*
Free the indicated compressible string position entry by returning it
to the available list for reuse.
*/
{
  cspp->next = avail_compressible_string_pos;
  avail_compressible_string_pos = cspp;
}  /* free_compressible_string_pos */


static char *compress_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl)
/*
Compress the mangled name that's been built up (pointed to by mangled_name,
with length given by mctl->length, including a terminating null).
If mangled_name is NULL, the mangled name is in mangling_text_buffer, starting
at offset 0.  It is important to pass NULL, and not a pointer to
mangling_text_buffer, in that case.  Return a pointer to the name, either
the original one or a compressed version.  The compressed version is
in mangling_text_buffer if mangled_name is NULL, and allocated in IL memory if
mangled_name is non-NULL.  mangling_text_buffer->size must indicate the
first available position in mangling_text_buffer (e.g., after the terminating
null of the mangled name).  scp, if non-NULL, points to the source
correspondence entry for the entity whose name this is.
*/
{
  char *compr_name = NULL;

/* Macro to determine the input buffer address.  This is recomputed each
   time it is needed because the mangling_text_buffer might move. */
#define src_mangled_name \
  ((mangled_name == NULL) ? mangling_text_buffer->buffer : mangled_name)

  /* See whether the name should be examined to see if it is
     compressible.  mctl->length indicates the length of the name,
     including the terminating null.  Don't try compression if the name
     is already fairly small.  Note that one advantage of avoiding
     compression on relatively small names is allowing more compatibility
     with libraries compiled by cfront.  The largest name noted in the
     iostream package had 56 characters. */
  if (compress_mangled_names && mctl->length >= 60) {
    /* Build up the compressed name in the mangling_text_buffer, following
       anything already in there (e.g., after the null character at the
       end of the mangled name). */
    /* Note that positions in the mangling_text_buffer are kept as offsets
       rather than pointers because the mangling_text_buffer may get moved
       if it is resized. */
    sizeof_t start_of_compressed_name = mangling_text_buffer->size;
    sizeof_t src_pos = 0;
    a_compressible_string_pos_ptr
             cspp;
    sizeof_t size_of_mangled_name = mctl->length; /* Including final null. */
    sizeof_t size_of_compressed_name, prefix_length;
    sizeof_t i;
    char     buffer[20];
#define NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE 64
    a_compressible_string_pos_ptr
             hash_table[NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE];
    /* Clear the hash table used to keep track of the position of
       compressible strings in the original mangled name. */
    memzero((char *)hash_table, sizeof(hash_table));
    
    for (;;) {
      /* Copy characters from the original name to the mangled name, looking
         for a string of digits (which starts a compressible section). */
      char ch = src_mangled_name[src_pos];
      if (ch == '\0') break;
      if (!isdigit((unsigned char)ch)) {
        add_char_to_text_buffer(mangling_text_buffer, ch);
        /* If a "J" appears, copy it as "JJ" to avoid confusion with the
           "J" markers used to indicate compression. */
        if (ch == 'J') add_char_to_text_buffer(mangling_text_buffer, 'J');
        src_pos++;
      } else {
        /* A digit.  This may be the start of a compressible string. */
        sizeof_t      num_digits = 1;
        sizeof_t      digit, length, hash_value;
        unsigned long value = (ch - '0');
        a_boolean     ovflo = FALSE, valid, compressed = FALSE;
        /* Determine the number of digits in the digit string and accumulate
           its value. */
        for (;;) {
          ch = src_mangled_name[src_pos+num_digits];
          if (!isdigit((unsigned char)ch)) break;
          digit = ch - '0';
          num_digits++;
          if (value > ULONG_MAX / 10) ovflo = TRUE;
          value *= 10;
          if (value > ULONG_MAX-digit) ovflo = TRUE;
          value += digit;
        }  /* for */
        /* See whether the length is valid. */
        if (ovflo) {
          valid = FALSE;
        } else if (value < 4) {
          /* Don't compress very small strings like "3ABC", because
             the compressed form is probably not smaller.  This also
             discards cases where a user variable has a name like "f2",
             which are not worth examining. */
          valid = FALSE;
        } else if ((length = num_digits + value),
                   size_of_mangled_name-src_pos <= length) {
          /* The length is too long -- it runs off the end of the mangled
             name.  That means it can't be a real length. */
          valid = FALSE;
        } else {
          /* The length is okay. */
          valid = TRUE;
        }  /* if */
        if (valid) {
          /* See whether the string has appeared previously by comparing
             it against the strings in the hash table. */
          hash_value = value % NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE;
          for (cspp = hash_table[hash_value];
               cspp != NULL;
               cspp = cspp->next) {
            /* Compare the previous string to this new string. */
            if (strncmp(src_mangled_name+cspp->str_pos,
                        src_mangled_name+src_pos,
                        size_t_arg(length)) == 0) {
              /* Found an identical previous string, so we can compress it. */
              compressed = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        if (compressed) {
          /* Replace the string by "JnnnJ", where "nnn" is the position of
             the previous identical string. */
          (void)sprintf(buffer, "J%luJ", (unsigned long)cspp->str_pos);
          add_string_to_text_buffer(mangling_text_buffer, buffer);
          /* Continue scanning the original string after the full string
             that was compressed away. */
          src_pos += length;
        } else {
          /* The string has not appeared previously.  If it's a valid
             string, remember it for possible later reuse. */
          if (valid) {
            cspp = alloc_compressible_string_pos();
            cspp->str_pos = src_pos;
            cspp->next = hash_table[hash_value];
            hash_table[hash_value] = cspp;
          }  /* if */
          /* Put out the digit string. */
          for (i = 0; i < num_digits; i++) {
            add_char_to_text_buffer(mangling_text_buffer,
                                    src_mangled_name[src_pos]);
            src_pos++;
          }  /* for */
          /* Continue scanning the original string after the digit
             string. */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Add the final null. */
    add_char_to_text_buffer(mangling_text_buffer, '\0');
    /* Free the entries in the hash table. */
    for (i = 0; i < NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE; i++) {
      a_compressible_string_pos_ptr cspp_next;
      for (cspp = hash_table[i]; cspp != NULL; cspp = cspp_next) {
        cspp_next = cspp->next;
        free_compressible_string_pos(cspp);
      } /* for */
    }  /* for */
    /* The prefix on the compressed form is "__CPR" followed by the size
       of the original (uncompressed) name, not counting the final null. */
    (void)sprintf(buffer, "__CPR%lu__", (unsigned long)size_of_mangled_name-1);
    prefix_length = strlen(buffer);
#if EXPENSIVE_CHECKING
    /* Make sure the name does not already have the compression prefix in
       it.  If it does, we've used a previously compressed name in building
       up this name, and that won't work. */
    check_assertion_str(strstr(mangling_text_buffer->buffer +
                                                      start_of_compressed_name,
                               "__CPR") == NULL,
                        "compress_mangled_name: double compression");
#endif /* EXPENSIVE_CHECKING */
    size_of_compressed_name = (mangling_text_buffer->size -
                               start_of_compressed_name) +
                              prefix_length;
    /* Note that both size_of_compressed_name and size_of_mangled_name
       include the terminating null. */
    if (size_of_compressed_name < size_of_mangled_name) {
      /* The compressed name is shorter, so use it.  (There are some
         pathological cases where the compressed version might be larger.) */
      if (mangled_name == NULL) {
        /* The original mangled name is in mangling_text_buffer, preceding
           the compressed form. */
        /* Put the prefix out in front of the compressed name, and return
           the position of the prefix in that position as the address of
           the full compressed name. */
        check_assertion(start_of_compressed_name >= prefix_length);
        compr_name = mangling_text_buffer->buffer+start_of_compressed_name -
                     prefix_length;
        (void)memcpy(compr_name, buffer, size_t_arg(prefix_length));
      } else {
        /* The mangled name is not in mangling_text_buffer.  Allocate new IL
           memory for the compressed name, including the prefix. */
        compr_name = alloc_lowered_name_string(size_of_compressed_name);
        (void)memcpy(compr_name, buffer, size_t_arg(prefix_length));
        (void)strcpy(compr_name+prefix_length,
                     mangling_text_buffer->buffer+start_of_compressed_name);
      }  /* if */
      mangled_name = compr_name;
      /* Update the length, including the null terminator. */
      mctl->length = size_of_compressed_name;
      if (scp != NULL) {
        /* A compressed name cannot be used as part of another mangled name. */
        scp->mangled_name_cannot_be_included_in_other_name = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* If the name was not compressed, return the source mangled name address. */
  if (compr_name == NULL) compr_name = src_mangled_name;
  return compr_name;
#undef get_char_from_mangled_name
}  /* compress_mangled_name */

#endif /* !IA64_ABI */

static char *truncate_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl)
/*
If necessary, truncate the mangled name that has been built up.  The
name is pointed to by mangled_name.  Its length (with terminating
null) is given by mctl->length.  If the name is longer than
max_mangled_name_length (and the latter is greater than zero), truncate it
by computing a CRC checksum and putting the checksum, in hex, at the end
of as much of the name as will fit along with the checksum.  Such
a truncated name is short enough, and likely to be unique, but it
cannot be demangled.  scp, if non-NULL, points to the source
correspondence entry for the entity whose name this is.
*/
{
  if (max_mangled_name_length != 0 &&
      mctl->length-1 > max_mangled_name_length) {
    /* The name must be truncated. */
    /* The suffix is of the form "__abcdabcd", i.e., one needs 10 characters
       for it. */
    sizeof_t max_allowed_length = max_mangled_name_length - 10;
    (void)sprintf(mangled_name+max_allowed_length, "__%08lx",
                  crc_32(mangled_name, (unsigned long)0));
    mctl->length = max_mangled_name_length+1;
    if (scp != NULL) {
      /* A truncated name cannot be used as part of another mangled name. */
      scp->mangled_name_cannot_be_included_in_other_name = TRUE;
    }  /* if */
  }  /* if */
  return mangled_name;
}  /* truncate_mangled_name */


void name_lower_one_time_init(void)
/*
Do one-time initialization of variables related to name mangling.
*/
{
  /* Allocate the text buffer used for mangling. */
  mangling_text_buffer = alloc_text_buffer(2048);
  second_mangling_text_buffer = NULL;
  /* Save variables from lower_name.c that are needed for precompiled
     headers */
  if (exceptions_enabled && precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(unnamed_class_name_seed),
      pch_saved_var_array_elem(unnamed_enum_name_seed),
      pch_saved_var_array_elem(unnamed_member_variable_name_seed),
#if IA64_ABI
      pch_saved_var_array_elem(avail_substitutions),
#endif /* IA64_ABI */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(unnamed_class_name_seed);
  register_trans_unit_variable(unnamed_enum_name_seed);
  register_trans_unit_variable(unnamed_member_variable_name_seed);
}  /* name_lower_one_time_init */


void name_lower_init(void)
/*
Initialize static variables related to name mangling that must be
initialized for each compilation.
*/
{
  unnamed_class_name_seed = 0;
  unnamed_enum_name_seed = 0;
  unnamed_member_variable_name_seed = 0;
#if !IA64_ABI
  avail_compressible_string_pos = NULL;
#if DEBUG
  num_compressible_string_pos_allocated = 0;
#endif /* DEBUG */
#else /* IA64_ABI */
  avail_substitutions = NULL;
#endif /* IA64_ABI */
}  /* name_lower_init */

#endif /* NEED_NAME_MANGLING */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
