#!/bin/bash
pacman -Syu --noconfirm clang
find src -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | xargs -r clang-format -i
