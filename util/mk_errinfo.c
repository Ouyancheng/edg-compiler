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

Utility program that generates error tables used by the compiler
from a text file.  Also generates a documentation file containing
the error text.

*/

#define COMPILING_MK_ERRINFO /* Used by host_envir.h. */
#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include "basics.h"
#include "host_envir.h"

#if __ANSIC__
/* Get bsearch and qsort definitions. */
#include <stdlib.h>
#else /* __ANSIC__ */
EXTERN_C a_void_ptr bsearch(a_const_void_ptr key,
                            a_const_void_ptr base,
                            sizeof_t         nmemb,
                            sizeof_t         size,
                            int(*compar)(a_const_void_ptr,
                                         a_const_void_ptr));

EXTERN_C a_void_ptr qsort(a_void_ptr       base,
                          sizeof_t         nmemb,
                          sizeof_t         size,
                          int(*compar)(a_const_void_ptr,
                                       a_const_void_ptr));
#endif /* __ANSIC__ */


/*
Lines from "nm" are read into this buffer for analysis.
*/
#define ME_INPUT_LINE_SIZE 32767
typedef char		a_me_input_line[ME_INPUT_LINE_SIZE];
static a_me_input_line	me_input_line;

/*
Maximum number of errors that can be processed.
*/
#define MAX_ERRORS 2000

/*
Maximum number of error tags that can be used.
*/
#define MAX_TAGS 10000

/* String that is used as the prefix of all diagnostic messages generated. */
static char *message_prefix = "mk_errinfo";



void me_internal_error(char*   error_string)
/*
Prints an internal error message and exits with a catastrophic error
exit status.
*/
{
  fprintf(stderr, "%s: %s\n", message_prefix, error_string);
  exit (RC_CATASTROPHE);
}  /* me_internal_error */


static void me_error(char		*error_text,
                     char		*insertion_string)
/*
Prints an error message and exits with an error exit status.  A string
may be inserted into the message by passing a pointer to the string
to be inserted in inseration_string.  This will only be used if
the error text contains a corresponding %s.  If the message contains such
a %s, insertion_string must not be NULL.
*/
{
  fprintf(stderr, "%s: ", message_prefix);
  fprintf(stderr, error_text, insertion_string);
  fprintf(stderr, "\n");
  exit (RC_ERROR);
}  /* me_error */


static a_void_ptr me_malloc_with_check(sizeof_t size)
/*
Interface to malloc that allocates "size" bytes.  Checks for failure of 
allocation and generates a catastrophic error.
*/
{
  a_void_ptr ptr;

  if ((ptr = (a_void_ptr)malloc(size)) == NULL) {
    me_error("out of memory", (char *)NULL);
  } /* if */
  return (ptr);
}  /* me_malloc_with_check */


a_boolean me_read_input_line(FILE* input_file)
/*
Reads a line of input from input_file.  Returns TRUE if a line of
input is being returned.  Returns FALSE at end-of-file.  Sets "line_size"
to the number of characters read not including the trailing null character.
*/
{
  register char*    buffer_pos = &me_input_line[0];
  register int      size = 0;
  register int      ch;
  a_boolean         result;

  while ((ch = getc(input_file)), ch != EOF && ch != '\n') {
    if (++size > ME_INPUT_LINE_SIZE) {
      me_internal_error("me_read_input_line: input line too long.");
    }  /* if */
    *buffer_pos++ = ch;
  }  /* while */
  
  /* Terminate string with a null character. */
  *buffer_pos++ = '\0';

  /* Determine whether to return end-of-file (FALSE). */
  result = TRUE;
  if (ch == EOF && size == 0) result = FALSE;

  return (result);
}  /* me_read_input_line */


static char *me_copy_string(char *source)
/*
Allocate space for a copy of the string and make a copy.  Return a pointer
to the copy.
*/
{
  char	*dest;
  dest = (char *)me_malloc_with_check(strlen(source) + 1);
  strcpy(dest, source);
  return dest;
}  /* me_copy_string */


static void me_invalid_input(void)
{
  me_error("invalid input line: %s", me_input_line);
}  /* me_invalid_input */


static void me_command_line_error(void)
{
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s %s\n",
          "mk_errinfo message_input_file_name tag_input_file_name",
          "codes_output_file data_output_file");
  fprintf(stderr, "  %s %s\n",
          "mk_errinfo -d message_input_file_name tag_input_file_name",
          "documentation_output_file");
  me_error("command line error", (char *)NULL);
}  /* me_command_line_error */

static char *header_comments[] =
{
  "/*",
  "",
  "DO NOT UPDATE THIS FILE!",
  "",
  "This file is generated by the mk_errinfo program from the information",
  "specified in the error_msg.txt and error_tag.txt files.",
  "",
  "*/",
  "",
  (char *)0
};


static void me_write_file_header(FILE* file)
/*
Output the comments that appear at the top of the generated files.
*/
{
  int	i;
  for (i = 0; header_comments[i] != NULL; ++i) {
    fprintf(file, "%s\n", header_comments[i]);
  }  /* for */
}  /* me_write_file_header */


/*
Structure used to record information about an error message.
*/
typedef struct an_error_info *an_error_info_ptr;
typedef struct an_error_info {
  char	*text;
  char	*enumerator;
  char	*tag;
} an_error_info;


static int compare_error_info(a_const_void_ptr arg1,
                              a_const_void_ptr arg2)
/*
Function called by qsort to compare two error_info records based on
the enumeration name.
*/
{
  an_error_info_ptr	eip1;
  an_error_info_ptr	eip2;

  eip1 = (an_error_info_ptr)arg1;
  eip2 = (an_error_info_ptr)arg2;
  return strcmp(eip1->enumerator, eip2->enumerator);
}  /* compare_error_info */


/*
Structure used to record information about an error message.
*/
typedef struct a_tag_info *a_tag_info_ptr;
typedef struct a_tag_info {
  char	*enumerator;
  char	*tag;
} a_tag_info;


static int compare_tag_info(a_const_void_ptr arg1,
                            a_const_void_ptr arg2)
/*
Function called by qsort to compare two tag_info records based on
the tag.
*/
{
  a_tag_info_ptr	eip1;
  a_tag_info_ptr	eip2;

  eip1 = (a_tag_info_ptr)arg1;
  eip2 = (a_tag_info_ptr)arg2;
  return strcmp(eip1->tag, eip2->tag);
}  /* compare_tag_info */


static an_error_info
		error_info[MAX_ERRORS];
static a_tag_info
		tag_info[MAX_TAGS];

#define skip_blanks(p) {while (*p == ' ') p++;}

static char		*message_input_file_name;
static FILE		*message_input_file;
static char		*tag_input_file_name;
static FILE		*tag_input_file;
static char		*codes_output_file_name;
static FILE		*codes_output_file;
static char		*data_output_file_name;
static FILE		*data_output_file;
static char		*doc_output_file_name;
static FILE		*doc_output_file;
static int		number_of_errors = 0;
static int		number_of_tags = 0;
static int		removed_count = 0;


static void me_read_input_file(void)
/*
Read the message input file and build the error_info array.
*/
{
  while (me_read_input_line(message_input_file)) {
    /* Read lines of the form:

		error_enumerator;error_tag;error_text
    */
    char	*ptr;
    char	*enumerator_start;
    char	*tag_start;
    char	*text_start;
    char	*copy_of_enumerator;
    char	*copy_of_tag;
    /* Find the end of the enumerator. */
    ptr = me_input_line;
    skip_blanks(ptr);
    /* A line that begins with a "#" is a comment.  Blank lines are
       ignored. */
    if (*ptr == '#' || *ptr == '\0') continue; 
    enumerator_start = ptr;
    ptr = strchr(enumerator_start, ';');
    if (ptr == NULL) me_invalid_input();
    *ptr++ = '\0';
    skip_blanks(ptr);
    tag_start = ptr;
    /* Find the end of the tag. */
    ptr = strchr(ptr, ';');
    if (ptr == NULL) me_invalid_input();
    *ptr++ = '\0';
    skip_blanks(ptr);
    if (strcmp(enumerator_start, "REMOVED") == 0) {
       /* A line that begins with "REMOVED" indicates that this error
          code is no longer in use, but the sequence number must be
          reserved to preserve the sequence numbers of the error codes
          that follow. */
       sprintf(me_input_line, "ec_removed_%0d", ++removed_count);
       error_info[number_of_errors].enumerator = me_copy_string(me_input_line);
       error_info[number_of_errors].text = (char *)NULL;
       number_of_errors++;
       continue;
    }  /* if */
    /* Make sure the string begins with a quote. */
    if (*ptr != '"') me_invalid_input();
    /* Note that text_start points to the opening quote. */
    text_start = ptr++;
    /* Find the end of the text string. */
    for (;;) {
      char	ch = *ptr;
      if (ch == '"' || ch == '\0') break;
      /* Skip the character following an escape. */
      if (ch == '\\') ptr++;
      ptr++;
    }  /* for */
    /* Make sure the line string was terminated by a closing quote. */
    if (*ptr != '"') me_invalid_input();
    /* If there are any characters after the quote, they must be blanks. */
    ptr++;
    if (*ptr != '\0') {
      char	*after_quote = ptr;
      skip_blanks(ptr);
      if (*ptr != '\0') me_invalid_input();
      /* Replace the first blank with a null terminator */
      *after_quote = '\0';
    }  /* if */
    error_info[number_of_errors].text = me_copy_string(text_start);
    copy_of_enumerator =  me_copy_string(enumerator_start);
    error_info[number_of_errors].enumerator = copy_of_enumerator;
    /* If there is no tag, use the enumerator as the tag. Skip past
       the ec_ prefix, though. */
    if (*tag_start == '\0') tag_start = me_input_line+3;
    if (strcmp(tag_start, "INTERNAL") == 0) {
      /* Don't create a tag_info entry for this message. */
      error_info[number_of_errors].tag = (char *)NULL;
    } else {
      copy_of_tag = me_copy_string(tag_start);
      error_info[number_of_errors].tag = copy_of_tag;
      tag_info[number_of_tags].enumerator = copy_of_enumerator;
      tag_info[number_of_tags].tag = copy_of_tag;
      number_of_tags++;
    }  /* if */
    number_of_errors++;
  }  /* while */
  fclose(message_input_file);
  /* Add a dummy "last" error code. */
  error_info[number_of_errors].enumerator = "ec_last";
  error_info[number_of_errors].text = (char *)NULL;
  number_of_errors++;
}  /* me_read_input_file */


static void me_read_tag_file(void)
/*
Process the tag file.  The tag file contains line of the form

	enumeration;tag

The enumeration is looked up in the error_info array (that is now
sorted by the enumeration string), and an entry is added to the
tag_info array. Note that there may be any number of tag
entries that refer to the same enumeration entry.
*/
{
  while (me_read_input_line(tag_input_file)) {
    char		*ptr;
    char		*tag_start;
    char		*enumerator_start;
    an_error_info	error_info_to_find;
    ptr = me_input_line;
    skip_blanks(ptr);
    /* A line that begins with a "#" is a comment.  Blank lines are
       ignored. */
    if (*ptr == '#' || *ptr == '\0') continue;
    enumerator_start = ptr;
    /* Find the end of the enumerator. */
    ptr = strchr(enumerator_start, ';');
    if (ptr == NULL) me_invalid_input();
    *ptr++ = '\0';
    skip_blanks(ptr);
    tag_start = ptr;
    /* Look up the enumeration code in the error_info table. */
    error_info_to_find.enumerator = enumerator_start;
    if (!bsearch((a_const_void_ptr)&error_info_to_find,
                 (a_const_void_ptr)error_info,
                 (sizeof_t)number_of_errors, sizeof(an_error_info),
                 compare_error_info)) {
      me_error("%s is not a valid error code", enumerator_start);
    }  /* if */
    tag_info[number_of_tags].enumerator = me_copy_string(enumerator_start);
    tag_info[number_of_tags].tag = me_copy_string(tag_start);
    number_of_tags++;
  }  /* while */
  fclose(tag_input_file);
}  /* me_read_tag_file */


static void me_write_error_codes(void)
/*
Generate the file containing the error code enumeration.
*/
{
  int	i;

  fprintf(codes_output_file, "typedef enum /*an_error_code*/ {\n");
  for (i = 0; i < number_of_errors; ++i) {
    /* If this is not the first time through, terminate the previous line. */
    if (i != 0) fprintf(codes_output_file, ",\n");
    fprintf(codes_output_file, "  %s /* = %0d */",
            error_info[i].enumerator, i);
  }  /* for */
  fprintf(codes_output_file, "\n} an_error_code;\n\n");
}  /* me_write_error_codes */

typedef enum /*a_font_kind*/ {
  fk_none,
  fk_normal,
  fk_tt,
  fk_em
} a_font_kind;


static a_font_kind curr_font;
			/* The font currently used for characters written
			   to the documentation file. */

a_boolean any_em_chars;


static void me_output_doc_string(char	     *string,
                                 int	     length,
				 a_font_kind font)
/*
Output characters that are part of the error text.  Make sure that
certain characters are put in the right font, when needed.
If the length specified is zero, the string is null terminated and strlen
should be used to determine the length.
*/
{
  int	i;

  if (curr_font != font) {
    char	*start_string;
    /* We need to switch fonts.  Terminate the previous font. */
    if (curr_font == fk_normal) {
      /* No action needed to terminate normal font. */
    } else if (curr_font == fk_em) {
      /* Terminate em font with "\/}". */
      fprintf(doc_output_file, "%s", "\\/}");
    } else {
      /* Terminate other fonts with a "}". */
      fprintf(doc_output_file, "%s", "}");
    }  /* if */
    /* Begin the new font. */
    any_em_chars = FALSE;
    switch (font) {
      case fk_normal: start_string = ""; break;
      case fk_tt: start_string = "{\\tt "; break;
      case fk_em: start_string = "{\\em "; break;
    }  /* switch */
    fprintf(doc_output_file, "%s", start_string);
    curr_font = font;
  }  /* if */
  if (length == 0) length = strlen(string);
  for (i = 0; i < length; ++i) {
    char	ch = string[i];
    /* Check for characters that must be escaped. */
    if (strchr("_#&${}%", ch) != NULL) putc('\\', doc_output_file);
    if (curr_font != fk_tt && strchr("\"<>", ch) != NULL) {
      /* Check for characters that can't be displayed in the normal
         font. */
      if (curr_font == fk_em && any_em_chars) fprintf(doc_output_file, "\\/");
      fprintf(doc_output_file, "{\\tt %c}", ch);
    } else {
      /* Just a normal character. */
      putc(ch, doc_output_file);
    }  /* if */
    if (curr_font == fk_em) any_em_chars = TRUE;
  }  /* if */
}  /* me_output_doc_string */


static void me_create_doc_fillin(char	**ptr_to_ptr)
/*
*/
{
  char		*ptr = *ptr_to_ptr;
  char		*orig_ptr = ptr;
  char		ch;
  char    	*fill_in = NULL;
  char		fill_in_specifier[100];
  char		*fis_ptr;
  char		*fill_in_override = NULL;

  /* Scan the characters that make up the fill-in specifier. */
  fis_ptr = fill_in_specifier;
  /* Always copy the first character, then any alphanumeric characters that
     follow. */
  *fis_ptr++ = *ptr++;
  while (isalnum(*ptr)) *fis_ptr++ = *ptr++;
  *fis_ptr = '\0';
  /* Check for a fill-in override.  This is specified in the source
     using notation like

	This uses a %s\='fill-in' override

     Make a pointer to a null-terminated fill in string. */
  if (*ptr == '\\' && ptr[1] == '=' && ptr[2] == '\'') {
    /* Skip past the \='. */
    ptr += 3;
    fill_in_override = ptr;
    while (*ptr != '\'' && *ptr != '\0') ptr++;
    /* Replace the ending quote with a null. */
    *ptr = '\0';
    ptr++;
  }  /* if */
  if (fill_in_override != NULL) {
    me_output_doc_string(fill_in_override, 0, fk_normal);
  } else {
    fis_ptr = fill_in_specifier;
    ch = *fis_ptr++;
    switch (ch) {
      case 's':
        if (*fis_ptr == 'q') {
          fill_in = "\"xxxx\"";
          fis_ptr++;
        } else {
          fill_in = "xxxx";
        }  /* if */
        me_output_doc_string(fill_in, 0, fk_em);
        break;
      case 't':
        me_output_doc_string("\"type\"", 0, fk_em);
        break;
      case 'p':
        fill_in = "at line {\\em xxxx\\/}";
        me_output_doc_string("at line ", 0, fk_normal);
        me_output_doc_string("xxxx", 0, fk_em);
        break;
      case '%':
        me_output_doc_string("%", 0, fk_normal);
        break;
      case 'n':
        {
          a_boolean	name_only = FALSE;
          a_boolean	template_args = FALSE;
          a_boolean	decl_pos = FALSE;
          while (isalnum(*fis_ptr)) {
            ch = *fis_ptr++;
            switch (ch) {
              case 'f': break;
              case 'o': name_only = TRUE; break;
              case 'a': template_args = TRUE; break;
              case 'd': decl_pos = TRUE; break;
              case '1': break;
              case '2': break;
              default: me_error("unexpected symbol fill-in %s\n", fis_ptr);
            }  /* switch */
          }  /* while */
          if (!name_only) me_output_doc_string("entity-kind ", 0, fk_em);
          me_output_doc_string("\"entity\"", 0, fk_em);
          if (template_args) me_output_doc_string("<args>", 0, fk_em);
          if (decl_pos) {
            me_output_doc_string(" (declared at line ", 0, fk_normal);
            me_output_doc_string("xxxx", 0, fk_em);
            me_output_doc_string(")", 0, fk_normal);
          }  /* if */
        }
        break;
      default:
        me_error("unexpected message fill-in: %s", orig_ptr);
    }  /* switch */
  }  /* if */
  *ptr_to_ptr = ptr;
}  /* me_create_doc_fillin */


static void me_write_error_text(void)
/*
Write the error text array to the error data file.
*/
{
  int	i;

  fprintf(data_output_file,
          "static char *message_text[(int)ec_last + 1] = {\n");
  for (i = 0; i < number_of_errors; ++i) {
    char	*ptr;
    /* If this is not the first time through, terminate the previous line. */
    if (i != 0) fprintf(data_output_file, ",\n");
    fprintf(data_output_file, "  /* %s */\n", error_info[i].enumerator);
    putc(' ', data_output_file);
    putc(' ', data_output_file);
    ptr = error_info[i].text;
    if (ptr == NULL) {
      /* There is no error text.  This is used for REMOVED errors. */
      fprintf(data_output_file, "(char *)NULL");
    } else {
      for (; *ptr != '\0'; ++ptr) {
        char ch = *ptr;
        if (ch == '\\' && ptr[1] == '=') {
          /* A fill-in override string of the form \='xxx'.  Skip past this
             string.  First skip past the opening quote. */
          ptr += 3;
          while (*ptr != '\'' && *ptr != '\0') ptr++;
          continue;
        }  /* if */
        putc(ch, data_output_file);
      }  /* for */
    }  /* if */
  }  /* for */
  fprintf(data_output_file, "\n};\n");
}  /* me_write_error_text */


static void me_write_tag_table(void)
/*
Write the tag lookup table to the error data file.
*/
{
  int	i;

  fprintf(data_output_file, "#define NUMBER_OF_ERROR_TAGS %0d\n",
          number_of_tags);
  /* Generate the sorted list of tags and associated enumerators. */
  fprintf(data_output_file,
          "static an_error_tag_entry error_tags[NUMBER_OF_ERROR_TAGS] = {\n");
  for (i = 0; i < number_of_tags; ++i) {
    /* If this is not the first time through, terminate the previous line. */
    if (i != 0) fprintf(data_output_file, ",\n");
    fprintf(data_output_file, "  \"%s\", %s", tag_info[i].tag,
            tag_info[i].enumerator);
  }  /* for */
  fprintf(data_output_file, "\n};\n");
}  /* me_write_tag_table */


static void me_write_doc_file(void)
/*
Generate a TeX file that documents the error messages
*/
{
  int	i;

  /* Start with position 1 to skip over ec_no_error. */
  for (i = 1; i < number_of_errors; ++i) {
    char	*ptr;
    /* Skip any removed errors. */
    if (error_info[i].text == NULL) continue;
    /* Skip INTERNAL messages that have no tags. */
    if (error_info[i].tag == NULL) continue;
    /* Reset the current font kind. */
    curr_font = fk_normal;
    /* Write the second item command containing the number. */
    fprintf(doc_output_file, "\\item[\\tt %04d ", i);
    /* Write the tag name. */
    ptr = error_info[i].tag;
    while (*ptr != '\0') {
      if (*ptr == '_') putc('\\', doc_output_file);
      putc(*ptr, doc_output_file);
      ptr++;
    }  /* while */
    /* Close the tag line. */
    fprintf(doc_output_file, ":]\n");
    /* Write an empty item command. */
    fprintf(doc_output_file, "\\item[]\n");
    /* Put out a \parskip 0pt and \itemsep 0pt. */
    fprintf(doc_output_file, "\\parskip 0pt\n\\itemsep 0pt\n");
    /* Write the error text. */
    /* Skip the opening quote. */
    ptr = error_info[i].text;
    ptr++;
    for (;;) {
      char	ch = *ptr;
      if (ch == '%') {
        ptr++;
        me_create_doc_fillin(&ptr);
      } else {
        /* Exit the loop when we find an unescaped quote. */
        if (ch == '"') break;
        if (ch == '\\') ptr++;
        ch = *ptr;
        /* Just a normal character. */
        me_output_doc_string(&ch, 1, fk_normal);
        ptr++;
      }  /* if */
    }  /* for */
    me_output_doc_string("\n", 0, fk_normal);
  }  /* for */
}  /* me_write_doc_file */


int main(int argc, char *argv[])
{
  int		argpos = 1;
  a_boolean	doc_mode = FALSE;

  if (argc != 5) me_command_line_error();
  if (strcmp(argv[argpos], "-d") == 0) {
    /* We should generate a documentation output file instead of the normal
       code files. */
    doc_mode = TRUE;
    argpos++;
  }  /* if */
  message_input_file_name = argv[argpos++];
  tag_input_file_name = argv[argpos++];
  if (doc_mode) {
    doc_output_file_name = argv[argpos++];
  } else {
    codes_output_file_name = argv[argpos++];
    data_output_file_name = argv[argpos++];
  }  /* if */
  /* Open the message input file. */
  message_input_file = fopen(message_input_file_name, "r");
  if (message_input_file == NULL) {
   me_error("cannot open %s", message_input_file_name);
  }  /* if */
  if (doc_mode) {
    /* Open the documentation output file. */
    doc_output_file = fopen(doc_output_file_name, "w");
    if (doc_output_file == NULL) {
      me_error("cannot open %s", doc_output_file_name);
    }  /* if */
  } else {
    /* Open the tag input file. */
    tag_input_file = fopen(tag_input_file_name, "r");
    if (tag_input_file == NULL) {
      me_error("cannot open %s", tag_input_file_name);
    }  /* if */
    codes_output_file = fopen(codes_output_file_name, "w");
    if (codes_output_file == NULL) {
      me_error("cannot open %s", codes_output_file_name);
    }  /* if */
    data_output_file = fopen(data_output_file_name, "w");
    if (codes_output_file == NULL) {
      me_error("cannot open %s", data_output_file_name);
    }  /* if */
    /* Generate the output file headers. */
    me_write_file_header(codes_output_file);
    me_write_file_header(data_output_file);
  }  /* if */
  /* Read the input file. */
  me_read_input_file();
  if (doc_mode) {
    /* Generate the documentation output file. */
    me_write_doc_file();
    fclose(doc_output_file);
  } else {
    /* Generate the output file.  Start with the error code enumeration. */
    me_write_error_codes();
    /* Generate the error text array. */
    me_write_error_text();
    /* Sort the error information by enumeration code so that the enumerations
       can be looked up while processing the tag file. */
    qsort((a_void_ptr)error_info, (sizeof_t)number_of_errors,
           sizeof(an_error_info), compare_error_info);
    /* Read the data from the tag file. */
    me_read_tag_file();
    /* Sort the tag information by tag. */
    qsort((a_void_ptr)tag_info, (sizeof_t)number_of_tags,
          sizeof(a_tag_info), compare_tag_info);
    /* Output the number of tags to the error code file. */
    me_write_tag_table();
    fclose(codes_output_file);
    fclose(data_output_file);
  }  /* if */
  return (0);
}  /* main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
