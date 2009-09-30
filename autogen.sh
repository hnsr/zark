#!/bin/sh

# (re)generate autoconf/make stuff

aclocal -I m4
#autoheader
autoconf
automake --copy --add-missing

