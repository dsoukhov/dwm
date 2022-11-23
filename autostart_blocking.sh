#!/bin/sh
#xmodmap -e 'clear Lock' -e 'keycode 0x42 = Escape'
pkill set-keyboard
~/scripts/set-keyboard &
