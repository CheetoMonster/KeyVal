
# This package is an object-oriented interface to KeyVal, whose C API is, by
# definition, procedural.  You can use KeyVal_C_API directly if you want, but
# this should be more straightforward.

import KeyVal_C_API


class KeyVal:
  def __init__(self):
    ptr = KeyVal_C_API.new_KeyVal_ptr_ptr()
    errcode = KeyVal_C_API.KeyVal_new(ptr)
    if errcode != 0:
      raise Exception("[ERROR] KeyVal.__init__")
    self.kv = KeyVal_C_API.KeyVal_ptr_ptr_value(ptr)
    KeyVal_C_API.delete_KeyVal_ptr_ptr(ptr)

  def __del__(self):
    errcode = KeyVal_C_API.KeyVal_delete(self.kv)
    if errcode:
      raise Exception("[ERROR] KeyVal.delete")

  def load(self, filepath):
    errcode = KeyVal_C_API.KeyVal_load(self.kv, filepath)
    if errcode:
      raise Exception("[ERROR] KeyVal.load")

  def save(self, filepath, interp=True, align=False):
    errcode = KeyVal_C_API.KeyVal_save(self.kv, filepath, interp, align)
    if errcode:
      raise Exception("[ERROR] KeyVal.save")

  def setValue(self, key, val):
    errcode = KeyVal_C_API.KeyVal_setValue(self.kv, key, val)
    if errcode:
      raise Exception("[ERROR] KeyVal.setValue")

  def getValue(self, key, interp=True):
    res_p = KeyVal_C_API.new_char_ptr_ptr()
    errcode = KeyVal_C_API.KeyVal_getValue(res_p, self.kv, key, interp)
    if errcode:
      raise Exception("[ERROR] KeyVal.getValue")
    res = KeyVal_C_API.char_ptr_ptr_value(res_p)
    KeyVal_C_API.delete_char_ptr_ptr(res_p)
    # free up the C's returned value:
    KeyVal_C_API.KeyVal_dtltyjr()
    return res

  def remove(self, key):
    errcode = KeyVal_C_API.KeyVal_remove(self.kv, key)
    if errcode:
      raise Exception("[ERROR] KeyVal.remove")

  def getKeys(self, path):
    res_p = KeyVal_C_API.new_char_ptr_ptr_ptr()
    errcode = KeyVal_C_API.KeyVal_getKeys(res_p, self.kv, path)
    if errcode:
      raise Exception("[ERROR] KeyVal.getKeys")
    # convert from C/swig array to python array:
    res = []
    idx = 0
    res_p_p = KeyVal_C_API.char_ptr_ptr_ptr_value(res_p)
    ptr = KeyVal_C_API.char_ptr_arr_getitem(res_p_p, idx)
    while ptr:
      if not ptr: break  # null pointers are converted to undef
      str = ptr
      res.append(str)
      idx += 1
      ptr = KeyVal_C_API.char_ptr_arr_getitem(res_p_p, idx)

    # free up our local pointer:
    KeyVal_C_API.delete_char_ptr_ptr_ptr(res_p)
    # free up the C's returned value:
    KeyVal_C_API.KeyVal_dtltyjr()
    return res

  def getAllKeys(self):
    res_p = KeyVal_C_API.new_char_ptr_ptr_ptr()
    errcode = KeyVal_C_API.KeyVal_getAllKeys(res_p, self.kv)
    if errcode:
      raise Exception("[ERROR] KeyVal.getAllKeys")
    # convert from C/swig array to python array:
    res = []
    idx = 0
    res_p_p = KeyVal_C_API.char_ptr_ptr_ptr_value(res_p)
    ptr = KeyVal_C_API.char_ptr_arr_getitem(res_p_p, idx)
    while ptr:
      if not ptr: break  # null pointers are converted to undef
      str = ptr
      res.append(str)
      idx += 1
      ptr = KeyVal_C_API.char_ptr_arr_getitem(res_p_p, idx)

    # free up local pointer memory:
    KeyVal_C_API.delete_char_ptr_ptr_ptr(res_p)
    # free up the C's returned value:
    KeyVal_C_API.KeyVal_dtltyjr()
    return res

  def size(self):
    res_p = KeyVal_C_API.new_long_ptr()
    errcode = KeyVal_C_API.KeyVal_size(res_p, self.kv)
    if errcode:
      raise Exception("[ERROR] KeyVal.size")
    res = KeyVal_C_API.long_ptr_value(res_p)
    KeyVal_C_API.delete_long_ptr(res_p)
    return res

  def hasValue(self, key):
    res_p = KeyVal_C_API.new_bool_ptr()
    errcode = KeyVal_C_API.KeyVal_hasValue(res_p, self.kv, key)
    if errcode:
      raise Exception("[ERROR] KeyVal.hasValue")
    res = KeyVal_C_API.bool_ptr_value(res_p)
    KeyVal_C_API.delete_bool_ptr(res_p)
    return True if res else False

  def hasKeys(self, path):
    res_p = KeyVal_C_API.new_bool_ptr()
    errcode = KeyVal_C_API.KeyVal_hasKeys(res_p, self.kv, path)
    if errcode:
      raise Exception("[ERROR] KeyVal.hasKeys")
    res = KeyVal_C_API.bool_ptr_value(res_p)
    KeyVal_C_API.delete_bool_ptr(res_p)
    return True if res else False

  def exists(self, key_or_path):
    res_p = KeyVal_C_API.new_bool_ptr()
    errcode = KeyVal_C_API.KeyVal_exists(res_p, self.kv, key_or_path)
    if errcode:
      raise Exception("[ERROR] KeyVal.exists")
    res = KeyVal_C_API.bool_ptr_value(res_p)
    KeyVal_C_API.delete_bool_ptr(res_p)
    return True if res else False

  def print(self):
    # just for debugging
    KeyVal_C_API.KeyVal_print(self.kv)

