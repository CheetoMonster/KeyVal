#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "KeyVal.h"

extern unsigned char KEYVAL_QUIET;

static void die(const char *file, int line, const char *expected, char got) {
  if (KEYVAL_QUIET) return;  // hopefully only for regressions

  char got_fmt[16];
  if (got == -1)
    strcpy(got_fmt, "EOF");
  else if (got == '\n')
    strcpy(got_fmt, "EOL");
  else if (got == '\t')
    strcpy(got_fmt, "\\t");
  else {
    got_fmt[0] = got;
    got_fmt[1] = 0;
  }

  fprintf(stderr,
      "[ERROR] expecting %s, but got '%s'\n  at line %d of %s\n",
      expected, got_fmt, line, file);
  return;
}


static char *get_input_char__buf;
static int get_input_char__ptr;
static int get_input_char__eof_location;
// Returns:
//   -1  at EOF
//   -2  on error
//   or whatever the next character of input is
static short get_input_char(FILE *fh) {

  // do I need to read in the next page:
  if (get_input_char__ptr == 4096) {
    int num_read = fread(get_input_char__buf, 1, 4096, fh);
    if (!num_read) {
      // could be EOF or an error.  Error => return -2:
      if (ferror(fh)) {
        if (!KEYVAL_QUIET) {
          fprintf(stderr,
              "[ERROR] problem reading input file.  (Did it vanish?)\n");
        }
        return -2;
      }
    }
    get_input_char__eof_location = num_read;
    get_input_char__ptr = 0;
  }

  // EOF => return -1:
  if (get_input_char__ptr == get_input_char__eof_location) {
    return -1;
  }

  // ok, next byte's ready to go:
//printf("** returning '%c'\n", get_input_char__buf[get_input_char__ptr]);
  return get_input_char__buf[get_input_char__ptr++];
}


unsigned char KeyVal_load(struct KeyVal *keyval, const char *filename) {

  FILE *fh = fopen(filename, "r");
  if (!fh) {
    if (!KEYVAL_QUIET) {
      fprintf(stderr,
          "[ERROR] cannot open file '%s'\n",
          filename);
    }
    return 2;
  }

  typedef enum {
      S_WAITING_FOR_KEY,
      S_COMMENT,
      S_QUOTEDSTRING,
      S_WAITING_FOR_EQ_OR_DELETE,
      S_WAITING_FOR_VALUE,
      S_WAITING_FOR_EOL,
      S_ESCAPE,
    } statelist;

  // (re)initialize all the state variables:
  statelist curr_state = S_WAITING_FOR_KEY;
  statelist stack_state = -1;  // where to pop back from certain states
  get_input_char__buf = malloc(4096);
  get_input_char__ptr = 4096;
  get_input_char__eof_location = -1;

  int line_num = 1;

  // we use the same code for filling both the key and the value, so as an
  // abstraction we point 'curr_str' to whichever one we're filling at the time:
  char *curr_key = malloc(1024);
  char *curr_val = malloc(1024);
  char *curr_str;
  int curr_str_len;

  int retcode = 0;
  int burn_to_eol = 0;  // for error listing
  int error_count = 0;

  short input_char;

  do {  // this is a do-while because the EOF needs to go through the machine

    input_char = get_input_char(fh);
//printf("* state=%d, input=%c (%d)\n", curr_state, input_char, input_char);

    if (input_char == -2) break;  // in case of error

    switch(curr_state) {
    case S_WAITING_FOR_KEY:
      switch(input_char) {
      // comments start with "#":
      case '#':
        curr_state = S_COMMENT;
        break;
      // keys are quoted strings, so we look for a quote:
      case '`':
        stack_state = S_WAITING_FOR_EQ_OR_DELETE;
        curr_state = S_QUOTEDSTRING;
        curr_str = curr_key;
        curr_str_len = 0;
        break;
      // skip whitespace:
      case ' ':
      case '\t':
        break;
      case '\n':
        ++line_num;
        break;
      // EOF:
      case -1:
        break;
      // anything else is unexpected:
      default:
        die(filename, line_num, "comment or key", input_char);
        burn_to_eol = 1;
        break;
      }
      break;

    case S_COMMENT:
      switch(input_char) {
      // comments go to end-of-line:
      case '\n':
        ++line_num;
        curr_state = S_WAITING_FOR_KEY;
        break;
      // EOF:
      case -1:
        break;
      // everything else is ignored:
      default:
        break;
      }
      break;

    case S_QUOTEDSTRING:
      switch(input_char) {
      // close-quote:
      case '`':
        curr_str[curr_str_len] = 0;
        curr_state = stack_state;
        stack_state = -1;
        break;
      // escape sequence:
      case '\\':
        curr_state = S_ESCAPE;
        break;
      // carriage returns and EOF are about the only thing we don't want to see:
      case '\n':
      case -1:
        ++line_num;
        die(filename, line_num, "anything but \\n or EOF", input_char);
        burn_to_eol = 1;
        break;
      // any other character just gets added to the string:
      default: curr_str[curr_str_len++] = input_char; break;
      }
      break;

    case S_WAITING_FOR_EQ_OR_DELETE:
      switch(input_char) {
      // skip whitespace:
      case ' ':
      case '\t':
        break;
      // an "=" means is a key=val pair:
      case '=':
        curr_state = S_WAITING_FOR_VALUE;
        break;
      // a "d" means it's a key-delete (maybe)
      case 'r':
        // manually scan the next several bytes for "remove"
        if (get_input_char(fh) != 'e'
            || get_input_char(fh) != 'm'
            || get_input_char(fh) != 'o'
            || get_input_char(fh) != 'v'
            || get_input_char(fh) != 'e') {
          // this is perhaps not the clearest error message, but hey:
          die(filename, line_num, "remove", '?');
          burn_to_eol = 1;
          break;
        }
        KeyVal_remove(keyval, curr_key);
        curr_state = S_WAITING_FOR_EOL;
        break;
      case '\n':
        ++line_num;
      default:
        die(filename, line_num, "= or delete", input_char);
        burn_to_eol = 1;
        break;
      }
      break;

    case S_WAITING_FOR_VALUE:
      switch(input_char) {
      // a quote means the start of a value:
      case '`':
        stack_state = S_WAITING_FOR_EOL;
        curr_state = S_QUOTEDSTRING;
        curr_str = curr_val;
        curr_str_len = 0;
        break;
      // skip whitespace:
      case ' ':
      case '\t':
        break;
      // anything else is unexpected:
      case '\n':
        ++line_num;
      default:
        die(filename, line_num, "comment or value", input_char);
        burn_to_eol = 1;
        break;
      }
      break;

    case S_ESCAPE:
      switch(input_char) {
      // escapes are removed from backslashes and quotes:
      case '\\':
      case '`':
        curr_str[curr_str_len++] = input_char;
        curr_state = S_QUOTEDSTRING;
        break;
      // any other escapes are actually just preserved:
      case '\n':
        ++line_num;
      default:
        curr_str[curr_str_len++] = '\\';
        curr_str[curr_str_len++] = input_char;
        break;
      }
      break;

    case S_WAITING_FOR_EOL:
      switch(input_char) {
      // ignore whitespace:
      case ' ':
      case '\t':
        break;
      // carriage returns and EOFs trigger the "we have a key=value pair" code:
      case '\n':
        ++line_num;
        curr_state = S_WAITING_FOR_KEY;
      case -1: // (EOF)
//printf("[debug] '%s' => '%s'\n", curr_key, curr_val);
        // add it to the database:
        KeyVal_setValue(keyval, curr_key, curr_val);
//printf("b\n");
        break;
      // anything else is unrecognized:
      default:
        die(filename, line_num, "carriage return", input_char);
        burn_to_eol = 1;
        break;
      }
      break;
    }
//printf("c\n");

    // if there was a parse error, keep going so that we can list out more than
    // one problem at once.  This is a manual implementation of error
    // recovery states:
    if (burn_to_eol) {
      // eventually return an error code:
      retcode = 1;
      // pretend we're starting back at the beginning:
      curr_state = S_WAITING_FOR_KEY;
      curr_str = curr_key;
      curr_str_len = 0;
      // burn input until we hit \n (or EOF) (or an actual error):
      while (input_char != '\n' && input_char != -1 && input_char != -2) {
        input_char = get_input_char(fh);
      }
      ++line_num;
      if (input_char == -2) break; // in case of error reading input

      // count the number of errors we see, and stop before flooding the screen:
      ++error_count;
      if (error_count == 13) {
        if (!KEYVAL_QUIET) fprintf(stderr, "^^ too many errors, halting\n");
        break;
      }

      // reset flag:
      burn_to_eol = 0;
    }
//printf("d\n");
  } while (input_char != -1);
//printf("e\n");
  // cleanup:
  fclose(fh);
  free(curr_key); curr_key = 0;
  free(curr_val); curr_val = 0;
  free(get_input_char__buf); get_input_char__buf = 0;
//printf("f\n");

  return retcode;
}
