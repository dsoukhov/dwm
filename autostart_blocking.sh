#!/bin/sh
pamixer --set-volume 40
pamixer --source "alsa_input.pci-0000_00_1f.3.analog-stereo" --set-volume 20
xmodmap -e 'clear Lock' -e 'keycode 0x42 = Escape'
hsetroot -solid '#000000'
