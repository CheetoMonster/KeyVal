
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "KeyVal.h"


unsigned char KEYVAL_QUIET = 0;  // only meant for regressions

static const int KEYVAL_MIN_ARRAY_SIZE = 16;
static const int KEYVAL_MAX_STR_LEN = 1024;  // 'str' means key or value
static const int KEYVAL_MAX_INTERP_DEPTH = 25;

// setting 'errno' is usually automatic on malloc fails, but I do it explicitly


//////////////////////////////////////// KeyValElement
/*
static struct KeyValElement* KeyValElement_new() {
  struct KeyValElement *res = malloc(sizeof(struct KeyValElement));
  if (!res) {
    fprintf(stderr, "KeyValElement_new: out of memory\n");
    errno = ENOMEM;
    return 0;
  }
  res->key = 0;
  res->val = 0;
  return res;
}
*/
// Creates a new KeyValElement, and initializes data to a copy of the given
// parameters.
static struct KeyValElement* KeyValElement_new2(const char *key, const char *val) {

  struct KeyValElement *res = malloc(sizeof(struct KeyValElement));
  if (!res) {
    fprintf(stderr, "KeyValElement_new2: out of memory\n");
    errno = ENOMEM;
    return 0;
  }

  res->key = strdup(key);
  if (!res->key) {
    fprintf(stderr, "KeyValElement_new2: out of memory\n");
    free(res);
    errno = ENOMEM;
    return 0;
  }
  res->val = strdup(val);
  if (!res->val) {
    fprintf(stderr, "KeyValElement_new2: out of memory\n");
    free(res->key);
    free(res);
    errno = ENOMEM;
    return 0;
  }
  return res;
}

// Cleans up the given KeyValElement object by deleting its data and then itself.
static void KeyValElement_delete(struct KeyValElement *element) {
  free(element->key); element->key = 0;
  free(element->val); element->val = 0;
  free(element);
  return;
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
// Surprisingly, this is not as dreadfully slow as you'd expect.
//
// I don't really want to export this function, but I do want it tested by
// test.c, so no 'static' for this one.  :/
int KeyVal_strcmp(const char *s1, const char *s2) {

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
      int s1_dc = (*_s1 == ':' && *(_s1+1) == ':');
      int s2_dc = (*_s2 == ':' && *(_s2+1) == ':');
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
//   -1: only if there's a bad error with inputs
//   <int>: index where the given key should exist in the array:
//     - if the key exists, this is where it is;
//     - if the key does not exist, this is where it should be inserted.
// This function is pretty much internal, so it is not declared in KeyVal.h.
// However, it is tested directly in test.c, so it is not static.
int KeyVal_findIdealIndex(struct KeyVal *kv, const char *key) {
  if (!kv) {
    fprintf(stderr, "KeyVal_findIdealIndex: 'kv' argument null\n");
    return -1;
  }
  if (!key) {
    fprintf(stderr, "KeyVal_findIdealIndex: 'key' argument null\n");
    return -1;
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

  int curr_low = 0;
  int curr_hi = kv->used_size;  // can't be - 1; what if ideal is off the end?
  int curr_mid = curr_low;  // in case array is empty
  while (curr_low != curr_hi) {

    // SHR is both a fast divide and a fast floor:
    curr_mid = (curr_low + curr_hi) >> 1;

    if (KeyVal_strcmp(kv->data[curr_mid]->key, key) < 0)
      curr_low = curr_mid + 1;
    else
      curr_hi = curr_mid;
  }
  return curr_low;
}

// Returns:
//   -1: if the key was not found (or if there was a bad input error)
//   <int>: index where the key is
// This function is pretty much internal, so it is not declared in KeyVal.h.
// However, it is tested directly in test.c, so it is not static.
int KeyVal_findIndex(struct KeyVal *kv, const char *key) {
  if (!kv) {
    fprintf(stderr, "KeyVal_findIndex: 'kv' argument null\n");
    return -1;
  }
  if (!key) {
    fprintf(stderr, "KeyVal_findIndex: 'key' argument null\n");
    return -1;
  }

  // find where it 'should' be:
  int idx = KeyVal_findIdealIndex(kv, key);
  if (kv->used_size == idx) return -1;  // ideal is off the end of the array, so it wasn't found
  // check if the key at the ideal index happens to be it:
  if (strcmp(kv->data[idx]->key, key) == 0)
    return idx;
  return -1;
}



//////////////////////////////////////// KeyVal

struct KeyVal* KeyVal_new() {

  // alloc:
  struct KeyVal *res = malloc(sizeof(struct KeyVal));
  if (!res) {
    fprintf(stderr, "KeyVal_new: out of memory\n");
    errno = ENOMEM;
    return 0;
  }

  // init:
  res->max_size = KEYVAL_MIN_ARRAY_SIZE;
  res->used_size = 0;
  res->last_sorted = 0;
  res->data = calloc(KEYVAL_MIN_ARRAY_SIZE, sizeof(struct KeyValElement*));
  if (!res->data) {
    fprintf(stderr, "KeyVal_new: out of memory\n");
    free(res);
    errno = ENOMEM;
    return 0;
  }

  return res;
}


void KeyVal_delete(struct KeyVal *kv) {
  if (!kv) {
    fprintf(stderr, "KeyVal_delete: 'kv' argument null\n");
    return;
  }

  // I have a habit of zero'ing out things after freeing them, so that if
  // anything is still pointing to them, they segfault right away instead
  // picking up essentially random data.  However, there's a good argument
  // to be made that it's unnecessary computation.

  // destroy components:
  for (int i = 0; i < kv->used_size; ++i) {
    KeyValElement_delete(kv->data[i]);
    kv->data[i] = 0;
  }
  // destroy array:
  free(kv->data);
  kv->data = 0;

  // destroy myself:
  free(kv);
  return;
}


// this is a custom function because we need to account for escapes
static int KeyVal_strlen(const char *keystr) {
  int res = 0;
  for (const char *ch = keystr;
      *ch;
      ch++) {
    res += (*ch == '`' || *ch == '\\') ? 2 : 1;
  }
  return res;
}


static void KeyVal_escape_and_quote(char *dest, const char *src) {

  int dest_idx = 0;
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


static char* KeyVal_interp_next(struct KeyVal *kv, const char *str, int depth) {

  // hit KEYVAL_MAX_INTERP_DEPTH number of recursive variables?
  if (depth == KEYVAL_MAX_INTERP_DEPTH) {
    return 0;
  }

  // various error cases return a copy of the original string.  If variables
  // are found, we'll reconstruct the string:
  char *res = strdup(str);
  if (!res) {
    fprintf(stderr, "KeyVal_interp: out of memory\n");
    errno = ENOMEM;
    return 0;
  }

  // find the first close-brace, if there is one:
  char *close_brace = index(res, '}');
  while (close_brace) {

    // trace back to the preceding "${", if there is one:
    char *open_brace = 0;
    char *t = close_brace - 2;
    while (res <= t) {
      if (*t == '$' && *(t+1) == '{') {
        open_brace = t;
        break;
      }
      --t;
    }
    if (!open_brace) {
      return res;
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
      return res;
    }

    // get its (uninterpolated) value:
    const char *subval = KeyVal_getValue(kv, tmpstr, 0);
    if (!subval) {
      return res;
    }

    // interpolate variables in it:
    char *substr = KeyVal_interp_next(kv, subval, depth+1);
    if (!substr) { // propagate errors
      return 0;
    }

    // patch together the new string.
    // copy the pre-variable part:
    tptr = &tmpstr[0];
    t = res;
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
    free(res);
    res = strdup(tmpstr);
    if (!res) {
      fprintf(stderr, "KeyVal_interp: out of memory\n");
      errno = ENOMEM;
      return 0;
    }

    // cleanup the sub-variable memory:
    free(substr);

    // next iteration:
    close_brace = index(res, '}');
  }

  return res;
}


char* KeyVal_interp(struct KeyVal *kv, const char *str) {
  if (!kv) {
    fprintf(stderr, "KeyVal_interp: 'kv' argument is null\n");
    return 0;
  }
  if (!str) {
    fprintf(stderr, "KeyVal_interp: 'str' argument is null\n");
    return 0;
  }

  char *res = KeyVal_interp_next(kv, str, 0);
  if (!res && !KEYVAL_QUIET) {
    fprintf(stderr,
        "[ERROR] KeyVal_interp: encountered %d levels of variables; possible recursion\n"
        "in key `%s`\n",
        KEYVAL_MAX_INTERP_DEPTH, str);
    return 0;
  }
  return res;
}


static void KeyVal_ensureSorted(struct KeyVal *kv) {

  // make sure there's something to sort:
  if (kv->used_size == 0) return;

  // make sure we actually need to sort:
  if (kv->last_sorted == kv->used_size) return;

  // this uses insertion sort!
  int orig_size = kv->used_size;
  kv->used_size = kv->last_sorted;  // so that findIdealIndex works right
  for (int idx = kv->last_sorted;
      idx < orig_size;
      ++idx) {

    const char *key = kv->data[idx]->key;

    int ideal_idx = KeyVal_findIdealIndex(kv, key);

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
      int num_bytes = sizeof(struct KeyValElement*) * (kv->used_size - ideal_idx);
      memmove(dest, src, num_bytes);

      // set the ideal spot to idx's (original) data:
      kv->data[ideal_idx] = newb;
      ++kv->used_size;
    }
  }

  // and now we know it's sorted!
  kv->last_sorted = kv->used_size;

  return;
}


static void KeyVal_resize(struct KeyVal *kv, int new_size) {
  if (!kv) return;

  // do not resize below KEYVAL_MIN_ARRAY_SIZE
  if (new_size < KEYVAL_MIN_ARRAY_SIZE)
    new_size = KEYVAL_MIN_ARRAY_SIZE;

  // don't resize to the same value:
  if (new_size == kv->max_size)
    return;

  kv->max_size = new_size;
  
  // create new array:
  struct KeyValElement **new_data = calloc(kv->max_size, sizeof(struct KeyValElement *));
  if (!new_data) {
    fprintf(stderr, "KeyVal_resize: out of memory\n");
    errno = ENOMEM;
    return;
  }

  // copy over existing things:
  memcpy(new_data,
      kv->data,
      kv->used_size * sizeof(struct KeyValElement *));

  // free up old array:
  free(kv->data);

  // shuffle:
  kv->data = new_data;

  return;
}


int KeyVal_save(struct KeyVal *kv, const char *filepath, unsigned char interp, unsigned char align) {
  if (!kv) {
    fprintf(stderr, "KeyVal_save: 'kv' argument null\n");
    return -1;
  }
  if (!filepath) {
    fprintf(stderr, "KeyVal_save: 'filepath' argument null\n");
    return -1;
  }

  FILE *fh = fopen(filepath, "w");
  if (!fh) {
    fprintf(stderr, "[ERROR] KeyVal_save: cannot write to this file:\n  %s\n  because of:\n  ", filepath);
    perror(0);
    return 1;
  }

  // make sure the database is sane:
  KeyVal_ensureSorted(kv);

  // if we need to align, find the max size of all the keys:
  char fmt_str[16];
  if (align) {
    int max_size = 0;
    for (int i = 0;
        i < kv->used_size;
        ++i) {
      int this_len = KeyVal_strlen(kv->data[i]->key);
      if (max_size < this_len) max_size = this_len;
    }
    sprintf(fmt_str, "%%-%ds = %%s\n", max_size+2);  // "-" is for left-align; "+2" is for the surrounding quotes
  }
  else {
    sprintf(fmt_str, "%%s = %%s\n");
  }

  // the +2 is for the surrounding quote characters:
  char this_key[KEYVAL_MAX_STR_LEN+2];
  char this_val[KEYVAL_MAX_STR_LEN+2];    

  for (int i = 0;
      i < kv->used_size;
      ++i) {
    KeyVal_escape_and_quote(this_key, kv->data[i]->key);
    // may need to interpolate variables in this_val:
    if (interp) {
      char *interped_val = KeyVal_interp(kv, kv->data[i]->val);
      if (!interped_val) return -1; // stderr already spewed
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
    return 1;
  }
  return 0;
}


void KeyVal_setValue(struct KeyVal *kv, const char *key, const char *val) {
  if (!kv) {
    fprintf(stderr, "KeyVal_setValue: 'kv' argument is null\n");
    return;
  }
  if (!key) {
    fprintf(stderr, "KeyVal_setValue: 'key' argument is null\n");
    return;
  }
  if (!val) {
    fprintf(stderr, "KeyVal_setValue: 'val' argument is null\n");
    return;
  }
  int _tmp_len = KeyVal_strlen(key); // use the KeyVal version to account for escapes
  if (KEYVAL_MAX_STR_LEN < _tmp_len) {
    fprintf(stderr, "KeyVal_setValue: 'key' argument too long (%d > %d): '%s'\n", _tmp_len, KEYVAL_MAX_STR_LEN, key);
    return;
  }
  _tmp_len = KeyVal_strlen(val);
  if (KEYVAL_MAX_STR_LEN < _tmp_len) {
    fprintf(stderr, "KeyVal_setValue: 'val' argument too long (%d > %d): '%s'\n", _tmp_len, KEYVAL_MAX_STR_LEN, val);
    return;
  }

  int _need_to_add = 1;

  // base case: nothing in the array at all.
  if (kv->used_size == 0) {
    kv->data[0] = KeyValElement_new2(key, val);
    if (!kv->data[0]) return; // stderr already spewed
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
        KeyVal_resize(kv, kv->max_size*2);
      }
      // add to end:
      kv->data[kv->used_size] = KeyValElement_new2(key, val);
      if (!kv->data[kv->used_size]) return; // stderr already spewed
      ++kv->used_size;
      ++kv->last_sorted;
    }

    // next case: overwrites an existing setting:
    else {
      int ideal_idx = KeyVal_findIdealIndex(kv, key);
      if (!strcmp(kv->data[ideal_idx]->key, key)) {
        _need_to_add = 0;
        free(kv->data[ideal_idx]->val);
        kv->data[ideal_idx]->val = strdup(val);
        if (!kv->data[ideal_idx]->val) {
          fprintf(stderr, "KeyVal_setValue: out of memory\n");
          errno = ENOMEM;
          return;
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
      KeyVal_resize(kv, kv->max_size*2);
    }
    // add to end:
    kv->data[kv->used_size] = KeyValElement_new2(key, val);
    if (!kv->data[kv->used_size]) return; // stderr already spewed
    ++kv->used_size;

    // this does not preserve sorting, so do not increment last_sorted
  }

  return;
}


char* KeyVal_getValue(struct KeyVal *kv, const char *key, int interp) {
  if (!kv) {
    fprintf(stderr, "KeyVal_getValue: 'kv' argument is null\n");
    return 0;
  }
  if (!key) {
    fprintf(stderr, "KeyVal_getValue: 'key' argument is null\n");
    return 0;
  }

  KeyVal_ensureSorted(kv);

  int idx = KeyVal_findIndex(kv, key);
  // not found:
  if (idx == -1) return 0;
  // found, but need to interpolate variables:
  if (interp) {
    char *res = KeyVal_interp(kv, kv->data[idx]->val);
    if (!res) return 0;  // stderr already spewed
    return res;
  }
  // found, with no interpolation:
  char *res = strdup(kv->data[idx]->val);
  if (!res) {
    fprintf(stderr, "KeyVal_getValue: out of memory\n");
    errno = ENOMEM;
    return 0;
  }
  return res;
}


void KeyVal_remove(struct KeyVal *kv, const char *key) {
  if (!kv) {
    fprintf(stderr, "KeyVal_remove: 'kv' argument null\n");
    return;
  }
  if (!key) {
    fprintf(stderr, "KeyVal_remove: 'key' argument null\n");
    return;
  }

  // database must be sane first:
  KeyVal_ensureSorted(kv);

  // where is it?
  int idx = KeyVal_findIndex(kv, key);
  if (idx == -1) return;  // not in array at all

  // delete it:
  KeyValElement_delete(kv->data[idx]);

  // if it's the last one, nothing to move:
  if (idx == kv->used_size - 1) {
    // (this space for rent)
  }
  // otherwise, move everything up one:
  else {
    void *src = &kv->data[idx+1];
    void *dest = &kv->data[idx];
    int num_bytes = sizeof(struct KeyValElement*)*(kv->used_size - idx - 1);
    memmove(dest, src, num_bytes);
  }

  // decrement counters:
  --kv->used_size;
  --kv->last_sorted;

  // should probably automatically resize, though it's not strictly necessary:
  // (the "- 2" is so that a tight loop of add+remove doesn't resize on every
  // single call)
  if (kv->used_size < kv->max_size / 2 - 2)
    KeyVal_resize(kv, kv->max_size/2);

  return;
}



static int KeyVal_has_subkey(const char *base, const char *extended, int base_len) {

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
//  int prev_was_colon = 0;
  while (src[offset]) {  // stop if we hit end-of-string
//    if (prev_was_colon && src[offset] == ':') {
//      --dest_idx; // back up to not include the first one
//      break;  // done
//    }

//    prev_was_colon = (src[offset] == ':');

    // stop if we hit another double-colon:
    if (src[offset]==':' && src[offset+1]==':')
      break;

    dest[dest_idx++] = src[offset++];
  }
  dest[dest_idx] = 0;

  return;
}


char ** KeyVal_getKeys(struct KeyVal *kv, const char *path) {
  if (!kv) {
    fprintf(stderr, "KeyVal_getKeys: 'kv' argument null\n");
    return 0;
  }
  if (!path) {
    fprintf(stderr, "KeyVal_getKeys: 'path' argument null\n");
    return 0;
  }

  KeyVal_ensureSorted(kv);

  int path_len = strlen(path);
  int start_of_subkey;
  int data_start_idx;
  int data_end_idx;
  if (path_len == 0) {
    // this is an easy case:
    data_start_idx = 0;
    data_end_idx = kv->used_size;
    start_of_subkey = path_len;  // start at beginning
  }
  else {

    // the "ideal spot" for this path is where we'll start looking:
    data_start_idx = KeyVal_findIdealIndex(kv, path);

    // if it's off the end, we're done:
    if (kv->used_size <= data_start_idx) {
      char **res = malloc(1 * sizeof(char*));
      if (!res) {
        fprintf(stderr, "KeyVal_getKeys: out of memory\n");
        errno = ENOMEM;
        return 0;
      }
      res[0] = 0;
      return res;
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
    char **res = malloc(1 * sizeof(char*));
    if (!res) {
      fprintf(stderr, "KeyVal_getKeys: out of memory\n");
      errno = ENOMEM;
      return 0;
    }
    res[0] = 0;
    return res;
  }

  // now we can alloc, though this is the upper bound of the number of keys:
  char **res = malloc((data_end_idx - data_start_idx + 1) * sizeof(char*));
  if (!res) {
    fprintf(stderr, "KeyVal_getKeys: out of memory\n");
    errno = ENOMEM;
    return 0;
  }


  // go back and add the keys to the array:
  char prev_subkey[KEYVAL_MAX_STR_LEN]; prev_subkey[0] = 0;
  char this_subkey[KEYVAL_MAX_STR_LEN];
  int num_unique_keys = 0;  // not necessarily the same as "data_end_idx - data_start_idx"
  for (int i = data_start_idx;
      i < data_end_idx;
      ++i) {
    KeyVal_extract_subkey(this_subkey, kv->data[i]->key, start_of_subkey);
    if (strcmp(prev_subkey, this_subkey)) {
      // different, so it's a new key -- add to res:
      res[num_unique_keys] = strdup(this_subkey);
      if (!res[num_unique_keys]) {
        fprintf(stderr, "KeyVal_getKeys: out of memory\n");
        free(res);
        errno = ENOMEM;
        return 0;
      }
      ++num_unique_keys;
      strcpy(prev_subkey, this_subkey);
    }
  }
  res[num_unique_keys] = 0;

  // realloc back down if we're overconsuming memory:
  if (num_unique_keys != data_end_idx - data_start_idx) {
    // inexplicably, realloc may move 'res' to a completely different place:
    res = realloc(res, (num_unique_keys+1)*sizeof(char*));
    if (!res) {
      fprintf(stderr, "KeyVal_getKeys: out of memory\n");
      errno = ENOMEM;
      return 0;
    }
  }

  return res;
}

char** KeyVal_getAllKeys(struct KeyVal *kv) {
  if (!kv) {
    fprintf(stderr, "KeyVal_getAllKeys: 'kv' argument null\n");
    return 0;
  }

  // database must be sane:
  KeyVal_ensureSorted(kv);

  // then just copy off all the keys:
  char **res = malloc(sizeof(char*) * (kv->used_size + 1));
  if (!res) {
    fprintf(stderr, "KeyVal_getAllKeys: out of memory\n");
    errno = ENOMEM;
    return 0;
  }
  for (int idx=0;
      idx<kv->used_size;
      ++idx) {
    res[idx] = strdup(kv->data[idx]->key);
    if (!res[idx]) {
      fprintf(stderr, "KeyVal_getAllKeys: out of memory\n");
      free(res);
      errno = ENOMEM;
      return 0;
    }
  }
  res[kv->used_size] = 0;
  return res;
}




int KeyVal_size(struct KeyVal *kv) {
  if (!kv) {
    fprintf(stderr, "KeyVal_size: 'kv' argument null\n");
    return 0;
  }

  // need to ensureSorted because unsorted arrays may have duplicates that we
  // don't want to count:
  KeyVal_ensureSorted(kv);
  return kv->used_size;
}


int KeyVal_hasValue(struct KeyVal *kv, const char *key) {
  if (!kv) {
    fprintf(stderr, "KeyVal_hasValue: 'kv' argument null\n");
    return 0;
  }
  if (!key) {
    fprintf(stderr, "KeyVal_hasValue: 'key' argument null\n");
    return 0;
  }

  KeyVal_ensureSorted(kv);

  int idx = KeyVal_findIdealIndex(kv, key);
  // check if it's even in the array:
  if (idx == kv->used_size)
    return 0;
  // return if the key at the ideal index happens to be it:
  return strcmp(kv->data[idx]->key, key) == 0;
}


int KeyVal_hasKeys(struct KeyVal *kv, const char *path) {
  if (!kv) {
    fprintf(stderr, "KeyVal_hasKeys: 'kv' argument null\n");
    return 0;
  }
  if (!path) {
    fprintf(stderr, "KeyVal_hasKeys: 'path' argument null\n");
    return 0;
  }

  KeyVal_ensureSorted(kv);

  // the "ideal spot" for this path is where we'll start looking:
  int idx = KeyVal_findIdealIndex(kv, path);

  // if it's off the end, we're done:
  if (kv->used_size <= idx)
    return 0;

  // what did we find at idx?
  if (!strcmp(kv->data[idx]->key, path)) {
    // exact match, so the path is itself a valid key.  Which we skip, in case
    // the next one has keys:
    ++idx;
  }

  // check to see if we have a subkey of this path:
  if (KeyVal_has_subkey(path, kv->data[idx]->key, strlen(path)))
    return 1;

  return 0;
}


int KeyVal_exists(struct KeyVal *kv, const char *key_or_path) {
  if (!kv) {
    fprintf(stderr, "KeyVal_exists: 'kv' argument null\n");
    return 0;
  }
  if (!key_or_path) {
    fprintf(stderr, "KeyVal_exists: 'key_or_path' argument null\n");
    return 0;
  }

  // this is essentially the hasValue and hasKeys functions mashed together.

  KeyVal_ensureSorted(kv);

  // the "ideal spot" for this path is where we'll start looking:
  int idx = KeyVal_findIdealIndex(kv, key_or_path);

  // if it's off the end, we're done:
  if (kv->used_size <= idx)
    return 0;

  // what did we find at idx?
  if (!strcmp(kv->data[idx]->key, key_or_path)) {
    // exact match, which means it has a value:
    return 1;
  }

  // check to see if we have a subkey of this path:
  if (KeyVal_has_subkey(key_or_path, kv->data[idx]->key, strlen(key_or_path)))
    return 1;

  return 0;
}




//////////////////////////////////////// debugging

void KeyVal_print(struct KeyVal *kv) {
  if (!kv) {
    printf("KeyVal_print: array is null\n");
    return;
  }

  printf("Sizes:\n  max: %d\n  used: %d\n  sorted: %d\n", kv->max_size, kv->used_size, kv->last_sorted);
  if (kv->used_size == kv->last_sorted)
    printf("Sorted:  yes\n");
  else
    printf("Sorted:  no\n");
  printf("Data array: %p\n", kv->data);
  for (int i=0;
      i < kv->used_size;
      ++i) {
    struct KeyValElement *e = kv->data[i];
    if (!e)
      printf("[%03i=>%p] (at 0)\n", i, &kv->data[i]);
    else if (e->key == 0 && e->val == 0)
      printf("[%03i=>%p] (at %p) %p:'' => %p:''\n", i, &kv->data[i], e, e->key, e->val);
    else if (e->key == 0)
      printf("[%03i=>%p] (at %p) %p:'' => %p:'%s'\n", i, &kv->data[i], e, e->key, e->val, e->val);
    else if (e->val == 0)
      printf("[%03i=>%p] (at %p) %p:'%s' => %p:''\n", i, &kv->data[i], e, e->key, e->key, e->val);
    else
      printf("[%03i=>%p] (at %p) %p:'%s' => %p:'%s'\n", i, &kv->data[i], e, e->key, e->key, e->val, e->val);
  }
}


