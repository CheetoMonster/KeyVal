
# change these to whichever versions you want:
PERL := /usr/bin/perl
SWIG := /usr/local/bin/swig
CC := /usr/bin/clang


FLAGS := -O0 -g -fPIC
WARNS := -Wshadow



PERL_ARCHLIB := $(shell $(PERL) -e 'use Config; print $$Config{archlib};')
PERL_ARCHNAME := $(shell $(PERL) -e 'use Config; print $$Config{archname};')
PERL_VERSION := $(shell $(PERL) -e 'use Config; print $$Config{version};')
#SWIG := /Users/chrisverburg/Downloads/swig-2.0.10/swig
#SWIGLIB := /Users/chrisverburg/Downloads/swig-2.0.10/Lib/
#PERL_COMPILE_FLAGS := $(shell perl -MConfig -e 'print join(" ", @Config{qw(ccflags optimize cccdlflags)}, "-I$$Config{archlib}/CORE")')
#PERL_LINK_FLAGS := $(shell perl -MConfig -e 'print $$Config{lddlflags}')

#PERL_COMPILE_FLAGS := $(shell perl -MExtUtils::Embed -e ccopts)
#PERL_LINK_FLAGS := -bundle $(shell perl -MExtUtils::Embed -e ldopts)

PERL_COMPILE_FLAGS := -I$(PERL_ARCHLIB)/CORE
PERL_LINK_FLAGS := -L$(PERL_ARCHLIB)/CORE -lperl





#default: python/KeyVal_python_wrap.so perl/KeyVal_perl_wrap.so tcl/KeyVal_tcl_wrap.so
default: KeyVal_python_wrap.so

## SWIG-generated wrappers for python, perl, and tcl:
python-swig: KeyVal_python_wrap.so
#perl-swig: perl/KeyVal.bundle
tcl-swig: KeyVal_tcl_wrap.so
perl: perl/KeyVal_C_API.pm perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_C_API.dylib

KeyVal_python_wrap.c: KeyVal.i
	$(SWIG) -python -I$(SWIGLIB) -I$(SWIGLIB)python -outdir py -o KeyVal_python_wrap.c KeyVal.i -module KeyVal_python

perl/KeyVal_wrap.c perl/KeyVal_C_API.pm: KeyVal.i KeyVal.h
	$(SWIG) -perl5 -module KeyVal_C_API -outdir perl -o perl/KeyVal_wrap.c KeyVal.i

KeyVal_tcl_wrap.c: KeyVal.i
	$(SWIG) -tcl -I$(SWIGLIB) -I$(SWIGLIB)tcl -outdir tcl -o KeyVal_tcl_wrap.c KeyVal.i -module KeyVal_tcl


KeyVal_python_wrap.o: KeyVal_python_wrap.c
	$(CC) -c $(FLAGS) -I/Library/Frameworks/Python.framework/Versions/3.3/include/python3.3m $^ -fPIC -Wall -o $@

perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_wrap.o: perl/KeyVal_wrap.c
	-mkdir -p perl/$(PERL_VERSION)/$(PERL_ARCHNAME)
	$(CC) -c $(FLAGS) -I. $(PERL_COMPILE_FLAGS) $^ -o $@

KeyVal_tcl_wrap.o: KeyVal_tcl_wrap.c
	clang -c $(FLAGS) $^ -fPIC -Wall -o $@


KeyVal_python_wrap.so: KeyVal_python_wrap.o KeyVal.so
	clang $^ -dynamic -o $@

#clang $^ /System/Library/Perl/5.12/darwin-thread-multi-2level/CORE/libperl.dylib -shared -o $@
#perl/KeyVal.bundle: KeyVal.so KeyVal_perl_wrap.o
#	$(CC) -shared $(PERL_LINK_FLAGS) $^ $(PERL_ARCHLIB)/CORE/libperl.dylib -o $@
perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_C_API.dylib: KeyVal.o KeyVal_load.o perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_wrap.o 
	$(CC) -shared $(PERL_LINK_FLAGS) $^ -o $@


KeyVal_tcl_wrap.so: KeyVal_tcl_wrap.o KeyVal.so
	clang $(WARNS) $^ -dynamic -o $@


clean:
	rm -f perl/KeyVal_wrap.c
	rm -f perl/KeyVal_C_API.so
	rm -f perl/KeyVal_C_API.pm
	rm -f perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_wrap.o
	rm -f perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_C_API.dylib

