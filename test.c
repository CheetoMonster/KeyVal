#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "KeyVal.h"

/* TODO

- perl/python/tcl frontends

*/


static int test_passes = 0;
static int test_fails = 0;
static const char *IN = "/tmp/keyval.test.in";
static const char *OUT = "/tmp/keyval.test.out";


// declare functions I want to test here, but didn't want to include in the header file:
char* KeyVal_interp(struct KeyVal *kv, const char *str);
int   KeyVal_findIdealIndex(struct KeyVal *kv, const char *key);
int   KeyVal_findIndex(struct KeyVal *kv, const char *key);



static int
_check_load_return(struct KeyVal *kv, int expected_return, const char *msg) {
  if (KeyVal_load(kv, IN) == expected_return) {
    ++test_passes;
    printf("[test-ok] %s\n", msg);
    return 1;  // ok
  }
  else {
    ++test_fails;
    printf("[test-ERROR] %s\n", msg);
    return 0;  // not ok
  }
}


static int
ok(int is_true, const char *msg) {
  if (is_true) {
    ++test_passes;
    printf("[test-ok] %s\n", msg);
    return 1;  // ok
  }
  else {
    ++test_fails;
    printf("[test-ERROR] %s\n", msg);
    return 0;  // not ok
  }
}



static void
_set_input(const char *contents) {
  FILE *fh = fopen(IN, "w");
  fprintf(fh, "%s", contents);
  fclose(fh);
  return;
}

static int
_check_output(const char *expected) {
  char buf[1024];  // think that's all I'll need here
  buf[0] = 0;  // needed for the empty-file case
  FILE *fh = fopen(OUT, "r");
  if (!fh) return -1;
  int count = fread(buf, 1, 1024, fh);
//printf("** count: %d\n", count);
  buf[count] = 0;
  fclose(fh);
//printf("** string from %s: '%s'\n", OUT, buf);
//printf("** string from param: '%s'\n", expected);
  return strcmp(buf, expected);  // so "0" means "match"
}


static void
test1() {
  
  struct KeyVal *kv = KeyVal_new();

  // 1a: missing file
  remove(IN);
  _check_load_return(kv, 2, "1a. detects missing input file");

  // 1b: file with complete crap in it
  _set_input("this file has complete crap in it\n");
  _check_load_return(kv, 1, "1b. detects complete crap");

  // 1c: file with EOF in key
  _set_input("`key");
  _check_load_return(kv, 1, "1c. detects EOF in key");

  // 1d/1e: file with missing "=" or "remove"
  _set_input("`key`\n");
  _check_load_return(kv, 1, "1d. detects '\\n' instead of '=' or 'remove'");
  _set_input("`key`");
  _check_load_return(kv, 1, "1e. detects EOF instead of '=' or 'remove'");

  // 1f: file with "foo" instead of "=" or "remove"
  _set_input("`key` foo");
  _check_load_return(kv, 1, "1f. detects crap instead of '=' or 'remove'");

  // 1g/1h: file with missing value
  _set_input("`key` =\n");
  _check_load_return(kv, 1, "1g. detects '\\n' instead of value");
  _set_input("`key` =");
  _check_load_return(kv, 1, "1h. detects EOF instead of value");

  // 1i: file with EOF in value
  _set_input("`key` = `val");
  _check_load_return(kv, 1, "1i. detects EOF in value (missing close-quote)");

  // 1j: file with comment after value (sorry, not allowed!)
  _set_input("`key` = `val` #moo!");
  _check_load_return(kv, 1, "1j. detects comment after value");

  KeyVal_delete(kv);
}


static void
test2() {
  struct KeyVal *kv = KeyVal_new();
  // 2a: file with nothing in it
  FILE *fh = fopen(IN, "w");
  fclose(fh);
  _check_load_return(kv, 0, "2a. empty input file");

  // 2b: file with only whitespace
  _set_input("  \t \t  \n\t\t  \n\n ");  // also tests missing \n at EOF
  _check_load_return(kv, 0, "2b. whitespace-only input file");

  // 2c: file with only comments
  _set_input("# nothing\n# at all!\n");
  _check_load_return(kv, 0, "2c. comment-only input file");

  // 2d: save it out; should get nothing
  KeyVal_save(kv, OUT, 0, 0);
  ok(_check_output("") == 0, "2d. writing out empty input generates empty output");

  KeyVal_delete(kv);
}

static void
test3() {
  struct KeyVal *kv = KeyVal_new();

  // 3a: unloaded kv should be empty:
  ok(KeyVal_size(kv) == 0, "3a. uninitialized KeyVal empty");
  // 3b: single-element file
  _set_input("`key` = `val`\n");  // minimal typical input
  if (!ok(KeyVal_load(kv, IN) == 0, "3b. reads single-element input")) return;
  // 3c: size set from file input
  ok(KeyVal_size(kv) == 1, "3c. size of single-element input is 1");

  // 3d: search for element before it:
  ok(KeyVal_getValue(kv, "asdf", 0) == 0, "3d. search for nonexistent 'asdf'");
  // 3e: search for element after it:
  ok(KeyVal_getValue(kv, "zxcv", 0) == 0, "3e. search for nonexistent 'zxcv'");

  // 3f: search for it itself:
  const char *val = KeyVal_getValue(kv, "key", 0);
  if (ok(val != 0, "3f. search for existing 'key'")) {
    // 3g: has the right value:
    ok(strcmp(val, "val") == 0, "3g. 'key' has value 'val'");
  }

  // 3h: search for subset key:
  ok(KeyVal_getValue(kv, "ke", 0) == 0, "3h. search for subset 'ke'");
  // 3i: search for superset key:
  ok(KeyVal_getValue(kv, "keysha", 0) == 0, "3i. search for superset 'keysha'");


  // 3j: getKeys returns one thing
  char ** keys = KeyVal_getKeys(kv, "");
  if (keys) {
    if (!ok(keys[0]!=0 && !strcmp(keys[0], "key") && keys[1]==0, "3j. getKeys returns ['key']")) {
      printf("keys returned:\n");
      for (char **f = keys;
          *f;
          ++f) {
        printf("  '%s'\n", *f);
      }
    }
    for (char **f = keys;
        *f;
        ++f) {
      free(*f);
    }
    free(keys);
  }
  else {
    printf("[UNEXPECTED-1] getKeys should never return a null result");
  }


  // 3k: save it out; should get single value
  KeyVal_save(kv, OUT, 0, 0);
  ok(_check_output("`key` = `val`\n") == 0, "3k. writes out the single value");


  // 3l: removing element before:
  KeyVal_remove(kv, "asdf");
  ok(KeyVal_size(kv) == 1, "3l. removing nonexistent 'asdf'");
  // 3m: removing element after:
  KeyVal_remove(kv, "zxcv");
  ok(KeyVal_size(kv) == 1, "3m. removing nonexistent 'zxcv'");
  // 3n: removing element:
  KeyVal_remove(kv, "key");
  ok(KeyVal_size(kv) == 0, "3n. removing 'key'");

  // 3o: getKeys returns nothing
  keys = KeyVal_getKeys(kv, "");
  if (keys) {
    ok(keys[0] == 0, "3o: getKeys now returns ['']");
    free(keys);
  }
  else {
    printf("[UNEXPECTED-2] getKeys should never return a null result");
  }

  // 3p: save it out; should get nothing now
  KeyVal_save(kv, OUT, 0, 0);
  ok(_check_output("") == 0, "3p. writes out empty file");

  KeyVal_delete(kv);
}


static void
test4() {
  struct KeyVal *kv = KeyVal_new();
  // intentionally not in alphabetic order:
  KeyVal_setValue(kv, "seagram's", "7");
  KeyVal_setValue(kv, "jack", "daniel's");

  // 4a. size set from setValues:
  ok(KeyVal_size(kv) == 2, "4a. size=2");

  // 4b: search for first existing:
  const char *val = KeyVal_getValue(kv, "jack", 0);
  if (ok(val != 0, "4b. search for existing 'jack'")) {
    // 4c: has the right value:
    ok(strcmp(val, "daniel's") == 0, "4c. jack => daniel's");
  }

  // 4d: search for non-existing key that would be in the middle of existing ones
  ok(KeyVal_getValue(kv, "jim::beam", 0) == 0, "4d. checking for mid-value nonexistent key");

  // 4e: search for last existing:
  val = KeyVal_getValue(kv, "seagram's", 0);
  if (ok(val != 0, "4e. search for existing 'seagram's'")) {
    // 4f: has the right value:
    ok(strcmp(val, "7") == 0, "4f. seagram's => 7");
  }

  // 4g: save it out; should get two lines in order
  KeyVal_save(kv, OUT, 0, 0);
  ok(_check_output("`jack` = `daniel's`\n`seagram's` = `7`\n") == 0, "4g. writes out correctly");

  // 4h: remove one; should still have the other
  KeyVal_remove(kv, "seagram's");
  ok(KeyVal_size(kv) == 1, "4h. removing 'seagram's'");
  val = KeyVal_getValue(kv, "jack", 0);
  // 4i: removing one didn't affect the other's value, either:
  if (ok(val != 0, "4i. 'jack' ok after removing seagram's")) {
    // 4j: has the right value:
    ok(strcmp(val, "daniel's") == 0, "4j. jack => daniel's, still");
  }

  KeyVal_delete(kv);
}


static void
test5() {

  struct KeyVal *kv = KeyVal_new();

  // 5a: key and val with escaped quote.  Input should look like this:
  // `\`key\\` = `\\val\``
  // `key\`\`key` = `val\\\\val`
  const char *TEXT = "`\\`key\\\\` = `\\\\val\\``\n`key\\`\\`key` = `val\\\\\\\\val`\n";
  _set_input(TEXT);
  if (!ok(KeyVal_load(kv, IN) == 0, "5a. reads escapes ok")) return;

  // 5b. escapes on the outside of key:
  const char *val = KeyVal_getValue(kv, "`key\\", 0);
  if (ok(val != 0, "5b. key with escapes at start and end")) {

    // 5c. escapes on the outsides of value:
    ok(strcmp(val, "\\val`") == 0, "5c. value with escapes at start and end");
  }

  // 5d. (consecutive) escapes in the middle of key:
  val = KeyVal_getValue(kv, "key``key", 0);
  if (ok(val != 0, "5d. key with consecutive escapes in middle")) {

    // 5e. (consecutive) escapes in the middle of value:
    ok(strcmp(val, "val\\\\val") == 0, "5e. value with consecutive escapes in middle");
  }

  // 5f. write out this nastiness:
  KeyVal_save(kv, OUT, 0, 0);
  ok(_check_output(TEXT) == 0, "5f. escapes write out correctly");

  KeyVal_delete(kv);
}


static void
reverse(char *dest, const char *src) {
  
  int len = strlen(src);
  for (int i = 0; i < len; ++i) {
    dest[i] = src[len - i - 1];
  }
  dest[len] = 0;
  return;
}


static void
test6() {
  // create million-line input:
  // - first key is alphabetic
  // - optional second key is numeric
  // - third key is also numeric because I ran out of ideas
  // - value is just the key backwards

  FILE *fh = fopen(IN, "w");
  if (!fh) {
    printf("[ERROR] cannot write to %s!\n", IN);
    ++test_fails;
    return;
  }

  char key[1024];
  char val[1024];
  char tmp[16];  // this is a surprisingly effective speedup
  // foreach first-key in [a..z]_level:
  for (char a = 'a'; a <= 'z'; ++a) {
//for (char a = 'a'; a <= 'a'; ++a) {

    sprintf(tmp, "%c%s", a, "_level");
    // foreach second-key in none, 1..9:
    for (int b = 10; b >= 0; --b) {  // out of order to test sorting
      if (!b) {
        reverse(val, tmp);
        fprintf(fh, "`%s` = `%s`\n", tmp, val);
        continue;
      }
      // foreach third-key in none, 1..3847: (to make it ~million lines)
      for (int c = 0; c < 3847; ++c) {
//for (int c = 0; c < 1; ++c) {
        if (!c)
          sprintf(key, "%s::%d", tmp, b);
        else
          sprintf(key, "%s::%d::%d", tmp, b, c);
        reverse(val, key);
        fprintf(fh, "`%s` = `%s`\n", key, val);
      }
    }
  }
  fclose(fh);


  struct KeyVal *kv = KeyVal_new();

  // 6a: reads the huge input (update for later: may want to report load-time?)
  //printf("loading huge file..\n");
  if (!ok(KeyVal_load(kv, IN) == 0, "6a. reads big input")) return;
//printf("done!!\n");

  // 6b: size is right
  if (!ok(KeyVal_size(kv) == 1000246, "6b. big input has 1,000,246 settings"))
    printf("  -> thinks it has %d settings\n", KeyVal_size(kv));


  char **keys = KeyVal_getKeys(kv, "b_level");
  if (keys) {
    // count them; should be 11, which means it returns downstream sub-keys only once
    int num_keys = 0;
    for (char **ptr = keys; *ptr; ++ptr) { ++num_keys; }
    if (ok(num_keys == 10, "6c. number of keys under 'b_level' is exactly 10")) {

      // I guess make sure they are what I think they should be.  Probably overkill:
      ok(strcmp(keys[0], "1") == 0, "6e. keys[0]=1");
      ok(strcmp(keys[1], "10") == 0, "6f. keys[1]=10");
      ok(strcmp(keys[2], "2") == 0, "6g. keys[2]=2");
      ok(strcmp(keys[3], "3") == 0, "6h. keys[3]=3");
      ok(strcmp(keys[4], "4") == 0, "6i. keys[4]=4");
      ok(strcmp(keys[5], "5") == 0, "6j. keys[5]=5");
      ok(strcmp(keys[6], "6") == 0, "6k. keys[6]=6");
      ok(strcmp(keys[7], "7") == 0, "6l. keys[7]=7");
      ok(strcmp(keys[8], "8") == 0, "6m. keys[8]=8");
      ok(strcmp(keys[9], "9") == 0, "6n. keys[9]=9");
    }
    for (char **ptr = keys; *ptr; ++ptr) { free(*ptr); }
    free(keys);
  }
  else {
    printf("[UNEXPECTED-3] getKeys should never return a null result\n");
  }

  // check that getAllKeys at least doesn't do nothing:
  keys = KeyVal_getAllKeys(kv);
  if (keys) {
    int count = 0;
    while (keys[count]) {
      free(keys[count]);
      ++count;
    }
    free(keys);
    if (!ok(count == 1000246, "6o. getAllKeys returns 1,000,246 keys"))
      printf("  -> thinks it has %d settings\n", count);
  }
  else {
    printf("[UNEXPECTED-4] getAllKeys should never return a null result\n");
  }
    
  // random getValue, just 'cuz:
  char *v = KeyVal_getValue(kv, "o_level::3::1500", 0);
  if (ok(v != 0, "6p. random key in the middle exists")) {
    ok(strcmp(v, "0051::3::level_o") == 0,
        "6q. random key has the right value");
  }

  // hasValue:
  ok(KeyVal_hasValue(kv, "c_level::2::42"), "6q. hasValue on value");
  KeyVal_remove(kv, "b_level::1");  // need a strict sub-key to test [6r]
  ok(!KeyVal_hasValue(kv, "b_level::1"), "6r. hasValue on keys");
  ok(!KeyVal_hasValue(kv, "c_level::moo"), "6s. hasValue on nothing");

  // hasKeys:
  ok(!KeyVal_hasKeys(kv, "c_level::2::42"), "6t. hasKeys on value");
  ok(KeyVal_hasKeys(kv, "c_level::2"), "6u. hasKeys on keys");
  ok(!KeyVal_hasKeys(kv, "c_level::moo"), "6v. hasKeys on nothing");

  // exists::
  ok(KeyVal_exists(kv, "c_level::2::42"), "6w. exists on value");
  ok(KeyVal_exists(kv, "c_level::2"), "6x. exists on keys");
  ok(!KeyVal_exists(kv, "c_level::moo"), "6y. exists on nothing");


//KeyVal_save(kv, OUT, 0, 0);  //DEBUG
//printf("--saved to '%s'\n", OUT);

  KeyVal_delete(kv);
}

static void
test7() {
  struct KeyVal *kv = KeyVal_new();

  // empty arrays:
  ok(KeyVal_findIdealIndex(kv, "a") == 0, "7a. findIdealIndex ok on empty array");
  ok(KeyVal_findIndex(kv, "a") == -1, "7b. findIndex ok on empty array");
  //ok(KeyVal_find(kv, "a") == 0, "7c. find ok on empty array");

  // one element:
  KeyVal_setValue(kv, "2", "asdf");
  // searching something that should go in front:
  ok(KeyVal_findIdealIndex(kv, "1") == 0, "7d. findIdealIndex-1 ok on array1");
  ok(KeyVal_findIndex(kv, "1") == -1, "7e. findIndex-1 ok on array1");
  //ok(KeyVal_find(kv, "1") == 0, "7f. find-1 ok on array1");
  // searching the one thing that's in it:
  ok(KeyVal_findIdealIndex(kv, "2") == 0, "7g. findIdealIndex-2 ok on array1");
  ok(KeyVal_findIndex(kv, "2") == 0, "7h. findIndex-2 ok on array1");
  //ok(KeyVal_find(kv, "2") != 0, "7i. find-2 ok on array1");
  // searching something that should go at the end:
  ok(KeyVal_findIdealIndex(kv, "3") == 1, "7j. findIdealIndex-3 ok on array1");
  ok(KeyVal_findIndex(kv, "3") == -1, "7k. findIndex-3 ok on array1");
  //ok(KeyVal_find(kv, "3") == 0, "7l. find-3 ok on array1");

  // two elements:
  KeyVal_setValue(kv, "4", "asdf");
  // searching something that should go in front:
  ok(KeyVal_findIdealIndex(kv, "1") == 0, "7m. findIdealIndex-1 ok on array2");
  ok(KeyVal_findIndex(kv, "1") == -1, "7n. findIndex-1 ok on array2");
  //ok(KeyVal_find(kv, "1") == 0, "7o. find-1 ok on array2");
  // searching the first thing that's in it:
  ok(KeyVal_findIdealIndex(kv, "2") == 0, "7p. findIdealIndex-2 ok on array2");
  ok(KeyVal_findIndex(kv, "2") == 0, "7q. findIndex-2 ok on array2");
  //ok(KeyVal_find(kv, "2") != 0, "7r. find-2 ok on array2");
  // searching something that should go in the middle:
  ok(KeyVal_findIdealIndex(kv, "3") == 1, "7s. findIdealIndex-3 ok on array2");
  ok(KeyVal_findIndex(kv, "3") == -1, "7t. findIndex-3 ok on array2");
  //ok(KeyVal_find(kv, "3") == 0, "7u. find-3 ok on array2");
  // searching the second thing that's in it:
  ok(KeyVal_findIdealIndex(kv, "4") == 1, "7v. findIdealIndex-4 ok on array2");
  ok(KeyVal_findIndex(kv, "4") == 1, "7w. findIndex-4 ok on array2");
  //ok(KeyVal_find(kv, "4") != 0, "7x. find-4 ok on array2");
  // searching something that should go at the end:
  ok(KeyVal_findIdealIndex(kv, "5") == 2, "7y. findIdealIndex-5 ok on array2");
  ok(KeyVal_findIndex(kv, "5") == -1, "7z. findIndex-5 ok on array2");
  //ok(KeyVal_find(kv, "5") == 0, "7aa. find-5 ok on array2");

  // three elements:
  KeyVal_setValue(kv, "6", "asdf");
  // searching something that should go in front:
  ok(KeyVal_findIdealIndex(kv, "1") == 0, "7ab. findIdealIndex-1 ok on array3");
  ok(KeyVal_findIndex(kv, "1") == -1, "7ac. findIndex-1 ok on array3");
  //ok(KeyVal_find(kv, "1") == 0, "7ad. find-1 ok on array3");
  // searching the first thing that's in it:
  ok(KeyVal_findIdealIndex(kv, "2") == 0, "7ae. findIdealIndex-2 ok on array3");
  ok(KeyVal_findIndex(kv, "2") == 0, "7af. findIndex-2 ok on array3");
  //ok(KeyVal_find(kv, "2") != 0, "7ag. find-2 ok on array3");
  // searching something that should go in the middle:
  ok(KeyVal_findIdealIndex(kv, "3") == 1, "7ah. findIdealIndex-3 ok on array3");
  ok(KeyVal_findIndex(kv, "3") == -1, "7ai. findIndex-3 ok on array3");
  //ok(KeyVal_find(kv, "3") == 0, "7aj. find-3 ok on array3");
  // searching the second thing that's in it:
  ok(KeyVal_findIdealIndex(kv, "4") == 1, "7ak. findIdealIndex-4 ok on array3");
  ok(KeyVal_findIndex(kv, "4") == 1, "7al. findIndex-4 ok on array3");
  //ok(KeyVal_find(kv, "4") != 0, "7am. find-4 ok on array3");
  // searching something that should go in the middle:
  ok(KeyVal_findIdealIndex(kv, "5") == 2, "7an. findIdealIndex-5 ok on array3");
  ok(KeyVal_findIndex(kv, "5") == -1, "7ao. findIndex-5 ok on array3");
  //ok(KeyVal_find(kv, "5") == 0, "7ap. find-5 ok on array3");
  // searching the second thing that's in it:
  ok(KeyVal_findIdealIndex(kv, "6") == 2, "7aq. findIdealIndex-6 ok on array3");
  ok(KeyVal_findIndex(kv, "6") == 2, "7ar. findIndex-6 ok on array3");
  //ok(KeyVal_find(kv, "6") != 0, "7as. find-6 ok on array3");
  // searching something that should go at the end:
  ok(KeyVal_findIdealIndex(kv, "7") == 3, "7at. findIdealIndex-7 ok on array3");
  ok(KeyVal_findIndex(kv, "7") == -1, "7au. findIndex-7 ok on array3");
  //ok(KeyVal_find(kv, "7") == 0, "7av. find-7 ok on array3");

  // we'll assume all larger cases successfully degrade into one of the above :)

  KeyVal_delete(kv);
}

static void test8() {
  struct KeyVal *kv = KeyVal_new();

  KeyVal_setValue(kv, "k1", "2");
  KeyVal_setValue(kv, "k2", "asdf${k1}zxcv");  // in middle
  KeyVal_setValue(kv, "k3", "${k1}zxcv");  // at beginning
  KeyVal_setValue(kv, "k4", "asdf${k1}");  // at end
  KeyVal_setValue(kv, "k5", "a${k1}b${k1}${k1}c");  // multiple values
  KeyVal_setValue(kv, "k6", "${k${k1}}");   // embedded variables
  KeyVal_setValue(kv, "k7", "}${");   // weird string 1
  KeyVal_setValue(kv, "k8", "${}");   // weird string 2
  KeyVal_setValue(kv, "k9", "${k10}");   // mutually recursive variables
  KeyVal_setValue(kv, "k10", "${k9}");   // mutually recursive variables

  char *val = KeyVal_getValue(kv, "k2", 1);
  if (!ok(strcmp(val, "asdf2zxcv") == 0, "8a. variable in middle of string")) {
    printf(" - got '%s' instead of 'asdf2zxcv'\n", val);
  }
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k3", 1);
  ok(strcmp(val, "2zxcv") == 0, "8b. variable at beginning of string");
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k4", 1);
  ok(strcmp(val, "asdf2") == 0, "8c. variable at end of string");
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k5", 1);
  ok(strcmp(val, "a2b22c") == 0, "8d. multiple variables in the string");
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k6", 1);
  ok(strcmp(val, "asdf2zxcv") == 0, "8e. embedded variables");
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k7", 1);
  ok(strcmp(val, "}${") == 0, "8f. weird initial closing brace");
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k8", 1);
  ok(strcmp(val, "${}") == 0, "8g. empty variable name");
  free(val);  // because we interpolate vars

  val = KeyVal_getValue(kv, "k9", 1);
  ok(val == 0, "8h. recursive variables detected");

  KeyVal_delete(kv);
}

static void test9() {
  struct KeyVal *kv = KeyVal_new();

  KeyVal_setValue(kv, "k1", "k3");            // sets k1 to k3
  KeyVal_setValue(kv, "k2::k3", "${k1}");     // sets k2::k3 to "k3"
  KeyVal_setValue(kv, "k4", "${k2::${k1}}");  // sets k4 to "k3"

  // baseline writing:
  KeyVal_save(kv, OUT, 0, 0);
  ok(_check_output("`k1` = `k3`\n`k2::k3` = `${k1}`\n`k4` = `${k2::${k1}}`\n") == 0, "9a. saving with interp=0, align=0");

  // writing with just interp:
  KeyVal_save(kv, OUT, 1, 0);
  ok(_check_output("`k1` = `k3`\n`k2::k3` = `k3`\n`k4` = `k3`\n") == 0, "9b. saving with interp=1, align=0");

  // writing with just align:
  KeyVal_save(kv, OUT, 0, 1);
  ok(_check_output("`k1`     = `k3`\n`k2::k3` = `${k1}`\n`k4`     = `${k2::${k1}}`\n") == 0, "9c. saving with interp=0, align=1");

  // writing with both interp and align:
  KeyVal_save(kv, OUT, 1, 1);
  ok(_check_output("`k1`     = `k3`\n`k2::k3` = `k3`\n`k4`     = `k3`\n") == 0, "9d. saving with interp=1, align=1");

  KeyVal_delete(kv);
}


int KeyVal_strcmp(const char *s1, const char *s2);
static void test10() {

  // KeyVal_strcmp deserves some white-box testing because it's critical to
  // making getKeys work correctly, and because we mix it with the real strcmp.

  // All of these tests are done twice, to test the symmetry between 's1' and 's2'.

  // base case:
  ok(KeyVal_strcmp("a", "a") == 0, "10a. a == a");
  ok(KeyVal_strcmp("a", "b") < 0, "10b. a < b");
  ok(KeyVal_strcmp("b", "a") > 0, "10c. b > a");
  ok(KeyVal_strcmp("a", "aa") < 0, "10d. a < aa");
  ok(KeyVal_strcmp("aa", "a") > 0, "10e. aa > a");

  // pathology:
  ok(KeyVal_strcmp("", "foo") < 0, "10f. '' < foo");
  ok(KeyVal_strcmp("foo", "") > 0, "10g. foo > ''");
  ok(KeyVal_strcmp("", "") == 0, "10h. '' == ''");

  // normal cases with separators:
  ok(KeyVal_strcmp("foo::1", "foo::10") < 0, "10i. foo::1 < foo::10");
  ok(KeyVal_strcmp("foo::10", "foo::1") > 0, "10j. foo::10 > foo::1");
  ok(KeyVal_strcmp("foo::1", "foo::1::bar") < 0, "10k. foo::1 < foo::1::bar");
  ok(KeyVal_strcmp("foo::1::bar", "foo::1") > 0, "10l. foo::1::bar > foo::1");

  // this is the actual problem that necessitated a local strcmp in the first place:
  ok(KeyVal_strcmp("foo::1::bar", "foo::10") < 0, "10m. foo::1::bar < foo::10");
  ok(KeyVal_strcmp("foo::10", "foo::1::bar") > 0, "10n. foo::10 > foo::1::bar");

  // keys with non-separator colons:
  ok(KeyVal_strcmp("a:b", "a:c") < 0, "10o. a:b < a:c");
  ok(KeyVal_strcmp("a:c", "a:b") > 0, "10p. a:c > a:b");
  ok(KeyVal_strcmp("a::b:c", "a::b:c") == 0, "10q. a::b:c == a::b:c");
  ok(KeyVal_strcmp("a::b:c", "a::b:d") < 0, "10r. a::b:c < a::b:d");
  ok(KeyVal_strcmp("a::b:d", "a::b:c") > 0, "10s. a::b:d > a::b:c");

  ok(KeyVal_strcmp("a:", "a:") == 0, "10t. a: == a:");
  ok(KeyVal_strcmp("a:", "a0") > 0, "10u. a: > a0");
  ok(KeyVal_strcmp("a0", "a:") < 0, "10v. a0 < a:");
}


////////////////////////////////////////

int main(int argc, char **argv) {

  // turn on super-secret hidden variable that suppresses all error messages,
  // because we test several error conditions and don't want the spewage:
  extern unsigned char KEYVAL_QUIET;
  KEYVAL_QUIET = 1;


  test1();  // test 1: error conditions (missing file, syntax problems)
  test2();  // test 2: valid but content-less files (empty, all whitespace, all comments)
  test3();  // test 3: single-element database created by file
  test4();  // test 4: two-element database created by setValue
  test5();  // test 5: escape sequences and other oddities
  test6();  // test 6: large file
  test7();  // test 7: lots of doubling-array edge cases
  test8();  // test 8: interpolated variables
  test9();  // test 9: saving's align and interp switches
  test10();  // test 10: custom KeyVal_strcmp function

  // there's a good meta-test to add here, which is to run 'test' itself through
  // valgrind to make sure there are no memory leaks.

  printf("\n");
  printf("[test] results:\n");
  printf("  %d passed\n", test_passes);
  printf("  %d failed\n", test_fails);

  // cleanup:
  remove(IN);
  remove(OUT);

  return test_fails > 0;
}

