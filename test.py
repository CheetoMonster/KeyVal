#!/usr/bin/env python3

# The real tests are in test.c.  The intent of this test.py is to make sure
# that the python interface works, so this is much smaller in scope.

import os
import sys
import tempfile

# oddly, python will pick up a stray "KeyVal.so" sitting in . even if there
# is no KeyVal.py.  Check that if the following doesn't work:
sys.path.insert(1, "python")
#for p in sys.path:
#  print("*", p)
import KeyVal

ok_count = 0

def ok(condition, errmsg):
  global ok_count
  ok_count += 1
  if condition:
    print("ok", ok_count, "-", errmsg)
    return True
  else:
    print("FAILED", ok_count, "-", errmsg)
    return False

# new:
o = KeyVal.KeyVal()
ok(o, "KeyVal::new")

# size, exists, hasValue:
ok(o.size() == 0, "KeyVal::size == 0")
ok(o.exists("key") == False, "KeyVal::exists(key) == 0")
ok(o.hasValue("key") == False, "KeyVal::hasValue(key) == 0")
ok(o.hasKeys("key") == False, "KeyVal::hasKeys(key) == 0")
ok(o.exists("foo") == False, "KeyVal::exists(foo) == 0")
ok(o.hasValue("foo") == False, "KeyVal::hasValue(foo) == 0")
ok(o.hasKeys("foo") == False, "KeyVal::hasKeys(foo) == 0")

temp_file_obj = tempfile.NamedTemporaryFile(delete=False)
temp_file_obj.write(b"`key` = `val`\n")
temp_file_obj.close()

# load:
o.load(temp_file_obj.name)
ok(True, "KeyVal::load("+temp_file_obj.name+")")

# size, exists, hasValue:
ok(o.size() == 1, "KeyVal::size == 1")
ok(o.exists("key") == True, "KeyVal::exists(key) == 1")
ok(o.hasValue("key") == True, "KeyVal::hasValue(key) == 1")
ok(o.hasKeys("key") == False, "KeyVal::hasKeys(key) == 0")

# getValue:
ok(o.getValue("key") == "val", "KeyVal::getValue(key) == val")

# setValue, size, exists, hasValue, getValue (again): 
o.setValue("foo::bar", "bas")
ok(1, "KeyVal::setValue(foo::bar, bas)")
ok(o.size() == 2, "KeyVal::size == 2")
ok(o.exists("foo") == True, "KeyVal::exists(foo) == 1")
ok(o.hasValue("foo") == False, "KeyVal::hasValue(foo) == 0")
ok(o.hasKeys("foo") == True, "KeyVal::hasKeys(foo) == 1")
ok(o.getValue("foo::bar") == "bas", "KeyVal::getValue(foo::bar) == bas")

# getKeys:
keys = o.getKeys("asdf")
ok(len(keys) == 0, "KeyVal::getKeys(asdf) == 0")
keys = o.getKeys("key")
ok(len(keys) == 0, "KeyVal::getKeys(key) == 0")
keys = o.getKeys("foo")
ok(len(keys) == 1 and keys[0] == 'bar', "KeyVal::getKeys(foo) == [bar]")

# getAllKeys:
keys = o.getAllKeys()
ok(len(keys) == 2, "KeyVal::getAllKeys returns 2 items")
ok(keys[0] == 'foo::bar' and keys[1] == 'key', "KeyVal::getAllKeys returns the correct 2 items")

# save:
os.unlink(temp_file_obj.name)
o.save(temp_file_obj.name)
if ok(os.path.exists(temp_file_obj.name), "KeyVal::save writes output to "+temp_file_obj.name):
  fh = open(temp_file_obj.name)
  contents = fh.read()
  fh.close()
  expected = "`foo::bar` = `bas`\n`key` = `val`\n"
  if not ok(contents == expected, "KeyVal::save writes correct content"):
    print("-> contents were actually '"+contents+"'")
    print("   instead of '"+expected+"'")


# remove, exists:
o.remove("key")
ok(True, "KeyVal::remove(key)")
ok(o.exists("key") == False, "KeyVal::exists(key) == 0")

# delete:
del o
ok(True, "KeyVal::delete")

# (cleanup)
os.unlink(temp_file_obj.name)

