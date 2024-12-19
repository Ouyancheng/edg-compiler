/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2024 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

cfe_daemon_common.c -- Common code for the C++/C front end daemon
                       (cfe_daemon.c) and its corresponding client
                       (cfe_daemon_client.c).

This file is provided as an example of using the front end as a library to
build a multi-threaded client/server C++/C compiler front end.

This client and its corresponding daemon are not (currently) production grade
and are intended for exposition only.

*/

#ifndef EDG_DAEMON_COMMON_H
#define EDG_DAEMON_COMMON_H 1

/* C standard library headers used to implement the daemon. */
#include <cstddef>
#include <cstring>

/* Unix headers used to implement the daemon. */
#include <unistd.h>

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

constexpr int   rc_daemon_error = 9;

constexpr a_const_char
                *exec_request_bytes = "exec";
                        /* The connection request tag bytes that indicate the
                           requested action is a front end execution. */
constexpr a_const_char
                *end_of_message_bytes = "7ef77b7a-cd45-4b2e-9498-5e999bb07a06";
                        /* The magic byte sequence used to indicate the end of
                           a message over the socket. */


template<typename a_Func>
inline char* doubling_buffer(a_Func func)
/*
Allocate a character buffer using operator new[] and pass it to the given
function.  If the buffer is sufficiently sized, the function should return
TRUE; otherwise, the function should return FALSE.

Once a sufficiently sized buffer has been created, the buffer will be returned.
The caller is responsible for freeing the buffer.
*/
{
  size_t buf_size = 100;
  char   *buf = NULL;

  /* Note that while Linux will allocate the buffer for you (if you use a NULL
     buffer argument) POSIX does not mandate this behavior. */
  do {
    if (buf != NULL) {
      delete [] buf;
      buf_size *= 2;
    }  /* if */
    buf = new char[buf_size];
  } while (!func(buf, buf_size));
  return buf;
}  /* doubling_buffer */


inline a_const_char* allocating_socket_file_name()
/*
Return a pointer to a null-terminated buffer containing the file name where the
client <-> daemon Unix socket can be found.  The caller is responsible for
freeing the buffer.
*/
{
  a_const_char *tmp_dir = getenv("TMP");

  if (tmp_dir == NULL) {
    /* Fall back to a predefined temporary directory. */
#if defined(DEFAULT_TMPDIR)
    tmp_dir = DEFAULT_TMPDIR;
#else /* !defined(DEFAULT_TMPDIR) */
    tmp_dir = "/tmp";
#endif /* defined(DEFAULT_TMPDIR) */
  }  /* if */

  auto apply_file_name = [tmp_dir](char *buf, size_t buf_size) -> a_boolean {
    int chars_written = snprintf(buf, buf_size, "%s/cpfe-daemon-%lld.sock",
                                 tmp_dir, (long long)geteuid());
    return chars_written >= 0 && ((size_t)chars_written) <= buf_size;
  };
  return doubling_buffer(apply_file_name);
}  /* allocating_socket_file_name */


/*
A type that encapsulates a buffered read from a Unix socket.
*/
struct a_socket_reader {
  a_socket_reader(int socket_fd_val)
    : socket_fd(socket_fd_val)
    {}
  template<typename a_Func>
  a_boolean read_message_parts(a_Func callback_fn);
private:
  template<typename a_Func>
  a_boolean forward_buffer_from(int offset, a_Func callback_fn);
  static constexpr size_t
                buffer_size = 1000;
                        /* The size of the socket reader buffer. */
  int           socket_fd;
                        /* The socket file descriptor to read from. */
  char          buffer[buffer_size] = {};
                        /* The buffer used to read from the file descriptor. */
  int           num_buffer_chars = 0;
                        /* The number of characters written to the buffer
                           during the last read operation. */
  int           unprocessed_bytes_start = -1;
                        /* If there are no bytes from the previous read call,
                           this is -1.  If there are bytes from the previous
                           read call this is the offset from the start of
                           buffer to those bytes. */
  int           eom_matches = 0;
                        /* The number of end of message characters that have
                           been matched. */
};  /* a_socket_reader */


template<typename a_Func>
a_boolean a_socket_reader::read_message_parts(a_Func callback_fn)
/*
Given a callback (accepting a_const_char* and size_t) read the next message
from the socket calling the given callback with any new message parts.  If the
complete message has been read without any errors, return TRUE; otherwise,
return FALSE.
*/
{
  a_boolean result = false;

  if (this->unprocessed_bytes_start != -1) {
    /* Reset the unprocessed byte start and then process the remaining bytes
       from the previous call to read_message_parts. */
    int offset = this->unprocessed_bytes_start;

    this->unprocessed_bytes_start = -1;
    if (this->forward_buffer_from(offset, callback_fn)) {
      /* The previous buffer contained an additional complete message. */
      goto complete_message;
    }  /* if */
  }  /* if */
  while (true) {
    this->num_buffer_chars = read(this->socket_fd, this->buffer, buffer_size);
    if (this->num_buffer_chars <= 0) {
      break;
    }  /* if */
    if (forward_buffer_from(0, callback_fn)) {
      goto complete_message;
    }  /* if */
  }  /* while */
  goto done;
complete_message:
  result = true;
done:
  return result;
}  /* a_socket_reader::read_message_parts */


template<typename a_Func>
a_boolean a_socket_reader::forward_buffer_from(int offset, a_Func callback_fn)
/*
Forward buffer contents from the given starting position to the callback (see
read_message_parts for more information about the callback).  If a complete
message was found, return TRUE; otherwise, return FALSE.
*/
{
  /* This is the next needed matching character to terminate output. */
  unsigned  num_matches_needed = strlen(end_of_message_bytes);
  a_boolean result = false;
  int       end_of_message_part = this->num_buffer_chars;

  for (int i = offset; i < this->num_buffer_chars; ++i) {
    if (this->buffer[i] == end_of_message_bytes[this->eom_matches]) {
      if (++(this->eom_matches) == num_matches_needed) {
        /* This is the end of the message. */
        int next_idx = i + 1;

        if (next_idx < this->num_buffer_chars) {
          /* There are unprocessed bytes still in the buffer after the end of
             message string starting at the next position. */
          this->unprocessed_bytes_start = next_idx;
        }  /* if */
        /* Update the end of message part to reflect the fact no characters
           after this point should be included in this message. */
        end_of_message_part = next_idx;
        break;
      }  /* if */
    } else if (eom_matches > 0) {
      /* This is not the end of message string. */
      if (eom_matches > i) {
        /* This partial match starts before the current buffer: replay the
           matching characters and update the initial offset so that bytes
           aren't repeated. */
        callback_fn(end_of_message_bytes, (size_t)eom_matches);
        offset = i;
      }  /* if */
      this->eom_matches = 0;
    }  /* if */
  }  /* for */

  /* Forward all characters in the buffer that have not yet been forwarded
     and are known to not be part of the end of message bytes. */
  int num_chars_to_forward = (end_of_message_part -
                                                 (offset + this->eom_matches));
  if (num_chars_to_forward > 0) {
    callback_fn(this->buffer + offset, (size_t)num_chars_to_forward);
  }  /* if */
  if (this->eom_matches == num_matches_needed) {
    result = true;
    this->eom_matches = 0;
  }  /* if */
  return result;
}  /* a_socket_reader::forward_buffer_from */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* EDG_DAEMON_COMMON_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2024 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
