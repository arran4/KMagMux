#!/bin/bash
set -e

echo "Building KMagMux using Docker..."
docker build -t kmagmux-build .

echo "Build complete. You can run tests inside the container using:"
echo "docker run --rm -it kmagmux-build bash -c \"cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure\""
