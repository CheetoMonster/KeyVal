KeyVal
======

Lightweight, greppable text database for key-value pairs.

The core code is written in C, but I've tried to make it easy to run it through
SWIG to use from any language you want.


Building
========

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


Documentation
=============

KeyVal.h has copious commenting, but you might also want to check out the
project home on github:

https://github.com/CheetoMonster/KeyVal

