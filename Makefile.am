
lib_LTLIBRARIES = libkeyval.la
libkeyval_la_SOURCES = KeyVal.c KeyVal_load.c

include_HEADERS = KeyVal.h

check_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = libkeyval.la

noinst_PROGRAMS = oom
oom_SOURCES = oom.c
oom_LDADD = libkeyval.la

dist_swig_DATA = KeyVal.i Makefile.swig
swigdir = .

dist_perl_DATA = test.pl perl/KeyVal.pm perl/oom.pl
perldir = perl

ACLOCAL_AMFLAGS = -I m4

