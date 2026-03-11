#!/bin/bash
export LD_LIBRARY_PATH=./build
xvfb-run -a bash -c "./build/KMagMux & APP_PID=\$! ; sleep 2 ; echo closing ; xdotool search --name 'KMagMux' windowclose ; wait \$APP_PID"
