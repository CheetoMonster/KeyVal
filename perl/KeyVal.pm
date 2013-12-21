package KeyVal;

# This package is an object-oriented interface to KeyVal, whose C API is, by
# definition, procedural.  You can use KeyVal_C_API directly if you want, but
# this should be more straightforward.

use Carp;
use strict;
use warnings;

#use lib qw(.);  --this should be done by whoever's use'ing KeyVal itself.
use KeyVal_C_API;

sub new {
  my ($class) = @_;

  my $self = {};

  my $ptr = KeyVal_C_API::new_KeyVal_ptr_ptr();
  my $errcode = KeyVal_C_API::KeyVal_new($ptr);
  if ($errcode != 0) { croak "[ERROR] KeyVal::new"; }
  $self->{kv} = KeyVal_C_API::KeyVal_ptr_ptr_value($ptr);
  KeyVal_C_API::delete_KeyVal_ptr_ptr($ptr);
  return(bless($self, $class));
}

sub delete {
  my ($self) = @_;
  my $errcode = KeyVal_C_API::KeyVal_delete($self->{kv});
  if ($errcode != 0) { croak "[ERROR] KeyVal::delete"; }
  return;
}

sub load {
  my ($self, $path) = @_;
  my $errcode = KeyVal_C_API::KeyVal_load($self->{kv}, $path);
  if ($errcode != 0) { croak "[ERROR] KeyVal::load"; }
  return;
}

sub save {
  my ($self, $path, $interp, $align) = @_;
  if (!defined $interp) { $interp = 1; }
  if (!defined $align) { $align = 0; }
  my $errcode = KeyVal_C_API::KeyVal_save($self->{kv}, $path, $interp, $align);
  if ($errcode != 0) { croak "[ERROR] KeyVal::save"; }
  return;
}

sub setValue {
  my ($self, $key, $val) = @_;
  my $errcode = KeyVal_C_API::KeyVal_setValue($self->{kv}, $key, $val);
  if ($errcode != 0) { croak "[ERROR] KeyVal::setValue"; }
  return;
}

sub getValue {
  my ($self, $key, $interp) = @_;
  if (!defined $interp) { $interp = 1; }
  my $res_p = KeyVal_C_API::new_char_ptr_ptr();
  my $errcode = KeyVal_C_API::KeyVal_getValue($res_p, $self->{kv}, $key, $interp);
  if ($errcode != 0) { croak "[ERROR] KeyVal::getValue"; }
  my $res = KeyVal_C_API::char_ptr_ptr_value($res_p);
  KeyVal_C_API::delete_char_ptr_ptr($res_p);
  # free up the C's returned value:
  KeyVal_C_API::KeyVal_dtltyjr();
  return $res;
}

sub remove {
  my ($self, $key) = @_;
  my $errcode = KeyVal_C_API::KeyVal_remove($self->{kv}, $key);
  if ($errcode != 0) { croak "[ERROR] KeyVal::remove"; }
  return;
}

sub getKeys {
  my ($self, $path) = @_;
  my $res_p = KeyVal_C_API::new_char_ptr_ptr_ptr();
  my $errcode = KeyVal_C_API::KeyVal_getKeys($res_p, $self->{kv}, $path);
  if ($errcode != 0) { croak "[ERROR] KeyVal::getKeys"; }
  # convert from C/swig array to perl array:
  my @res;
  my $idx = 0;
  my $res_p_p = KeyVal_C_API::char_ptr_ptr_ptr_value($res_p);
  while (my $ptr = KeyVal_C_API::char_ptr_arr_getitem($res_p_p, $idx)) {
    #my $str = KeyVal_C_API::char_ptr_ptr_value($ptr);
    #last if (!defined $str);
    last if (!defined $ptr);  # null pointers are converted to undef
    my $str = $ptr;
    push @res, $str;
    ++$idx;
    #BUG: memory leak here.  I don't know how to tell the C API to free $ptr,
    # because SWIG automatically messes with char*'s so $ptr isn't really the
    # address.  I think the only way to fix this is to redefine the SWIG
    # typemap for char** and char***, and my SWIG-fu is not strong enough
    # for that..
  }

  # free up our local pointer:
  KeyVal_C_API::delete_char_ptr_ptr_ptr($res_p);
  # free up the C's returned value:
  KeyVal_C_API::KeyVal_dtltyjr();
  return @res;
}

sub getAllKeys {
  my ($self) = @_;
  my $res_p = KeyVal_C_API::new_char_ptr_ptr_ptr();
  my $errcode = KeyVal_C_API::KeyVal_getAllKeys($res_p, $self->{kv});
  if ($errcode != 0) { croak "[ERROR] KeyVal::getAllKeys"; }
  # convert from C/swig array to perl array:
  my @res;
  my $idx = 0;
  my $res_p_p = KeyVal_C_API::char_ptr_ptr_ptr_value($res_p);
  while (my $ptr = KeyVal_C_API::char_ptr_arr_getitem($res_p_p, $idx)) {
    last if (!defined $ptr);  # null pointers are converted to undef
    my $str = $ptr;
    push @res, $str;
    ++$idx;
  }

  # free up local pointer memory:
  KeyVal_C_API::delete_char_ptr_ptr_ptr($res_p);
  # free up the C's returned value:
  KeyVal_C_API::KeyVal_dtltyjr();
  return @res;
}

sub size {
  my ($self) = @_;
  my $res_p = KeyVal_C_API::new_long_ptr();
  my $errcode = KeyVal_C_API::KeyVal_size($res_p, $self->{kv});
  if ($errcode != 0) { croak "[ERROR] KeyVal::size"; }
  my $res = KeyVal_C_API::long_ptr_value($res_p);
  KeyVal_C_API::delete_long_ptr($res_p);
  return $res;
}

sub hasValue {
  my ($self, $key) = @_;
  my $res_p = KeyVal_C_API::new_bool_ptr();
  my $errcode = KeyVal_C_API::KeyVal_hasValue($res_p, $self->{kv}, $key);
  if ($errcode != 0) { croak "[ERROR] KeyVal::hasValue"; }
  my $res = KeyVal_C_API::bool_ptr_value($res_p);
  KeyVal_C_API::delete_bool_ptr($res_p);
  return int($res);
}

sub hasKeys {
  my ($self, $path) = @_;
  my $res_p = KeyVal_C_API::new_bool_ptr();
  my $errcode = KeyVal_C_API::KeyVal_hasKeys($res_p, $self->{kv}, $path);
  if ($errcode != 0) { croak "[ERROR] KeyVal::hasKeys"; }
  my $res = KeyVal_C_API::bool_ptr_value($res_p);
  KeyVal_C_API::delete_bool_ptr($res_p);
  return int($res);
}

sub exists {
  my ($self, $key_or_path) = @_;
  my $res_p = KeyVal_C_API::new_bool_ptr();
  my $errcode = KeyVal_C_API::KeyVal_exists($res_p, $self->{kv}, $key_or_path);
  if ($errcode != 0) { croak "[ERROR] KeyVal::exists"; }
  my $res = KeyVal_C_API::bool_ptr_value($res_p);
  KeyVal_C_API::delete_bool_ptr($res_p);
  return int($res);
}


sub print {
  # just for debugging
  my ($self) = @_;
  KeyVal_C_API::KeyVal_print($self->{kv});
  return;
}

1;

