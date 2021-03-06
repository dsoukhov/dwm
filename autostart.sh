#!/bin/sh
pamixer --set-volume 40 && pkill -RTMIN+1 dwmblocks
pamixer --source "alsa_input.pci-0000_00_1f.3.analog-stereo" --set-volume 20 && pkill -RTMIN+2 dwmblocks
hsetroot -solid '#000000'
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
pkill clipmenud
CM_DIR=~/.cache clipmenud &
pkill picom
#picom -CGb
picom -b
pkill xsettingsd
xsettingsd -c ~/.xsettingsd &
pkill dunst
dunst &
pkill dwmblocks
dwmblocks &
pkill 'pinknoise'
pinknoise &
pkill autorandr-launcher
# pkill autorandr
autorandr-launcher -d
# autorandr -c
