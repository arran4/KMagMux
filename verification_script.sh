#!/bin/bash
# A small script to compile the QT app and take a screenshot of the TorrentInfoDialog
xvfb-run -s "-screen 0 1024x768x24" bash -c "./build/KMagMux magnet:?xt=urn:btih:f5dc2535ba2452c9db1d04423b4daeaf0cc1b702&dn=Ubuntu+22.04+LTS&tr=udp%3A%2F%2Ftracker.opentrackr.org%3A1337 & sleep 3 && xwd -root | convert xwd:- screenshot.png"
