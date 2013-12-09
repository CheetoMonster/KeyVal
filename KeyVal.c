
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "KeyVal.h"


unsigned char KEYVAL_QUIET = 0;  // only meant for regressions

static const long KEYVAL_MIN_ARRAY_SIZE = 16;
static const int KEYVAL_MAX_STR_LEN = 1024;  // 'str' means key or value
static const int KEYVAL_MAX_INTERP_DEPTH = 25;

// setting 'errno' is usually automatic on malloc fails, but I do it explicitly

static const char *ERRSTR = "%s: '%s' argument null\n";



//////////////////////////////////////// KeyValElement

// Creates a new KeyValElement, and initializes data to a copy of the given
// parameters.
// Returns:
//   0: everything okay.  '*res' now points to a valid KeyValElement object
//   1: encountered errors. stderr spewed, errno is set.
static unsigned char
KeyValElement_new(struct KeyValElement **res, const char *key, const char *val) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  *res = 0;  // just for safety
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }
  if (!val) {
    fprintf(stderr, ERRSTR, __func__, "val");
    errno = EINVAL;
    return 1;
  }

  struct KeyValElement *tmp = malloc(sizeof(struct KeyValElement));
  if (!tmp) {
    fprintf(stderr, "KeyValElement_new: out of memory\n");
    errno = ENOMEM;
    return 1;
  }

  tmp->key = strdup(key);
  if (!tmp->key) {
    fprintf(stderr, "KeyValElement_new: out of memory\n");
    free(tmp);
    errno = ENOMEM;
    return 1;
  }
  tmp->val = strdup(val);
  if (!tmp->val) {
    fprintf(stderr, "KeyValElement_new: out of memory\n");
    free(tmp->key);
    free(tmp);
    errno = ENOMEM;
    return 1;
  }
  *res = tmp;
  return 0;
}

// Cleans up the given KeyValElement object by deleting its data and then itself.
// Returns:
//   0: everything okay
//   1: encountered errors.  stderr spewed, errno is set.
static unsigned char
KeyValElement_delete(struct KeyValElement *element) {
  if (!element) {
    fprintf(stderr, ERRSTR, __func__, "element");
    errno = EINVAL;
    return 1;
  }
  free(element->key); element->key = 0;
  free(element->val); element->val = 0;
  free(element);
  return 0;
}


//////////////////////////////////////// KeyVal

// KeyVal_strcmp
// We need a custom strcmp because we need "::" to be handled differently.  With
// regular strcmp on US ASCII strings, a direct sort would result in the following:
//   key::1
//   key::10
//   key::1::bar
// But we want all the keys at a single level grouped together, like so:
//   key::1
//   key::1::bar
//   key::10
// To do that, we handle "::" as a special case.
// The return value is the same for regular strcmp:
//   0  if s1==s2
//   >0 if s1>s2
//   <0 if s1<s2
// Surprisingly, this custom strcmp is not as slow as you'd expect.
//
// This function is internal, so it is not declared in KeyVal.h.  However, it
// is tested directly in test.c, so it is not static.
//
// Returns:
//   0: strings match exactly
//   <0: s1 < s2
//   >0: s1 > s2
// (Note: this function does NOT return an error code, but will spew stderr and
// set errno.)
int KeyVal_strcmp(const char *s1, const char *s2) {
  if (!s1) {
    fprintf(stderr, ERRSTR, __func__, "s1");
    errno = EINVAL;
  }
  if (!s2) {
    fprintf(stderr, ERRSTR, __func__, "s2");
    errno = EINVAL;
  }

  const unsigned char *_s1 = (const unsigned char*)s1;
  const unsigned char *_s2 = (const unsigned char*)s2;

  while (1) {
    if (!*_s1 && !*_s2) {
      // s1 and s2 are same length, and all same chars:
      return 0;
    }
    else if (!*_s2) {
      // s1 is longer than s2:
      return 1;
    }
    else if (!*_s1) {
      // s2 is longer than s1:
      return -1;
    }
    else {
      // need to figure out which, if either, has double-colons:
      unsigned char s1_dc = (*_s1 == ':' && *(_s1+1) == ':');
      unsigned char s2_dc = (*_s2 == ':' && *(_s2+1) == ':');
      if (s1_dc && s2_dc) {
        // match so far, and we can increment two chars:
        ++_s1; ++_s1;
        ++_s2; ++_s2;
      }
      else if (s1_dc && !s2_dc) {
        // artifically make s1 < s2
        return -2;
      }
      else if (s2_dc && !s1_dc) {
        // artificially make s2 < s1
        return 2;
      }
      // no double-colons, so compare as usual:
      else if (*_s1 < *_s2) {
        return -3;
      }
      else if (*_s2 < *_s1) {
        return 3;
      }
      else {
        // match so far.  Carry on!
        ++_s1;
        ++_s2;
      }
    }
  }
  // (will never get here)
  // (famous last words!!)
}

// Returns:
//   0: everything okay.  '*res' is set to a valid result
//   1: encountered errors.  stderr spewed, errno is set.
// The real result is stored wherever 'res' points, so please give it a
// valid pointer.
// The real result is the index where the given key should exist in the array:
//   - if the key exists, this is where it is;
//   - if the key does not exist, this is where it should be inserted.
//
// This function is internal, so it is not declared in KeyVal.h.  However, it
// is tested directly in test.c, so it is not static.
unsigned char KeyVal_findIdealIndex(unsigned long *res,
    struct KeyVal *kv, const char *key) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }

  // Instead of a typical binary search, we're using something called
  // "deferred detection", where we don't check for equality inside the loop.
  // Instead, we check less-than vs greater-than-or-equal-to.  This not only
  // gives us the 'ideal' location (whether or not it exists), but has
  // fewer comparisons inside the loop!

  // The general idea is to have curr_low and curr_hi converge on the ideal
  // location.  At all times:
  // - the element at curr_hi's spot is either the key or something above it (or off the end)
  // - the element at curr_lo's spot is either the key or something below it

  unsigned long curr_low = 0;
  unsigned long curr_hi = kv->used_size;  // can't be - 1; what if ideal is off the end?
  unsigned long curr_mid = curr_low;  // in case array is empty
  while (curr_low != curr_hi) {

    // SHR is both a fast divide and a fast floor:
    curr_mid = (curr_low + curr_hi) >> 1;
//printf("lo: %lu, mid: %lu, hi: %lu\n", curr_low, curr_mid, curr_hi);
//printf("strcmp(%s, %s)=%d\n", kv->data[curr_mid]->key, key, KeyVal_strcmp(kv->data[curr_mid]->key, key));

    if (KeyVal_strcmp(kv->data[curr_mid]->key, key) < 0) {
      curr_low = curr_mid + 1;
    } else {
      curr_hi = curr_mid;
    }
  }

  *res = curr_low;
  return 0;
}


// Returns:
//   0: everything okay.  '*res' is set to where the given key should exist in
//     the array:
//     - if the key exists, this is where it is;
//     - if the key does not exist, this is where it should be inserted.
//   1: encountered errors.  stderr spewed, errno is set.
//   2: key was not found.
//
// This function is internal, so it is not declared in KeyVal.h.  However, it
// is tested directly in test.c, so it is not static.
unsigned char KeyVal_findIndex(unsigned long *res,
    struct KeyVal *kv, const char *key) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }

  // find where it 'should' be:
  unsigned long idx;
  if (KeyVal_findIdealIndex(&idx, kv, key)) return 1;

//printf("** findIndex: used=%lu, idx=%lu\n", kv->used_size, idx);
  if (kv->used_size == idx) return 2;  // ideal is off the end of the array, so it wasn't found
  // check if the key at the ideal index happens to be it:
//printf("** strcmp'ing %s and %s..\n", kv->data[idx]->key, key);
  if (strcmp(kv->data[idx]->key, key) == 0) {
    *res = idx;
    return 0;
  }
  // not found:
  return 2;
}



//////////////////////////////////////// KeyVal

unsigned char
KeyVal_new(struct KeyVal **res) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }

  // alloc:
  struct KeyVal *tmp_res = malloc(sizeof(struct KeyVal));
  if (!tmp_res) {
    fprintf(stderr, "KeyVal_new: out of memory\n");
    errno = ENOMEM;
    return 1;
  }

  // init:
  tmp_res->max_size = KEYVAL_MIN_ARRAY_SIZE;
  tmp_res->used_size = 0;
  tmp_res->last_sorted = 0;
  tmp_res->data = calloc(KEYVAL_MIN_ARRAY_SIZE, sizeof(struct KeyValElement*));
  if (!tmp_res->data) {
    fprintf(stderr, "KeyVal_new: out of memory\n");
    free(tmp_res);
    errno = ENOMEM;
    return 1;
  }

  *res = tmp_res;
  return 0;
}


unsigned char
KeyVal_delete(struct KeyVal *kv) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }

  // I have a habit of zero'ing out things after freeing them, so that if
  // anything is still pointing to them, they segfault right away instead
  // picking up essentially random data.  However, there's a good argument
  // to be made that it's unnecessary computation.

  // destroy components:
  for (unsigned long i = 0; i < kv->used_size; ++i) {
    if (KeyValElement_delete(kv->data[i])) return 1;
    kv->data[i] = 0;
  }
  // destroy array:
  free(kv->data);
  kv->data = 0;

  // destroy myself:
  free(kv);
  return 0;
}


// this is a custom function because we need to account for escapes
static int
KeyVal_strlen(const char *keystr) {
  if (!keystr) {
    fprintf(stderr, ERRSTR, __func__, "keystr");
  }

  int res = 0;
  for (const char *ch = keystr;
      *ch;
      ch++) {
    res += (*ch == '`' || *ch == '\\') ? 2 : 1;
  }
  return res;
}


static void
KeyVal_escape_and_quote(char *dest, const char *src) {
  if (!dest) {
    fprintf(stderr, ERRSTR, __func__, "dest");
  }
  if (!src) {
    fprintf(stderr, ERRSTR, __func__, "src");
  }

  unsigned long dest_idx = 0;
  dest[dest_idx++] = '`';
  for (const char *ch = src;
      *ch;
      ++ch) {
    if (*ch == '`' || *ch == '\\') {
      dest[dest_idx++] = '\\';
    }
    dest[dest_idx++] = *ch;
  }
  dest[dest_idx++] = '`';
  dest[dest_idx++] = 0;
  return;
}


static unsigned char
KeyVal_interp_next(char **res, struct KeyVal *kv, const char *str, unsigned int depth) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!str) {
    fprintf(stderr, ERRSTR, __func__, "str");
    errno = EINVAL;
    return 1;
  }

  // hit KEYVAL_MAX_INTERP_DEPTH number of recursive variables?
  if (depth == KEYVAL_MAX_INTERP_DEPTH) {
    return 2;
  }

  // various error cases return a copy of the original string.  If variables
  // are found, we'll reconstruct the string:
  *res = strdup(str);
  if (!*res) {
    fprintf(stderr, "KeyVal_interp: out of memory\n");
    errno = ENOMEM;
    return 1;
  }

  // find the first close-brace, if there is one:
  char *close_brace = index(*res, '}');
  while (close_brace) {

    // trace back to the preceding "${", if there is one:
    char *open_brace = 0;
    char *t = close_brace - 2;
    while (*res <= t) {
      if (*t == '$' && *(t+1) == '{') {
        open_brace = t;
        break;
      }
      --t;
    }
    if (!open_brace) {
      return 0;
    }

    // extract variable path:
    char tmpstr[KEYVAL_MAX_STR_LEN];
    char *tptr = &tmpstr[0];
    t = open_brace + 2;
    while (t < close_brace) {
      *tptr++ = *t++;
    }
    *tptr = 0;
    if (!strcmp(tmpstr, "")) {  // just in case
      return 0;
    }

    // get its (uninterpolated) value:
    char *subval;
    if (KeyVal_getValue(&subval, kv, tmpstr, 0)) {
      // propagate errors
      return 1;
    }
    if (!*subval) {
      // uninterpolatable variable, so just stop here with what we've got
      return 0;
    }

    // interpolate variables in it:
    char *substr;
    unsigned char interp_res = KeyVal_interp_next(&substr, kv, subval, depth+1);
    if (interp_res == 1) return 1;  // propagate error
    if (interp_res == 2) return 2;  // propagate recursive variable error

    // patch together the new string.
    // copy the pre-variable part:
    tptr = &tmpstr[0];
    t = *res;
    while (t < open_brace) {
      *tptr++ = *t++;
    }
    // then the variable part:
    t = &substr[0];
    while (*t) {
      *tptr++ = *t++;
    }
    // then the post-variable part:
    t = close_brace + 1;
    while (*t) {
      *tptr++ = *t++;
    }
    *tptr = 0;
    // Finally, move it back to res:
    free(*res);
    *res = strdup(tmpstr);
    if (!*res) {
      fprintf(stderr, "KeyVal_interp_next: out of memory\n");
      errno = ENOMEM;
      return 1;
    }

    // cleanup the sub-variable memory:
    free(substr);

    // next iteration:
    close_brace = index(*res, '}');
  }

  return 0;
}


unsigned char
KeyVal_interp(char **res, struct KeyVal *kv, const char *str) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!str) {
    fprintf(stderr, ERRSTR, __func__, "str");
    errno = EINVAL;
    return 1;
  }

  unsigned char interp_res = KeyVal_interp_next(res, kv, str, 0);
  if (interp_res == 1) return 1;  // propagate error
  if (interp_res == 2) {
    if (!KEYVAL_QUIET) {
      fprintf(stderr,
          "%s: encountered %d levels of variables; possible recursion\n"
          "in key `%s`\n",
          __func__,
          KEYVAL_MAX_INTERP_DEPTH, str);
    }
    return 2;
  }
  return 0;
}


static unsigned char
KeyVal_ensureSorted(struct KeyVal *kv) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }

  // make sure there's something to sort:
  if (kv->used_size == 0) return 0;

  // make sure we actually need to sort:
  if (kv->last_sorted == kv->used_size) return 0;

  // this uses insertion sort!
  unsigned long orig_size = kv->used_size;
  kv->used_size = kv->last_sorted;  // so that findIdealIndex works right
  for (unsigned long idx = kv->last_sorted;
      idx < orig_size;
      ++idx) {

    const char *key = kv->data[idx]->key;

    unsigned long ideal_idx;
    if (KeyVal_findIdealIndex(&ideal_idx, kv, key)) return 1;

    // if it goes at the end, we're already done!
    if (ideal_idx == kv->used_size) {
      ++kv->used_size;
    }
    // if it's already there, just overwrite the value:
    else if (!strcmp(kv->data[ideal_idx]->key, key)) {
      // move the existing value from [idx] to [ideal_idx]:
      free(kv->data[ideal_idx]->val);
      kv->data[ideal_idx]->val = kv->data[idx]->val;
      // destroy the key for [idx]:
      // (don't use KeyValElement_delete because we don't want 'val' deleted)
      free(kv->data[idx]->key);
      kv->data[idx]->key = 0;
      free(kv->data[idx]);
      kv->data[idx] = 0;
      // do not increase used_size!
    }

    // otherwise, we need to insert it to the ideal_idx spot:
    else {

      // we're going to overwrite this spot, so remember what it points to:
      struct KeyValElement *newb = kv->data[idx];

      // move everything down one:
      void *src = &kv->data[ideal_idx];
      void *dest = &kv->data[ideal_idx+1];
      unsigned long num_bytes = sizeof(struct KeyValElement*) * (kv->used_size - ideal_idx);
      memmove(dest, src, num_bytes);

      // set the ideal spot to idx's (original) data:
      kv->data[ideal_idx] = newb;
      ++kv->used_size;
    }
  }

  // and now we know it's sorted!
  kv->last_sorted = kv->used_size;

  return 0;
}


static unsigned char
KeyVal_resize(struct KeyVal *kv, unsigned long new_size) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }

  // do not resize below KEYVAL_MIN_ARRAY_SIZE
  if (new_size < KEYVAL_MIN_ARRAY_SIZE) {
    new_size = KEYVAL_MIN_ARRAY_SIZE;
  }

  // don't resize to the same value:
  if (new_size == kv->max_size) {
    return 0;
  }

  kv->max_size = new_size;
  
  // create new array:
  struct KeyValElement **new_data = calloc(kv->max_size, sizeof(struct KeyValElement *));
  if (!new_data) {
    fprintf(stderr, "KeyVal_resize: out of memory\n");
    errno = ENOMEM;
    return 1;
  }

  // copy over existing things:
  memcpy(new_data,
      kv->data,
      kv->used_size * sizeof(struct KeyValElement *));

  // free up old array:
  free(kv->data);

  // shuffle:
  kv->data = new_data;

  return 0;
}


unsigned char
KeyVal_save(struct KeyVal *kv, const char *filepath, unsigned char interp, unsigned char align) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!filepath) {
    fprintf(stderr, ERRSTR, __func__, "filepath");
    errno = EINVAL;
    return 1;
  }

  FILE *fh = fopen(filepath, "w");
  if (!fh) {
    fprintf(stderr, "[ERROR] KeyVal_save: cannot write to this file:\n  %s\n  because of:\n  ", filepath);
    perror(0);
    return 2;
  }

  // make sure the database is sane:
  if (KeyVal_ensureSorted(kv)) return 1;

  // if we need to align, find the max size of all the keys:
  char fmt_str[16];
  if (align) {
    int max_size = 0;
    for (unsigned long i = 0;
        i < kv->used_size;
        ++i) {
      int this_len = KeyVal_strlen(kv->data[i]->key);
      if (max_size < this_len) {
        max_size = this_len;
      }
    }
    sprintf(fmt_str, "%%-%ds = %%s\n", max_size+2);  // "-" is for left-align; "+2" is for the surrounding quotes
  }
  else {
    sprintf(fmt_str, "%%s = %%s\n");
  }

  // the +2 is for the surrounding quote characters:
  char this_key[KEYVAL_MAX_STR_LEN+2];
  char this_val[KEYVAL_MAX_STR_LEN+2];    

  for (unsigned long i = 0;
      i < kv->used_size;
      ++i) {
    KeyVal_escape_and_quote(this_key, kv->data[i]->key);
    // may need to interpolate variables in this_val:
    if (interp) {
      char *interped_val;
      if (KeyVal_interp(&interped_val, kv, kv->data[i]->val)) return 1;
      KeyVal_escape_and_quote(this_val, interped_val);
      free(interped_val);
    }
    else {
      KeyVal_escape_and_quote(this_val, kv->data[i]->val);
    }
    // good to go:
    fprintf(fh, fmt_str, this_key, this_val);
  }

  // check close for errors, because this is what fails when disks fill up, etc:
  if (fclose(fh)) {
    fprintf(stderr, "[ERROR] KeyVal_save: cannot finish writing this file:\n  %s\n  because of:\n  ", filepath);
    perror(0);
    return 2;
  }
  return 0;
}


unsigned char
KeyVal_setValue(struct KeyVal *kv, const char *key, const char *val) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }
  if (!val) {
    fprintf(stderr, ERRSTR, __func__, "val");
    errno = EINVAL;
    return 1;
  }
  int _tmp_len = KeyVal_strlen(key); // use the KeyVal version to account for escapes
  if (KEYVAL_MAX_STR_LEN < _tmp_len) {
    fprintf(stderr, "KeyVal_setValue: 'key' argument too long (%d > %d): '%s'\n", _tmp_len, KEYVAL_MAX_STR_LEN, key);
    errno = EINVAL;
    return 1;
  }
  _tmp_len = KeyVal_strlen(val);
  if (KEYVAL_MAX_STR_LEN < _tmp_len) {
    fprintf(stderr, "KeyVal_setValue: 'val' argument too long (%d > %d): '%s'\n", _tmp_len, KEYVAL_MAX_STR_LEN, val);
    errno = EINVAL;
    return 1;
  }

  int _need_to_add = 1;

  // base case: nothing in the array at all.
  if (kv->used_size == 0) {
    if (KeyValElement_new(&kv->data[0], key, val)) return 1;
    kv->used_size = 1;
    kv->last_sorted = 1;
    _need_to_add = 0;
  }

  // next two cases only apply if it's currently sorted:
  else if (kv->last_sorted == kv->used_size) {

    // next case: goes on the end of sorted list.  (Programmatically, this can
    // be rolled into the next case by checking if the ideal index is off the
    // end of the array.  However, findIdealIndex does log(n) strcmps, and we
    // want loading already-sorted files to be extremely fast, so we'll spend
    // one (possibly extra) strcmp to get that speedup.)
    if (KeyVal_strcmp(kv->data[kv->used_size - 1]->key, key) < 0) {
      _need_to_add = 0;
      // may need to resize:
      if (kv->used_size == kv->max_size) {
        if (KeyVal_resize(kv, kv->max_size*2)) return 1;
      }
      // add to end:
      if (KeyValElement_new(&kv->data[kv->used_size], key, val)) return 1;
      ++kv->used_size;
      ++kv->last_sorted;
    }

    // next case: overwrites an existing setting:
    else {
      unsigned long ideal_idx;
      if (KeyVal_findIdealIndex(&ideal_idx, kv, key)) return 1;
      if (!strcmp(kv->data[ideal_idx]->key, key)) {
        _need_to_add = 0;
        free(kv->data[ideal_idx]->val);
        kv->data[ideal_idx]->val = strdup(val);
        if (!kv->data[ideal_idx]->val) {
          fprintf(stderr, "KeyVal_setValue: out of memory\n");
          errno = ENOMEM;
          return 1;
        }
      }
      // otherwise it's a new setting somewhere in the middle, so just add to
      // the end and we'll sort it out later:
      else {
        _need_to_add = 1;
      }
    }
  }
  // last case: currently unsorted, so just add to the end, and sort later:
  else {
    _need_to_add = 1;
  }


  // still need to add it?
  if (_need_to_add == 1) {

    // may need to resize:
    if (kv->used_size == kv->max_size) {
      if (KeyVal_resize(kv, kv->max_size*2)) return 1;
    }
    // add to end:
    if (KeyValElement_new(&kv->data[kv->used_size], key, val)) return 1;
    ++kv->used_size;

    // this does not preserve sorting, so do not increment last_sorted
  }

  return 0;
}


unsigned char
KeyVal_getValue(char **res, struct KeyVal *kv, const char *key, int interp) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }

  if (KeyVal_ensureSorted(kv)) return 1;  // propagate error

  unsigned long idx;
  unsigned char find_res = KeyVal_findIndex(&idx, kv, key);
  if (find_res == 1) return 1;  // propagate error
  if (find_res == 2) {
    *res = 0;
    return 0;  // not found
  }
  // found, but need to interpolate variables:
  if (interp) {
    unsigned char interp_res = KeyVal_interp(res, kv, kv->data[idx]->val);
    if (interp_res == 1) return 1;  // propagate error
    if (interp_res == 2) return 2;  // propagate recursive variables
    return 0;
  }
  // found, with no interpolation:
  *res = strdup(kv->data[idx]->val);
  if (!*res) {
    fprintf(stderr, "KeyVal_getValue: out of memory\n");
    errno = ENOMEM;
    return 1;
  }
  return 0;
}


unsigned char
KeyVal_remove(struct KeyVal *kv, const char *key) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }

  // database must be sane first:
  if (KeyVal_ensureSorted(kv)) return 1;

  // where is it?
  unsigned long idx;
  unsigned char find_res = KeyVal_findIndex(&idx, kv, key);
  if (find_res == 1) return 1;  // propagate error
  if (find_res == 2) return 0;  // not found

  // delete it:
  if (KeyValElement_delete(kv->data[idx])) return 1;

  // if it's the last one, nothing to move:
  if (idx == kv->used_size - 1) {
    // (this space for rent)
  }
  // otherwise, move everything up one:
  else {
    void *src = &kv->data[idx+1];
    void *dest = &kv->data[idx];
    unsigned long num_bytes = sizeof(struct KeyValElement*)*(kv->used_size - idx - 1);
    memmove(dest, src, num_bytes);
  }

  // decrement counters:
  --kv->used_size;
  --kv->last_sorted;

  // should probably automatically resize, though it's not strictly necessary:
  // (the "- 2" is so that a tight loop of add+remove doesn't resize on every
  // single call)
  if (kv->used_size < kv->max_size / 2 - 2) {
    if (KeyVal_resize(kv, kv->max_size/2)) return 1;
  }

  return 0;
}



static int
KeyVal_has_subkey(const char *base, const char *extended, int base_len) {

  // this returns if 'extended' starts with 'base'+'::'+anything.

  // extended starts with base?
  if (strncmp(base, extended, base_len)) {
    // nope:
    return 0;
  }

  // followed by at least three characters?
  if (strlen(extended) <= base_len + 2) {
    // too short to have any subkeys:
    return 0;
  }

  // immediately following characters are two colons?
  if (extended[base_len] != ':' || extended[base_len+1] != ':') {
    // not followed by double-colons:
    return 0;
  }

  // well we don't know what it is, but it's a subkey:
  return 1;
}


static void KeyVal_extract_subkey(char *dest, const char *src, int offset) {

  int dest_idx = 0;

  // start copying characters until we hit a double-colon:
  while (src[offset]) {  // stop if we hit end-of-string

    // stop if we hit another double-colon:
    if (src[offset]==':' && src[offset+1]==':') {
      break;
    }

    dest[dest_idx++] = src[offset++];
  }
  dest[dest_idx] = 0;

  return;
}


unsigned char
KeyVal_getKeys(char ***res, struct KeyVal *kv, const char *path) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!path) {
    fprintf(stderr, ERRSTR, __func__, "path");
    errno = EINVAL;
    return 1;
  }

  if (KeyVal_ensureSorted(kv)) return 1;

  int path_len = strlen(path);
  int start_of_subkey;
  unsigned long data_start_idx;
  unsigned long data_end_idx;
  if (path_len == 0) {
    // this is an easy case:
    data_start_idx = 0;
    data_end_idx = kv->used_size;
    start_of_subkey = path_len;  // start at beginning
  }
  else {

    // the "ideal spot" for this path is where we'll start looking:
    if (KeyVal_findIdealIndex(&data_start_idx, kv, path)) return 1;

    // if it's off the end, we're done:
    if (kv->used_size <= data_start_idx) {
      *res = malloc(1 * sizeof(char*));
      if (!*res) {
        fprintf(stderr, "KeyVal_getKeys: out of memory\n");
        errno = ENOMEM;
        return 1;
      }
      (*res)[0] = 0;
      return 0;
    }

    // what did we find at data_start_idx?
    if (!strcmp(kv->data[data_start_idx]->key, path)) {
      // exact match, so the path is itself a valid key.  Which we skip:
      ++data_start_idx;
    }

    // now walk through the list until we find no more matches:
    data_end_idx = data_start_idx;  // points one past the last one
    while (data_end_idx < kv->used_size) {
      if (!KeyVal_has_subkey(path, kv->data[data_end_idx]->key, path_len)) {
        break;  // stopped matching
      }
      ++data_end_idx;
    }

    start_of_subkey = path_len + 2; // offset the two colons
  }


  // if nothing matched, we're done:
  if (data_start_idx == data_end_idx) {
    *res = malloc(1 * sizeof(char*));
    if (!*res) {
      fprintf(stderr, "KeyVal_getKeys: out of memory\n");
      errno = ENOMEM;
      return 1;
    }
    (*res)[0] = 0;
    return 0;
  }

  // now we can alloc, though this is the upper bound of the number of keys:
  *res = malloc((data_end_idx - data_start_idx + 1) * sizeof(char*));
  if (!*res) {
    fprintf(stderr, "KeyVal_getKeys: out of memory\n");
    errno = ENOMEM;
    return 1;
  }

  // go back and add the keys to the array:
  char prev_subkey[KEYVAL_MAX_STR_LEN]; prev_subkey[0] = 0;
  char this_subkey[KEYVAL_MAX_STR_LEN];
  unsigned long num_unique_keys = 0;  // not necessarily the same as "data_end_idx - data_start_idx"
  for (unsigned long i = data_start_idx;
      i < data_end_idx;
      ++i) {
    KeyVal_extract_subkey(this_subkey, kv->data[i]->key, start_of_subkey);
    if (strcmp(prev_subkey, this_subkey)) {
      // different, so it's a new key -- add to res:
      (*res)[num_unique_keys] = strdup(this_subkey);
      if (!(*res)[num_unique_keys]) {
        fprintf(stderr, "KeyVal_getKeys: out of memory\n");
        free(*res);
        errno = ENOMEM;
        return 1;
      }
      ++num_unique_keys;
      strcpy(prev_subkey, this_subkey);
    }
  }
  (*res)[num_unique_keys] = 0;

  // realloc back down if we're overconsuming memory:
  if (num_unique_keys != data_end_idx - data_start_idx) {
    // inexplicably, realloc may move 'res' to a completely different place:
    *res = realloc(*res, (num_unique_keys+1)*sizeof(char*));
    if (!*res) {
      fprintf(stderr, "KeyVal_getKeys: out of memory\n");
      errno = ENOMEM;
      return 1;
    }
  }

  return 0;
}

unsigned char
KeyVal_getAllKeys(char ***res, struct KeyVal *kv) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }

  // database must be sane:
  if (KeyVal_ensureSorted(kv)) return 1;

  // then just copy off all the keys:
  *res = malloc(sizeof(char*) * (kv->used_size + 1));
  if (!*res) {
    fprintf(stderr, "KeyVal_getAllKeys: out of memory\n");
    errno = ENOMEM;
    return 1;
  }
  for (unsigned long idx=0;
      idx<kv->used_size;
      ++idx) {
    (*res)[idx] = strdup(kv->data[idx]->key);
    if (!(*res)[idx]) {
      fprintf(stderr, "KeyVal_getAllKeys: out of memory\n");
      free(*res);
      errno = ENOMEM;
      return 1;
    }
  }
  (*res)[kv->used_size] = 0;
  return 0;
}




unsigned char
KeyVal_size(unsigned long *res, struct KeyVal *kv) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }

  // need to ensureSorted because unsorted arrays may have duplicates that we
  // don't want to count:
  if (KeyVal_ensureSorted(kv)) return 1;
  *res = kv->used_size;
  return 0;
}


unsigned char
KeyVal_hasValue(unsigned char *res, struct KeyVal *kv, const char *key) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key) {
    fprintf(stderr, ERRSTR, __func__, "key");
    errno = EINVAL;
    return 1;
  }

  if (KeyVal_ensureSorted(kv)) return 1;

  unsigned long idx;
  if (KeyVal_findIdealIndex(&idx, kv, key)) return 1;
  // check if it's even in the array:
  if (idx == kv->used_size) {
    *res = 0;
    return 0;
  }
  // return if the key at the ideal index happens to be it:
  *res = strcmp(kv->data[idx]->key, key) == 0;
  return 0;
}


unsigned char
KeyVal_hasKeys(unsigned char *res, struct KeyVal *kv, const char *path) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!path) {
    fprintf(stderr, ERRSTR, __func__, "path");
    errno = EINVAL;
    return 1;
  }

  if (KeyVal_ensureSorted(kv)) return 1;

  // the "ideal spot" for this path is where we'll start looking:
  unsigned long idx;
  if (KeyVal_findIdealIndex(&idx, kv, path)) return 1;

  // if it's off the end, we're done:
  if (kv->used_size <= idx) {
    *res = 0;
    return 0;
  }

  // what did we find at idx?
  if (!strcmp(kv->data[idx]->key, path)) {
    // exact match, so the path is itself a valid key.  Which we skip, in case
    // the next one has keys:
    ++idx;
    // did that just go off the end of the list?
    if (kv->used_size == idx) {
      *res = 0;
      return 0;
    }
  }

  // check to see if we have a subkey of this path:
  if (KeyVal_has_subkey(path, kv->data[idx]->key, strlen(path))) {
    *res = 1;
  } else {
    *res = 0;
  }
  return 0;
}


unsigned char
KeyVal_exists(unsigned char *res, struct KeyVal *kv, const char *key_or_path) {
  if (!res) {
    fprintf(stderr, ERRSTR, __func__, "res");
    errno = EINVAL;
    return 1;
  }
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    errno = EINVAL;
    return 1;
  }
  if (!key_or_path) {
    fprintf(stderr, ERRSTR, __func__, "key_or_path");
    errno = EINVAL;
    return 1;
  }

  // this is essentially the hasValue and hasKeys functions mashed together.

  if (KeyVal_ensureSorted(kv)) return 1;

  // the "ideal spot" for this path is where we'll start looking:
  unsigned long idx;
  if (KeyVal_findIdealIndex(&idx, kv, key_or_path)) return 1;

  // if it's off the end, we're done:
  if (kv->used_size <= idx) {
    *res = 0;
    return 0;
  }

  // what did we find at idx?
  if (!strcmp(kv->data[idx]->key, key_or_path)) {
    // exact match, which means it has a value:
    *res = 1;
    return 0;
  }

  // check to see if we have a subkey of this path:
  if (KeyVal_has_subkey(key_or_path, kv->data[idx]->key, strlen(key_or_path))) {
    *res = 1;
  } else {
    *res = 0;
  }
  return 0;
}


//////////////////////////////////////// debugging

void KeyVal_print(struct KeyVal *kv) {
  if (!kv) {
    fprintf(stderr, ERRSTR, __func__, "kv");
    return;
  }

  printf("Sizes:\n  max: %lud\n  used: %lud\n  sorted: %lud\n", kv->max_size, kv->used_size, kv->last_sorted);
  if (kv->used_size == kv->last_sorted) {
    printf("Sorted:  yes\n");
  } else {
    printf("Sorted:  no\n");
  }
  printf("Data array: %p\n", kv->data);
  for (unsigned long i=0;
      i < kv->used_size;
      ++i) {
    struct KeyValElement *e = kv->data[i];
    if (!e) {
      printf("[%03lu=>%p] (at 0)\n", i, &kv->data[i]);
    } else if (e->key == 0 && e->val == 0) {
      printf("[%03lu=>%p] (at %p) %p:'' => %p:''\n", i, &kv->data[i], e, e->key, e->val);
    } else if (e->key == 0) {
      printf("[%03lu=>%p] (at %p) %p:'' => %p:'%s'\n", i, &kv->data[i], e, e->key, e->val, e->val);
    } else if (e->val == 0) {
      printf("[%03lu=>%p] (at %p) %p:'%s' => %p:''\n", i, &kv->data[i], e, e->key, e->key, e->val);
    } else {
      printf("[%03lu=>%p] (at %p) %p:'%s' => %p:'%s'\n", i, &kv->data[i], e, e->key, e->key, e->val, e->val);
    }
  }
}


