#! /usr/bin/tclsh

# The real tests are in test.c.  The intent of this test.tcl is to make sure
# that the tcl interface works, so this is much smaller in scope.

set ok_count 0

proc ok { condition errmsg } {
  global ok_count
  incr ok_count
  if { $condition } {
    puts "ok $ok_count - $errmsg"
    return 1
  } else {
    puts "FAILED $ok_count - $errmsg"
    return 0
  }
}


lappend ::auto_path tcl
package require KeyVal

# new:
set o [KeyVal::new]
ok [expr {$o ne ""}] "KeyVal::new"

# size, exists, hasValue:
ok [expr { [KeyVal::size $o] == 0}] "KeyVal::size == 0"
ok [expr { [KeyVal::exists $o "key"] == 0}] "KeyVal::exists(key) == 0"
ok [expr { [KeyVal::hasValue $o "key"] == 0}] "KeyVal::hasValue(key) == 0"
ok [expr { [KeyVal::hasKeys $o "key"] == 0}] "KeyVal::hasKeys(key) == 0"
ok [expr { [KeyVal::exists $o "foo"] == 0}] "KeyVal::exists(foo) == 0"
ok [expr { [KeyVal::hasValue $o "foo"] == 0}] "KeyVal::hasValue(foo) == 0"
ok [expr { [KeyVal::hasKeys $o "foo"] == 0}] "KeyVal::hasKeys(foo) == 0"

# (tcl's "file tempfile" is relatively new, so we do this ourselves)
set temp_path "/tmp/keyval.test.[pid]"
set fh [open $temp_path "w"]
puts $fh "`key` = `val`"
close $fh

# load:
KeyVal::load $o $temp_path
ok 1 "KeyVal::load($temp_path)"

# size, exists, hasValue:
ok [expr { [KeyVal::size $o] == 1}] "KeyVal::size == 1"
ok [expr { [KeyVal::exists $o "key"] == 1}] "KeyVal::exists(key) == 1"
ok [expr { [KeyVal::hasValue $o "key"] == 1}] "KeyVal::hasValue(key) == 1"
ok [expr { [KeyVal::hasKeys $o "key"] == 0}] "KeyVal::hasKeys(key) == 0"

# getValue:
ok [expr { [KeyVal::getValue $o "key"] == "val"}] "KeyVal::getValue(key) == val"

# setValue, size, exists, hasValue, getValue (again): 
KeyVal::setValue $o "foo::bar" "bas"
ok 1 "KeyVal::setValue(foo::bar, bas)"
ok [expr { [KeyVal::size $o] == 2}] "KeyVal::size == 2"
ok [expr { [KeyVal::exists $o "foo"] == 1}] "KeyVal::exists(foo) == 1"
ok [expr { [KeyVal::hasValue $o "foo"] == 0}] "KeyVal::hasValue(foo) == 0"
ok [expr { [KeyVal::hasKeys $o "foo"] == 1}] "KeyVal::hasKeys(foo) == 1"
ok [expr { [KeyVal::getValue $o "foo::bar"] == "bas"}] "KeyVal::getValue(foo::bar) == bas"

# getKeys:
set keys [KeyVal::getKeys $o "asdf"]
ok [expr { [llength $keys] == 0}] "KeyVal::getKeys(asdf) == 0"
set keys [KeyVal::getKeys $o "key"]
ok [expr { [llength $keys] == 0}] "KeyVal::getKeys(key) == 0"
set keys [KeyVal::getKeys $o "foo"]
ok [expr { [llength $keys] == 1 && [lindex $keys 0] == "bar"}] "KeyVal::getKeys(foo) == \[bar]"

# getAllKeys:
set keys [KeyVal::getAllKeys $o]
ok [expr { [llength $keys] == 2}] "KeyVal::getAllKeys returns 2 items"
ok [expr { [lindex $keys 0] == "foo::bar" && [lindex $keys 1] == "key"}] "KeyVal::getAllKeys returns the correct 2 items"

# save:
file delete $temp_path
KeyVal::save $o $temp_path
if { [ok [file exists $temp_path] "KeyVal::save writes output to $temp_path"] } {
  set fh [open $temp_path "r"]
  set contents [read $fh]
  close $fh
  set expected "`foo::bar` = `bas`\n`key` = `val`\n"
  if { ![ok [expr { $contents == $expected }] "KeyVal::save writes correct content"] } {
    puts "-> contents were actually '$contents'\n";
    puts "   instead of '$expected'\n";
  }
}


# remove, exists:
KeyVal::remove $o "key"
ok 1 "KeyVal::remove(key)"
ok [expr { [KeyVal::exists $o "key"] == 0}] "KeyVal::exists(key) == 0"

# delete:
KeyVal::delete $o
ok 1 "KeyVal::delete"

# (cleanup)
file delete $temp_path

