KeyVal
===

Lightweight, greppable text database for key-value pairs.

The core code is written in C, but I've tried to make it easy to run it through
SWIG to use from any language you want.


Building
===

KeyVal's core code is written in C (and must thus be compiled).  Additionally,
KeyVal has optional perl and python interfaces (which must be generated).
Here's how to generate each of them.

C
---

For building this as a user:
- you should have gotten this from a tarball with a configure script, so all
  you need to do is the standard gnu tool routine:
  % ./configure
  % make
  % make check
  % make install

For building this as a developer:
- you should have gotten this from the git repository, which means you'll have
  no configure script.  To generate it:
  % autoconf --install
  % ./configure
  % make

perl
---

You can build the perl frontend with the following steps:
- edit the PERL, SWIG, and CC variables in the header of Makefile.swig to
point to your desired versions of each of those tools.
- run "make -f Makefile.swig perl"

Now that it's built, you can test that it works by running ./test.pl.  If
everything's ok, now you need to install it.  You could either:
- mimic CPAN and put it into the site-perl directory of your perl installation
- or put it wherever you want and add the path to ENV{PERL5LIB}.

In both cases, remember you need to install several files:
- perl/KeyVal_C_API.pm
- perl/KeyVal.pm
- perl/$version/$arch/KeyVal_C_API.?? (so or dylib or dll or something)

TODO: I would love it if someone who knows perl packaging would figure out how
to package this up so that the install can be done like CPAN installs should be:
- perl Makefile.PL
- make
- make install

python
---

You can build the python frontend with the following steps:
- edit the PYTHON, SWIG, and CC variables in the header of Makefile.swig to
point to your desired versions of each of those tools.
- run "make -f Makefile.swig python"

Now that it's built, you can test that it works by running ./test.py.  If
everything's ok, now you need to install it.  You could either:
- mimic PYPY and put it into the site-python directory of your python installation
- or put it wherever you want and add the path to ENV{PYTHONLIB}.

In both cases, remember you need to install several files:
- python/KeyVal_C_API.py
- python/KeyVal.py
- python/KeyVal_C_API.?? (so or dylib or dll or something)

TODO: I would love it if someone who knows python packaging would figure out how
to package this up so that the install can be done like PYPY installs should be.


Documentation
===

KeyVal.h has copious commenting, but you might also want to check out the
project home on github:

https://github.com/CheetoMonster/KeyVal

