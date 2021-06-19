#!/bin/sh
#feh --bg-scale ~/Pictures/wallpaper/wp1.png &
compton --vsync opengl-swc --backend glx -e 1.0 -i 1.0 -o 1.0 -b
xsettingsd -c ~/.xsettingsd &
dunst &
udiskie &
dwmblocks &
