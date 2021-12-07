#!/bin/sh
dunst-cfg &
killall bat_hib
bat_hib &
xmodmap -e 'clear Lock' -e 'keycode 0x42 = Escape'
