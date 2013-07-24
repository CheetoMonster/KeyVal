#ifndef KEYVAL_H
#define KEYVAL_H

//////////////////////////////////////// KeyValElement

struct KeyValElement{
  // KeyValElement stores a single key-value pair.  Users should never need to
  // work with these, or even know they exist.
  char *key;  // owned by object
  char *val;  // owned by object
};

/*
// Creates a new KeyValElement, with uninitialized data.
struct KeyValElement*
  KeyValElement_new();

struct KeyValElement*
  KeyValElement_new2(const char *key, const char *val);

void
  KeyValElement_delete(struct KeyValElement *element);
*/


//////////////////////////////////////// KeyVal

struct KeyVal {
  // KeyVal is implemented as a doubling array of KeyValElements, which is then
  // searched using binary search.  The elements of the doubling array are
  // lazy-sorted on demand.
  struct KeyValElement **data;  // array of pointers
  int max_size;  // total number of slots available in data.  0 <= MIN_SIZE <= max_size
  int used_size; // total number of slots used.  0 <= used_size <= max_size
  int last_sorted;  // number of sorted elements.  1 <= last_sorted <= used_size
};


// Creates a new KeyVal object.
struct KeyVal*
  KeyVal_new();


// Cleans up the given KeyVal object by deleting all data and then itself.
void
  KeyVal_delete(struct KeyVal *kv);


// Loads in a keyval file.  You may call this multiple times on a given KeyVal
// object; duplicated keys are overwritten (so, last one wins).
// Parameters:
//   <filepath>: the path to the keyval file to load
// Returns:
//   0: everything okay
//   1: parsing problem with the keyval file (stderr is spewed)
//   2: problem opening the keyval file (stderr is spewed)
int
  KeyVal_load(struct KeyVal *kv, const char *filepath);


// Writes the contents to disk.
// Parameters:
//   <filepath>: the path to the keyval file to write
//   <interp>:   whether to interpolate variables
//   <align>:    whether to inject whitespace so all the values align
// Returns:
//   0: everything okay
//   1: if there were any problems writing the file (stderr is spewed)
int
  KeyVal_save(struct KeyVal *kv, const char *filepath, unsigned char interp, unsigned char align);


// Sets the given key to the given value.  KeyVal makes its own copies of
// both the key and the value, so pointer ownership stays with the caller.
// Parameters:
//   <key>: a key path string, such as "foo" or "this::is::a::path"
//   <val>: a value string,
void
  KeyVal_setValue(struct KeyVal *kv, const char *key, const char *val);


// Returns the value for the given key.
// Parameters:
//   <key>: the key path to retrieve
//   <interp>: whether to interpolate variables
// Returns:
//   0: if the key does not exist
//   nonzero: the string value.  You own the pointer and must free it.
char*
  KeyVal_getValue(struct KeyVal *kv, const char *key, int interp);


// Removes the specified key from the database.
// Parameters:
//   <key>: the key path to remove
void
  KeyVal_remove(struct KeyVal *kv, const char *key);


// Returns the list of all immediate sub-keys under a given key path.
// Parameters:
//   <path>: the parent key path.  An empty path ("") means top-level keys
// Returns:
//   0: only if serious problem
//   nonzero: an array of strings.  Ownership of both the array and the strings
//     is given to the caller to free.
char**
  KeyVal_getKeys(struct KeyVal *kv, const char *path);


// Returns a list of all the keys in the database.
// Returns:
//   0: only if serious problem
//   nonzero: null-terminated array of strings.  Ownership of both the array
//     and the strings is given to the caller to free.
char**
  KeyVal_getAllKeys(struct KeyVal *kv);


// Returns:
//   <int>: the number of things in the database.
int
  KeyVal_size(struct KeyVal *kv);


// Returns:
//   1: if the given key exists, and is a leaf, so that it makes sense to
//      ask what its value is
//   0: if the given key does not exist, or if it's only part of a hierarchy
//      so you'd ask what its sub-keys are.
int
  KeyVal_hasValue(struct KeyVal *kv, const char *key);


// Returns:
//   1: if the given path exists and has sub-keys, so calling getKeys on it
//      would do something.
//   0: if the given path does not exist, or exists only as a leaf value.
int
  KeyVal_hasKeys(struct KeyVal *kv, const char *path);


// Returns:
//   1: if the path exists in any form (hasValue || hasKeys)
//   0: if the path doesn't exist in any form
int
  KeyVal_exists(struct KeyVal *kv, const char *key_or_path);




// This is just for debugging, though if you need it, go for it.
void
  KeyVal_print(struct KeyVal *kv);

#endif
