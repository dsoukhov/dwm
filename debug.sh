#!/usr/bin/env sh

make debug
startx ~/.xinitrc_test -- /usr/bin/Xephyr -screen 800x600 -br -reset -terminate :1 2> /dev/null &
sleep 2
dwmpid=$(pgrep -n dwm-debug)
sudo -E gdb -p ${dwmpid}
