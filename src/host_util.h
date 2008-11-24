/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

host_util.h -- host environment utility routines that are shared by
               the front end and other utility programs.

*/

unsigned long crc_32(char		*str,
		     unsigned long	prev_crc)
/*
Determines and returns the CRC-32 value for a null-terminated string.
This is the CRC used by ZMODEM and PKZIP.  There are plenty of more
efficient ways of computing CRC; this straightforward approach is used
only because in this context the CRC is needed just a small number of times.
prev_crc is a previously computed value.  This permits a CRC
to be computed by several calls to this routine.  If there is no previous
value, a zero should be passed in.
*/
{
  unsigned long crc;

  /* Start with an initial value of 0xfffffff, or undo the exclusive
     or done when the previous value was returned. */
  crc = prev_crc ^ 0xffffffff;
  while (*str != '\0') {
    unsigned long ch = (unsigned char)*str++;
    int nbit;

    for (nbit = 0; nbit < CHAR_BIT; nbit++, ch >>= 1) {
      int low_bit = (ch^crc) & 1;
      crc >>= 1;
      if (low_bit) crc ^= 0xEDB88320L;
    }  /* for */
  }  /* while */
  crc ^= 0xffffffff;
  return crc;
}  /* crc_32 */

#if ONE_INSTANTIATION_PER_OBJECT

char *generate_instantiation_output_file_name(char *mangled_name)
/*
Generate the name of an instantiation output file that is used in
one instantiation per object mode.  A pointer to a static buffer
is returned, so the value must be copied before this routine is
called again.
*/
{
#define MAX_INSTANTIATION_OUTPUT_FILE_LEN 31
  static char	buffer[MAX_INSTANTIATION_OUTPUT_FILE_LEN+1];
  int		max_len_without_suffix;

  /* Determine the output file name.  Use the mangled name (or the beginning
     of it) plus an underscore plus the hexadecimal for the CRC-32 checksum
     for the whole mangled name.  Note that the following computation uses
     the size of GEN_C_FILE_SUFFIX or OBJECT_FILE_SUFFIX (which includes the
     null terminator), not the strlen. */
  max_len_without_suffix = MAX_INSTANTIATION_OUTPUT_FILE_LEN - 8;
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
  max_len_without_suffix -= sizeof(GEN_C_FILE_SUFFIX);
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
  max_len_without_suffix -= sizeof(OBJECT_FILE_SUFFIX);
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
  check_assertion(max_len_without_suffix > 0);
  (void)strncpy(buffer, mangled_name, max_len_without_suffix);
  buffer[max_len_without_suffix] = '\0';
  (void)sprintf(buffer+strlen(buffer), "_%08lx",
                crc_32(mangled_name, (unsigned long)0));
#undef MAX_INSTANTIATION_OUTPUT_FILE_LEN
  return buffer;
}  /* generate_instantiation_output_file_name */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
wchar_t *translate_filename_to_wchar(char *filename)
/*
Copy the supplied filename to a buffer as wchar_t characters, translating
any UTF-8 multibyte characters to UTF-16, and return the address of the
buffer.  If the filename contains only ASCII characters, the returned
address will be NULL, indicating that the filename needs no translation and
can be used directly.  The buffer is reused by each successive call, so the
caller should copy the contents as needed.
*/
{
  unsigned char   *p;
  unsigned long   unicode_char;
  a_boolean       err;
  int             num_utf8_bytes;
  int             num_utf16_chars;
  sizeof_t        utf16_len;
  unsigned short  utf16_chars[2];
  int             i;
  static wchar_t  *buffer;
  a_boolean       utf8_character_seen = FALSE;
  static sizeof_t buffer_allocation_size = 512 * sizeof(wchar_t);

/* Macro to add one wide character to the buffer, expanding the buffer as
   needed. */
#if !defined(MEM_MANAGE_H) || STANDALONE_UTILITY_PROGRAM
/* The memory management environment upon which the text buffer utility
   relies is not available in a standalone utility program, so we must
   provide the facility locally. */
#define add_to_wchar_buffer(wchar)                                \
  if ((++utf16_len) * sizeof(wchar_t) > buffer_allocation_size) { \
    buffer_allocation_size *= 2;                                  \
    buffer = (wchar_t *)realloc((a_stdio_arg)buffer,              \
                                buffer_allocation_size);          \
    if (buffer == NULL) {                                         \
      fprintf(stderr, "Out of memory.\n");                        \
      exit(RC_ERROR);                                             \
    }  /* if */                                                   \
  }  /* if */                                                     \
  buffer[utf16_len-1] = wchar;
#else /* !(!defined(MEM_MANAGE_H) || STANDALONE_UTILITY_PROGRAM) */
#define add_to_wchar_buffer(wchar)                                  \
  ensure_text_buffer_space(wchar_filename_buffer,                   \
                           (++utf16_len) * sizeof(wchar_t));        \
  /* Update the buffer pointer to reflect possible reallocation. */ \
  buffer = (wchar_t *)wchar_filename_buffer->buffer;                \
  buffer[utf16_len-1] = wchar;
#endif /* !defined(MEM_MANAGE_H) || STANDALONE_UTILITY_PROGRAM */

#if !defined(MEM_MANAGE_H) || STANDALONE_UTILITY_PROGRAM
  if (buffer == NULL) {
    buffer = (wchar_t *)malloc(buffer_allocation_size);
    if (buffer == NULL) {
      fprintf(stderr, "Out of memory.\n");
      exit(RC_ERROR);
    }  /* if */
  }  /* if */
#else /* !(!defined(MEM_MANAGE_H) || STANDALONE_UTILITY_PROGRAM) */
  if (wchar_filename_buffer == NULL) {
    wchar_filename_buffer = alloc_text_buffer(buffer_allocation_size);
    buffer = (wchar_t *)wchar_filename_buffer->buffer;
  }  /* if */
#endif /* !defined(MEM_MANAGE_H) || STANDALONE_UTILITY_PROGRAM */
  utf16_len = 0;
  for (p = (unsigned char *)filename; *p != 0; p += num_utf8_bytes) {
    if (*p < 0x80) {
      /* This is an ASCII character, so we can just copy it directly. */
      add_to_wchar_buffer(*p);
      num_utf8_bytes = 1;
    } else {
      /* Convert a UTF-8 character to a single Unicode code point, noting
         how many bytes from filename were occupied by the UTF-8
         representation. */
      utf8_character_seen = TRUE;
      num_utf8_bytes = mbc_to_wide_char(p, &unicode_char, &err,
                                        /*is_native=*/FALSE);
      /* Convert that to either one UTF-16 value or a pair of surrogates. */
      num_utf16_chars = ucn_to_utf16(unicode_char, utf16_chars);
      check_assertion(num_utf16_chars <= 2);
      /* Copy the result into the buffer. */
      for (i = 0; i < num_utf16_chars; ++i) {
        add_to_wchar_buffer(utf16_chars[i]);
      }  /* for */
    }  /* if */
  }  /* for */
  /* Add the terminating null character. */
  add_to_wchar_buffer(0);
  return utf8_character_seen ? buffer : (wchar_t *)NULL;
#undef add_to_wchar_buffer
}  /* translate_filename_to_wchar */
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */


static char *convert_file_name_encoding(char		*orig_name,
					a_boolean	to_internal)
/*
orig_name is the null-terminated name of a file or directory.  Translate
to or from the internal encoding of the file name (depending on the value
of to_internal).  If some translation is required, a new string is allocated
in general storage, it is filled with the converted form, and its address
is returned. If no conversion is required, the original string is returned.
*/
{
  char *file_name = orig_name;

#if UNICODE_SOURCE_SUPPORTED
#if EDG_WIN32
  /* In case the native multibyte locale has been changed (e.g., by the
     setlocale pragma) set it back to the ANSI code page locale for purposes
     of file name translation. */
  _locale_t	saved_locale = native_multibyte_locale;
  native_multibyte_locale = ansi_code_page_locale;
#endif /* EDG_WIN32 */
  if (DEFAULT_UNICODE_SOURCE_KIND == usk_none) {  /*lint !e506*/
    /* The environment uses a non-Unicode encoding.  Go through the file
       name and check for any multibyte characters or characters > 0x7f.
       If it contains any such characters, it must be rewritten as UTF-8
       because UTF-8 is the standard internal encoding for file names. */
    a_boolean		conversion_needed = FALSE;
    sizeof_t		size_needed = 0;
    char		*p;
    unsigned long	wc;
    int			in_len;
    int			out_len;
    char		arr[4];
    /* Look to see whether the string contains any characters that
       require conversion.  Also determine the size needed if we have to
       allocate space for the converted copy. */
    for (p = orig_name; *p != '\0'; p += in_len) {
      in_len = mbc_to_wide_char(p, &wc, (a_boolean*)NULL,
                                /*is_native=*/to_internal);
      /* A conversion is needed if the input was a multibyte character or
         if we are converting to internal form an the input character must
         be converted to UTF-8. */
      if (in_len == 1 && (!to_internal || wc > 0x7f)) {
        out_len = 1;
      } else {
        conversion_needed = TRUE;
        out_len = to_internal ? wide_char_to_utf8(wc, arr) : 1;
      }  /* if */
      size_needed += out_len;
    }  /* for */
    if (conversion_needed) {
      /* The string contains at least one character that needs to be rewritten
         as UTF-8.  Allocate and fill a new string. */
      char *dest;
      dest = file_name = alloc_general(size_needed+1);
      for (p = orig_name; *p != '\0'; p += in_len) {
        int		i;
        a_boolean	err;
        in_len = mbc_to_wide_char(p, &wc, &err, /*is_native=*/to_internal);
        /* If the character could not be converted, substitute a "?". */
        if (err) wc = (unsigned long)'?';
        if (to_internal) {
          /* Converting from the external encoding to UTF-8. */
          if (wc <= 0x7f && in_len == 1) {
            *dest++ = *p;
          } else {
            out_len = wide_char_to_utf8(wc, arr);
            for (i = 0; i < out_len; i++) *dest++ = arr[i];
          }  /* if */
        } else {
          /* Converting from UTF-8 to the external encoding.  This version
             only supports single byte external encodings (e.g., Latin-1). */
          if (wc <= UCHAR_MAX) {
            *dest++ = (char)wc;
          } else {
            /* The character does not fit in a single byte.  Keep it in the
               internal encoding.  This can occur if a UTF-8 file name is
               converted to the internal encoding. */
            for (i = 0; i < in_len; i++) *dest++ = p[i];
          }  /* if */
        }  /* if */
      }  /* for */
      *dest = '\0';
    }  /* if */
  } else {
    /* We don't have code to handle UTF-16 as the default character set
       from the environment. */
    check_assertion(DEFAULT_UNICODE_SOURCE_KIND == usk_utf8); /*lint !e506*/
  }  /* if */
#if EDG_WIN32
  /* Restore the original locale. */
  native_multibyte_locale = saved_locale;
#endif /* EDG_WIN32 */
#endif /* UNICODE_SOURCE_SUPPORTED */
  return file_name;
}  /* convert_file_name_encoding */


char *file_name_in_internal_encoding(char *orig_name)
/*
orig_name is the null-terminated name of a file or directory as provided
by the environment, e.g., from the command line or from a system call.
Convert it if necessary to the character encoding used internally
for file names.  If some translation is required, a new string is allocated
in general storage, it is filled with the converted form, and its address
is returned. If no conversion is required, the original string is returned.
*/
{
  char	*file_name;

  file_name = convert_file_name_encoding(orig_name, /*to_internal=*/TRUE);
  return file_name;
}  /* file_name_in_internal_encoding */


static char *file_name_in_external_encoding(char *orig_name)
/*
orig_name is the null-terminated name of a file or directory in the
internal encoding.  Convert it if necessary to the form used by the
environment.  If some translation is required, a new string is allocated
in general storage, it is filled with the converted form, and its address
is returned. If no conversion is required, the original string is returned.
*/
{
  char	*file_name;

  file_name = convert_file_name_encoding(orig_name, /*to_internal=*/FALSE);
  return file_name;
}  /* file_name_in_internal_encoding */


a_boolean get_file_modification_time(char   *file_name,
                                     time_t *p_time)
/*
Determine whether a file exists, and if so, return the last modification
time.  Return TRUE if the file exists and is a regular file, FALSE otherwise.
*/
{
  a_boolean	is_regular = FALSE;

#if EDG_WIN32 && UNICODE_SOURCE_SUPPORTED
  wchar_t *wchar_file_name = translate_filename_to_wchar(file_name);
  if (wchar_file_name != NULL) {
    /* Use the Windows _wstat function instead of regular stat to handle
       non-ASCII characters in the file name. */
    struct _stat buf;
    if (_wstat(wchar_file_name, &buf) == 0) {
      is_regular = ((buf.st_mode & S_IFREG) != 0);
      if (is_regular && p_time != NULL) *p_time = buf.st_mtime;
    } else {
      /* If the file doesn't exist, set the time to zero just to be neat. */
      if (p_time != NULL) *p_time = 0;
    }  /* if */
  } else
#endif /* EDG_WIN32 && UNICODE_SOURCE_SUPPORTED */
  /* Do not insert code here. */
  {
    /* Check the file type.  Use the stat call instead of fstat because some
       implementations do not have the _file field in the structure. */
    struct stat buf;
    /* Translate the file name into the form used by the file system. */
    file_name = file_name_in_external_encoding(file_name);
    if (stat(file_name, &buf) == 0) {
      /* Use the POSIX S_ISREG if it is defined.  Otherwise use the
         non-POSIX test using S_IFREG. */
#ifdef S_ISREG
      is_regular = S_ISREG(buf.st_mode);
#else /* ifndef S_ISREG */
      is_regular = ((buf.st_mode & S_IFREG) != 0);
#endif /* ifdef S_ISREG */
      if (is_regular && p_time != NULL) *p_time = buf.st_mtime;
    } else {
      /* If the file doesn't exist, set the time to zero just to be neat. */
      if (p_time != NULL) *p_time = 0;
    }  /* if */
  }
  return is_regular;
}  /* get_file_modification_time */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
