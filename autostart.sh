#!/bin/sh
#pamixer --set-volume 40
hsetroot -solid '#000000'
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
pkill clipmenud
CM_DIR=~/.cache clipmenud &
pkill picom
#picom -CGb
picom -b
pkill xsettingsd
xsettingsd -c ~/.xsettingsd &
pkill dwmblocks
dwmblocks &
pkill autorandr-launcher
# pkill autorandr
autorandr-launcher -d
pkill udiskie
udiskie -anT &
# autorandr -c
