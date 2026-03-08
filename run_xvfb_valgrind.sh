Xvfb :99 -screen 0 1024x768x24 &
XVFB_PID=$!
export DISPLAY=:99
sleep 1
valgrind ./build/KMagMux &
KM_PID=$!
sleep 5
xdotool search --name "KMagMux" windowactivate --sync
xdotool key alt+h
sleep 1
xdotool key Down
sleep 1
xdotool key Return
sleep 2
kill $KM_PID
kill $XVFB_PID
