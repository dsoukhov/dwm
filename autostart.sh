#!/bin/sh
#pamixer --set-volume 40
hsetroot -solid '#000000'
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
pkill clipmenud
CM_DIR=~/.cache clipmenud &
pkill picon
picom -CGb &
pkill xsettingsd
xsettingsd -c ~/.xsettingsd &
pkill dwmblocks
dwmblocks &
