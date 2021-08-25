#!/bin/sh
nm-applet &
pamixer --set-volume 40
hsetroot -solid '#000000'
#hsetroot -solid '#ffffff'
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
compton --vsync opengl-swc --backend glx -e 1.0 -i 1.0 -o 1.0 -b
xsettingsd -c ~/.xsettingsd &
dunst &
udiskie &
dwmblocks &
