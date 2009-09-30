#!/bin/sh

# (re)generate autoconf/make stuff

aclocal -I m4
#autoheader
autoconf

# Prevent automake from erroring out on missing build-aux dir
if test \! -e build-aux; then
    mkdir build-aux
fi

automake --copy --add-missing

