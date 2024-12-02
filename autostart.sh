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
pkill snixembed
snixembed &
pkill dwmblocks
dwmblocks &
pkill iwgtk
iwgtk -i &
pkill autorandr-launcher
autorandr-launcher -d
pkill autorandr
autorandr -c
# pkill udiskie
# udiskie -anT &
pkill alerter
alerter &
