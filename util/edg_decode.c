/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*
edg_decode -- Name demangler for C++.

Reads input from stdin, writes output to stdout.  Things that look like
mangled names in the input are demangled.  Everything else is passed
through unchanged.
*/
#include "basics.h"
#include "host_envir.h"

/*
Return TRUE if the given character is one that can start an identifier.
The set includes "$" as an extension.  This is a macro, and it evaluates
its argument more than once.
*/
#define is_id_start_char(ch)                                          \
  (ch != EOF &&                                                       \
   (isalpha((unsigned char)ch) || ch == '_' || ch == '$'))

/*
Return TRUE if the given character is one that can be part of an identifier
after the first character.  This is a macro, and it evaluates its argument
more than once.
*/
#define is_id_following_char(ch)                                      \
  (ch != EOF && (is_id_start_char(ch) || isdigit(ch)))


/*
Maximum size of an identifier.
*/
#define MAX_ID_LENGTH 2000
static char	curr_id[MAX_ID_LENGTH];
			/* Identifier being processed currently. */
static unsigned long
		curr_id_len = 0;
			/* Current identifier size. */
static int	ch;
			/* Current input character. */
static a_boolean
		err = FALSE;
			/* TRUE if any error was encountered. */
static a_boolean
		err_in_id;
			/* TRUE if any error was encountered in the current
			   identifier. */
static unsigned long
		suppress_id_output = 0;
			/* If > 0, unmangled id output is suppressed. */


/*
Declarations needed because of forward references:
*/
static char *demangle_type(char *ptr);


static void write_id_ch(char ch)
/*
Write out the indicated character, which is part of the demangled version
of an identifier.
*/
{
  if (!suppress_id_output) fputc(ch, stdout);
}  /* write_id_ch */


static void write_id_str(char *str)
/*
Write out the indicated string, which is part of the demangled version
of an identifier.
*/
{
  if (!suppress_id_output) fputs(str, stdout);
}  /* write_id_str */


static void bad_mangled_name(void)
/*
A bad name mangling has been encountered.  Record an error.
*/
{
  int i;

  if (!err_in_id) {
    err = err_in_id = TRUE;
    suppress_id_output++;
    fputs("[incorrect mangled name:]", stdout);
    /* Put out the original identifier. */
    for (i = 0; i < curr_id_len; i++) putchar(curr_id[i]);
  }  /* if */
}  /* bad_mangled_name */


static a_boolean start_of_id_is(char *str, char *id)
/*
Return TRUE if the identifier (at id) begins with the string str.
*/
{
  a_boolean is_start = FALSE;

  for (;;) {
    char chs = *str++;
    if (chs == '\0') {
      is_start = TRUE;
      break;
    }  /* if */
    if (chs != *id++) break;
  }  /* for */
  return is_start;
}  /* start_of_id_is */


static char *get_number(char          *p,
                        unsigned long *num)
/*
Accumulate a number starting at position p and return its value in *num.
Return a pointer to the character position following the number.
*/
{
  unsigned long n = 0;

  if (!isdigit(*p)) {
    bad_mangled_name();
    goto end_of_routine;
  }  /* if */
  do {
    n = n*10 + (*p - '0');
    if (n > curr_id_len) {
      /* Bad number. */
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    p++;
  } while (isdigit(*p));
end_of_routine:
  *num = n;
  return p;
}  /* get_number */


static char *get_single_digit_number(char          *p,
                                     unsigned long *num)
/*
Accumulate a number starting at position p and return its value in *num.
The number is a single digit.  Return a pointer to the character position
following the number.
*/
{
  *num = 0;
  if (!isdigit(*p)) {
    bad_mangled_name();
    goto end_of_routine;
  }  /* if */
  *num = (*p - '0');
  p++;
end_of_routine:
  return p;
}  /* get_single_digit_number */


static a_boolean is_immediate_type_qualifier(char *p)
/*
Return TRUE if the encoding pointed to is one that indicates type
qualification.
*/
{
  a_boolean is_type_qual = FALSE;

  if (*p == 'C' || *p == 'V') {
    /* This is a type qualifier. */
    is_type_qual = TRUE;
  }  /* if */
  return is_type_qual;
}  /* is_immediate_type_qualifier */


static char *advance_past_underscore(char *p)
/*
An underscore is expected at *p.  If it's there, advance past it.  If
not, call bad_mangled_name.  In either case, return the updated value of p.
*/
{
  if (*p == '_') {
    p++;
  } else {
    bad_mangled_name();
  }  /* if */
  return p;
}  /* advance_past_underscore */


static char *demangle_type_name(char      *ptr,
                                a_boolean base_name_only)
/*
Demangle the type name at ptr and output the demangled form.  Return a pointer
to the character position following what was demangled.  The name can be
a simple type name or a nested type name.  If base_name_only is TRUE,
do not put out any nested type qualifiers, e.g., put out "A::x" as simply "x".
*/
{
  char          *p = ptr;
  unsigned long digits, nquals;

  if (*p == 'Q') {
    /* A nested type name has the form
         Q2_5outer5inner   (outer::inner)
          ^ ^-----^--------Names from outermost to innermost
          -----------------Number of levels of qualification.
    */
    p = get_number(p+1, &nquals);
    if (nquals > curr_id_len) {
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    p = advance_past_underscore(p);
    /* Handle each level of qualification. */
    for (; nquals > 0; nquals--) {
      /* Do not put out the nested type qualifiers if base_name_only is
         TRUE. */
      if (base_name_only && nquals != 1) suppress_id_output++;
      p = demangle_type_name(p, /*base_name_only=*/FALSE);
      if (nquals != 1) write_id_str("::");
      if (base_name_only && nquals != 1) suppress_id_output--;
    }  /* for */
  } else {
    /* A mangled type name consists of digits indicating the length of the
       name followed by the name itself, e.g., "3abc". */
    /* Accumulate the count. */
    p = get_number(p, &digits);
    /* Process the characters of the type name. */
    for (; digits > 0; digits--, p++) {
      if (*p == '\0') {
        /* Ran off the end of the identifier. */
        bad_mangled_name();
        goto end_of_routine;
      }  /* if */
      write_id_ch(*p);
    }  /* for */
  }  /* if */
end_of_routine:
  return p;
}  /* demangle_type_name */


static char *demangle_type_qualifiers(char *ptr)
/*
Demangle any type qualifiers (const/volatile) at the indicated location.
Return a pointer to the character position following what was demangled.
*/
{
  char *p = ptr;

  for (;; p++) {
    if (*p == 'C') {
      write_id_str("const ");
    } else if (*p == 'V') {
      write_id_str("volatile ");
    } else {
      break;
    }  /* if */
  }  /* for */
  return p;
}  /* demangle_type_qualifiers */


static char *demangle_type_specifier(char *ptr)
/*
Demangle the type at ptr and output the specifier part.  Return a pointer
to the character position following what was demangled.
*/
{
  char *p = ptr, *s;

  /* Process type qualifiers. */
  p = demangle_type_qualifiers(p);
  if (isdigit(*p)) {
    /* Named type, like class or enum, e.g., "3abc". */
    p = demangle_type_name(p, /*base_name_only=*/FALSE);
  } else {
    /* Builtin type. */
    /* Handle signed and unsigned. */
    if (*p == 'S') {
      write_id_str("signed ");
      p++;
    } else if (*p == 'U') {
      write_id_str("unsigned ");
      p++;
    }  /* if */
    switch (*p++) {
      case 'v':
        s = "void";
        break;
      case 'c':
        s = "char";
        break;
      case 's':
        s = "short";
        break;
      case 'i':
        s = "int";
        break;
      case 'l':
        if (*p != 'l') {
          s = "long";
        } else {
          s = "long long";
          p++;
        }  /* if */
        break;
      case 'f':
        s = "float";
        break;
      case 'd':
        s = "double";
        break;
      case 'r':
        s = "long double";
        break;
      default:
        bad_mangled_name();
        s = "";
    }  /* switch */
    write_id_str(s);
  }  /* if */
  return p;
}  /* demangle_type_specifier */


static char *demangle_function_parameters(char *ptr)
/*
Demangle the parameter list beginning at ptr and output the demangled form.
Return a pointer to the character position following what was demangled.
*/
{
  char     *p = ptr;
  char     *param_pos[10];
  unsigned long i, num, nreps;

  write_id_ch('(');
  if (*p == 'v') {
    /* Void parameter list. */
    p++;
  } else {
    /* Loop for each parameter. */
    for (i = 1;; i++) {
      if (i < 10) param_pos[i] = NULL;
      if (*p == 'T' || *p == 'N') {
        /* Tn means repeat the type of parameter "n". */
        /* Nmn means "m" repetitions of the type of parameter "n".  "m"
           is a one-digit number. */
        /* "n" is also treated as a single-digit number; the front end enforces
           that.  cfront does not, which leads to some ambiguities. */
        if (*p++ == 'N') {
          /* Get the number of repetitions. */
          p = get_single_digit_number(p, &nreps);
        } else {
          nreps = 1;
        }  /* if */
        /* Get the parameter number. */
        p = get_single_digit_number(p, &num);
        if (num < 1 || num >= i || param_pos[num] == NULL) {
          /* Parameter number out of range. */
          bad_mangled_name();
          goto end_of_routine;
        }  /* if */
        /* Produce "nreps" copies of parameter "num". */
        for (; nreps > 0; nreps--) {
          (void)demangle_type(param_pos[num]);
          if (nreps != 1) write_id_str(", ");
        }  /* if */
      } else {
        /* A normal parameter. */
        if (i < 10) param_pos[i] = p;
        p = demangle_type(p);
      }  /* if */
      /* Stop after the last parameter. */
      if (*p == '\0' || *p == 'e' || *p == '_') break;
      write_id_str(", ");
    }  /* for */
  }  /* if */
  if (*p == 'e') {
    /* Ellipsis. */
    write_id_str("...");
    p++;
  }  /* if */
  write_id_ch(')');
end_of_routine:
  return p;
}  /* demangle_function_parameters */


static char *demangle_type_first_part(char       *ptr,
                                      a_boolean  need_paren,
                                      a_boolean  need_trailing_space)
/*
Demangle the type at ptr and output the specifier part and the part of the
declarator that precedes the name.  Return a pointer to the character
position following what was demangled.  If need_paren is TRUE, put a left
parenthesis at the end of the first half of the declarator.
If need_trailing_space is TRUE, put a space at the end of the specifiers
part (needed if the declarator part is not empty, because it contains a
name or a derived type).
*/
{
  char *p = ptr, *qualp = p;
  char kind;

  /* Remove type qualifiers. */
  while (is_immediate_type_qualifier(p)) p++;
  kind = *p;
  if (kind == 'P' || kind == 'R') {
    /* Pointer or reference type, e.g., "Pc" is pointer to char. */
    p = demangle_type_first_part(p+1, /*need_paren=*/TRUE,
                                 /*need_trailing_space=*/TRUE);
    /* Output "*" or "&" for pointer or reference. */
    if (kind == 'R') {
      write_id_ch('&');
    } else {
      write_id_ch('*');
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    (void)demangle_type_qualifiers(qualp);
    if (need_paren) write_id_ch('(');
  } else if (kind == 'M') {
    /* Pointer-to-member type, e.g., "M1Ai" is pointer to member of A of
       type int. */
    char *classp = p+1;
    /* Skip over the class name. */
    suppress_id_output++;
    p = demangle_type_name(classp, /*base_name_only=*/FALSE);
    suppress_id_output--;
    p = demangle_type_first_part(p, /*need_paren=*/TRUE,
                                 /*need_trailing_space=*/TRUE);
    /* Output Classname::*. */
    (void)demangle_type_name(classp, /*base_name_only=*/FALSE);
    write_id_str("::*");
    /* Output the type qualifiers on the pointer, if any. */
    (void)demangle_type_qualifiers(qualp);
    if (need_paren) write_id_ch('(');
  } else if (kind == 'F') {
    /* Function type, e.g., "Fii_f" is function(int, int) returning float.
       The return type is not present for top-level function types. */
    /* Skip over the parameter types without outputting anything. */
    suppress_id_output++;
    p = demangle_function_parameters(p+1);
    suppress_id_output--;
    if (*p == '_') {
      /* The return type is present. */
      p = demangle_type_first_part(p+1, /*need_paren=*/TRUE,
                                   /*need_trailing_space=*/TRUE);
    }  /* if */
    if (need_paren) write_id_ch('(');
  } else if (kind == 'A') {
    /* Array type, e.g., "A10_i" is array[10] of int. */
    p++;
    /* Skip the array size. */
    while (isdigit(*p)) p++;
    p = advance_past_underscore(p);
    p = demangle_type_first_part(p, /*need_paren=*/TRUE,
                                 /*need_trailing_space=*/TRUE);
    if (need_paren) write_id_ch('(');
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    p = demangle_type_specifier(qualp);
    if (need_trailing_space) write_id_ch(' ');
  }  /* if */
  return p;
}  /* demangle_type_first_part */


static char *demangle_type_second_part(char       *ptr,
                                       a_boolean  need_paren)
/*
Demangle the type at ptr and output the part of the declarator that follows the
name.  Return a pointer to the character position following what was demangled.
If need_paren is TRUE, put a closing parenthesis out first if anything is
generated.
*/
{
  char *p = ptr, *qualp = p;
  char kind;

  /* Remove type qualifiers. */
  while (is_immediate_type_qualifier(p)) p++;
  kind = *p;
  if (kind == 'P' || kind == 'R') {
    /* Pointer or reference type, e.g., "Pc" is pointer to char. */
    if (need_paren) write_id_ch(')');
    p = demangle_type_second_part(p+1, /*need_paren=*/TRUE);
  } else if (kind == 'M') {
    /* Pointer-to-member type, e.g., "M1Ai" is pointer to member of A of
       type int. */
    if (need_paren) write_id_ch(')');
    /* Advance over the class name. */
    suppress_id_output++;
    p = demangle_type_name(p+1, /*base_name_only=*/FALSE);
    suppress_id_output--;
    p = demangle_type_second_part(p, /*need_paren=*/TRUE);
  } else if (kind == 'F') {
    /* Function type, e.g., "Fii_f" is function(int, int) returning float.
       The return type is not present for top-level function types. */
    if (need_paren) write_id_ch(')');
    /* Put out the parameter types. */
    p = demangle_function_parameters(p+1);
    /* Put out any cv-qualifiers (member functions). */
    /* Note that such things could come up on nonmember functions in the
       presence of typedefs.  In such a case what we generate here will not
       be valid C, but it's a reasonable representation of the mangled
       type, and there's no way of getting the typedef name in there,
       so let it be. */
    if (*qualp != 'F') {
      write_id_ch(' ');
      (void)demangle_type_qualifiers(qualp);
    }  /* if */
    if (*p == '_') {
      /* Process the return type. */
      p = demangle_type_second_part(p+1, /*need_paren=*/TRUE);
    }  /* if */
  } else if (kind == 'A') {
    /* Array type, e.g., "A10_i" is array[10] of int. */
    if (need_paren) write_id_ch(')');
    write_id_ch('[');
    p++;
    if (*p == '0' && p[1] == '_') {
      /* Size is zero, so do not put out a size (the result is "[]"). */
      p++;
    } else {
      /* Put out the array size. */
      while (isdigit(*p)) write_id_ch(*p++);
    }  /* if */
    p = advance_past_underscore(p);
    write_id_ch(']');
    /* Process the element type. */
    p = demangle_type_second_part(p, /*need_paren=*/TRUE);
  } else {
    /* No declarator part to process.  Skip the specifier type. */
    suppress_id_output++;
    p = demangle_type_specifier(qualp);
    suppress_id_output--;
  }  /* if */
  return p;
}  /* demangle_type_second_part */


static char *demangle_type(char *ptr)
/*
Demangle the type at ptr and output the demangled form.  Return a pointer to
the character position following what was demangled.
*/
{
  char *p;

  /* Generate the specifier part of the type. */
  (void)demangle_type_first_part(ptr, /*need_paren=*/FALSE,
                                 /*need_trailing_space=*/FALSE);
  /* Generate the declarator part of the type. */
  p = demangle_type_second_part(ptr, /*need_paren=*/FALSE);
  return p;
}  /* demangle_type */


static void demangle_operator_function_name(char *ptr)
/*
ptr points to an operator function name.  Demangle it, and output the
demangled form.
*/
{
  char *s;

  write_id_str("operator ");
  if (strcmp("nw", ptr) == 0) {
    s = "new";
  } else if (strcmp("dl", ptr) == 0) {
    s = "delete";
  } else if (strcmp("pl", ptr) == 0) {
    s = "+";
  } else if (strcmp("mi", ptr) == 0) {
    s = "-";
  } else if (strcmp("ml", ptr) == 0) {
    s = "*";
  } else if (strcmp("dv", ptr) == 0) {
    s = "/";
  } else if (strcmp("md", ptr) == 0) {
    s = "%";
  } else if (strcmp("er", ptr) == 0) {
    s = "^";
  } else if (strcmp("ad", ptr) == 0) {
    s = "&";
  } else if (strcmp("or", ptr) == 0) {
    s = "|";
  } else if (strcmp("co", ptr) == 0) {
    s = "~";
  } else if (strcmp("nt", ptr) == 0) {
    s = "!";
  } else if (strcmp("as", ptr) == 0) {
    s = "=";
  } else if (strcmp("lt", ptr) == 0) {
    s = "<";
  } else if (strcmp("gt", ptr) == 0) {
    s = ">";
  } else if (strcmp("apl", ptr) == 0) {
    s = "+=";
  } else if (strcmp("ami", ptr) == 0) {
    s = "-=";
  } else if (strcmp("amu", ptr) == 0) {
    s = "*=";
  } else if (strcmp("adv", ptr) == 0) {
    s = "/=";
  } else if (strcmp("amd", ptr) == 0) {
    s = "%=";
  } else if (strcmp("aer", ptr) == 0) {
    s = "^=";
  } else if (strcmp("aad", ptr) == 0) {
    s = "&=";
  } else if (strcmp("aor", ptr) == 0) {
    s = "|=";
  } else if (strcmp("ls", ptr) == 0) {
    s = "<<";
  } else if (strcmp("rs", ptr) == 0) {
    s = ">>";
  } else if (strcmp("ars", ptr) == 0) {
    s = ">>=";
  } else if (strcmp("als", ptr) == 0) {
    s = "<<=";
  } else if (strcmp("eq", ptr) == 0) {
    s = "==";
  } else if (strcmp("ne", ptr) == 0) {
    s = "!=";
  } else if (strcmp("le", ptr) == 0) {
    s = "<=";
  } else if (strcmp("ge", ptr) == 0) {
    s = ">=";
  } else if (strcmp("aa", ptr) == 0) {
    s = "&&";
  } else if (strcmp("oo", ptr) == 0) {
    s = "||";
  } else if (strcmp("pp", ptr) == 0) {
    s = "++";
  } else if (strcmp("mm", ptr) == 0) {
    s = "--";
  } else if (strcmp("cm", ptr) == 0) {
    s = ",";
  } else if (strcmp("rm", ptr) == 0) {
    s = "->*";
  } else if (strcmp("rf", ptr) == 0) {
    s = "->";
  } else if (strcmp("cl", ptr) == 0) {
    s = "()";
  } else if (strcmp("vc", ptr) == 0) {
    s = "[]";
  } else {
    bad_mangled_name();
    s = "";
  }  /* if */
  write_id_str(s);
}  /* demangle_operator_function_name */


static char *demangle_identifier(char *ptr)
/*
Demangle the identifier at ptr and output the demangled form.  Return
a pointer to the character position following what was demangled.
*/
{
  char      *p = ptr, *origname, *uscore, *mname, *end_ptr;
  a_boolean special_name = FALSE;

  if (p[0] == '_' && p[1] == '_') {
    /* The name starts with "__", which means it's a constructor, destructor,
       or operator function name, e.g., __pl__1AFf. */
    special_name = TRUE;
    origname = p+2;
    /* Start the search for "__" below after the initial "__". */
    p += 3;
  } else {
    /* Normal case (not an operator function). */
    origname = p;
  }  /* if */
  /* Find the first "__" in the identifier. */
  uscore = p;
  for (;;) {
    uscore = strchr(uscore, '_');
    if (uscore == NULL) {
      /* No "__" in the name. */
      /* One strange case -- an extern "C" operator function, like
           extern "C" void operator +(A, A);
         Such routines have nothing following the special name,
         i.e., "__pl" for the above. */
      if (special_name) {
        demangle_operator_function_name(origname);
        end_ptr = strchr(origname, '\0');
        goto end_of_routine;
      }  /* if */        
      /* No "__"; bad name. */
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    if (uscore[1] == '_') break;
    uscore += 2;
  }  /* for */
  /* End the origname at the underscore. */
  *uscore = '\0';
  mname = uscore + 2;
  /* Now origname points to the original-name part of the mangled name, and
     mname points to the mangled-name part at the end.
       f__1A
       ^  ^--mname
       ------origname
     The mangled-name part is
       (a)  A class name for a static data member.
       (b)  A class name followed by "F" followed by the encoding for the
            parameter types for a member function.
       (c)  "F" followed by the encoding for the parameter types for a
            nonmember function.
  */
#if 0
  /* Promoted entity names. */
#endif /* 0 */
  end_ptr = mname;
  if (mname[0] != 'F') {
    /* A class name must be next. */
    end_ptr = demangle_type_name(end_ptr, /*base_name_only=*/FALSE);
    write_id_str("::");
  }  /* if */
  /* If the name ends here, this is a static data member. */
  if (*end_ptr == '\0') {
    if (end_ptr == mname) {
      /* The mangled-name part was empty. */
      bad_mangled_name();
      goto end_of_routine;
    }  /* if */
    /* Write the static data member's name. */
    write_id_str(origname);
  } else {
    /* This must be a function. */
    /* "S" here means a static member function (ignore). */
    if (*end_ptr == 'S') end_ptr++;
    /* Process the specifier part of the type. */
    demangle_type_first_part(end_ptr, /*need_paren=*/FALSE,
                             /*need_trailing_space=*/TRUE);
    /* Write the name of the function. */
    if (special_name) {
      /* Process a special name. */
      if (strcmp(origname, "ct") == 0) {
        /* Constructor. */
        (void)demangle_type_name(mname, /*base_name_only=*/TRUE);
      } else if (strcmp(origname, "dt") == 0) {
        /* Destructor. */
        write_id_str("~");
        (void)demangle_type_name(mname, /*base_name_only=*/TRUE);
      } else if (start_of_id_is("op", origname)) {
        /* Conversion function.  Name looks like __opi__... where the part
           after "op" encodes the type (e.g., "opi" is "operator int"). */
        char *after_op_name;
        write_id_str("operator ");
        after_op_name = demangle_type(origname+2);
        if (*after_op_name != '\0') {
          /* Not all of the name was taken. */
          bad_mangled_name();
          goto end_of_routine;
        }  /* if */
      } else {
        /* Operator function. */
        demangle_operator_function_name(origname);
      }  /* if */
    } else {
      /* Not a special name. */
      write_id_str(origname);
    }  /* if */
    /* Write the declarator part of the type. */
    end_ptr = demangle_type_second_part(end_ptr, /*need_paren=*/FALSE);
  }  /* if */
end_of_routine:
  return end_ptr;
}  /* demangle_identifier */


static void demangle_whole_identifier(void)
/*
Demangle the current identifier and output the demangled form.
*/
{
  char *p;

  /* Check for special cases. */
  if (start_of_id_is("__vtbl__", curr_id)) {
    write_id_str("virtual function table for ");
    /* ??? */
  } else if (start_of_id_is("__CBI__", curr_id)) {
    write_id_str("can-be-instantiated flag for ");
    p = demangle_identifier(curr_id+7);
  } else if (start_of_id_is("__DNI__", curr_id)) {
    write_id_str("do-not-instantiate flag for ");
    p = demangle_identifier(curr_id+7);
  } else if (start_of_id_is("__TIR__", curr_id)) {
    write_id_str("template-instantiatiation-request flag for ");
    p = demangle_identifier(curr_id+7);
  } else if (start_of_id_is("__TID_", curr_id)) {
    write_id_str("type identifier for ");
    p = demangle_type(curr_id+6);
  } else if (start_of_id_is("__T_", curr_id)) {
    write_id_str("typeinfo for ");
    p = demangle_type(curr_id+4);
  } else {
    /* Normal case: function name, static data member name, or
       name of type or variable promoted out of function. */
    p = demangle_identifier(curr_id);
  }  /* if */
  /* Make sure the whole identifier was taken. */
  if (!err_in_id && *p != '\0') {
    bad_mangled_name();
  }  /* if */
}  /* demangle_whole_identifier */


static void process_identifier(void)
/*
The current input character is the beginning of an identifier.  Read the
identifier, process it, and output it.  On return, the current character
is the one following the identifier.
*/
{
  a_boolean any_mangling = FALSE, too_long_err = FALSE;
  a_boolean prev_was_underscore;
  int       i;

  err_in_id = FALSE;
  suppress_id_output = 0;
  /* Accumulate the identifier. */
  curr_id_len = 1;
  curr_id[0] = ch;
  for (;;) {
    prev_was_underscore = (ch == '_');
    /* Get another character, stop on a non-identifier character. */
    ch = getchar();
    if (!is_id_following_char(ch)) break;
    if (curr_id_len >= MAX_ID_LENGTH-1) {
      /* Identifier is too long. */
      if (!too_long_err) {
        /* First time through. */
        /* Dump the identifier (the part seen so far) in original form. */
        for (i = 0; i < curr_id_len; i++) putchar(curr_id[i]);
        too_long_err = TRUE;
      }  /* if */
      /* Keep going to the end of the identifier, passing through the rest
         of the characters. */
      putchar(ch);
    } else {
      /* Add the character to the current identifier. */
      curr_id[curr_id_len] = ch;
      /* Keep track of whether "__" appears in the name. */
      if (ch == '_' && prev_was_underscore) any_mangling = TRUE;
      curr_id_len++;
    }  /* if */
  }  /* for */
  /* The identifier has been accumulated. */
  if (too_long_err) {
    /* It was too long (it's already been copied unchanged to the output). */
    /* If it is a mangled name we haven't processed it properly. */
    if (any_mangling) bad_mangled_name();
  } else {
    /* The identifier was not too long. */
    curr_id[curr_id_len] = '\0';
    if (any_mangling) {
      /* It needs to be demangled. */
      demangle_whole_identifier();
    } else {
      /* Not a mangled name. */
      /* Dump the identifier in original form. */
      for (i = 0; i < curr_id_len; i++) putchar(curr_id[i]);
    }  /* if */
  }  /* if */
}  /* process_identifier */


/*ARGSUSED*/
int main(int argc, char *argv[]) {
  /* Read and echo characters until end of file.  When the start of an
     identifier is encountered, process it specially. */
  while ((ch = getchar()) != EOF) {
    /* Look for the start of an identifier. */
    if (is_id_start_char(ch)) {
      process_identifier();
    }  /* if */
    putchar(ch);
  }  /* while */
  return err ? RC_ERROR : 0;
}  /* main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
