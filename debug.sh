#!/usr/bin/env sh

# Make sure to build dwm with the commented out CFLAGS that include -g
# and add -DDEBUG to those flags.

Xephyr -br -ac -noreset -screen 800x600 :2 &
export DISPLAY=:2
make
gdb ./dwm
