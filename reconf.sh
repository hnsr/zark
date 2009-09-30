#!/bin/sh
./autogen.sh

CONF="./configure --prefix=/home/hans/dev --with-asset-dir=/home/hans/prog/zark_data"

export CFLAGS="-march=athlon64 -O2 -pipe"

# Debugging
CFLAGS="$CFLAGS -g" $CONF --enable-debug

# Release
#$CONF

# Release + profiling
#LDFLAGS="-pg" CFLAGS="$CFLAGS -pg" $CONF

