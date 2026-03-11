cmake -S . -B build_test -DCMAKE_MODULE_PATH="" -DQT_DIR="/usr/lib/qt6"
cmake --build build_test -j$(nproc)
export LD_LIBRARY_PATH=./build_test
export QT_QPA_PLATFORM=offscreen
./build_test/TestSegfault
