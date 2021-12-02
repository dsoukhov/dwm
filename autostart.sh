#!/bin/sh
pamixer --set-volume 40 && pkill -RTMIN+1 dwmblocks
pamixer --source "alsa_input.pci-0000_00_1f.3.analog-stereo" --set-volume 20 && pkill -RTMIN+2 dwmblocks
hsetroot -solid '#000000'
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
CM_DIR=~/.cache clipmenud &
picom -CGb
xsettingsd -c ~/.xsettingsd &
dunst &
udiskie &
dwmblocks &
pkill 'pinknoise'
pinknoise &
