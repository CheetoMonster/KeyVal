#! /usr/bin/perl

# The real tests are in test.c.  The intent of this test.pl is to make sure
# that the perl interface works, so this is much smaller in scope.

use strict;
use warnings;

use File::Temp;
use Test::Simple tests => 30;

use lib qw(perl);
use KeyVal;

# new:
my $o = KeyVal->new();
ok($o, "KeyVal::new");

# size, exists, hasValue:
ok($o->size() == 0, "KeyVal::size == 0");
ok($o->exists("key") == 0, "KeyVal::exists(key) == 0");
ok($o->hasValue("key") == 0, "KeyVal::hasValue(key) == 0");
ok($o->hasKeys("key") == 0, "KeyVal::hasKeys(key) == 0");
ok($o->exists("foo") == 0, "KeyVal::exists(foo) == 0");
ok($o->hasValue("foo") == 0, "KeyVal::hasValue(foo) == 0");
ok($o->hasKeys("foo") == 0, "KeyVal::hasKeys(foo) == 0");

my ($fh, $path) = File::Temp::tempfile();
print {$fh} "`key` = `val`\n";
close($fh) or die "[ERROR] problem creating temp file at $path";

# load:
$o->load($path);
ok(1, "KeyVal::load($path)");

# size, exists, hasValue:
ok($o->size() == 1, "KeyVal::size == 1");
ok($o->exists("key") == 1, "KeyVal::exists(key) == 1");
ok($o->hasValue("key") == 1, "KeyVal::hasValue(key) == 1");
ok($o->hasKeys("key") == 0, "KeyVal::hasKeys(key) == 0");

# getValue:
ok($o->getValue("key") eq "val", "KeyVal::getValue(key) == val");

# setValue, size, exists, hasValue, getValue (again): 
$o->setValue("foo::bar", "bas");
ok(1, "KeyVal::setValue(foo::bar, bas)");
ok($o->size() == 2, "KeyVal::size == 2");
ok($o->exists("foo") == 1, "KeyVal::exists(foo) == 1");
ok($o->hasValue("foo") == 0, "KeyVal::hasValue(foo) == 0");
ok($o->hasKeys("foo") == 1, "KeyVal::hasKeys(foo) == 1");
ok($o->getValue("foo::bar") eq "bas", "KeyVal::getValue(foo::bar) == bas");

# getKeys:
my @keys = $o->getKeys("asdf");
ok(@keys == 0, "KeyVal::getKeys(asdf) == 0");
@keys = $o->getKeys("key");
ok(@keys == 0, "KeyVal::getKeys(key) == 0");
@keys = $o->getKeys("foo");
ok(@keys == 1 && $keys[0] eq 'bar', "KeyVal::getKeys(foo) == [bar]");

# getAllKeys:
@keys = $o->getAllKeys();
ok(@keys == 2, "KeyVal::getAllKeys returns 2 items");
ok($keys[0] eq 'foo::bar' && $keys[1] eq 'key', "KeyVal::getAllKeys returns the correct 2 items");

# save:
my ($fh2, $path2) = File::Temp::tempfile();
close($fh2);
$o->save($path2);
if (ok(-e $path2, "KeyVal::save writes output to $path2")) {
  my $contents = qx{cat $path2};
  my $expected = "`foo::bar` = `bas`\n`key` = `val`\n";
  if (!ok($contents eq $expected, "KeyVal::save writes correct content")) {
    print "-> contents were actually '$contents'\n";
    print "   instead of '$expected'\n";
  }
}


# remove, exists:
$o->remove("key");
ok(1, "KeyVal::remove(key)");
ok($o->exists("key") == 0, "KeyVal::exists(key) == 0");

# delete:
$o->delete();
ok(1, "KeyVal::delete");


