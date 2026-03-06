mkdir -p build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON
make -j$(nproc)
