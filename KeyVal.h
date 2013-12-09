#ifndef KEYVAL_H
#define KEYVAL_H


//////////////////////////////////////// KeyValElement

struct KeyValElement{
  // KeyValElement stores a single key-value pair.  Users should never need to
  // work with these, or even know they exist.
  char *key;  // owned by object
  char *val;  // owned by object
};


//////////////////////////////////////// KeyVal

struct KeyVal {
  // KeyVal is implemented as a doubling array of KeyValElements, which is then
  // searched using binary search.  The elements of the doubling array are
  // lazy-sorted on demand.
  struct KeyValElement **data;  // array of pointers
  unsigned long max_size;  // total number of slots available in data.  0 <= MIN_SIZE <= max_size
  unsigned long used_size; // total number of slots used.  0 <= used_size <= max_size
  unsigned long last_sorted;  // number of sorted elements.  1 <= last_sorted <= used_size
};


// Creates a new KeyVal object.
// Parameters:
//   <res>: where to put the result.  This must be the address of a valid
//     pointer, though the pointer itself doesn't have to be valid.  If
//     that made any sense.
// Returns:
//   0: everything okay.  '*res' points to a new KeyVal object.
//   1: encountered errors.  stderr spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   if (KeyVal_new(&kv)) abort();
unsigned char
  KeyVal_new(struct KeyVal **res);


// Cleans up the given KeyVal object by deleting all data and then itself.
// Parameters:
//   <kv>: a KeyVal object.
// Returns:
//   0: everything okay.
//   1: encountered errors.  stderr spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   if (KeyVal_new(&kv)) abort();
//   if (KeyVal_delete(kv)) abort();
unsigned char
  KeyVal_delete(struct KeyVal *kv);


// Loads in a keyval file.  You may call this multiple times on a given KeyVal
// object to load multiple files; duplicated keys are overwritten (so, last
// one wins).
// Parameters:
//   <kv>: a KeyVal object.
//   <filepath>: the path to the keyval file to load.
// Returns:
//   0: everything okay.
//   1: parsing problem with the keyval file (stderr spewed, errno is set).
//   2: problem opening the keyval file (stderr spewed, errno is set).
// Example:
//   struct KeyVal *kv;
//   if (KeyVal_new(&kv)) abort();
//   if (KeyVal_load(kv, "/path/to/somewhere.kv")) abort();
unsigned char
  KeyVal_load(struct KeyVal *kv, const char *filepath);


// Writes the contents to disk.
// Parameters:
//   <kv>: a KeyVal object.
//   <filepath>: the path to the file to write.
//   <interp>: whether to interpolate variables.
//   <align>: whether to inject whitespace so all the values align.
// Returns:
//   0: everything okay.
//   1: problems with arguments or memory (stderr spewed, errno is set).
//   2: problems writing the file (stderr spewed, errno is set).
// Example:
//   struct KeyVal *kv;
//   ..
//   if (KeyVal_save(kv, "/path/to/somewhere.kv", 1, 0)) abort();
unsigned char
  KeyVal_save(struct KeyVal *kv, const char *filepath, unsigned char interp, unsigned char align);


// Sets the given key to the given value.  KeyVal makes its own copies of
// both the key and the value, so pointer ownership stays with the caller.
// Parameters:
//   <kv>: a KeyVal object.
//   <key>: a key path string, such as "foo" or "this::is::a::path"
//   <val>: a value string.
// Returns:
//   0: everything okay.
//   1: encountered errors.  stderr spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   if (KeyVal_setValue(kv, "some::random::path", "my value!")) abort();
unsigned char
  KeyVal_setValue(struct KeyVal *kv, const char *key, const char *val);


// Returns the value for the given key.
// Parameters:
//   <res>: pointer to where to put the string.  This must be the address of a
//     valid pointer, though the pointer value is irrelevant.
//   <kv>: a KeyVal object.
//   <key>: the key path to retrieve.
//   <interp>: whether to interpolate variables.
// Returns:
//   0: everything okay.  '*res' is either null (if the key does not exist) or
//     else it points to a string that you own and must free.
//   1: encountered errors.  stderr spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   char *value;
//   if (KeyVal_getValue(&value, kv, "some::random::path", 1)) abort();
unsigned char
  KeyVal_getValue(char **res, struct KeyVal *kv, const char *key, int interp);


// Removes the specified key from the database.  It is okay if the key is
// already not in the database.
// Parameters:
//   <kv>: a KeyVal object.
//   <key>: the key path to remove.
// Returns:
//   0: everything okay.
//   1: encountered errors.  stderr spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   if (KeyVal_remove(kv, "some::random::path")) abort();
unsigned char
  KeyVal_remove(struct KeyVal *kv, const char *key);


// Returns the list of all immediate sub-keys under a given key path.
// Parameters:
//   <res>: pointer to where to put the array.  This must be the address of a
//     valid pointer, though the pointer value is irrelevant.
//   <kv>: a KeyVal object.
//   <path>: the parent key path.  An empty path ("") means top-level keys.
// Returns:
//   0: everything okay.  '*res' points to a null-terminated array of strings.
//     Ownership of both the array and the strings is given to the caller
//     to free.
//   1: encountered errors.  stderr spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   char **key_array;
//   if (KeyVal_getKeys(&key_array, kv, "some::random::path")) abort();
//   for (unsigned int idx = 0;
//       key_array[idx] != 0;
//       ++idx) {
//     char *this_key = key_array[idx];
//   }
unsigned char
  KeyVal_getKeys(char ***res, struct KeyVal *kv, const char *path);


// Returns a list of all the keys in the database.
// Parameters:
//   <res>: pointer to where to put the array.  This must be the address of a
//     valid pointer, though the pointer value is irrelevant.
//   <kv>: a KeyVal object.
// Returns:
//   0: everything okay.  '*res' points to a null-terminated array of strings.
//     Ownership of both the array and the strings is given to the caller
//     to free.
//   1: encountered errors.  stderr is spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   char **key_array;
//   if (KeyVal_getAllKeys(&key_array, kv)) abort();
//   for (unsigned int idx = 0;
//       key_array[idx] != 0;
//       ++idx) {
//     char *this_key = key_array[idx];
//   }
unsigned char
  KeyVal_getAllKeys(char ***res, struct KeyVal *kv);


// Returns the number of items (key=value pairs) currently in the database.
// Parameters:
//   <res>: pointer to where to put the result.  This must be a valid pointer,
//     though the pointed-to value is irrelevant.
//   <kv>: a KeyVal object.
// Returns:
//   0: everything okay.  '*res' contains the number of things in the database.
//   1: encountered errors.  stderr is spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   unsigned long size;
//   if (KeyVal_size(&size, kv)) abort();
unsigned char
  KeyVal_size(unsigned long *res, struct KeyVal *kv);


// Returns whether the given key path exists and is a leaf (as opposed to 
// being only part of a deeper hierarchy).
// Parameters:
//   <res>: pointer to where to put the result.  This must be a valid pointer,
//     though the pointed-to value is irrelevant.
//   <kv>: a KeyVal object.
//   <key>: a key path.
// Returns:
//   0: everything okay.  '*res' is set to either 0 or 1 indicating if the
//     given key exists and is a leaf.
//   1: encountered errors.  stderr is spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   unsigned char has_my_path;
//   if (KeyVal_hasValue(&has_my_path, kv, "some::random::path")) abort();
//   if (has_my_path) ..
unsigned char
  KeyVal_hasValue(unsigned char *res, struct KeyVal *kv, const char *key);


// Returns whether the given path has sub-keys or not.
// Parameters:
//   <res>: pointer to where to put the result.  This must be a valid pointer,
//     though the pointed-to value is irrelevant.
//   <kv>: a KeyVal object.
//   <path>: a key path.
// Returns:
//   0: everything okay.  '*res' is set to either 0 or 1 indicating if the
//     given path exists and has sub-keys.
//   1: encountered errors.  stderr is spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   unsigned char has_keys;
//   if (KeyVal_hasKeys(&has_keys, kv, "some::random::path")) abort();
//   if (has_keys) ..
unsigned char
  KeyVal_hasKeys(unsigned char *res, struct KeyVal *kv, const char *path);


// Returns whether the given path exists as either a full path or as part
// of a longer path.  This is essentially the same as
// "KeyVal_hasValue(..) || KeyVal_hasKeys(..)".
// Parameters:
//   <res>: pointer to where to put the result.  This must be a valid pointer,
//     though the pointed-to value is irrelevant.
//   <kv>: a KeyVal object.
//   <key_or_path>: a key path.
// Returns:
//   0: everything okay.  '*res' is set to either 0 or 1 indicating if the
//     given path exists in any form (either hasValue || hasKeys).
//   1: encountered errors.  stderr is spewed, errno is set.
// Example:
//   struct KeyVal *kv;
//   ..
//   unsigned char exists;
//   if (KeyVal_exists(&exists, kv, "some::random::path")) abort();
//   if (exists) ..
unsigned char
  KeyVal_exists(unsigned char *res, struct KeyVal *kv, const char *key_or_path);



// This is just for debugging, though if you need it, go for it.
void
  KeyVal_print(struct KeyVal *kv);

#endif
