
# change these to whichever versions you want:
PERL := /usr/bin/perl
PYTHON := /usr/local/bin/python3
TCL := /usr/bin/tclsh
SWIG := /usr/local/bin/swig
CC := /usr/bin/clang

# change this to your platform's lib extention (.so, .dylib, .ldd, whatever)
EXT := dylib



FLAGS := -O0 -g -fPIC
WARNS := -Wshadow


#BUG: can this be moved into autotools, so that we're not doing compiling
# and linking ourselves?
#BUG: if the python compile is fine with .so, why does perl need .dylib?  Can
# we always use .so on all platforms??


PERL_ARCHLIB := $(shell $(PERL) -e 'use Config; print $$Config{archlib};')
PERL_ARCHNAME := $(shell $(PERL) -e 'use Config; print $$Config{archname};')
PERL_VERSION := $(shell $(PERL) -e 'use Config; print $$Config{version};')
#PERL_COMPILE_FLAGS := $(shell perl -MConfig -e 'print join(" ", @Config{qw(ccflags optimize cccdlflags)}, "-I$$Config{archlib}/CORE")')
#PERL_LINK_FLAGS := $(shell perl -MConfig -e 'print $$Config{lddlflags}')
#PERL_COMPILE_FLAGS := $(shell perl -MExtUtils::Embed -e ccopts)
#PERL_LINK_FLAGS := -bundle $(shell perl -MExtUtils::Embed -e ldopts)
PERL_COMPILE_FLAGS := -I$(PERL_ARCHLIB)/CORE
PERL_LINK_FLAGS := -L$(PERL_ARCHLIB)/CORE -lperl

PYTHON_INC := $(shell $(PYTHON) -c 'from distutils.sysconfig import get_python_inc; print(get_python_inc())')
PYTHON_COMPILE_FLAGS := -I$(PYTHON_INC)

TCL_LINK_FLAGS := -ltcl


default: perl python tcl


## SWIG-generated wrappers for python, perl, and tcl:
perl: perl/KeyVal_C_API.pm perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_C_API.dylib
python: python/KeyVal_C_API.py python/_KeyVal_C_API.so 
tcl: tcl/KeyVal_C_API.dylib



perl/KeyVal_wrap.c perl/KeyVal_C_API.pm: KeyVal.i KeyVal.h
	$(SWIG) -perl5 -module KeyVal_C_API -outdir perl -o perl/KeyVal_wrap.c KeyVal.i
python/KeyVal_wrap.c python/KeyVal_C_API.py: KeyVal.i KeyVal.h
	$(SWIG) -python -py3 -module KeyVal_C_API -outdir python -o python/KeyVal_wrap.c KeyVal.i
tcl/KeyVal_wrap.c: KeyVal.i KeyVal.h
	$(SWIG) -tcl -module KeyVal_C_API -outdir tcl -o tcl/KeyVal_wrap.c KeyVal.i


perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_wrap.o: perl/KeyVal_wrap.c
	-mkdir -p perl/$(PERL_VERSION)/$(PERL_ARCHNAME)
	$(CC) -c $(FLAGS) -I. $(PERL_COMPILE_FLAGS) $^ -o $@
python/KeyVal_wrap.o: python/KeyVal_wrap.c
	-mkdir -p python
	$(CC) -c $(FLAGS) -I. $(PYTHON_COMPILE_FLAGS) $^ -o $@
tcl/KeyVal_wrap.o: tcl/KeyVal_wrap.c
	-mkdir -p tcl
	$(CC) -c $(FLAGS) -I. $(TCL_COMPILE_FLAGS) $^ -o $@



perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_C_API.dylib: KeyVal.o KeyVal_load.o perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_wrap.o 
	$(CC) -shared $(PERL_LINK_FLAGS) $^ -o $@
python/_KeyVal_C_API.so: KeyVal.o KeyVal_load.o python/KeyVal_wrap.o python/setup.py
	$(PYTHON) python/setup.py build_ext --inplace
	mv _KeyVal_C_API.so python/_KeyVal_C_API.so
tcl/KeyVal_C_API.dylib: KeyVal.o KeyVal_load.o tcl/KeyVal_wrap.o
	$(CC) -shared $(TCL_LINK_FLAGS) $^ -o $@
	


clean:
	rm -f perl/KeyVal_wrap.c
	#rm -f perl/KeyVal_C_API.so
	rm -f perl/KeyVal_C_API.pm
	rm -f perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_wrap.o
	rm -f perl/$(PERL_VERSION)/$(PERL_ARCHNAME)/KeyVal_C_API.dylib
	rm -f python/KeyVal_wrap.c
	rm -f python/KeyVal_wrap.o
	rm -f python/KeyVal_C_API.py
	rm -f _KeyVal_C_API.so python/_KeyVal_C_API.so
	rm -f tcl/KeyVal_wrap.c
	rm -f tcl/KeyVal_wrap.o
	rm -f tcl/KeyVal_C_API.dylib

