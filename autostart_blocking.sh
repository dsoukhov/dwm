#!/bin/sh
pkill dunst
dunst-cfg &
xmodmap -e 'clear Lock' -e 'keycode 0x42 = Escape'
