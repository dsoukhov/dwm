#!/bin/sh
pamixer --set-volume 40 && pkill -RTMIN+1 dwmblocks
pamixer --default-source --set-volume 20 && pkill -RTMIN+2 dwmblocks
xsetroot -solid "#000000"
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
pkill clipmenud
CM_DIR=~/.cache clipmenud &
# pkill picom
# picom -b
pkill xsettingsd
xsettingsd -c ~/.xsettingsd &
dunst-cfg &
pkill dwmblocks
dwmblocks &
pkill 'pinknoise'
pinknoise &
pkill udiskie
udiskie -anT &
pkill autorandr-launcher
autorandr-launcher -d
pkill autorandr
autorandr -c
