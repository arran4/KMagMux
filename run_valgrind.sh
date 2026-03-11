export LD_LIBRARY_PATH=./build
export QT_QPA_PLATFORM=offscreen
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./build/KMagMux magnet:?xt=urn:btih:0123 &
APP_PID=$!
sleep 2
kill -SIGTERM $APP_PID
wait $APP_PID
