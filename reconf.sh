#!/bin/sh

# Quick and dirty script to generate and configure build system

./autogen.sh

# Base configure commandline, modify paths as needed
CONF="./configure --prefix=/usr/local --with-asset-dir=/usr/local/share/zark-data"

# Base CFLAGS, modify as needed
BASE_CFLAGS="-march=athlon64 -O2 -pipe"


# Uncomment one of these (debugging/release/release+profiling)

# Debugging
#CFLAGS="$BASE_CFLAGS -g" $CONF --enable-debug

# Release
CFLAGS="$BASE_CFLAGS" $CONF

# Release + profiling
#CFLAGS="$BASE_CFLAGS -pg" LDFLAGS="-pg" $CONF

