#!/bin/bash

# Removes everything produced during the build process, but not removed by "make distclean"
rm -rf aclocal.m4 autom4te.cache compile config.guess config.h.in config.sub configure depcomp install-sh lib m4 missing snippet test-driver gnulib *~ */*~ INSTALL config.log config.status stamp-h1 build-aux config.h Makefile.in src/Makefile.in test/Makefile.in
