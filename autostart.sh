#!/bin/sh
pamixer --set-volume 40
hsetroot -solid '#000000'
#hsetroot -solid '#ffffff'
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
picom -CGb
xsettingsd -c ~/.xsettingsd &
udiskie &
dwmblocks &
