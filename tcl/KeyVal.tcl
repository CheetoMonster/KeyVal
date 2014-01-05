
package provide KeyVal 0.2

#BUG: you must change "tcl" to whatever path contains the file!
#BUG: you might need to rename "dylib" on your platform
load tcl/KeyVal_C_API.dylib

namespace eval KeyVal {
  namespace export new delete load save setValue getValue remove getKeys\
      getAllKeys size hasValue hasKeys exists print
}

proc ::KeyVal::new {} {
  set ptr [new_KeyVal_ptr_ptr]
  set errcode [KeyVal_new $ptr]
  if { $errcode != 0 } { error "ERROR: KeyVal::new" }
  set res [KeyVal_ptr_ptr_value $ptr]
  delete_KeyVal_ptr_ptr $ptr
  return $res
}

proc ::KeyVal::delete { kv } {
  set errcode [KeyVal_delete $kv]
  if { $errcode != 0 } { error "ERROR: KeyVal::delete" }
  return
}

proc ::KeyVal::load { kv filepath } {
  set errcode [KeyVal_load $kv $filepath]
  if { $errcode != 0 } { error "ERROR: KeyVal::load" }
  return
}

proc ::KeyVal::save { kv filepath {interp 1} {align 0} } {
  set errcode [KeyVal_save $kv $filepath $interp $align]
  if { $errcode != 0 } { error "ERROR: KeyVal::save" }
  return
}

proc ::KeyVal::setValue { kv key val } {
  set errcode [KeyVal_setValue $kv $key $val]
  if { $errcode != 0 } { error "ERROR: KeyVal::setValue" }
  return
}

proc ::KeyVal::getValue { kv key {interp 1} } {
  set res_p [new_char_ptr_ptr]
  set errcode [KeyVal_getValue $res_p $kv $key $interp]
  if { $errcode != 0 } { error "ERROR: KeyVal::getValue" }
  set res [char_ptr_ptr_value $res_p]
  delete_char_ptr_ptr $res_p
  # free up the C's returned value:
  KeyVal_dtltyjr
  return $res
}

proc ::KeyVal::remove { kv key } {
  set errcode [KeyVal_remove $kv $key]
  if { $errcode != 0 } { error "ERROR: KeyVal::remove" }
  return
}

proc ::KeyVal::getKeys { kv path } {
  set res_p [new_char_ptr_ptr_ptr]
  set errcode [KeyVal_getKeys $res_p $kv $path]
  if { $errcode != 0 } { error "ERROR: KeyVal::getKeys" }
  # convert from C/swig array to tcl list:
  set res [list]
  set idx 0
  set res_p_p [char_ptr_ptr_ptr_value $res_p]
  set str [char_ptr_arr_getitem $res_p_p $idx]
  while { $str != "" } {
    lappend res $str
    incr idx
    set str [char_ptr_arr_getitem $res_p_p $idx]
  }

  # free up our local pointer:
  delete_char_ptr_ptr_ptr $res_p
  # free up the C's returned value:
  KeyVal_dtltyjr
  return $res
}

proc ::KeyVal::getAllKeys { kv } {
  set res_p [new_char_ptr_ptr_ptr]
  set errcode [KeyVal_getAllKeys $res_p $kv]
  if { $errcode != 0 } { error "ERROR: KeyVal::getAllKeys" }
  # convert from C/swig array to tcl list:
  set res [list]
  set idx 0
  set res_p_p [char_ptr_ptr_ptr_value $res_p]
  set str [char_ptr_arr_getitem $res_p_p $idx]
  while { $str != "" } {
    lappend res $str
    incr idx
    set str [char_ptr_arr_getitem $res_p_p $idx]
  }

  # free up local pointer memory:
  delete_char_ptr_ptr_ptr $res_p
  # free up the C's returned value:
  KeyVal_dtltyjr
  return $res
}

proc ::KeyVal::size { kv } {
  set res_p [new_long_ptr]
  set errcode [KeyVal_size $res_p $kv]
  if { $errcode != 0 } { error "ERROR: KeyVal::size" }
  set res [long_ptr_value $res_p]
  delete_long_ptr $res_p
  return $res
}

proc ::KeyVal::hasValue { kv key } {
  set res_p [new_bool_ptr]
  set errcode [KeyVal_hasValue $res_p $kv $key]
  if { $errcode != 0 } { error "ERROR: KeyVal::hasValue" }
  set res [bool_ptr_value $res_p]
  delete_bool_ptr $res_p
  return $res
}

proc ::KeyVal::hasKeys { kv path } {
  set res_p [new_bool_ptr]
  set errcode [KeyVal_hasKeys $res_p $kv $path]
  if { $errcode != 0 } { error "ERROR: KeyVal::hasKeys" }
  set res [bool_ptr_value $res_p]
  delete_bool_ptr $res_p
  return $res
}

proc ::KeyVal::exists { kv key_or_path } {
  set res_p [new_bool_ptr]
  set errcode [KeyVal_exists $res_p $kv $key_or_path]
  if { $errcode != 0 } { error "ERROR: KeyVal::exists" }
  set res [bool_ptr_value $res_p]
  delete_bool_ptr $res_p
  return $res
}

proc ::print { kv } {
  # just for debugging
  KeyVal_print $kv
  return
}


