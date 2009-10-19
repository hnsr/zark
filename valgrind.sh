#!/bin/sh
valgrind --undef-value-errors=no --leak-check=full --show-reachable=yes src/zark
