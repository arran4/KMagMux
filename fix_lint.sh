#!/bin/bash
pacman -Syu --noconfirm base-devel gcc clang cmake git qt6-base qt6-svg qt6-tools qtkeychain-qt6 kxmlgui kconfig kconfigwidgets kcoreaddons ki18n
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
find src -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | xargs -r clang-tidy -p build/ --fix-errors
